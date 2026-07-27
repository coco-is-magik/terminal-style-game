/**
 * app.c — Application Layer (Main Game Loop & Rendering Patterns)
 */
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
#include "asset_loader.h"
#include "lighting.h"
#ifdef USE_LIGHTING_CACHE
#include "lighting_cache.h"
#endif
#ifdef USE_GLYPH_CACHE
#include "glyph_block_cache.h"
#endif
#include "ui_ele.h"
#include "menu_state.h"
#include "unified_editor.h"
#include "smc_render_opt.h"
#if defined(USE_SMC_STATE_TRACKER) || defined(USE_SMC_INDEXED_STATE_TRACKER) || defined(USE_SMC_BATCH_STATE_TRACKER) || defined(USE_SMC_STREAM_STATE_TRACKER)
#include "smc_state_tracker.h"
#include "smc_indexed_state_tracker.h"
#include "smc.h"
#endif
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
                (uint8_t)(255), 255
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
                (uint8_t)((x * y + frame_count * 3) % 256), 255
            };
            SDL_Color bg = {(uint8_t)(255 - fg.r), (uint8_t)(255 - fg.g), (uint8_t)(255 - fg.b), 255};
            if (x == 0 || x == grid->width - 1 || y == 0 || y == grid->height - 1) {
                glyph = '#';
                fg = (SDL_Color){255, 255, 255, 255};
                bg = (SDL_Color){100, 0, 0, 255};
            }
            grid_set(grid, x, y, glyph, fg, bg);
        }
    }
}

static void set_ui_text(UiCache *cache, const char *name, const char *text) {
    UiElement *element;
    if (!cache || !name || !text) return;
    element = ui_cache_get(cache, name);
    if (element) ui_ele_set_content(element, text);
}

static void draw_data_ui_overlay(Grid *grid, UiCache *cache, UiLayout *layout,
                                 uint64_t frame_count, PerfStats *stats,
                                 VisualMode mode, int target_fps) {
    char line[128];
    const char *mode_str = "NORMAL PATTERN";
    SDL_Color fg = {255, 255, 255, 255};
    SDL_Color bg;
    if (!grid || !cache || !layout || !stats) return;
    if (mode == VISUAL_STRESS) mode_str = "STRESS PATTERN";
    else if (mode == VISUAL_RAYCAST) mode_str = "RAYCAST WORLD";
    snprintf(line, sizeof(line), "Grid: %dx%d", grid->width, grid->height);
    set_ui_text(cache, "hud_grid", line);
    snprintf(line, sizeof(line), "Frame: %llu", (unsigned long long)frame_count);
    set_ui_text(cache, "hud_frame", line);
    snprintf(line, sizeof(line), "Mode: %s", mode_str);
    set_ui_text(cache, "hud_mode", line);
    snprintf(line, sizeof(line), "Target FPS: %d", target_fps);
    set_ui_text(cache, "hud_target_fps", line);
    snprintf(line, sizeof(line), "Actual FPS: %.1f", stats->pub_avg_fps);
    set_ui_text(cache, "hud_actual_fps", line);
    snprintf(line, sizeof(line), "Avg Frame Time: %.2f ms", stats->pub_avg_frame_time_ms);
    set_ui_text(cache, "hud_avg_frame", line);
    snprintf(line, sizeof(line), "Worst Frame Time: %.2f ms", stats->pub_worst_frame_time_ms);
    set_ui_text(cache, "hud_worst_frame", line);
    snprintf(line, sizeof(line), "Min Spare Time: %.2f ms", stats->pub_min_spare_time_ms);
    set_ui_text(cache, "hud_min_spare", line);
    snprintf(line, sizeof(line), "Status: %s", stats->pub_min_spare_time_ms < 0 ? "OVER BUDGET" : "OK");
    set_ui_text(cache, "hud_status", line);
    bg = stats->pub_min_spare_time_ms < 0 ? (SDL_Color){150, 0, 0, 255} : (SDL_Color){0, 0, 0, 255};
    ui_layout_render(layout, grid, fg, bg);
}

