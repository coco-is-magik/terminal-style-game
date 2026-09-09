/** ui_layout_resolver.h — Pure authored UiDocument geometry resolution. */
#ifndef UI_LAYOUT_RESOLVER_H
#define UI_LAYOUT_RESOLVER_H

#include "ui_document.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    int x;
    int y;
    int width;
    int height;
} UiResolvedRect;

typedef struct {
    UiElementId element_id;
    UiResolvedRect rect;
    UiResolvedRect clip;
    size_t paint_order;
} UiResolvedElement;

typedef enum {
    UI_LAYOUT_RESOLVE_OK = 0,
    UI_LAYOUT_RESOLVE_INVALID_ARGUMENT,
    UI_LAYOUT_RESOLVE_INVALID_DOCUMENT,
    UI_LAYOUT_RESOLVE_OVERFLOW
} UiLayoutResolveResult;

UiLayoutResolveResult ui_layout_resolve(
    const UiDocument *document,
    int viewport_width,
    int viewport_height,
    UiResolvedElement out_elements[UI_DOCUMENT_MAX_ELEMENTS],
    size_t *out_count
);

#endif