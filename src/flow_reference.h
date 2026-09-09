/** flow_reference.h — Borrowed typed asset/port validation for FlowDocument. */
#ifndef FLOW_REFERENCE_H
#define FLOW_REFERENCE_H

#include "flow_document.h"

#include <stddef.h>

#define FLOW_REFERENCE_MAX_ASSETS FLOW_MAX_NODES
#define FLOW_REFERENCE_MAX_PORTS 16U

typedef struct {
    FlowNodeType type;
    const char *name;
    const char *const *ports;
    size_t port_count;
} FlowReferenceEntry;

typedef struct {
    const FlowReferenceEntry *entries;
    size_t count;
} FlowReferenceCatalog;

typedef enum {
    FLOW_REFERENCE_OK = 0,
    FLOW_REFERENCE_INVALID_ARGUMENT,
    FLOW_REFERENCE_INVALID_GRAPH,
    FLOW_REFERENCE_INVALID_ENTRY,
    FLOW_REFERENCE_DUPLICATE_ASSET,
    FLOW_REFERENCE_DUPLICATE_PORT,
    FLOW_REFERENCE_MISSING_ASSET,
    FLOW_REFERENCE_TYPE_MISMATCH,
    FLOW_REFERENCE_MISSING_PORT
} FlowReferenceResult;

FlowReferenceResult flow_reference_validate_catalog(
    const FlowReferenceCatalog *catalog
);
const FlowReferenceEntry *flow_reference_find(
    const FlowReferenceCatalog *catalog,
    FlowNodeType type,
    const char *name
);
FlowReferenceResult flow_reference_validate_document(
    const FlowDocument *document,
    const FlowReferenceCatalog *catalog
);

#endif