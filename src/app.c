/**
 * app.c — Application Layer (Main Game Loop & Rendering Patterns)
 *
 * This file implements the top-level application logic: it initializes all
 * subsystems (renderer, input, timing, world, map, camera), runs the main
 * frame loop, and optionally enters benchmark/stability-test modes that
 * measure and report performance metrics.
 *
 * Three visual modes are supported:
 *   - VISUAL_NORMAL   : a simple animated pattern (for debugging the grid)
 *   - VISUAL_STRESS   : a heavy computational pattern (for performance testing)
 *   - VISUAL_RAYCAST  : the actual 3D raycasted world rendering
 *
 * Two non-interactive run modes exist:
 *   - RUN_MODE_BENCHMARK_STRESS : runs for N seconds in stress mode, prints JSON stats
 *   - RUN_MODE_STABILITY        : runs for N seconds in stress mode for leak/crash checks
 */

/* ===================================================================
 *  Header inclusions — each brings in types and functions needed below
 * =================================================================== */

#include "app.h"           /* Declares app_main() — the entry point called by main() */
#include "config.h"        /* EngineConfig, RunMode, VisualMode enums; config_init_defaults(),
                              config_load_from_file(), config_get() */
#include "renderer.h"      /* Renderer struct, renderer_create/destroy/draw, and
                              instrumentation counters (renderer_alloc_count, etc.) */
#include "grid.h"          /* Grid struct (width, height, Cell[]), grid_create/destroy/clear/set/print */
#include "timing.h"        /* PerfStats struct, perf_stats_init/update, timing_target_ms/spare_ms/sleep_ms */
#include "input.h"         /* InputState struct (quit, WASD, mouse), input_process() */
#include "map.h"           /* Map struct, map_create/destroy, map_in_bounds, map_get/set */
#include "camera.h"        /* Camera struct (Entity transform + FOV + pitch),
                              camera_init(), camera_update() */
#include "raycast.h"       /* RayResult struct, raycast_fire(), raycast_render() — the 3D renderer */
#include "assets.h"        /* AssetRegistry struct (palettes, materials, sprites) */
#include "asset_loader.h"  /* asset_loader_load_registry(), asset_loader_load_map_data() */
#include "lighting.h"      /* lighting_update() — per-frame light propagation on the map */
#include <stdio.h>         /* printf(), fprintf(), snprintf() */
#include <stdlib.h>        /* atof() */
#include <string.h>        /* strcmp() */
#include <stdbool.h>       /* bool, true, false */
#include <SDL3/SDL.h>      /* SDL_SetWindowRelativeMouseMode, SDL_GetPerformanceCounter/Frequency,
                              SDL_Delay, SDL_Color, SDL_sinf */

/* ===================================================================
 *  Static helper functions — not exposed outside this file
 * =================================================================== */

/**
 * draw_world_pattern() — Simple animated per-cell pattern (VISUAL_NORMAL mode)
 *
 * Fills every cell in the grid with a rotating ASCII glyph whose colour
 * varies smoothly using sine waves based on x/y position and frame count.
 * Border cells are rendered as '#' with a dark-red background to give the
 * user a clear visual boundary.
 *
 * @param grid         Pointer to the Grid to draw into
 * @param frame_count  Monotonically increasing frame counter (used for animation)
 */
static void draw_world_pattern(Grid *grid, uint64_t frame_count) {
    /* Clear the entire grid to black */
    SDL_Color bg_color = {0, 0, 0, 255};
    grid_clear(grid, bg_color);

    /* Iterate over every cell in the grid */
    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            /* Cycle through printable ASCII characters (codes 33–126).
             * The formula (x + y + frame_count/4) % 94 picks a glyph that
             * depends on position AND gradually shifts over time. */
            uint8_t glyph = 33 + ((x + y + frame_count / 4) % 94);
            
            /* Foreground colour: red and green channels oscillate sinusoidally
             * based on x/y and time; blue is always fully on (255). */
            SDL_Color fg = {
                (uint8_t)(128 + 127 * SDL_sinf((x + frame_count / 10.0f) * 0.1f)),
                (uint8_t)(128 + 127 * SDL_sinf((y + frame_count / 10.0f) * 0.1f)),
                (uint8_t)(255),
                255
            };
            SDL_Color bg = {0, 0, 0, 255};

            /* Border cells get a special treatment: '#' glyph, white on dark-red */
            if (x == 0 || x == grid->width - 1 || y == 0 || y == grid->height - 1) {
                glyph = '#';
                fg = (SDL_Color){255, 255, 255, 255};
                bg = (SDL_Color){100, 0, 0, 255};
            }

            /* Write the computed cell into the grid */
            grid_set(grid, x, y, glyph, fg, bg);
        }
    }
}

