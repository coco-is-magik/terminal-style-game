#define _POSIX_C_SOURCE 200809L

#include "flow_project_catalog.h"

#include "map_catalog.h"
#include "scene_flow_adapter.h"
#include "ui_document.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void catalog_rebind(FlowProjectCatalog *catalog) {
    size_t i;
    size_t port;
    for (i = 0U; i < catalog->count; i++) {
        FlowProjectCatalogEntry *owned = &catalog->entries[i];
        owned->entry.name = owned->name;
        for (port = 0U; port < owned->entry.port_count; port++)
            owned->ports[port] = owned->port_names[port];
        owned->entry.ports = owned->ports;
        catalog->reference_entries[i] = owned->entry;
    }
    catalog->reference_catalog.entries = catalog->reference_entries;
    catalog->reference_catalog.count = catalog->count;
}

void flow_project_catalog_init(FlowProjectCatalog *catalog) {
    if (!catalog) return;
    memset(catalog, 0, sizeof(*catalog));
    catalog_rebind(catalog);
}

static FlowProjectCatalogResult append_entry(
    FlowProjectCatalog *catalog,
    const FlowReferenceEntry *entry
) {
    FlowProjectCatalogEntry *owned;
    size_t i;
    if (catalog->count >= FLOW_REFERENCE_MAX_ASSETS)
        return FLOW_PROJECT_CATALOG_FULL;
    owned = &catalog->entries[catalog->count];
    memset(owned, 0, sizeof(*owned));
    owned->entry.type = entry->type;
    owned->entry.port_count = entry->port_count;
    if (snprintf(owned->name, sizeof(owned->name), "%s", entry->name) >=
        (int)sizeof(owned->name)) return FLOW_PROJECT_CATALOG_INVALID_ASSET;
    for (i = 0U; i < entry->port_count; i++)
        if (snprintf(owned->port_names[i], sizeof(owned->port_names[i]), "%s",
                     entry->ports[i]) >= (int)sizeof(owned->port_names[i]))
            return FLOW_PROJECT_CATALOG_INVALID_ASSET;
    catalog->count++;
    catalog_rebind(catalog);
    return FLOW_PROJECT_CATALOG_OK;
}

static FlowProjectCatalogResult load_scenes(FlowProjectCatalog *candidate,
                                            const MapCatalog *files,
                                            const AssetRegistry *assets) {
    size_t i;
    for (i = 0U; i < files->count; i++) {
        SceneDocument document;
        SceneFlowReferenceView view;
        SceneDiagnostic diagnostic;
        SceneLoadResult load_result;
        SceneFlowAdapterResult adapter_result = SCENE_FLOW_ADAPTER_INVALID_SCENE;
        FlowProjectCatalogResult result;
        scene_document_init(&document);
        load_result = scene_document_load_native_with_assets(
            &document, files->entries[i].path, assets, &diagnostic);
        if (load_result == SCENE_LOAD_OK)
            adapter_result = scene_flow_reference_view_build(&document, &view);
        if (load_result != SCENE_LOAD_OK || adapter_result != SCENE_FLOW_ADAPTER_OK) {
            scene_document_destroy(&document);
            return FLOW_PROJECT_CATALOG_INVALID_ASSET;
        }
        result = append_entry(candidate, &view.entry);
        scene_document_destroy(&document);
        if (result != FLOW_PROJECT_CATALOG_OK) return result;
    }
    return FLOW_PROJECT_CATALOG_OK;
}

static FlowProjectCatalogResult load_menus(FlowProjectCatalog *candidate,
                                           const MapCatalog *files) {
    size_t i;
    for (i = 0U; i < files->count; i++) {
        UiDocument document;
        UiFlowReferenceView view;
        FlowProjectCatalogResult result;
        ui_document_init(&document);
        if (ui_document_load(&document, files->entries[i].path) != UI_DOCUMENT_OK ||
            ui_document_build_flow_reference(&document, &view) != UI_DOCUMENT_OK)
            return FLOW_PROJECT_CATALOG_INVALID_ASSET;
        result = append_entry(candidate, &view.entry);
        if (result != FLOW_PROJECT_CATALOG_OK) return result;
    }
    return FLOW_PROJECT_CATALOG_OK;
}

static FlowProjectCatalogResult scan_optional(MapCatalog *files, const char *path,
                                              const char *extension) {
    struct stat metadata;
    if (stat(path, &metadata) != 0) {
        return errno == ENOENT ? FLOW_PROJECT_CATALOG_OK
                               : FLOW_PROJECT_CATALOG_SCAN_FAILED;
    }
    if (!S_ISDIR(metadata.st_mode)) return FLOW_PROJECT_CATALOG_SCAN_FAILED;
    return map_catalog_refresh_extension(files, path, extension) == MAP_CATALOG_OK
        ? FLOW_PROJECT_CATALOG_OK : FLOW_PROJECT_CATALOG_SCAN_FAILED;
}

