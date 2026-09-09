#include "flow_runtime.h"

#include <stddef.h>
#include <string.h>

FlowRuntimeResult flow_runtime_session_init(
    FlowRuntimeSession *session,
    const FlowDocument *document,
    const FlowReferenceCatalog *catalog
) {
    size_t i;
    FlowNodeId start_id = 0U;
    if (!session || !document || !catalog) return FLOW_RUNTIME_INVALID_ARGUMENT;
    if (flow_reference_validate_document(document, catalog) != FLOW_REFERENCE_OK)
        return FLOW_RUNTIME_INVALID_DATA;
    for (i = 0U; i < document->node_count; i++)
        if (document->nodes[i].type == FLOW_NODE_START) {
            start_id = document->nodes[i].id;
            break;
        }
    if (start_id == 0U) return FLOW_RUNTIME_INVALID_DATA;
    session->current_node_id = start_id;
    return FLOW_RUNTIME_OK;
}

const FlowNode *flow_runtime_current_node(
    const FlowRuntimeSession *session,
    const FlowDocument *document
) {
    if (!session || !document) return NULL;
    return flow_document_find_node(document, session->current_node_id);
}

FlowRuntimeResult flow_runtime_transition(
    FlowRuntimeSession *session,
    const FlowDocument *document,
    const char *source_port,
    const FlowNode **out_target
) {
    const FlowNode *current;
    const FlowNode *target;
    size_t i;
    if (!session || !document || !source_port || source_port[0] == '\0' ||
        !out_target) return FLOW_RUNTIME_INVALID_ARGUMENT;
    if (flow_document_validate(document) != FLOW_DOCUMENT_OK)
        return FLOW_RUNTIME_INVALID_DATA;
    current = flow_runtime_current_node(session, document);
    if (!current) return FLOW_RUNTIME_INVALID_SESSION;
    for (i = 0U; i < document->edge_count; i++) {
        const FlowEdge *edge = &document->edges[i];
        if (edge->source_id != current->id ||
            strcmp(edge->source_port, source_port) != 0) continue;
        target = flow_document_find_node(document, edge->target_id);
        if (!target) return FLOW_RUNTIME_INVALID_DATA;
        session->current_node_id = target->id;
        *out_target = target;
        return FLOW_RUNTIME_OK;
    }
    return FLOW_RUNTIME_MISSING_PORT;
}