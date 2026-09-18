/** ui_workbench_frame.h — Headless application UI workbench frame contract. */
#ifndef UI_WORKBENCH_FRAME_H
#define UI_WORKBENCH_FRAME_H

#include "grid.h"
#include "ui_app_theme_adapter.h"
#include "ui_ele.h"
#include "ui_workbench.h"
#include "ui_workbench_store.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * The application UI editor frame contract is fixed at
 * editor_baseline 260 columns x 160 rows. An unsigned 64-bit FNV-1a checksum
 * summarizes a fully composed frame. Identical renderer, Grid, UiWorkbench,
 * and UiAppWorkbenchPalette snapshot semantics must reproduce the same
 * checksum. The oracle is deliberately display-free so composed frames can be
 * reviewed, diffed, and regression-gated without SDL or a monitor.
 */
#define UI_WORKBENCH_FRAME_CONTRACT_COLUMNS 260
#define UI_WORKBENCH_FRAME_CONTRACT_ROWS 160

typedef enum {
    UI_WORKBENCH_FRAME_OK = 0,
    UI_WORKBENCH_FRAME_INVALID_ARGUMENT,
    UI_WORKBENCH_FRAME_COMPOSITION_FAILED,
    UI_WORKBENCH_FRAME_CHECKSUM_FAILED
} UiWorkbenchFrameResult;

typedef struct {
    Grid *grid;
    const UiWorkbench *workbench;
    const UiAppWorkbenchPalette *palette;
    int scale_percent;
    double elapsed_ms;
    bool reduced_motion;
    int pointer_row;
    int pointer_column;
    bool pointer_active;
} UiWorkbenchFrameInput;

const char *ui_workbench_frame_result_string(UiWorkbenchFrameResult result);
bool ui_workbench_frame_contract_dimensions(int columns, int rows);
bool ui_workbench_frame_contract_scale_policy(int scale_percent);
bool ui_workbench_frame_preview_matches(Grid *grid,
                                        const UiWorkbench *workbench,
                                        const UiAppWorkbenchPalette *palette,
                                        int scale_percent, double elapsed_ms,
                                        bool reduced_motion);
bool ui_workbench_frame_render(UiWorkbenchFrameInput input);
bool ui_workbench_frame_copy_cells(const Grid *grid, Cell *out_cells,
                                    size_t capacity);
uint64_t ui_workbench_frame_checksum_fixture(const Cell *cells,
                                              size_t cell_count);
bool ui_workbench_frame_footer_distinct(const Grid *grid,
                                         bool *out_distinct);

#endif /* UI_WORKBENCH_FRAME_H */
