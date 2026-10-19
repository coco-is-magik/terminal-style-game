/** flow_graph.h — Deterministic layout for the game-flow node graph. */
#ifndef FLOW_GRAPH_H
#define FLOW_GRAPH_H

#include "flow_document.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    size_t node_index;
    int x;
    int y;
    int width;
    int height;
    int layer;
    int order;
    bool selected;
} FlowGraphNodeBox;

typedef struct {
    size_t edge_index;
    size_t source_node;
    size_t target_node;
    int source_x;
    int source_y;
    int target_x;
    int target_y;
    int lane_x;
} FlowGraphEdgeRoute;

typedef struct {
    int left;
    int top;
    int width;
    int height;
    int node_width;
    int node_height;
    int offset_x;
    int offset_y;
    FlowGraphNodeBox nodes[FLOW_MAX_NODES];
    size_t node_count;
    FlowGraphEdgeRoute edges[FLOW_MAX_EDGES];
    size_t edge_count;
} FlowGraphLayout;

bool flow_graph_layout_build(const FlowDocument *document,
                             const FlowNode *selected,
                             int left, int top, int width, int height,
                             FlowGraphLayout *out_layout);

const FlowGraphNodeBox *flow_graph_node_at(const FlowGraphLayout *layout,
                                           int x, int y);

#endif /* FLOW_GRAPH_H */
