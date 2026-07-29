/**
 * map_catalog.c — Bounded current-map directory catalog
 *
 * Refresh is transactional: a complete candidate catalog is built and sorted
 * before replacing the caller's live catalog.
 */

#define _POSIX_C_SOURCE 200809L

#include "map_catalog.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

static bool catalog_name_is_txt(const char *name) {
    size_t len;

    if (!name) return false;
    len = strlen(name);
    return len > 4 && strcmp(name + len - 4, ".txt") == 0;
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

MapCatalogResult map_catalog_refresh(MapCatalog *catalog, const char *root_path) {
    MapCatalog candidate;
    DIR *directory;
    struct dirent *entry;
    MapCatalogResult result = MAP_CATALOG_OK;
    int directory_fd;

    if (!catalog || !root_path || root_path[0] == '\0') {
        return MAP_CATALOG_INVALID_ARGUMENT;
    }

    directory = opendir(root_path);
    if (!directory) return MAP_CATALOG_OPEN_FAILED;
    directory_fd = dirfd(directory);
    if (directory_fd < 0) {
        closedir(directory);
        return MAP_CATALOG_OPEN_FAILED;
    }

    map_catalog_init(&candidate);
    errno = 0;
    while ((entry = readdir(directory)) != NULL) {
        struct stat metadata;

        if (!catalog_name_is_txt(entry->d_name)) continue;
        if (fstatat(directory_fd, entry->d_name, &metadata,
                    AT_SYMLINK_NOFOLLOW) != 0) {
            result = MAP_CATALOG_METADATA_FAILED;
            break;
        }
        if (!S_ISREG(metadata.st_mode)) continue;
        result = catalog_append(&candidate, root_path, entry->d_name);
        if (result != MAP_CATALOG_OK) break;
        errno = 0;
    }
    if (result == MAP_CATALOG_OK && errno != 0) {
        result = MAP_CATALOG_READ_FAILED;
    }
    if (closedir(directory) != 0 && result == MAP_CATALOG_OK) {
        result = MAP_CATALOG_READ_FAILED;
    }
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

const MapCatalogEntry *map_catalog_get(
    const MapCatalog *catalog,
    size_t index
) {
    if (!catalog || index >= catalog->count) return NULL;
    return &catalog->entries[index];
}
