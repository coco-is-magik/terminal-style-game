#include "ui_interaction.h"

#include <stdint.h>

typedef struct {
    UiResolvedElement resolved[UI_DOCUMENT_MAX_ELEMENTS];
    bool eligible[UI_DOCUMENT_MAX_ELEMENTS];
    size_t count;
} InteractionView;

static const UiInteractionElementState *find_state(
    const UiInteractionElementState *states, size_t count, UiElementId id
) {
    size_t i;
    for (i = 0U; i < count; i++) if (states[i].element_id == id) return &states[i];
    return NULL;
}

static size_t document_index(const UiDocument *document, UiElementId id) {
    size_t i;
    for (i = 0U; i < document->element_count; i++)
        if (document->elements[i].id == id) return i;
    return document->element_count;
}

static UiInteractionResult build_view(
    const UiDocument *document, const UiInteractionElementState *states,
    size_t state_count, int viewport_width, int viewport_height,
    InteractionView *out_view
) {
    size_t i;
    if (!document || !out_view || viewport_width <= 0 || viewport_height <= 0 ||
        state_count > UI_DOCUMENT_MAX_ELEMENTS || (state_count > 0U && !states))
        return UI_INTERACTION_INVALID_ARGUMENT;
    if (ui_document_validate(document) != UI_DOCUMENT_OK)
        return UI_INTERACTION_INVALID_DOCUMENT;
    for (i = 0U; i < state_count; i++) {
        size_t j;
        if (!ui_document_find_element(document, states[i].element_id))
            return UI_INTERACTION_INVALID_STATE;
        for (j = 0U; j < i; j++)
            if (states[j].element_id == states[i].element_id)
                return UI_INTERACTION_INVALID_STATE;
    }
    if (ui_layout_resolve(document, viewport_width, viewport_height,
                          out_view->resolved, &out_view->count) !=
        UI_LAYOUT_RESOLVE_OK) return UI_INTERACTION_INVALID_DOCUMENT;
    for (i = 0U; i < out_view->count; i++) {
        size_t index = document_index(document, out_view->resolved[i].element_id);
        const UiDocumentElement *element = &document->elements[index];
        const UiInteractionElementState *state = find_state(
            states, state_count, element->id);
        bool visible = state ? state->visible : element->visual.visible_by_default;
        bool disabled = state && state->disabled;
        out_view->eligible[i] = element->type == UI_DOCUMENT_ELEMENT_BUTTON &&
            visible && !disabled && out_view->resolved[i].clip.width > 0 &&
            out_view->resolved[i].clip.height > 0;
    }
    return UI_INTERACTION_OK;
}

static size_t resolved_index(const InteractionView *view, UiElementId id) {
    size_t i;
    for (i = 0U; i < view->count; i++)
        if (view->resolved[i].element_id == id) return i;
    return view->count;
}

static bool point_inside(UiResolvedRect rect, int x, int y) {
    return x >= rect.x && y >= rect.y && x < rect.x + rect.width &&
           y < rect.y + rect.height;
}

UiInteractionResult ui_interaction_session_init(
    UiInteractionSession *session, const UiDocument *document,
    const UiInteractionElementState *states, size_t state_count,
    int viewport_width, int viewport_height
) {
    InteractionView view;
    size_t i;
    UiInteractionResult result;
    if (!session) return UI_INTERACTION_INVALID_ARGUMENT;
    result = build_view(document, states, state_count, viewport_width,
                        viewport_height, &view);
    if (result != UI_INTERACTION_OK) return result;
    for (i = 0U; i < view.count; i++)
        if (view.eligible[i]) {
            session->focused_element_id = view.resolved[i].element_id;
            return UI_INTERACTION_OK;
        }
    session->focused_element_id = 0U;
    return UI_INTERACTION_OK;
}

UiInteractionResult ui_interaction_hit_test(
    const UiDocument *document, const UiInteractionElementState *states,
    size_t state_count, int viewport_width, int viewport_height,
    int pointer_x, int pointer_y, UiElementId *out_element_id
) {
    InteractionView view;
    size_t i;
    UiInteractionResult result;
    if (!out_element_id) return UI_INTERACTION_INVALID_ARGUMENT;
    result = build_view(document, states, state_count, viewport_width,
                        viewport_height, &view);
    if (result != UI_INTERACTION_OK) return result;
    for (i = view.count; i > 0U; i--)
        if (view.eligible[i - 1U] &&
            point_inside(view.resolved[i - 1U].clip, pointer_x, pointer_y)) {
            *out_element_id = view.resolved[i - 1U].element_id;
            return UI_INTERACTION_OK;
        }
    return UI_INTERACTION_NO_HIT;
}

UiInteractionResult ui_interaction_focus_pointer(
    UiInteractionSession *session, const UiDocument *document,
    const UiInteractionElementState *states, size_t state_count,
    int viewport_width, int viewport_height, int pointer_x, int pointer_y
) {
    UiElementId hit;
    UiInteractionResult result;
    if (!session) return UI_INTERACTION_INVALID_ARGUMENT;
    result = ui_interaction_hit_test(document, states, state_count, viewport_width,
                                     viewport_height, pointer_x, pointer_y, &hit);
    if (result != UI_INTERACTION_OK) return result;
    session->focused_element_id = hit;
    return UI_INTERACTION_OK;
}

