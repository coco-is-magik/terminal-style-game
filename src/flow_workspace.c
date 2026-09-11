#include "flow_workspace.h"
#include "ui_nested_inspector.h"

#include <stdio.h>
#include <string.h>

static bool valid_workspace_document(const FlowDocument *document) {
    return document && flow_document_validate(document) == FLOW_DOCUMENT_OK;
}

void flow_workspace_init(FlowWorkspace *workspace) {
    if (!workspace) return;
    memset(workspace, 0, sizeof(*workspace));
    flow_document_init(&workspace->document);
    workspace->saved_document = workspace->document;
}

static void clear_history(FlowWorkspace *workspace) {
    memset(workspace->changes, 0, sizeof(workspace->changes));
    workspace->change_count = 0U;
    workspace->change_cursor = 0U;
}

void flow_workspace_clear(FlowWorkspace *workspace) {
    if (!workspace) return;
    clear_history(workspace);
    flow_workspace_init(workspace);
}

FlowWorkspaceResult flow_workspace_set_catalog(
    FlowWorkspace *workspace,
    const FlowReferenceCatalog *catalog
) {
    if (!workspace || !catalog) return FLOW_WORKSPACE_INVALID_ARGUMENT;
    if (flow_reference_validate_catalog(catalog) != FLOW_REFERENCE_OK)
        return FLOW_WORKSPACE_INVALID_DOCUMENT;
    workspace->catalog = catalog;
    return FLOW_WORKSPACE_OK;
}

