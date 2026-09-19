#include "ui_workbench_chrome.h"

#include "checked_size.h"
#include "ui_canvas.h"
#include "ui_ele.h"
#include "ui_preferences.h"
#include "ui_theme.h"

#include <stdio.h>

static bool pane_geometry(const UiThemeGeometry *geometry, int pane,
                          int grid_columns, int grid_rows, int *out_x,
                          int *out_y, int *out_width, int *out_height) {
    int inset;
    int pane_height;
    if (!geometry || !out_x || !out_y || !out_width || !out_height) return false;
    if (grid_columns != UI_WORKBENCH_CHROME_COLUMNS ||
        grid_rows != UI_WORKBENCH_CHROME_ROWS) return false;
    inset = geometry->panel_inset_cells;
    if (inset < 1) return false;
    pane_height = (grid_rows - UI_WORKBENCH_CHROME_FOOTER_ROWS -
                   UI_WORKBENCH_CHROME_PREVIEW_FIRST_ROW) -
                  2 * inset;
    if (pane_height <= 2) return false;
    if (pane == 0) {
        *out_x = inset;
        *out_width = UI_WORKBENCH_CHROME_LEFT_PANE_WIDTH;
    } else if (pane == 1) {
        *out_x = grid_columns - inset - UI_WORKBENCH_CHROME_RIGHT_PANE_WIDTH;
        *out_width = UI_WORKBENCH_CHROME_RIGHT_PANE_WIDTH;
    } else {
        return false;
    }
    *out_y = UI_WORKBENCH_CHROME_PREVIEW_FIRST_ROW + inset;
    *out_height = pane_height;
    return true;
}

bool ui_workbench_chrome_paint_panes(Grid *grid,
                                     const UiAppWorkbenchPalette *palette,
                                     bool left_focused, bool right_focused) {
    int pane;
    bool focus[2];
    int pane_x[2] = {0, 0};
    int pane_y[2] = {0, 0};
    int pane_width[2] = {0, 0};
    int pane_height[2] = {0, 0};
    if (!grid || !grid->cells || !palette ||
        grid->width != UI_WORKBENCH_CHROME_COLUMNS ||
        grid->height != UI_WORKBENCH_CHROME_ROWS) return false;
    if (!left_focused && !right_focused) return false;
    focus[0] = left_focused;
    focus[1] = right_focused;
    if (!ui_workbench_chrome_pane_content_bounds(0, grid->width, grid->height,
                                                 &pane_x[0], &pane_y[0],
                                                 &pane_width[0],
                                                 &pane_height[0])) return false;
    if (!ui_workbench_chrome_pane_content_bounds(1, grid->width, grid->height,
                                                 &pane_x[1], &pane_y[1],
                                                 &pane_width[1],
                                                 &pane_height[1])) return false;
    for (pane = 0; pane < 2; pane++) {
        if (!ui_workbench_chrome_paint_panel(grid, palette, pane_x[pane],
                                             pane_y[pane], pane_width[pane],
                                             pane_height[pane],
                                             focus[pane])) return false;
    }
    return true;
}

bool ui_workbench_chrome_pane_content_bounds(int pane, int grid_columns,
                                             int grid_rows, int *out_x,
                                             int *out_y, int *out_width,
                                             int *out_height) {
    return pane_geometry(&ui_theme_provisional_tokens()->geometry, pane,
                         grid_columns, grid_rows, out_x, out_y, out_width,
                         out_height);
}