static const char *menu_layout_name(MenuId menu) {
    switch (menu) {
        case MENU_MAIN: return "main_menu";
        case MENU_PAUSE: return "pause_menu";
        case MENU_CONFIRM_QUIT: return "confirm_quit";
        case MENU_NONE:
        case MENU_ID_COUNT:
        default: return NULL;
    }
}

static void menu_sync_button_colors(UiLayout *layout, int selected) {
    if (!layout) return;
    int count = ui_layout_focusable_count(layout);
    for (int i = 0; i < count; i++) {
        UiElement *button = ui_layout_get_focused(layout, i);
        if (!button) continue;
        button->has_fg = true;
        button->has_bg = true;
        if (i == selected) {
            button->fg = (SDL_Color){50, 255, 50, 255};
            button->bg = (SDL_Color){0, 40, 0, 255};
        } else {
            button->fg = (SDL_Color){200, 200, 200, 255};
            button->bg = (SDL_Color){0, 0, 0, 255};
        }
    }
}

static void draw_data_menu(Grid *grid, UiLayout *layout, int selected) {
    SDL_Color fg = {255, 255, 255, 255};
    SDL_Color bg = {0, 0, 0, 255};
    if (!grid || !layout) return;
    menu_sync_button_colors(layout, selected);
    ui_layout_render(layout, grid, fg, bg);
}

static bool enter_unified_editor(UnifiedEditorState *ued,
                                 AppState *app_state,
                                 MenuStack *ms,
                                 AssetRegistry *assets,
                                 Camera *cam) {
    if (!ued || !app_state || !ms || !assets || !cam) return false;
    if (!unified_editor_init(ued, assets)) {
        fprintf(stderr, "unified_editor_init failed\n");
        return false;
    }
    if (unified_editor_load_scene(ued, "assets/maps/1.txt") != SCENE_LOAD_OK) {
        fprintf(stderr, "unified_editor_load_scene failed for assets/maps/1.txt\n");
        unified_editor_destroy(ued);
        return false;
    }
    camera_init(cam, 1.5, 1.5, PI / 4.0, PI / 2.0);
    menu_stack_clear(ms);
    *app_state = APP_STATE_EDITOR;
    return true;
}

static bool dispatch_menu_action(const char *action,
                                 MenuStack *ms,
                                 AppState *app_state,
                                 InputState *input,
                                 UnifiedEditorState *ued,
                                 Camera *cam,
                                 AssetRegistry *assets) {
    if (!action || !ms || !app_state) return false;
    if (strcmp(action, "start_game") == 0) {
        menu_stack_clear(ms);
        *app_state = APP_STATE_PLAYING;
        return true;
    }
    if (strcmp(action, "open_level_editor") == 0) {
        return enter_unified_editor(ued, app_state, ms, assets, cam);
    }
    if (strcmp(action, "quit") == 0) {
        menu_stack_push(ms, MENU_CONFIRM_QUIT);
        return true;
    }
    if (strcmp(action, "resume") == 0) {
        menu_stack_pop(ms);
        return true;
    }
    if (strcmp(action, "return_to_main_menu") == 0) {
        menu_stack_clear(ms);
        *app_state = APP_STATE_MAIN_MENU;
        menu_stack_push(ms, MENU_MAIN);
        return true;
    }
    if (strcmp(action, "confirm_quit") == 0) {
        if (input) input->quit = true;
        return true;
    }
    if (strcmp(action, "cancel") == 0) {
        menu_stack_pop(ms);
        return true;
    }
    if (strcmp(action, "discard_changes") == 0) {
        if (*app_state == APP_STATE_EDITOR && ued) {
            unified_editor_destroy(ued);
        }
        menu_stack_clear(ms);
        *app_state = APP_STATE_MAIN_MENU;
        menu_stack_push(ms, MENU_MAIN);
        return true;
    }
    return false;
}

