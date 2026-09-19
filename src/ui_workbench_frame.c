#include "ui_workbench_frame.h"

#include "checked_size.h"
#include "ui_animation.h"
#include "ui_canvas.h"
#include "ui_compositor.h"
#include "ui_ele.h"
#include "ui_preferences.h"
#include "ui_theme.h"
#include "ui_workbench_chrome.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool valid_input(const UiWorkbenchFrameInput *input) {
    return input && input->grid && input->grid->cells && input->workbench &&
           input->workbench->layout && input->palette &&
           ui_workbench_frame_contract_dimensions(input->grid->width,
                                                  input->grid->height) &&
           ui_preferences_is_valid_scale(input->scale_percent) &&
           input->elapsed_ms >= 0.0 && input->elapsed_ms <= 3600000.0 &&
           (!input->pointer_active ||
            (input->pointer_row >= 0 && input->pointer_column >= 0 &&
             input->pointer_row < UI_WORKBENCH_FRAME_CONTRACT_ROWS &&
             input->pointer_column < UI_WORKBENCH_FRAME_CONTRACT_COLUMNS));
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

static void draw_help(Grid *grid, const UiWorkbench *workbench,
                      const UiAppWorkbenchPalette *palette,
                      const char *tooltip_text, int scale_percent,
                      bool reduced_motion) {
    (void)ui_workbench_chrome_footer_rows(grid, palette, workbench,
                                          tooltip_text, scale_percent,
                                          reduced_motion);
}

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

bool ui_workbench_frame_contract_scale_policy(int scale_percent) {
    return ui_preferences_is_valid_scale(scale_percent);
}

static int preview_scaled_rows(int grid_rows, int scale_percent) {
    return ui_workbench_chrome_preview_rows(grid_rows, scale_percent);
}

static bool paint_preview_region(Grid *grid,
                                 const UiAppWorkbenchPalette *palette) {
    int footer_first = ui_workbench_chrome_footer_first_row(grid->height);
    int first = ui_workbench_chrome_preview_first_row();
    int y;
    int x;
    if (!grid || !grid->cells || !palette || footer_first < 0) return false;
    for (y = first; y < footer_first; y++) {
        for (x = 0; x < grid->width; x++)
            grid->cells[(size_t)y * (size_t)grid->width + (size_t)x] =
                (Cell){' ', palette->secondary_text, palette->panel};
    }
    return true;
}

static bool preview_focus_index(UiWorkbench *workbench, Grid *grid,
                                const UiAppWorkbenchPalette *palette,
                                double elapsed_ms, bool reduced_motion) {
    int index;
    int count;
    UiElement *selected;
    if (!workbench || !workbench->layout || !grid || !palette) return false;
    selected = ui_workbench_current_element(workbench);
    if (!selected) return true;
    count = ui_layout_focusable_count(workbench->layout);
    for (index = 0; index < count; index++) {
        if (ui_layout_get_focused(workbench->layout, index) == selected) {
            (void)ui_layout_render_focus_effect(
                workbench->layout, index, grid, elapsed_ms, reduced_motion,
                palette->accent, palette->focus, palette->canvas);
            return true;
        }
    }
    return true;
}

static bool preview_render_into(const UiWorkbench *workbench, Grid *target,
                               const UiAppWorkbenchPalette *palette,
                               double elapsed_ms, bool reduced_motion) {
    UiElement *element;
    int x;
    int y;
    int width;
    int height;
    if (!workbench || !workbench->layout || !target || !target->cells ||
        !palette) return false;
    (void)grid_clear_region_zero(target, 0, 0, target->width, target->height);
    ui_layout_set_focus((UiLayout *)workbench->layout,
                        selected_focus_index(workbench));
    ui_layout_render((UiLayout *)workbench->layout, target,
                     palette->secondary_text, palette->canvas);
    (void)ui_animation_render_layout((UiLayout *)workbench->layout, target,
                                     elapsed_ms, reduced_motion, true,
                                     UI_ANIMATION_EVENT_PREVIEW);
    if (!preview_focus_index((UiWorkbench *)workbench, target, palette,
                             elapsed_ms, reduced_motion)) return false;
    element = ui_workbench_current_element((UiWorkbench *)workbench);
    if (!element) return true;
    if (!ui_ele_absolute_bounds(element, &x, &y, &width, &height) ||
        width <= 0 || height <= 0) return false;
    if (y < 0 || y >= target->height) return false;
    draw_selection(target, palette, element);
    return true;
}

