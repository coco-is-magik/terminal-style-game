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
/* The transition a surface takes when it hands over to the world frame instead of
 * to another surface: the material on `grid` (the outgoing surface, already drawn)
 * condenses onto its lattice, cycles through the fabric's glyphs as the backdrop
 * wave crosses it, and then thins away, so the live frame underneath is left
 * uncovered by the end of the exit window. `outgoing` supplies the surface anchor
 * the primitive addresses; the transition itself is chosen by the caller because
 * only the caller knows the destination (2026-10-09 direction record, item 4,
 * Stage C). Reduced motion draws nothing, which is the recorded no-motion
 * response. */
bool ui_animation_render_world_settle(UiLayout *outgoing, Grid *grid,
                                      double elapsed_ms, bool reduced_motion);

#endif