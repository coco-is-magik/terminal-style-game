#define _POSIX_C_SOURCE 200809L

#include "platform_fs.h"
#include "platform_fs_internal.h"
#include "platform_path.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static void clear_error(PlatformNativeError *error) {
    if (!error) return;
    error->domain = PLATFORM_NATIVE_ERROR_NONE;
    error->code = 0U;
}


static PlatformFsResult invalid_argument(PlatformNativeError *error) {
    clear_error(error);
    return PLATFORM_FS_INVALID_ARGUMENT;
}

#ifdef _WIN32
static PlatformFsResult from_win32(DWORD code, PlatformNativeError *error) {
    if (error) {
        error->domain = PLATFORM_NATIVE_ERROR_WIN32;
        error->code = (uint32_t)code;
    }
    switch (code) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            return PLATFORM_FS_NOT_FOUND;
        case ERROR_FILE_EXISTS:
        case ERROR_ALREADY_EXISTS:
            return PLATFORM_FS_ALREADY_EXISTS;
        case ERROR_ACCESS_DENIED:
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
        case ERROR_WRITE_PROTECT:
            return PLATFORM_FS_ACCESS_DENIED;
        case ERROR_INVALID_NAME:
        case ERROR_BAD_PATHNAME:
        case ERROR_FILENAME_EXCED_RANGE:
            return PLATFORM_FS_PATH_INVALID;
        case ERROR_NOT_SUPPORTED:
        case ERROR_INVALID_FUNCTION:
            return PLATFORM_FS_UNSUPPORTED;
        default:
            return PLATFORM_FS_IO_ERROR;
    }
}

static PlatformFsResult native_path(const char *path, PlatformNativePath *out,
                                    PlatformNativeError *error) {
    PlatformPathResult result;
    if (!path || path[0] == '\0') return invalid_argument(error);
    result = platform_path_from_utf8(path, strlen(path), out);
    if (result == PLATFORM_PATH_INVALID_UTF8 || result == PLATFORM_PATH_TOO_LONG ||
        result == PLATFORM_PATH_CONVERSION_FAILED) {
        clear_error(error);
        return PLATFORM_FS_PATH_INVALID;
    }
    if (result != PLATFORM_PATH_OK) {
        clear_error(error);
        return result == PLATFORM_PATH_INVALID_ARGUMENT
            ? PLATFORM_FS_INVALID_ARGUMENT : PLATFORM_FS_IO_ERROR;
    }
    return PLATFORM_FS_OK;
}
#else
static PlatformFsResult from_errno(int code, PlatformNativeError *error) {
    if (error) {
        error->domain = PLATFORM_NATIVE_ERROR_ERRNO;
        error->code = (uint32_t)code;
    }
    switch (code) {
        case ENOENT: return PLATFORM_FS_NOT_FOUND;
        case EEXIST: return PLATFORM_FS_ALREADY_EXISTS;
        case EACCES:
        case EPERM:
        case EROFS: return PLATFORM_FS_ACCESS_DENIED;
        case ENAMETOOLONG:
        case EINVAL: return PLATFORM_FS_PATH_INVALID;
#ifdef ENOTSUP
        case ENOTSUP: return PLATFORM_FS_UNSUPPORTED;
#endif
        default: return PLATFORM_FS_IO_ERROR;
    }
}
#endif

PlatformFsResult platform_fs_internal_inspect_nofollow(
    const char *path, PlatformFileMetadata *out, PlatformNativeError *error,
    PlatformFsFault fault
) {
    PlatformFileMetadata candidate;
    if (!path || path[0] == '\0' || !out) return invalid_argument(error);
    clear_error(error);
    if (fault == PLATFORM_FS_FAULT_INSPECT) {
#ifdef _WIN32
        return from_win32(ERROR_ACCESS_DENIED, error);
#else
        return from_errno(EIO, error);
#endif
    }
    memset(&candidate, 0, sizeof(candidate));
#ifdef _WIN32
    {
        PlatformNativePath native;
        DWORD attributes;
        PlatformFsResult path_result;
        platform_native_path_init(&native);
        path_result = native_path(path, &native, error);
        if (path_result != PLATFORM_FS_OK) return path_result;
        attributes = GetFileAttributesW(native.data);
        platform_native_path_destroy(&native);
        if (attributes == INVALID_FILE_ATTRIBUTES) return from_win32(GetLastError(), error);
        candidate.native_attributes = (uint32_t)attributes;
        candidate.is_link_or_reparse =
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
        candidate.is_directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U &&
                                 !candidate.is_link_or_reparse;
        candidate.is_regular_file = !candidate.is_directory &&
                                    !candidate.is_link_or_reparse;
    }
#else
    {
        struct stat metadata;
        if (lstat(path, &metadata) != 0) return from_errno(errno, error);
        candidate.is_regular_file = S_ISREG(metadata.st_mode);
        candidate.is_directory = S_ISDIR(metadata.st_mode);
        candidate.is_link_or_reparse = S_ISLNK(metadata.st_mode);
        candidate.permissions = (uint32_t)(metadata.st_mode & (mode_t)07777);
    }
#endif
    *out = candidate;
    return PLATFORM_FS_OK;
}

