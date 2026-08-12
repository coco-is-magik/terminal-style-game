/**
 * app.c — Application Layer (Main Game Loop & Rendering Patterns)
 */
#include "app.h"
#include "app_options.h"
#include "app_resources.h"
#include "benchmark_session.h"
#include "frame_dispatch.h"
#include "menu_controller.h"
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
#include "ui_canvas.h"
#include "ui_compositor.h"
#include "ui_preferences.h"
#include "menu_state.h"
#include "unified_editor.h"
#include "editor_highlight.h"
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

#define APP_UI_MENU_WIDTH 80
#define APP_UI_MENU_HEIGHT 40
#define APP_UI_HUD_WIDTH 64
#define APP_UI_HUD_HEIGHT 20
#define APP_UI_EDITOR_WIDTH 100
#define APP_UI_EDITOR_HEIGHT 40
#define APP_UI_FOOTER_WIDTH 160
#define APP_UI_FEEDBACK_WIDTH 80
#define APP_UI_FEEDBACK_FRAMES 180

enum {
    APP_UI_ROLE_MENU = 1,
    APP_UI_ROLE_HUD,
    APP_UI_ROLE_EDITOR,
    APP_UI_ROLE_FOOTER,
    APP_UI_ROLE_FEEDBACK,
    APP_UI_ROLE_CROSSHAIR
};

typedef struct {
    char text[96];
    int frames_remaining;
} UiScaleFeedback;

typedef struct {
    Grid *staging;
    UiCanvas *menu;
    UiCanvas *hud;
    UiCanvas *editor;
    UiCanvas *footer;
    UiCanvas *feedback;
    UiCanvas *crosshair;
} AppUiResources;

static void app_ui_resources_destroy(AppUiResources *ui) {
    if (!ui) return;
    ui_canvas_destroy(ui->crosshair);
    ui_canvas_destroy(ui->feedback);
    ui_canvas_destroy(ui->footer);
    ui_canvas_destroy(ui->editor);
    ui_canvas_destroy(ui->hud);
    ui_canvas_destroy(ui->menu);
    grid_destroy(ui->staging);
    memset(ui, 0, sizeof(*ui));
}

static bool app_ui_resources_create(AppUiResources *ui, int grid_width,
                                    int grid_height) {
    if (!ui || grid_width < APP_UI_FOOTER_WIDTH ||
        grid_height < APP_UI_EDITOR_HEIGHT + 2) {
        return false;
    }
    memset(ui, 0, sizeof(*ui));
    ui->staging = grid_create(grid_width, grid_height);
    ui->menu = ui_canvas_create(APP_UI_MENU_WIDTH, APP_UI_MENU_HEIGHT);
    ui->hud = ui_canvas_create(APP_UI_HUD_WIDTH, APP_UI_HUD_HEIGHT);
    ui->editor = ui_canvas_create(APP_UI_EDITOR_WIDTH, APP_UI_EDITOR_HEIGHT);
    ui->footer = ui_canvas_create(APP_UI_FOOTER_WIDTH, 2);
    ui->feedback = ui_canvas_create(APP_UI_FEEDBACK_WIDTH, 1);
    ui->crosshair = ui_canvas_create(1, 1);
    if (!ui->staging || !ui->menu || !ui->hud || !ui->editor || !ui->footer ||
        !ui->feedback || !ui->crosshair) {
        app_ui_resources_destroy(ui);
        return false;
    }
    return true;
}

static void ui_scale_feedback_set(UiScaleFeedback *feedback,
                                  const UiPreferences *preferences,
                                  UiPreferencesChangeResult result) {
    const char *suffix = result == UI_PREFERENCES_CHANGE_ACTIVE_NOT_SAVED
        ? " active; preference not saved" : "";
    if (!feedback || !preferences) return;
    snprintf(feedback->text, sizeof(feedback->text), "UI Scale: %d%%%s",
             ui_preferences_scale(preferences), suffix);
    feedback->frames_remaining = APP_UI_FEEDBACK_FRAMES;
}

static bool app_add_ui_layer(UiLayerList *layers, int role, const UiCanvas *canvas,
                             UiAnchor anchor, UiScalePolicy policy, int z_order,
                             int pixel_width, int pixel_height) {
    UiLayer layer = {
        role, canvas, anchor, {0, 0, pixel_width, pixel_height}, policy, 100,
        z_order, true, 0
    };
    return ui_layer_list_add(layers, &layer);
}

static bool benchmark_is_layered_ui(const char *scenario) {
    return scenario && strcmp(scenario, "ui-layered") == 0;
}