static int entry_compare(const void *left, const void *right) {
    const FlowProjectCatalogEntry *a = left;
    const FlowProjectCatalogEntry *b = right;
    if (a->entry.type != b->entry.type)
        return a->entry.type < b->entry.type ? -1 : 1;
    return strcmp(a->name, b->name);
}

FlowProjectCatalogResult flow_project_catalog_refresh(
    FlowProjectCatalog *catalog,
    const char *asset_root,
    const AssetRegistry *assets
) {
    FlowProjectCatalog candidate;
    MapCatalog files;
    char path[FLOW_PATH_CAPACITY];
    FlowProjectCatalogResult result;
    if (!catalog || !asset_root || asset_root[0] == '\0' || !assets)
        return FLOW_PROJECT_CATALOG_INVALID_ARGUMENT;
    flow_project_catalog_init(&candidate);
    map_catalog_init(&files);
    if (snprintf(path, sizeof(path), "%s/scenes", asset_root) >= (int)sizeof(path))
        return FLOW_PROJECT_CATALOG_INVALID_ARGUMENT;
    result = scan_optional(&files, path, ".tscene");
    if (result == FLOW_PROJECT_CATALOG_OK)
        result = load_scenes(&candidate, &files, assets);
    map_catalog_clear(&files);
    if (result != FLOW_PROJECT_CATALOG_OK) return result;
    if (snprintf(path, sizeof(path), "%s/menus", asset_root) >= (int)sizeof(path))
        return FLOW_PROJECT_CATALOG_INVALID_ARGUMENT;
    result = scan_optional(&files, path, ".tui");
    if (result == FLOW_PROJECT_CATALOG_OK) result = load_menus(&candidate, &files);
    map_catalog_clear(&files);
    if (result != FLOW_PROJECT_CATALOG_OK) return result;
    if (candidate.count > 1U)
        qsort(candidate.entries, candidate.count, sizeof(candidate.entries[0]),
              entry_compare);
    catalog_rebind(&candidate);
    if (flow_reference_validate_catalog(&candidate.reference_catalog) !=
        FLOW_REFERENCE_OK) return FLOW_PROJECT_CATALOG_DUPLICATE_ASSET;
    *catalog = candidate;
    catalog_rebind(catalog);
    return FLOW_PROJECT_CATALOG_OK;
}

FlowProjectCatalogResult flow_project_catalog_overlay_entry(
    FlowProjectCatalog *catalog,
    const FlowReferenceEntry *entry
) {
    FlowProjectCatalog candidate;
    FlowProjectCatalogResult result;
    size_t i;
    if (!catalog || !entry ||
        flow_reference_validate_catalog(&(FlowReferenceCatalog){entry, 1U}) !=
            FLOW_REFERENCE_OK)
        return FLOW_PROJECT_CATALOG_INVALID_ARGUMENT;
    candidate = *catalog;
    catalog_rebind(&candidate);
    for (i = 0U; i < candidate.count; i++)
        if (candidate.entries[i].entry.type == entry->type &&
            strcmp(candidate.entries[i].name, entry->name) == 0) {
            memset(&candidate.entries[i], 0, sizeof(candidate.entries[i]));
            candidate.count--;
            if (i < candidate.count)
                memmove(&candidate.entries[i], &candidate.entries[i + 1U],
                        (candidate.count - i) * sizeof(candidate.entries[0]));
            break;
        }
    result = append_entry(&candidate, entry);
    if (result != FLOW_PROJECT_CATALOG_OK) return result;
    if (candidate.count > 1U)
        qsort(candidate.entries, candidate.count, sizeof(candidate.entries[0]),
              entry_compare);
    catalog_rebind(&candidate);
    if (flow_reference_validate_catalog(&candidate.reference_catalog) != FLOW_REFERENCE_OK)
        return FLOW_PROJECT_CATALOG_DUPLICATE_ASSET;
    *catalog = candidate;
    catalog_rebind(catalog);
    return FLOW_PROJECT_CATALOG_OK;
}

const FlowReferenceEntry *flow_project_catalog_get(
    const FlowProjectCatalog *catalog,
    size_t index
) {
    if (!catalog || index >= catalog->count) return NULL;
    return &catalog->reference_entries[index];
}

const FlowReferenceCatalog *flow_project_catalog_reference(
    const FlowProjectCatalog *catalog
) {
    return catalog ? &catalog->reference_catalog : NULL;
}