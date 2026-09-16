/** ui_motion_demo_runtime.h — Display adapter for the isolated motion specimen. */
#ifndef UI_MOTION_DEMO_RUNTIME_H
#define UI_MOTION_DEMO_RUNTIME_H

#include "grid.h"
#include "renderer.h"

typedef enum {
    UI_MOTION_DEMO_RUNTIME_OK = 0,
    UI_MOTION_DEMO_RUNTIME_INVALID_ARGUMENT,
    UI_MOTION_DEMO_RUNTIME_OUT_OF_MEMORY,
    UI_MOTION_DEMO_RUNTIME_RENDER_FAILED
} UiMotionDemoRuntimeResult;

bool ui_motion_demo_runtime_arguments_valid(const Renderer *renderer,
                                             const Grid *grid, int target_fps);
UiMotionDemoRuntimeResult ui_motion_demo_runtime_run(Renderer *renderer, Grid *grid,
                                                      int target_fps);

#endif