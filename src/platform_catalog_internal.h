/** platform_catalog_internal.h — Explicit per-call enumeration fault seam. */
#ifndef PLATFORM_CATALOG_INTERNAL_H
#define PLATFORM_CATALOG_INTERNAL_H

#include "platform_catalog.h"

typedef enum {
    PLATFORM_CATALOG_FAULT_NONE = 0,
    PLATFORM_CATALOG_FAULT_OPEN,
    PLATFORM_CATALOG_FAULT_READ,
    PLATFORM_CATALOG_FAULT_METADATA,
    PLATFORM_CATALOG_FAULT_NAME_CONVERSION
} PlatformCatalogFault;

PlatformCatalogResult platform_catalog_internal_enumerate(
    const char *root_path, PlatformCatalogCallback callback, void *context,
    PlatformNativeError *error, PlatformCatalogFault fault
);

#endif /* PLATFORM_CATALOG_INTERNAL_H */