bool ui_workbench_chrome_cell_in_preview(int x, int y, int grid_columns,
                                         int grid_rows) {
    int left_x;
    int left_y;
    int left_width;
    int left_height;
    int right_x;
    int right_y;
    int right_width;
    int right_height;
    if (x < 0 || y < 0 || grid_columns != UI_WORKBENCH_CHROME_COLUMNS ||
        grid_rows != UI_WORKBENCH_CHROME_ROWS) return false;
    if (y < UI_WORKBENCH_CHROME_PREVIEW_FIRST_ROW ||
        y >= grid_rows - UI_WORKBENCH_CHROME_FOOTER_ROWS) return false;
    if (!pane_geometry(&ui_theme_provisional_tokens()->geometry, 0,
                       grid_columns, grid_rows, &left_x, &left_y, &left_width,
                       &left_height)) return false;
    if (!pane_geometry(&ui_theme_provisional_tokens()->geometry, 1,
                       grid_columns, grid_rows, &right_x, &right_y,
                       &right_width, &right_height)) return false;
    if (x >= left_x && x < left_x + left_width && y >= left_y &&
        y < left_y + left_height) return false;
    if (x >= right_x && x < right_x + right_width && y >= right_y &&
        y < right_y + right_height) return false;
    return true;
}

static bool pointer_in_pane(int pane, bool pointer_active, int pointer_column,
                            int pointer_row) {
    int x;
    int y;
    int width;
    int height;
    if (!pointer_active) return false;
    if (!pane_geometry(&ui_theme_provisional_tokens()->geometry, pane,
                       UI_WORKBENCH_CHROME_COLUMNS, UI_WORKBENCH_CHROME_ROWS,
                       &x, &y, &width, &height)) return false;
    return pointer_column >= x && pointer_column < x + width &&
           pointer_row >= y && pointer_row < y + height;
}

bool ui_workbench_chrome_default_left_focused(bool pointer_active,
                                              int pointer_column,
                                              int pointer_row) {
    if (pointer_in_pane(0, pointer_active, pointer_column, pointer_row))
        return true;
    if (pointer_in_pane(1, pointer_active, pointer_column, pointer_row))
        return false;
    return true;
}

bool ui_workbench_chrome_default_right_focused(bool pointer_active,
                                               int pointer_column,
                                               int pointer_row) {
    if (pointer_in_pane(1, pointer_active, pointer_column, pointer_row))
        return true;
    if (pointer_in_pane(0, pointer_active, pointer_column, pointer_row))
        return false;
    return false;
}

int ui_workbench_chrome_preview_rows(int grid_rows, int scale_percent) {
    const UiThemeGeometry *geometry = &ui_theme_provisional_tokens()->geometry;
    int preview_space;
    if (grid_rows != UI_WORKBENCH_CHROME_ROWS) return 0;
    if (!ui_preferences_is_valid_scale(scale_percent)) return 0;
    preview_space = grid_rows - UI_WORKBENCH_CHROME_FOOTER_ROWS -
                    UI_WORKBENCH_CHROME_PREVIEW_FIRST_ROW;
    if (preview_space <= 0 || preview_space > 10000) return 0;
    if (geometry->panel_inset_cells < 1 ||
        geometry->group_gap_cells < 0) return 0;
    return (preview_space * 100) / scale_percent;
}

bool ui_workbench_chrome_scale_preview_rows(UiCanvas *scaled,
                                            const UiCanvas *authored,
                                            int preview_rows, int scale_percent) {
    int source;
    int row;
    int column;
    if (!scaled || !scaled->cells || !scaled->touched || !authored ||
        !authored->cells || !authored->touched || preview_rows <= 0 ||
        scale_percent < 100 || preview_rows > authored->height ||
        scaled->width != authored->width || scaled->height <= 0 ||
        authored->height <= 0 || scaled->width <= 0 ||
        authored->width <= 0 || preview_rows > scaled->height) return false;
    source = (authored->height * 100) / scale_percent;
    if (source <= 0 || source > authored->height) return false;
    ui_canvas_clear(scaled);
    for (row = 0; row < preview_rows; row++) {
        int sample = row < source ? row : source - 1;
        if (sample < 0) sample = 0;
        if (sample >= authored->height) sample = authored->height - 1;
        for (column = 0; column < scaled->width; column++) {
            if (ui_canvas_is_touched(authored, column, sample)) {
                const Cell *picked =
                    &authored->cells[(size_t)sample *
                                         (size_t)authored->width +
                                     (size_t)column];
                (void)ui_canvas_set(scaled, column, row, picked->glyph,
                                    picked->fg, picked->bg);
            }
        }
    }
    return true;
}

