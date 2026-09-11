/** flow_workspace.h — Headless staged controller for game-flow authoring. */
#ifndef FLOW_WORKSPACE_H
#define FLOW_WORKSPACE_H

#include "flow_document.h"
#include "flow_reference.h"
#include "ui_nested_inspector.h"

#define FLOW_WORKSPACE_HISTORY_CAPACITY 32U

typedef enum {
    FLOW_WORKSPACE_NODES = 0,
    FLOW_WORKSPACE_EDGES,
    FLOW_WORKSPACE_TARGETS,
    FLOW_WORKSPACE_CLOSE_PROMPT
} FlowWorkspaceMode;

typedef enum {
    FLOW_WORKSPACE_CLOSE_SAVE = 0,
    FLOW_WORKSPACE_CLOSE_DISCARD,
    FLOW_WORKSPACE_CLOSE_CANCEL,
    FLOW_WORKSPACE_CLOSE_CHOICE_COUNT
} FlowWorkspaceCloseChoice;

typedef enum {
    FLOW_WORKSPACE_INPUT_PREVIOUS = 0,
    FLOW_WORKSPACE_INPUT_NEXT,
    FLOW_WORKSPACE_INPUT_CONFIRM,
    FLOW_WORKSPACE_INPUT_ESCAPE,
    FLOW_WORKSPACE_INPUT_SAVE,
    FLOW_WORKSPACE_INPUT_UNDO,
    FLOW_WORKSPACE_INPUT_REDO,
    FLOW_WORKSPACE_INPUT_REMOVE
} FlowWorkspaceInput;

typedef enum {
    FLOW_WORKSPACE_OK = 0,
    FLOW_WORKSPACE_INVALID_ARGUMENT,
    FLOW_WORKSPACE_INVALID_DOCUMENT,
    FLOW_WORKSPACE_INACTIVE,
    FLOW_WORKSPACE_NO_ACTION,
    FLOW_WORKSPACE_SAVE_FAILED,
    FLOW_WORKSPACE_MUTATION_FAILED,
    FLOW_WORKSPACE_CATALOG_FAILED
} FlowWorkspaceResult;

typedef struct {
    FlowDocument before;
    FlowDocument after;
} FlowWorkspaceChange;

typedef struct {
    FlowDocument document;
    FlowDocument saved_document;
    FlowWorkspaceChange changes[FLOW_WORKSPACE_HISTORY_CAPACITY];
    size_t change_count;
    size_t change_cursor;
    FlowWorkspaceMode mode;
    FlowWorkspaceCloseChoice close_choice;
    size_t node_index;
    size_t edge_index;
    size_t target_index;
    UiNestedInspectorCursor nested_cursor;
    const FlowReferenceCatalog *catalog;
    bool active;
} FlowWorkspace;

void flow_workspace_init(FlowWorkspace *workspace);
void flow_workspace_clear(FlowWorkspace *workspace);
FlowWorkspaceResult flow_workspace_set_catalog(
    FlowWorkspace *workspace,
    const FlowReferenceCatalog *catalog
);
FlowWorkspaceResult flow_workspace_open_document(FlowWorkspace *workspace,
                                                 const FlowDocument *document);
FlowWorkspaceResult flow_workspace_load(FlowWorkspace *workspace,
                                        const char *path);
FlowWorkspaceResult flow_workspace_save(FlowWorkspace *workspace);
FlowWorkspaceResult flow_workspace_save_as(FlowWorkspace *workspace,
                                            const char *path);
FlowWorkspaceResult flow_workspace_handle_input(FlowWorkspace *workspace,
                                                FlowWorkspaceInput input);
FlowWorkspaceResult flow_workspace_undo(FlowWorkspace *workspace);
FlowWorkspaceResult flow_workspace_redo(FlowWorkspace *workspace);
bool flow_workspace_is_dirty(const FlowWorkspace *workspace);
size_t flow_workspace_outgoing_count(const FlowWorkspace *workspace);
size_t flow_workspace_connection_count(const FlowWorkspace *workspace);
size_t flow_workspace_target_count(const FlowWorkspace *workspace);
const FlowNode *flow_workspace_selected_node(const FlowWorkspace *workspace);
const FlowEdge *flow_workspace_selected_edge(const FlowWorkspace *workspace);
const FlowNode *flow_workspace_selected_target(const FlowWorkspace *workspace);
const char *flow_workspace_selected_port(const FlowWorkspace *workspace);
const FlowReferenceEntry *flow_workspace_selected_asset(
    const FlowWorkspace *workspace
);

#endif /* FLOW_WORKSPACE_H */