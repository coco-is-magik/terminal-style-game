/** platform_path.h — Strict UTF-8 validation and owned native path conversion. */
#ifndef PLATFORM_PATH_H
#define PLATFORM_PATH_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    PLATFORM_PATH_OK = 0,
    PLATFORM_PATH_INVALID_ARGUMENT,
    PLATFORM_PATH_INVALID_UTF8,
    PLATFORM_PATH_TOO_LONG,
    PLATFORM_PATH_OUT_OF_MEMORY,
    PLATFORM_PATH_CONVERSION_FAILED
} PlatformPathResult;

typedef struct {
    void *data;
    size_t length;
} PlatformNativePath;

void platform_native_path_init(PlatformNativePath *path);
void platform_native_path_destroy(PlatformNativePath *path);
bool platform_path_is_valid_utf8(const char *text, size_t length);
PlatformPathResult platform_path_from_utf8(const char *text, size_t length,
                                           PlatformNativePath *out);
PlatformPathResult platform_path_to_utf8(const PlatformNativePath *path,
                                         char **out_text, size_t *out_length);

#endif