bool ui_workbench_chrome_placement_cover_row(UiCanvas *expected,
                                             const UiCanvas *authored,
                                             int preview_space,
                                             int scale_percent,
                                             int footer_first_row) {
    int source;
    int rows;
    int row;
    int column;
    if (!expected || !expected->cells || !expected->touched || !authored ||
        !authored->cells || !authored->touched || preview_space <= 0 ||
        scale_percent < 100 ||
        footer_first_row <= UI_WORKBENCH_CHROME_PREVIEW_FIRST_ROW ||
        preview_space != authored->height ||
        expected->width != authored->width || expected->height <= 0 ||
        authored->height <= 0 || expected->width <= 0 ||
        authored->width <= 0) return false;
    rows = ui_workbench_chrome_preview_rows(
        footer_first_row + UI_WORKBENCH_CHROME_FOOTER_ROWS, scale_percent);
    if (rows <= 0 || rows > authored->height || rows > expected->height)
        return false;
    source = (authored->height * 100) / scale_percent;
    if (source <= 0 || source > authored->height) return false;
    ui_canvas_clear(expected);
    for (row = 0; row < rows; row++) {
        int sample = row < source ? row : source - 1;
        if (sample < 0) sample = 0;
        if (sample >= authored->height) sample = authored->height - 1;
        for (column = 0; column < expected->width; column++) {
            if (ui_canvas_is_touched(authored, column, sample)) {
                const Cell *picked =
                    &authored->cells[(size_t)sample *
                                         (size_t)authored->width +
                                     (size_t)column];
                (void)ui_canvas_set(expected, column, row, picked->glyph,
                                    picked->fg, picked->bg);
            }
        }
    }
    return true;
}

bool ui_workbench_chrome_chrome_stable(const Grid *grid, int grid_columns,
                                       int grid_rows, int preview_space) {
    int footer_first;
    int row0;
    int row1;
    int row2;
    int stable_x;
    int stable_y;
    int focus_r;
    int focus_g;
    int focus_b;
    (void)preview_space;
    if (!grid || !grid->cells || grid_columns != UI_WORKBENCH_CHROME_COLUMNS ||
        grid_rows != UI_WORKBENCH_CHROME_ROWS) return false;
    footer_first = ui_workbench_chrome_footer_first_row(grid_rows);
    if (footer_first < 0) return false;
    for (stable_y = 0; stable_y < UI_WORKBENCH_CHROME_PREVIEW_FIRST_ROW;
         stable_y++) {
        for (stable_x = 0; stable_x < grid_columns; stable_x++) {
            if (grid->cells[(size_t)stable_y * (size_t)grid_columns +
                            (size_t)stable_x]
                    .glyph != 0) return false;
        }
    }
    row0 = (int)grid->cells[(size_t)footer_first * (size_t)grid_columns + 1U]
               .glyph;
    row1 = (int)grid->cells[(size_t)(footer_first + 1) *
                                (size_t)grid_columns +
                            1U]
               .glyph;
    row2 = (int)grid->cells[(size_t)(footer_first + 2) *
                                (size_t)grid_columns +
                            1U]
               .glyph;
    if (row0 != 'U' && row0 != 'A') return false;
    if (row1 == 0 || row2 == 0) return false;
    focus_r =
        (int)grid->cells[(size_t)footer_first * (size_t)grid_columns + 1U].fg.r;
    focus_g =
        (int)grid->cells[(size_t)footer_first * (size_t)grid_columns + 1U].fg.g;
    focus_b =
        (int)grid->cells[(size_t)footer_first * (size_t)grid_columns + 1U].fg.b;
    if (focus_r == 0 && focus_g == 0 && focus_b == 0) return false;
    return true;
}

