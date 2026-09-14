/** platform_fs_internal.h — Explicit fault seams for focused platform tests. */
#ifndef PLATFORM_FS_INTERNAL_H
#define PLATFORM_FS_INTERNAL_H

#include "platform_fs.h"

typedef enum {
    PLATFORM_FS_FAULT_NONE = 0,
    PLATFORM_FS_FAULT_SYNC_FILE,
    PLATFORM_FS_FAULT_INSPECT,
    PLATFORM_FS_FAULT_APPLY_METADATA,
    PLATFORM_FS_FAULT_REPLACE,
    PLATFORM_FS_FAULT_DURABILITY
} PlatformFsFault;

PlatformFsResult platform_fs_internal_inspect_nofollow(
    const char *path, PlatformFileMetadata *out, PlatformNativeError *error,
    PlatformFsFault fault
);
PlatformFsResult platform_fs_internal_apply_metadata(
    FILE *file, const PlatformFileMetadata *metadata, PlatformNativeError *error,
    PlatformFsFault fault
);
PlatformFsResult platform_fs_internal_sync_file(
    FILE *file, PlatformNativeError *error, PlatformFsFault fault
);
PlatformReplaceResult platform_fs_internal_replace(
    const char *temporary_path, const char *destination_path,
    bool destination_exists, PlatformFsFault fault
);

#endif
