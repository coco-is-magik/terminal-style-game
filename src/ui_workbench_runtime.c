#include "ui_workbench_runtime.h"

#include "input.h"
#include "timing.h"
#include "ui_app_theme_adapter.h"
#include "ui_canvas.h"
#include "ui_compositor.h"
#include "ui_preferences.h"
#include "ui_workbench.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static void draw_selection(Grid *grid, UiElement *element, SDL_Color color,
                           uint64_t ticks) {
    int x;
    int y;
    int width;
    int height;
    SDL_Color bg = {0, 0, 0, 255};
    if (!ui_ele_absolute_bounds(element, &x, &y, &width, &height) ||
        width <= 0 || height <= 0) return;
    if (strcmp(element->focus_effect, "focus_pulse") == 0 &&
        ((ticks / 180U) % 2U) != 0U) color = (SDL_Color){255, 255, 255, 255};
    if (strcmp(element->focus_effect, "focus_glitch") == 0)
        y += (int)((ticks / 90U) % 3U) - 1;
    if (strcmp(element->transition, "center_out") == 0) {
        int center = x + width / 2;
        (void)grid_set(grid, center - 1, y - 1, '<', color, bg);
        (void)grid_set(grid, center + 1, y - 1, '>', color, bg);
    } else if (strcmp(element->transition, "perimeter_burst") == 0) {
        int offset = (int)((ticks / 100U) % 3U);
        (void)grid_set(grid, x - 1 - offset, y, '*', color, bg);
        (void)grid_set(grid, x + width + offset, y, '*', color, bg);
    } else if (strcmp(element->transition, "local_glitch") == 0) {
        x += (int)((ticks / 75U) % 3U) - 1;
    }
    (void)grid_set(grid, x - 1, y, '>', color, bg);
    (void)grid_set(grid, x + width, y, '<', color, bg);
}

static void draw_help(Grid *grid, const UiWorkbench *workbench,
                      const UiAppWorkbenchPalette *palette, int scale_percent) {
    char line[256];
    UiElement *element = ui_workbench_current_element((UiWorkbench *)workbench);
    (void)snprintf(line, sizeof(line),
        "UI WORKBENCH | %s | %s | %s=%s | scale=%d%%",
        workbench->layout ? workbench->layout->name : "none",
        element ? element->name : "none",
        ui_workbench_property_name(workbench->property),
        ui_workbench_current_value(workbench), scale_percent);
    grid_print(grid, 1, grid->height - 3, line, palette->primary_text, palette->canvas);
    grid_print(grid, 1, grid->height - 2,
        "Up/Down element | Enter move mode | arrows move | Tab property | [ or ] value | Ctrl+Left/Right layout | Ctrl+Enter action | Ctrl+-/+ scale | Esc exit",
        palette->secondary_text, palette->canvas);
    if (element && strcmp(element->focus_effect, "input_hold_short") == 0)
        grid_print(grid, 1, grid->height - 1,
                   "Preview metadata only: input_hold_short has no production execution",
                   palette->secondary_text, palette->canvas);
    else
        grid_print(grid, 1, grid->height - 1, workbench->status,
                   palette->secondary_text, palette->canvas);
}

static void set_scale_status(UiWorkbench *workbench,
                             const UiPreferences *preferences,
                             UiPreferencesChangeResult result) {
    const char *suffix = result == UI_PREFERENCES_CHANGE_ACTIVE_NOT_SAVED
        ? " active; preference not saved"
        : result == UI_PREFERENCES_CHANGE_SAVED_DURABILITY_WARNING
            ? " saved; durability warning" : "";
    (void)snprintf(workbench->status, sizeof(workbench->status),
                   "UI Scale: %d%%%s", ui_preferences_scale(preferences), suffix);
}

