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
#include <math.h>
#include <limits.h>

static void draw_selection(UiCanvas *overlay, const UiCanvas *authored,
                            const UiAppWorkbenchPalette *palette, UiElement *element,
                            bool selected) {
    int x;
    int y;
    int width;
    int height;
    SDL_Color bg;
    SDL_Color color;
    if (!palette) return;
    bg = palette->canvas;
    color = selected ? palette->editor_selection : palette->border;
    if (!ui_ele_absolute_bounds(element, &x, &y, &width, &height) ||
        width <= 0 || height <= 0) return;
    ui_canvas_clear(overlay);
    if (x <= INT_MIN || y <= INT_MIN || x > INT_MAX - width || y > INT_MAX - height) return;
    for (int row = 0; row < overlay->height; row++) {
        for (int column = 0; column < overlay->width; column++) {
            bool horizontal = (row == y - 1 || row == y + height) &&
                               column >= x - 1 && column <= x + width;
            bool vertical = (column == x - 1 || column == x + width) &&
                             row >= y - 1 && row <= y + height;
            if ((horizontal || vertical) && !ui_canvas_is_touched(authored, column, row))
                (void)ui_canvas_set(overlay, column, row,
                    horizontal && vertical ? '+' : horizontal ? (selected ? '=' : '-') : '|', color, bg);
        }
    }
}