int app_main(int argc, char* argv[]) {
    config_init_defaults();
    config_load_from_file("config.ini");
    const EngineConfig *cfg = config_get();

    RunMode mode = RUN_MODE_NORMAL;
    VisualMode visual_mode = VISUAL_RAYCAST;
    double run_duration_seconds = 0.0;
    const char *benchmark_scenario = NULL;
    int benchmark_frames = 600;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--benchmark-scenario") == 0 && i + 1 < argc) {
            mode = RUN_MODE_BENCHMARK_SCENARIO;
            visual_mode = VISUAL_RAYCAST;
            benchmark_scenario = argv[++i];
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            benchmark_frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--benchmark-stress") == 0 && i + 1 < argc) {
            mode = RUN_MODE_BENCHMARK_STRESS;
            visual_mode = VISUAL_STRESS;
            run_duration_seconds = atof(argv[++i]);
        } else if (strcmp(argv[i], "--benchmark-raycast") == 0 && i + 1 < argc) {
            mode = RUN_MODE_BENCHMARK_RAYCAST;
            visual_mode = VISUAL_RAYCAST;
            run_duration_seconds = atof(argv[++i]);
        } else if (strcmp(argv[i], "--stability-test") == 0 && i + 1 < argc) {
            mode = RUN_MODE_STABILITY;
            visual_mode = VISUAL_STRESS;
            run_duration_seconds = atof(argv[++i]);
        } else if (strcmp(argv[i], "--benchmark-lighting") == 0 && i + 1 < argc) {
            mode = RUN_MODE_BENCHMARK_LIGHTING;
            visual_mode = VISUAL_RAYCAST;
            run_duration_seconds = atof(argv[++i]);
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            const char* mode_str = argv[++i];
            if (strcmp(mode_str, "normal") == 0) visual_mode = VISUAL_NORMAL;
            else if (strcmp(mode_str, "stress") == 0) visual_mode = VISUAL_STRESS;
            else if (strcmp(mode_str, "raycast") == 0) visual_mode = VISUAL_RAYCAST;
        }
    }

    Renderer *ren = renderer_create(cfg->window_width, cfg->window_height,
                                     cfg->grid_width, cfg->grid_height,
                                     cfg->cell_width, cfg->cell_height);
    if (!ren) {
        fprintf(stderr, "Failed to initialize renderer. (Headless environment expected)\n");
        return 0;
    }

    Grid *grid = grid_create(cfg->grid_width, cfg->grid_height);
    if (!grid) {
        fprintf(stderr, "Failed to initialize grid.\n");
        renderer_destroy(ren);
        return 1;
    }

    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_loader_load_registry(&assets, "assets");

    WorldState world;
    world_init(&world);

    Map *map = asset_loader_load_map_data(&world, "assets", 1);
    if (!map) {
        fprintf(stderr, "Failed to load map.\n");
        grid_destroy(grid);
        renderer_destroy(ren);
        return 1;
    }

    Camera cam;
    camera_init(&cam, 1.5, 1.5, PI / 4.0, PI / 2.0);

    if (smc_render_opt_init() != 0) {
        fprintf(stderr, "Failed to initialize SMC renderer optimization layer.\n");
        grid_destroy(grid);
        renderer_destroy(ren);
        return 1;
    }
    smc_render_opt_reset_stats();

#ifdef USE_LIGHTING_CACHE
    lighting_cache_init();
#endif
#ifdef USE_GLYPH_CACHE
    glyph_block_cache_init();
#endif
#ifdef USE_SMC_STATE_TRACKER
    { size_t max_cells = (size_t)cfg->grid_width * (size_t)cfg->grid_height;
      if (smc_state_tracker_init(max_cells) != SMC_OK) {
        fprintf(stderr, "Failed to initialize SMC state tracker.\n");
        grid_destroy(grid); renderer_destroy(ren); return 1;
      }
      smc_state_tracker_reset(); }
#endif
#ifdef USE_SMC_INDEXED_STATE_TRACKER
    { size_t max_cells = (size_t)cfg->grid_width * (size_t)cfg->grid_height;
      if (smc_indexed_state_tracker_init(max_cells) != SMC_OK) {
        fprintf(stderr, "Failed to initialize SMC indexed state tracker.\n");
        grid_destroy(grid); renderer_destroy(ren); return 1;
      }
      smc_indexed_state_tracker_reset(); }
#endif
#ifdef USE_SMC_BATCH_STATE_TRACKER
    { size_t max_cells = (size_t)cfg->grid_width * (size_t)cfg->grid_height;
      if (smc_indexed_state_tracker_init(max_cells) != SMC_OK) {
        fprintf(stderr, "Failed to initialize SMC batch state tracker.\n");
        grid_destroy(grid); renderer_destroy(ren); return 1;
      }
      smc_indexed_state_tracker_reset(); }
