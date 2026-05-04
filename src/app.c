#include "app.h"
#include "config.h"
#include "renderer.h"
#include "grid.h"
#include "timing.h"
#include "input.h"
#include "map.h"
#include "camera.h"
#include "raycast.h"
#include "assets.h"
#include "map_loader.h"
#include "lighting.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <SDL3/SDL.h>

static void draw_world_pattern(Grid *grid, uint64_t frame_count) {
    SDL_Color bg_color = {0, 0, 0, 255};
    grid_clear(grid, bg_color);

    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            uint8_t glyph = 33 + ((x + y + frame_count / 4) % 94);
            
            SDL_Color fg = {
                (uint8_t)(128 + 127 * SDL_sinf((x + frame_count / 10.0f) * 0.1f)),
                (uint8_t)(128 + 127 * SDL_sinf((y + frame_count / 10.0f) * 0.1f)),
                (uint8_t)(255),
                255
            };
            SDL_Color bg = {0, 0, 0, 255};

            if (x == 0 || x == grid->width - 1 || y == 0 || y == grid->height - 1) {
                glyph = '#';
                fg = (SDL_Color){255, 255, 255, 255};
                bg = (SDL_Color){100, 0, 0, 255};
            }

            grid_set(grid, x, y, glyph, fg, bg);
        }
    }
}

static void draw_stress_pattern(Grid *grid, uint64_t frame_count) {
    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            uint8_t glyph = 33 + ((x * 17 + y * 31 + frame_count * 13) % 94);
            
            SDL_Color fg = {
                (uint8_t)((x * 11 + frame_count * 5) % 256),
                (uint8_t)((y * 19 + frame_count * 7) % 256),
                (uint8_t)((x * y + frame_count * 3) % 256),
                255
            };
            SDL_Color bg = {
                (uint8_t)(255 - fg.r),
                (uint8_t)(255 - fg.g),
                (uint8_t)(255 - fg.b),
                255
            };
            
            if (x == 0 || x == grid->width - 1 || y == 0 || y == grid->height - 1) {
                glyph = '#';
                fg = (SDL_Color){255, 255, 255, 255};
                bg = (SDL_Color){100, 0, 0, 255};
            }

            grid_set(grid, x, y, glyph, fg, bg);
        }
    }
}

static void draw_ui_overlay(Grid *grid, uint64_t frame_count, PerfStats *stats, VisualMode mode) {
    char ui_text[1024];
    
    const char* mode_str = "NORMAL PATTERN";
    if (mode == VISUAL_STRESS) mode_str = "STRESS PATTERN";
    else if (mode == VISUAL_RAYCAST) mode_str = "RAYCAST WORLD";

    snprintf(ui_text, sizeof(ui_text), 
             "Grid: %dx%d\n"
             "Frame: %llu\n"
             "Mode: %s\n"
             "Press ESC to quit\n"
             "---\n"
             "[1-Second Rolling Stats]\n"
             "Target FPS: %d\n"
             "Actual FPS: %.1f\n"
             "Avg Frame Time: %.2f ms\n"
             "Worst Frame Time: %.2f ms\n"
             "Min Spare Time: %.2f ms\n"
             "Status: %s", 
             grid->width, grid->height, 
             (unsigned long long)frame_count,
             mode_str,
             TARGET_FPS,
             stats->pub_avg_fps,
             stats->pub_avg_frame_time_ms,
             stats->pub_worst_frame_time_ms,
             stats->pub_min_spare_time_ms,
             stats->pub_min_spare_time_ms < 0 ? "OVER BUDGET" : "OK");

    SDL_Color ui_fg = {255, 255, 255, 255};
    SDL_Color ui_bg = stats->pub_min_spare_time_ms < 0 ? (SDL_Color){150, 0, 0, 255} : (SDL_Color){0, 0, 0, 255};

    grid_print(grid, 2, 2, ui_text, ui_fg, ui_bg);
}

