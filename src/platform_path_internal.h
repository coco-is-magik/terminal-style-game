/** platform_path_internal.h — Explicit fault seams for focused platform tests. */
#ifndef PLATFORM_PATH_INTERNAL_H
#define PLATFORM_PATH_INTERNAL_H

#include "platform_path.h"

typedef enum {
    PLATFORM_PATH_FAULT_NONE = 0,
    PLATFORM_PATH_FAULT_ALLOCATION,
    PLATFORM_PATH_FAULT_CONVERSION
} PlatformPathFault;

PlatformPathResult platform_path_internal_from_utf8(
    const char *text, size_t length, PlatformNativePath *out,
    PlatformPathFault fault
);
PlatformPathResult platform_path_internal_to_utf8(
    const PlatformNativePath *path, char **out_text, size_t *out_length,
    PlatformPathFault fault
);

#endif