UiInteractionResult ui_interaction_focus_next(
    UiInteractionSession *session, const UiDocument *document,
    const UiInteractionElementState *states, size_t state_count,
    int viewport_width, int viewport_height, bool previous
) {
    InteractionView view;
    size_t current;
    size_t step;
    UiInteractionResult result;
    if (!session) return UI_INTERACTION_INVALID_ARGUMENT;
    result = build_view(document, states, state_count, viewport_width,
                        viewport_height, &view);
    if (result != UI_INTERACTION_OK) return result;
    current = resolved_index(&view, session->focused_element_id);
    if (current >= view.count || !view.eligible[current]) {
        if (previous) {
            for (step = view.count; step > 0U; step--)
                if (view.eligible[step - 1U]) {
                    session->focused_element_id = view.resolved[step - 1U].element_id;
                    return UI_INTERACTION_OK;
                }
        } else {
            for (step = 0U; step < view.count; step++)
                if (view.eligible[step]) {
                    session->focused_element_id = view.resolved[step].element_id;
                    return UI_INTERACTION_OK;
                }
        }
        return UI_INTERACTION_NO_ELIGIBLE;
    }
    for (step = 1U; step <= view.count; step++) {
        size_t candidate = previous
            ? (current + view.count - (step % view.count)) % view.count
            : (current + step) % view.count;
        if (view.eligible[candidate]) {
            session->focused_element_id = view.resolved[candidate].element_id;
            return UI_INTERACTION_OK;
        }
    }
    return UI_INTERACTION_NO_ELIGIBLE;
}

static int64_t absolute_i64(int64_t value) { return value < 0 ? -value : value; }

UiInteractionResult ui_interaction_focus_direction(
    UiInteractionSession *session, const UiDocument *document,
    const UiInteractionElementState *states, size_t state_count,
    int viewport_width, int viewport_height, UiInteractionDirection direction
) {
    InteractionView view;
    size_t current;
    size_t i;
    size_t best = UI_DOCUMENT_MAX_ELEMENTS;
    int64_t best_primary = INT64_MAX;
    int64_t best_secondary = INT64_MAX;
    int64_t current_x;
    int64_t current_y;
    UiInteractionResult result;
    if (!session || direction < UI_INTERACTION_DIRECTION_LEFT ||
        direction > UI_INTERACTION_DIRECTION_DOWN)
        return UI_INTERACTION_INVALID_ARGUMENT;
    result = build_view(document, states, state_count, viewport_width,
                        viewport_height, &view);
    if (result != UI_INTERACTION_OK) return result;
    current = resolved_index(&view, session->focused_element_id);
    if (current >= view.count || !view.eligible[current])
        return UI_INTERACTION_INVALID_SESSION;
    current_x = (int64_t)view.resolved[current].rect.x * 2 +
                view.resolved[current].rect.width;
    current_y = (int64_t)view.resolved[current].rect.y * 2 +
                view.resolved[current].rect.height;
    for (i = 0U; i < view.count; i++) {
        int64_t x;
        int64_t y;
        int64_t primary;
        int64_t secondary;
        bool in_direction;
        if (!view.eligible[i] || i == current) continue;
        x = (int64_t)view.resolved[i].rect.x * 2 + view.resolved[i].rect.width;
        y = (int64_t)view.resolved[i].rect.y * 2 + view.resolved[i].rect.height;
        if (direction == UI_INTERACTION_DIRECTION_LEFT ||
            direction == UI_INTERACTION_DIRECTION_RIGHT) {
            in_direction = direction == UI_INTERACTION_DIRECTION_LEFT
                ? x < current_x : x > current_x;
            primary = absolute_i64(x - current_x);
            secondary = absolute_i64(y - current_y);
        } else {
            in_direction = direction == UI_INTERACTION_DIRECTION_UP
                ? y < current_y : y > current_y;
            primary = absolute_i64(y - current_y);
            secondary = absolute_i64(x - current_x);
        }
        if (in_direction && (primary < best_primary ||
            (primary == best_primary && secondary < best_secondary) ||
            (primary == best_primary && secondary == best_secondary && i < best))) {
            best = i;
            best_primary = primary;
            best_secondary = secondary;
        }
    }
    if (best == UI_DOCUMENT_MAX_ELEMENTS) return UI_INTERACTION_NO_DIRECTIONAL_TARGET;
    session->focused_element_id = view.resolved[best].element_id;
    return UI_INTERACTION_OK;
}

UiInteractionResult ui_interaction_activate(
    const UiInteractionSession *session, const UiDocument *document,
    const UiInteractionElementState *states, size_t state_count,
    int viewport_width, int viewport_height,
    UiInteractionActivation *out_activation
) {
    InteractionView view;
    size_t index;
    size_t element_index;
    UiInteractionResult result;
    if (!session || !out_activation) return UI_INTERACTION_INVALID_ARGUMENT;
    result = build_view(document, states, state_count, viewport_width,
                        viewport_height, &view);
    if (result != UI_INTERACTION_OK) return result;
    index = resolved_index(&view, session->focused_element_id);
    if (index >= view.count || !view.eligible[index])
        return UI_INTERACTION_INVALID_SESSION;
    element_index = document_index(document, session->focused_element_id);
    out_activation->element_id = session->focused_element_id;
    out_activation->flow_port = document->elements[element_index].flow_port;
    return UI_INTERACTION_OK;
}