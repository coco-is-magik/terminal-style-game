/** ui_editor_panel.h — Shared editor panel rows, bed and material. */
#ifndef UI_EDITOR_PANEL_H
#define UI_EDITOR_PANEL_H

#include "grid.h"
#include "ui_app_theme_adapter.h"

#include <stdbool.h>
#include <stddef.h>

#define UI_EDITOR_PANEL_MAX_ROWS 64U
#define UI_EDITOR_PANEL_ROW_STEP 2
#define UI_EDITOR_PANEL_PAD 2
#define UI_EDITOR_PANEL_COLUMN_GAP 2
#define UI_EDITOR_PANEL_MARGIN 2

typedef struct {
    char text[80];   /* the label: what the entry is              */
    char detail[64]; /* its value and the keys that act on it     */
    bool focused;    /* takes the focus perimeter and its chase   */
    bool strong;     /* a heading: bright, but not a control      */
    bool note;       /* a notice or an error: the bright role     */
} UiEditorPanelRow;

/* Collect one line into the panel block, classified by the role it was drawn with:
   focus is the live selection, warning is a note, primary text is a heading and
   anything else is an ordinary entry. A leading "> " marker is dropped because the
   panel draws its own perimeter. */
void ui_editor_panel_collect(UiEditorPanelRow *rows, size_t *count, const char *text,
                             SDL_Color colour, const UiAppWorkbenchPalette *palette);

/* Measure the label column and the content-fitted panel width. */
void ui_editor_panel_measure(const UiEditorPanelRow *rows, size_t count,
                             const char *title, const char *subtitle,
                             const char *hint, const char *note, int max_width,
                             int *out_label_columns, int *out_width);

/* The panel height the renderer will use at this corner, after the two-row rhythm
   has been closed up as far as the display requires. */
int ui_editor_panel_planned_height(size_t count, bool has_subtitle, bool has_note,
                                   int y, int grid_height);

/* The first row of a window that keeps `selected` visible. Lists longer than the
   panel's row buffer use this instead of silently dropping their tail. */
size_t ui_editor_panel_window_start(size_t total, size_t selected, size_t visible);

/* Draw the panel and return the first row clear below it (its bed, the one cell
   the material may splash past it, and one spare). */
int ui_editor_panel_render(Grid *grid, int x, int y, const char *title,
                           const char *subtitle, const UiEditorPanelRow *rows,
                           size_t count, const char *hint, const char *note,
                           double now_ms, bool reduced_motion,
                           const UiAppWorkbenchPalette *palette);

/* The same panel without the moving material, for secondary panes that sit beside
   a primary panel. */
int ui_editor_panel_render_plain(Grid *grid, int x, int y, const char *title,
                                 const char *subtitle, const UiEditorPanelRow *rows,
                                 size_t count, const char *hint, const char *note,
                                 const UiAppWorkbenchPalette *palette);

/* A plain black bed and one clipped line on it. Used by dialogs and by callers that
   compose their own chrome. */
void ui_editor_panel_bed(Grid *grid, int x, int y, int columns, int rows,
                         const UiAppWorkbenchPalette *palette);
void ui_editor_panel_put(Grid *grid, int x, int y, int columns, const char *text,
                         SDL_Color fg, const UiAppWorkbenchPalette *palette);

#endif /* UI_EDITOR_PANEL_H */
