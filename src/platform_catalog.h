/** platform_catalog.h — Direct-child enumeration without following links. */
#ifndef PLATFORM_CATALOG_H
#define PLATFORM_CATALOG_H

#include "platform_fs.h"

typedef enum {
    PLATFORM_CATALOG_ENTRY_REGULAR_FILE = 0,
    PLATFORM_CATALOG_ENTRY_DIRECTORY,
    PLATFORM_CATALOG_ENTRY_LINK_OR_REPARSE,
    PLATFORM_CATALOG_ENTRY_OTHER
} PlatformCatalogEntryKind;

typedef struct {
    const char *name;
    PlatformCatalogEntryKind kind;
    uint32_t native_attributes;
} PlatformCatalogEntry;

typedef enum {
    PLATFORM_CATALOG_CALLBACK_CONTINUE = 0,
    PLATFORM_CATALOG_CALLBACK_STOP,
    PLATFORM_CATALOG_CALLBACK_FAILED
} PlatformCatalogCallbackResult;

typedef PlatformCatalogCallbackResult (*PlatformCatalogCallback)(
    const PlatformCatalogEntry *entry, void *context
);

typedef enum {
    PLATFORM_CATALOG_OK = 0,
    PLATFORM_CATALOG_INVALID_ARGUMENT,
    PLATFORM_CATALOG_OPEN_FAILED,
    PLATFORM_CATALOG_READ_FAILED,
    PLATFORM_CATALOG_METADATA_FAILED,
    PLATFORM_CATALOG_PATH_INVALID,
    PLATFORM_CATALOG_OUT_OF_MEMORY,
    PLATFORM_CATALOG_CALLBACK_REJECTED
} PlatformCatalogResult;

PlatformCatalogResult platform_catalog_enumerate(
    const char *root_path, PlatformCatalogCallback callback, void *context,
    PlatformNativeError *error
);

#endif /* PLATFORM_CATALOG_H */
