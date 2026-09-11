#include "ui_nested_inspector.h"

#include <stdint.h>
#include <stdio.h>

bool ui_nested_inspector_step(size_t *index, size_t count, bool previous) {
    if (!index || count == 0U || *index >= count) return false;
    if (previous) *index = *index == 0U ? count - 1U : *index - 1U;
    else *index = (*index + 1U) % count;
    return true;
}

bool ui_nested_inspector_enter(UiNestedInspectorCursor *cursor, size_t child_count) {
    if (!cursor || child_count == 0U ||
        cursor->depth + 1U >= UI_NESTED_INSPECTOR_MAX_DEPTH) return false;
    cursor->depth++;
    cursor->indices[cursor->depth] = 0U;
    return true;
}

bool ui_nested_inspector_escape(UiNestedInspectorCursor *cursor) {
    if (!cursor || cursor->depth == 0U) return false;
    cursor->depth--;
    return true;
}

size_t *ui_nested_inspector_index(UiNestedInspectorCursor *cursor) {
    if (!cursor || cursor->depth >= UI_NESTED_INSPECTOR_MAX_DEPTH) return NULL;
    return &cursor->indices[cursor->depth];
}

const size_t *ui_nested_inspector_index_const(const UiNestedInspectorCursor *cursor) {
    if (!cursor || cursor->depth >= UI_NESTED_INSPECTOR_MAX_DEPTH) return NULL;
    return &cursor->indices[cursor->depth];
}

bool ui_nested_inspector_format_row(char *out, size_t capacity, bool selected,
                                    size_t depth, size_t insertion_order,
                                    const char *label) {
    int written;
    if (!out || capacity == 0U || !label || depth > 16U) return false;
    written = snprintf(out, capacity, " %c %*s[%zu] %s",
                       selected ? '>' : ' ', (int)(depth * 2U), "",
                       insertion_order, label);
    return written >= 0 && (size_t)written < capacity;
}