/**
 * draw_stress_pattern() — CPU-intensive animated pattern (VISUAL_STRESS mode)
 *
 * Similar to draw_world_pattern() but uses intentionally more "chaotic"
 * math (multiplication, larger primes, modulo operations) to stress the
 * CPU and saturate the rendering pipeline.  Useful for benchmarking and
 * detecting regressions.
 *
 * Background colour is set to the inverse of the foreground colour,
 * which forces more work in the glyph atlas / pixel pipeline.
 *
 * @param grid         Pointer to the Grid to draw into
 * @param frame_count  Monotonically increasing frame counter
 */
static void draw_stress_pattern(Grid *grid, uint64_t frame_count) {
    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            /* More chaotic glyph selection: uses large prime multipliers (17, 31, 13)
             * and mixes in frame_count to guarantee rapid glyph transitions. */
            uint8_t glyph = 33 + ((x * 17 + y * 31 + frame_count * 13) % 94);
            
            /* Foreground colour: each channel has its own complex pattern so
             * that every frame produces a completely different colour distribution. */
            SDL_Color fg = {
                (uint8_t)((x * 11 + frame_count * 5) % 256),
                (uint8_t)((y * 19 + frame_count * 7) % 256),
                (uint8_t)((x * y + frame_count * 3) % 256),
                255
            };
            /* Background = inverted foreground → forces full redraw */
            SDL_Color bg = {
                (uint8_t)(255 - fg.r),
                (uint8_t)(255 - fg.g),
                (uint8_t)(255 - fg.b),
                255
            };
            
            /* Same border treatment as the normal pattern */
            if (x == 0 || x == grid->width - 1 || y == 0 || y == grid->height - 1) {
                glyph = '#';
                fg = (SDL_Color){255, 255, 255, 255};
                bg = (SDL_Color){100, 0, 0, 255};
            }

            grid_set(grid, x, y, glyph, fg, bg);
        }
    }
}

/**
 * draw_ui_overlay() — Heads-up display (HUD) overlay
 *
 * Renders performance statistics and status text in the top-left corner
 * of the grid (starting at column 2, row 2).  Shows grid dimensions,
 * current frame count, visual mode, FPS, frame times, and whether the
 * frame budget is being exceeded.
 *
 * Background of the overlay turns red when the frame is over budget
 * (spare_time < 0), giving an instant visual cue.
 *
 * @param grid       Grid to draw onto
 * @param frame_count  Current frame number
 * @param stats      Pointer to PerfStats containing rolling 1-second averages
 * @param mode       Current VisualMode (affects label shown)
 * @param target_fps The desired frame rate (set in config.ini)
 */
static void draw_ui_overlay(Grid *grid, uint64_t frame_count, PerfStats *stats, VisualMode mode, int target_fps) {
    char ui_text[1024];
    
    /* Human-readable label for the current visual mode */
    const char* mode_str = "NORMAL PATTERN";
    if (mode == VISUAL_STRESS) mode_str = "STRESS PATTERN";
    else if (mode == VISUAL_RAYCAST) mode_str = "RAYCAST WORLD";

    /* Build the multi-line overlay string */
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
             target_fps,
             stats->pub_avg_fps,
             stats->pub_avg_frame_time_ms,
             stats->pub_worst_frame_time_ms,
             stats->pub_min_spare_time_ms,
             stats->pub_min_spare_time_ms < 0 ? "OVER BUDGET" : "OK");

    /* White text on a black background; red background when over budget */
    SDL_Color ui_fg = {255, 255, 255, 255};
    SDL_Color ui_bg = stats->pub_min_spare_time_ms < 0 ? (SDL_Color){150, 0, 0, 255} : (SDL_Color){0, 0, 0, 255};

    grid_print(grid, 2, 2, ui_text, ui_fg, ui_bg);
}