FlowWorkspaceResult flow_workspace_open_document(FlowWorkspace *workspace,
                                                 const FlowDocument *document) {
    FlowDocument document_copy;
    const FlowReferenceCatalog *catalog;
    if (!workspace || !document) return FLOW_WORKSPACE_INVALID_ARGUMENT;
    if (!valid_workspace_document(document)) return FLOW_WORKSPACE_INVALID_DOCUMENT;
    document_copy = *document;
    catalog = NULL;
    flow_workspace_init(workspace);
    workspace->catalog = catalog;
    workspace->document = document_copy;
    asset_document_state_mark_saved(&workspace->document.state);
    workspace->saved_document = workspace->document;
    workspace->active = true;
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

static const FlowReferenceEntry *selected_node_entry(const FlowWorkspace *workspace) {
    const FlowNode *node = flow_workspace_selected_node(workspace);
    if (!node || node->type == FLOW_NODE_START || !workspace->catalog) return NULL;
    return flow_reference_find(workspace->catalog, node->type, node->asset_name);
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

size_t flow_workspace_connection_count(const FlowWorkspace *workspace) {
    const FlowNode *node = flow_workspace_selected_node(workspace);
    const FlowReferenceEntry *entry;
    if (!node) return 0U;
    if (node->type == FLOW_NODE_START)
        return flow_workspace_outgoing_count(workspace) > 0U ||
               flow_workspace_target_count(workspace) > 0U ? 1U : 0U;
    entry = selected_node_entry(workspace);
    return entry ? entry->port_count : flow_workspace_outgoing_count(workspace);
}

const char *flow_workspace_selected_port(const FlowWorkspace *workspace) {
    const FlowNode *node = flow_workspace_selected_node(workspace);
    const FlowReferenceEntry *entry;
    size_t index = 0U;
    size_t i;
    if (!node || workspace->edge_index >= flow_workspace_connection_count(workspace))
        return NULL;
    if (node->type == FLOW_NODE_START) return "start";
    entry = selected_node_entry(workspace);
    if (entry) return entry->ports[workspace->edge_index];
    for (i = 0U; i < workspace->document.edge_count; i++)
        if (workspace->document.edges[i].source_id == node->id &&
            index++ == workspace->edge_index)
            return workspace->document.edges[i].source_port;
    return NULL;
}

const FlowEdge *flow_workspace_selected_edge(const FlowWorkspace *workspace) {
    const FlowNode *node = flow_workspace_selected_node(workspace);
    const char *port = flow_workspace_selected_port(workspace);
    size_t i;
    if (!node || !port) return NULL;
    for (i = 0U; i < workspace->document.edge_count; i++)
        if (workspace->document.edges[i].source_id == node->id &&
            strcmp(workspace->document.edges[i].source_port, port) == 0)
            return &workspace->document.edges[i];
    return NULL;
}

static bool asset_is_in_document(const FlowWorkspace *workspace,
                                 const FlowReferenceEntry *entry) {
    size_t i;
    for (i = 0U; i < workspace->document.node_count; i++)
        if (workspace->document.nodes[i].type == entry->type &&
            strcmp(workspace->document.nodes[i].asset_name, entry->name) == 0)
            return true;
    return false;
}

static size_t absent_asset_count(const FlowWorkspace *workspace) {
    size_t count = 0U;
    size_t i;
    if (!workspace->catalog) return 0U;
    for (i = 0U; i < workspace->catalog->count; i++)
        if (!asset_is_in_document(workspace, &workspace->catalog->entries[i])) count++;
    return count;
}

size_t flow_workspace_target_count(const FlowWorkspace *workspace) {
    if (!workspace || workspace->document.node_count == 0U) return 0U;
    return workspace->document.node_count - 1U + absent_asset_count(workspace);
}

const FlowNode *flow_workspace_selected_target(const FlowWorkspace *workspace) {
    size_t index = 0U;
    size_t i;
    if (!workspace || !workspace->active) return NULL;
    for (i = 0U; i < workspace->document.node_count; i++) {
        if (workspace->document.nodes[i].type == FLOW_NODE_START) continue;
        if (index++ == workspace->target_index) return &workspace->document.nodes[i];
    }
    return NULL;
}

const FlowReferenceEntry *flow_workspace_selected_asset(
    const FlowWorkspace *workspace
) {
    size_t index;
    size_t i;
    if (!workspace || !workspace->catalog || workspace->document.node_count == 0U ||
        workspace->target_index < workspace->document.node_count - 1U) return NULL;
    index = workspace->target_index - (workspace->document.node_count - 1U);
    for (i = 0U; i < workspace->catalog->count; i++) {
        const FlowReferenceEntry *entry = &workspace->catalog->entries[i];
        if (asset_is_in_document(workspace, entry)) continue;
        if (index == 0U) return entry;
        index--;
    }
    return NULL;
}

static void step_index(size_t *index, size_t count, bool previous) {
    if (count == 0U) { *index = 0U; return; }
    (void)ui_nested_inspector_step(index, count, previous);
}

static void sync_nested_index(FlowWorkspace *workspace, size_t index) {
    size_t *nested = ui_nested_inspector_index(&workspace->nested_cursor);
    if (nested) *nested = index;
}

static bool enter_nested(FlowWorkspace *workspace, size_t parent_index,
                         size_t child_count, size_t *child_index) {
    sync_nested_index(workspace, parent_index);
    if (!ui_nested_inspector_enter(&workspace->nested_cursor, child_count)) return false;
    *child_index = *ui_nested_inspector_index(&workspace->nested_cursor);
    return true;
}

static bool escape_nested(FlowWorkspace *workspace, size_t *parent_index) {
    if (!ui_nested_inspector_escape(&workspace->nested_cursor)) return false;
    *parent_index = *ui_nested_inspector_index(&workspace->nested_cursor);
    return true;
}

static FlowWorkspaceResult record_change(FlowWorkspace *workspace,
                                         const FlowDocument *before,
                                         const FlowDocument *after) {
    if (workspace->change_cursor >= FLOW_WORKSPACE_HISTORY_CAPACITY)
        return FLOW_WORKSPACE_MUTATION_FAILED;
    workspace->changes[workspace->change_cursor].before = *before;
    workspace->changes[workspace->change_cursor].after = *after;
    workspace->change_cursor++;
    workspace->change_count = workspace->change_cursor;
    workspace->document = *after;
    return FLOW_WORKSPACE_OK;
}

static FlowWorkspaceResult commit_candidate(FlowWorkspace *workspace,
                                            const FlowDocument *before,
                                            FlowDocument *candidate) {
    if (flow_document_validate(candidate) != FLOW_DOCUMENT_OK ||
        (workspace->catalog && flow_reference_validate_document(
            candidate, workspace->catalog) != FLOW_REFERENCE_OK))
        return FLOW_WORKSPACE_MUTATION_FAILED;
    candidate->state = before->state;
    if (!asset_document_state_advance(&candidate->state))
        return FLOW_WORKSPACE_MUTATION_FAILED;
    return record_change(workspace, before, candidate);
}

FlowWorkspaceResult flow_workspace_save_as(FlowWorkspace *workspace,
                                            const char *path) {
    FlowDocument candidate;
    size_t i;
    if (!workspace || !path) return FLOW_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return FLOW_WORKSPACE_INACTIVE;
    candidate = workspace->document;
    if (flow_document_save_as(&candidate, path) != FLOW_DOCUMENT_OK)
        return FLOW_WORKSPACE_SAVE_FAILED;
    workspace->document = candidate;
    workspace->saved_document = candidate;
    for (i = 0U; i < workspace->change_count; i++) {
        workspace->changes[i].before.state.saved_state = candidate.state.saved_state;
        workspace->changes[i].after.state.saved_state = candidate.state.saved_state;
        (void)snprintf(workspace->changes[i].before.path,
                       sizeof(workspace->changes[i].before.path), "%s", candidate.path);
        (void)snprintf(workspace->changes[i].after.path,
                       sizeof(workspace->changes[i].after.path), "%s", candidate.path);
    }
    return FLOW_WORKSPACE_OK;
}

FlowWorkspaceResult flow_workspace_undo(FlowWorkspace *workspace) {
    DocumentStateId next_state;
    if (!workspace) return FLOW_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return FLOW_WORKSPACE_INACTIVE;
    if (workspace->change_cursor == 0U) return FLOW_WORKSPACE_NO_ACTION;
    next_state = workspace->document.state.next_state;
    workspace->document = workspace->changes[workspace->change_cursor - 1U].before;
    if (workspace->document.state.next_state < next_state)
        workspace->document.state.next_state = next_state;
    workspace->change_cursor--;
    return FLOW_WORKSPACE_OK;
}

FlowWorkspaceResult flow_workspace_redo(FlowWorkspace *workspace) {
    DocumentStateId next_state;
    if (!workspace) return FLOW_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return FLOW_WORKSPACE_INACTIVE;
    if (workspace->change_cursor >= workspace->change_count)
        return FLOW_WORKSPACE_NO_ACTION;
    next_state = workspace->document.state.next_state;
    workspace->document = workspace->changes[workspace->change_cursor].after;
    if (workspace->document.state.next_state < next_state)
        workspace->document.state.next_state = next_state;
    workspace->change_cursor++;
    return FLOW_WORKSPACE_OK;
}

FlowWorkspaceResult flow_workspace_save(FlowWorkspace *workspace) {
    if (!workspace) return FLOW_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return FLOW_WORKSPACE_INACTIVE;
    return flow_workspace_save_as(workspace, workspace->document.path);
}

static FlowWorkspaceResult remove_selection(FlowWorkspace *workspace) {
    FlowDocument before = workspace->document;
    FlowDocument candidate = before;
    FlowDocumentResult result;
    if (workspace->mode == FLOW_WORKSPACE_EDGES) {
        const FlowEdge *edge = flow_workspace_selected_edge(workspace);
        if (!edge) return FLOW_WORKSPACE_NO_ACTION;
        result = flow_document_disconnect(&candidate, edge->id);
    } else if (workspace->mode == FLOW_WORKSPACE_NODES) {
        const FlowNode *node = flow_workspace_selected_node(workspace);
        if (!node || node->type == FLOW_NODE_START) return FLOW_WORKSPACE_NO_ACTION;
        result = flow_document_remove_node(&candidate, node->id);
    } else return FLOW_WORKSPACE_NO_ACTION;
    if (result != FLOW_DOCUMENT_OK) return FLOW_WORKSPACE_MUTATION_FAILED;
    if (workspace->node_index >= candidate.node_count)
        workspace->node_index = candidate.node_count - 1U;
    return commit_candidate(workspace, &before, &candidate);
}

static FlowWorkspaceResult choose_target(FlowWorkspace *workspace) {
    FlowDocument before = workspace->document;
    FlowDocument candidate = before;
    const FlowEdge *edge = flow_workspace_selected_edge(workspace);
    const FlowNode *target = flow_workspace_selected_target(workspace);
    const FlowReferenceEntry *asset = flow_workspace_selected_asset(workspace);
    const FlowNode *source = flow_workspace_selected_node(workspace);
    const char *port = flow_workspace_selected_port(workspace);
    FlowNodeId target_id;
    FlowEdgeId edge_id;
    FlowDocumentResult result;
    FlowWorkspaceResult workspace_result;
    if (!source || !port || (!target && !asset)) return FLOW_WORKSPACE_NO_ACTION;
    if (asset) {
        result = flow_document_add_node(&candidate, asset->type, asset->name, &target_id);
        if (result != FLOW_DOCUMENT_OK) return FLOW_WORKSPACE_MUTATION_FAILED;
    } else target_id = target->id;
    if (edge) {
        if (edge->target_id == target_id) {
            workspace->mode = FLOW_WORKSPACE_EDGES;
            return FLOW_WORKSPACE_OK;
        }
        result = flow_document_set_edge_target(&candidate, edge->id, target_id);
    } else {
        result = flow_document_connect(&candidate, source->id, port, target_id, &edge_id);
    }
    if (result != FLOW_DOCUMENT_OK) return FLOW_WORKSPACE_MUTATION_FAILED;
    workspace_result = commit_candidate(workspace, &before, &candidate);
    if (workspace_result == FLOW_WORKSPACE_OK) {
        (void)escape_nested(workspace, &workspace->edge_index);
        workspace->mode = FLOW_WORKSPACE_EDGES;
    }
    return workspace_result;
}

static void step_close_choice(FlowWorkspace *workspace, bool previous) {
    int choice = (int)workspace->close_choice;
    if (previous)
        choice = choice == 0 ? FLOW_WORKSPACE_CLOSE_CHOICE_COUNT - 1 : choice - 1;
    else choice = (choice + 1) % FLOW_WORKSPACE_CLOSE_CHOICE_COUNT;
    workspace->close_choice = (FlowWorkspaceCloseChoice)choice;
}

FlowWorkspaceResult flow_workspace_handle_input(FlowWorkspace *workspace,
                                                FlowWorkspaceInput input) {
    size_t count;
    if (!workspace) return FLOW_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return FLOW_WORKSPACE_INACTIVE;
    if (input < FLOW_WORKSPACE_INPUT_PREVIOUS || input > FLOW_WORKSPACE_INPUT_REMOVE)
        return FLOW_WORKSPACE_INVALID_ARGUMENT;
    if (input == FLOW_WORKSPACE_INPUT_SAVE) return flow_workspace_save(workspace);
    if (input == FLOW_WORKSPACE_INPUT_UNDO) return flow_workspace_undo(workspace);
    if (input == FLOW_WORKSPACE_INPUT_REDO) return flow_workspace_redo(workspace);
    if (input == FLOW_WORKSPACE_INPUT_REMOVE) return remove_selection(workspace);
    if (workspace->mode == FLOW_WORKSPACE_CLOSE_PROMPT) {
        if (input == FLOW_WORKSPACE_INPUT_ESCAPE) {
            workspace->mode = FLOW_WORKSPACE_NODES;
            return FLOW_WORKSPACE_OK;
        }
        if (input == FLOW_WORKSPACE_INPUT_PREVIOUS || input == FLOW_WORKSPACE_INPUT_NEXT) {
            step_close_choice(workspace, input == FLOW_WORKSPACE_INPUT_PREVIOUS);
            return FLOW_WORKSPACE_OK;
        }
        if (input == FLOW_WORKSPACE_INPUT_CONFIRM) {
            if (workspace->close_choice == FLOW_WORKSPACE_CLOSE_SAVE) {
                FlowWorkspaceResult result = flow_workspace_save(workspace);
                if (result == FLOW_WORKSPACE_OK) workspace->active = false;
                return result;
            }
            if (workspace->close_choice == FLOW_WORKSPACE_CLOSE_DISCARD) {
                workspace->document = workspace->saved_document;
                clear_history(workspace);
                workspace->active = false;
                return FLOW_WORKSPACE_OK;
            }
            workspace->mode = FLOW_WORKSPACE_NODES;
            return FLOW_WORKSPACE_OK;
        }
        return FLOW_WORKSPACE_NO_ACTION;
    }
    if (input == FLOW_WORKSPACE_INPUT_ESCAPE) {
        if (workspace->mode == FLOW_WORKSPACE_TARGETS) {
            (void)escape_nested(workspace, &workspace->edge_index);
            workspace->mode = FLOW_WORKSPACE_EDGES;
        } else if (workspace->mode == FLOW_WORKSPACE_EDGES) {
            (void)escape_nested(workspace, &workspace->node_index);
            workspace->mode = FLOW_WORKSPACE_NODES;
        } else if (flow_workspace_is_dirty(workspace))
            workspace->mode = FLOW_WORKSPACE_CLOSE_PROMPT;
        else workspace->active = false;
        return FLOW_WORKSPACE_OK;
    }
    if (workspace->mode == FLOW_WORKSPACE_NODES) count = workspace->document.node_count;
    else if (workspace->mode == FLOW_WORKSPACE_EDGES)
        count = flow_workspace_connection_count(workspace);
    else count = flow_workspace_target_count(workspace);
    if (input == FLOW_WORKSPACE_INPUT_PREVIOUS || input == FLOW_WORKSPACE_INPUT_NEXT) {
        size_t *index = workspace->mode == FLOW_WORKSPACE_NODES ? &workspace->node_index :
                        workspace->mode == FLOW_WORKSPACE_EDGES ? &workspace->edge_index :
                        &workspace->target_index;
        step_index(index, count, input == FLOW_WORKSPACE_INPUT_PREVIOUS);
        sync_nested_index(workspace, *index);
        return count ? FLOW_WORKSPACE_OK : FLOW_WORKSPACE_NO_ACTION;
    }
    if (input != FLOW_WORKSPACE_INPUT_CONFIRM) return FLOW_WORKSPACE_NO_ACTION;
    if (workspace->mode == FLOW_WORKSPACE_NODES) {
        if (flow_workspace_connection_count(workspace) == 0U)
            return FLOW_WORKSPACE_NO_ACTION;
        if (!enter_nested(workspace, workspace->node_index,
                          flow_workspace_connection_count(workspace),
                          &workspace->edge_index)) return FLOW_WORKSPACE_NO_ACTION;
        workspace->mode = FLOW_WORKSPACE_EDGES;
        return FLOW_WORKSPACE_OK;
    }
    if (workspace->mode == FLOW_WORKSPACE_EDGES) {
        const FlowEdge *edge = flow_workspace_selected_edge(workspace);
        size_t i;
        if (flow_workspace_target_count(workspace) == 0U)
            return FLOW_WORKSPACE_NO_ACTION;
        if (!enter_nested(workspace, workspace->edge_index,
                          flow_workspace_target_count(workspace),
                          &workspace->target_index)) return FLOW_WORKSPACE_NO_ACTION;
        if (edge) {
            for (i = 0U; i < workspace->document.node_count - 1U; i++) {
                workspace->target_index = i;
                if (flow_workspace_selected_target(workspace)->id == edge->target_id) break;
            }
            sync_nested_index(workspace, workspace->target_index);
        }
        workspace->mode = FLOW_WORKSPACE_TARGETS;
        return FLOW_WORKSPACE_OK;
    }
    return choose_target(workspace);
}