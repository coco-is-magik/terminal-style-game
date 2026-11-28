/**
 * text_edit.c — see text_edit.h. Every boundary answer comes from text_utf8's segmentation, so there
 * is exactly one definition of where a cluster ends.
 */

#include "text_edit.h"

#include "text_utf8.h"

#include <string.h>

const char *text_edit_result_name(TextEditResult result) {
    switch (result) {
    case TEXT_EDIT_OK: return "OK";
    case TEXT_EDIT_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case TEXT_EDIT_MALFORMED: return "MALFORMED";
    case TEXT_EDIT_TRUNCATED: return "TRUNCATED";
    case TEXT_EDIT_NOTHING_FITS: return "NOTHING_FITS";
    case TEXT_EDIT_NO_ROOM: return "NO_ROOM";
    }
    return "UNKNOWN";
}

/* Walk from the start, one cluster at a time, reporting where we got to. Kept private because every
   public answer must come from the same walk — two slightly different loops is how a cursor and a
   Backspace start disagreeing about the same string. */
static TextEditResult walk_to(const char *bytes, size_t size, size_t limit, size_t *out_offset) {
    size_t cursor = 0U;
    size_t offset = 0U;

    while (cursor < size && cursor < limit) {
        uint32_t codepoint = 0U;
        size_t length = 0U;
        if (text_utf8_next_cluster(bytes, size, &cursor, &codepoint, &length) != TEXT_UTF8_OK) {
            return TEXT_EDIT_MALFORMED;
        }
        if (cursor > limit) break; /* the boundary we were looking for does not exist */
        offset = cursor;
    }
    *out_offset = offset;
    return TEXT_EDIT_OK;
}

bool text_edit_is_boundary(const char *bytes, size_t size, size_t offset) {
    size_t walked = 0U;
    if (!bytes || offset > size) return false;
    if (offset == 0U || offset == size) return true;
    if (walk_to(bytes, size, offset, &walked) != TEXT_EDIT_OK) return false;
    return walked == offset;
}

TextEditResult text_edit_prev_boundary(const char *bytes, size_t size, size_t offset, size_t *out) {
    size_t walked = 0U;
    if (!bytes || !out || offset > size) return TEXT_EDIT_INVALID_ARGUMENT;
    if (offset == 0U) {
        *out = 0U;
        return TEXT_EDIT_OK;
    }
    /* The greatest boundary *strictly* less than offset: walk to just before it. */
    if (walk_to(bytes, size, offset - 1U, &walked) != TEXT_EDIT_OK) return TEXT_EDIT_MALFORMED;
    *out = walked;
    return TEXT_EDIT_OK;
}

TextEditResult text_edit_next_boundary(const char *bytes, size_t size, size_t offset, size_t *out) {
    size_t cursor = offset;
    uint32_t codepoint = 0U;
    size_t length = 0U;
    if (!bytes || !out || offset > size) return TEXT_EDIT_INVALID_ARGUMENT;
    if (offset == size) {
        *out = size;
        return TEXT_EDIT_OK;
    }
    /* Forward from a non-boundary would decode a continuation byte and report a boundary that is not
       one — the caller's cursor is wrong, and saying so is more useful than inventing an answer. */
    if (!text_edit_is_boundary(bytes, size, offset)) return TEXT_EDIT_INVALID_ARGUMENT;
    if (text_utf8_next_cluster(bytes, size, &cursor, &codepoint, &length) != TEXT_UTF8_OK) {
        return TEXT_EDIT_MALFORMED;
    }
    (void)length;
    *out = cursor;
    return TEXT_EDIT_OK;
}

TextEditResult text_edit_backspace(char *bytes, size_t capacity, size_t *size, size_t *cursor) {
    size_t start = 0U;
    size_t tail;

    if (!bytes || !size || !cursor) return TEXT_EDIT_INVALID_ARGUMENT;
    if (*size > capacity || *cursor > *size) return TEXT_EDIT_INVALID_ARGUMENT;
    if (*cursor == 0U) return TEXT_EDIT_OK; /* nothing before the cursor */

    if (!text_edit_is_boundary(bytes, *size, *cursor)) return TEXT_EDIT_MALFORMED;
    if (text_edit_prev_boundary(bytes, *size, *cursor, &start) != TEXT_EDIT_OK) {
        return TEXT_EDIT_MALFORMED;
    }

    tail = *size - *cursor;
    memmove(bytes + start, bytes + *cursor, tail);
    *size -= (*cursor - start);
    *cursor = start;
    /* Keep the buffer a C string as well as a sized buffer: the editor's fields are both, and a field
       whose length says 6 while its NUL sits at 7 reads back as 7 bytes to every `strlen`. */
    if (*size < capacity) bytes[*size] = '\0';
    return TEXT_EDIT_OK;
}

