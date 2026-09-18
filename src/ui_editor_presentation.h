/** ui_editor_presentation.h — Shared UI Scene editor chrome and preview. */
#ifndef UI_EDITOR_PRESENTATION_H
#define UI_EDITOR_PRESENTATION_H

#include "grid.h"
#include "ui_menu_workspace.h"
#include "ui_render_adapter.h"

bool ui_editor_presentation_render(
    const UiMenuWorkspace *workspace,
    UiMenuWorkspaceResult last_result,
    const AssetRegistry *assets,
    const UiRenderTheme *theme,
    double now_ms,
    Grid *grid);

#endif /* UI_EDITOR_PRESENTATION_H */