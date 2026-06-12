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
#include "ui_asset.h"      /* UIButtonAsset, ui_asset_load/destroy, ui_button_render */
#include "menu_state.h"    /* MenuId, MenuStack, MENU_STACK_MAX, menu_stack_* functions */
#include "asset_designer.h"     /* AssetDesignerState, asset_designer_*, AD_RESULT_* */
#include "material_designer.h"  /* MaterialDesignerState, material_designer_*, MD_RESULT_* */
#include "live_editor.h"        /* LiveEditorState, live_editor_*, LE_RESULT_* */
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
             "Press ESC for menu\n"
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
 *  Menu rendering helpers
 * =================================================================== */

/* Layout constants for button slots (must match asset width=20, height=3) */
#define MENU_BUTTON_W   20
#define MENU_BUTTON_H    3
#define MENU_BUTTON_GAP  1

/* Total number of button assets (btns[0] unused, btns[1..8] loaded) */
#define BTN_COUNT        9

/**
 * MenuDef — Describes the contents and layout of one menu screen
 */
typedef struct {
    int         count;          /* Number of buttons */
    int         asset_ids[4];   /* Button asset IDs (indices into btns[]) */
    const char *labels[4];      /* Fallback text labels */
    const char *title;          /* Optional title line above buttons (NULL = none) */
} MenuDef;

/**
 * MENU_DEFS — Per-menu static definitions, indexed by MenuId
 *
 * Asset mapping:
 *   1=Start Game  2=Asset Editor  3=Quit (main)
 *   4=Resume      5=Main Menu     6=Quit (secondary)
 *   7=Yes         8=No
 */
static const MenuDef MENU_DEFS[] = {
    /* [MENU_NONE]         */ {0, {0,0,0,0}, {NULL,NULL,NULL,NULL},            NULL},
    /* [MENU_MAIN]         */ {3, {1,2,3,0}, {"START GAME","ASSET EDITOR","QUIT",NULL}, NULL},
    /* [MENU_PAUSE]        */ {3, {4,5,6,0}, {"RESUME","MAIN MENU","QUIT",NULL},        "-- PAUSED --"},
    /* [MENU_EDITOR]       */ {2, {4,5,0,0}, {"BACK TO EDITOR","MAIN MENU",NULL,NULL},  "-- EDITOR --"},
    /* [MENU_CONFIRM_QUIT]           */ {2, {7,8,0,0}, {"YES","NO",NULL,NULL},                      "QUIT? ARE YOU SURE?"},
    /* [MENU_DESIGNER_EXIT_CONFIRM]  */ {2, {7,8,0,0}, {"DISCARD CHANGES","CANCEL",NULL,NULL},      "UNSAVED CHANGES"},
    /* [MENU_ASSET_SELECT]           */ {4, {0,0,0,0}, {"LIVE EDIT","DECALS","MATERIALS","LIGHTS (COMING SOON)"}, "-- ASSET EDITOR --"},
};

/**
 * draw_fallback_button() — Render a plain text button when the asset is missing
 */
static void draw_fallback_button(Grid *grid, int x, int y,
                                 const char *label, bool selected) {
    char buf[64];
    if (selected) {
        snprintf(buf, sizeof(buf), "[ > %s < ]", label);
    } else {
        snprintf(buf, sizeof(buf), "[   %s   ]", label);
    }
    SDL_Color fg = selected ? (SDL_Color){ 50, 255,  50, 255}
                            : (SDL_Color){200, 200, 200, 255};
    SDL_Color bg = selected ? (SDL_Color){  0,  40,   0, 255}
                            : (SDL_Color){  0,   0,   0, 255};
    grid_print(grid, x, y, buf, fg, bg);
}

/**
 * draw_active_menu() — Render the currently active menu to the grid
 *
 * Looks up the MenuDef for `active`, optionally draws a title line, then
 * centres the button stack vertically and horizontally.
 *
 * @param grid    Target grid
 * @param active  Which menu to render (MENU_NONE is a no-op)
 * @param sel     Selected button index for this menu
 * @param btns    Button asset array, btns[0] unused, btns[1..BTN_COUNT-1] loaded
 */
