/** flow_binding.h — Pure typed activation adapter for authored game flow. */
#ifndef FLOW_BINDING_H
#define FLOW_BINDING_H

#include "entity_trigger_session.h"
#include "flow_runtime.h"
#include "ui_interaction.h"

typedef struct {
    FlowNodeId node_id;
    FlowNodeType type;
    const char *asset_name;
} FlowBindingTargetRequest;

typedef enum {
    FLOW_BINDING_OK = 0,
    FLOW_BINDING_INVALID_ARGUMENT,
    FLOW_BINDING_INVALID_ACTIVATION,
    FLOW_BINDING_INVALID_DATA,
    FLOW_BINDING_INVALID_SESSION,
    FLOW_BINDING_WRONG_SOURCE_TYPE,
    FLOW_BINDING_MISSING_PORT,
    FLOW_BINDING_INVALID_TARGET
} FlowBindingResult;

FlowBindingResult flow_binding_activate_button(
    FlowRuntimeSession *session,
    const FlowDocument *document,
    const UiInteractionActivation *activation,
    FlowBindingTargetRequest *out_request
);

FlowBindingResult flow_binding_activate_scene_exit(
    FlowRuntimeSession *session,
    const FlowDocument *document,
    const EntityTriggerTickResult *tick_result,
    FlowBindingTargetRequest *out_request
);

#endif /* FLOW_BINDING_H */