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
#ifdef USE_LIGHTING_CACHE
#include "lighting_cache.h" /* Lighting shadow ray cache */
#endif
#ifdef USE_GLYPH_CACHE
#include "glyph_block_cache.h" /* Glyph block cache */
#endif
#include "ui_ele.h"        /* Data-driven UI elements/layouts */
#include "menu_state.h"    /* MenuId, MenuStack, MENU_STACK_MAX, menu_stack_* functions */
#include "asset_designer.h"     /* AssetDesignerState, asset_designer_*, AD_RESULT_* */
#include "material_designer.h"  /* MaterialDesignerState, material_designer_*, MD_RESULT_* */
#include "live_editor.h"        /* LiveEditorState, live_editor_*, LE_RESULT_* */
#include "smc_render_opt.h"     /* SMC runtime init/shutdown/stats for benchmark modes */
#if defined(USE_SMC_STATE_TRACKER) || defined(USE_SMC_INDEXED_STATE_TRACKER) || defined(USE_SMC_BATCH_STATE_TRACKER) || defined(USE_SMC_STREAM_STATE_TRACKER)
#include "smc_state_tracker.h"  /* SMC v2 dirty-state tracking */
#include "smc_indexed_state_tracker.h" /* SMC v2.1 indexed state tracking */
#include "smc.h"                /* SMC_OK, smc_state_stats_t, etc. */
#endif
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

    bg = stats->pub_min_spare_time_ms < 0 ? (SDL_Color){150, 0, 0, 255}
                                          : (SDL_Color){0, 0, 0, 255};
    ui_layout_render(layout, grid, fg, bg);
}

