/** ui_pause_motion.h — Pure presentation state for the pause major context. */
#ifndef UI_PAUSE_MOTION_H
#define UI_PAUSE_MOTION_H

#include "ui_motion.h"

#include <stdbool.h>
#include <stdint.h>

#define UI_PAUSE_MOTION_STABLE_ID UINT32_C(0x50415553)

typedef struct {
    bool initialized;
    bool pause_visible;
    bool reduced_motion;
    bool transition_active;
    double last_time_ms;
    UiMotionTransition transition;
} UiPauseMotionState;

typedef struct {
    bool pause_visible;
    bool decoration_visible;
    UiMotionPhase phase;
    double progress;
    double registration;
} UiPauseMotionSample;

/** Initializes an inert state transactionally from explicit context visibility and time. */
bool ui_pause_motion_init(UiPauseMotionState *state, bool pause_visible,
                          double now_ms, bool reduced_motion);

/**
 * Observes immediate external pause visibility and returns presentation-only motion.
 * State and output are unchanged on invalid or backward time.
 */
bool ui_pause_motion_update(UiPauseMotionState *state, bool pause_visible,
                            double now_ms, bool reduced_motion,
                            UiPauseMotionSample *out_sample);

#endif