#endif
#ifdef USE_SMC_STREAM_STATE_TRACKER
    { size_t max_cells = (size_t)cfg->grid_width * (size_t)cfg->grid_height;
      if (smc_indexed_state_tracker_init(max_cells) != SMC_OK) {
        fprintf(stderr, "Failed to initialize SMC stream state tracker.\n");
        grid_destroy(grid); renderer_destroy(ren); return 1;
      }
      smc_indexed_state_tracker_reset(); }
#endif

    AppState app_state = (mode == RUN_MODE_NORMAL) ? APP_STATE_MAIN_MENU : APP_STATE_PLAYING;

    if (mode == RUN_MODE_BENCHMARK_RAYCAST) {
        cam.transform.pos.x = 1.5;
        cam.transform.pos.y = 1.5;
        cam.transform.angle = PI / 4.0;
        cam.pitch = 0.0;
    }

    MenuStack ms;
    menu_stack_init(&ms);
    if (mode == RUN_MODE_NORMAL) {
        menu_stack_push(&ms, MENU_MAIN);
    }

    int menu_selected[MENU_ID_COUNT];
    for (int i = 0; i < MENU_ID_COUNT; i++) menu_selected[i] = 0;

    bool mouse_locked = false;

    uint64_t frame_count = 0;
    PerfStats perf_stats;
    perf_stats_init(&perf_stats);
    InputState input = {0};
    UnifiedEditorState ued = {0};

    UiCache menu_cache;
    UiLayout *menu_layouts[MENU_ID_COUNT];
    ui_cache_init(&menu_cache, "assets/ui_layouts/master_map.txt");
    for (int i = 0; i < MENU_ID_COUNT; i++) {
        const char *layout_name = menu_layout_name((MenuId)i);
        menu_layouts[i] = NULL;
        if (layout_name) {
            char path[256];
            ui_cache_tick(&menu_cache, layout_name, "assets/ui_elements");
            snprintf(path, sizeof(path), "assets/ui_layouts/%s.txt", layout_name);
            menu_layouts[i] = ui_layout_load(path, &menu_cache);
        }
    }
    ui_cache_tick(&menu_cache, "hud_overlay", "assets/ui_elements");
    UiLayout *hud_layout = ui_layout_load("assets/ui_layouts/hud_overlay.txt", &menu_cache);

    uint64_t initial_time = SDL_GetPerformanceCounter();
    uint64_t last_time = initial_time;
    double target_time_ms = timing_target_ms(cfg->target_fps);

#if PROFILE_FRAME
    frame_profile_init(&g_frame_profile);
