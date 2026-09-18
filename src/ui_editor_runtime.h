/** ui_editor_runtime.h — Standalone UI Scene editor display host. */
#ifndef UI_EDITOR_RUNTIME_H
#define UI_EDITOR_RUNTIME_H

#include "renderer.h"

typedef enum {
    UI_EDITOR_RUNTIME_OK = 0,
    UI_EDITOR_RUNTIME_INVALID_ARGUMENT,
    UI_EDITOR_RUNTIME_LOAD_FAILED,
    UI_EDITOR_RUNTIME_RENDER_FAILED
} UiEditorRuntimeResult;

UiEditorRuntimeResult ui_editor_runtime_run(
    Renderer *renderer, Grid *grid, int target_fps, const char *asset_root);

#endif /* UI_EDITOR_RUNTIME_H */