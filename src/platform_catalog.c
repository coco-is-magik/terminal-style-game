#define _POSIX_C_SOURCE 200809L

#include "platform_catalog.h"
#include "platform_catalog_internal.h"
#include "platform_path.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

static void clear_error(PlatformNativeError *error) {
    if (!error) return;
    error->domain = PLATFORM_NATIVE_ERROR_NONE;
    error->code = 0U;
}

#ifdef _WIN32
static void set_win32_error(PlatformNativeError *error, DWORD code) {
    if (!error) return;
    error->domain = PLATFORM_NATIVE_ERROR_WIN32;
    error->code = (uint32_t)code;
}
#else
static void set_errno_error(PlatformNativeError *error, int code) {
    if (!error) return;
    error->domain = PLATFORM_NATIVE_ERROR_ERRNO;
    error->code = (uint32_t)code;
}
#endif

static PlatformCatalogResult invoke_callback(
    const PlatformCatalogEntry *entry, PlatformCatalogCallback callback,
    void *context, bool *out_stop
) {
    PlatformCatalogCallbackResult result = callback(entry, context);
    if (result == PLATFORM_CATALOG_CALLBACK_FAILED)
        return PLATFORM_CATALOG_CALLBACK_REJECTED;
    *out_stop = result == PLATFORM_CATALOG_CALLBACK_STOP;
    return PLATFORM_CATALOG_OK;
}

#ifdef _WIN32
static PlatformCatalogEntryKind windows_kind(DWORD attributes) {
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U)
        return PLATFORM_CATALOG_ENTRY_LINK_OR_REPARSE;
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U)
        return PLATFORM_CATALOG_ENTRY_DIRECTORY;
    if ((attributes & FILE_ATTRIBUTE_DEVICE) != 0U)
        return PLATFORM_CATALOG_ENTRY_OTHER;
    return PLATFORM_CATALOG_ENTRY_REGULAR_FILE;
}

static PlatformCatalogResult close_windows_search(
    HANDLE search, PlatformNativeError *error
) {
    if (FindClose(search)) return PLATFORM_CATALOG_OK;
    set_win32_error(error, GetLastError());
    return PLATFORM_CATALOG_READ_FAILED;
}

static PlatformCatalogResult windows_pattern(
    const char *root_path, wchar_t **out_pattern, PlatformNativeError *error
) {
    PlatformNativePath native;
    wchar_t *pattern;
    size_t extra;
    platform_native_path_init(&native);
    switch (platform_path_from_utf8(root_path, strlen(root_path), &native)) {
        case PLATFORM_PATH_OK: break;
        case PLATFORM_PATH_OUT_OF_MEMORY: return PLATFORM_CATALOG_OUT_OF_MEMORY;
        default: return PLATFORM_CATALOG_PATH_INVALID;
    }
    extra = native.length > 0U &&
            ((wchar_t *)native.data)[native.length - 1U] != L'/' &&
            ((wchar_t *)native.data)[native.length - 1U] != L'\\' ? 2U : 1U;
    if (native.length > SIZE_MAX - extra - 1U) {
        platform_native_path_destroy(&native);
        return PLATFORM_CATALOG_PATH_INVALID;
    }
    pattern = malloc((native.length + extra + 1U) * sizeof(*pattern));
    if (!pattern) {
        platform_native_path_destroy(&native);
        return PLATFORM_CATALOG_OUT_OF_MEMORY;
    }
    memcpy(pattern, native.data, native.length * sizeof(*pattern));
    if (extra == 2U) pattern[native.length++] = L'\\';
    pattern[native.length++] = L'*';
    pattern[native.length] = L'\0';
    platform_native_path_destroy(&native);
    *out_pattern = pattern;
    clear_error(error);
    return PLATFORM_CATALOG_OK;
}
#endif

