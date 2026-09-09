#include "flow_workspace.h"

#include <string.h>

void flow_workspace_init(FlowWorkspace *workspace) {
    if (!workspace) return;
    memset(workspace, 0, sizeof(*workspace));
    flow_document_init(&workspace->document);
    workspace->saved_document = workspace->document;
}

static bool valid_workspace_document(const FlowDocument *document) {
    return document && flow_document_validate(document) == FLOW_DOCUMENT_OK;
}

FlowWorkspaceResult flow_workspace_open_document(FlowWorkspace *workspace,
                                                 const FlowDocument *document) {
    FlowWorkspace candidate;
    if (!workspace || !document) return FLOW_WORKSPACE_INVALID_ARGUMENT;
    if (!valid_workspace_document(document)) return FLOW_WORKSPACE_INVALID_DOCUMENT;
    flow_workspace_init(&candidate);
    candidate.document = *document;
    asset_document_state_mark_saved(&candidate.document.state);
    candidate.saved_document = *document;
    asset_document_state_mark_saved(&candidate.saved_document.state);
    candidate.active = true;
    *workspace = candidate;
    return FLOW_WORKSPACE_OK;
}

FlowWorkspaceResult flow_workspace_load(FlowWorkspace *workspace,
                                        const char *path) {
    FlowDocument document;
    FlowDocumentResult result;
    if (!workspace || !path) return FLOW_WORKSPACE_INVALID_ARGUMENT;
    flow_document_init(&document);
    result = flow_document_load(&document, path);
    if (result != FLOW_DOCUMENT_OK) return FLOW_WORKSPACE_INVALID_DOCUMENT;
    return flow_workspace_open_document(workspace, &document);
}

bool flow_workspace_is_dirty(const FlowWorkspace *workspace) {
    return workspace && workspace->active &&
           flow_document_is_dirty(&workspace->document);
}

const FlowNode *flow_workspace_selected_node(const FlowWorkspace *workspace) {
    if (!workspace || !workspace->active ||
        workspace->node_index >= workspace->document.node_count) return NULL;
    return &workspace->document.nodes[workspace->node_index];
}

size_t flow_workspace_outgoing_count(const FlowWorkspace *workspace) {
    const FlowNode *node = flow_workspace_selected_node(workspace);
    size_t count = 0U;
    size_t i;
    if (!node) return 0U;
    for (i = 0U; i < workspace->document.edge_count; i++)
        if (workspace->document.edges[i].source_id == node->id) count++;
    return count;
}

const FlowEdge *flow_workspace_selected_edge(const FlowWorkspace *workspace) {
    const FlowNode *node = flow_workspace_selected_node(workspace);
    size_t index = 0U;
    size_t i;
    if (!node || workspace->edge_index >= flow_workspace_outgoing_count(workspace))
        return NULL;
    for (i = 0U; i < workspace->document.edge_count; i++)
        if (workspace->document.edges[i].source_id == node->id &&
            index++ == workspace->edge_index) return &workspace->document.edges[i];
    return NULL;
}

static size_t target_count(const FlowWorkspace *workspace) {
    size_t count = 0U;
    size_t i;
    for (i = 0U; i < workspace->document.node_count; i++)
        if (workspace->document.nodes[i].type != FLOW_NODE_START) count++;
    return count;
}

const FlowNode *flow_workspace_selected_target(const FlowWorkspace *workspace) {
    size_t index = 0U;
    size_t i;
    if (!workspace || !workspace->active ||
        workspace->target_index >= target_count(workspace)) return NULL;
    for (i = 0U; i < workspace->document.node_count; i++)
        if (workspace->document.nodes[i].type != FLOW_NODE_START &&
            index++ == workspace->target_index) return &workspace->document.nodes[i];
    return NULL;
}

static void step_index(size_t *index, size_t count, bool previous) {
    if (count == 0U) { *index = 0U; return; }
    if (previous) *index = *index == 0U ? count - 1U : *index - 1U;
    else *index = (*index + 1U) % count;
}

FlowWorkspaceResult flow_workspace_save_as(FlowWorkspace *workspace,
                                            const char *path) {
    FlowDocument candidate;
    if (!workspace || !path) return FLOW_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return FLOW_WORKSPACE_INACTIVE;
    candidate = workspace->document;
    if (flow_document_save_as(&candidate, path) != FLOW_DOCUMENT_OK)
        return FLOW_WORKSPACE_SAVE_FAILED;
    workspace->document = candidate;
    workspace->saved_document = candidate;
    return FLOW_WORKSPACE_OK;
}