static UiCanvas *selection_layer(UiLayerList *layers, const UiCanvas *authored,
                                 const UiAppWorkbenchPalette *palette,
                                 UiWorkbench *workbench) {
    UiCanvas *overlay = ui_canvas_create(authored->width, authored->height);
    UiLayer layer;
    if (!overlay) return NULL;
    draw_selection(overlay, authored, palette, ui_workbench_current_element(workbench), workbench->editing);
    layer = layers->layers[0];
    layer.canvas = overlay;
    layer.role_id = 3;
    layer.z_order = 1;
    if (!ui_layer_list_add(layers, &layer)) {
        ui_canvas_destroy(overlay);
        return NULL;
    }
    return overlay;
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

bool ui_workbench_runtime_preview(UiCanvas *canvas, Grid *staging,
                                  UiWorkbench *workbench,
                                  const UiAppWorkbenchPalette *palette,
                                  double elapsed_ms, bool reduced_motion) {
    return ui_workbench_runtime_preview_event(canvas, staging, workbench, palette,
                                              UI_ANIMATION_EVENT_PREVIEW,
                                              elapsed_ms, reduced_motion);
}

bool ui_workbench_runtime_preview_event(UiCanvas *canvas, Grid *staging,
                                        UiWorkbench *workbench,
                                        const UiAppWorkbenchPalette *palette,
                                        UiAnimationEvent event,
                                        double elapsed_ms, bool reduced_motion) {
    int focus;
    if (!canvas || !staging || !workbench || !workbench->layout || !palette ||
        canvas->width != staging->width || canvas->height != staging->height ||
        !isfinite(elapsed_ms) || elapsed_ms < 0.0 ||
        event < UI_ANIMATION_EVENT_CONTEXT_ENTER || event > UI_ANIMATION_EVENT_PREVIEW)
        return false;
    if (!grid_clear_region_zero(staging, 0, 0, staging->width, staging->height)) return false;
    focus = selected_focus_index(workbench);
    ui_layout_set_focus(workbench->layout, focus);
    ui_layout_render(workbench->layout, staging, palette->secondary_text, palette->canvas);
    if (!ui_animation_render_layout(workbench->layout, staging, elapsed_ms,
                                    reduced_motion, false, event)) return false;
    (void)ui_layout_render_focus_effect(workbench->layout, focus, staging, elapsed_ms,
                                        reduced_motion, palette->primary_text,
                                        palette->border, palette->canvas);
    ui_canvas_copy_grid_region(canvas, staging, 0, 0);
    return true;
}

bool ui_workbench_runtime_preview_events(UiCanvas *canvas, Grid *staging,
                                         UiWorkbench *workbench,
                                         const UiAppWorkbenchPalette *palette,
                                         const double ages[UI_ANIMATION_EVENT_PREVIEW],
                                         bool reduced_motion) {
    int focus;
    double focus_age;
    if (!ages || !canvas || !canvas->cells || !staging || !staging->cells ||
        !workbench || !workbench->layout || !palette ||
        canvas->width != staging->width || canvas->height != staging->height) return false;
    for (int event = 0; event < UI_ANIMATION_EVENT_PREVIEW; event++)
        if (!isfinite(ages[event])) return false;
    if (!grid_clear_region_zero(staging, 0, 0, staging->width, staging->height)) return false;
    focus = selected_focus_index(workbench);
    ui_layout_set_focus(workbench->layout, focus);
    ui_layout_render(workbench->layout, staging, palette->secondary_text, palette->canvas);
    for (int event = 0; event < UI_ANIMATION_EVENT_PREVIEW; event++) {
        if (ages[event] >= 0 && !ui_animation_render_layout(workbench->layout, staging,
                ages[event], reduced_motion, false, (UiAnimationEvent)event)) return false;
    }
    focus_age = ages[UI_ANIMATION_EVENT_FOCUS] >= 0 ? ages[UI_ANIMATION_EVENT_FOCUS] :
        ages[UI_ANIMATION_EVENT_CONTEXT_ENTER];
    (void)ui_layout_render_focus_effect(workbench->layout, focus, staging,
        focus_age >= 0 ? focus_age : 0, reduced_motion,
        palette->primary_text, palette->border, palette->canvas);
    ui_canvas_copy_grid_region(canvas, staging, 0, 0);
    return true;
}

bool ui_workbench_runtime_exit_overlay(UiCanvas *preview, Grid *staging,
                                       UiWorkbench *outgoing, double elapsed_ms,
                                       bool reduced_motion) {
    size_t count;
    UiCanvas *source;
    UiAppWorkbenchPalette palette;
    if (!preview || !preview->cells || !staging || !staging->cells || !outgoing ||
        !outgoing->layout || preview->width != staging->width ||
        preview->height != staging->height || !isfinite(elapsed_ms) || elapsed_ms < 0)
        return false;
    if (reduced_motion || elapsed_ms >=
        ui_theme_motion_duration_ms(UI_THEME_MOTION_MAJOR_EXIT, false)) return true;
    source = ui_canvas_create(staging->width, staging->height);
    if (!source) return false;
    if (!ui_app_theme_workbench_palette(&palette) ||
        !grid_clear_region_zero(staging, 0, 0, staging->width, staging->height)) {
        ui_canvas_destroy(source);
        return false;
    }
    ui_layout_render(outgoing->layout, staging, palette.secondary_text, palette.canvas);
    ui_canvas_copy_grid_region(source, staging, 0, 0);
    if (!ui_animation_render_layout(outgoing->layout, staging, elapsed_ms, false, false,
                                     UI_ANIMATION_EVENT_CONTEXT_EXIT)) {
        ui_canvas_destroy(source);
        return false;
    }
    count = (size_t)preview->width * (size_t)preview->height;
    for (size_t i = 0; i < count; i++) {
        if (!preview->touched[i] && !source->touched[i] && staging->cells[i].glyph != 0) {
            preview->cells[i] = staging->cells[i];
            preview->touched[i] = 1;
        }
    }
    ui_canvas_destroy(source);
    return true;
}

bool ui_workbench_runtime_snapshot(UiWorkbench *workbench,
                                   const UiAppWorkbenchPalette *palette,
                                   const char *tooltip_text,
                                   int authored_scale, int workbench_scale,
                                   double elapsed_ms, bool reduced_motion,
                                   uint32_t *pixels, size_t pixel_count) {
    Grid *staging = NULL;
    UiCanvas *preview = NULL;
    UiCanvas *footer = NULL;
    UiCanvas *overlay = NULL;
    UiLayerList layers;
    int width;
    int height;
    bool ok = false;
    const size_t count = 2080U * 1280U;
    if (!pixels || pixel_count != count || !palette ||
        !workbench || !ui_workbench_runtime_interface_size(workbench_scale, workbench->editing,
                                                           &width, &height)) return false;
    staging = grid_create(260, 160);
    preview = ui_canvas_create(260, 160);
    footer = ui_canvas_create(width, height);
    if (!staging || !preview || !footer) goto cleanup;
    if (!ui_workbench_runtime_preview(preview, staging, workbench, palette,
                                      elapsed_ms, reduced_motion) ||
        !ui_workbench_runtime_compose_footer_canvas(footer, staging, workbench,
            palette, tooltip_text, workbench_scale, reduced_motion) ||
        !ui_workbench_runtime_build_layers(&layers, preview, footer, 2080, 1280,
                                           authored_scale, workbench_scale)) goto cleanup;
    overlay = selection_layer(&layers, preview, palette, workbench);
    if (!overlay) goto cleanup;
    {
        uint32_t backdrop;
        size_t i;
        memcpy(&backdrop, &palette->canvas, sizeof(backdrop));
        for (i = 0; i < count; i++) pixels[i] = backdrop;
    }
    ok = ui_compositor_compose(&layers, 100, pixels, 2080, 1280, NULL, 260, 160);
cleanup:
    ui_canvas_destroy(overlay);
    ui_canvas_destroy(footer);
    ui_canvas_destroy(preview);
    grid_destroy(staging);
    return ok;
}

bool ui_workbench_runtime_footer_size(int scale_percent, int *width, int *height) {
    int rows_per_channel;
    if (!width || !height || !ui_preferences_is_valid_scale(scale_percent)) return false;
    *width = UI_WORKBENCH_CHROME_COLUMNS * 100 / scale_percent;
    rows_per_channel = (UI_WORKBENCH_CHROME_COLUMNS + *width - 1) / *width;
    *height = UI_WORKBENCH_CHROME_FOOTER_ROWS * rows_per_channel +
              (UI_WORKBENCH_GUIDE_TEXT_MAX + *width - 1) / *width;
    return true;
}

bool ui_workbench_runtime_interface_size(int scale_percent, bool editing, int *width, int *height) {
    if (!ui_workbench_runtime_footer_size(scale_percent, width, height)) return false;
    (void)editing;
    *height = UI_WORKBENCH_CHROME_ROWS * 100 / scale_percent;
    return true;
}

bool ui_workbench_runtime_compose_footer_canvas(UiCanvas *canvas, Grid *grid,
                                                const UiWorkbench *workbench,
                                                const UiAppWorkbenchPalette *palette,
                                                const char *tooltip_text,
                                                int scale_percent,
                                                bool reduced_motion) {
    int footer_first;
    int width;
    int height;
    int channel;
    int rows_per_channel;
    if (!canvas || !canvas->cells || !grid || !grid->cells || !workbench ||
        !workbench->layout || !palette || !ui_preferences_is_valid_scale(scale_percent) ||
        !ui_workbench_runtime_interface_size(scale_percent, workbench->editing, &width, &height) ||
        canvas->width != width || canvas->height != height) return false;
    footer_first = ui_workbench_chrome_footer_first_row(grid->height);
    if (footer_first < 0) return false;
    (void)grid_clear_region_zero(grid, 0, 0, grid->width, grid->height);
    if (!ui_workbench_chrome_footer_rows(grid, palette, workbench,
                                         NULL, scale_percent, reduced_motion)) return false;
    ui_canvas_clear(canvas);
    rows_per_channel = (grid->width + width - 1) / width;
    /* Each semantic channel owns its wrapped rows; magnification never clips text. */
    for (channel = 0; channel < UI_WORKBENCH_CHROME_FOOTER_ROWS; channel++) {
        int column;
        for (column = 0; column < grid->width; column++) {
            const Cell *cell = &grid->cells[(footer_first + channel) * grid->width + column];
            if (!ui_canvas_set(canvas, column % width,
                               (channel == 0 ? 0 : channel == 1 ? height - 3 * rows_per_channel :
                                height - rows_per_channel) + column / width,
                               cell->glyph, cell->fg, cell->bg)) return false;
        }
    }
    if (tooltip_text && workbench->mode == UI_WORKBENCH_MODE_HELP) {
        size_t i;
        size_t length = strlen(tooltip_text);
        if (length >= UI_WORKBENCH_GUIDE_TEXT_MAX) return false;
        for (i = 0; i < length; i++) {
            if (!ui_canvas_set(canvas, (int)(i % (size_t)width),
                               UI_WORKBENCH_CHROME_FOOTER_ROWS * rows_per_channel +
                               (int)(i / (size_t)width), (uint8_t)tooltip_text[i],
                               palette->secondary_text, palette->canvas)) return false;
        }
    }
    {
        int row;
        int column;
        if (!ui_workbench_chrome_edit_panels(grid, palette, workbench, width, 12)) return false;
        for (row = 0; row < 12; row++) {
            for (column = 0; column < width; column++) {
                const Cell *cell = &grid->cells[row * grid->width + column];
                if (!ui_canvas_set(canvas, column, height - 12 - 3 * rows_per_channel + row, cell->glyph,
                                    cell->fg, cell->bg)) return false;
            }
        }
    }
    {
        char menu[512];
        size_t used = 0;
        static const UiWorkbenchProperty properties[] = {
            UI_WORKBENCH_PROPERTY_TRANSITION, UI_WORKBENCH_PROPERTY_ADD,
            UI_WORKBENCH_PROPERTY_STYLE, UI_WORKBENCH_PROPERTY_CONTENT,
            UI_WORKBENCH_PROPERTY_VISIBLE, UI_WORKBENCH_PROPERTY_ALIGN,
            UI_WORKBENCH_PROPERTY_REMOVE
        };
        static const char *const labels[] = {"Transition", "Add", "Style", "Text", "Visible", "Align", "Remove"};
        int written = snprintf(menu, sizeof(menu), "MENU: ");
        size_t element_start = 0;
        if (written < 0) return false;
        used = (size_t)written;
        for (size_t i = 0; i < sizeof(properties) / sizeof(properties[0]); i++) {
            bool enabled = ui_workbench_category_enabled(workbench, properties[i]);
            if (i == 2) {
                written = snprintf(menu + used, sizeof(menu) - used, "| ELEMENT: ");
                if (written < 0 || (size_t)written >= sizeof(menu) - used) return false;
                element_start = used;
                used += (size_t)written;
            }
            written = snprintf(menu + used, sizeof(menu) - used, "%c%s%c  ",
                enabled && workbench->property == properties[i] ? '[' : ' ',
                labels[i],
                enabled && workbench->property == properties[i] ? ']' : ' ');
            if (written < 0 || (size_t)written >= sizeof(menu) - used) return false;
            for (size_t j = used; j < used + (size_t)written; j++)
                if (!ui_canvas_set(canvas, (int)(j % (size_t)width),
                    rows_per_channel + (int)(j / (size_t)width), (uint8_t)menu[j],
                    enabled ? palette->primary_text : palette->disabled_text, palette->canvas)) return false;
            used += (size_t)written;
        }
        for (size_t i = 0; i < used; i++) {
            if (i >= 6 && (i < element_start || i >= element_start + 11)) continue;
            if (!ui_canvas_set(canvas, (int)(i % (size_t)width),
                rows_per_channel + (int)(i / (size_t)width), (uint8_t)menu[i],
                ((i < 6) != workbench->editing) ? palette->primary_text : palette->disabled_text,
                palette->canvas)) return false;
        }
    }
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
    int footer_width;
    int footer_height;
    bool should_exit = false;
    bool reduced_motion = false;
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
    if (!ui_workbench_runtime_interface_size(workbench_scale_percent, workbench.editing, &footer_width,
                                          &footer_height)) {
        ui_canvas_destroy(preview_canvas);
        ui_workbench_guide_destroy(&guide);
        ui_workbench_destroy(&workbench);
        return UI_WORKBENCH_RUNTIME_RENDER_FAILED;
    }
    footer_canvas = ui_canvas_create(footer_width, footer_height);
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
    uint64_t event_start[UI_ANIMATION_EVENT_PREVIEW] = {0};
    bool event_active[UI_ANIMATION_EVENT_PREVIEW] = {false};
    event_start[UI_ANIMATION_EVENT_CONTEXT_ENTER] = preview_start;
    event_start[UI_ANIMATION_EVENT_WHILE_VISIBLE] = preview_start;
    event_active[UI_ANIMATION_EVENT_CONTEXT_ENTER] = true;
    event_active[UI_ANIMATION_EVENT_WHILE_VISIBLE] = true;
    UiWorkbench outgoing;
    uint64_t exit_start = preview_start;
    ui_workbench_init(&outgoing);
    while (!should_exit) {
        uint64_t start = SDL_GetPerformanceCounter();
        UiCanvas *overlay;
        double frame_ms;
        uint32_t sleep_ms;
        UiLayerList layers;
        MenuId previous_context = workbench.context;
        int previous_element = workbench.element_index;
        input_process(&input, false);
        if (workbench.mode == UI_WORKBENCH_MODE_BROWSE &&
            (input.ctrl_left || input.ctrl_right || input.ctrl_confirm)) {
            ui_workbench_destroy(&outgoing);
            ui_workbench_init(&outgoing);
            if (!reduced_motion && ui_workbench_open(&outgoing, workbench.context) == UI_WORKBENCH_OK)
                exit_start = SDL_GetTicks();
        }
        if (workbench.mode == UI_WORKBENCH_MODE_TEXT) {
            if (input.quit) should_exit = true;
            else if (input.esc) ui_workbench_cancel_mode(&workbench);
            else if (input.confirm) (void)ui_workbench_confirm_text(&workbench);
            else (void)ui_workbench_text_input(&workbench, input.text_input,
                                               input.editor_text_backspace_pressed);
            if (workbench.mode != UI_WORKBENCH_MODE_TEXT || should_exit)
                (void)SDL_StopTextInput(renderer->window);
        } else if (input.esc && workbench.mode != UI_WORKBENCH_MODE_BROWSE)
            ui_workbench_cancel_mode(&workbench);
        else if (input.esc && workbench.editing) (void)ui_workbench_toggle_editing(&workbench);
        else if (input.esc || input.quit) should_exit = true;
        else if (workbench.mode == UI_WORKBENCH_MODE_HELP) {
            if (input.up || input.down)
                ui_workbench_cycle_help(&workbench, input.down ? 1 : -1);
        } else if (input.load) {
            (void)ui_workbench_open_help(&workbench);
        } else if (input.save_as) {
            reduced_motion = !reduced_motion;
        } else if (input.save) {
            (void)ui_workbench_open(&workbench, workbench.context);
        } else if (input.editor_undo_pressed) {
            (void)ui_workbench_undo_membership(&workbench);
        }
        else if (input.editor_place_sprite_pressed) {
            ui_workbench_cycle_preview(&workbench);
        } else if (workbench.mode == UI_WORKBENCH_MODE_ADD) {
            if (input.tab) {
                ui_workbench_cancel_mode(&workbench);
                (void)ui_workbench_cycle_property(&workbench, 1);
            } else if (input.up || input.down || input.prev_glyph || input.next_glyph)
                (void)ui_workbench_cycle_add_source(&workbench, input.down || input.next_glyph ? 1 : -1);
            else if (input.confirm) (void)ui_workbench_confirm_add(&workbench);
        } else if (workbench.mode == UI_WORKBENCH_MODE_REMOVE_CONFIRM) {
            if (input.confirm) (void)ui_workbench_confirm_remove(&workbench);
        }
        else if (input.editor_new_pressed && !workbench.editing) {
            workbench.property = UI_WORKBENCH_PROPERTY_ADD;
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
            if (workbench.editing && workbench.property == UI_WORKBENCH_PROPERTY_REMOVE) {
                if (workbench.remove_choice && ui_workbench_request_remove(&workbench) == UI_WORKBENCH_OK)
                    (void)ui_workbench_confirm_remove(&workbench);
            } else (void)ui_workbench_toggle_editing(&workbench);
        } else if (input.tab) {
            (void)ui_workbench_cycle_property(&workbench, 1);
        } else if (input.prev_glyph || input.next_glyph) {
            if (workbench.property == UI_WORKBENCH_PROPERTY_ADD && !workbench.editing)
                (void)ui_workbench_begin_add(&workbench);
            else if (workbench.property == UI_WORKBENCH_PROPERTY_REMOVE && workbench.editing)
                workbench.remove_choice = !workbench.remove_choice;
            else if (ui_workbench_category_enabled(&workbench, workbench.property))
                (void)ui_workbench_cycle_value(&workbench, input.next_glyph ? 1 : -1);
            if (workbench.mode == UI_WORKBENCH_MODE_TEXT &&
                !SDL_StartTextInput(renderer->window)) {
                ui_workbench_cancel_mode(&workbench);
                (void)snprintf(workbench.status, sizeof(workbench.status),
                               "Text input unavailable; content preserved. Retry.");
            }
        } else if (workbench.editing &&
                   (input.arrow_left || input.arrow_right || input.up || input.down)) {
            (void)ui_workbench_move(&workbench,
                input.arrow_right ? 1 : input.arrow_left ? -1 : 0,
                input.down ? 1 : input.up ? -1 : 0);
        } else if (!workbench.editing && (input.up || input.down)) {
            (void)ui_workbench_cycle_element(&workbench, input.down ? 1 : -1);
        }
        if (workbench.context != previous_context || input.save) {
            preview_start = SDL_GetTicks();
            memset(event_active, 0, sizeof(event_active));
            event_start[UI_ANIMATION_EVENT_CONTEXT_ENTER] = preview_start;
            event_start[UI_ANIMATION_EVENT_WHILE_VISIBLE] = preview_start;
            event_active[UI_ANIMATION_EVENT_CONTEXT_ENTER] = true;
            event_active[UI_ANIMATION_EVENT_WHILE_VISIBLE] = true;
        } else if (workbench.element_index != previous_element) {
            event_start[UI_ANIMATION_EVENT_FOCUS] = SDL_GetTicks();
            event_active[UI_ANIMATION_EVENT_FOCUS] = true;
        } else if (input.ctrl_confirm) {
            event_start[UI_ANIMATION_EVENT_ACTIVATE] = SDL_GetTicks();
            event_active[UI_ANIMATION_EVENT_ACTIVATE] = true;
        }
        if (workbench.context == previous_context &&
            (input.ctrl_left || input.ctrl_right || input.ctrl_confirm)) {
            ui_workbench_destroy(&outgoing);
            ui_workbench_init(&outgoing);
        }
        double ages[UI_ANIMATION_EVENT_PREVIEW];
        uint64_t now = SDL_GetTicks();
        workbench.preview_elapsed_ms = (double)(now - preview_start);
        workbench.preview_reduced_motion = reduced_motion;
        for (int event = 0; event < UI_ANIMATION_EVENT_PREVIEW; event++)
            ages[event] = event_active[event] ? (double)(now - event_start[event]) : -1.0;
        if (!ui_workbench_runtime_preview_events(preview_canvas, grid, &workbench, &palette,
                ages, reduced_motion)) {
            ui_workbench_destroy(&outgoing);
            ui_workbench_guide_destroy(&guide);
            ui_canvas_destroy(preview_canvas);
            ui_canvas_destroy(footer_canvas);
            ui_workbench_destroy(&workbench);
            return UI_WORKBENCH_RUNTIME_RENDER_FAILED;
        }
        if (outgoing.layout) {
            double elapsed = (double)(SDL_GetTicks() - exit_start);
            if (!ui_workbench_runtime_exit_overlay(preview_canvas, grid, &outgoing,
                                                    elapsed, reduced_motion)) {
                ui_workbench_destroy(&outgoing);
                ui_workbench_guide_destroy(&guide);
                ui_canvas_destroy(preview_canvas);
                ui_canvas_destroy(footer_canvas);
                ui_workbench_destroy(&workbench);
                return UI_WORKBENCH_RUNTIME_RENDER_FAILED;
            }
            if (reduced_motion || elapsed >= ui_theme_motion_duration_ms(UI_THEME_MOTION_MAJOR_EXIT, false)) {
                ui_workbench_destroy(&outgoing);
                ui_workbench_init(&outgoing);
            }
        }
        if (ui_workbench_runtime_interface_size(workbench_scale_percent, workbench.editing, &footer_width,
                                              &footer_height) &&
            (footer_canvas->width != footer_width || footer_canvas->height != footer_height)) {
            UiCanvas *resized = ui_canvas_create(footer_width, footer_height);
            if (!resized) {
                ui_workbench_destroy(&outgoing);
                ui_workbench_guide_destroy(&guide);
                ui_canvas_destroy(preview_canvas);
                ui_canvas_destroy(footer_canvas);
                ui_workbench_destroy(&workbench);
                return UI_WORKBENCH_RUNTIME_RENDER_FAILED;
            }
            ui_canvas_destroy(footer_canvas);
            footer_canvas = resized;
        }
        if (!ui_workbench_runtime_compose_footer_canvas(
                footer_canvas, grid, &workbench, &palette,
                ui_workbench_guide_tooltip(&guide, &workbench),
                workbench_scale_percent, reduced_motion)) {
            ui_workbench_destroy(&outgoing);
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
            ui_workbench_destroy(&outgoing);
            ui_workbench_guide_destroy(&guide);
            ui_canvas_destroy(preview_canvas);
            ui_canvas_destroy(footer_canvas);
            ui_workbench_destroy(&workbench);
            return UI_WORKBENCH_RUNTIME_RENDER_FAILED;
        }
        overlay = selection_layer(&layers, preview_canvas, &palette, &workbench);
        if (!overlay) {
            ui_workbench_destroy(&outgoing);
            ui_workbench_guide_destroy(&guide);
            ui_canvas_destroy(preview_canvas);
            ui_canvas_destroy(footer_canvas);
            ui_workbench_destroy(&workbench);
            return UI_WORKBENCH_RUNTIME_RENDER_FAILED;
        }
        renderer_draw_layers(renderer, grid, &layers, 100);
        ui_canvas_destroy(overlay);
        frame_ms = (double)(SDL_GetPerformanceCounter() - start) * 1000.0 /
                   (double)SDL_GetPerformanceFrequency();
        sleep_ms = timing_sleep_ms(timing_spare_ms(frame_ms, target_ms));
        if (sleep_ms > 0U) SDL_Delay(sleep_ms);
    }
    ui_workbench_destroy(&outgoing);
    ui_workbench_guide_destroy(&guide);
    ui_canvas_destroy(preview_canvas);
    ui_canvas_destroy(footer_canvas);
    ui_workbench_destroy(&workbench);
    return UI_WORKBENCH_RUNTIME_OK;
}