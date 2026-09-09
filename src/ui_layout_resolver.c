#include "ui_layout_resolver.h"

#include <limits.h>
#include <stdint.h>

static bool scale_value(int value, int percent, int *out) {
    int64_t product;
    int64_t magnitude;
    int64_t rounded;
    if (!out) return false;
    product = (int64_t)value * (int64_t)percent;
    magnitude = product < 0 ? -product : product;
    rounded = (magnitude + 50) / 100;
    if (product < 0) rounded = -rounded;
    if (rounded < INT_MIN || rounded > INT_MAX) return false;
    *out = (int)rounded;
    return true;
}

static bool add_int(int a, int b, int *out) {
    int64_t value = (int64_t)a + (int64_t)b;
    if (!out || value < INT_MIN || value > INT_MAX) return false;
    *out = (int)value;
    return true;
}

static bool resolve_axis(int parent_start, int parent_size, int position,
                         int authored_size, UiDocumentAnchor anchor,
                         int scale_percent, int *out_start, int *out_size) {
    int size;
    int start;
    if (anchor == UI_DOCUMENT_ANCHOR_STRETCH) {
        int64_t remaining = (int64_t)parent_size - position - authored_size;
        if (position < 0 || authored_size < 0 || remaining < 0 || remaining > INT_MAX)
            return false;
        if (!add_int(parent_start, position, &start)) return false;
        *out_start = start;
        *out_size = (int)remaining;
        return true;
    }
    if (!scale_value(authored_size, scale_percent, &size) || size <= 0) return false;
    if (anchor == UI_DOCUMENT_ANCHOR_START) {
        if (!add_int(parent_start, position, &start)) return false;
    } else if (anchor == UI_DOCUMENT_ANCHOR_CENTER) {
        int64_t centered = ((int64_t)parent_size - size) / 2;
        int64_t centered_start = (int64_t)parent_start + centered + position;
        if (centered_start < INT_MIN || centered_start > INT_MAX) return false;
        start = (int)centered_start;
    } else if (anchor == UI_DOCUMENT_ANCHOR_END) {
        int64_t end_start = (int64_t)parent_start + parent_size - size - position;
        if (end_start < INT_MIN || end_start > INT_MAX) return false;
        start = (int)end_start;
    } else return false;
    *out_start = start;
    *out_size = size;
    return true;
}

static UiResolvedRect intersect_rect(UiResolvedRect a, UiResolvedRect b) {
    int64_t a_right = (int64_t)a.x + a.width;
    int64_t a_bottom = (int64_t)a.y + a.height;
    int64_t b_right = (int64_t)b.x + b.width;
    int64_t b_bottom = (int64_t)b.y + b.height;
    int64_t left = a.x > b.x ? a.x : b.x;
    int64_t top = a.y > b.y ? a.y : b.y;
    int64_t right = a_right < b_right ? a_right : b_right;
    int64_t bottom = a_bottom < b_bottom ? a_bottom : b_bottom;
    UiResolvedRect result;
    if (right < left) right = left;
    if (bottom < top) bottom = top;
    result.x = (int)left;
    result.y = (int)top;
    result.width = (int)(right - left);
    result.height = (int)(bottom - top);
    return result;
}

static size_t find_resolved(const UiResolvedElement *elements, size_t count,
                            UiElementId id) {
    size_t i;
    for (i = 0U; i < count; i++)
        if (elements[i].element_id == id) return i;
    return count;
}

UiLayoutResolveResult ui_layout_resolve(
    const UiDocument *document, int viewport_width, int viewport_height,
    UiResolvedElement out_elements[UI_DOCUMENT_MAX_ELEMENTS], size_t *out_count
) {
    UiResolvedElement candidate[UI_DOCUMENT_MAX_ELEMENTS];
    UiResolvedRect viewport;
    size_t resolved = 0U;
    size_t pass;
    if (!document || !out_elements || !out_count || viewport_width <= 0 ||
        viewport_height <= 0) return UI_LAYOUT_RESOLVE_INVALID_ARGUMENT;
    if (ui_document_validate(document) != UI_DOCUMENT_OK)
        return UI_LAYOUT_RESOLVE_INVALID_DOCUMENT;
    viewport = (UiResolvedRect){0, 0, viewport_width, viewport_height};
    for (pass = 0U; pass < document->element_count; pass++) {
        size_t i;
        bool progress = false;
        for (i = 0U; i < document->element_count; i++) {
            const UiDocumentElement *element = &document->elements[i];
            UiResolvedElement value;
            UiResolvedRect parent_rect;
            UiResolvedRect parent_clip;
            size_t parent_index;
            if (find_resolved(candidate, resolved, element->id) < resolved) continue;
            if (element->parent_id == 0U) {
                value.rect = viewport;
                value.clip = viewport;
            } else {
                parent_index = find_resolved(candidate, resolved, element->parent_id);
                if (parent_index == resolved) continue;
                parent_rect = candidate[parent_index].rect;
                parent_clip = candidate[parent_index].clip;
                if (!resolve_axis(parent_rect.x, parent_rect.width, element->layout.x,
                                  element->layout.width,
                                  element->layout.horizontal_anchor,
                                  element->layout.scale_percent,
                                  &value.rect.x, &value.rect.width) ||
                    !resolve_axis(parent_rect.y, parent_rect.height, element->layout.y,
                                  element->layout.height,
                                  element->layout.vertical_anchor,
                                  element->layout.scale_percent,
                                  &value.rect.y, &value.rect.height))
                    return UI_LAYOUT_RESOLVE_OVERFLOW;
                value.clip = intersect_rect(value.rect, parent_clip);
                value.clip = intersect_rect(value.clip, viewport);
            }
            value.element_id = element->id;
            value.paint_order = i;
            candidate[resolved++] = value;
            progress = true;
        }
        if (resolved == document->element_count) break;
        if (!progress) return UI_LAYOUT_RESOLVE_INVALID_DOCUMENT;
    }
    if (resolved != document->element_count)
        return UI_LAYOUT_RESOLVE_INVALID_DOCUMENT;
    for (pass = 0U; pass < resolved; pass++) out_elements[pass] = candidate[pass];
    *out_count = resolved;
    return UI_LAYOUT_RESOLVE_OK;
}