static FlowWorkspaceResult apply_history_target(FlowWorkspace *workspace,
                                                const FlowWorkspaceChange *change,
                                                bool redo) {
    FlowDocument candidate = workspace->document;
    FlowDocumentResult result = flow_document_set_edge_target(
        &candidate, change->edge_id,
        redo ? change->after_target_id : change->before_target_id);
    if (result != FLOW_DOCUMENT_OK) return FLOW_WORKSPACE_MUTATION_FAILED;
    asset_document_state_restore(&candidate.state,
        redo ? change->after_state : change->before_state);
    workspace->document = candidate;
    return FLOW_WORKSPACE_OK;
}

FlowWorkspaceResult flow_workspace_undo(FlowWorkspace *workspace) {
    FlowWorkspaceResult result;
    if (!workspace) return FLOW_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return FLOW_WORKSPACE_INACTIVE;
    if (workspace->change_cursor == 0U) return FLOW_WORKSPACE_NO_ACTION;
    result = apply_history_target(workspace,
        &workspace->changes[workspace->change_cursor - 1U], false);
    if (result == FLOW_WORKSPACE_OK) workspace->change_cursor--;
    return result;
}

FlowWorkspaceResult flow_workspace_redo(FlowWorkspace *workspace) {
    FlowWorkspaceResult result;
    if (!workspace) return FLOW_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return FLOW_WORKSPACE_INACTIVE;
    if (workspace->change_cursor >= workspace->change_count)
        return FLOW_WORKSPACE_NO_ACTION;
    result = apply_history_target(workspace,
        &workspace->changes[workspace->change_cursor], true);
    if (result == FLOW_WORKSPACE_OK) workspace->change_cursor++;
    return result;
}

FlowWorkspaceResult flow_workspace_save(FlowWorkspace *workspace) {
    if (!workspace) return FLOW_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return FLOW_WORKSPACE_INACTIVE;
    if (workspace->document.path[0] == '\0') return FLOW_WORKSPACE_SAVE_FAILED;
    return flow_workspace_save_as(workspace, workspace->document.path);
}

