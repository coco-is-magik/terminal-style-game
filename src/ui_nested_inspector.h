/** ui_nested_inspector.h — Shared headless nested-list navigation/presentation. */
#ifndef UI_NESTED_INSPECTOR_H
#define UI_NESTED_INSPECTOR_H

#include <stdbool.h>
#include <stddef.h>

#define UI_NESTED_INSPECTOR_MAX_DEPTH 4U

typedef struct {
    size_t depth;
    size_t indices[UI_NESTED_INSPECTOR_MAX_DEPTH];
} UiNestedInspectorCursor;

bool ui_nested_inspector_step(size_t *index, size_t count, bool previous);
bool ui_nested_inspector_enter(UiNestedInspectorCursor *cursor, size_t child_count);
bool ui_nested_inspector_escape(UiNestedInspectorCursor *cursor);
size_t *ui_nested_inspector_index(UiNestedInspectorCursor *cursor);
const size_t *ui_nested_inspector_index_const(const UiNestedInspectorCursor *cursor);
bool ui_nested_inspector_format_row(char *out, size_t capacity, bool selected,
                                    size_t depth, size_t insertion_order,
                                    const char *label);

#endif