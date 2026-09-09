/** scene_flow_adapter.h — Borrowed SceneDocument exit-port flow views. */
#ifndef SCENE_FLOW_ADAPTER_H
#define SCENE_FLOW_ADAPTER_H

#include "flow_reference.h"
#include "scene_document.h"

typedef enum {
    SCENE_FLOW_ADAPTER_OK = 0,
    SCENE_FLOW_ADAPTER_INVALID_ARGUMENT,
    SCENE_FLOW_ADAPTER_INVALID_SCENE,
    SCENE_FLOW_ADAPTER_TOO_MANY_PORTS,
    SCENE_FLOW_ADAPTER_DUPLICATE_PORT
} SceneFlowAdapterResult;

typedef struct {
    const char *ports[FLOW_REFERENCE_MAX_PORTS];
    FlowReferenceEntry entry;
} SceneFlowReferenceView;

SceneFlowAdapterResult scene_flow_reference_view_build(
    const SceneDocument *document,
    SceneFlowReferenceView *out_view
);

#endif /* SCENE_FLOW_ADAPTER_H */