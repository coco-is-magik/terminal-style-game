/**
 * text_utf8.c — see text_utf8.h for the three rules this implements and why they are decisions.
 */

#include "text_utf8.h"

#include <utf8proc.h>

#include <stdlib.h>
#include <string.h>

const char *text_utf8_result_name(TextUtf8Result result) {
    switch (result) {
    case TEXT_UTF8_OK: return "OK";
    case TEXT_UTF8_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case TEXT_UTF8_MALFORMED: return "MALFORMED";
    case TEXT_UTF8_END: return "END";
    case TEXT_UTF8_CLUSTER_TOO_LONG: return "CLUSTER_TOO_LONG";
    case TEXT_UTF8_BUFFER_TOO_SMALL: return "BUFFER_TOO_SMALL";
    case TEXT_UTF8_NO_MEMORY: return "NO_MEMORY";
    }
    return "UNKNOWN";
}

/* One decode step. Kept here so both callers agree on what "malformed" means: utf8proc rejects
   overlong forms, surrogates and values above U+10FFFF, which is exactly the strict reading. */
static TextUtf8Result decode_one(const char *bytes, size_t size, size_t offset,
                                 utf8proc_int32_t *out_codepoint, size_t *out_length) {
    utf8proc_ssize_t step = utf8proc_iterate((const utf8proc_uint8_t *)bytes + offset,
                                             (utf8proc_ssize_t)(size - offset), out_codepoint);
    if (step <= 0) return TEXT_UTF8_MALFORMED;
    *out_length = (size_t)step;
    return TEXT_UTF8_OK;
}

TextUtf8Result text_utf8_validate(const char *bytes, size_t size) {
    size_t offset = 0U;

    if (!bytes && size > 0U) return TEXT_UTF8_INVALID_ARGUMENT;
    while (offset < size) {
        utf8proc_int32_t codepoint = 0;
        size_t step = 0U;
        TextUtf8Result result = decode_one(bytes, size, offset, &codepoint, &step);
        if (result != TEXT_UTF8_OK) return result;
        offset += step;
    }
    return TEXT_UTF8_OK;
}

TextUtf8Result text_utf8_normalize_nfc(const char *bytes, size_t size, char *out,
                                       size_t out_capacity, size_t *out_size) {
    utf8proc_uint8_t *mapped = NULL;
    utf8proc_ssize_t length;

    if (!out || !out_size) return TEXT_UTF8_INVALID_ARGUMENT;
    *out_size = 0U;
    if (size == 0U) return TEXT_UTF8_OK;
    if (!bytes) return TEXT_UTF8_INVALID_ARGUMENT;

    /* STABLE keeps the ordering deterministic across releases; COMPOSE is NFC by definition. NFKC is
       not offered: it rewrites creative content, which V1-4's product model forbids. */
    length = utf8proc_map((const utf8proc_uint8_t *)bytes, (utf8proc_ssize_t)size, &mapped,
                          UTF8PROC_STABLE | UTF8PROC_COMPOSE);
    if (length < 0) {
        free(mapped);
        if (length == UTF8PROC_ERROR_NOMEM) return TEXT_UTF8_NO_MEMORY;
        if (length == UTF8PROC_ERROR_OVERFLOW) return TEXT_UTF8_BUFFER_TOO_SMALL;
        return TEXT_UTF8_MALFORMED;
    }
    if ((size_t)length > out_capacity) {
        free(mapped);
        return TEXT_UTF8_BUFFER_TOO_SMALL;
    }
    if (length > 0) memcpy(out, mapped, (size_t)length);
    free(mapped);
    *out_size = (size_t)length;
    return TEXT_UTF8_OK;
}

TextUtf8Result text_utf8_next_cluster(const char *bytes, size_t size, size_t *offset,
                                      uint32_t *out_first_codepoint, size_t *out_length) {
    size_t start;
    size_t cursor;
    uint32_t first_codepoint = 0U;
    bool first = true;
    /* utf8proc's segmentation state starts at 0 and is fed every adjacent pair in order. Resetting it
       after a break is explicitly allowed, which is why each call seeds its own. */
    utf8proc_int32_t state = 0;
    utf8proc_int32_t previous = 0;

    if (!bytes || !offset || !out_first_codepoint || !out_length) return TEXT_UTF8_INVALID_ARGUMENT;
    start = *offset;
    if (start > size) return TEXT_UTF8_INVALID_ARGUMENT;
    if (start == size) return TEXT_UTF8_END;

    cursor = start;
    while (cursor < size) {
        utf8proc_int32_t codepoint = 0;
        size_t step = 0U;
        TextUtf8Result result = decode_one(bytes, size, cursor, &codepoint, &step);
        if (result != TEXT_UTF8_OK) return result; /* *offset untouched */

        if (!first && utf8proc_grapheme_break_stateful(previous, codepoint, &state) != 0) break;
        if (first) first_codepoint = (uint32_t)codepoint;
        previous = codepoint;
        first = false;
        cursor += step;
        if ((cursor - start) > TEXT_UTF8_MAX_CLUSTER_BYTES) return TEXT_UTF8_CLUSTER_TOO_LONG;
    }

    *out_first_codepoint = first_codepoint;
    *out_length = cursor - start;
    *offset = cursor;
    return TEXT_UTF8_OK;
}
