#include "ui_workbench_runtime.h"

#include "input.h"
#include "timing.h"
#include "ui_app_theme_adapter.h"
#include "ui_animation.h"
#include "ui_canvas.h"
#include "ui_compositor.h"
#include "ui_preferences.h"
#include "ui_workbench.h"
#include "ui_workbench_chrome.h"
#include "ui_workbench_guide.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static void draw_selection(Grid *grid, const UiAppWorkbenchPalette *palette,
                           UiElement *element) {
    int x;
    int y;
    int width;
    int height;
    SDL_Color bg;
    SDL_Color color;
    if (!palette) return;
    bg = palette->canvas;
    color = palette->accent;
    if (!ui_ele_absolute_bounds(element, &x, &y, &width, &height) ||
        width <= 0 || height <= 0) return;
    (void)grid_set(grid, x - 1, y, '>', color, bg);
    (void)grid_set(grid, x + width, y, '<', color, bg);
}

static int selected_focus_index(const UiWorkbench *workbench) {
    int i;
    int count;
    UiElement *selected;
    if (!workbench || !workbench->layout) return -1;
    selected = ui_workbench_current_element((UiWorkbench *)workbench);
    count = ui_layout_focusable_count(workbench->layout);
    for (i = 0; i < count; i++)
        if (ui_layout_get_focused(workbench->layout, i) == selected) return i;
    return -1;
}

bool ui_workbench_runtime_compose_footer_canvas(UiCanvas *canvas, Grid *grid,
                                                const UiWorkbench *workbench,
                                                const UiAppWorkbenchPalette *palette,
                                                const char *tooltip_text,
                                                int scale_percent,
                                                bool reduced_motion) {
    int footer_first;
    if (!canvas || !canvas->cells || !grid || !grid->cells || !workbench ||
        !workbench->layout || !palette || !ui_preferences_is_valid_scale(scale_percent) ||
        canvas->width != grid->width ||
        canvas->height != UI_WORKBENCH_CHROME_FOOTER_ROWS) return false;
    footer_first = ui_workbench_chrome_footer_first_row(grid->height);
    if (footer_first < 0) return false;
    (void)grid_clear_region_zero(grid, 0, 0, grid->width, grid->height);
    (void)ui_workbench_chrome_footer_rows(grid, palette, workbench,
                                          tooltip_text, scale_percent,
                                          reduced_motion);
    ui_canvas_copy_grid_region(canvas, grid, 0, footer_first);
    return true;
}

bool ui_workbench_runtime_build_layers(UiLayerList *layers,
                                       const UiCanvas *preview_canvas,
                                       const UiCanvas *footer_canvas,
                                       int logical_w, int logical_h,
                                       int authored_scale_percent,
                                       int workbench_scale_percent) {
    UiLayer preview_layer;
    UiLayer footer_layer;
    if (!layers || !preview_canvas || !preview_canvas->cells || !footer_canvas ||
        !footer_canvas->cells || logical_w <= 0 || logical_h <= 0 ||
        !ui_preferences_is_valid_scale(authored_scale_percent) ||
        !ui_preferences_is_valid_scale(workbench_scale_percent)) return false;
    ui_layer_list_clear(layers);
    preview_layer = (UiLayer){
        1, preview_canvas, UI_ANCHOR_CENTER,
        {0, 0, logical_w, logical_h},
        UI_SCALE_EXPLICIT_PRESET, authored_scale_percent, 1, true, 0U
    };
    footer_layer = (UiLayer){
        2, footer_canvas, UI_ANCHOR_BOTTOM_LEFT,
        {0, 0, logical_w, logical_h},
        UI_SCALE_EXPLICIT_PRESET, workbench_scale_percent, 2, true, 0U
    };
    if (!ui_layer_list_add(layers, &preview_layer)) return false;
    return ui_layer_list_add(layers, &footer_layer);
}

int ui_workbench_runtime_step_scale(int scale_percent, int direction) {
    static const int scales[] = {100, 125, 150, 200};
    size_t i;
    if (!ui_preferences_is_valid_scale(scale_percent) || direction == 0)
        return scale_percent;
    for (i = 0U; i < sizeof(scales) / sizeof(scales[0]); i++) {
        if (scales[i] != scale_percent) continue;
        if (direction > 0 && i + 1U < sizeof(scales) / sizeof(scales[0]))
            return scales[i + 1U];
        if (direction < 0 && i > 0U) return scales[i - 1U];
        return scale_percent;
    }
    return scale_percent;
}

static void set_scale_status(UiWorkbench *workbench, int authored_scale_percent,
                             int workbench_scale_percent) {
    (void)snprintf(workbench->status, sizeof(workbench->status),
                   "Preview: %d%% | Workbench: %d%%",
                   authored_scale_percent, workbench_scale_percent);
}

