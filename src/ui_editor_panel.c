#include "ui_editor_panel.h"

#include "ui_animation.h"
#include "ui_ele.h"

#include <stdio.h>
#include <string.h>

void ui_editor_panel_collect(UiEditorPanelRow *rows, size_t *count, const char *text,
                             SDL_Color colour, const UiAppWorkbenchPalette *palette) {
    const char *label = text ? text : "";
    const char *split = NULL;
    const char *rest = NULL;
    bool focused = false;
    bool strong = false;
    bool note = false;
    while (*label == ' ') label++;
    if (*label == '>') {
        focused = true;
        label++;
        while (*label == ' ') label++;
    }
    {
        const char *scan = label;
        while (*scan != '\0') {
            if (scan[0] == ' ' && scan[1] == ' ') {
                const char *value = scan + 2;
                while (*value == ' ') value++;
                if (*value != '\0') {
                    split = scan;
                    rest = value;
                }
                break;
            }
            scan++;
        }
    }
    if (palette) {
        if (colour.r == palette->focus.r && colour.g == palette->focus.g &&
            colour.b == palette->focus.b && colour.a == palette->focus.a)
            focused = true;
        else if (colour.r == palette->warning.r && colour.g == palette->warning.g &&
                 colour.b == palette->warning.b && colour.a == palette->warning.a)
            note = true;
        else if (colour.r == palette->primary_text.r &&
                 colour.g == palette->primary_text.g &&
                 colour.b == palette->primary_text.b &&
                 colour.a == palette->primary_text.a)
            strong = true;
    }
    if (!rows || !count || *count >= UI_EDITOR_PANEL_MAX_ROWS) return;
    (void)snprintf(rows[*count].text, sizeof(rows[*count].text), "%.*s",
                   (int)(split ? (size_t)(split - label) : strlen(label)), label);
    (void)snprintf(rows[*count].detail, sizeof(rows[*count].detail), "%s",
                   rest ? rest : "");
    rows[*count].focused = focused;
    rows[*count].strong = strong;
    rows[*count].note = note;
    (*count)++;
}

void ui_editor_panel_measure(const UiEditorPanelRow *rows, size_t count,
                             const char *title, const char *subtitle,
                             const char *hint, const char *note, int max_width,
                             int *out_label_columns, int *out_width) {
    const int pad = UI_EDITOR_PANEL_PAD;
    int label_columns = 0;
    int width;
    size_t i;
    for (i = 0U; i < count; i++)
        if (rows[i].detail[0] && (int)strlen(rows[i].text) > label_columns)
            label_columns = (int)strlen(rows[i].text);
    width = (int)strlen(title ? title : "");
    if (subtitle && (int)strlen(subtitle) > width) width = (int)strlen(subtitle);
    for (i = 0U; i < count; i++) {
        int columns = (int)strlen(rows[i].text);
        if (rows[i].detail[0]) {
            const int spanned = label_columns + UI_EDITOR_PANEL_COLUMN_GAP +
                                (int)strlen(rows[i].detail);
            if (spanned > columns) columns = spanned;
        }
        if (columns > width) width = columns;
    }
    if (hint && (int)strlen(hint) > width) width = (int)strlen(hint);
    if (note && (int)strlen(note) > width) width = (int)strlen(note);
    width += pad * 2 + 2;
    if (width > max_width) width = max_width;
    if (width < 8) width = 8;
    if (out_label_columns) *out_label_columns = label_columns;
    if (out_width) *out_width = width;
}

static int panel_step_for(size_t count, bool has_subtitle, bool has_note,
                          int y, int grid_height) {
    const int listed = (int)count;
    int step = UI_EDITOR_PANEL_ROW_STEP;
    while (step > 1 &&
           y + (2 * UI_EDITOR_PANEL_MARGIN + 3 + (has_subtitle ? 1 : 0) +
                listed * step + (has_note ? 1 : 0)) > grid_height)
        step--;
    return step;
}

