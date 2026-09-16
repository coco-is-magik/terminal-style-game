#include "ui_motion.h"

#include <math.h>

static bool role_is_valid(UiThemeMotionRole role) {
    return role >= UI_THEME_MOTION_IMMEDIATE && role < UI_THEME_MOTION_ROLE_COUNT;
}

bool ui_motion_sample(const UiMotionTransition *transition, double sample_time_ms,
                      UiMotionSample *out_sample) {
    UiMotionSample sample;
    unsigned int duration;
    double elapsed;

    if (!transition || !out_sample || transition->stable_id == 0U ||
        !role_is_valid(transition->role) || !isfinite(transition->start_time_ms) ||
        transition->start_time_ms < 0.0 || !isfinite(sample_time_ms) ||
        sample_time_ms < 0.0 || !isfinite(transition->start_value) ||
        !isfinite(transition->end_value)) return false;

    duration = ui_theme_motion_duration_ms(transition->role,
                                           transition->reduced_motion);
    if (sample_time_ms < transition->start_time_ms && duration != 0U) {
        sample.phase = UI_MOTION_PHASE_PENDING;
        sample.progress = 0.0;
        sample.value = transition->start_value;
    } else {
        elapsed = sample_time_ms < transition->start_time_ms
            ? 0.0 : sample_time_ms - transition->start_time_ms;
        if (!ui_theme_motion_progress(transition->role, elapsed,
                                      transition->reduced_motion,
                                      &sample.progress) ||
            !ui_theme_motion_value(transition->role, elapsed,
                                   transition->reduced_motion,
                                   transition->start_value,
                                   transition->end_value, &sample.value)) return false;
        sample.phase = duration == 0U || elapsed >= (double)duration
            ? UI_MOTION_PHASE_COMPLETE : UI_MOTION_PHASE_ACTIVE;
    }
    *out_sample = sample;
    return true;
}

bool ui_motion_redirect(const UiMotionTransition *current, double sample_time_ms,
                        UiThemeMotionRole new_role, double new_end_value,
                        bool reduced_motion, UiMotionTransition *out_transition) {
    UiMotionSample current_sample;
    UiMotionTransition redirected;

    if (!out_transition || !role_is_valid(new_role) || !isfinite(new_end_value) ||
        !ui_motion_sample(current, sample_time_ms, &current_sample)) return false;
    redirected.stable_id = current->stable_id;
    redirected.role = new_role;
    redirected.start_time_ms = sample_time_ms;
    redirected.start_value = current_sample.value;
    redirected.end_value = new_end_value;
    redirected.reduced_motion = reduced_motion;
    *out_transition = redirected;
    return true;
}

bool ui_motion_glyph_offset(uint32_t stable_id, size_t glyph_index,
                            size_t glyph_count, double progress,
                            bool reduced_motion, UiMotionOffset *out_offset) {
    static const UiMotionOffset paths[] = {
        {-3, 0}, {3, 0}, {0, -2}, {0, 2}, {-2, -1}, {2, 1}
    };
    UiMotionOffset offset;
    size_t path_index;
    double remaining;

    if (!out_offset || stable_id == 0U || glyph_count == 0U ||
        glyph_index >= glyph_count || !isfinite(progress) ||
        progress < 0.0 || progress > 1.0) return false;
    if (reduced_motion || progress >= 1.0) {
        offset = (UiMotionOffset){0, 0};
    } else {
        path_index = ((size_t)stable_id * 5U + glyph_index * 3U) %
                     (sizeof(paths) / sizeof(paths[0]));
        remaining = 1.0 - progress;
        offset.x = (int)lround((double)paths[path_index].x * remaining);
        offset.y = (int)lround((double)paths[path_index].y * remaining);
    }
    *out_offset = offset;
    return true;
}