static bool compose_preview(UiWorkbench *workbench, Grid *grid,
                            const UiAppWorkbenchPalette *palette,
                            int scale_percent, double elapsed_ms,
                            bool reduced_motion) {
    UiCanvas *authored = NULL;
    Grid *staging = NULL;
    UiElement *element;
    int preview_rows;
    int footer_first = ui_workbench_chrome_footer_first_row(
        UI_WORKBENCH_FRAME_CONTRACT_ROWS);
    if (!workbench || !workbench->layout || !grid || !grid->cells || !palette ||
        footer_first < 0) return false;
    preview_rows = preview_scaled_rows(UI_WORKBENCH_FRAME_CONTRACT_ROWS,
                                      scale_percent);
    if (preview_rows <= 0 ||
        preview_rows > footer_first - UI_WORKBENCH_CHROME_PREVIEW_FIRST_ROW)
        return false;
    staging = grid_create(UI_WORKBENCH_FRAME_CONTRACT_COLUMNS,
                          footer_first - UI_WORKBENCH_CHROME_PREVIEW_FIRST_ROW);
    if (!staging) return false;
    if (!preview_render_into(workbench, staging, palette, elapsed_ms,
                             reduced_motion)) {
        grid_destroy(staging);
        return false;
    }
    element = ui_workbench_current_element(workbench);
    authored = ui_canvas_create(UI_WORKBENCH_FRAME_CONTRACT_COLUMNS,
                                footer_first -
                                    UI_WORKBENCH_CHROME_PREVIEW_FIRST_ROW);
    if (!authored) {
        grid_destroy(staging);
        return false;
    }
    ui_canvas_copy_grid_region(authored, staging, 0, 0);
    if (!ui_workbench_chrome_blend_preview(grid, authored, preview_rows,
                                           scale_percent)) {
        ui_canvas_destroy(authored);
        grid_destroy(staging);
        return false;
    }
    (void)element;
    ui_canvas_destroy(authored);
    grid_destroy(staging);
    return true;
}

/*FRAME_NORMAL_RUN*/

const char *ui_workbench_frame_result_string(UiWorkbenchFrameResult result) {
    switch (result) {
        case UI_WORKBENCH_FRAME_OK: return "ok";
        case UI_WORKBENCH_FRAME_INVALID_ARGUMENT: return "invalid argument";
        case UI_WORKBENCH_FRAME_COMPOSITION_FAILED: return "composition failed";
        case UI_WORKBENCH_FRAME_CHECKSUM_FAILED: return "checksum failed";
        default: return "unknown result";
    }
}

bool ui_workbench_frame_contract_dimensions(int columns, int rows) {
    return columns == UI_WORKBENCH_FRAME_CONTRACT_COLUMNS &&
           rows == UI_WORKBENCH_FRAME_CONTRACT_ROWS;
}

bool ui_workbench_frame_render(UiWorkbenchFrameInput input) {
    bool left_focused;
    bool right_focused;
    if (!valid_input(&input)) return false;
    if (!grid_clear_region_zero(input.grid, 0, 0,
                                UI_WORKBENCH_FRAME_CONTRACT_COLUMNS,
                                UI_WORKBENCH_FRAME_CONTRACT_ROWS)) return false;
    if (!paint_preview_region(input.grid, input.palette)) return false;
    left_focused = ui_workbench_chrome_default_left_focused(
        input.pointer_active, input.pointer_column, input.pointer_row);
    right_focused = ui_workbench_chrome_default_right_focused(
        input.pointer_active, input.pointer_column, input.pointer_row);
    if (!left_focused && !right_focused) {
        left_focused = true;
    }
    if (input.workbench->editing &&
        !ui_workbench_chrome_paint_panes(input.grid, input.palette, left_focused,
                                         right_focused)) return false;
    if (!compose_preview((UiWorkbench *)input.workbench, input.grid,
                         input.palette, input.scale_percent, input.elapsed_ms,
                         input.reduced_motion)) return false;
    draw_help(input.grid, input.workbench, input.palette, input.tooltip_text,
              input.scale_percent, input.reduced_motion);
    return true;
}

