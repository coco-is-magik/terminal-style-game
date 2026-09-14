/** flow_workspace_internal.h — Focused workspace-save fault seam. */
#ifndef FLOW_WORKSPACE_INTERNAL_H
#define FLOW_WORKSPACE_INTERNAL_H

#include "flow_document_internal.h"
#include "flow_workspace.h"

FlowWorkspaceResult flow_workspace_internal_save_as(
    FlowWorkspace *workspace, const char *path, FlowDocumentSaveFault fault
);

#endif