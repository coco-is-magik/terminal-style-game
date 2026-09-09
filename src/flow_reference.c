#include "flow_reference.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

static bool valid_name(const char *name) {
    size_t length;
    size_t i;
    if (!name || name[0] == '\0') return false;
    length = strlen(name);
    if (length >= FLOW_NAME_CAPACITY) return false;
    for (i = 0U; i < length; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!(isalnum(c) || c == '_' || c == '-')) return false;
    }
    return true;
}

FlowReferenceResult flow_reference_validate_catalog(
    const FlowReferenceCatalog *catalog
) {
    size_t i;
    size_t j;
    if (!catalog || catalog->count > FLOW_REFERENCE_MAX_ASSETS ||
        (catalog->count > 0U && !catalog->entries))
        return FLOW_REFERENCE_INVALID_ARGUMENT;
    for (i = 0U; i < catalog->count; i++) {
        const FlowReferenceEntry *entry = &catalog->entries[i];
        size_t port;
        if ((entry->type != FLOW_NODE_SCENE && entry->type != FLOW_NODE_MENU) ||
            !valid_name(entry->name) || entry->port_count > FLOW_REFERENCE_MAX_PORTS ||
            (entry->port_count > 0U && !entry->ports))
            return FLOW_REFERENCE_INVALID_ENTRY;
        for (j = 0U; j < i; j++)
            if (catalog->entries[j].type == entry->type &&
                strcmp(catalog->entries[j].name, entry->name) == 0)
                return FLOW_REFERENCE_DUPLICATE_ASSET;
        for (port = 0U; port < entry->port_count; port++) {
            size_t prior;
            if (!valid_name(entry->ports[port])) return FLOW_REFERENCE_INVALID_ENTRY;
            for (prior = 0U; prior < port; prior++)
                if (strcmp(entry->ports[prior], entry->ports[port]) == 0)
                    return FLOW_REFERENCE_DUPLICATE_PORT;
        }
    }
    return FLOW_REFERENCE_OK;
}

const FlowReferenceEntry *flow_reference_find(
    const FlowReferenceCatalog *catalog,
    FlowNodeType type,
    const char *name
) {
    size_t i;
    if (!catalog || !name || !catalog->entries) return NULL;
    for (i = 0U; i < catalog->count; i++)
        if (catalog->entries[i].type == type && catalog->entries[i].name &&
            strcmp(catalog->entries[i].name, name) == 0)
            return &catalog->entries[i];
    return NULL;
}

static bool entry_has_port(const FlowReferenceEntry *entry, const char *port) {
    size_t i;
    if (!entry || !port) return false;
    for (i = 0U; i < entry->port_count; i++)
        if (strcmp(entry->ports[i], port) == 0) return true;
    return false;
}

FlowReferenceResult flow_reference_validate_document(
    const FlowDocument *document,
    const FlowReferenceCatalog *catalog
) {
    size_t i;
    FlowReferenceResult catalog_result;
    if (!document || !catalog) return FLOW_REFERENCE_INVALID_ARGUMENT;
    if (flow_document_validate(document) != FLOW_DOCUMENT_OK)
        return FLOW_REFERENCE_INVALID_GRAPH;
    catalog_result = flow_reference_validate_catalog(catalog);
    if (catalog_result != FLOW_REFERENCE_OK) return catalog_result;
    for (i = 0U; i < document->node_count; i++) {
        const FlowNode *node = &document->nodes[i];
        const FlowReferenceEntry *entry;
        size_t candidate;
        bool other_type = false;
        if (node->type == FLOW_NODE_START) continue;
        entry = flow_reference_find(catalog, node->type, node->asset_name);
        if (!entry) {
            for (candidate = 0U; candidate < catalog->count; candidate++)
                if (strcmp(catalog->entries[candidate].name, node->asset_name) == 0)
                    other_type = true;
            return other_type ? FLOW_REFERENCE_TYPE_MISMATCH
                              : FLOW_REFERENCE_MISSING_ASSET;
        }
        for (candidate = 0U; candidate < document->edge_count; candidate++)
            if (document->edges[candidate].source_id == node->id &&
                !entry_has_port(entry, document->edges[candidate].source_port))
                return FLOW_REFERENCE_MISSING_PORT;
    }
    return FLOW_REFERENCE_OK;
}