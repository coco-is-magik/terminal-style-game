#include "scene_flow_adapter.h"

#include <ctype.h>
#include <string.h>

_Static_assert(SCENE_MAX_FLOW_EXITS == FLOW_REFERENCE_MAX_PORTS,
               "scene and flow exit capacities must match");

static bool bounded_identifier(const char *text, size_t capacity) {
    size_t i;
    if (!text || text[0] == '\0') return false;
    for (i = 0U; i < capacity && text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];
        if (!(isalnum(c) || c == '_' || c == '-')) return false;
    }
    return i < capacity;
}

SceneFlowAdapterResult scene_flow_reference_view_build(
    const SceneDocument *document,
    SceneFlowReferenceView *out_view
) {
    SceneFlowReferenceView candidate = {0};
    size_t i;
    if (!document || !out_view) return SCENE_FLOW_ADAPTER_INVALID_ARGUMENT;
    if (!bounded_identifier(document->name, sizeof(document->name)) ||
        document->trigger_count > SCENE_MAX_TRIGGERS ||
        (document->trigger_count > 0U && !document->triggers))
        return SCENE_FLOW_ADAPTER_INVALID_SCENE;
    for (i = 0U; i < document->trigger_count; i++) {
        const SceneTrigger *trigger = &document->triggers[i];
        size_t prior;
        if (trigger->action != SCENE_TRIGGER_ACTION_EXIT_FLOW) continue;
        if (!bounded_identifier(trigger->flow_port,
                                SCENE_TRIGGER_FLOW_PORT_CAPACITY))
            return SCENE_FLOW_ADAPTER_INVALID_SCENE;
        if (candidate.entry.port_count >= FLOW_REFERENCE_MAX_PORTS)
            return SCENE_FLOW_ADAPTER_TOO_MANY_PORTS;
        for (prior = 0U; prior < candidate.entry.port_count; prior++)
            if (strncmp(candidate.ports[prior], trigger->flow_port,
                        SCENE_TRIGGER_FLOW_PORT_CAPACITY) == 0)
                return SCENE_FLOW_ADAPTER_DUPLICATE_PORT;
        candidate.ports[candidate.entry.port_count++] = trigger->flow_port;
    }
    candidate.entry.type = FLOW_NODE_SCENE;
    candidate.entry.name = document->name;
    candidate.entry.ports = candidate.ports;
    if (flow_reference_validate_catalog(&(FlowReferenceCatalog){&candidate.entry, 1U}) !=
        FLOW_REFERENCE_OK) {
        return SCENE_FLOW_ADAPTER_INVALID_SCENE;
    }
    *out_view = candidate;
    out_view->entry.ports = out_view->ports;
    return SCENE_FLOW_ADAPTER_OK;
}