FlowWorkspaceResult flow_workspace_handle_input(FlowWorkspace *workspace,
                                                FlowWorkspaceInput input) {
    size_t count;
    if (!workspace || input < FLOW_WORKSPACE_INPUT_PREVIOUS ||
        input > FLOW_WORKSPACE_INPUT_REDO) return FLOW_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return FLOW_WORKSPACE_INACTIVE;
    if (input == FLOW_WORKSPACE_INPUT_SAVE) return flow_workspace_save(workspace);
    if (input == FLOW_WORKSPACE_INPUT_UNDO) return flow_workspace_undo(workspace);
    if (input == FLOW_WORKSPACE_INPUT_REDO) return flow_workspace_redo(workspace);
    if (workspace->mode == FLOW_WORKSPACE_CLOSE_PROMPT) {
        if (input == FLOW_WORKSPACE_INPUT_PREVIOUS || input == FLOW_WORKSPACE_INPUT_NEXT) {
            size_t choice = (size_t)workspace->close_choice;
            step_index(&choice, FLOW_WORKSPACE_CLOSE_CHOICE_COUNT,
                       input == FLOW_WORKSPACE_INPUT_PREVIOUS);
            workspace->close_choice = (FlowWorkspaceCloseChoice)choice;
            return FLOW_WORKSPACE_OK;
        }
        if (input == FLOW_WORKSPACE_INPUT_ESCAPE) {
            workspace->mode = FLOW_WORKSPACE_NODES;
            workspace->close_choice = FLOW_WORKSPACE_CLOSE_SAVE;
            return FLOW_WORKSPACE_OK;
        }
        if (input == FLOW_WORKSPACE_INPUT_CONFIRM) {
            if (workspace->close_choice == FLOW_WORKSPACE_CLOSE_CANCEL) {
                workspace->mode = FLOW_WORKSPACE_NODES;
                workspace->close_choice = FLOW_WORKSPACE_CLOSE_SAVE;
                return FLOW_WORKSPACE_OK;
            }
            if (workspace->close_choice == FLOW_WORKSPACE_CLOSE_SAVE) {
                FlowWorkspaceResult result = flow_workspace_save(workspace);
                if (result != FLOW_WORKSPACE_OK) return result;
            } else {
                workspace->document = workspace->saved_document;
                workspace->change_count = 0U;
                workspace->change_cursor = 0U;
            }
            workspace->active = false;
            return FLOW_WORKSPACE_OK;
        }
        return FLOW_WORKSPACE_NO_ACTION;
    }
    if (input == FLOW_WORKSPACE_INPUT_ESCAPE) {
        if (workspace->mode == FLOW_WORKSPACE_TARGETS)
            workspace->mode = FLOW_WORKSPACE_EDGES;
        else if (workspace->mode == FLOW_WORKSPACE_EDGES)
            workspace->mode = FLOW_WORKSPACE_NODES;
        else if (flow_workspace_is_dirty(workspace))
            workspace->mode = FLOW_WORKSPACE_CLOSE_PROMPT;
        else workspace->active = false;
        return FLOW_WORKSPACE_OK;
    }
    if (workspace->mode == FLOW_WORKSPACE_NODES) count = workspace->document.node_count;
    else if (workspace->mode == FLOW_WORKSPACE_EDGES) count = flow_workspace_outgoing_count(workspace);
    else count = target_count(workspace);
    if (input == FLOW_WORKSPACE_INPUT_PREVIOUS || input == FLOW_WORKSPACE_INPUT_NEXT) {
        size_t *index = workspace->mode == FLOW_WORKSPACE_NODES ? &workspace->node_index :
                        workspace->mode == FLOW_WORKSPACE_EDGES ? &workspace->edge_index :
                        &workspace->target_index;
        step_index(index, count, input == FLOW_WORKSPACE_INPUT_PREVIOUS);
        return count ? FLOW_WORKSPACE_OK : FLOW_WORKSPACE_NO_ACTION;
    }
    if (input != FLOW_WORKSPACE_INPUT_CONFIRM) return FLOW_WORKSPACE_NO_ACTION;
    if (workspace->mode == FLOW_WORKSPACE_NODES) {
        if (flow_workspace_outgoing_count(workspace) == 0U)
            return FLOW_WORKSPACE_NO_ACTION;
        workspace->edge_index = 0U;
        workspace->mode = FLOW_WORKSPACE_EDGES;
        return FLOW_WORKSPACE_OK;
    }
    if (workspace->mode == FLOW_WORKSPACE_EDGES) {
        const FlowEdge *edge = flow_workspace_selected_edge(workspace);
        size_t i;
        if (!edge || target_count(workspace) == 0U) return FLOW_WORKSPACE_NO_ACTION;
        workspace->target_index = 0U;
        for (i = 0U; i < target_count(workspace); i++) {
            workspace->target_index = i;
            if (flow_workspace_selected_target(workspace)->id == edge->target_id) break;
        }
        workspace->mode = FLOW_WORKSPACE_TARGETS;
        return FLOW_WORKSPACE_OK;
    }
    {
        FlowDocument candidate = workspace->document;
        const FlowEdge *edge = flow_workspace_selected_edge(workspace);
        const FlowNode *target = flow_workspace_selected_target(workspace);
        FlowWorkspaceChange change;
        FlowDocumentResult result;
        if (!edge || !target) return FLOW_WORKSPACE_NO_ACTION;
        if (edge->target_id == target->id) {
            workspace->mode = FLOW_WORKSPACE_EDGES;
            return FLOW_WORKSPACE_OK;
        }
        if (workspace->change_cursor >= FLOW_MAX_EDGES)
            return FLOW_WORKSPACE_MUTATION_FAILED;
        change.edge_id = edge->id;
        change.before_target_id = edge->target_id;
        change.after_target_id = target->id;
        change.before_state = candidate.state.current_state;
        result = flow_document_set_edge_target(&candidate, edge->id, target->id);
        if (result != FLOW_DOCUMENT_OK) return FLOW_WORKSPACE_MUTATION_FAILED;
        change.after_state = candidate.state.current_state;
        workspace->document = candidate;
        workspace->changes[workspace->change_cursor] = change;
        workspace->change_cursor++;
        workspace->change_count = workspace->change_cursor;
        workspace->mode = FLOW_WORKSPACE_EDGES;
        return FLOW_WORKSPACE_OK;
    }
}