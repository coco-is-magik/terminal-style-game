/** ui_workbench_runtime.h — Dedicated display loop for live application UI tweaking. */
#ifndef UI_WORKBENCH_RUNTIME_H
#define UI_WORKBENCH_RUNTIME_H

#include "grid.h"
#include "renderer.h"

typedef enum {
    UI_WORKBENCH_RUNTIME_OK = 0,
    UI_WORKBENCH_RUNTIME_INVALID_ARGUMENT,
    UI_WORKBENCH_RUNTIME_LOAD_FAILED,
    UI_WORKBENCH_RUNTIME_RENDER_FAILED
} UiWorkbenchRuntimeResult;

UiWorkbenchRuntimeResult ui_workbench_runtime_run(Renderer *renderer, Grid *grid,
                                                   int target_fps);

#endif