bool ui_workbench_chrome_blend_matches(const Grid *grid,
                                       const UiCanvas *expected,
                                       int preview_first_row,
                                       int preview_rows) {
    int first = UI_WORKBENCH_CHROME_PREVIEW_FIRST_ROW;
    int row;
    int column;
    if (!grid || !grid->cells ||
        grid->width != UI_WORKBENCH_CHROME_COLUMNS ||
        grid->height != UI_WORKBENCH_CHROME_ROWS || !expected ||
        !expected->cells || !expected->touched ||
        preview_first_row != first || preview_rows <= 0 ||
        preview_rows > grid->height - UI_WORKBENCH_CHROME_FOOTER_ROWS -
                            first ||
        expected->width != grid->width || expected->height < preview_rows)
        return false;
    for (row = 0; row < preview_rows; row++) {
        for (column = 0; column < grid->width; column++) {
            const Cell *wanted =
                &expected->cells[(size_t)row * (size_t)expected->width +
                                 (size_t)column];
            const Cell *composed = &grid->cells[(size_t)(first + row) *
                                                    (size_t)grid->width +
                                                (size_t)column];
            if (wanted->glyph != composed->glyph ||
                wanted->fg.r != composed->fg.r ||
                wanted->fg.g != composed->fg.g ||
                wanted->fg.b != composed->fg.b ||
                wanted->fg.a != composed->fg.a ||
                wanted->bg.r != composed->bg.r ||
                wanted->bg.g != composed->bg.g ||
                wanted->bg.b != composed->bg.b ||
                wanted->bg.a != composed->bg.a) return false;
        }
    }
    return true;
}

bool ui_workbench_chrome_blend_preview(Grid *grid, const UiCanvas *preview,
                                       int preview_rows, int scale_percent) {
    const UiAppWorkbenchPalette *owned = NULL;
    UiAppWorkbenchPalette snapshot;
    int first = UI_WORKBENCH_CHROME_PREVIEW_FIRST_ROW;
    int y;
    int x;
    if (!grid || !grid->cells || grid->width != UI_WORKBENCH_CHROME_COLUMNS ||
        grid->height != UI_WORKBENCH_CHROME_ROWS || !preview || !preview->cells ||
        !ui_preferences_is_valid_scale(scale_percent) || preview_rows <= 0 ||
        preview_rows > grid->height - UI_WORKBENCH_CHROME_FOOTER_ROWS - first)
        return false;
    if (!ui_app_theme_workbench_palette(&snapshot)) return false;
    owned = &snapshot;
    for (y = 0; y < preview_rows; y++) {
        int cell_row = first + y;
        for (x = 0; x < grid->width; x++) {
            const Cell *authored =
                &preview->cells[(size_t)y * (size_t)grid->width + (size_t)x];
            Cell composed;
            if (!ui_workbench_chrome_cell_in_preview(x, cell_row, grid->width,
                                                     grid->height)) continue;
            composed = *authored;
            if (composed.glyph == 0) {
                composed.fg = owned->secondary_text;
                composed.bg = owned->panel;
            }
            grid->cells[(size_t)cell_row * (size_t)grid->width + (size_t)x] =
                composed;
        }
    }
    return true;
}

static bool valid_panel(const Grid *grid,
                        const UiAppWorkbenchPalette *palette, int x, int y,
                        int width, int height) {
    return grid && grid->cells && palette && x >= 0 && y >= 0 && width > 0 &&
           height > 0 && x + width <= grid->width && y + height <= grid->height;
}

static void paint_cell(Grid *grid, int x, int y, uint8_t glyph,
                       SDL_Color foreground, SDL_Color background) {
    if (!grid || !grid->cells || x < 0 || y < 0 || x >= grid->width ||
        y >= grid->height) return;
    grid->cells[(size_t)y * (size_t)grid->width + (size_t)x] =
        (Cell){glyph, foreground, background};
}

