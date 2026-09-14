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
#include <dirent.h>
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

static PlatformFsResult existing_directory_result(
    const char *path, PlatformNativeError *error
) {
    PlatformFileMetadata metadata;
    PlatformFsResult result = platform_fs_inspect_nofollow(path, &metadata, error);
    if (result != PLATFORM_FS_OK) return result;
    if (!metadata.is_directory || metadata.is_link_or_reparse) {
        clear_error(error);
        return PLATFORM_FS_WRONG_TYPE;
    }
    return PLATFORM_FS_OK;
}

PlatformFsResult platform_fs_internal_ensure_directory(
    const char *path, uint32_t mode, PlatformNativeError *error,
    PlatformFsFault fault
) {
    PlatformFsResult result;
    if (!path || path[0] == '\0') return invalid_argument(error);
    result = existing_directory_result(path, error);
    if (result == PLATFORM_FS_OK || result != PLATFORM_FS_NOT_FOUND) return result;
    if (fault == PLATFORM_FS_FAULT_ENSURE_DIRECTORY) {
#ifdef _WIN32
        return from_win32(ERROR_ACCESS_DENIED, error);
#else
        return from_errno(EACCES, error);
#endif
    }
#ifdef _WIN32
    {
        PlatformNativePath native;
        (void)mode;
        platform_native_path_init(&native);
        result = native_path(path, &native, error);
        if (result != PLATFORM_FS_OK) return result;
        if (!CreateDirectoryW(native.data, NULL)) {
            DWORD saved = GetLastError();
            platform_native_path_destroy(&native);
            if (saved == ERROR_ALREADY_EXISTS)
                return existing_directory_result(path, error);
            return from_win32(saved, error);
        }
        platform_native_path_destroy(&native);
    }
#else
    if (mkdir(path, (mode_t)mode) != 0) {
        int saved = errno;
        if (saved == EEXIST) return existing_directory_result(path, error);
        return from_errno(saved, error);
    }
#endif
    clear_error(error);
    return PLATFORM_FS_OK;
}

