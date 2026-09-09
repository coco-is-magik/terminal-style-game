/** flow_runtime.h — Pure deterministic navigation over a validated flow graph. */
#ifndef FLOW_RUNTIME_H
#define FLOW_RUNTIME_H

#include "flow_reference.h"

typedef struct {
    FlowNodeId current_node_id;
} FlowRuntimeSession;

typedef enum {
    FLOW_RUNTIME_OK = 0,
    FLOW_RUNTIME_INVALID_ARGUMENT,
    FLOW_RUNTIME_INVALID_DATA,
    FLOW_RUNTIME_INVALID_SESSION,
    FLOW_RUNTIME_MISSING_PORT
} FlowRuntimeResult;

FlowRuntimeResult flow_runtime_session_init(
    FlowRuntimeSession *session,
    const FlowDocument *document,
    const FlowReferenceCatalog *catalog
);
const FlowNode *flow_runtime_current_node(
    const FlowRuntimeSession *session,
    const FlowDocument *document
);
FlowRuntimeResult flow_runtime_transition(
    FlowRuntimeSession *session,
    const FlowDocument *document,
    const char *source_port,
    const FlowNode **out_target
);

#endif