void text_edit_selection(size_t anchor, size_t caret, size_t *out_begin, size_t *out_end) {
    if (!out_begin || !out_end) return;
    *out_begin = anchor < caret ? anchor : caret;
    *out_end = anchor < caret ? caret : anchor;
}

TextEditResult text_edit_replace_range(char *bytes, size_t capacity, size_t *size,
                                       size_t begin, size_t end,
                                       const char *with, size_t with_size,
                                       size_t *out_cursor) {
    size_t tail;
    size_t removed;
    size_t result;

    if (!bytes || !size) return TEXT_EDIT_INVALID_ARGUMENT;
    if (*size > capacity) return TEXT_EDIT_INVALID_ARGUMENT;
    if (begin > end || end > *size) return TEXT_EDIT_INVALID_ARGUMENT;
    if (with_size > 0U && !with) return TEXT_EDIT_INVALID_ARGUMENT;

    /* Both ends must already be boundaries. `is_boundary` walks the buffer, so this also refuses a
       malformed buffer anywhere up to `end` — a caret that cannot be justified is not a caret. */
    if (!text_edit_is_boundary(bytes, *size, begin)) return TEXT_EDIT_MALFORMED;
    if (!text_edit_is_boundary(bytes, *size, end)) return TEXT_EDIT_MALFORMED;
    if (with_size > 0U && text_utf8_validate(with, with_size) != TEXT_UTF8_OK)
        return TEXT_EDIT_MALFORMED;

    removed = end - begin;
    result = *size - removed + with_size;
    /* capacity counts the terminator: a field of 256 bytes holds 255. */
    if (capacity == 0U || result >= capacity) return TEXT_EDIT_NO_ROOM;

    /* Validate everything before touching anything, so a refusal never leaves half an edit behind.
       memmove handles both directions: the tail shifts right when the replacement is longer. */
    tail = *size - end;
    memmove(bytes + begin + with_size, bytes + end, tail);
    if (with_size > 0U) memcpy(bytes + begin, with, with_size);
    *size = result;
    bytes[result] = '\0';
    if (out_cursor) *out_cursor = begin + with_size;
    return TEXT_EDIT_OK;
}

TextEditResult text_edit_fit(const char *src, size_t src_size, char *dst, size_t capacity,
                             size_t *out_size) {
    size_t cursor = 0U;
    size_t written = 0U;

    if (!dst || !out_size) return TEXT_EDIT_INVALID_ARGUMENT;
    *out_size = 0U;
    if (src_size == 0U) return TEXT_EDIT_OK;
    if (!src) return TEXT_EDIT_INVALID_ARGUMENT;

    /* Validate before copying anything: a caller must never inherit half a field because the tenth
       character was malformed. */
    if (text_utf8_validate(src, src_size) != TEXT_UTF8_OK) return TEXT_EDIT_MALFORMED;

    while (cursor < src_size) {
        uint32_t codepoint = 0U;
        size_t length = 0U;
        size_t after;
        if (text_utf8_next_cluster(src, src_size, &cursor, &codepoint, &length) != TEXT_UTF8_OK) {
            return TEXT_EDIT_MALFORMED; /* unreachable after validate; kept so the path cannot lie */
        }
        (void)codepoint;
        after = cursor;
        if ((written + length) > capacity) break; /* stop at a boundary, never inside one */
        memcpy(dst + written, src + (after - length), length);
        written += length;
    }

    *out_size = written;
    if (written == src_size) return TEXT_EDIT_OK;
    return written == 0U ? TEXT_EDIT_NOTHING_FITS : TEXT_EDIT_TRUNCATED;
}