int ui_editor_panel_planned_height(size_t count, bool has_subtitle, bool has_note,
                                   int y, int grid_height) {
    const int step = panel_step_for(count, has_subtitle, has_note, y, grid_height);
    return 2 * UI_EDITOR_PANEL_MARGIN + 3 + (has_subtitle ? 1 : 0) +
           (int)count * step + (has_note ? 1 : 0);
}

size_t ui_editor_panel_window_start(size_t total, size_t selected, size_t visible) {
    size_t start;
    if (visible == 0U || total <= visible) return 0U;
    if (selected < visible) return 0U;
    start = selected - (visible - 1U);
    if (start + visible > total) start = total - visible;
    return start;
}

void ui_editor_panel_bed(Grid *grid, int x, int y, int columns, int rows,
                         const UiAppWorkbenchPalette *palette) {
    int row;
    int column;
    if (!grid || !palette || !grid->cells) return;
    for (row = 0; row < rows; row++)
        for (column = 0; column < columns; column++)
            (void)grid_set(grid, x + column, y + row, ' ', palette->primary_text,
                           palette->canvas);
}

/* The panel's material: the shore, bound to the panel's own rectangle. It paints
   only blank cells, so it never covers an entry, focus marker or control. */
static bool panel_material(Grid *grid, int x, int y, int columns, int rows,
                           double now_ms, bool reduced_motion) {
    UiElement container = {0};
    UiElement field = {0};
    UiLayout layout = {0};

    (void)snprintf(container.name, sizeof(container.name), "%s", "editor_menu_panel");
    container.type = UI_ELE_CONTAINER;
    container.layout = (UiElementLayout){x, y, UI_COORD_ABSOLUTE, columns, rows};
    container.visible = 1;
    container.align = UI_ALIGN_LEFT;
    (void)snprintf(container.style, sizeof(container.style), "%s", "plain");
    (void)snprintf(container.transition, sizeof(container.transition), "%s", "none");
    (void)snprintf(container.focus_effect, sizeof(container.focus_effect), "%s", "none");

    (void)snprintf(field.name, sizeof(field.name), "%s", "editor_menu_panel_shore");
    field.type = UI_ELE_ANIMATION;
    field.layout = (UiElementLayout){0, 0, UI_COORD_ABSOLUTE, 0, 0};
    field.extent = UI_EXTENT_BOX;
    field.visible = 1;
    field.z_index = -1;
    field.align = UI_ALIGN_LEFT;
    (void)snprintf(field.preset, sizeof(field.preset), "%s", "living_shore");
    (void)snprintf(field.target, sizeof(field.target), "%s", "editor_menu_panel");
    (void)snprintf(field.trigger, sizeof(field.trigger), "%s", "while_visible");
    (void)snprintf(field.orientation, sizeof(field.orientation), "%s", "radial");
    (void)snprintf(field.style, sizeof(field.style), "%s", "plain");
    (void)snprintf(field.transition, sizeof(field.transition), "%s", "none");
    (void)snprintf(field.focus_effect, sizeof(field.focus_effect), "%s", "none");

    (void)snprintf(layout.name, sizeof(layout.name), "%s", "editor_menu_panel");
    (void)snprintf(layout.transition, sizeof(layout.transition), "%s", "none");
    layout.elements[0] = &container;
    layout.elements[1] = &field;
    layout.element_count = 2;
    return ui_animation_render_layout(&layout, grid, now_ms, reduced_motion, false,
                                      UI_ANIMATION_EVENT_WHILE_VISIBLE);
}

void ui_editor_panel_put(Grid *grid, int x, int y, int columns, const char *text,
                         SDL_Color fg, const UiAppWorkbenchPalette *palette) {
    int column;
    if (!grid || !palette || !text || columns <= 0) return;
    for (column = 0; text[column] != '\0' && column < columns; column++)
        (void)grid_set(grid, x + column, y, (uint8_t)text[column], fg,
                       palette->canvas);
}