int app_main(int argc, char* argv[]) {
    RunMode mode = RUN_MODE_NORMAL;
    VisualMode visual_mode = VISUAL_RAYCAST; // default
    double run_duration_seconds = 0.0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--benchmark-stress") == 0 && i + 1 < argc) {
            mode = RUN_MODE_BENCHMARK_STRESS;
            visual_mode = VISUAL_STRESS;
            run_duration_seconds = atof(argv[++i]);
        } else if (strcmp(argv[i], "--stability-test") == 0 && i + 1 < argc) {
            mode = RUN_MODE_STABILITY;
            visual_mode = VISUAL_STRESS;
            run_duration_seconds = atof(argv[++i]);
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            const char* mode_str = argv[++i];
            if (strcmp(mode_str, "normal") == 0) visual_mode = VISUAL_NORMAL;
            else if (strcmp(mode_str, "stress") == 0) visual_mode = VISUAL_STRESS;
            else if (strcmp(mode_str, "raycast") == 0) visual_mode = VISUAL_RAYCAST;
        }
    }

    Renderer *ren = renderer_create(WINDOW_WIDTH, WINDOW_HEIGHT, GRID_WIDTH, GRID_HEIGHT, CELL_WIDTH, CELL_HEIGHT);
    if (!ren) {
        fprintf(stderr, "Failed to initialize renderer. (Headless environment expected)\n");
        return 0;
    }

    if (mode == RUN_MODE_NORMAL) {
        SDL_SetWindowRelativeMouseMode(ren->window, true);
    }

    Grid *grid = grid_create(GRID_WIDTH, GRID_HEIGHT);
    if (!grid) {
        fprintf(stderr, "Failed to initialize grid.\n");
        renderer_destroy(ren);
        return 1;
    }

    AssetRegistry assets;
    asset_registry_init(&assets);
    
    // Palette 0: Empty (unused)
    // Palette 1: Grey Wall
    asset_registry_set_palette(&assets, 1, 
        (SDL_Color){255, 255, 255, 255}, 
        (SDL_Color){150, 150, 150, 255}, 
        (SDL_Color){50, 50, 50, 255});
    // Palette 2: Blue Wall
    asset_registry_set_palette(&assets, 2, 
        (SDL_Color){100, 150, 255, 255}, 
        (SDL_Color){50, 75, 150, 255}, 
        (SDL_Color){20, 30, 50, 255});
    
    asset_registry_set_material(&assets, 1, 1, "#x-.");
    asset_registry_set_material(&assets, 2, 2, "OX+:");
    asset_registry_set_material(&assets, 3, 1, "====");

    const char *map_txt = 
        "1111111111\n"
        "1000000001\n"
        "1022003001\n"
        "1020003001\n"
        "1000000001\n"
        "1111111111\n";
    Map *map = map_load_from_string(map_txt);

    Camera cam;
    camera_init(&cam, 1.5, 1.5, PI / 4.0, PI / 2.0);

    WorldState world;
    world_init(&world);
    
    // Add wall decal
    Decal wall_decal = {
        .surface = DECAL_SURFACE_WALL,
        .map_x = 2, .map_y = 2, .side = 0,
        .u = 0.2, .v = 0.4,
        .width = 0.6, .height = 0.2,
        .text = "This is a decal",
        .fg = {255, 255, 0, 255},
        .use_bg = true, .bg = {50, 0, 0, 255}
    };
    world_add_decal(&world, wall_decal);

    // Add floor decal
    Decal floor_decal = {
        .surface = DECAL_SURFACE_FLOOR,
        .x = 5.0, .y = 3.0,
        .width = 2.0, .height = 0.5,
        .text = "this is the floor",
        .fg = {0, 255, 255, 255},
        .use_bg = false
    };
    world_add_decal(&world, floor_decal);

    // Add ceiling decal
    Decal ceil_decal = {
        .surface = DECAL_SURFACE_CEILING,
        .x = 5.0, .y = 2.0,
        .width = 2.0, .height = 0.5,
        .text = "this is the ceiling",
        .fg = {255, 0, 255, 255},
        .use_bg = false
    };
    world_add_decal(&world, ceil_decal);

    // Add a light source
    world_add_light(&world, 4.5, 2.5, (SDL_Color){255, 255, 255, 255}, 1.0, 4.0, false);

    uint64_t frame_count = 0;
    PerfStats perf_stats;
    perf_stats_init(&perf_stats);
    InputState input = {0};

    uint64_t initial_time = SDL_GetPerformanceCounter();
    uint64_t last_time = initial_time;
    double target_time_ms = timing_target_ms(TARGET_FPS);

    double global_total_render_ms = 0.0;
    double absolute_worst_render_ms = 0.0;
    double second_worst_render_ms = 0.0;
    double global_min_spare_ms = 10000.0;
    bool outlier_trimmed = false;

    int initial_alloc_count = renderer_alloc_count;
    int initial_texture_count = renderer_texture_create_count;

    while (!input.quit) {
        uint64_t start_time = SDL_GetPerformanceCounter();
        double elapsed_total_sec = (double)(start_time - initial_time) / SDL_GetPerformanceFrequency();
        
        if (mode != RUN_MODE_NORMAL && elapsed_total_sec >= run_duration_seconds) {
            input.quit = true;
            break;
        }

        double delta_time_ms = (double)((start_time - last_time) * 1000) / SDL_GetPerformanceFrequency();
        last_time = start_time;

        input_process(&input, mode != RUN_MODE_NORMAL);
        
        double delta_time_sec = delta_time_ms / 1000.0;
        if (visual_mode == VISUAL_RAYCAST) {
            camera_update(&cam, map, &input, delta_time_sec);
        }

        if (visual_mode == VISUAL_STRESS) {
            draw_stress_pattern(grid, frame_count);
        } else if (visual_mode == VISUAL_RAYCAST) {
            lighting_update(map, &world);
            raycast_render(grid, map, &cam, &assets, &world);
        } else {
            draw_world_pattern(grid, frame_count);
        }

        draw_ui_overlay(grid, frame_count, &perf_stats, visual_mode);

        uint64_t render_start = SDL_GetPerformanceCounter();
        renderer_draw(ren, grid);
        uint64_t render_end = SDL_GetPerformanceCounter();
        
        double current_render_ms = (double)((render_end - render_start) * 1000) / SDL_GetPerformanceFrequency();
        global_total_render_ms += current_render_ms;
        
        if (frame_count >= 64) {
            if (current_render_ms > absolute_worst_render_ms) {
                second_worst_render_ms = absolute_worst_render_ms;
                absolute_worst_render_ms = current_render_ms;
            } else if (current_render_ms > second_worst_render_ms) {
                second_worst_render_ms = current_render_ms;
            }
        }

        uint64_t end_time = SDL_GetPerformanceCounter();
        double frame_time_ms = (double)((end_time - start_time) * 1000) / SDL_GetPerformanceFrequency();
        double spare_time_ms = timing_spare_ms(frame_time_ms, target_time_ms);
        
        if (frame_count >= 64 && spare_time_ms < global_min_spare_ms) {
            global_min_spare_ms = spare_time_ms;
        }
        
        perf_stats_update(&perf_stats, delta_time_ms, frame_time_ms, spare_time_ms);

        if (renderer_alloc_count > initial_alloc_count) {
            fprintf(stderr, "ERROR: Per-frame allocation detected!\n");
            input.quit = true;
        }
        if (renderer_texture_create_count > initial_texture_count) {
            fprintf(stderr, "ERROR: Per-frame texture creation detected!\n");
            input.quit = true;
        }

        uint32_t sleep_time = timing_sleep_ms(spare_time_ms);
        if (sleep_time > 0) {
            SDL_Delay(sleep_time);
        }

        frame_count++;
    }

    if (map) map_destroy(map);
    grid_destroy(grid);
    renderer_destroy(ren);

    if (mode == RUN_MODE_BENCHMARK_STRESS || mode == RUN_MODE_STABILITY) {
        double avg_render_ms = frame_count > 0 ? (global_total_render_ms / frame_count) : 0.0;
        
        double effective_worst = absolute_worst_render_ms;
        if (absolute_worst_render_ms > second_worst_render_ms * 2.0 && second_worst_render_ms > 0) {
            effective_worst = second_worst_render_ms;
            outlier_trimmed = true;
        }

        const char *result_str = "fail";
        int exit_code = 1;

        if (renderer_alloc_count > initial_alloc_count || renderer_texture_create_count > initial_texture_count) {
            result_str = "fail_allocation_detected";
        } else {
            if (avg_render_ms <= 4.0 && effective_worst <= 6.0) {
                result_str = "ideal";
                exit_code = 0;
            } else if (avg_render_ms <= 6.0 && effective_worst <= 8.0) {
                result_str = "pass_minimum";
                exit_code = 0;
            } else {
                result_str = "fail_performance";
                exit_code = 1;
            }
        }

        printf("{\n");
        printf("  \"grid_width\": %d,\n", GRID_WIDTH);
        printf("  \"grid_height\": %d,\n", GRID_HEIGHT);
        printf("  \"target_fps\": %d,\n", TARGET_FPS);
        printf("  \"avg_render_ms\": %.2f,\n", avg_render_ms);
        printf("  \"worst_render_ms\": %.2f,\n", absolute_worst_render_ms);
        printf("  \"effective_worst_ms\": %.2f,\n", effective_worst);
        printf("  \"outlier_trimmed\": %s,\n", outlier_trimmed ? "true" : "false");
        printf("  \"min_spare_ms\": %.2f,\n", global_min_spare_ms);
        printf("  \"frames\": %llu,\n", (unsigned long long)frame_count);
        printf("  \"result\": \"%s\"\n", result_str);
        printf("}\n");
        
        return exit_code;
    }

    return 0;
}
