/** map_catalog_internal.h — Focused platform-enumeration fault seam. */
#ifndef MAP_CATALOG_INTERNAL_H
#define MAP_CATALOG_INTERNAL_H

#include "map_catalog.h"
#include "platform_catalog_internal.h"

MapCatalogResult map_catalog_internal_refresh_extension(
    MapCatalog *catalog, const char *root_path, const char *extension,
    PlatformCatalogFault fault
);

#endif /* MAP_CATALOG_INTERNAL_H */