PlatformFsResult platform_fs_ensure_directory(const char *path, uint32_t mode,
                                              PlatformNativeError *error) {
    return platform_fs_internal_ensure_directory(
        path, mode, error, PLATFORM_FS_FAULT_NONE);
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

static PlatformMoveResult move_result(PlatformFsResult result,
                                      PlatformCommitState commit_state,
                                      PlatformNativeError error) {
    PlatformMoveResult moved;
    moved.result = result;
    moved.commit_state = commit_state;
    moved.error = error;
    return moved;
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

PlatformMoveResult platform_fs_internal_move(
    const char *source_path, const char *destination_path, PlatformFsFault fault
) {
    PlatformNativeError error = {PLATFORM_NATIVE_ERROR_NONE, 0U};
    PlatformFileMetadata metadata;
    PlatformFsResult result;
    if (!source_path || source_path[0] == '\0' || !destination_path ||
        destination_path[0] == '\0')
        return move_result(PLATFORM_FS_INVALID_ARGUMENT,
                           PLATFORM_COMMIT_NOT_COMMITTED, error);
    result = platform_fs_inspect_nofollow(destination_path, &metadata, &error);
    if (result == PLATFORM_FS_OK)
        return move_result(PLATFORM_FS_ALREADY_EXISTS,
                           PLATFORM_COMMIT_NOT_COMMITTED, error);
    if (result != PLATFORM_FS_NOT_FOUND)
        return move_result(result, PLATFORM_COMMIT_NOT_COMMITTED, error);
    clear_error(&error);
    if (fault == PLATFORM_FS_FAULT_MOVE) {
#ifdef _WIN32
        result = from_win32(ERROR_ACCESS_DENIED, &error);
#else
        result = from_errno(EIO, &error);
#endif
        return move_result(result, PLATFORM_COMMIT_NOT_COMMITTED, error);
    }
#ifdef _WIN32
    {
        PlatformNativePath source;
        PlatformNativePath destination;
        platform_native_path_init(&source);
        platform_native_path_init(&destination);
        result = native_path(source_path, &source, &error);
        if (result != PLATFORM_FS_OK)
            return move_result(result, PLATFORM_COMMIT_NOT_COMMITTED, error);
        result = native_path(destination_path, &destination, &error);
        if (result != PLATFORM_FS_OK) {
            platform_native_path_destroy(&source);
            return move_result(result, PLATFORM_COMMIT_NOT_COMMITTED, error);
        }
        if (!MoveFileExW(source.data, destination.data, MOVEFILE_WRITE_THROUGH)) {
            DWORD saved = GetLastError();
            platform_native_path_destroy(&destination);
            platform_native_path_destroy(&source);
            result = from_win32(saved, &error);
            return move_result(result, PLATFORM_COMMIT_NOT_COMMITTED, error);
        }
        platform_native_path_destroy(&destination);
        platform_native_path_destroy(&source);
        return move_result(PLATFORM_FS_OK,
                           PLATFORM_COMMIT_COMMITTED_DURABILITY_WARNING, error);
    }
#else
    if (rename(source_path, destination_path) != 0) {
        result = from_errno(errno, &error);
        return move_result(result, PLATFORM_COMMIT_NOT_COMMITTED, error);
    }
    if (fault == PLATFORM_FS_FAULT_DURABILITY) {
        (void)from_errno(EIO, &error);
        return move_result(PLATFORM_FS_OK,
                           PLATFORM_COMMIT_COMMITTED_DURABILITY_WARNING, error);
    }
    if (sync_parent_directory(destination_path, &error) != PLATFORM_FS_OK)
        return move_result(PLATFORM_FS_OK,
                           PLATFORM_COMMIT_COMMITTED_DURABILITY_WARNING, error);
    return move_result(PLATFORM_FS_OK, PLATFORM_COMMIT_COMMITTED, error);
#endif
}

PlatformMoveResult platform_fs_move(const char *source_path,
                                    const char *destination_path) {
    return platform_fs_internal_move(
        source_path, destination_path, PLATFORM_FS_FAULT_NONE);
}

PlatformFsResult platform_fs_internal_remove_flat_directory(
    const char *path, PlatformNativeError *error, PlatformFsFault fault
) {
    PlatformFileMetadata root_metadata;
    PlatformFsResult root_result;
    if (!path || path[0] == '\0') return invalid_argument(error);
    clear_error(error);
    root_result = platform_fs_inspect_nofollow(path, &root_metadata, error);
    if (root_result != PLATFORM_FS_OK) return root_result;
    if (!root_metadata.is_directory || root_metadata.is_link_or_reparse) {
        clear_error(error);
        return PLATFORM_FS_WRONG_TYPE;
    }
    if (fault == PLATFORM_FS_FAULT_REMOVE_FLAT_DIRECTORY) {
#ifdef _WIN32
        return from_win32(ERROR_ACCESS_DENIED, error);
#else
        return from_errno(EIO, error);
#endif
    }
#ifdef _WIN32
    {
        PlatformNativePath native;
        wchar_t *pattern;
        WIN32_FIND_DATAW entry;
        HANDLE search;
        size_t pattern_length;
        platform_native_path_init(&native);
        {
            PlatformFsResult result = native_path(path, &native, error);
            if (result != PLATFORM_FS_OK) return result;
        }
        pattern_length = native.length + 3U;
        pattern = malloc(pattern_length * sizeof(*pattern));
        if (!pattern) {
            platform_native_path_destroy(&native);
            return from_win32(ERROR_NOT_ENOUGH_MEMORY, error);
        }
        memcpy(pattern, native.data, native.length * sizeof(*pattern));
        pattern[native.length] = L'\\';
        pattern[native.length + 1U] = L'*';
        pattern[native.length + 2U] = L'\0';
        search = FindFirstFileW(pattern, &entry);
        if (search == INVALID_HANDLE_VALUE) {
            DWORD saved = GetLastError();
            free(pattern);
            platform_native_path_destroy(&native);
            return from_win32(saved, error);
        }
        do {
            if (wcscmp(entry.cFileName, L".") == 0 ||
                wcscmp(entry.cFileName, L"..") == 0) continue;
            if ((entry.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY |
                                           FILE_ATTRIBUTE_REPARSE_POINT)) != 0U) {
                (void)FindClose(search);
                free(pattern);
                platform_native_path_destroy(&native);
                return PLATFORM_FS_WRONG_TYPE;
            }
        } while (FindNextFileW(search, &entry));
        if (GetLastError() != ERROR_NO_MORE_FILES) {
            DWORD saved = GetLastError();
            (void)FindClose(search);
            free(pattern);
            platform_native_path_destroy(&native);
            return from_win32(saved, error);
        }
        if (!FindClose(search)) {
            DWORD saved = GetLastError();
            free(pattern);
            platform_native_path_destroy(&native);
            return from_win32(saved, error);
        }
        search = FindFirstFileW(pattern, &entry);
        if (search == INVALID_HANDLE_VALUE) {
            DWORD saved = GetLastError();
            free(pattern);
            platform_native_path_destroy(&native);
            return from_win32(saved, error);
        }
        do {
            wchar_t *child;
            size_t name_length;
            size_t child_length;
            if (wcscmp(entry.cFileName, L".") == 0 ||
                wcscmp(entry.cFileName, L"..") == 0) continue;
            name_length = wcslen(entry.cFileName);
            child_length = native.length + 1U + name_length + 1U;
            child = malloc(child_length * sizeof(*child));
            if (!child) {
                (void)FindClose(search);
                free(pattern);
                platform_native_path_destroy(&native);
                return from_win32(ERROR_NOT_ENOUGH_MEMORY, error);
            }
            memcpy(child, native.data, native.length * sizeof(*child));
            child[native.length] = L'\\';
            memcpy(child + native.length + 1U, entry.cFileName,
                   (name_length + 1U) * sizeof(*child));
            if (!DeleteFileW(child)) {
                DWORD saved = GetLastError();
                free(child);
                (void)FindClose(search);
                free(pattern);
                platform_native_path_destroy(&native);
                return from_win32(saved, error);
            }
            free(child);
        } while (FindNextFileW(search, &entry));
        if (GetLastError() != ERROR_NO_MORE_FILES) {
            DWORD saved = GetLastError();
            (void)FindClose(search);
            free(pattern);
            platform_native_path_destroy(&native);
            return from_win32(saved, error);
        }
        if (!FindClose(search)) {
            DWORD saved = GetLastError();
            free(pattern);
            platform_native_path_destroy(&native);
            return from_win32(saved, error);
        }
        free(pattern);
        if (!RemoveDirectoryW(native.data)) {
            DWORD saved = GetLastError();
            platform_native_path_destroy(&native);
            return from_win32(saved, error);
        }
        platform_native_path_destroy(&native);
    }
#else
    {
        DIR *directory = opendir(path);
        struct dirent *entry;
        int saved = 0;
        if (!directory) return from_errno(errno, error);
        errno = 0;
        while ((entry = readdir(directory)) != NULL) {
            char *child;
            size_t path_length;
            size_t name_length;
            struct stat metadata;
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) continue;
            path_length = strlen(path);
            name_length = strlen(entry->d_name);
            if (path_length > SIZE_MAX - name_length - 2U) {
                saved = ENOMEM;
                break;
            }
            child = malloc(path_length + name_length + 2U);
            if (!child) { saved = ENOMEM; break; }
            memcpy(child, path, path_length);
            child[path_length] = '/';
            memcpy(child + path_length + 1U, entry->d_name, name_length + 1U);
            if (lstat(child, &metadata) != 0) saved = errno;
            else if (!S_ISREG(metadata.st_mode)) saved = EISDIR;
            free(child);
            if (saved != 0) break;
            errno = 0;
        }
        if (saved == 0 && errno != 0) saved = errno;
        if (closedir(directory) != 0 && saved == 0) saved = errno;
        if (saved != 0) return saved == EISDIR
            ? PLATFORM_FS_WRONG_TYPE : from_errno(saved, error);
        directory = opendir(path);
        if (!directory) return from_errno(errno, error);
        errno = 0;
        while ((entry = readdir(directory)) != NULL) {
            char *child;
            size_t path_length;
            size_t name_length;
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) continue;
            path_length = strlen(path);
            name_length = strlen(entry->d_name);
            child = malloc(path_length + name_length + 2U);
            if (!child) { saved = ENOMEM; break; }
            memcpy(child, path, path_length);
            child[path_length] = '/';
            memcpy(child + path_length + 1U, entry->d_name, name_length + 1U);
            if (unlink(child) != 0) saved = errno;
            free(child);
            if (saved != 0) break;
            errno = 0;
        }
        if (saved == 0 && errno != 0) saved = errno;
        if (closedir(directory) != 0 && saved == 0) saved = errno;
        if (saved != 0) return from_errno(saved, error);
        if (rmdir(path) != 0) return from_errno(errno, error);
    }
#endif
    return PLATFORM_FS_OK;
}

PlatformFsResult platform_fs_remove_flat_directory(
    const char *path, PlatformNativeError *error) {
    return platform_fs_internal_remove_flat_directory(
        path, error, PLATFORM_FS_FAULT_NONE);
}

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