static void prepare_layered_ui_benchmark(AppUiResources *ui, UiLayerList *layers,
                                         int pixel_width, int pixel_height,
                                         uint64_t frame_count) {
    SDL_Color fg = {255, 255, 255, 255};
    SDL_Color bg = {(uint8_t)(32U + (frame_count & 31U)), 16, 48, 255};
    int y;
    int x;
    /* Hide the layer regularly so the next draw must restore its prior coverage.
     * This deterministically exercises appear, steady overlap, and disappear. */
    if (frame_count % 16U == 15U) return;
    (void)grid_clear_region_zero(ui->staging, 0, 0,
                                 APP_UI_HUD_WIDTH, APP_UI_HUD_HEIGHT);
    for (y = 0; y < APP_UI_HUD_HEIGHT; y++) {
        for (x = 0; x < APP_UI_HUD_WIDTH; x++) {
            uint8_t glyph = (uint8_t)('!' + ((x + y + (int)frame_count) % 90));
            (void)grid_set(ui->staging, x, y, glyph, fg, bg);
        }
    }
    ui_canvas_copy_grid_region(ui->hud, ui->staging, 0, 0);
    {
        UiLayer layer = {
            APP_UI_ROLE_HUD, ui->hud, UI_ANCHOR_TOP_LEFT,
            {0, 0, pixel_width, pixel_height}, UI_SCALE_EXPLICIT_PRESET,
            150, 10, true, 0
        };
        (void)ui_layer_list_add(layers, &layer);
    }
}

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

static bool reserve_dynamic_ui_text(UiCache *cache) {
    static const char *const names[] = {
        "hud_grid", "hud_frame", "hud_mode", "hud_target_fps",
        "hud_actual_fps", "hud_avg_frame", "hud_worst_frame",
        "hud_min_spare", "hud_status", "settings_ui_scale_value"
    };
    size_t i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        UiElement *element = ui_cache_get(cache, names[i]);
        if (!element || !ui_ele_reserve_content(element, 128U)) return false;
    }
    return true;
}

static bool set_ui_text_bounded(UiCache *cache, const char *name, const char *text) {
    UiElement *element;
    if (!cache || !name || !text) return false;
    element = ui_cache_get(cache, name);
    return element && ui_ele_set_content_bounded(element, text);
}

static void update_settings_scale_text(UiCache *cache,
                                       const UiPreferences *preferences) {
    char text[48];
    if (!cache || !preferences) return;
    snprintf(text, sizeof(text), "UI Scale: %d%%", ui_preferences_scale(preferences));
    set_ui_text(cache, "settings_ui_scale_value", text);
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
    (void)set_ui_text_bounded(cache, "hud_grid", line);
    snprintf(line, sizeof(line), "Frame: %llu", (unsigned long long)frame_count);
    (void)set_ui_text_bounded(cache, "hud_frame", line);
    snprintf(line, sizeof(line), "Mode: %s", mode_str);
    (void)set_ui_text_bounded(cache, "hud_mode", line);
    snprintf(line, sizeof(line), "Target FPS: %d", target_fps);
    (void)set_ui_text_bounded(cache, "hud_target_fps", line);
    snprintf(line, sizeof(line), "Actual FPS: %.1f", stats->pub_avg_fps);
    (void)set_ui_text_bounded(cache, "hud_actual_fps", line);
    snprintf(line, sizeof(line), "Avg Frame Time: %.2f ms", stats->pub_avg_frame_time_ms);
    (void)set_ui_text_bounded(cache, "hud_avg_frame", line);
    snprintf(line, sizeof(line), "Worst Frame Time: %.2f ms", stats->pub_worst_frame_time_ms);
    (void)set_ui_text_bounded(cache, "hud_worst_frame", line);
    snprintf(line, sizeof(line), "Min Spare Time: %.2f ms", stats->pub_min_spare_time_ms);
    (void)set_ui_text_bounded(cache, "hud_min_spare", line);
    snprintf(line, sizeof(line), "Status: %s", stats->pub_min_spare_time_ms < 0 ? "OVER BUDGET" : "OK");
    (void)set_ui_text_bounded(cache, "hud_status", line);
    bg = stats->pub_min_spare_time_ms < 0 ? (SDL_Color){150, 0, 0, 255} : (SDL_Color){0, 0, 0, 255};
    ui_layout_render(layout, grid, fg, bg);
}

static const char *menu_layout_name(MenuId menu) {
    switch (menu) {
        case MENU_MAIN: return "main_menu";
        case MENU_PAUSE: return "pause_menu";
        case MENU_SETTINGS: return "settings";
        case MENU_CONFIRM_QUIT: return "confirm_quit";
        case MENU_NONE:
        case MENU_ID_COUNT:
        default: return NULL;
    }
}

