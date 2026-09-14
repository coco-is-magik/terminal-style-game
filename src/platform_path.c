#include "platform_path.h"
#include "platform_path_internal.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
typedef char PlatformPathUnit;
#endif

void platform_native_path_init(PlatformNativePath *path) {
    if (!path) return;
    path->data = NULL;
    path->length = 0U;
}


void platform_native_path_destroy(PlatformNativePath *path) {
    if (!path) return;
    free(path->data);
    platform_native_path_init(path);
}

static bool continuation(unsigned char byte) {
    return byte >= UINT8_C(0x80) && byte <= UINT8_C(0xbf);
}

bool platform_path_is_valid_utf8(const char *text, size_t length) {
    size_t i = 0U;
    if (!text && length != 0U) return false;
    while (i < length) {
        unsigned char first = (unsigned char)text[i++];
        if (first == 0U) return false;
        if (first <= UINT8_C(0x7f)) continue;
        if (first >= UINT8_C(0xc2) && first <= UINT8_C(0xdf)) {
            if (i >= length || !continuation((unsigned char)text[i])) return false;
            i++;
            continue;
        }
        if (first >= UINT8_C(0xe0) && first <= UINT8_C(0xef)) {
            unsigned char second;
            if (length - i < 2U) return false;
            second = (unsigned char)text[i];
            if (!continuation(second) || !continuation((unsigned char)text[i + 1U]) ||
                (first == UINT8_C(0xe0) && second < UINT8_C(0xa0)) ||
                (first == UINT8_C(0xed) && second > UINT8_C(0x9f))) return false;
            i += 2U;
            continue;
        }
        if (first >= UINT8_C(0xf0) && first <= UINT8_C(0xf4)) {
            unsigned char second;
            if (length - i < 3U) return false;
            second = (unsigned char)text[i];
            if (!continuation(second) || !continuation((unsigned char)text[i + 1U]) ||
                !continuation((unsigned char)text[i + 2U]) ||
                (first == UINT8_C(0xf0) && second < UINT8_C(0x90)) ||
                (first == UINT8_C(0xf4) && second > UINT8_C(0x8f))) return false;
            i += 3U;
            continue;
        }
        return false;
    }
    return true;
}

PlatformPathResult platform_path_internal_from_utf8(
    const char *text, size_t length, PlatformNativePath *out,
    PlatformPathFault fault
) {
    PlatformNativePath candidate;
    if (!out || (!text && length != 0U)) return PLATFORM_PATH_INVALID_ARGUMENT;
    if (!platform_path_is_valid_utf8(text, length)) return PLATFORM_PATH_INVALID_UTF8;
    if (fault == PLATFORM_PATH_FAULT_ALLOCATION) return PLATFORM_PATH_OUT_OF_MEMORY;
    if (fault == PLATFORM_PATH_FAULT_CONVERSION) return PLATFORM_PATH_CONVERSION_FAILED;
    platform_native_path_init(&candidate);
#ifdef _WIN32
    {
        int source_length;
        int required;
        wchar_t *wide;
        if (length > (size_t)INT_MAX) return PLATFORM_PATH_TOO_LONG;
        source_length = (int)length;
        if (source_length == 0) {
            wide = malloc(sizeof(*wide));
            if (!wide) return PLATFORM_PATH_OUT_OF_MEMORY;
            wide[0] = L'\0';
            candidate.data = wide;
            candidate.length = 0U;
        } else {
            required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text,
                                           source_length, NULL, 0);
            if (required <= 0) return PLATFORM_PATH_CONVERSION_FAILED;
            if ((size_t)required > (SIZE_MAX / sizeof(*wide)) - 1U)
                return PLATFORM_PATH_TOO_LONG;
            wide = malloc(((size_t)required + 1U) * sizeof(*wide));
            if (!wide) return PLATFORM_PATH_OUT_OF_MEMORY;
            if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text,
                                    source_length, wide, required) != required) {
                free(wide);
                return PLATFORM_PATH_CONVERSION_FAILED;
            }
            wide[required] = L'\0';
            candidate.data = wide;
            candidate.length = (size_t)required;
        }
    }
#else
    {
        PlatformPathUnit *native;
        if (length == SIZE_MAX) return PLATFORM_PATH_TOO_LONG;
        native = malloc(length + 1U);
        if (!native) return PLATFORM_PATH_OUT_OF_MEMORY;
        if (length > 0U) memcpy(native, text, length);
        native[length] = '\0';
        candidate.data = native;
        candidate.length = length;
    }
#endif
    *out = candidate;
    return PLATFORM_PATH_OK;
}

PlatformPathResult platform_path_from_utf8(const char *text, size_t length,
                                           PlatformNativePath *out) {
    return platform_path_internal_from_utf8(text, length, out,
                                            PLATFORM_PATH_FAULT_NONE);
}

PlatformPathResult platform_path_internal_to_utf8(
    const PlatformNativePath *path, char **out_text, size_t *out_length,
    PlatformPathFault fault
) {
    char *candidate;
    size_t candidate_length;
    if (!path || !out_text || !out_length || !path->data)
        return PLATFORM_PATH_INVALID_ARGUMENT;
    if (fault == PLATFORM_PATH_FAULT_ALLOCATION) return PLATFORM_PATH_OUT_OF_MEMORY;
    if (fault == PLATFORM_PATH_FAULT_CONVERSION) return PLATFORM_PATH_CONVERSION_FAILED;
#ifdef _WIN32
    {
        int source_length;
        int required;
        if (path->length > (size_t)INT_MAX) return PLATFORM_PATH_TOO_LONG;
        source_length = (int)path->length;
        if (source_length == 0) {
            candidate = malloc(1U);
            if (!candidate) return PLATFORM_PATH_OUT_OF_MEMORY;
            candidate[0] = '\0';
            candidate_length = 0U;
        } else {
            required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                           path->data, source_length,
                                           NULL, 0, NULL, NULL);
            if (required <= 0) return PLATFORM_PATH_CONVERSION_FAILED;
            if ((size_t)required == SIZE_MAX) return PLATFORM_PATH_TOO_LONG;
            candidate = malloc((size_t)required + 1U);
            if (!candidate) return PLATFORM_PATH_OUT_OF_MEMORY;
            if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path->data,
                                    source_length, candidate, required,
                                    NULL, NULL) != required) {
                free(candidate);
                return PLATFORM_PATH_CONVERSION_FAILED;
            }
            candidate[required] = '\0';
            candidate_length = (size_t)required;
        }
    }
#else
    candidate_length = path->length;
    if (candidate_length == SIZE_MAX) return PLATFORM_PATH_TOO_LONG;
    candidate = malloc(candidate_length + 1U);
    if (!candidate) return PLATFORM_PATH_OUT_OF_MEMORY;
    if (candidate_length > 0U) memcpy(candidate, path->data, candidate_length);
    candidate[candidate_length] = '\0';
#endif
    *out_text = candidate;
    *out_length = candidate_length;
    return PLATFORM_PATH_OK;
}

PlatformPathResult platform_path_to_utf8(const PlatformNativePath *path,
                                         char **out_text, size_t *out_length) {
    return platform_path_internal_to_utf8(path, out_text, out_length,
                                          PLATFORM_PATH_FAULT_NONE);
}
