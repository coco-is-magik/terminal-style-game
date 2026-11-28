/** ui_editor_host.h — Shared host input adapter for UI Scene authoring. */
#ifndef UI_EDITOR_HOST_H
#define UI_EDITOR_HOST_H

#include "input.h"
#include "ui_editor_action.h"

typedef struct {
    int viewport_columns;
    int viewport_rows;
} UiEditorHostViewport;

/* `clipboard_text` is the text the caller read from the system clipboard for *this* frame, or NULL when
   it read none. The host deliberately cannot read it itself: it has no platform calls (that is what makes
   it testable on both platforms), so the surface that owns the clipboard supplies the bytes and the host
   decides what a paste means (V1-6 plan, D-E). NULL with the paste key pressed is rule 1 — nothing to
   paste, so nothing happens and nothing is reported. */
UiMenuWorkspaceResult ui_editor_host_apply_input(
    UiMenuWorkspace *workspace,
    const InputState *input,
    const UiEditorHostViewport *viewport,
    const char *clipboard_text,
    bool *handled);

#endif /* UI_EDITOR_HOST_H */