UiWorkbenchRuntimeResult ui_workbench_runtime_run(Renderer *renderer, Grid *grid,
                                                   int target_fps) {
    UiWorkbench workbench;
    UiAppWorkbenchPalette palette;
    InputState input = {0};
    UiCanvas *canvas;
    UiPreferences preferences;
    bool should_exit = false;
    double target_ms;
    if (!renderer || !renderer->window || !renderer->sdl_ren || !grid || !grid->cells ||
        target_fps <= 0 || !ui_app_theme_workbench_palette(&palette))
        return UI_WORKBENCH_RUNTIME_INVALID_ARGUMENT;
    ui_workbench_init(&workbench);
    if (ui_workbench_open(&workbench, MENU_MAIN) != UI_WORKBENCH_OK) {
        ui_workbench_destroy(&workbench);
        return UI_WORKBENCH_RUNTIME_LOAD_FAILED;
    }
    ui_preferences_init(&preferences, "default_user.ini", "user.ini");
    if (preferences.default_load_result != UI_PREFERENCES_IO_OK)
        fprintf(stderr, "UI preferences: default_user.ini invalid or missing; using 150%% fallback\n");
    if (preferences.user_load_result == UI_PREFERENCES_IO_INVALID ||
        preferences.user_load_result == UI_PREFERENCES_IO_FAILED)
        fprintf(stderr, "UI preferences: user.ini invalid; using immutable default\n");
    canvas = ui_canvas_create(grid->width, grid->height);
    if (!canvas) {
        ui_canvas_destroy(canvas);
        ui_workbench_destroy(&workbench);
        return UI_WORKBENCH_RUNTIME_RENDER_FAILED;
    }
    (void)SDL_SetWindowTitle(renderer->window, "ASCII FPS - UI Workbench (Live Writes)");
    target_ms = timing_target_ms(target_fps);
    while (!should_exit) {
        uint64_t start = SDL_GetPerformanceCounter();
        UiElement *element;
        double frame_ms;
        uint32_t sleep_ms;
        UiLayerList layers;
        UiLayer layer;
        input_process(&input, false);
        if (input.esc || input.quit) should_exit = true;
        else if (input.ui_scale_reset_pressed) {
            set_scale_status(&workbench, &preferences,
                             ui_preferences_reset(&preferences));
        } else if (input.ui_scale_increase_pressed ||
                   input.ui_scale_decrease_pressed) {
            UiPreferencesChangeResult result = input.ui_scale_increase_pressed
                ? ui_preferences_increase(&preferences)
                : ui_preferences_decrease(&preferences);
            set_scale_status(&workbench, &preferences, result);
        }
        else if (input.ctrl_left || input.ctrl_right) {
            int context = (int)workbench.context + (input.ctrl_right ? 1 : 3);
            context = ((context - (int)MENU_MAIN) % 4) + (int)MENU_MAIN;
            (void)ui_workbench_open(&workbench, (MenuId)context);
        } else if (input.ctrl_confirm) {
            (void)ui_workbench_invoke(&workbench);
        } else if (input.confirm) {
            (void)ui_workbench_toggle_editing(&workbench);
        } else if (input.tab) {
            (void)ui_workbench_cycle_property(&workbench, 1);
        } else if (input.prev_glyph || input.next_glyph) {
            (void)ui_workbench_cycle_value(&workbench, input.next_glyph ? 1 : -1);
        } else if (workbench.editing &&
                   (input.arrow_left || input.arrow_right || input.up || input.down)) {
            (void)ui_workbench_move(&workbench,
                input.arrow_right ? 1 : input.arrow_left ? -1 : 0,
                input.down ? 1 : input.up ? -1 : 0);
        } else if (!workbench.editing && (input.up || input.down)) {
            (void)ui_workbench_cycle_element(&workbench, input.down ? 1 : -1);
        }
        (void)grid_clear_region_zero(grid, 0, 0, grid->width, grid->height);
        ui_layout_render(workbench.layout, grid, palette.secondary_text, palette.canvas);
        element = ui_workbench_current_element(&workbench);
        draw_selection(grid, element, palette.border, SDL_GetTicks());
        ui_canvas_copy_grid_region(canvas, grid, 0, 0);
        grid_clear(grid, palette.canvas);
        draw_help(grid, &workbench, &palette, ui_preferences_scale(&preferences));
        ui_layer_list_clear(&layers);
        layer = (UiLayer){
            1, canvas, UI_ANCHOR_CENTER,
            {0, 0, renderer->logical_w, renderer->logical_h},
            UI_SCALE_INHERIT_GLOBAL, 100, 1, true, 0U
        };
        if (!ui_layer_list_add(&layers, &layer)) {
            ui_canvas_destroy(canvas);
            ui_workbench_destroy(&workbench);
            return UI_WORKBENCH_RUNTIME_RENDER_FAILED;
        }
        renderer_draw_layers(renderer, grid, &layers,
                             ui_preferences_scale(&preferences));
        frame_ms = (double)(SDL_GetPerformanceCounter() - start) * 1000.0 /
                   (double)SDL_GetPerformanceFrequency();
        sleep_ms = timing_sleep_ms(timing_spare_ms(frame_ms, target_ms));
        if (sleep_ms > 0U) SDL_Delay(sleep_ms);
    }
    ui_canvas_destroy(canvas);
    ui_workbench_destroy(&workbench);
    return UI_WORKBENCH_RUNTIME_OK;
}