const char *ui_workbench_chrome_result_string(UiWorkbenchChromeResult result) {
    switch (result) {
        case UI_WORKBENCH_CHROME_OK: return "ok";
        case UI_WORKBENCH_CHROME_INVALID_ARGUMENT: return "invalid argument";
        case UI_WORKBENCH_CHROME_FAILED: return "composition failed";
        default: return "unknown result";
    }
}

bool ui_workbench_chrome_contract_dimensions(int columns, int rows) {
    return columns == UI_WORKBENCH_CHROME_COLUMNS &&
           rows == UI_WORKBENCH_CHROME_ROWS;
}

bool ui_workbench_chrome_role_color(const UiAppWorkbenchPalette *palette,
                                    UiWorkbenchChromeRole role,
                                    SDL_Color *out_color) {
    if (!palette || !out_color || role < 0 ||
        role >= UI_WORKBENCH_CHROME_ROLE_COUNT) return false;
    switch (role) {
        case UI_WORKBENCH_CHROME_ROLE_PANEL: *out_color = palette->panel; break;
        case UI_WORKBENCH_CHROME_ROLE_ELEVATED:
            *out_color = palette->elevated;
            break;
        case UI_WORKBENCH_CHROME_ROLE_ACCENT: *out_color = palette->accent; break;
        case UI_WORKBENCH_CHROME_ROLE_FOCUS: *out_color = palette->focus; break;
        case UI_WORKBENCH_CHROME_ROLE_SELECTION_BACKGROUND:
            *out_color = palette->selection_background;
            break;
        case UI_WORKBENCH_CHROME_ROLE_DISABLED_TEXT:
            *out_color = palette->disabled_text;
            break;
        case UI_WORKBENCH_CHROME_ROLE_DISABLED_BACKGROUND:
            *out_color = palette->disabled_background;
            break;
        case UI_WORKBENCH_CHROME_ROLE_WARNING: *out_color = palette->warning; break;
        case UI_WORKBENCH_CHROME_ROLE_ERROR: *out_color = palette->error; break;
        case UI_WORKBENCH_CHROME_ROLE_SUCCESS:
            *out_color = palette->success;
            break;
        case UI_WORKBENCH_CHROME_ROLE_DESTRUCTIVE:
            *out_color = palette->destructive;
            break;
        default: return false;
    }
    return true;
}

int ui_workbench_chrome_preview_first_row(void) {
    return UI_WORKBENCH_CHROME_PREVIEW_FIRST_ROW;
}

int ui_workbench_chrome_footer_first_row(int grid_height) {
    if (grid_height != UI_WORKBENCH_CHROME_ROWS) return -1;
    return grid_height - UI_WORKBENCH_CHROME_FOOTER_ROWS;
}

int ui_workbench_chrome_left_pane_width(void) {
    return UI_WORKBENCH_CHROME_LEFT_PANE_WIDTH;
}

int ui_workbench_chrome_right_pane_width(void) {
    return UI_WORKBENCH_CHROME_RIGHT_PANE_WIDTH;
}

bool ui_workbench_chrome_paint_panel(Grid *grid,
                                     const UiAppWorkbenchPalette *palette,
                                     int x, int y, int width, int height,
                                     bool focused) {
    SDL_Color surface;
    SDL_Color edge;
    SDL_Color text;
    int row;
    int column;
    int inset;
    if (!valid_panel(grid, palette, x, y, width, height)) return false;
    if (!ui_workbench_chrome_role_color(
            palette, focused ? UI_WORKBENCH_CHROME_ROLE_ELEVATED
                             : UI_WORKBENCH_CHROME_ROLE_PANEL,
            &surface)) return false;
    if (!ui_workbench_chrome_role_color(
            palette, UI_WORKBENCH_CHROME_ROLE_ACCENT, &edge)) return false;
    text = focused ? palette->focus : palette->secondary_text;
    for (row = 0; row < height; row++) {
        for (column = 0; column < width; column++)
            paint_cell(grid, x + column, y + row, ' ', palette->secondary_text,
                       surface);
    }
    inset = ui_theme_provisional_tokens()->geometry.border_cells;
    if (inset < 1) inset = 1;
    if (inset >= width || inset >= height) return false;
    for (column = 0; column < width; column++) {
        paint_cell(grid, x + column, y, '-', text, surface);
        paint_cell(grid, x + column, y + height - 1, '-', text, surface);
    }
    for (row = 0; row < height; row++) {
        paint_cell(grid, x, y + row, '|', text, surface);
        paint_cell(grid, x + width - 1, y + row, '|', text, surface);
    }
    paint_cell(grid, x, y, '+', focused ? edge : text, surface);
    paint_cell(grid, x + width - 1, y, '+', focused ? edge : text, surface);
    paint_cell(grid, x, y + height - 1, '+', focused ? edge : text, surface);
    paint_cell(grid, x + width - 1, y + height - 1, '+',
               focused ? edge : text, surface);
    return true;
}