static int panel_render_core(Grid *grid, int x, int y, const char *title,
                             const char *subtitle, const UiEditorPanelRow *rows,
                             size_t count, const char *hint, const char *note,
                             double now_ms, bool reduced_motion,
                             const UiAppWorkbenchPalette *palette, bool material) {
    const int pad = UI_EDITOR_PANEL_PAD;
    const int listed = (int)count;
    const bool has_subtitle = subtitle && subtitle[0];
    const bool has_note = note && note[0];
    int label_columns = 0;
    int width = 0;
    int height;
    int title_row;
    int first_row;
    int hint_row;
    int step;
    size_t cursor_row = count;
    size_t i;
    if (!grid || !palette) return y;
    for (i = 0U; i < count; i++)
        if (rows[i].focused) cursor_row = i;
    ui_editor_panel_measure(rows, count, title, subtitle, hint, note,
                            grid->width - x - 1, &label_columns, &width);
    step = panel_step_for(count, has_subtitle, has_note, y, grid->height);
    height = 2 * UI_EDITOR_PANEL_MARGIN + 3 + (has_subtitle ? 1 : 0) +
             listed * step + (has_note ? 1 : 0);
    if (y + height > grid->height) y = grid->height - height;
    if (y < 0) y = 0;
    title_row = y + UI_EDITOR_PANEL_MARGIN;
    first_row = title_row + (has_subtitle ? 2 : 1) + 1;
    hint_row = first_row + listed * step;

    ui_editor_panel_bed(grid, x, y, width, height, palette);
    if (material)
        (void)panel_material(grid, x, y, width, height, now_ms, reduced_motion);
    if (title && title[0])
        ui_editor_panel_put(grid, x + pad, title_row, width - pad * 2, title,
                            palette->primary_text, palette);
    if (has_subtitle)
        ui_editor_panel_put(grid, x + pad, title_row + 1, width - pad * 2, subtitle,
                            palette->secondary_text, palette);
    for (i = 0U; i < count; i++) {
        const UiEditorPanelRow *entry = &rows[i];
        const int row = first_row + (int)i * step;
        const SDL_Color fg = (entry->focused || entry->strong || entry->note)
                                 ? palette->primary_text
                                 : palette->secondary_text;
        int columns = (int)strlen(entry->text);
        ui_editor_panel_put(grid, x + pad, row, width - pad * 2, entry->text, fg,
                            palette);
        if (entry->detail[0]) {
            const int at = x + pad + label_columns + UI_EDITOR_PANEL_COLUMN_GAP;
            ui_editor_panel_put(grid, at, row, x + width - pad - at, entry->detail,
                                fg, palette);
            columns = label_columns + UI_EDITOR_PANEL_COLUMN_GAP +
                      (int)strlen(entry->detail);
        }
        if (entry->focused && step >= UI_EDITOR_PANEL_ROW_STEP) {
            if (i == cursor_row)
                ui_ele_focus_perimeter_draw(grid, x + pad, row, columns + 2, 1,
                                            now_ms, reduced_motion,
                                            palette->primary_text, palette->canvas);
            else
                ui_ele_focus_perimeter_draw_still(grid, x + pad, row, columns + 2, 1,
                                                  palette->primary_text,
                                                  palette->canvas);
        }
    }
    if (hint && hint[0])
        ui_editor_panel_put(grid, x + pad, hint_row, width - pad * 2, hint,
                            palette->secondary_text, palette);
    if (has_note)
        ui_editor_panel_put(grid, x + pad, hint_row + 1, width - pad * 2, note,
                            palette->primary_text, palette);
    return y + height + 1;
}

int ui_editor_panel_render(Grid *grid, int x, int y, const char *title,
                           const char *subtitle, const UiEditorPanelRow *rows,
                           size_t count, const char *hint, const char *note,
                           double now_ms, bool reduced_motion,
                           const UiAppWorkbenchPalette *palette) {
    return panel_render_core(grid, x, y, title, subtitle, rows, count, hint, note,
                             now_ms, reduced_motion, palette, true);
}

int ui_editor_panel_render_plain(Grid *grid, int x, int y, const char *title,
                                 const char *subtitle, const UiEditorPanelRow *rows,
                                 size_t count, const char *hint, const char *note,
                                 const UiAppWorkbenchPalette *palette) {
    return panel_render_core(grid, x, y, title, subtitle, rows, count, hint, note,
                             0.0, true, palette, false);
}