UiWorkbenchRuntimeResult ui_workbench_runtime_run(Renderer *renderer, Grid *grid,
                                                   int target_fps) {
    UiWorkbench workbench;
    UiAppWorkbenchPalette palette;
    UiWorkbenchGuide guide;
    InputState input = {0};
    UiCanvas *preview_canvas;
    UiCanvas *footer_canvas;
    UiPreferences preferences;
    int authored_scale_percent;
    int workbench_scale_percent = 100;
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
    if (ui_workbench_guide_load(&guide, "assets/editor_tooltips.txt") !=
        UI_WORKBENCH_GUIDE_OK) {
        ui_workbench_destroy(&workbench);
        return UI_WORKBENCH_RUNTIME_LOAD_FAILED;
    }
    ui_preferences_init(&preferences, "default_user.ini", "user.ini");
    if (preferences.default_load_result != UI_PREFERENCES_IO_OK)
        fprintf(stderr, "UI preferences: default_user.ini invalid or missing; using 150%% fallback\n");
    if (preferences.user_load_result == UI_PREFERENCES_IO_INVALID ||
        preferences.user_load_result == UI_PREFERENCES_IO_FAILED)
        fprintf(stderr, "UI preferences: user.ini invalid; using immutable default\n");
    authored_scale_percent = ui_preferences_scale(&preferences);
    set_scale_status(&workbench, authored_scale_percent,
                     workbench_scale_percent);
    preview_canvas = ui_canvas_create(grid->width, grid->height);
    footer_canvas = ui_canvas_create(grid->width, UI_WORKBENCH_CHROME_FOOTER_ROWS);
    if (!preview_canvas || !footer_canvas) {
        ui_workbench_guide_destroy(&guide);
        ui_canvas_destroy(preview_canvas);
        ui_canvas_destroy(footer_canvas);
        ui_workbench_destroy(&workbench);
        return UI_WORKBENCH_RUNTIME_RENDER_FAILED;
    }
    (void)SDL_SetWindowTitle(renderer->window, "ASCII FPS - UI Workbench (Live Writes)");
    target_ms = timing_target_ms(target_fps);
    uint64_t preview_start = SDL_GetTicks();
    while (!should_exit) {
        uint64_t start = SDL_GetPerformanceCounter();
        UiElement *element;
        double frame_ms;
        uint32_t sleep_ms;
        UiLayerList layers;
        input_process(&input, false);
        if (input.esc && workbench.mode != UI_WORKBENCH_MODE_BROWSE)
            ui_workbench_cancel_mode(&workbench);
        else if (input.esc || input.quit) should_exit = true;
        else if (workbench.mode == UI_WORKBENCH_MODE_ADD) {
            if (input.up || input.down)
                (void)ui_workbench_cycle_add_source(&workbench, input.down ? 1 : -1);
            else if (input.confirm) (void)ui_workbench_confirm_add(&workbench);
        } else if (workbench.mode == UI_WORKBENCH_MODE_REMOVE_CONFIRM) {
            if (input.confirm) (void)ui_workbench_confirm_remove(&workbench);
        }
        else if (input.editor_new_pressed) {
            (void)ui_workbench_begin_add(&workbench);
        } else if (input.erase) {
            (void)ui_workbench_request_remove(&workbench);
        }
        else if (input.ui_scale_reset_pressed) {
            workbench_scale_percent = 100;
            set_scale_status(&workbench, authored_scale_percent,
                             workbench_scale_percent);
        } else if (input.ui_scale_increase_pressed ||
                   input.ui_scale_decrease_pressed) {
            workbench_scale_percent = ui_workbench_runtime_step_scale(
                workbench_scale_percent,
                input.ui_scale_increase_pressed ? 1 : -1);
            set_scale_status(&workbench, authored_scale_percent,
                             workbench_scale_percent);
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
        ui_layout_set_focus(workbench.layout, selected_focus_index(&workbench));
        ui_layout_render(workbench.layout, grid, palette.secondary_text, palette.canvas);
        (void)ui_animation_render_layout(workbench.layout, grid,
                                         (double)(SDL_GetTicks() - preview_start),
                                         false, true, UI_ANIMATION_EVENT_PREVIEW);
        (void)ui_layout_render_focus_effect(
            workbench.layout, selected_focus_index(&workbench),
            grid, (double)(SDL_GetTicks() - preview_start), false,
            palette.primary_text, palette.border, palette.canvas);
        element = ui_workbench_current_element(&workbench);
        draw_selection(grid, &palette, element);
        ui_canvas_copy_grid_region(preview_canvas, grid, 0, 0);
        if (!ui_workbench_runtime_compose_footer_canvas(
                footer_canvas, grid, &workbench, &palette,
                ui_workbench_guide_tooltip(&guide, &workbench),
                workbench_scale_percent, false)) {
            ui_workbench_guide_destroy(&guide);
            ui_canvas_destroy(preview_canvas);
            ui_canvas_destroy(footer_canvas);
            ui_workbench_destroy(&workbench);
            return UI_WORKBENCH_RUNTIME_RENDER_FAILED;
        }
        grid_clear(grid, palette.canvas);
        ui_layer_list_clear(&layers);
        if (!ui_workbench_runtime_build_layers(&layers, preview_canvas,
                                               footer_canvas,
                                               renderer->logical_w,
                                               renderer->logical_h,
                                               authored_scale_percent,
                                               workbench_scale_percent)) {
            ui_workbench_guide_destroy(&guide);
            ui_canvas_destroy(preview_canvas);
            ui_canvas_destroy(footer_canvas);
            ui_workbench_destroy(&workbench);
            return UI_WORKBENCH_RUNTIME_RENDER_FAILED;
        }
        renderer_draw_layers(renderer, grid, &layers, 100);
        frame_ms = (double)(SDL_GetPerformanceCounter() - start) * 1000.0 /
                   (double)SDL_GetPerformanceFrequency();
        sleep_ms = timing_sleep_ms(timing_spare_ms(frame_ms, target_ms));
        if (sleep_ms > 0U) SDL_Delay(sleep_ms);
    }
    ui_workbench_guide_destroy(&guide);
    ui_canvas_destroy(preview_canvas);
    ui_canvas_destroy(footer_canvas);
    ui_workbench_destroy(&workbench);
    return UI_WORKBENCH_RUNTIME_OK;
}