#include "ui_pause_motion.h"

#include <math.h>
#include <string.h>

static UiPauseMotionSample settled_sample(bool pause_visible) {
    UiPauseMotionSample sample = {0};
    sample.pause_visible = pause_visible;
    sample.decoration_visible = false;
    sample.phase = UI_MOTION_PHASE_COMPLETE;
    sample.progress = 1.0;
    sample.registration = pause_visible ? 1.0 : 0.0;
    return sample;
}

bool ui_pause_motion_init(UiPauseMotionState *state, bool pause_visible,
                          double now_ms, bool reduced_motion) {
    UiPauseMotionState candidate = {0};
    if (!state || !isfinite(now_ms) || now_ms < 0.0) return false;
    candidate.initialized = true;
    candidate.pause_visible = pause_visible;
    candidate.reduced_motion = reduced_motion;
    candidate.transition_active = false;
    candidate.last_time_ms = now_ms;
    candidate.transition = (UiMotionTransition){
        UI_PAUSE_MOTION_STABLE_ID, UI_THEME_MOTION_IMMEDIATE, now_ms,
        pause_visible ? 1.0 : 0.0, pause_visible ? 1.0 : 0.0,
        reduced_motion
    };
    memcpy(state, &candidate, sizeof(candidate));
    return true;
}

bool ui_pause_motion_update(UiPauseMotionState *state, bool pause_visible,
                            double now_ms, bool reduced_motion,
                            UiPauseMotionSample *out_sample) {
    UiPauseMotionState candidate;
    UiPauseMotionSample result = {0};
    UiMotionSample motion_sample;
    double current_value;

    if (!state || !out_sample || !state->initialized || !isfinite(now_ms) ||
        now_ms < state->last_time_ms) return false;
    candidate = *state;
    current_value = candidate.pause_visible ? 1.0 : 0.0;
    if (candidate.transition_active) {
        if (!ui_motion_sample(&candidate.transition, now_ms, &motion_sample)) return false;
        current_value = motion_sample.value;
    }

    if (pause_visible != candidate.pause_visible) {
        candidate.transition = (UiMotionTransition){
            UI_PAUSE_MOTION_STABLE_ID,
            pause_visible ? UI_THEME_MOTION_MAJOR_ENTER : UI_THEME_MOTION_MAJOR_EXIT,
            now_ms, current_value, pause_visible ? 1.0 : 0.0, reduced_motion
        };
        candidate.pause_visible = pause_visible;
        candidate.transition_active = true;
    } else if (reduced_motion != candidate.reduced_motion &&
               candidate.transition_active) {
        if (!ui_motion_redirect(&candidate.transition, now_ms,
                                candidate.transition.role,
                                candidate.pause_visible ? 1.0 : 0.0,
                                reduced_motion, &candidate.transition)) return false;
    }
    candidate.reduced_motion = reduced_motion;
    candidate.last_time_ms = now_ms;

    if (!candidate.transition_active) {
        result = settled_sample(candidate.pause_visible);
    } else {
        if (!ui_motion_sample(&candidate.transition, now_ms, &motion_sample)) return false;
        result.pause_visible = candidate.pause_visible;
        result.phase = motion_sample.phase;
        result.progress = motion_sample.progress;
        result.registration = motion_sample.value;
        result.decoration_visible = !reduced_motion &&
            motion_sample.phase != UI_MOTION_PHASE_COMPLETE;
        if (motion_sample.phase == UI_MOTION_PHASE_COMPLETE)
            candidate.transition_active = false;
    }
    memcpy(state, &candidate, sizeof(candidate));
    memcpy(out_sample, &result, sizeof(result));
    return true;
}