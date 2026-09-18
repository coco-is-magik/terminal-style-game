#include "ui_animation_playback.h"

#include "ui_theme.h"

#include <math.h>
#include <string.h>

static bool valid_event(UiAnimationEvent event) {
    return event >= UI_ANIMATION_EVENT_CONTEXT_ENTER &&
           event <= UI_ANIMATION_EVENT_WHILE_VISIBLE;
}

static bool valid_animation(const UiDocumentAnimation *animation) {
    return animation && animation->target_id != 0U &&
           animation->trigger >= UI_DOCUMENT_ANIMATION_TRIGGER_CONTEXT_ENTER &&
           animation->trigger <= UI_DOCUMENT_ANIMATION_TRIGGER_WHILE_VISIBLE;
}

static UiAnimationEvent trigger_event(UiDocumentAnimationTrigger trigger) {
    return (UiAnimationEvent)trigger;
}

bool ui_animation_playback_init(UiAnimationPlayback *playback, double now_ms) {
    UiAnimationPlayback candidate = {0};
    if (!playback || !isfinite(now_ms) || now_ms < 0.0) return false;
    candidate.initialized = true;
    candidate.initialized_at_ms = now_ms;
    candidate.last_event_at_ms = now_ms;
    candidate.context_event = UI_ANIMATION_EVENT_CONTEXT_ENTER;
    candidate.context_event_at_ms = now_ms;
    memcpy(playback, &candidate, sizeof(candidate));
    return true;
}

bool ui_animation_playback_event(UiAnimationPlayback *playback,
                                 UiAnimationEvent event,
                                 UiElementId target_id,
                                 double now_ms) {
    UiAnimationPlayback candidate;
    if (!playback || !playback->initialized || !valid_event(event) ||
        event == UI_ANIMATION_EVENT_PREVIEW || !isfinite(now_ms) ||
        now_ms < playback->last_event_at_ms) return false;
    if ((event == UI_ANIMATION_EVENT_FOCUS ||
         event == UI_ANIMATION_EVENT_ACTIVATE) == (target_id == 0U)) return false;
    candidate = *playback;
    candidate.last_event_at_ms = now_ms;
    if (event == UI_ANIMATION_EVENT_CONTEXT_ENTER ||
        event == UI_ANIMATION_EVENT_CONTEXT_EXIT) {
        candidate.context_active = true;
        candidate.context_event = event;
        candidate.context_event_at_ms = now_ms;
    } else if (event == UI_ANIMATION_EVENT_FOCUS) {
        candidate.focused_element_id = target_id;
        candidate.focus_event_at_ms = now_ms;
    } else if (event == UI_ANIMATION_EVENT_ACTIVATE) {
        candidate.activated_element_id = target_id;
        candidate.activate_event_at_ms = now_ms;
    }
    *playback = candidate;
    return true;
}

static bool event_start(const UiAnimationPlayback *playback,
                        const UiDocumentAnimation *animation,
                        bool target_visible,
                        double *out_start) {
    UiAnimationEvent event = trigger_event(animation->trigger);
    if (event == UI_ANIMATION_EVENT_WHILE_VISIBLE) {
        if (!target_visible) return false;
        *out_start = playback->initialized_at_ms;
        return true;
    }
    if (event == UI_ANIMATION_EVENT_CONTEXT_ENTER ||
        event == UI_ANIMATION_EVENT_CONTEXT_EXIT) {
        if (!playback->context_active || playback->context_event != event) return false;
        *out_start = playback->context_event_at_ms;
        return true;
    }
    if (event == UI_ANIMATION_EVENT_FOCUS) {
        if (playback->focused_element_id != animation->target_id) return false;
        *out_start = playback->focus_event_at_ms;
        return true;
    }
    if (playback->activated_element_id != animation->target_id) return false;
    *out_start = playback->activate_event_at_ms;
    return true;
}

bool ui_animation_playback_sample(const UiAnimationPlayback *playback,
                                  const UiDocumentAnimation *animation,
                                  bool target_visible,
                                  double now_ms,
                                  bool reduced_motion,
                                  UiAnimationPlaybackSample *out_sample) {
    UiAnimationPlaybackSample sample = {0};
    UiThemeMotionRole role;
    unsigned int duration;
    double start;
    double elapsed;
    if (!playback || !playback->initialized || !valid_animation(animation) ||
        !out_sample || !isfinite(now_ms) || now_ms < playback->last_event_at_ms)
        return false;
    if (reduced_motion || !event_start(playback, animation, target_visible, &start)) {
        sample.complete = reduced_motion;
        sample.progress = 1.0;
        *out_sample = sample;
        return true;
    }
    if (now_ms < start) return false;
    role = animation->trigger == UI_DOCUMENT_ANIMATION_TRIGGER_CONTEXT_EXIT
        ? UI_THEME_MOTION_MAJOR_EXIT : UI_THEME_MOTION_MAJOR_ENTER;
    duration = ui_theme_motion_duration_ms(role, false);
    elapsed = now_ms - start;
    if (animation->loop ||
        animation->trigger == UI_DOCUMENT_ANIMATION_TRIGGER_WHILE_VISIBLE) {
        if (duration == 0U) return false;
        elapsed = fmod(elapsed, (double)duration);
    } else if (elapsed >= (double)duration) {
        sample.complete = true;
        sample.elapsed_ms = elapsed;
        sample.progress = animation->trigger == UI_DOCUMENT_ANIMATION_TRIGGER_CONTEXT_EXIT
            ? 0.0 : 1.0;
        *out_sample = sample;
        return true;
    }
    if (!ui_theme_motion_progress(role, elapsed, false, &sample.progress)) return false;
    if (animation->trigger == UI_DOCUMENT_ANIMATION_TRIGGER_CONTEXT_EXIT)
        sample.progress = 1.0 - sample.progress;
    sample.visible = true;
    sample.elapsed_ms = elapsed;
    *out_sample = sample;
    return true;
}