PlatformFsResult platform_fs_inspect_nofollow(const char *path,
                                              PlatformFileMetadata *out,
                                              PlatformNativeError *error) {
    return platform_fs_internal_inspect_nofollow(path, out, error,
                                                 PLATFORM_FS_FAULT_NONE);
}

PlatformFsResult platform_fs_internal_apply_metadata(
    FILE *file, const PlatformFileMetadata *metadata, PlatformNativeError *error,
    PlatformFsFault fault
) {
    if (!file || !metadata) return invalid_argument(error);
    clear_error(error);
    if (fault == PLATFORM_FS_FAULT_APPLY_METADATA) {
#ifdef _WIN32
        return from_win32(ERROR_ACCESS_DENIED, error);
#else
        return from_errno(EIO, error);
#endif
    }
#ifdef _WIN32
    {
        int descriptor = _fileno(file);
        intptr_t handle;
        DWORD applicable;
        if (descriptor < 0) return from_win32(ERROR_INVALID_HANDLE, error);
        handle = _get_osfhandle(descriptor);
        if (handle == -1) return from_win32(ERROR_INVALID_HANDLE, error);
        applicable = metadata->native_attributes & FILE_ATTRIBUTE_READONLY;
        if (!SetFileInformationByHandle((HANDLE)handle, FileBasicInfo,
                                        &(FILE_BASIC_INFO){
                                            .FileAttributes = applicable != 0U
                                                ? applicable : FILE_ATTRIBUTE_NORMAL
                                        }, sizeof(FILE_BASIC_INFO))) {
            return from_win32(GetLastError(), error);
        }
    }
#else
    {
        int descriptor = fileno(file);
        if (descriptor < 0) return from_errno(errno, error);
        if (fchmod(descriptor, (mode_t)metadata->permissions) != 0)
            return from_errno(errno, error);
    }
#endif
    return PLATFORM_FS_OK;
}

PlatformFsResult platform_fs_apply_metadata(FILE *file,
                                            const PlatformFileMetadata *metadata,
                                            PlatformNativeError *error) {
    return platform_fs_internal_apply_metadata(file, metadata, error,
                                               PLATFORM_FS_FAULT_NONE);
}

PlatformFsResult platform_fs_internal_sync_file(
    FILE *file, PlatformNativeError *error, PlatformFsFault fault
) {
    if (!file) return invalid_argument(error);
    clear_error(error);
    if (fault == PLATFORM_FS_FAULT_SYNC_FILE) {
#ifdef _WIN32
        return from_win32(ERROR_WRITE_FAULT, error);
#else
        return from_errno(EIO, error);
#endif
    }
#ifdef _WIN32
    {
        int descriptor = _fileno(file);
        intptr_t handle;
        if (descriptor < 0) return from_win32(ERROR_INVALID_HANDLE, error);
        handle = _get_osfhandle(descriptor);
        if (handle == -1) return from_win32(ERROR_INVALID_HANDLE, error);
        if (!FlushFileBuffers((HANDLE)handle)) return from_win32(GetLastError(), error);
    }
#else
    {
        int descriptor = fileno(file);
        if (descriptor < 0) return from_errno(errno, error);
        if (fsync(descriptor) != 0) return from_errno(errno, error);
    }
#endif
    return PLATFORM_FS_OK;
}

PlatformFsResult platform_fs_sync_file(FILE *file, PlatformNativeError *error) {
    return platform_fs_internal_sync_file(file, error, PLATFORM_FS_FAULT_NONE);
}

static PlatformReplaceResult replace_result(PlatformFsResult result,
                                            PlatformCommitState commit_state,
                                            PlatformNativeError error) {
    PlatformReplaceResult replacement;
    replacement.result = result;
    replacement.commit_state = commit_state;
    replacement.error = error;
    return replacement;
}

