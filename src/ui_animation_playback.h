/** ui_animation_playback.h — Deterministic explicit-time UI Scene playback state. */
#ifndef UI_ANIMATION_PLAYBACK_H
#define UI_ANIMATION_PLAYBACK_H

#include "ui_animation.h"
#include "ui_document.h"

#include <stdbool.h>

typedef struct {
    bool initialized;
    double initialized_at_ms;
    double last_event_at_ms;
    bool context_active;
    UiAnimationEvent context_event;
    double context_event_at_ms;
    UiElementId focused_element_id;
    double focus_event_at_ms;
    UiElementId activated_element_id;
    double activate_event_at_ms;
} UiAnimationPlayback;

typedef struct {
    bool visible;
    bool complete;
    double elapsed_ms;
    double progress;
} UiAnimationPlaybackSample;

bool ui_animation_playback_init(UiAnimationPlayback *playback, double now_ms);

/** Applies one lifecycle event. Invalid or backward time leaves playback unchanged. */
bool ui_animation_playback_event(UiAnimationPlayback *playback,
                                 UiAnimationEvent event,
                                 UiElementId target_id,
                                 double now_ms);

/** Samples an authored Animation element without mutating playback. */
bool ui_animation_playback_sample(const UiAnimationPlayback *playback,
                                  const UiDocumentAnimation *animation,
                                  bool target_visible,
                                  double now_ms,
                                  bool reduced_motion,
                                  UiAnimationPlaybackSample *out_sample);

#endif /* UI_ANIMATION_PLAYBACK_H */