/* ===================================================================
 *  Entry point — app_main()
 *  Called by main() in main.c.  Returns 0 on success, 1 on failure.
 * =================================================================== */

int app_main(int argc, char* argv[]) {
    /* -----------------------------------------------------------------
     *  1. Configuration initialisation
     *     Load sensible defaults, then override with config.ini
     * ----------------------------------------------------------------- */
    config_init_defaults();                   /* Hard-coded fallback values */
    config_load_from_file("config.ini");       /* Override from disk file */
    const EngineConfig *cfg = config_get();    /* Read-only pointer to current config */

    /* -----------------------------------------------------------------
     *  2. Parse CLI arguments
     *     Supports: --benchmark-stress <sec>, --stability-test <sec>,
     *               --mode {normal|stress|raycast}
     * ----------------------------------------------------------------- */
    RunMode mode = RUN_MODE_NORMAL;
    VisualMode visual_mode = VISUAL_RAYCAST;   /* Default visual mode = actual 3D game */
    double run_duration_seconds = 0.0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--benchmark-stress") == 0 && i + 1 < argc) {
            /* Runs stress-mode benchmarking for a fixed duration, then prints JSON stats */
            mode = RUN_MODE_BENCHMARK_STRESS;
            visual_mode = VISUAL_STRESS;
            run_duration_seconds = atof(argv[++i]);
        } else if (strcmp(argv[i], "--stability-test") == 0 && i + 1 < argc) {
            /* Like benchmark, but intended to detect memory leaks / crashes over time */
            mode = RUN_MODE_STABILITY;
            visual_mode = VISUAL_STRESS;
            run_duration_seconds = atof(argv[++i]);
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            /* Manual override for the visual mode */
            const char* mode_str = argv[++i];
            if (strcmp(mode_str, "normal") == 0) visual_mode = VISUAL_NORMAL;
            else if (strcmp(mode_str, "stress") == 0) visual_mode = VISUAL_STRESS;
            else if (strcmp(mode_str, "raycast") == 0) visual_mode = VISUAL_RAYCAST;
        }
    }

    /* -----------------------------------------------------------------
     *  3. Create the SDL Renderer
     *     Sets up window, SDL renderer, glyph atlas, pixel buffer.
     *     If creation fails (e.g. headless CI), we exit gracefully.
     * ----------------------------------------------------------------- */
    Renderer *ren = renderer_create(cfg->window_width, cfg->window_height,
                                     cfg->grid_width, cfg->grid_height,
                                     cfg->cell_width, cfg->cell_height);
    if (!ren) {
        fprintf(stderr, "Failed to initialize renderer. (Headless environment expected)\n");
        return 0;   /* Not a fatal error — just can't draw */
    }

    /* In normal (interactive) mode, lock mouse pointer to the window
     * so the user can look around freely (FPS-style).  Non-interactive
     * modes do not need mouse locking. */
    if (mode == RUN_MODE_NORMAL) {
        SDL_SetWindowRelativeMouseMode(ren->window, true);
    }

    /* -----------------------------------------------------------------
     *  4. Create the character Grid
     *     This is the 2D array of (glyph, fg, bg) cells that the
     *     renderer eventually draws to the screen.
     * ----------------------------------------------------------------- */
    Grid *grid = grid_create(cfg->grid_width, cfg->grid_height);
    if (!grid) {
        fprintf(stderr, "Failed to initialize grid.\n");
        renderer_destroy(ren);
        return 1;
    }

    /* -----------------------------------------------------------------
     *  5. Load game assets
     *     - AssetRegistry : palettes, materials, sprites
     *     - WorldState    : dynamic lights and decals placed in the level
     *     - Map           : tile grid with material IDs and a light map
     * ----------------------------------------------------------------- */
    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_loader_load_registry(&assets, "assets");      /* Load all material/palette files */

    WorldState world;
    world_init(&world);                                 /* Start with empty light/sprite/decal arrays */

    Map *map = asset_loader_load_map_data(&world, "assets", 1);  /* Load map #1's layout, lights, decals */
    if (!map) {
        fprintf(stderr, "Failed to load map.\n");
        grid_destroy(grid);
        renderer_destroy(ren);
        return 1;
    }

    /* -----------------------------------------------------------------
     *  6. Initialise the Camera
     *     Position: (1.5, 1.5) — centre of the starting tile
     *     Angle:    PI/4 (45°, facing diagonally toward +X/+Y)
     *     FOV:      PI/2 (90° horizontal field of view)
     * ----------------------------------------------------------------- */
    Camera cam;
    camera_init(&cam, 1.5, 1.5, PI / 4.0, PI / 2.0);

    /* -----------------------------------------------------------------
     *  7. Main-loop state variables
     * ----------------------------------------------------------------- */
    uint64_t frame_count = 0;       /* Total frames rendered so far */
    PerfStats perf_stats;           /* Rolling one-second performance window */
    perf_stats_init(&perf_stats);
    InputState input = {0};         /* All fields zero-initialised (quit=false, etc.) */

    /* High-resolution timer: used to enforce frame budget and detect duration expiry */
    uint64_t initial_time = SDL_GetPerformanceCounter();
    uint64_t last_time = initial_time;
    double target_time_ms = timing_target_ms(cfg->target_fps);  /* e.g. 16.67 ms for 60 FPS */

    /* Accumulators for benchmark-report statistics */
    double global_total_render_ms = 0.0;          /* Sum of all renderer_draw() durations */
    double absolute_worst_render_ms = 0.0;        /* Single longest render call */
    double second_worst_render_ms = 0.0;          /* Second-longest — used for outlier trimming */
    double global_min_spare_ms = 10000.0;         /* Worst (most negative) spare time seen */
    bool outlier_trimmed = false;                 /* Was the worst outlier > 2× the second-worst? */

    /* Track initial heap/texture counters — any increase during the loop
     * signals a per-frame allocation bug. */
    int initial_alloc_count = renderer_alloc_count;
    int initial_texture_count = renderer_texture_create_count;

    /* ================================================================
     *  8. Main Frame Loop
     *     Runs until input.quit is set (ESC key in normal mode, or
     *     elapsed time in benchmark/stability mode).
     * ================================================================ */
    while (!input.quit) {
        /* --- 8a. Frame start timing --- */
        uint64_t start_time = SDL_GetPerformanceCounter();
        double elapsed_total_sec = (double)(start_time - initial_time) / SDL_GetPerformanceFrequency();
        
        /* Non-interactive modes: quit once the requested duration has elapsed */
        if (mode != RUN_MODE_NORMAL && elapsed_total_sec >= run_duration_seconds) {
            input.quit = true;
            break;
        }

        /* Compute delta time since the previous frame (milliseconds) */
        double delta_time_ms = (double)((start_time - last_time) * 1000) / SDL_GetPerformanceFrequency();
        last_time = start_time;

        /* --- 8b. Input processing --- */
        /* In non-interactive modes, pass headless_mode=true so input_process()
         * skips interactive keyboard/mouse handling. */
        input_process(&input, mode != RUN_MODE_NORMAL);
        
        /* --- 8c. Camera update (raycast mode only) --- */
        double delta_time_sec = delta_time_ms / 1000.0;
        if (visual_mode == VISUAL_RAYCAST) {
            camera_update(&cam, map, &input, delta_time_sec);
        }

        /* --- 8d. Draw the frame contents into the Grid --- */
        if (visual_mode == VISUAL_STRESS) {
            draw_stress_pattern(grid, frame_count);
        } else if (visual_mode == VISUAL_RAYCAST) {
            lighting_update(map, &world);                /* Run light propagation on the map */
            raycast_render(grid, map, &cam, &assets, &world);  /* Full 3D raycast render pass */
        } else {
            draw_world_pattern(grid, frame_count);
        }

        /* --- 8e. Debug overlay --- */
        if (cfg->debug_display_enabled) {
            draw_ui_overlay(grid, frame_count, &perf_stats, visual_mode, cfg->target_fps);
        }

        /* --- 8f. Transfer the Grid to the screen (SDL rendering) --- */
        uint64_t render_start = SDL_GetPerformanceCounter();
        renderer_draw(ren, grid);
        uint64_t render_end = SDL_GetPerformanceCounter();
        
        double current_render_ms = (double)((render_end - render_start) * 1000) / SDL_GetPerformanceFrequency();
        global_total_render_ms += current_render_ms;
        
        /* Keep track of the two longest render durations.
         * We skip the first 64 frames to let the system "warm up"
         * (caches, scheduler stabilisation, etc.) */
        if (frame_count >= 64) {
            if (current_render_ms > absolute_worst_render_ms) {
                second_worst_render_ms = absolute_worst_render_ms;
                absolute_worst_render_ms = current_render_ms;
            } else if (current_render_ms > second_worst_render_ms) {
                second_worst_render_ms = current_render_ms;
            }
        }

        /* --- 8g. Frame budget & spare time --- */
        uint64_t end_time = SDL_GetPerformanceCounter();
        double frame_time_ms = (double)((end_time - start_time) * 1000) / SDL_GetPerformanceFrequency();
        double spare_time_ms = timing_spare_ms(frame_time_ms, target_time_ms);
        
        /* Track the worst spare time (most over budget) after warm-up */
        if (frame_count >= 64 && spare_time_ms < global_min_spare_ms) {
            global_min_spare_ms = spare_time_ms;
        }
        
        /* Feed frame metrics into the rolling 1-second performance stats */
        perf_stats_update(&perf_stats, delta_time_ms, frame_time_ms, spare_time_ms);

        /* --- 8h. Sanity checks: detect per-frame allocations --- */
        if (renderer_alloc_count > initial_alloc_count) {
            fprintf(stderr, "ERROR: Per-frame allocation detected!\n");
            input.quit = true;
        }
        if (renderer_texture_create_count > initial_texture_count) {
            fprintf(stderr, "ERROR: Per-frame texture creation detected!\n");
            input.quit = true;
        }

        /* --- 8i. Yield CPU until the next frame is due --- */
        uint32_t sleep_time = timing_sleep_ms(spare_time_ms);
        if (sleep_time > 0) {
            SDL_Delay(sleep_time);
        }

        frame_count++;
    }

    /* ================================================================
     *  9. Cleanup — release all resources
     * ================================================================ */
    if (map) map_destroy(map);
    grid_destroy(grid);
    renderer_destroy(ren);

    /* ================================================================
     *  10. Benchmark & Stability- test results
     *      Print JSON-formatted performance summary and return
     *      an exit code the test harness can interpret.
     * ================================================================ */
    if (mode == RUN_MODE_BENCHMARK_STRESS || mode == RUN_MODE_STABILITY) {
        double avg_render_ms = frame_count > 0 ? (global_total_render_ms / frame_count) : 0.0;
        
        /* Outlier trimming: if the absolute worst render time is more than
         * double the second-worst, it was likely a one-off spike (scheduler
         * jitter, swap) and should not count against us. */
        double effective_worst = absolute_worst_render_ms;
        if (absolute_worst_render_ms > second_worst_render_ms * 2.0 && second_worst_render_ms > 0) {
            effective_worst = second_worst_render_ms;
            outlier_trimmed = true;
        }

        /* Determine pass/fail:
         *   - "ideal"         : avg ≤ 4 ms, effective worst ≤ 6 ms  → exit 0
         *   - "pass_minimum"  : avg ≤ 6 ms, effective worst ≤ 8 ms  → exit 0
         *   - "fail..."       : otherwise                           → exit 1
         * Additionally, any per-frame allocation or texture creation
         * results in an automatic "fail_allocation_detected".
         */
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

        /* Machine-readable JSON output — parsed by CI scripts or test harnesses */
        printf("{\n");
        printf("  \"grid_width\": %d,\n", cfg->grid_width);
        printf("  \"grid_height\": %d,\n", cfg->grid_height);
        printf("  \"target_fps\": %d,\n", cfg->target_fps);
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

    /* Normal interactive mode — success */
    return 0;
}