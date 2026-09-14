/**
 * map_catalog.c — Bounded current-map directory catalog
 *
 * Refresh is transactional: a complete candidate catalog is built and sorted
 * before replacing the caller's live catalog.
 */

#include "map_catalog.h"
#include "map_catalog_internal.h"
#include "platform_catalog.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char *catalog_duplicate(const char *text) {
    size_t len;
    char *copy;

    if (!text) return NULL;
    len = strlen(text);
    if (len == SIZE_MAX) return NULL;
    copy = malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, text, len + 1);
    return copy;
}

static bool catalog_name_has_extension(const char *name, const char *extension) {
    size_t len;
    size_t extension_len;

    if (!name || !extension || extension[0] != '.') return false;
    len = strlen(name);
    extension_len = strlen(extension);
    return len > extension_len &&
           strcmp(name + len - extension_len, extension) == 0;
}

static MapCatalogResult catalog_join_path(
    const char *root,
    const char *name,
    char **out_path
) {
    size_t root_len;
    size_t name_len;
    bool needs_slash;
    size_t total;
    char *path;

    if (!root || !name || !out_path) return MAP_CATALOG_INVALID_ARGUMENT;
    root_len = strlen(root);
    name_len = strlen(name);
    needs_slash = root_len > 0 && root[root_len - 1] != '/';
    if (root_len > SIZE_MAX - name_len - 2) return MAP_CATALOG_PATH_TOO_LONG;
    total = root_len + (needs_slash ? 1U : 0U) + name_len + 1U;
    path = malloc(total);
    if (!path) return MAP_CATALOG_OUT_OF_MEMORY;

    memcpy(path, root, root_len);
    if (needs_slash) path[root_len++] = '/';
    memcpy(path + root_len, name, name_len + 1);
    *out_path = path;
    return MAP_CATALOG_OK;
}

static MapCatalogResult catalog_append(
    MapCatalog *catalog,
    const char *root,
    const char *name
) {
    MapCatalogEntry *grown;
    char *name_copy;
    char *path = NULL;
    MapCatalogResult result;

    if (catalog->count == SIZE_MAX / sizeof(*catalog->entries)) {
        return MAP_CATALOG_OUT_OF_MEMORY;
    }
    name_copy = catalog_duplicate(name);
    if (!name_copy) return MAP_CATALOG_OUT_OF_MEMORY;
    result = catalog_join_path(root, name, &path);
    if (result != MAP_CATALOG_OK) {
        free(name_copy);
        return result;
    }

    grown = realloc(catalog->entries,
                    (catalog->count + 1) * sizeof(*catalog->entries));
    if (!grown) {
        free(path);
        free(name_copy);
        return MAP_CATALOG_OUT_OF_MEMORY;
    }
    catalog->entries = grown;
    catalog->entries[catalog->count].name = name_copy;
    catalog->entries[catalog->count].path = path;
    catalog->count++;
    return MAP_CATALOG_OK;
}

static int catalog_entry_compare(const void *left, const void *right) {
    const MapCatalogEntry *a = left;
    const MapCatalogEntry *b = right;
    return strcmp(a->name, b->name);
}

void map_catalog_init(MapCatalog *catalog) {
    if (!catalog) return;
    catalog->entries = NULL;
    catalog->count = 0;
}

void map_catalog_clear(MapCatalog *catalog) {
    size_t i;

    if (!catalog) return;
    for (i = 0; i < catalog->count; i++) {
        free(catalog->entries[i].name);
        free(catalog->entries[i].path);
    }
    free(catalog->entries);
    map_catalog_init(catalog);
}

typedef struct {
    MapCatalog *candidate;
    const char *root_path;
    const char *extension;
    MapCatalogResult result;
} MapCatalogBuildContext;

