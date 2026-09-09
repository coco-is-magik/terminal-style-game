/** ui_interaction.h — Pure authored UI hit, focus, and activation semantics. */
#ifndef UI_INTERACTION_H
#define UI_INTERACTION_H

#include "ui_layout_resolver.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    UiElementId element_id;
    bool disabled;
    bool visible;
} UiInteractionElementState;

typedef struct {
    UiElementId focused_element_id;
} UiInteractionSession;

typedef struct {
    UiElementId element_id;
    const char *flow_port;
} UiInteractionActivation;

typedef enum {
    UI_INTERACTION_DIRECTION_LEFT = 0,
    UI_INTERACTION_DIRECTION_RIGHT,
    UI_INTERACTION_DIRECTION_UP,
    UI_INTERACTION_DIRECTION_DOWN
} UiInteractionDirection;

typedef enum {
    UI_INTERACTION_OK = 0,
    UI_INTERACTION_INVALID_ARGUMENT,
    UI_INTERACTION_INVALID_DOCUMENT,
    UI_INTERACTION_INVALID_STATE,
    UI_INTERACTION_INVALID_SESSION,
    UI_INTERACTION_NO_ELIGIBLE,
    UI_INTERACTION_NO_HIT,
    UI_INTERACTION_NO_DIRECTIONAL_TARGET
} UiInteractionResult;

UiInteractionResult ui_interaction_session_init(
    UiInteractionSession *session,
    const UiDocument *document,
    const UiInteractionElementState *states,
    size_t state_count,
    int viewport_width,
    int viewport_height
);
UiInteractionResult ui_interaction_hit_test(
    const UiDocument *document,
    const UiInteractionElementState *states,
    size_t state_count,
    int viewport_width,
    int viewport_height,
    int pointer_x,
    int pointer_y,
    UiElementId *out_element_id
);
UiInteractionResult ui_interaction_focus_pointer(
    UiInteractionSession *session,
    const UiDocument *document,
    const UiInteractionElementState *states,
    size_t state_count,
    int viewport_width,
    int viewport_height,
    int pointer_x,
    int pointer_y
);
UiInteractionResult ui_interaction_focus_next(
    UiInteractionSession *session,
    const UiDocument *document,
    const UiInteractionElementState *states,
    size_t state_count,
    int viewport_width,
    int viewport_height,
    bool previous
);
UiInteractionResult ui_interaction_focus_direction(
    UiInteractionSession *session,
    const UiDocument *document,
    const UiInteractionElementState *states,
    size_t state_count,
    int viewport_width,
    int viewport_height,
    UiInteractionDirection direction
);
UiInteractionResult ui_interaction_activate(
    const UiInteractionSession *session,
    const UiDocument *document,
    const UiInteractionElementState *states,
    size_t state_count,
    int viewport_width,
    int viewport_height,
    UiInteractionActivation *out_activation
);

#endif