#ifndef _WIN32
static PlatformFsResult sync_parent_directory(const char *path,
                                              PlatformNativeError *error) {
    const char *slash = strrchr(path, '/');
    char *directory;
    size_t length;
    int descriptor;
    int saved;
    if (!slash) {
        directory = NULL;
        descriptor = open(".", O_RDONLY);
    } else {
        length = slash == path ? 1U : (size_t)(slash - path);
        directory = malloc(length + 1U);
        if (!directory) return from_errno(ENOMEM, error);
        memcpy(directory, path, length);
        directory[length] = '\0';
        descriptor = open(directory, O_RDONLY);
        free(directory);
    }
    if (descriptor < 0) return from_errno(errno, error);
    if (fsync(descriptor) != 0) {
        saved = errno;
        (void)close(descriptor);
        return from_errno(saved, error);
    }
    if (close(descriptor) != 0) return from_errno(errno, error);
    return PLATFORM_FS_OK;
}
#endif

PlatformReplaceResult platform_fs_internal_replace(
    const char *temporary_path, const char *destination_path,
    bool destination_exists, PlatformFsFault fault
) {
    PlatformNativeError error = {PLATFORM_NATIVE_ERROR_NONE, 0U};
    if (!temporary_path || temporary_path[0] == '\0' || !destination_path ||
        destination_path[0] == '\0') {
        return replace_result(PLATFORM_FS_INVALID_ARGUMENT,
                              PLATFORM_COMMIT_NOT_COMMITTED, error);
    }
    if (fault == PLATFORM_FS_FAULT_REPLACE) {
#ifdef _WIN32
        PlatformFsResult result = from_win32(ERROR_ACCESS_DENIED, &error);
#else
        PlatformFsResult result = from_errno(EIO, &error);
#endif
        return replace_result(result, PLATFORM_COMMIT_NOT_COMMITTED, error);
    }
#ifdef _WIN32
    {
        PlatformNativePath temporary;
        PlatformNativePath destination;
        PlatformFsResult path_result;
        BOOL moved;
        platform_native_path_init(&temporary);
        platform_native_path_init(&destination);
        path_result = native_path(temporary_path, &temporary, &error);
        if (path_result != PLATFORM_FS_OK)
            return replace_result(path_result, PLATFORM_COMMIT_NOT_COMMITTED, error);
        path_result = native_path(destination_path, &destination, &error);
        if (path_result != PLATFORM_FS_OK) {
            platform_native_path_destroy(&temporary);
            return replace_result(path_result, PLATFORM_COMMIT_NOT_COMMITTED, error);
        }
        if (destination_exists) {
            moved = ReplaceFileW(destination.data, temporary.data, NULL,
                                 REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL);
        } else {
            moved = MoveFileExW(temporary.data, destination.data,
                                MOVEFILE_WRITE_THROUGH);
        }
        if (!moved) {
            DWORD saved = GetLastError();
            platform_native_path_destroy(&destination);
            platform_native_path_destroy(&temporary);
            path_result = from_win32(saved, &error);
            return replace_result(path_result, PLATFORM_COMMIT_NOT_COMMITTED, error);
        }
        platform_native_path_destroy(&destination);
        platform_native_path_destroy(&temporary);
        if (fault == PLATFORM_FS_FAULT_DURABILITY || destination_exists) {
            if (fault == PLATFORM_FS_FAULT_DURABILITY)
                (void)from_win32(ERROR_WRITE_FAULT, &error);
            return replace_result(PLATFORM_FS_OK,
                                  PLATFORM_COMMIT_COMMITTED_DURABILITY_WARNING,
                                  error);
        }
    }
#else
    (void)destination_exists;
    if (rename(temporary_path, destination_path) != 0) {
        PlatformFsResult result = from_errno(errno, &error);
        return replace_result(result, PLATFORM_COMMIT_NOT_COMMITTED, error);
    }
    if (fault == PLATFORM_FS_FAULT_DURABILITY) {
        (void)from_errno(EIO, &error);
        return replace_result(PLATFORM_FS_OK,
                              PLATFORM_COMMIT_COMMITTED_DURABILITY_WARNING,
                              error);
    }
    if (sync_parent_directory(destination_path, &error) != PLATFORM_FS_OK) {
        return replace_result(PLATFORM_FS_OK,
                              PLATFORM_COMMIT_COMMITTED_DURABILITY_WARNING,
                              error);
    }
#endif
    return replace_result(PLATFORM_FS_OK, PLATFORM_COMMIT_COMMITTED, error);
}

PlatformReplaceResult platform_fs_replace(const char *temporary_path,
                                          const char *destination_path,
                                          bool destination_exists) {
    return platform_fs_internal_replace(temporary_path, destination_path,
                                        destination_exists,
                                        PLATFORM_FS_FAULT_NONE);
}