bool ui_workbench_frame_preview_matches(Grid *grid, UiWorkbench *workbench,
                                        const UiAppWorkbenchPalette *palette,
                                        int scale_percent, double elapsed_ms,
                                        bool reduced_motion) {
    const int contract_columns = UI_WORKBENCH_FRAME_CONTRACT_COLUMNS;
    const int contract_rows = UI_WORKBENCH_FRAME_CONTRACT_ROWS;
    const int preview_first = UI_WORKBENCH_CHROME_PREVIEW_FIRST_ROW;
    const int footer_first = contract_rows - UI_WORKBENCH_CHROME_FOOTER_ROWS;
    Grid *staging = NULL;
    UiCanvas *authored = NULL;
    UiCanvas *expected = NULL;
    int preview_space = footer_first - preview_first;
    int preview_rows;
    bool matches = false;
    if (!grid || !grid->cells || !workbench || !workbench->layout || !palette ||
        !ui_workbench_frame_contract_dimensions(grid->width, grid->height) ||
        !ui_preferences_is_valid_scale(scale_percent) || elapsed_ms < 0.0 ||
        elapsed_ms > 3600000.0) return false;
    if (preview_space <= 0 || preview_space > 10000) return false;
    preview_rows = ui_workbench_chrome_preview_rows(contract_rows,
                                                    scale_percent);
    if (preview_rows <= 0 || preview_rows > preview_space) return false;
    staging = grid_create(contract_columns, preview_space);
    if (!staging) return false;
    if (!preview_render_into(workbench, staging, palette, elapsed_ms,
                             reduced_motion)) {
        grid_destroy(staging);
        return false;
    }
    authored = ui_canvas_create(contract_columns, preview_space);
    if (!authored) {
        grid_destroy(staging);
        return false;
    }
    ui_canvas_copy_grid_region(authored, staging, 0, 0);
    expected = ui_canvas_create(contract_columns, preview_space);
    if (!expected) {
        ui_canvas_destroy(authored);
        grid_destroy(staging);
        return false;
    }
    if (!ui_workbench_chrome_scale_preview_rows(expected, authored,
                                                preview_rows,
                                                scale_percent)) {
        ui_canvas_destroy(expected);
        ui_canvas_destroy(authored);
        grid_destroy(staging);
        return false;
    }
    matches = ui_workbench_chrome_blend_matches(grid, expected, preview_first,
                                                preview_rows);
    ui_canvas_destroy(expected);
    ui_canvas_destroy(authored);
    grid_destroy(staging);
    if (!matches) return false;
    return ui_workbench_chrome_chrome_stable(grid, contract_columns,
                                             contract_rows, preview_space);
}

bool ui_workbench_frame_copy_cells(const Grid *grid, Cell *out_cells,
                                    size_t capacity) {
    size_t count;
    if (!grid || !grid->cells || !out_cells || grid->width <= 0 ||
        grid->height <= 0) return false;
    if (!checked_size_2d(grid->width, grid->height, &count) || count == 0U ||
        count > capacity) return false;
    memcpy(out_cells, grid->cells, count * sizeof(*out_cells));
    return true;
}

uint64_t ui_workbench_frame_checksum_fixture(const Cell *cells,
                                              size_t cell_count) {
    uint64_t checksum = 1469598103934665603ULL;
    size_t i;
    if (!cells || cell_count == 0U) return 0U;
    for (i = 0U; i < cell_count; i++) {
        uint64_t word = (uint64_t)cells[i].glyph;
        word = (word << 24U) | (uint64_t)cells[i].fg.r;
        word = (word << 16U) | (uint64_t)cells[i].bg.r;
        word = (word << 8U) | (uint64_t)(
            (unsigned)cells[i].fg.g ^ (unsigned)cells[i].fg.b ^
            (unsigned)cells[i].bg.g ^ (unsigned)cells[i].bg.b);
        checksum ^= word;
        checksum *= 1099511628211ULL;
    }
    return checksum;
}

bool ui_workbench_frame_footer_distinct(const Grid *grid,
                                         bool *out_distinct) {
    SDL_Color status_fg;
    SDL_Color control_fg;
    SDL_Color diagnostic_fg;
    bool have_colors = false;
    int status_length = 0;
    int control_length = 0;
    int diagnostic_length = 0;
    int status_words = 0;
    int control_words = 0;
    int diagnostic_words = 0;
    int x;
    if (!grid || !grid->cells ||
        grid->width != UI_WORKBENCH_FRAME_CONTRACT_COLUMNS ||
        grid->height != UI_WORKBENCH_FRAME_CONTRACT_ROWS || !out_distinct)
        return false;
    for (x = 0; x < 96; x++) {
        const Cell *status = &grid->cells[
            (size_t)(UI_WORKBENCH_FRAME_CONTRACT_ROWS - 3) *
            (size_t)grid->width + (size_t)x];
        const Cell *control = &grid->cells[
            (size_t)(UI_WORKBENCH_FRAME_CONTRACT_ROWS - 2) *
            (size_t)grid->width + (size_t)x];
        const Cell *diagnostic = &grid->cells[
            (size_t)(UI_WORKBENCH_FRAME_CONTRACT_ROWS - 1) *
            (size_t)grid->width + (size_t)x];
        if (status->glyph != ' ') {
            status_length++;
            status_words++;
        }
        if (control->glyph != ' ') {
            control_length++;
            control_words++;
        }
        if (diagnostic->glyph != ' ') {
            diagnostic_length++;
            diagnostic_words++;
        }
        if (x == 1) {
            status_fg = status->fg;
            control_fg = control->fg;
            diagnostic_fg = diagnostic->fg;
            have_colors = true;
        }
    }
    *out_distinct = status_words >= 1 && control_words >= 1 &&
                    diagnostic_words >= 1 &&
                    status_length != control_length &&
                    status_length != diagnostic_length &&
                    control_length != diagnostic_length &&
                    have_colors &&
                    (status_fg.r != control_fg.r ||
                     status_fg.g != control_fg.g ||
                     status_fg.b != control_fg.b ||
                     diagnostic_fg.r != status_fg.r ||
                     diagnostic_fg.g != status_fg.g ||
                     diagnostic_fg.b != status_fg.b);
    return true;
}