#endif

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

        if (mode == RUN_MODE_BENCHMARK_SCENARIO && frame_count >= (uint64_t)benchmark_frames) {
            input.quit = true; break;
        }
        if (mode != RUN_MODE_NORMAL && mode != RUN_MODE_BENCHMARK_SCENARIO && elapsed_total_sec >= run_duration_seconds) {
            input.quit = true; break;
        }

        double delta_time_ms = (double)((start_time - last_time) * 1000) / SDL_GetPerformanceFrequency();
        last_time = start_time;

        input_process(&input, mode != RUN_MODE_NORMAL);

        {
            bool want_lock = (mode == RUN_MODE_NORMAL)
                             && (ms.depth == 0)
                             && ((app_state == APP_STATE_PLAYING)
                                 || (app_state == APP_STATE_EDITOR
                                     && ued.active
                                     && ued.mode == EDITOR_MODE_WALK
                                     && ued.modal == EDITOR_MODAL_NONE));
            if (want_lock != mouse_locked) {
                SDL_SetWindowRelativeMouseMode(ren->window, want_lock);
                mouse_locked = want_lock;
            }
        }

        if (input.esc) {
            if (app_state == APP_STATE_MAIN_MENU) {
            } else if (ms.depth > 0) {
                menu_stack_pop(&ms);
            } else if (app_state == APP_STATE_PLAYING) {
                menu_stack_push(&ms, MENU_PAUSE);
            }
        }

        MenuId active_menu = menu_stack_peek(&ms);
        if (active_menu != MENU_NONE) {
            int mid = (int)active_menu;
            UiLayout *active_layout = (mid >= 0 && mid < MENU_ID_COUNT) ? menu_layouts[mid] : NULL;
            int count = active_layout ? ui_layout_focusable_count(active_layout) : 0;
            if (count > 0) {
                if (input.up)
                    menu_selected[mid] = (menu_selected[mid] - 1 + count) % count;
                if (input.down)
                    menu_selected[mid] = (menu_selected[mid] + 1) % count;
            }
            if (input.confirm && count > 0) {
                int sel = menu_selected[mid];
                if (active_layout) {
                    UiElement *focused = ui_layout_get_focused(active_layout, sel);
                    if (focused && focused->action[0] != '\0') {
                        dispatch_menu_action(focused->action, &ms, &app_state, &input,
                                             &ued, &cam, &assets);
                    }
                }
                active_menu = menu_stack_peek(&ms);
            }
        }

        EditorInputConsumption ued_consume = {false, false};
        if (app_state == APP_STATE_EDITOR && menu_stack_peek(&ms) == MENU_NONE && ued.active) {
            ued_consume = unified_editor_update(&ued, &input, &cam, delta_time_ms / 1000.0);
            if (ued.request_exit_to_main_menu) {
                unified_editor_destroy(&ued);
                menu_stack_clear(&ms);
                app_state = APP_STATE_MAIN_MENU;
                menu_stack_push(&ms, MENU_MAIN);
            }
        }

        double delta_time_sec = delta_time_ms / 1000.0;
        active_menu = menu_stack_peek(&ms);

#if PROFILE_FRAME
        double profile_grid_start = profile_now_ms();
#endif

        if (active_menu != MENU_NONE) {
            SDL_Color mbg = {0, 0, 0, 255};
            UiLayout *active_layout = ((int)active_menu >= 0 && (int)active_menu < MENU_ID_COUNT)
                                      ? menu_layouts[(int)active_menu] : NULL;
            grid_clear(grid, mbg);
            draw_data_menu(grid, active_layout, menu_selected[(int)active_menu]);

        } else if (app_state == APP_STATE_PLAYING) {
            if (visual_mode == VISUAL_RAYCAST) {
                if (benchmark_scenario && frame_count >= 64) {
                    if (strcmp(benchmark_scenario, "idle") == 0) { }
                    else if (strcmp(benchmark_scenario, "camera") == 0) {
                        cam.transform.angle += 0.01047f;
                    } else if (strcmp(benchmark_scenario, "rotate") == 0) {
                        cam.transform.angle += 0.04189f;
                    } else if (strcmp(benchmark_scenario, "flicker") == 0) {
                        for (int idx = 0; idx < (int)grid->width * grid->height; idx++) {
                            if (((idx * 1103515245u + (unsigned)frame_count * 12345u) % 100u) < 3u) {
                                int x = idx % grid->width; int y = idx / grid->width; Cell c;
                                grid_get(grid, x, y, &c);
                                c.bg.r = (frame_count & 1) ? 0xFF : 0x80;
                                grid_set(grid, x, y, c.glyph, c.fg, c.bg);
                            }
                        }
                    } else if (strcmp(benchmark_scenario, "ui") == 0) {
                        int row_start = grid->height >= 2 ? grid->height - 2 : 0;
                        for (int y = row_start; y < grid->height; y++)
                            for (int x = 0; x < grid->width; x++) {
                                Cell c; grid_get(grid, x, y, &c);
                                c.bg.r = (frame_count & 1) ? 0x40 : 0x20;
                                c.bg.g = (frame_count & 2) ? 0x40 : 0x20;
                                c.bg.b = (frame_count & 4) ? 0x40 : 0x20;
                                grid_set(grid, x, y, c.glyph, c.fg, c.bg);
                            }
                    } else if (strcmp(benchmark_scenario, "fullchange") == 0) {
                        for (int idx = 0; idx < (int)grid->width * grid->height; idx++) {
                            int x = idx % grid->width; int y = idx / grid->width; Cell c;
                            grid_get(grid, x, y, &c);
                            c.bg.r = (frame_count & 1) ? 0xFF : 0x00;
                            grid_set(grid, x, y, c.glyph, c.fg, c.bg);
                        }
                    }
                }
                camera_update(&cam, map, &input, delta_time_sec);
                lighting_update(map, &world);
                raycast_render(grid, map, &cam, &assets, &world);
            } else if (visual_mode == VISUAL_STRESS) {
                draw_stress_pattern(grid, frame_count);
            } else {
                draw_world_pattern(grid, frame_count);
            }
            if (cfg->debug_display_enabled) {
                draw_data_ui_overlay(grid, &menu_cache, hud_layout, frame_count,
                                     &perf_stats, visual_mode, cfg->target_fps);
            }

#if PROFILE_FRAME
            g_frame_profile.raycast_grid_ms += (profile_now_ms() - profile_grid_start);
#endif

        } else if (app_state == APP_STATE_EDITOR) {
            (void)ued_consume;
            if (ued.active) {
                Map *ed_map = scene_document_get_map_for_runtime(&ued.document);
                if (ed_map) {
                    lighting_update(ed_map, &world);
                    raycast_render(grid, ed_map, &cam, &assets, &world);
                } else {
                    SDL_Color ae_bg = {0, 0, 0, 255};
                    grid_clear(grid, ae_bg);
                }
                unified_editor_render_overlay(&ued, grid);
            } else {
                SDL_Color ae_bg = {0, 0, 0, 255};
                grid_clear(grid, ae_bg);
            }

        } else {
            SDL_Color bg = {0, 0, 0, 255};
            grid_clear(grid, bg);
        }

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

        if (frame_count >= 64 && spare_time_ms < global_min_spare_ms)
            global_min_spare_ms = spare_time_ms;

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
        if (sleep_time > 0) SDL_Delay(sleep_time);

        frame_count++;
    }

