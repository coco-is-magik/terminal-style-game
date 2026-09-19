/** ui_workbench_runtime.h — Dedicated display loop for live application UI tweaking. */
#ifndef UI_WORKBENCH_RUNTIME_H
#define UI_WORKBENCH_RUNTIME_H

#include "grid.h"
#include "renderer.h"
#include "ui_app_theme_adapter.h"
#include "ui_canvas.h"
#include "ui_workbench.h"

typedef enum {
    UI_WORKBENCH_RUNTIME_OK = 0,
    UI_WORKBENCH_RUNTIME_INVALID_ARGUMENT,
    UI_WORKBENCH_RUNTIME_LOAD_FAILED,
    UI_WORKBENCH_RUNTIME_RENDER_FAILED
} UiWorkbenchRuntimeResult;

UiWorkbenchRuntimeResult ui_workbench_runtime_run(Renderer *renderer, Grid *grid,
                                                   int target_fps);
bool ui_workbench_runtime_compose_footer_canvas(UiCanvas *canvas, Grid *grid,
                                                const UiWorkbench *workbench,
                                                const UiAppWorkbenchPalette *palette,
                                                const char *tooltip_text,
                                                int scale_percent,
                                                bool reduced_motion);
bool ui_workbench_runtime_build_layers(UiLayerList *layers,
                                       const UiCanvas *preview_canvas,
                                       const UiCanvas *footer_canvas,
                                       int logical_w, int logical_h,
                                       int authored_scale_percent,
                                       int workbench_scale_percent);
int ui_workbench_runtime_step_scale(int scale_percent, int direction);

#endif