static void menu_sync_button_colors(UiLayout *layout, int selected) {
    if (!layout) return;
    ui_layout_set_focus(layout, selected);
    int count = ui_layout_focusable_count(layout);
    for (int i = 0; i < count; i++) {
        UiElement *button = ui_layout_get_focused(layout, i);
        if (!button) continue;
        if (i == selected) {
            ui_ele_set_colors(button, (SDL_Color){50, 255, 50, 255},
                              (SDL_Color){0, 40, 0, 255});
        } else {
            ui_ele_set_colors(button, (SDL_Color){200, 200, 200, 255},
                              (SDL_Color){0, 0, 0, 255});
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
    if (!unified_editor_set_material_root(ued, "assets/materials")) {
        unified_editor_destroy(ued);
        fprintf(stderr, "material editor root allocation failed\n");
        return false;
    }
    if (!unified_editor_set_asset_root(ued, "assets")) {
        unified_editor_destroy(ued);
        fprintf(stderr, "asset editor root allocation failed\n");
        return false;
    }
    (void)unified_editor_begin_legacy_import(ued, "assets/maps");
    (void)unified_editor_begin_native_open(ued, "assets/scenes");
    camera_init(cam, 1.5, 1.5, PI / 4.0, PI / 2.0);
    menu_stack_clear(ms);
    *app_state = APP_STATE_EDITOR;
    return true;
}

static void sync_editor_text_input(Renderer *renderer,
                                   AppState app_state,
                                   const UnifiedEditorState *editor) {
    bool should_be_active;

    if (!renderer || !renderer->window) return;
    should_be_active = app_state == APP_STATE_EDITOR && editor && editor->active &&
        ((editor->modal == EDITOR_MENU_SAVE &&
          editor->save_menu_stage == EDITOR_SAVE_MENU_EDIT_NAME) ||
          (editor->modal == EDITOR_MODAL_NONE && editor->inspector_open &&
           (editor->inspector_kind == EDITOR_INSPECTOR_LIGHT ||
            editor->material_picker_open)));
    if (should_be_active && !SDL_TextInputActive(renderer->window)) {
        if (!SDL_StartTextInput(renderer->window)) {
            fprintf(stderr, "Failed to start editor text input: %s\n", SDL_GetError());
        }
    } else if (!should_be_active && SDL_TextInputActive(renderer->window)) {
        SDL_StopTextInput(renderer->window);
    }
}

static bool dispatch_menu_action(const char *action,
                                 MenuStack *ms,
                                 AppState *app_state,
                                 InputState *input,
                                 UnifiedEditorState *ued,
                                 Camera *cam,
                                 AssetRegistry *assets,
                                 UiPreferences *preferences,
                                 UiScaleFeedback *feedback,
                                 UiCache *ui_cache) {
    if (!action || !ms || !app_state) return false;
    switch (menu_controller_parse_action(action)) {
    case MENU_ACTION_START_GAME:
        menu_stack_clear(ms);
        *app_state = APP_STATE_PLAYING;
        return true;
    case MENU_ACTION_OPEN_EDITOR:
        return enter_unified_editor(ued, app_state, ms, assets, cam);
    case MENU_ACTION_QUIT:
        menu_stack_push(ms, MENU_CONFIRM_QUIT);
        return true;
    case MENU_ACTION_RESUME:
        menu_stack_pop(ms);
        return true;
    case MENU_ACTION_MAIN_MENU:
        menu_stack_clear(ms);
        *app_state = APP_STATE_MAIN_MENU;
        menu_stack_push(ms, MENU_MAIN);
        return true;
    case MENU_ACTION_CONFIRM_QUIT:
        if (input) input->quit = true;
        return true;
    case MENU_ACTION_CANCEL:
        menu_stack_pop(ms);
        return true;
    case MENU_ACTION_DISCARD_CHANGES:
        if (*app_state == APP_STATE_EDITOR && ued) {
            unified_editor_destroy(ued);
        }
        menu_stack_clear(ms);
        *app_state = APP_STATE_MAIN_MENU;
        menu_stack_push(ms, MENU_MAIN);
        return true;
    case MENU_ACTION_OPEN_SETTINGS:
        return menu_stack_push(ms, MENU_SETTINGS);
    case MENU_ACTION_UI_SCALE_DECREASE:
        ui_scale_feedback_set(feedback, preferences,
                              ui_preferences_decrease(preferences));
        update_settings_scale_text(ui_cache, preferences);
        return true;
    case MENU_ACTION_UI_SCALE_INCREASE:
        ui_scale_feedback_set(feedback, preferences,
                              ui_preferences_increase(preferences));
        update_settings_scale_text(ui_cache, preferences);
        return true;
    case MENU_ACTION_UI_SCALE_RESET:
        ui_scale_feedback_set(feedback, preferences,
                              ui_preferences_reset(preferences));
        update_settings_scale_text(ui_cache, preferences);
        return true;
    case MENU_ACTION_BACK:
        return menu_stack_pop(ms);
    case MENU_ACTION_UNKNOWN:
    default:
        return false;
    }
}

static int run_headless_smoke(void) {
    AssetRegistry assets;
    WorldState world;
    Map *map;

    if (!asset_registry_init(&assets)) {
        fprintf(stderr, "{\"smoke\":\"fail\",\"stage\":\"asset_registry\"}\n");
        return 1;
    }
    world_init(&world);
    if (!asset_loader_load_registry(&assets, "assets")) {
        fprintf(stderr, "{\"smoke\":\"fail\",\"stage\":\"asset_load\"}\n");
        world_clear(&world);
        asset_registry_clear(&assets);
        return 1;
    }

    map = asset_loader_load_map_data(&world, "assets", 1);
    if (!map) {
        fprintf(stderr, "{\"smoke\":\"fail\",\"stage\":\"map\"}\n");
        world_clear(&world);
        asset_registry_clear(&assets);
        return 1;
    }

    printf("{\"smoke\":\"ok\",\"map_width\":%d,\"map_height\":%d}\n",
           map->width, map->height);
    map_destroy(map);
    world_clear(&world);
    asset_registry_clear(&assets);
    return 0;
}

int app_main(int argc, char* argv[]) {
    AppResources resources;
    app_resources_init(&resources);
    AppOptions options;
    AppOptionsResult options_result = app_options_parse(argc, argv, &options);
    if (options_result != APP_OPTIONS_OK) {
        fprintf(stderr, "Invalid command line: %s\n", app_options_result_string(options_result));
        return 2;
    }

    config_init_defaults();
    config_load_from_file("config.ini");
    const EngineConfig *cfg = config_get();

    if (options.mode == RUN_MODE_SMOKE) {
        return run_headless_smoke();
    }

    RunMode mode = options.mode;
    VisualMode visual_mode = options.visual_mode;
    double run_duration_seconds = options.run_duration_seconds;
    const char *benchmark_scenario = options.benchmark_scenario;
    int benchmark_frames = options.benchmark_frames;
    bool layered_ui_benchmark = benchmark_is_layered_ui(benchmark_scenario);
    uint64_t benchmark_frame_limit = mode == RUN_MODE_BENCHMARK_SCENARIO
        ? benchmark_session_total_scenario_frames(benchmark_frames) : 0;

    Renderer *ren = renderer_create(cfg->window_width, cfg->window_height,
                                     cfg->grid_width, cfg->grid_height,
                                     cfg->cell_width, cfg->cell_height);
    if (!ren) {
        fprintf(stderr, "Failed to initialize renderer. (Headless environment expected)\n");
        return 0;
    }
    resources.renderer = ren;

    Grid *grid = grid_create(cfg->grid_width, cfg->grid_height);
    if (!grid) {
        fprintf(stderr, "Failed to initialize grid.\n");
        app_resources_cleanup(&resources);
        return 1;
    }
    resources.grid = grid;

    AssetRegistry assets;
    if (!asset_registry_init(&assets)) {
        fprintf(stderr, "Failed to initialize asset registry.\n");
        app_resources_cleanup(&resources);
        return 1;
    }
    resources.assets = &assets;
    resources.assets_initialized = true;
    if (!asset_loader_load_registry(&assets, "assets")) {
        fprintf(stderr, "Failed to load asset registry.\n");
        app_resources_cleanup(&resources);
        return 1;
    }

    WorldState world;
    world_init(&world);
    resources.world = &world;
    resources.world_initialized = true;

    Map *map = asset_loader_load_map_data(&world, "assets", 1);
    resources.map = map;
    if (!map) {
        fprintf(stderr, "Failed to load map.\n");
        app_resources_cleanup(&resources);
        return 1;
    }

    Camera cam;
    camera_init(&cam, 1.5, 1.5, PI / 4.0, PI / 2.0);

    if (smc_render_opt_init() != 0) {
        fprintf(stderr, "Failed to initialize SMC renderer optimization layer.\n");
        app_resources_cleanup(&resources);
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
        app_resources_cleanup(&resources); return 1;
      }
      resources.state_tracker_initialized = true;
      smc_state_tracker_reset(); }
#endif
#ifdef USE_SMC_INDEXED_STATE_TRACKER
    { size_t max_cells = (size_t)cfg->grid_width * (size_t)cfg->grid_height;
      if (smc_indexed_state_tracker_init(max_cells) != SMC_OK) {
        fprintf(stderr, "Failed to initialize SMC indexed state tracker.\n");
        app_resources_cleanup(&resources); return 1;
      }
      resources.indexed_tracker_initialized = true;
      smc_indexed_state_tracker_reset(); }
#endif
#ifdef USE_SMC_BATCH_STATE_TRACKER
    { size_t max_cells = (size_t)cfg->grid_width * (size_t)cfg->grid_height;
      if (smc_indexed_state_tracker_init(max_cells) != SMC_OK) {
        fprintf(stderr, "Failed to initialize SMC batch state tracker.\n");
        app_resources_cleanup(&resources); return 1;
      }
      resources.indexed_tracker_initialized = true;
      smc_indexed_state_tracker_reset(); }
#endif
#ifdef USE_SMC_STREAM_STATE_TRACKER
    { size_t max_cells = (size_t)cfg->grid_width * (size_t)cfg->grid_height;
      if (smc_indexed_state_tracker_init(max_cells) != SMC_OK) {
        fprintf(stderr, "Failed to initialize SMC stream state tracker.\n");
        app_resources_cleanup(&resources); return 1;
      }
      resources.indexed_tracker_initialized = true;
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
    if (!reserve_dynamic_ui_text(&menu_cache)) {
        fprintf(stderr, "Failed to reserve bounded dynamic UI text.\n");
        for (int i = 0; i < MENU_ID_COUNT; i++) ui_layout_destroy(menu_layouts[i]);
        ui_layout_destroy(hud_layout);
        ui_cache_destroy(&menu_cache);
        app_resources_cleanup(&resources);
        return 1;
    }
    UiPreferences preferences;
    UiScaleFeedback scale_feedback = {{0}, 0};
    AppUiResources ui_resources;
    ui_preferences_init(&preferences, "default_user.ini", "user.ini");
    if (preferences.default_load_result != UI_PREFERENCES_IO_OK) {
        fprintf(stderr, "UI preferences: default_user.ini invalid or missing; using 150%% fallback\n");
    }
    if (preferences.user_load_result == UI_PREFERENCES_IO_INVALID ||
        preferences.user_load_result == UI_PREFERENCES_IO_FAILED) {
        fprintf(stderr, "UI preferences: user.ini invalid; using immutable default\n");
    }
    update_settings_scale_text(&menu_cache, &preferences);
    if (!app_ui_resources_create(&ui_resources, cfg->grid_width, cfg->grid_height)) {
        fprintf(stderr, "Failed to initialize bounded UI resources.\n");
        for (int i = 0; i < MENU_ID_COUNT; i++) ui_layout_destroy(menu_layouts[i]);
        ui_layout_destroy(hud_layout);
        ui_cache_destroy(&menu_cache);
        app_resources_cleanup(&resources);
        return 1;
    }

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

        if (benchmark_session_should_stop(mode, frame_count, benchmark_frame_limit,
                                          elapsed_total_sec, run_duration_seconds)) {
            input.quit = true; break;
        }

        double delta_time_ms = (double)((start_time - last_time) * 1000) / SDL_GetPerformanceFrequency();
        last_time = start_time;

        sync_editor_text_input(ren, app_state, &ued);
        input_process(&input, mode != RUN_MODE_NORMAL);
        if (input.quit && app_state == APP_STATE_EDITOR && ued.active) {
            input.quit = false;
            unified_editor_request_window_close(&ued);
        }

        if (mode == RUN_MODE_NORMAL) {
            UiPreferencesChangeResult scale_result;
            bool scale_changed = false;
            if (input.ui_scale_reset_pressed) {
                scale_result = ui_preferences_reset(&preferences);
                input.ui_scale_reset_pressed = false;
                scale_changed = true;
            } else if (input.ui_scale_increase_pressed) {
                scale_result = ui_preferences_increase(&preferences);
                input.ui_scale_increase_pressed = false;
                scale_changed = true;
            } else if (input.ui_scale_decrease_pressed) {
                scale_result = ui_preferences_decrease(&preferences);
                input.ui_scale_decrease_pressed = false;
                scale_changed = true;
            }
            if (scale_changed) {
                ui_scale_feedback_set(&scale_feedback, &preferences, scale_result);
                update_settings_scale_text(&menu_cache, &preferences);
            }
        }

        {
            bool want_lock = (mode == RUN_MODE_NORMAL)
                             && (ms.depth == 0)
                             && ((app_state == APP_STATE_PLAYING)
                                 || (app_state == APP_STATE_EDITOR
                                     && ued.active
                                     && ued.mode == EDITOR_MODE_WALK
                                      && ued.modal == EDITOR_MODAL_NONE
                                      && unified_editor_has_document(&ued)));
            if (want_lock != mouse_locked) {
                SDL_SetWindowRelativeMouseMode(ren->window, want_lock);
                mouse_locked = want_lock;
            }
        }

        if (input.esc) {
            if (ms.depth > 1 || (ms.depth > 0 && app_state != APP_STATE_MAIN_MENU)) {
                menu_stack_pop(&ms);
            } else if (app_state == APP_STATE_MAIN_MENU) {
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
                    const char *action = ui_ele_get_action(focused);
                    if (action) {
                        bool action_handled = dispatch_menu_action(
                            action, &ms, &app_state, &input, &ued, &cam, &assets,
                            &preferences, &scale_feedback, &menu_cache);
                        menu_controller_consume_confirm(
                            &input.confirm, &input.editor_confirm_pressed,
                            action_handled);
                    }
                }
                active_menu = menu_stack_peek(&ms);
            }
        }

        EditorInputConsumption ued_consume = {false, false};
        if (app_state == APP_STATE_EDITOR && menu_stack_peek(&ms) == MENU_NONE && ued.active) {
            ued_consume = unified_editor_update(&ued, &input, &cam,
                                                delta_time_ms / 1000.0,
                                                grid->height);
            if (ued_consume.keyboard_consumed) {
                input.up = false;
                input.down = false;
                input.confirm = false;
                input.esc = false;
                input.editor_toggle_mode_pressed = false;
                input.editor_select_pressed = false;
                input.editor_confirm_pressed = false;
                input.editor_cancel_pressed = false;
                input.editor_undo_pressed = false;
                input.editor_redo_pressed = false;
                input.editor_save_pressed = false;
                input.editor_save_as_pressed = false;
                input.editor_open_pressed = false;
                input.editor_import_pressed = false;
                input.editor_new_pressed = false;
                input.editor_reload_pressed = false;
                input.editor_text_backspace_pressed = false;
                input.editor_previous_pressed = false;
                input.editor_next_pressed = false;
            }
            if (ued_consume.pointer_consumed) {
                input.mouse_dx = 0.0f;
                input.mouse_dy = 0.0f;
            }
            if (ued.request_exit_to_main_menu) {
                unified_editor_destroy(&ued);
                menu_stack_clear(&ms);
                app_state = APP_STATE_MAIN_MENU;
                menu_stack_push(&ms, MENU_MAIN);
            }
            if (ued.request_window_close) {
                input.quit = true;
            }
        }

        double delta_time_sec = delta_time_ms / 1000.0;
        active_menu = menu_stack_peek(&ms);

#if PROFILE_FRAME
        if (mode == RUN_MODE_BENCHMARK_SCENARIO &&
            frame_count == BENCHMARK_WARMUP_FRAMES) {
            frame_profile_init(&g_frame_profile);
        }
        double profile_grid_start = profile_now_ms();
#endif

        UiLayerList ui_layers;
        ui_layer_list_clear(&ui_layers);
        ui_canvas_clear(ui_resources.menu);
        ui_canvas_clear(ui_resources.hud);
        ui_canvas_clear(ui_resources.editor);
        ui_canvas_clear(ui_resources.footer);
        ui_canvas_clear(ui_resources.feedback);
        ui_canvas_clear(ui_resources.crosshair);

        if (layered_ui_benchmark) {
            prepare_layered_ui_benchmark(&ui_resources, &ui_layers,
                                         grid->width * 8, grid->height * 8,
                                         frame_count);
        }

        if (active_menu != MENU_NONE) {
            SDL_Color mbg = {0, 0, 0, 255};
            UiLayout *active_layout = ((int)active_menu >= 0 && (int)active_menu < MENU_ID_COUNT)
                                      ? menu_layouts[(int)active_menu] : NULL;
            grid_clear(grid, mbg);
            (void)grid_clear_region_zero(
                ui_resources.staging,
                (grid->width - APP_UI_MENU_WIDTH) / 2,
                (grid->height - APP_UI_MENU_HEIGHT) / 2,
                APP_UI_MENU_WIDTH, APP_UI_MENU_HEIGHT);
            draw_data_menu(ui_resources.staging, active_layout,
                           menu_selected[(int)active_menu]);
            ui_canvas_copy_grid_region(
                ui_resources.menu, ui_resources.staging,
                (grid->width - APP_UI_MENU_WIDTH) / 2,
                (grid->height - APP_UI_MENU_HEIGHT) / 2);
            (void)app_add_ui_layer(&ui_layers, APP_UI_ROLE_MENU, ui_resources.menu,
                                   UI_ANCHOR_CENTER, UI_SCALE_INHERIT_GLOBAL, 20,
                                   grid->width * 8, grid->height * 8);

        } else if (app_state == APP_STATE_PLAYING) {
            (void)grid_clear_region_zero(ui_resources.staging, 0, 0,
                                         APP_UI_HUD_WIDTH, APP_UI_HUD_HEIGHT);
            if (visual_mode == VISUAL_RAYCAST) {
                frame_dispatch_apply_scenario(grid, &cam, benchmark_scenario, frame_count);
                camera_update(&cam, map, &input, delta_time_sec, grid->height);
                lighting_update(map, &world);
                raycast_render(grid, map, &cam, &assets, &world, NULL);
            } else if (visual_mode == VISUAL_STRESS) {
                draw_stress_pattern(grid, frame_count);
            } else {
                draw_world_pattern(grid, frame_count);
            }
            if (cfg->debug_display_enabled && !layered_ui_benchmark) {
                draw_data_ui_overlay(ui_resources.staging, &menu_cache, hud_layout, frame_count,
                                     &perf_stats, visual_mode, cfg->target_fps);
                ui_canvas_copy_grid_region(ui_resources.hud, ui_resources.staging, 0, 0);
                (void)app_add_ui_layer(&ui_layers, APP_UI_ROLE_HUD, ui_resources.hud,
                                       UI_ANCHOR_TOP_LEFT, UI_SCALE_INHERIT_GLOBAL, 10,
                                       grid->width * 8, grid->height * 8);
            }

#if PROFILE_FRAME
            g_frame_profile.raycast_grid_ms += (profile_now_ms() - profile_grid_start);
#endif

        } else if (app_state == APP_STATE_EDITOR) {
            (void)grid_clear_region_zero(ui_resources.staging, 0, 0,
                                         APP_UI_EDITOR_WIDTH, APP_UI_EDITOR_HEIGHT);
            (void)grid_clear_region_zero(ui_resources.staging, 0, grid->height - 2,
                                         APP_UI_FOOTER_WIDTH, 2);
            if (ued.active) {
                Map *ed_map = unified_editor_has_document(&ued)
                    ? scene_document_get_map_for_runtime(&ued.document)
                    : NULL;
                if (ed_map && ed_map->cells && ed_map->width > 0 && ed_map->height > 0) {
                    SceneSurfaceView surface_view;
                    const SceneSurfaceView *surfaces =
                        scene_document_get_surface_view(&ued.document, &surface_view)
                            ? &surface_view : NULL;
                    size_t editor_light_count = 0U;
                    const SceneLight *editor_lights = scene_document_get_lights(
                        &ued.document, &editor_light_count);
                    lighting_update(ed_map, &ued.runtime_world);
                    raycast_render(grid, ed_map, &cam, &assets, &ued.runtime_world,
                                   surfaces);
                    editor_highlight_render(grid, ed_map, &cam,
                                            editor_lights, editor_light_count,
                                            ued.selection, ued.hover);
                } else {
                    SDL_Color ae_bg = {0, 0, 0, 255};
                    grid_clear(grid, ae_bg);
                }
                unified_editor_render_text_overlay(&ued, ui_resources.staging);
                ui_canvas_copy_grid_region(ui_resources.editor, ui_resources.staging, 0, 0);
                ui_canvas_copy_grid_region(ui_resources.footer, ui_resources.staging,
                                           0, grid->height - 2);
                (void)app_add_ui_layer(&ui_layers, APP_UI_ROLE_EDITOR,
                                       ui_resources.editor, UI_ANCHOR_TOP_LEFT,
                                       UI_SCALE_INHERIT_GLOBAL, 10,
                                       grid->width * 8, grid->height * 8);
                (void)app_add_ui_layer(&ui_layers, APP_UI_ROLE_FOOTER,
                                       ui_resources.footer, UI_ANCHOR_BOTTOM_LEFT,
                                       UI_SCALE_INHERIT_GLOBAL, 11,
                                       grid->width * 8, grid->height * 8);
                if (unified_editor_crosshair_visible(&ued)) {
                    int center_x = grid->width / 2;
                    int center_y = grid->height / 2;
                    Cell crosshair;
                    ui_resources.staging->cells[center_y * grid->width + center_x] =
                        grid->cells[center_y * grid->width + center_x];
                    editor_crosshair_render(ui_resources.staging);
                    crosshair = ui_resources.staging->cells[center_y * grid->width + center_x];
                    (void)ui_canvas_set(ui_resources.crosshair, 0, 0, crosshair.glyph,
                                        crosshair.fg, crosshair.bg);
                    (void)app_add_ui_layer(&ui_layers, APP_UI_ROLE_CROSSHAIR,
                                           ui_resources.crosshair, UI_ANCHOR_CENTER,
                                           UI_SCALE_FIXED_100, 100,
                                           grid->width * 8, grid->height * 8);
                }
            } else {
                SDL_Color ae_bg = {0, 0, 0, 255};
                grid_clear(grid, ae_bg);
            }

        } else {
            SDL_Color bg = {0, 0, 0, 255};
            grid_clear(grid, bg);
        }

        if (mode == RUN_MODE_NORMAL && scale_feedback.frames_remaining > 0) {
            SDL_Color feedback_fg = {255, 255, 255, 255};
            SDL_Color feedback_bg = {80, 0, 0, 255};
            ui_canvas_print(ui_resources.feedback, 1, 0, scale_feedback.text,
                            feedback_fg, feedback_bg);
            (void)app_add_ui_layer(&ui_layers, APP_UI_ROLE_FEEDBACK,
                                   ui_resources.feedback, UI_ANCHOR_TOP_LEFT,
                                   UI_SCALE_INHERIT_GLOBAL, 90,
                                   grid->width * 8, grid->height * 8);
            scale_feedback.frames_remaining--;
        }

        uint64_t render_start = SDL_GetPerformanceCounter();
        if (mode == RUN_MODE_NORMAL || layered_ui_benchmark) {
            renderer_draw_layers(ren, grid, &ui_layers,
                                 layered_ui_benchmark ? 150 : ui_preferences_scale(&preferences));
        } else {
            renderer_draw(ren, grid);
        }
        uint64_t render_end = SDL_GetPerformanceCounter();
        double current_render_ms = (double)((render_end - render_start) * 1000) / SDL_GetPerformanceFrequency();
        if (mode != RUN_MODE_BENCHMARK_SCENARIO ||
            benchmark_session_frame_is_measured(frame_count)) {
            global_total_render_ms += current_render_ms;
        }

        if (benchmark_session_frame_is_measured(frame_count)) {
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

        if (benchmark_session_frame_is_measured(frame_count) && spare_time_ms < global_min_spare_ms)
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

    if (benchmark_session_is_active(mode)) {
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
    resources.state_tracker_initialized = false;
    smc_state_tracker_shutdown();
#endif
#if defined(USE_SMC_INDEXED_STATE_TRACKER) || defined(USE_SMC_BATCH_STATE_TRACKER) || defined(USE_SMC_STREAM_STATE_TRACKER)
    resources.indexed_tracker_initialized = false;
    smc_indexed_state_tracker_shutdown();
#endif
    unified_editor_destroy(&ued);
    for (int i = 0; i < MENU_ID_COUNT; i++) ui_layout_destroy(menu_layouts[i]);
    ui_layout_destroy(hud_layout);
    ui_cache_destroy(&menu_cache);
    app_ui_resources_destroy(&ui_resources);
    app_resources_cleanup(&resources);

    if (benchmark_session_is_active(mode)) {
        uint64_t measured_frames = mode == RUN_MODE_BENCHMARK_SCENARIO
            ? benchmark_session_measured_frames(frame_count) : frame_count;
        double avg_render_ms = measured_frames > 0
            ? (global_total_render_ms / (double)measured_frames) : 0.0;
        double effective_worst = absolute_worst_render_ms;
        if (absolute_worst_render_ms > second_worst_render_ms * 2.0 && second_worst_render_ms > 0) {
            effective_worst = second_worst_render_ms; outlier_trimmed = true;
        }
        BenchmarkResult result = benchmark_session_classify(
            avg_render_ms, renderer_alloc_count > initial_alloc_count ||
                           renderer_texture_create_count > initial_texture_count);
        const char *result_str = benchmark_session_result_name(result);
        int exit_code = benchmark_session_exit_code(result);
        printf("{\n  \"grid_width\": %d,\n  \"grid_height\": %d,\n  \"target_fps\": %d,\n  \"scenario\": \"%s\",\n  \"ui_scale_percent\": %d,\n  \"warmup_frames\": %u,\n  \"avg_render_ms\": %.2f,\n  \"worst_render_ms\": %.2f,\n  \"effective_worst_ms\": %.2f,\n  \"outlier_trimmed\": %s,\n  \"min_spare_ms\": %.2f,\n  \"frames\": %llu,\n  \"framebuffer_checksum\": %u,\n  \"result\": \"%s\"\n}\n",
               cfg->grid_width, cfg->grid_height, cfg->target_fps,
               benchmark_scenario ? benchmark_scenario : "timed",
               layered_ui_benchmark ? 150 : 0,
               mode == RUN_MODE_BENCHMARK_SCENARIO ? BENCHMARK_WARMUP_FRAMES : 0U,
               avg_render_ms, absolute_worst_render_ms, effective_worst,
               outlier_trimmed ? "true" : "false", global_min_spare_ms,
               (unsigned long long)measured_frames, framebuffer_checksum, result_str);
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