static void draw_active_menu(Grid *grid, MenuId active, int sel,
                              UIButtonAsset **btns) {
    if (active == MENU_NONE || (int)active >= (int)(sizeof(MENU_DEFS)/sizeof(MENU_DEFS[0])))
        return;
    const MenuDef *def = &MENU_DEFS[(int)active];
    if (def->count == 0) return;

    /* Title row: 1 line of text + 1 blank gap */
    int title_rows = (def->title != NULL) ? 2 : 0;
    int total_h = title_rows
                  + def->count * MENU_BUTTON_H
                  + (def->count - 1) * MENU_BUTTON_GAP;
    int start_x = (grid->width  - MENU_BUTTON_W) / 2;
    int start_y = (grid->height - total_h)        / 2;

    if (def->title != NULL) {
        SDL_Color tfg = {255, 255, 255, 255};
        SDL_Color tbg = {  0,   0,   0, 255};
        int tlen = (int)strlen(def->title);
        int tx   = start_x + (MENU_BUTTON_W - tlen) / 2;
        if (tx < 0) tx = 0;
        grid_print(grid, tx, start_y, def->title, tfg, tbg);
    }

    int btn_y0 = start_y + title_rows;
    for (int i = 0; i < def->count; i++) {
        int btn_y    = btn_y0 + i * (MENU_BUTTON_H + MENU_BUTTON_GAP);
        bool selected = (i == sel);
        UIButtonState state = selected ? UI_BUTTON_SELECTED : UI_BUTTON_NORMAL;
        int aid = def->asset_ids[i];
        if (aid > 0 && aid < BTN_COUNT && btns[aid]) {
            ui_button_render(grid, start_x, btn_y, btns[aid], state);
        } else {
            draw_fallback_button(grid, start_x, btn_y, def->labels[i], selected);
        }
    }
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
     *  7. App state, menu stack, and UI button assets
     * ----------------------------------------------------------------- */
    /* Interactive runs start at the main menu; benchmarks skip it */
    AppState app_state = (mode == RUN_MODE_NORMAL) ? APP_STATE_MAIN_MENU
                                                   : APP_STATE_PLAYING;
    MenuStack ms;
    menu_stack_init(&ms);
    if (mode == RUN_MODE_NORMAL) {
        menu_stack_push(&ms, MENU_MAIN);
    }

    /* Per-menu selection state (one int per MenuId, indexed by MenuId value) */
    int menu_selected[MENU_ID_COUNT];
    for (int i = 0; i < MENU_ID_COUNT; i++) menu_selected[i] = 0;

    /* Load all button assets 1..8 (non-fatal; draw_active_menu falls back to text) */
    UIButtonAsset *btns[BTN_COUNT];
    for (int i = 0; i < BTN_COUNT; i++) btns[i] = NULL;
    if (mode == RUN_MODE_NORMAL) {
        for (int i = 1; i < BTN_COUNT; i++) {
            btns[i] = ui_asset_load(i, "assets");
        }
    }

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
            int count = (mid < (int)(sizeof(MENU_DEFS)/sizeof(MENU_DEFS[0])))
                        ? MENU_DEFS[mid].count : 0;

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
                if (active_menu == MENU_MAIN) {
                    if (sel == 0) {          /* Start Game */
                        menu_stack_clear(&ms);
                        app_state = APP_STATE_PLAYING;
                    } else if (sel == 1) {   /* Asset Editor → asset class selector */
                        menu_stack_push(&ms, MENU_ASSET_SELECT);
                    } else {                 /* Quit → confirm dialog */
                        menu_stack_push(&ms, MENU_CONFIRM_QUIT);
                    }
                } else if (active_menu == MENU_PAUSE) {
                    if (sel == 0) {          /* Resume → pop pause menu */
                        menu_stack_pop(&ms);
                    } else if (sel == 1) {   /* Main Menu */
                        menu_stack_clear(&ms);
                        app_state = APP_STATE_MAIN_MENU;
                        menu_stack_push(&ms, MENU_MAIN);
                    } else {                 /* Quit → confirm dialog */
                        menu_stack_push(&ms, MENU_CONFIRM_QUIT);
                    }
                } else if (active_menu == MENU_EDITOR) {
                    if (sel == 0) {          /* Back to Editor → pop */
                        menu_stack_pop(&ms);
                    } else {                 /* Main Menu */
                        menu_stack_clear(&ms);
                        app_state = APP_STATE_MAIN_MENU;
                        menu_stack_push(&ms, MENU_MAIN);
                    }
                } else if (active_menu == MENU_CONFIRM_QUIT) {
                    if (sel == 0) {          /* Yes → quit */
                        input.quit = true;
                    } else {                 /* No → pop confirm */
                        menu_stack_pop(&ms);
                    }
                } else if (active_menu == MENU_DESIGNER_EXIT_CONFIRM) {
                    if (sel == 0) {          /* Discard → destroy and exit designer */
                        if (app_state == APP_STATE_ASSET_DESIGNER) {
                            asset_designer_destroy(&ad_state);
                        } else if (app_state == APP_STATE_MATERIAL_DESIGNER) {
                            material_designer_destroy(&md_state);
                        } else if (app_state == APP_STATE_LIVE_EDITOR) {
                            live_editor_destroy(&le_state);
                        }
                        menu_stack_clear(&ms);
                        app_state = APP_STATE_MAIN_MENU;
                        menu_stack_push(&ms, MENU_MAIN);
                    } else {                 /* Cancel → back to designer */
                        menu_stack_pop(&ms);
                    }
                } else if (active_menu == MENU_ASSET_SELECT) {
                    if (sel == 0) {          /* Live Edit */
                        menu_stack_clear(&ms);
                        live_editor_init(&le_state, cfg, APP_STATE_MAIN_MENU, &assets);
                        app_state = APP_STATE_LIVE_EDITOR;
                    } else if (sel == 1) {   /* Decals */
                        menu_stack_clear(&ms);
                        asset_designer_init(&ad_state, cfg, APP_STATE_MAIN_MENU);
                        app_state = APP_STATE_ASSET_DESIGNER;
                    } else if (sel == 2) {   /* Materials */
                        menu_stack_clear(&ms);
                        material_designer_init(&md_state, cfg, APP_STATE_MAIN_MENU, &assets);
                        app_state = APP_STATE_MATERIAL_DESIGNER;
                    }
                    /* sel == 3: Lights — coming soon, no action */
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

        if (active_menu != MENU_NONE) {
            /* A menu is open — clear the screen and render the menu */
            SDL_Color mbg = {0, 0, 0, 255};
            grid_clear(grid, mbg);
            draw_active_menu(grid, active_menu,
                             menu_selected[(int)active_menu], btns);

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
                draw_ui_overlay(grid, frame_count, &perf_stats, visual_mode,
                                cfg->target_fps);
            }

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
     *  9. Cleanup — release all resources
     * ================================================================ */
    asset_designer_destroy(&ad_state);
    material_designer_destroy(&md_state);
    live_editor_destroy(&le_state);
    for (int i = 1; i < BTN_COUNT; i++) {
        ui_asset_destroy(btns[i]);
    }
    world_clear(&world);
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