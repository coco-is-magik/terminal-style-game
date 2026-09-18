/** ui_editor_host.h — Shared host input adapter for UI Scene authoring. */
#ifndef UI_EDITOR_HOST_H
#define UI_EDITOR_HOST_H

#include "input.h"
#include "ui_editor_action.h"

typedef struct {
    int viewport_columns;
    int viewport_rows;
    int preview_x;
    int preview_y;
} UiEditorHostViewport;

UiMenuWorkspaceResult ui_editor_host_apply_input(
    UiMenuWorkspace *workspace,
    const InputState *input,
    const UiEditorHostViewport *viewport,
    bool *handled);

#endif /* UI_EDITOR_HOST_H */