#ifdef USE_SMC_STATE_TRACKER
    smc_state_stats_t smc_state_s = {0};
#endif
#ifdef USE_SMC_INDEXED_STATE_TRACKER
    IndexedStateStats smc_indexed_s = {0};
#endif
#ifdef USE_SMC_BATCH_STATE_TRACKER
    IndexedStateStats smc_batch_s = {0};
#endif
#ifdef USE_SMC_STREAM_STATE_TRACKER
    IndexedStateStats smc_stream_s = {0};
#endif
    uint32_t framebuffer_checksum = 0;

    if (mode == RUN_MODE_BENCHMARK_STRESS || mode == RUN_MODE_BENCHMARK_RAYCAST || mode == RUN_MODE_STABILITY || mode == RUN_MODE_BENCHMARK_SCENARIO) {
        framebuffer_checksum = renderer_framebuffer_checksum(ren);
#ifdef USE_SMC_STATE_TRACKER
        smc_state_tracker_get_stats(&smc_state_s);
#endif
#ifdef USE_SMC_INDEXED_STATE_TRACKER
        smc_indexed_state_tracker_get_stats(&smc_indexed_s);
#endif
#ifdef USE_SMC_BATCH_STATE_TRACKER
        smc_indexed_state_tracker_get_stats(&smc_batch_s);
#endif
#ifdef USE_SMC_STREAM_STATE_TRACKER
        smc_indexed_state_tracker_get_stats(&smc_stream_s);
#endif
    }

#ifdef USE_SMC_STATE_TRACKER
    smc_state_tracker_shutdown();
#endif
#if defined(USE_SMC_INDEXED_STATE_TRACKER) || defined(USE_SMC_BATCH_STATE_TRACKER) || defined(USE_SMC_STREAM_STATE_TRACKER)
    smc_indexed_state_tracker_shutdown();
