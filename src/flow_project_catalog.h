/** flow_project_catalog.h — Owning catalog of authored project flow assets. */
#ifndef FLOW_PROJECT_CATALOG_H
#define FLOW_PROJECT_CATALOG_H

#include "flow_reference.h"
#include "assets.h"

typedef struct {
    FlowReferenceEntry entry;
    char name[FLOW_NAME_CAPACITY];
    char port_names[FLOW_REFERENCE_MAX_PORTS][FLOW_NAME_CAPACITY];
    const char *ports[FLOW_REFERENCE_MAX_PORTS];
} FlowProjectCatalogEntry;

typedef struct {
    FlowProjectCatalogEntry entries[FLOW_REFERENCE_MAX_ASSETS];
    FlowReferenceEntry reference_entries[FLOW_REFERENCE_MAX_ASSETS];
    size_t count;
    FlowReferenceCatalog reference_catalog;
} FlowProjectCatalog;

typedef enum {
    FLOW_PROJECT_CATALOG_OK = 0,
    FLOW_PROJECT_CATALOG_INVALID_ARGUMENT,
    FLOW_PROJECT_CATALOG_SCAN_FAILED,
    FLOW_PROJECT_CATALOG_INVALID_ASSET,
    FLOW_PROJECT_CATALOG_DUPLICATE_ASSET,
    FLOW_PROJECT_CATALOG_FULL
} FlowProjectCatalogResult;

void flow_project_catalog_init(FlowProjectCatalog *catalog);
FlowProjectCatalogResult flow_project_catalog_refresh(
    FlowProjectCatalog *catalog,
    const char *asset_root,
    const AssetRegistry *assets
);
FlowProjectCatalogResult flow_project_catalog_overlay_entry(
    FlowProjectCatalog *catalog,
    const FlowReferenceEntry *entry
);
const FlowReferenceEntry *flow_project_catalog_get(
    const FlowProjectCatalog *catalog,
    size_t index
);
const FlowReferenceCatalog *flow_project_catalog_reference(
    const FlowProjectCatalog *catalog
);

#endif /* FLOW_PROJECT_CATALOG_H */