static const char *menu_layout_name(MenuId menu) {
    switch (menu) {
        case MENU_MAIN: return "main_menu";
        case MENU_PAUSE: return "pause_menu";
        case MENU_EDITOR: return "editor_menu";
        case MENU_CONFIRM_QUIT: return "confirm_quit";
        case MENU_DESIGNER_EXIT_CONFIRM: return "designer_exit_confirm";
        case MENU_ASSET_SELECT: return "asset_select";
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

static bool dispatch_menu_action(const char *action,
                                 MenuStack *ms,
                                 AppState *app_state,
                                 InputState *input,
                                 AssetDesignerState *ad_state,
                                 MaterialDesignerState *md_state,
                                 LiveEditorState *le_state,
                                 const EngineConfig *cfg,
                                 AssetRegistry *assets) {
    if (!action || !ms || !app_state) return false;
    if (strcmp(action, "start_game") == 0) {
        menu_stack_clear(ms);
        *app_state = APP_STATE_PLAYING;
        return true;
    }
    if (strcmp(action, "open_asset_editor") == 0) {
        menu_stack_push(ms, MENU_ASSET_SELECT);
        return true;
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
    if (strcmp(action, "back_to_editor") == 0) {
        menu_stack_pop(ms);
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
        if (*app_state == APP_STATE_ASSET_DESIGNER && ad_state) {
            asset_designer_destroy(ad_state);
        } else if (*app_state == APP_STATE_MATERIAL_DESIGNER && md_state) {
            material_designer_destroy(md_state);
        } else if (*app_state == APP_STATE_LIVE_EDITOR && le_state) {
            live_editor_destroy(le_state);
        }
        menu_stack_clear(ms);
        *app_state = APP_STATE_MAIN_MENU;
        menu_stack_push(ms, MENU_MAIN);
        return true;
    }
    if (strcmp(action, "open_live_edit") == 0) {
        menu_stack_clear(ms);
        if (le_state && cfg && assets) live_editor_init(le_state, cfg, APP_STATE_MAIN_MENU, assets);
        *app_state = APP_STATE_LIVE_EDITOR;
        return true;
    }
    if (strcmp(action, "open_decals") == 0) {
        menu_stack_clear(ms);
        if (ad_state && cfg) asset_designer_init(ad_state, cfg, APP_STATE_MAIN_MENU);
        *app_state = APP_STATE_ASSET_DESIGNER;
        return true;
    }
    if (strcmp(action, "open_materials") == 0) {
        menu_stack_clear(ms);
        if (md_state && cfg && assets) material_designer_init(md_state, cfg, APP_STATE_MAIN_MENU, assets);
        *app_state = APP_STATE_MATERIAL_DESIGNER;
        return true;
    }
    if (strcmp(action, "coming_soon") == 0) {
        return true;
    }
    return false;
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
        } else if (strcmp(argv[i], "--benchmark-raycast") == 0 && i + 1 < argc) {
            /* Runs raycast-mode benchmarking for a fixed duration, then prints JSON stats */
            mode = RUN_MODE_BENCHMARK_RAYCAST;
            visual_mode = VISUAL_RAYCAST;
            run_duration_seconds = atof(argv[++i]);
        } else if (strcmp(argv[i], "--stability-test") == 0 && i + 1 < argc) {
            /* Like benchmark, but intended to detect memory leaks / crashes over time */
            mode = RUN_MODE_STABILITY;
            visual_mode = VISUAL_STRESS;
            run_duration_seconds = atof(argv[++i]);
        } else if (strcmp(argv[i], "--benchmark-lighting") == 0 && i + 1 < argc) {
            /* Runs lighting-only benchmarking for a fixed duration, then prints JSON stats */
            mode = RUN_MODE_BENCHMARK_LIGHTING;
            visual_mode = VISUAL_RAYCAST;
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

    /* Mouse locking is deferred: it is enabled when the player starts the game
     * from the menu.  Non-interactive modes (benchmark/stability) never lock. */

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
     *  6a. Initialise the SMC renderer optimization layer (if enabled).
     *     This is a no-op when USE_SMC is not defined.  We do this after
     *     the renderer is created but before the benchmark loop starts.
     * ----------------------------------------------------------------- */
    if (smc_render_opt_init() != 0) {
        fprintf(stderr, "Failed to initialize SMC renderer optimization layer.\n");
        grid_destroy(grid);
        renderer_destroy(ren);
        return 1;
    }
    smc_render_opt_reset_stats();

#ifdef USE_LIGHTING_CACHE
    /* Initialise the lighting shadow ray cache */
    lighting_cache_init();
#endif

#ifdef USE_GLYPH_CACHE
    /* Initialise the glyph block cache */
    glyph_block_cache_init();
#endif

#ifdef USE_SMC_STATE_TRACKER
    /* Initialise the SMC state tracker */
    size_t max_cells = (size_t)cfg->grid_width * (size_t)cfg->grid_height;
    if (smc_state_tracker_init(max_cells) != SMC_OK) {
        fprintf(stderr, "Failed to initialize SMC state tracker.\n");
        grid_destroy(grid);
        renderer_destroy(ren);
        return 1;
    }
    smc_state_tracker_reset();
#endif

#ifdef USE_SMC_INDEXED_STATE_TRACKER
    /* Initialise the SMC indexed state tracker */
    size_t max_cells = (size_t)cfg->grid_width * (size_t)cfg->grid_height;
    if (smc_indexed_state_tracker_init(max_cells) != SMC_OK) {
        fprintf(stderr, "Failed to initialize SMC indexed state tracker.\n");
        grid_destroy(grid);
        renderer_destroy(ren);
        return 1;
    }
    smc_indexed_state_tracker_reset();
#endif

#ifdef USE_SMC_BATCH_STATE_TRACKER
    /* Initialise the SMC indexed state tracker (batch mode uses same backend) */
    size_t max_cells = (size_t)cfg->grid_width * (size_t)cfg->grid_height;
    if (smc_indexed_state_tracker_init(max_cells) != SMC_OK) {
        fprintf(stderr, "Failed to initialize SMC batch state tracker.\n");
        grid_destroy(grid);
        renderer_destroy(ren);
        return 1;
    }
    smc_indexed_state_tracker_reset();
#endif

#ifdef USE_SMC_STREAM_STATE_TRACKER
    /* Initialise the SMC indexed state tracker (stream mode uses same backend with state_size=7) */
    size_t max_cells = (size_t)cfg->grid_width * (size_t)cfg->grid_height;
    if (smc_indexed_state_tracker_init(max_cells) != SMC_OK) {
        fprintf(stderr, "Failed to initialize SMC stream state tracker.\n");
        grid_destroy(grid);
        renderer_destroy(ren);
        return 1;
    }
    smc_indexed_state_tracker_reset();
#endif

    /* -----------------------------------------------------------------
     *  7. App state, menu stack, and UI button assets
     * ----------------------------------------------------------------- */
    /* Interactive runs start at the main menu; benchmarks skip it */
    AppState app_state = (mode == RUN_MODE_NORMAL) ? APP_STATE_MAIN_MENU
                                                   : APP_STATE_PLAYING;

    /* For deterministic raycast benchmarking, fix the camera so every frame
     * renders the same view. This removes input/mouse noise from measurements. */
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

    /* Per-menu selection state (one int per MenuId, indexed by MenuId value) */
    int menu_selected[MENU_ID_COUNT];
    for (int i = 0; i < MENU_ID_COUNT; i++) menu_selected[i] = 0;

    /* Mouse lock state — recomputed each frame based on app state */
    bool mouse_locked = false;

    /* -----------------------------------------------------------------
     *  8. Main-loop state variables
     * ----------------------------------------------------------------- */
    uint64_t frame_count = 0;       /* Total frames rendered so far */
    PerfStats perf_stats;           /* Rolling one-second performance window */
    perf_stats_init(&perf_stats);
    InputState input = {0};         /* All fields zero-initialised (quit=false, etc.) */
    AssetDesignerState ad_state = {0};   /* Decal canvas editor; init'd on entry, destroy'd on exit */
    MaterialDesignerState md_state = {0}; /* Material editor; init'd on entry, destroy'd on exit */
    LiveEditorState le_state = {0};       /* Combined decal/material live-preview editor */
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

    /* High-resolution timer: used to enforce frame budget and detect duration expiry */
    uint64_t initial_time = SDL_GetPerformanceCounter();
    uint64_t last_time = initial_time;
    double target_time_ms = timing_target_ms(cfg->target_fps);  /* e.g. 16.67 ms for 60 FPS */

#if PROFILE_FRAME
    /* Per-frame phase profiling accumulators */
    frame_profile_init(&g_frame_profile);
#endif

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
        
        /* --- 8c. Mouse lock — derived from app state each frame --- */
        {
            bool want_lock = (mode == RUN_MODE_NORMAL)
                             && (app_state == APP_STATE_PLAYING)
                             && (ms.depth == 0);
            if (want_lock != mouse_locked) {
                SDL_SetWindowRelativeMouseMode(ren->window, want_lock);
                mouse_locked = want_lock;
            }
        }

        /* --- 8d. ESC routing (context-aware) --- */
        if (input.esc) {
            if (app_state == APP_STATE_MAIN_MENU) {
                /* Main menu is the root: ESC is ignored */
            } else if (ms.depth > 0) {
                /* Pop the top overlay menu */
                menu_stack_pop(&ms);
            } else if (app_state == APP_STATE_PLAYING) {
                /* Enter pause menu */
                menu_stack_push(&ms, MENU_PAUSE);
            } else if (app_state == APP_STATE_EDITOR) {
                /* Enter editor menu */
                menu_stack_push(&ms, MENU_EDITOR);
            }
        }

        /* --- 8e. Menu navigation and confirm --- */
        MenuId active_menu = menu_stack_peek(&ms);
        if (active_menu != MENU_NONE) {
            int mid = (int)active_menu;
            UiLayout *active_layout = (mid >= 0 && mid < MENU_ID_COUNT) ? menu_layouts[mid] : NULL;
            int count = active_layout ? ui_layout_focusable_count(active_layout) : 0;

            /* Up/down navigation — wrap-around */
            if (count > 0) {
                if (input.up)
                    menu_selected[mid] = (menu_selected[mid] - 1 + count) % count;
                if (input.down)
                    menu_selected[mid] = (menu_selected[mid] + 1) % count;
            }

            /* Confirm: dispatch based on menu type and current selection */
            if (input.confirm && count > 0) {
                int sel = menu_selected[mid];
                if (active_layout) {
                    UiElement *focused = ui_layout_get_focused(active_layout, sel);
                    if (focused && focused->action[0] != '\0') {
                        dispatch_menu_action(focused->action, &ms, &app_state, &input,
                                             &ad_state, &md_state, &le_state, cfg, &assets);
                    }
                }
                /* Refresh active_menu after potential state change */
                active_menu = menu_stack_peek(&ms);
            }
        }

        /* --- 8e.5 Asset designer update (when no menu is overlaying it) --- */
        if (app_state == APP_STATE_ASSET_DESIGNER && menu_stack_peek(&ms) == MENU_NONE) {
            AssetDesignerResult ad_result = asset_designer_update(&ad_state, &input, &assets);
            if (ad_result == AD_RESULT_EXIT) {
                asset_designer_destroy(&ad_state);
                menu_stack_clear(&ms);
                app_state = APP_STATE_MAIN_MENU;
                menu_stack_push(&ms, MENU_MAIN);
            } else if (ad_result == AD_RESULT_CONFIRM_DISCARD) {
                menu_stack_push(&ms, MENU_DESIGNER_EXIT_CONFIRM);
            }
        }
        if (app_state == APP_STATE_MATERIAL_DESIGNER && menu_stack_peek(&ms) == MENU_NONE) {
            MaterialDesignerResult md_result = material_designer_update(&md_state, &input, &assets);
            if (md_result == MD_RESULT_EXIT) {
                material_designer_destroy(&md_state);
                menu_stack_clear(&ms);
                app_state = APP_STATE_MAIN_MENU;
                menu_stack_push(&ms, MENU_MAIN);
            } else if (md_result == MD_RESULT_CONFIRM_DISCARD) {
                menu_stack_push(&ms, MENU_DESIGNER_EXIT_CONFIRM);
            }
        }
        if (app_state == APP_STATE_LIVE_EDITOR && menu_stack_peek(&ms) == MENU_NONE) {
            LiveEditorResult le_result = live_editor_update(&le_state, &input, &assets);
            if (le_result == LE_RESULT_EXIT) {
                live_editor_destroy(&le_state);
                menu_stack_clear(&ms);
                app_state = APP_STATE_MAIN_MENU;
                menu_stack_push(&ms, MENU_MAIN);
            } else if (le_result == LE_RESULT_CONFIRM_DISCARD) {
                menu_stack_push(&ms, MENU_DESIGNER_EXIT_CONFIRM);
            }
        }

    /* --- 8f. Draw frame contents --- */
    double delta_time_sec = delta_time_ms / 1000.0;
    active_menu = menu_stack_peek(&ms);

#if PROFILE_FRAME
    double profile_grid_start = profile_now_ms();
#endif

    if (active_menu != MENU_NONE) {
            /* A menu is open — clear the screen and render the menu */
            SDL_Color mbg = {0, 0, 0, 255};
            UiLayout *active_layout = ((int)active_menu >= 0 && (int)active_menu < MENU_ID_COUNT)
                                      ? menu_layouts[(int)active_menu] : NULL;
            grid_clear(grid, mbg);
            draw_data_menu(grid, active_layout, menu_selected[(int)active_menu]);

        } else if (app_state == APP_STATE_PLAYING) {
            /* Game world */
            if (visual_mode == VISUAL_RAYCAST) {
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
            SDL_Color ae_bg = {0, 0, 0, 255};
            SDL_Color ae_fg = {255, 255, 255, 255};
            grid_clear(grid, ae_bg);
            grid_print(grid, 2, 2,
                       "ASSET EDITOR (placeholder)\nPress ESC for editor menu",
                       ae_fg, ae_bg);

        } else if (app_state == APP_STATE_ASSET_DESIGNER) {
            asset_designer_render(&ad_state, grid, &assets);

        } else if (app_state == APP_STATE_MATERIAL_DESIGNER) {
            material_designer_render(&md_state, grid, &assets);

        } else if (app_state == APP_STATE_LIVE_EDITOR) {
            live_editor_render(&le_state, grid);

        } else {
            /* APP_STATE_MAIN_MENU with empty stack — should not happen, clear only */
            SDL_Color bg = {0, 0, 0, 255};
            grid_clear(grid, bg);
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
     *  9. Capture benchmark data before cleanup
     * ================================================================ */
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
    uint32_t framebuffer_checksum = 0;   /* Checksum captured before cleanup */

    if (mode == RUN_MODE_BENCHMARK_STRESS || mode == RUN_MODE_BENCHMARK_RAYCAST || mode == RUN_MODE_STABILITY) {
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

    /* ================================================================
     *  10. Cleanup — release all resources
     * ================================================================ */
#ifdef USE_SMC_STATE_TRACKER
    smc_state_tracker_shutdown();
#endif
#if defined(USE_SMC_INDEXED_STATE_TRACKER) || defined(USE_SMC_BATCH_STATE_TRACKER) || defined(USE_SMC_STREAM_STATE_TRACKER)
    smc_indexed_state_tracker_shutdown();
#endif
    asset_designer_destroy(&ad_state);
    material_designer_destroy(&md_state);
    live_editor_destroy(&le_state);
    for (int i = 0; i < MENU_ID_COUNT; i++) {
        ui_layout_destroy(menu_layouts[i]);
    }
    ui_layout_destroy(hud_layout);
    ui_cache_destroy(&menu_cache);
    world_clear(&world);
    if (map) map_destroy(map);
    grid_destroy(grid);
    renderer_destroy(ren);

    /* ================================================================
     *  11. Benchmark & Stability- test results
     *      Print JSON-formatted performance summary and return
     *      an exit code the test harness can interpret.
     * ================================================================ */
    if (mode == RUN_MODE_BENCHMARK_STRESS || mode == RUN_MODE_BENCHMARK_RAYCAST || mode == RUN_MODE_STABILITY) {
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

        /* Report glyph cache instrumentation when the optimization layer is active. */
#ifdef USE_GLYPH_CACHE
        uint64_t glyphs_total = renderer_cache_hits + renderer_cache_misses;
        double glyph_hit_rate = glyphs_total > 0 ? (100.0 * renderer_cache_hits / glyphs_total) : 0.0;
        if (glyphs_total > 0) {
            fprintf(stderr, "Glyph cache stats: hits=%llu misses=%llu hit_rate=%.1f%% raster_ms=%.2f upload_ms=%.2f cells=%llu\n",
                    (unsigned long long)renderer_cache_hits,
                    (unsigned long long)renderer_cache_misses,
                    glyph_hit_rate,
                    renderer_time_raster_ms,
                    renderer_time_upload_ms,
                    (unsigned long long)renderer_cells_processed);
        }
#endif

        /* Report SMC instrumentation when the optimization layer is active. */
        uint64_t smc_total = 0, smc_fallback = 0, smc_arity = 0, smc_invalid = 0;
        smc_render_opt_get_stats(&smc_total, &smc_fallback, &smc_arity, &smc_invalid);
        if (smc_total > 0) {
            fprintf(stderr, "SMC stats: total_calls=%llu fallback=%llu arity_errors=%llu invalid_ids=%llu\n",
                    (unsigned long long)smc_total,
                    (unsigned long long)smc_fallback,
                    (unsigned long long)smc_arity,
                    (unsigned long long)smc_invalid);
        }

#ifdef USE_SMC_STATE_TRACKER
        /* Report SMC state tracker instrumentation. */
        fprintf(stderr, "SMC state stats: checks=%llu changed=%llu unchanged=%llu evictions=%llu bytes_compared=%llu\n",
                (unsigned long long)smc_state_s.checks,
                (unsigned long long)smc_state_s.changed,
                (unsigned long long)smc_state_s.unchanged,
                (unsigned long long)smc_state_s.evictions,
                (unsigned long long)smc_state_s.bytes_compared);
#endif
#ifdef USE_SMC_INDEXED_STATE_TRACKER
        fprintf(stderr, "SMC indexed stats: checks=%llu changed=%llu unchanged=%llu stores=%llu bytes_compared=%llu out_of_range=%llu clears=%llu\n",
                (unsigned long long)smc_indexed_s.checks,
                (unsigned long long)smc_indexed_s.changed,
                (unsigned long long)smc_indexed_s.unchanged,
                (unsigned long long)smc_indexed_s.stores,
                (unsigned long long)smc_indexed_s.bytes_compared,
                (unsigned long long)smc_indexed_s.out_of_range,
                (unsigned long long)smc_indexed_s.clears);
#endif
#ifdef USE_SMC_BATCH_STATE_TRACKER
        fprintf(stderr, "SMC batch stats: checks=%llu changed=%llu unchanged=%llu stores=%llu bytes_compared=%llu out_of_range=%llu clears=%llu\n",
                (unsigned long long)smc_batch_s.checks,
                (unsigned long long)smc_batch_s.changed,
                (unsigned long long)smc_batch_s.unchanged,
                (unsigned long long)smc_batch_s.stores,
                (unsigned long long)smc_batch_s.bytes_compared,
                (unsigned long long)smc_batch_s.out_of_range,
                (unsigned long long)smc_batch_s.clears);
#endif
#ifdef USE_SMC_STREAM_STATE_TRACKER
        fprintf(stderr, "SMC stream stats: checks=%llu changed=%llu unchanged=%llu stores=%llu bytes_compared=%llu out_of_range=%llu clears=%llu fallback_count=%llu\n",
                (unsigned long long)smc_stream_s.checks,
                (unsigned long long)smc_stream_s.changed,
                (unsigned long long)smc_stream_s.unchanged,
                (unsigned long long)smc_stream_s.stores,
                (unsigned long long)smc_stream_s.bytes_compared,
                (unsigned long long)smc_stream_s.out_of_range,
                (unsigned long long)smc_stream_s.clears,
                (unsigned long long)renderer_smc_fallback_count);
#endif

        /* Report renderer dirty tracking stats and framebuffer checksum. */
        uint64_t cells_total = (uint64_t)cfg->grid_width * (uint64_t)cfg->grid_height;
        
        fprintf(stderr, "Renderer stats: cells_total=%llu cells_rasterized=%llu cells_skipped=%llu skip_rate=%.1f%% framebuffer_checksum=%u\n",
                (unsigned long long)cells_total,
                (unsigned long long)renderer_cells_processed,
                (unsigned long long)renderer_cells_skipped,
                cells_total > 0 ? (100.0 * renderer_cells_skipped / cells_total) : 0.0,
                framebuffer_checksum);

#if PROFILE_FRAME
        /* Print per-frame phase profile summary for benchmark modes. */
        const char *profile_mode_name = "baseline";
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
        frame_profile_print(&g_frame_profile, profile_mode_name);
#endif

        return exit_code;
    }

    /* -----------------------------------------------------------------
     *  11. Lighting benchmark results (separate from other benchmarks)
     * ----------------------------------------------------------------- */
    if (mode == RUN_MODE_BENCHMARK_LIGHTING) {
        double avg_lighting_ms = frame_count > 0 ? (lighting_total_time_ms / frame_count) : 0.0;
        double avg_render_ms = frame_count > 0 ? (global_total_render_ms / frame_count) : 0.0;
        uint64_t total_shadow_rays = lighting_shadow_ray_count;
        
#ifdef USE_LIGHTING_CACHE
        uint64_t cache_hits = 0, cache_misses = 0, cache_evictions = 0;
        lighting_cache_get_stats(&cache_hits, &cache_misses, &cache_evictions);
        double hit_rate = (cache_hits + cache_misses) > 0 ? 
                          (100.0 * cache_hits / (cache_hits + cache_misses)) : 0.0;
#else
        uint64_t cache_hits = 0, cache_misses = 0, cache_evictions = 0;
        double hit_rate = 0.0;
#endif

        printf("{\n");
        printf("  \"avg_lighting_ms\": %.2f,\n", avg_lighting_ms);
        printf("  \"avg_render_ms\": %.2f,\n", avg_render_ms);
        printf("  \"total_shadow_rays\": %llu,\n", (unsigned long long)total_shadow_rays);
        printf("  \"cache_hits\": %llu,\n", (unsigned long long)cache_hits);
        printf("  \"cache_misses\": %llu,\n", (unsigned long long)cache_misses);
        printf("  \"cache_evictions\": %llu,\n", (unsigned long long)cache_evictions);
        printf("  \"cache_hit_rate\": %.1f,\n", hit_rate);
        printf("  \"frames\": %llu,\n", (unsigned long long)frame_count);
        printf("  \"result\": \"done\"\n");
        printf("}\n");

        return 0;
    }

    /* Normal interactive mode — success */
    return 0;
}