bool ui_workbench_chrome_footer_rows(Grid *grid,
                                     const UiAppWorkbenchPalette *palette,
                                     const UiWorkbench *workbench,
                                     const char *tooltip_text,
                                     int scale_percent,
                                     bool reduced_motion) {
    char line[256];
    char status[224];
    UiElement *element;
    const char *add_source;
    int footer_first;
    if (!grid || !grid->cells || !palette || !workbench ||
        grid->width != UI_WORKBENCH_CHROME_COLUMNS ||
        grid->height != UI_WORKBENCH_CHROME_ROWS ||
        !ui_preferences_is_valid_scale(scale_percent)) return false;
    footer_first = ui_workbench_chrome_footer_first_row(grid->height);
    if (footer_first < 0) return false;
    element = ui_workbench_current_element((UiWorkbench *)workbench);
    add_source = ui_workbench_add_source_name(workbench);
    if (workbench->mode == UI_WORKBENCH_MODE_ADD) {
        (void)snprintf(line, sizeof(line), "ADD EXISTING UNIT | %s",
                       add_source ? add_source : "none");
        grid_print(grid, 1, footer_first, line, palette->primary_text,
                   palette->canvas);
        grid_print(grid, 1, footer_first + 1,
                   "Up/Down choose | Enter clone/add | Esc cancel",
                   palette->secondary_text, palette->canvas);
        return true;
    }
    (void)snprintf(line, sizeof(line),
                   "UI WORKBENCH | %s | %s | %s=%s | scale=%d%% | reduced=%s",
                   workbench->layout ? workbench->layout->name : "none",
                   element ? element->name : "none",
                   ui_workbench_property_name(workbench->property),
                   ui_workbench_current_value(workbench), scale_percent,
                   reduced_motion ? "on" : "off");
    grid_print(grid, 1, footer_first, line, palette->primary_text,
               palette->canvas);
    grid_print(grid, 1, footer_first + 1,
        "Up/Down element | Enter move | arrows move | Tab property | [ or ] value | Ctrl+N add | Backspace remove | Ctrl+Left/Right layout | Esc exit",
        palette->secondary_text, palette->canvas);
    if (element && strcmp(element->focus_effect, "input_hold_short") == 0) {
        grid_print(grid, 1, footer_first + 2,
                   "Preview metadata only: input_hold_short has no production execution",
                   palette->secondary_text, palette->canvas);
    } else {
        (void)snprintf(status, sizeof(status), "%s", workbench->status);
        if (tooltip_text && tooltip_text[0] != '\0') {
            size_t used = strlen(status);
            size_t room = sizeof(status) > used + 3U
                ? sizeof(status) - used - 3U
                : 0U;
            if (room > 0U) {
                size_t take = strlen(tooltip_text);
                if (take > room) take = room;
                (void)snprintf(status + used, sizeof(status) - used, " | %.*s",
                               (int)take, tooltip_text);
            }
        }
        grid_print(grid, 1, footer_first + 2, status,
                   palette->secondary_text, palette->canvas);
    }
    return true;
}