/** ui_motion.h — Pure explicit-time registration and glyph-reassembly model. */
#ifndef UI_MOTION_H
#define UI_MOTION_H

#include "ui_theme.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    UI_MOTION_PHASE_PENDING = 0,
    UI_MOTION_PHASE_ACTIVE,
    UI_MOTION_PHASE_COMPLETE
} UiMotionPhase;

typedef struct {
    uint32_t stable_id;
    UiThemeMotionRole role;
    double start_time_ms;
    double start_value;
    double end_value;
    bool reduced_motion;
} UiMotionTransition;

typedef struct {
    UiMotionPhase phase;
    double progress;
    double value;
} UiMotionSample;

typedef struct {
    int x;
    int y;
} UiMotionOffset;

/** Samples one transition transactionally at an explicit monotonic timestamp. */
bool ui_motion_sample(const UiMotionTransition *transition, double sample_time_ms,
                      UiMotionSample *out_sample);

/**
 * Starts a new transition from the currently resolved value. This is the only
 * interruption/reversal operation; no hidden clock or mutable engine state exists.
 */
bool ui_motion_redirect(const UiMotionTransition *current, double sample_time_ms,
                        UiThemeMotionRole new_role, double new_end_value,
                        bool reduced_motion, UiMotionTransition *out_transition);

/**
 * Returns a deterministic cell offset for one glyph. Stable IDs select paths;
 * array order and frame count never affect the result.
 */
bool ui_motion_glyph_offset(uint32_t stable_id, size_t glyph_index,
                            size_t glyph_count, double progress,
                            bool reduced_motion, UiMotionOffset *out_offset);

#endif