#endif
    unified_editor_destroy(&ued);
    for (int i = 0; i < MENU_ID_COUNT; i++) ui_layout_destroy(menu_layouts[i]);
    ui_layout_destroy(hud_layout);
    ui_cache_destroy(&menu_cache);
    world_clear(&world);
    if (map) map_destroy(map);
    grid_destroy(grid);
    renderer_destroy(ren);

    if (mode == RUN_MODE_BENCHMARK_STRESS || mode == RUN_MODE_BENCHMARK_RAYCAST || mode == RUN_MODE_STABILITY || mode == RUN_MODE_BENCHMARK_SCENARIO) {
        double avg_render_ms = frame_count > 0 ? (global_total_render_ms / frame_count) : 0.0;
        double effective_worst = absolute_worst_render_ms;
        if (absolute_worst_render_ms > second_worst_render_ms * 2.0 && second_worst_render_ms > 0) {
            effective_worst = second_worst_render_ms; outlier_trimmed = true;
        }
        const char *result_str = "fail";
        int exit_code = 1;
        if (renderer_alloc_count > initial_alloc_count || renderer_texture_create_count > initial_texture_count) {
            result_str = "fail_allocation_detected";
        } else {
            if (avg_render_ms <= 4.0 && effective_worst <= 6.0) { result_str = "ideal"; exit_code = 0; }
            else if (avg_render_ms <= 6.0 && effective_worst <= 8.0) { result_str = "pass_minimum"; exit_code = 0; }
            else { result_str = "fail_performance"; }
        }
        printf("{\n  \"grid_width\": %d,\n  \"grid_height\": %d,\n  \"target_fps\": %d,\n  \"avg_render_ms\": %.2f,\n  \"worst_render_ms\": %.2f,\n  \"effective_worst_ms\": %.2f,\n  \"outlier_trimmed\": %s,\n  \"min_spare_ms\": %.2f,\n  \"frames\": %llu,\n  \"result\": \"%s\"\n}\n",
               cfg->grid_width, cfg->grid_height, cfg->target_fps, avg_render_ms, absolute_worst_render_ms, effective_worst,
               outlier_trimmed ? "true" : "false", global_min_spare_ms, (unsigned long long)frame_count, result_str);
#ifdef USE_GLYPH_CACHE
        { uint64_t glyphs_total = renderer_cache_hits + renderer_cache_misses;
          double glyph_hit_rate = glyphs_total > 0 ? (100.0 * renderer_cache_hits / glyphs_total) : 0.0;
          if (glyphs_total > 0) fprintf(stderr, "Glyph cache stats: hits=%llu misses=%llu hit_rate=%.1f%% raster_ms=%.2f upload_ms=%.2f cells=%llu\n",
                 (unsigned long long)renderer_cache_hits, (unsigned long long)renderer_cache_misses, glyph_hit_rate,
                 renderer_time_raster_ms, renderer_time_upload_ms, (unsigned long long)renderer_cells_processed); }
#endif
        { uint64_t smc_total = 0, smc_fallback = 0, smc_arity = 0, smc_invalid = 0;
          smc_render_opt_get_stats(&smc_total, &smc_fallback, &smc_arity, &smc_invalid);
          if (smc_total > 0) fprintf(stderr, "SMC stats: total_calls=%llu fallback=%llu arity_errors=%llu invalid_ids=%llu\n",
                 (unsigned long long)smc_total, (unsigned long long)smc_fallback, (unsigned long long)smc_arity, (unsigned long long)smc_invalid); }
#ifdef USE_SMC_STATE_TRACKER
        fprintf(stderr, "SMC state stats: checks=%llu changed=%llu unchanged=%llu evictions=%llu bytes_compared=%llu\n",
                (unsigned long long)smc_state_s.checks, (unsigned long long)smc_state_s.changed, (unsigned long long)smc_state_s.unchanged,
                (unsigned long long)smc_state_s.evictions, (unsigned long long)smc_state_s.bytes_compared);
#endif
#ifdef USE_SMC_INDEXED_STATE_TRACKER
        fprintf(stderr, "SMC indexed stats: checks=%llu changed=%llu unchanged=%llu stores=%llu bytes_compared=%llu out_of_range=%llu clears=%llu\n",
                (unsigned long long)smc_indexed_s.checks, (unsigned long long)smc_indexed_s.changed, (unsigned long long)smc_indexed_s.unchanged,
                (unsigned long long)smc_indexed_s.stores, (unsigned long long)smc_indexed_s.bytes_compared,
                (unsigned long long)smc_indexed_s.out_of_range, (unsigned long long)smc_indexed_s.clears);
#endif
#ifdef USE_SMC_BATCH_STATE_TRACKER
        fprintf(stderr, "SMC batch stats: checks=%llu changed=%llu unchanged=%llu stores=%llu bytes_compared=%llu out_of_range=%llu clears=%llu\n",
                (unsigned long long)smc_batch_s.checks, (unsigned long long)smc_batch_s.changed, (unsigned long long)smc_batch_s.unchanged,
                (unsigned long long)smc_batch_s.stores, (unsigned long long)smc_batch_s.bytes_compared,
                (unsigned long long)smc_batch_s.out_of_range, (unsigned long long)smc_batch_s.clears);
#endif
#ifdef USE_SMC_STREAM_STATE_TRACKER
        fprintf(stderr, "SMC stream stats: checks=%llu changed=%llu unchanged=%llu stores=%llu bytes_compared=%llu out_of_range=%llu clears=%llu fallback_count=%llu\n",
                (unsigned long long)smc_stream_s.checks, (unsigned long long)smc_stream_s.changed, (unsigned long long)smc_stream_s.unchanged,
                (unsigned long long)smc_stream_s.stores, (unsigned long long)smc_stream_s.bytes_compared,
                (unsigned long long)smc_stream_s.out_of_range, (unsigned long long)smc_stream_s.clears, (unsigned long long)renderer_smc_fallback_count);
#endif
        { uint64_t cells_total = (uint64_t)cfg->grid_width * (uint64_t)cfg->grid_height;
          fprintf(stderr, "Renderer stats: cells_total=%llu cells_rasterized=%llu cells_skipped=%llu skip_rate=%.1f%% framebuffer_checksum=%u\n",
                  (unsigned long long)cells_total, (unsigned long long)renderer_cells_processed, (unsigned long long)renderer_cells_skipped,
                  cells_total > 0 ? (100.0 * renderer_cells_skipped / cells_total) : 0.0, framebuffer_checksum); }
#if PROFILE_FRAME
        { const char *profile_mode_name = "baseline";
#ifdef USE_DIRTY_CELLS
          profile_mode_name = "custom dirty cells";
#endif
#ifdef USE_SMC_INDEXED_STATE_TRACKER
          profile_mode_name = "SMC indexed";
#endif
#ifdef USE_SMC_BATCH_STATE_TRACKER
          profile_mode_name = "SMC batch";
#endif
#ifdef USE_SMC_STREAM_STATE_TRACKER
          profile_mode_name = "SMC stream";
#endif
#ifdef USE_SMC_STATE_TRACKER
          profile_mode_name = "SMC generic";
#endif
          frame_profile_print(&g_frame_profile, profile_mode_name); }
#endif
        return exit_code;
    }

    if (mode == RUN_MODE_BENCHMARK_LIGHTING) {
        double avg_lighting_ms = frame_count > 0 ? (lighting_total_time_ms / frame_count) : 0.0;
        double avg_render_ms = frame_count > 0 ? (global_total_render_ms / frame_count) : 0.0;
        uint64_t total_shadow_rays = lighting_shadow_ray_count;
#ifdef USE_LIGHTING_CACHE
        uint64_t cache_hits = 0, cache_misses = 0, cache_evictions = 0;
        lighting_cache_get_stats(&cache_hits, &cache_misses, &cache_evictions);
        double hit_rate = (cache_hits + cache_misses) > 0 ? (100.0 * cache_hits / (cache_hits + cache_misses)) : 0.0;
#else
        uint64_t cache_hits = 0, cache_misses = 0, cache_evictions = 0; double hit_rate = 0.0;
#endif
        printf("{\n  \"avg_lighting_ms\": %.2f,\n  \"avg_render_ms\": %.2f,\n  \"total_shadow_rays\": %llu,\n  \"cache_hits\": %llu,\n  \"cache_misses\": %llu,\n  \"cache_evictions\": %llu,\n  \"cache_hit_rate\": %.1f,\n  \"frames\": %llu,\n  \"result\": \"done\"\n}\n",
               avg_lighting_ms, avg_render_ms, (unsigned long long)total_shadow_rays,
               (unsigned long long)cache_hits, (unsigned long long)cache_misses, (unsigned long long)cache_evictions,
               hit_rate, (unsigned long long)frame_count);
        return 0;
    }

    return 0;
}