#include "flow_graph.h"

#include <string.h>

#define FLOW_GRAPH_GAP_X 12
#define FLOW_GRAPH_GAP_Y 2
#define FLOW_GRAPH_MIN_NODE_WIDTH 20
#define FLOW_GRAPH_MAX_NODE_WIDTH 34
#define FLOW_GRAPH_NODE_HEIGHT 6

static int type_label_width(FlowNodeType type) {
    return type == FLOW_NODE_START ? 5 : type == FLOW_NODE_SCENE ? 5 : 4;
}

static int node_width_for(const FlowNode *node) {
    int width = (int)strlen(node->asset_name) + type_label_width(node->type) + 9;
    if (width < FLOW_GRAPH_MIN_NODE_WIDTH) width = FLOW_GRAPH_MIN_NODE_WIDTH;
    if (width > FLOW_GRAPH_MAX_NODE_WIDTH) width = FLOW_GRAPH_MAX_NODE_WIDTH;
    return width;
}

bool flow_graph_layout_build(const FlowDocument *document,
                             const FlowNode *selected,
                             int left, int top, int width, int height,
                             FlowGraphLayout *out_layout) {
    int layer[FLOW_MAX_NODES + 1];
    int order[FLOW_MAX_NODES + 1];
    int column_count[FLOW_MAX_NODES + 1];
    int raw_x[FLOW_MAX_NODES];
    int raw_y[FLOW_MAX_NODES];
    size_t queue[FLOW_MAX_NODES];
    size_t head = 0U;
    size_t tail = 0U;
    size_t start_index = FLOW_MAX_NODES;
    size_t selected_index = FLOW_MAX_NODES;
    size_t i;
    int max_layer = 0;
    int node_width;
    int offset_x = 0;
    int offset_y = 0;

    if (!document || !out_layout || document->node_count > FLOW_MAX_NODES ||
        document->edge_count > FLOW_MAX_EDGES)
        return false;
    if (width <= 0 || height <= 0) return false;
    memset(out_layout, 0, sizeof(*out_layout));
    out_layout->left = left;
    out_layout->top = top;
    out_layout->width = width;
    out_layout->height = height;
    out_layout->node_height = FLOW_GRAPH_NODE_HEIGHT;

    for (i = 0U; i < FLOW_MAX_NODES + 1U; i++) {
        layer[i] = -1;
        order[i] = 0;
        column_count[i] = 0;
    }
    for (i = 0U; i < document->node_count; i++) {
        if (document->nodes[i].type == FLOW_NODE_START && start_index == FLOW_MAX_NODES)
            start_index = i;
        if (selected && (&document->nodes[i] == selected ||
                         document->nodes[i].id == selected->id))
            selected_index = i;
    }
    if (start_index < FLOW_MAX_NODES) {
        layer[start_index] = 0;
        queue[tail++] = start_index;
        while (head < tail) {
            const size_t current = queue[head++];
            for (i = 0U; i < document->edge_count; i++) {
                const FlowEdge *edge = &document->edges[i];
                size_t target = FLOW_MAX_NODES;
                size_t j;
                if (edge->source_id != document->nodes[current].id) continue;
                for (j = 0U; j < document->node_count; j++)
                    if (document->nodes[j].id == edge->target_id) {
                        target = j;
                        break;
                    }
                if (target == FLOW_MAX_NODES || layer[target] >= 0) continue;
                layer[target] = layer[current] + 1;
                queue[tail++] = target;
            }
        }
    }
    for (i = 0U; i < document->node_count; i++)
        if (layer[i] > max_layer) max_layer = layer[i];
    for (i = 0U; i < document->node_count; i++)
        if (layer[i] < 0) layer[i] = ++max_layer;

    node_width = FLOW_GRAPH_MIN_NODE_WIDTH;
    for (i = 0U; i < document->node_count; i++) {
        const int width_i = node_width_for(&document->nodes[i]);
        if (width_i > node_width) node_width = width_i;
    }
    if (node_width > width) node_width = width;
    if (node_width < 8) node_width = 8;
    out_layout->node_width = node_width;
    for (i = 0U; i < document->node_count; i++) {
        const int column = layer[i];
        order[i] = column_count[column]++;
        raw_x[i] = left + column * (node_width + FLOW_GRAPH_GAP_X);
        raw_y[i] = top + order[i] * (FLOW_GRAPH_NODE_HEIGHT + FLOW_GRAPH_GAP_Y);
    }

    if (selected_index < FLOW_MAX_NODES) {
        const int node_left = raw_x[selected_index];
        const int node_right = node_left + node_width;
        const int node_top = raw_y[selected_index];
        const int node_bottom = node_top + FLOW_GRAPH_NODE_HEIGHT;
        if (node_right > left + width) offset_x = node_right - (left + width);
        else if (node_left - offset_x < left) offset_x = node_left - left;
        if (node_bottom > top + height) offset_y = node_bottom - (top + height);
        else if (node_top - offset_y < top) offset_y = node_top - top;
    }
    out_layout->offset_x = offset_x;
    out_layout->offset_y = offset_y;

    for (i = 0U; i < document->node_count; i++) {
        FlowGraphNodeBox *box = &out_layout->nodes[i];
        box->node_index = i;
        box->x = raw_x[i] - offset_x;
        box->y = raw_y[i] - offset_y;
        box->width = node_width;
        box->height = FLOW_GRAPH_NODE_HEIGHT;
        box->layer = layer[i];
        box->order = order[i];
        box->selected = i == selected_index;
    }
    out_layout->node_count = document->node_count;

    for (i = 0U; i < document->edge_count; i++) {
        const FlowEdge *edge = &document->edges[i];
        size_t source = FLOW_MAX_NODES;
        size_t target = FLOW_MAX_NODES;
        size_t j;
        for (j = 0U; j < document->node_count; j++) {
            if (document->nodes[j].id == edge->source_id) source = j;
            if (document->nodes[j].id == edge->target_id) target = j;
        }
        if (source == FLOW_MAX_NODES || target == FLOW_MAX_NODES) continue;
        {
            FlowGraphEdgeRoute *route = &out_layout->edges[out_layout->edge_count++];
            route->edge_index = i;
            route->source_node = source;
            route->target_node = target;
            route->source_x = out_layout->nodes[source].x + node_width;
            route->source_y = out_layout->nodes[source].y +
                              FLOW_GRAPH_NODE_HEIGHT / 2;
            route->target_x = out_layout->nodes[target].x;
            route->target_y = out_layout->nodes[target].y +
                              FLOW_GRAPH_NODE_HEIGHT / 2;
            route->lane_x = route->source_x + 2 + (int)(i % 3U);
        }
    }
    return true;
}

const FlowGraphNodeBox *flow_graph_node_at(const FlowGraphLayout *layout,
                                           int x, int y) {
    size_t i;
    if (!layout) return NULL;
    for (i = 0U; i < layout->node_count; i++) {
        const FlowGraphNodeBox *box = &layout->nodes[i];
        if (x >= box->x && x < box->x + box->width &&
            y >= box->y && y < box->y + box->height)
            return box;
    }
    return NULL;
}