static PlatformCatalogCallbackResult catalog_collect(
    const PlatformCatalogEntry *entry, void *context_ptr
) {
    MapCatalogBuildContext *context = context_ptr;
    if (!entry || !context) return PLATFORM_CATALOG_CALLBACK_FAILED;
    if (entry->kind != PLATFORM_CATALOG_ENTRY_REGULAR_FILE ||
        !catalog_name_has_extension(entry->name, context->extension)) {
        return PLATFORM_CATALOG_CALLBACK_CONTINUE;
    }
    context->result = catalog_append(
        context->candidate, context->root_path, entry->name);
    return context->result == MAP_CATALOG_OK
        ? PLATFORM_CATALOG_CALLBACK_CONTINUE
        : PLATFORM_CATALOG_CALLBACK_FAILED;
}

static MapCatalogResult catalog_platform_result(PlatformCatalogResult result) {
    switch (result) {
        case PLATFORM_CATALOG_OK: return MAP_CATALOG_OK;
        case PLATFORM_CATALOG_INVALID_ARGUMENT:
            return MAP_CATALOG_INVALID_ARGUMENT;
        case PLATFORM_CATALOG_OPEN_FAILED: return MAP_CATALOG_OPEN_FAILED;
        case PLATFORM_CATALOG_READ_FAILED: return MAP_CATALOG_READ_FAILED;
        case PLATFORM_CATALOG_METADATA_FAILED:
            return MAP_CATALOG_METADATA_FAILED;
        case PLATFORM_CATALOG_PATH_INVALID:
            return MAP_CATALOG_PATH_INVALID;
        case PLATFORM_CATALOG_OUT_OF_MEMORY:
            return MAP_CATALOG_OUT_OF_MEMORY;
        case PLATFORM_CATALOG_CALLBACK_REJECTED:
            return MAP_CATALOG_OUT_OF_MEMORY;
    }
    return MAP_CATALOG_READ_FAILED;
}

MapCatalogResult map_catalog_internal_refresh_extension(
    MapCatalog *catalog, const char *root_path, const char *extension,
    PlatformCatalogFault fault
) {
    MapCatalog candidate;
    MapCatalogBuildContext context;
    PlatformNativeError error;
    PlatformCatalogResult platform_result;
    MapCatalogResult result;

    if (!catalog || !root_path || root_path[0] == '\0' || !extension ||
        extension[0] != '.' || extension[1] == '\0') {
        return MAP_CATALOG_INVALID_ARGUMENT;
    }

    map_catalog_init(&candidate);
    context.candidate = &candidate;
    context.root_path = root_path;
    context.extension = extension;
    context.result = MAP_CATALOG_OK;
    platform_result = platform_catalog_internal_enumerate(
        root_path, catalog_collect, &context, &error, fault);
    result = platform_result == PLATFORM_CATALOG_CALLBACK_REJECTED
        ? context.result : catalog_platform_result(platform_result);
    if (result != MAP_CATALOG_OK) {
        map_catalog_clear(&candidate);
        return result;
    }

    if (candidate.count > 1) {
        qsort(candidate.entries, candidate.count, sizeof(*candidate.entries),
              catalog_entry_compare);
    }
    map_catalog_clear(catalog);
    *catalog = candidate;
    return MAP_CATALOG_OK;
}

MapCatalogResult map_catalog_refresh_extension(MapCatalog *catalog,
                                               const char *root_path,
                                               const char *extension) {
    return map_catalog_internal_refresh_extension(
        catalog, root_path, extension, PLATFORM_CATALOG_FAULT_NONE);
}

MapCatalogResult map_catalog_refresh(MapCatalog *catalog, const char *root_path) {
    return map_catalog_refresh_extension(catalog, root_path, ".txt");
}

MapCatalogResult map_catalog_refresh_native(MapCatalog *catalog,
                                            const char *root_path) {
    return map_catalog_refresh_extension(catalog, root_path, ".tscene");
}

const MapCatalogEntry *map_catalog_get(
    const MapCatalog *catalog,
    size_t index
) {
    if (!catalog || index >= catalog->count) return NULL;
    return &catalog->entries[index];
}
