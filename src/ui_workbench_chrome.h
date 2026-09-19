/** ui_workbench_chrome.h — Token-driven workbench chrome contract. */
#ifndef UI_WORKBENCH_CHROME_H
#define UI_WORKBENCH_CHROME_H

#include "grid.h"
#include "ui_app_theme_adapter.h"
#include "ui_canvas.h"
#include "ui_ele.h"
#include "ui_theme.h"
#include "ui_workbench.h"

#include <stdbool.h>

/*
 * Single owner for workbench chrome geometry and token roles. Both the live
 * runtime and the headless oracle must compose through this module so the
 * composed frames stay byte-identical by construction.
 *
 * Baseline geometry (token geometry, 260x160):
 *   footer rows        = height-3, height-2, height-1
 *   preview rows       = 1 .. height-4 (footer never overlaps preview)
 *   left pane columns  = 0 .. 47
 *   right pane columns = width-72 .. width-1
 *   pane inset         = panel_inset_cells (2)
 *   pane border        = border_cells (1)
 */
#define UI_WORKBENCH_CHROME_COLUMNS 260
#define UI_WORKBENCH_CHROME_ROWS 160
#define UI_WORKBENCH_CHROME_FOOTER_ROWS 3
#define UI_WORKBENCH_CHROME_LEFT_PANE_WIDTH 48
#define UI_WORKBENCH_CHROME_RIGHT_PANE_WIDTH 72
#define UI_WORKBENCH_CHROME_PREVIEW_FIRST_ROW 1

typedef enum {
    UI_WORKBENCH_CHROME_ROLE_PANEL = 0,
    UI_WORKBENCH_CHROME_ROLE_ELEVATED,
    UI_WORKBENCH_CHROME_ROLE_ACCENT,
    UI_WORKBENCH_CHROME_ROLE_FOCUS,
    UI_WORKBENCH_CHROME_ROLE_SELECTION_BACKGROUND,
    UI_WORKBENCH_CHROME_ROLE_DISABLED_TEXT,
    UI_WORKBENCH_CHROME_ROLE_DISABLED_BACKGROUND,
    UI_WORKBENCH_CHROME_ROLE_WARNING,
    UI_WORKBENCH_CHROME_ROLE_ERROR,
    UI_WORKBENCH_CHROME_ROLE_SUCCESS,
    UI_WORKBENCH_CHROME_ROLE_DESTRUCTIVE,
    UI_WORKBENCH_CHROME_ROLE_COUNT
} UiWorkbenchChromeRole;

typedef enum {
    UI_WORKBENCH_CHROME_OK = 0,
    UI_WORKBENCH_CHROME_INVALID_ARGUMENT,
    UI_WORKBENCH_CHROME_FAILED
} UiWorkbenchChromeResult;

const char *ui_workbench_chrome_result_string(UiWorkbenchChromeResult result);
bool ui_workbench_chrome_contract_dimensions(int columns, int rows);
bool ui_workbench_chrome_role_color(const UiAppWorkbenchPalette *palette,
                                    UiWorkbenchChromeRole role,
                                    SDL_Color *out_color);
int ui_workbench_chrome_preview_first_row(void);
int ui_workbench_chrome_footer_first_row(int grid_height);
int ui_workbench_chrome_left_pane_width(void);
int ui_workbench_chrome_right_pane_width(void);
bool ui_workbench_chrome_paint_panel(Grid *grid,
                                     const UiAppWorkbenchPalette *palette,
                                     int x, int y, int width, int height,
                                     bool focused);
bool ui_workbench_chrome_footer_rows(Grid *grid,
                                     const UiAppWorkbenchPalette *palette,
                                     const UiWorkbench *workbench,
                                     const char *tooltip_text,
                                     int scale_percent,
                                     bool reduced_motion);
bool ui_workbench_chrome_paint_panes(Grid *grid,
                                     const UiAppWorkbenchPalette *palette,
                                     bool left_focused, bool right_focused);
bool ui_workbench_chrome_pane_content_bounds(int pane, int grid_columns,
                                             int grid_rows, int *out_x,
                                             int *out_y, int *out_width,
                                             int *out_height);
bool ui_workbench_chrome_cell_in_preview(int x, int y, int grid_columns,
                                         int grid_rows);
bool ui_workbench_chrome_scale_preview_rows(UiCanvas *scaled,
                                            const UiCanvas *authored,
                                            int preview_rows, int scale_percent);
bool ui_workbench_chrome_placement_cover_row(UiCanvas *expected,
                                             const UiCanvas *authored,
                                             int preview_space,
                                             int scale_percent,
                                             int footer_first_row);
bool ui_workbench_chrome_blend_matches(const Grid *grid,
                                       const UiCanvas *expected,
                                       int preview_first_row,
                                       int preview_rows);
bool ui_workbench_chrome_chrome_stable(const Grid *grid, int grid_columns,
                                       int grid_rows, int preview_space);
bool ui_workbench_chrome_blend_preview(Grid *grid, const UiCanvas *preview,
                                       int preview_rows, int scale_percent);
bool ui_workbench_chrome_default_left_focused(bool pointer_active,
                                              int pointer_column,
                                              int pointer_row);
bool ui_workbench_chrome_default_right_focused(bool pointer_active,
                                               int pointer_column,
                                               int pointer_row);
int ui_workbench_chrome_preview_rows(int grid_rows, int scale_percent);

#endif /* UI_WORKBENCH_CHROME_H */
