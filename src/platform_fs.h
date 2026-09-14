/** platform_fs.h — Typed native filesystem primitives and commit state. */
#ifndef PLATFORM_FS_H
#define PLATFORM_FS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    PLATFORM_FS_OK = 0,
    PLATFORM_FS_INVALID_ARGUMENT,
    PLATFORM_FS_NOT_FOUND,
    PLATFORM_FS_ALREADY_EXISTS,
    PLATFORM_FS_WRONG_TYPE,
    PLATFORM_FS_PATH_INVALID,
    PLATFORM_FS_ACCESS_DENIED,
    PLATFORM_FS_UNSUPPORTED,
    PLATFORM_FS_IO_ERROR
} PlatformFsResult;

typedef enum {
    PLATFORM_NATIVE_ERROR_NONE = 0,
    PLATFORM_NATIVE_ERROR_ERRNO,
    PLATFORM_NATIVE_ERROR_WIN32
} PlatformNativeErrorDomain;

typedef struct {
    PlatformNativeErrorDomain domain;
    uint32_t code;
} PlatformNativeError;

typedef enum {
    PLATFORM_COMMIT_NOT_COMMITTED = 0,
    PLATFORM_COMMIT_COMMITTED,
    PLATFORM_COMMIT_COMMITTED_DURABILITY_WARNING
} PlatformCommitState;

typedef struct {
    bool is_regular_file;
    bool is_directory;
    bool is_link_or_reparse;
    uint32_t permissions;
    uint32_t native_attributes;
} PlatformFileMetadata;

typedef struct {
    PlatformFsResult result;
    PlatformCommitState commit_state;
    PlatformNativeError error;
} PlatformReplaceResult;

PlatformFsResult platform_fs_inspect_nofollow(const char *path,
                                              PlatformFileMetadata *out,
                                              PlatformNativeError *error);
PlatformFsResult platform_fs_apply_metadata(FILE *file,
                                            const PlatformFileMetadata *metadata,
                                            PlatformNativeError *error);
PlatformFsResult platform_fs_sync_file(FILE *file, PlatformNativeError *error);
PlatformReplaceResult platform_fs_replace(const char *temporary_path,
                                          const char *destination_path,
                                          bool destination_exists);

#endif