PlatformCatalogResult platform_catalog_internal_enumerate(
    const char *root_path, PlatformCatalogCallback callback, void *context,
    PlatformNativeError *error, PlatformCatalogFault fault
) {
    if (!root_path || root_path[0] == '\0' || !callback) {
        clear_error(error);
        return PLATFORM_CATALOG_INVALID_ARGUMENT;
    }
    clear_error(error);
    if (!platform_path_is_valid_utf8(root_path, strlen(root_path)))
        return PLATFORM_CATALOG_PATH_INVALID;
#ifdef _WIN32
    {
        WIN32_FIND_DATAW data;
        HANDLE search;
        wchar_t *pattern = NULL;
        PlatformCatalogResult result;
        if (fault == PLATFORM_CATALOG_FAULT_OPEN) {
            set_win32_error(error, ERROR_ACCESS_DENIED);
            return PLATFORM_CATALOG_OPEN_FAILED;
        }
        result = windows_pattern(root_path, &pattern, error);
        if (result != PLATFORM_CATALOG_OK) return result;
        search = FindFirstFileW(pattern, &data);
        free(pattern);
        if (search == INVALID_HANDLE_VALUE) {
            DWORD code = GetLastError();
            if (code == ERROR_FILE_NOT_FOUND) {
                PlatformNativePath native_root;
                DWORD attributes;
                DWORD attribute_error;
                platform_native_path_init(&native_root);
                if (platform_path_from_utf8(
                        root_path, strlen(root_path), &native_root) !=
                    PLATFORM_PATH_OK) {
                    return PLATFORM_CATALOG_PATH_INVALID;
                }
                attributes = GetFileAttributesW(native_root.data);
                attribute_error = attributes == INVALID_FILE_ATTRIBUTES
                    ? GetLastError() : ERROR_SUCCESS;
                platform_native_path_destroy(&native_root);
                if (attributes != INVALID_FILE_ATTRIBUTES &&
                    (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U &&
                    (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U) {
                    return PLATFORM_CATALOG_OK;
                }
                code = attributes == INVALID_FILE_ATTRIBUTES
                    ? attribute_error : ERROR_DIRECTORY;
            }
            set_win32_error(error, code);
            return PLATFORM_CATALOG_OPEN_FAILED;
        }
        for (;;) {
            if (wcscmp(data.cFileName, L".") != 0 &&
                wcscmp(data.cFileName, L"..") != 0) {
                PlatformNativePath native_name;
                PlatformCatalogEntry entry;
                PlatformPathResult name_result;
                char *name = NULL;
                size_t name_length = 0U;
                bool stop = false;
                platform_native_path_init(&native_name);
                native_name.data = data.cFileName;
                native_name.length = wcslen(data.cFileName);
                if (fault == PLATFORM_CATALOG_FAULT_NAME_CONVERSION) {
                    (void)FindClose(search);
                    return PLATFORM_CATALOG_PATH_INVALID;
                }
                name_result = platform_path_to_utf8(
                    &native_name, &name, &name_length);
                if (name_result != PLATFORM_PATH_OK) {
                    (void)FindClose(search);
                    return name_result == PLATFORM_PATH_OUT_OF_MEMORY
                        ? PLATFORM_CATALOG_OUT_OF_MEMORY
                        : PLATFORM_CATALOG_PATH_INVALID;
                }
                (void)name_length;
                if (fault == PLATFORM_CATALOG_FAULT_METADATA) {
                    free(name);
                    (void)FindClose(search);
                    set_win32_error(error, ERROR_FILE_NOT_FOUND);
                    return PLATFORM_CATALOG_METADATA_FAILED;
                }
                entry.name = name;
                entry.kind = windows_kind(data.dwFileAttributes);
                entry.native_attributes = (uint32_t)data.dwFileAttributes;
                result = invoke_callback(&entry, callback, context, &stop);
                free(name);
                if (result != PLATFORM_CATALOG_OK) {
                    (void)FindClose(search);
                    return result;
                }
                if (stop) {
                    return close_windows_search(search, error);
                }
            }
            if (fault == PLATFORM_CATALOG_FAULT_READ) {
                (void)FindClose(search);
                set_win32_error(error, ERROR_READ_FAULT);
                return PLATFORM_CATALOG_READ_FAILED;
            }
            if (!FindNextFileW(search, &data)) {
                DWORD code = GetLastError();
                if (code == ERROR_NO_MORE_FILES)
                    return close_windows_search(search, error);
                (void)FindClose(search);
                set_win32_error(error, code);
                return PLATFORM_CATALOG_READ_FAILED;
            }
        }
    }
#else
    {
        DIR *directory;
        struct dirent *directory_entry;
        int directory_fd;
        if (fault == PLATFORM_CATALOG_FAULT_OPEN) {
            set_errno_error(error, EACCES);
            return PLATFORM_CATALOG_OPEN_FAILED;
        }
        directory = opendir(root_path);
        if (!directory) {
            set_errno_error(error, errno);
            return PLATFORM_CATALOG_OPEN_FAILED;
        }
        directory_fd = dirfd(directory);
        if (directory_fd < 0) {
            set_errno_error(error, errno);
            closedir(directory);
            return PLATFORM_CATALOG_OPEN_FAILED;
        }
        errno = 0;
        while ((directory_entry = readdir(directory)) != NULL) {
            struct stat metadata;
            PlatformCatalogEntry entry;
            PlatformCatalogResult result;
            bool stop = false;
            if (strcmp(directory_entry->d_name, ".") == 0 ||
                strcmp(directory_entry->d_name, "..") == 0) continue;
            if (fault == PLATFORM_CATALOG_FAULT_NAME_CONVERSION ||
                !platform_path_is_valid_utf8(directory_entry->d_name,
                                             strlen(directory_entry->d_name))) {
                closedir(directory);
                return PLATFORM_CATALOG_PATH_INVALID;
            }
            if (fault == PLATFORM_CATALOG_FAULT_METADATA ||
                fstatat(directory_fd, directory_entry->d_name, &metadata,
                        AT_SYMLINK_NOFOLLOW) != 0) {
                int saved = fault == PLATFORM_CATALOG_FAULT_METADATA ? EIO : errno;
                set_errno_error(error, saved);
                closedir(directory);
                return PLATFORM_CATALOG_METADATA_FAILED;
            }
            entry.name = directory_entry->d_name;
            entry.native_attributes = 0U;
            if (S_ISREG(metadata.st_mode))
                entry.kind = PLATFORM_CATALOG_ENTRY_REGULAR_FILE;
            else if (S_ISDIR(metadata.st_mode))
                entry.kind = PLATFORM_CATALOG_ENTRY_DIRECTORY;
            else if (S_ISLNK(metadata.st_mode))
                entry.kind = PLATFORM_CATALOG_ENTRY_LINK_OR_REPARSE;
            else
                entry.kind = PLATFORM_CATALOG_ENTRY_OTHER;
            result = invoke_callback(&entry, callback, context, &stop);
            if (result != PLATFORM_CATALOG_OK) {
                (void)closedir(directory);
                return result;
            }
            if (stop) {
                if (closedir(directory) != 0) {
                    set_errno_error(error, errno);
                    return PLATFORM_CATALOG_READ_FAILED;
                }
                return PLATFORM_CATALOG_OK;
            }
            if (fault == PLATFORM_CATALOG_FAULT_READ) {
                set_errno_error(error, EIO);
                closedir(directory);
                return PLATFORM_CATALOG_READ_FAILED;
            }
            errno = 0;
        }
        if (errno != 0) {
            int saved = errno;
            set_errno_error(error, saved);
            closedir(directory);
            return PLATFORM_CATALOG_READ_FAILED;
        }
        if (closedir(directory) != 0) {
            set_errno_error(error, errno);
            return PLATFORM_CATALOG_READ_FAILED;
        }
        return PLATFORM_CATALOG_OK;
    }
#endif
}

PlatformCatalogResult platform_catalog_enumerate(
    const char *root_path, PlatformCatalogCallback callback, void *context,
    PlatformNativeError *error
) {
    return platform_catalog_internal_enumerate(
        root_path, callback, context, error, PLATFORM_CATALOG_FAULT_NONE);
}
