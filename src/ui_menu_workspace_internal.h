/** ui_menu_workspace_internal.h — Focused workspace-save fault seam. */
#ifndef UI_MENU_WORKSPACE_INTERNAL_H
#define UI_MENU_WORKSPACE_INTERNAL_H

#include "ui_document_internal.h"
#include "ui_menu_workspace.h"
#include "platform_fs_internal.h"

UiMenuWorkspaceResult ui_menu_workspace_internal_save(
    UiMenuWorkspace *workspace, UiDocumentSaveFault fault
);
UiMenuWorkspaceResult ui_menu_workspace_internal_confirm(
    UiMenuWorkspace *workspace, UiDocumentSaveFault fault
);
UiMenuWorkspaceResult ui_menu_workspace_internal_create_document(
    UiMenuWorkspace *workspace, PlatformFsFault fault
);

#endif