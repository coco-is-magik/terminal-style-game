#include "flow_binding.h"

#include <ctype.h>
#include <stddef.h>

static bool valid_port(const char *port) {
    size_t i;
    if (!port || port[0] == '\0') return false;
    for (i = 0U; i < FLOW_NAME_CAPACITY && port[i] != '\0'; i++) {
        unsigned char c = (unsigned char)port[i];
        if (!(isalnum(c) || c == '_' || c == '-')) return false;
    }
    return i < FLOW_NAME_CAPACITY;
}

static FlowBindingResult map_runtime_result(FlowRuntimeResult result) {
    if (result == FLOW_RUNTIME_INVALID_DATA) return FLOW_BINDING_INVALID_DATA;
    if (result == FLOW_RUNTIME_INVALID_SESSION) return FLOW_BINDING_INVALID_SESSION;
    if (result == FLOW_RUNTIME_MISSING_PORT) return FLOW_BINDING_MISSING_PORT;
    return FLOW_BINDING_INVALID_ARGUMENT;
}

static FlowBindingResult transition(
    FlowRuntimeSession *session,
    const FlowDocument *document,
    FlowNodeType required_source_type,
    const char *port,
    FlowBindingTargetRequest *out_request
) {
    FlowRuntimeSession candidate;
    const FlowNode *current;
    const FlowNode *target = NULL;
    FlowRuntimeResult runtime_result;
    FlowBindingTargetRequest request;
    if (!session || !document || !out_request)
        return FLOW_BINDING_INVALID_ARGUMENT;
    if (!valid_port(port)) return FLOW_BINDING_INVALID_ACTIVATION;
    if (flow_document_validate(document) != FLOW_DOCUMENT_OK)
        return FLOW_BINDING_INVALID_DATA;
    current = flow_runtime_current_node(session, document);
    if (!current) return FLOW_BINDING_INVALID_SESSION;
    if (current->type != required_source_type)
        return FLOW_BINDING_WRONG_SOURCE_TYPE;
    candidate = *session;
    runtime_result = flow_runtime_transition(&candidate, document, port, &target);
    if (runtime_result != FLOW_RUNTIME_OK) return map_runtime_result(runtime_result);
    if (!target || (target->type != FLOW_NODE_SCENE && target->type != FLOW_NODE_MENU) ||
        target->asset_name[0] == '\0')
        return FLOW_BINDING_INVALID_TARGET;
    request.node_id = target->id;
    request.type = target->type;
    request.asset_name = target->asset_name;
    *session = candidate;
    *out_request = request;
    return FLOW_BINDING_OK;
}

FlowBindingResult flow_binding_activate_button(
    FlowRuntimeSession *session,
    const FlowDocument *document,
    const UiInteractionActivation *activation,
    FlowBindingTargetRequest *out_request
) {
    if (!activation || activation->element_id == 0U)
        return activation ? FLOW_BINDING_INVALID_ACTIVATION
                          : FLOW_BINDING_INVALID_ARGUMENT;
    return transition(session, document, FLOW_NODE_MENU, activation->flow_port,
                      out_request);
}

FlowBindingResult flow_binding_activate_scene_exit(
    FlowRuntimeSession *session,
    const FlowDocument *document,
    const EntityTriggerTickResult *tick_result,
    FlowBindingTargetRequest *out_request
) {
    if (!tick_result) return FLOW_BINDING_INVALID_ARGUMENT;
    if (!tick_result->flow_exit_requested || tick_result->flow_exit_trigger_id == 0U)
        return FLOW_BINDING_INVALID_ACTIVATION;
    return transition(session, document, FLOW_NODE_SCENE,
                      tick_result->flow_exit_port, out_request);
}