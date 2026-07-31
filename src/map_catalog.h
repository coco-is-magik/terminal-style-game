/**
 * map_catalog.h — Deterministic discovery of current digit-grid map files
 *
 * MapCatalog owns display names and full paths for regular .txt files directly
 * under one borrowed root path. It does not recurse, follow symlinks, or load
 * map contents.
 */

#ifndef MAP_CATALOG_H
#define MAP_CATALOG_H

#include <stddef.h>

typedef struct {
    char *name;
    char *path;
} MapCatalogEntry;

typedef struct {
    MapCatalogEntry *entries;
    size_t count;
} MapCatalog;

typedef enum {
    MAP_CATALOG_OK = 0,
    MAP_CATALOG_INVALID_ARGUMENT,
    MAP_CATALOG_OPEN_FAILED,
    MAP_CATALOG_READ_FAILED,
    MAP_CATALOG_METADATA_FAILED,
    MAP_CATALOG_PATH_TOO_LONG,
    MAP_CATALOG_OUT_OF_MEMORY
} MapCatalogResult;

void map_catalog_init(MapCatalog *catalog);
void map_catalog_clear(MapCatalog *catalog);

MapCatalogResult map_catalog_refresh(MapCatalog *catalog, const char *root_path);
MapCatalogResult map_catalog_refresh_extension(MapCatalog *catalog,
                                               const char *root_path,
                                               const char *extension);
MapCatalogResult map_catalog_refresh_native(MapCatalog *catalog,
                                            const char *root_path);

const MapCatalogEntry *map_catalog_get(
    const MapCatalog *catalog,
    size_t index
);

#endif /* MAP_CATALOG_H */
