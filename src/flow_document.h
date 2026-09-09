/** flow_document.h — Authored game-progression graph document. */
#ifndef FLOW_DOCUMENT_H
#define FLOW_DOCUMENT_H

#include "asset_document.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FLOW_DOCUMENT_VERSION 1U
#define FLOW_MAX_NODES 64U
#define FLOW_MAX_EDGES 128U
#define FLOW_NAME_CAPACITY 65U
#define FLOW_PATH_CAPACITY 1024U

typedef uint32_t FlowNodeId;
typedef uint32_t FlowEdgeId;

typedef enum {
    FLOW_NODE_START = 0,
    FLOW_NODE_SCENE,
    FLOW_NODE_MENU
} FlowNodeType;

typedef struct {
    FlowNodeId id;
    FlowNodeType type;
    char asset_name[FLOW_NAME_CAPACITY];
} FlowNode;

typedef struct {
    FlowEdgeId id;
    FlowNodeId source_id;
    char source_port[FLOW_NAME_CAPACITY];
    FlowNodeId target_id;
} FlowEdge;

typedef struct {
    FlowNode nodes[FLOW_MAX_NODES];
    size_t node_count;
    FlowEdge edges[FLOW_MAX_EDGES];
    size_t edge_count;
    FlowNodeId next_node_id;
    FlowEdgeId next_edge_id;
    AssetDocumentState state;
    char path[FLOW_PATH_CAPACITY];
} FlowDocument;

typedef enum {
    FLOW_DOCUMENT_OK = 0,
    FLOW_DOCUMENT_INVALID_ARGUMENT,
    FLOW_DOCUMENT_INVALID_NAME,
    FLOW_DOCUMENT_FULL,
    FLOW_DOCUMENT_ID_EXHAUSTED,
    FLOW_DOCUMENT_DUPLICATE_ID,
    FLOW_DOCUMENT_INVALID_START,
    FLOW_DOCUMENT_MISSING_NODE,
    FLOW_DOCUMENT_INVALID_PORT,
    FLOW_DOCUMENT_DUPLICATE_SOURCE_PORT,
    FLOW_DOCUMENT_UNREACHABLE_NODE,
    FLOW_DOCUMENT_PARSE_ERROR,
    FLOW_DOCUMENT_UNSUPPORTED_VERSION,
    FLOW_DOCUMENT_IO_ERROR
} FlowDocumentResult;

void flow_document_init(FlowDocument *document);
bool flow_document_is_dirty(const FlowDocument *document);
const FlowNode *flow_document_find_node(const FlowDocument *document,
                                        FlowNodeId id);
const FlowEdge *flow_document_find_edge(const FlowDocument *document,
                                        FlowEdgeId id);
FlowDocumentResult flow_document_add_node(FlowDocument *document,
                                          FlowNodeType type,
                                          const char *asset_name,
                                          FlowNodeId *out_id);
FlowDocumentResult flow_document_connect(FlowDocument *document,
                                         FlowNodeId source_id,
                                         const char *source_port,
                                         FlowNodeId target_id,
                                         FlowEdgeId *out_id);
FlowDocumentResult flow_document_set_edge_target(FlowDocument *document,
                                                 FlowEdgeId edge_id,
                                                 FlowNodeId target_id);
FlowDocumentResult flow_document_validate(const FlowDocument *document);
FlowDocumentResult flow_document_load(FlowDocument *document, const char *path);
FlowDocumentResult flow_document_save_as(FlowDocument *document,
                                         const char *path);
FlowDocumentResult flow_document_save(FlowDocument *document);

#endif