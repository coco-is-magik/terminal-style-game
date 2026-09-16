/** ui_theme_demo_runtime.h — Display-backed accepted-palette diagnostic loop. */
#ifndef UI_THEME_DEMO_RUNTIME_H
#define UI_THEME_DEMO_RUNTIME_H

#include "grid.h"
#include "renderer.h"

typedef enum {
    UI_THEME_DEMO_RUNTIME_OK = 0,
    UI_THEME_DEMO_RUNTIME_INVALID_ARGUMENT,
    UI_THEME_DEMO_RUNTIME_OUT_OF_MEMORY,
    UI_THEME_DEMO_RUNTIME_RENDER_FAILED
} UiThemeDemoRuntimeResult;

bool ui_theme_demo_runtime_arguments_valid(const Renderer *renderer,
                                           const Grid *grid, int target_fps);

UiThemeDemoRuntimeResult ui_theme_demo_runtime_run(Renderer *renderer, Grid *grid,
                                                    int target_fps);

#endif