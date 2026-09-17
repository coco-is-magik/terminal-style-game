/** ui_animation.h — Shared rendering for fixed reusable application-UI motion units. */
#ifndef UI_ANIMATION_H
#define UI_ANIMATION_H

#include "ui_canvas.h"
#include "ui_ele.h"

typedef enum {
    UI_ANIMATION_EVENT_CONTEXT_ENTER = 0,
    UI_ANIMATION_EVENT_CONTEXT_EXIT,
    UI_ANIMATION_EVENT_FOCUS,
    UI_ANIMATION_EVENT_ACTIVATE,
    UI_ANIMATION_EVENT_WHILE_VISIBLE,
    UI_ANIMATION_EVENT_PREVIEW
} UiAnimationEvent;

bool ui_animation_render_pause_glitch_canvas(UiCanvas *canvas,
                                             const UiElementLayout *bounds,
                                             double progress,
                                             bool reduced_motion);
bool ui_animation_render_layout(UiLayout *layout, Grid *grid, double elapsed_ms,
                                bool reduced_motion, bool preview_loop,
                                UiAnimationEvent event);

#endif