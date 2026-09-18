/** ui_editor_presentation.h — Shared UI Scene editor chrome and preview. */
#ifndef UI_EDITOR_PRESENTATION_H
#define UI_EDITOR_PRESENTATION_H

#include "grid.h"
#include "ui_menu_workspace.h"
#include "ui_render_adapter.h"

typedef struct {
    const UiCanvas *runtime_canvas;
    const char *flow_status;
    const char *target_type;
    const char *target_name;
    unsigned target_id;
    bool test_mode;
    bool target_valid;
} UiEditorPresentationDiagnostics;

bool ui_editor_presentation_render(
    const UiMenuWorkspace *workspace,
    UiMenuWorkspaceResult last_result,
    const AssetRegistry *assets,
    const UiRenderTheme *theme,
    const UiEditorPresentationDiagnostics *diagnostics,
    double now_ms,
    Grid *grid);

#endif /* UI_EDITOR_PRESENTATION_H */