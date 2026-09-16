/** ui_motion_demo.h — Diagnostic-only motion vocabulary specimen. */
#ifndef UI_MOTION_DEMO_H
#define UI_MOTION_DEMO_H

#include "input.h"
#include "ui_canvas.h"

#include <stdbool.h>

#define UI_MOTION_DEMO_WIDTH 96
#define UI_MOTION_DEMO_HEIGHT 56
#define UI_MOTION_DEMO_CYCLE_MS 800.0
#define UI_MOTION_DEMO_STEP_MS 20.0

typedef struct {
    int scale_percent;
    bool paused;
    bool reduced_motion;
    double playback_origin_ms;
    double paused_elapsed_ms;
} UiMotionDemoState;

typedef enum {
    UI_MOTION_DEMO_RENDER_OK = 0,
    UI_MOTION_DEMO_RENDER_INVALID_ARGUMENT,
    UI_MOTION_DEMO_RENDER_OUT_OF_MEMORY,
    UI_MOTION_DEMO_RENDER_INVARIANT_FAILED
} UiMotionDemoRenderResult;

void ui_motion_demo_state_init(UiMotionDemoState *state, double now_ms);
bool ui_motion_demo_elapsed_ms(const UiMotionDemoState *state, double now_ms,
                               double *out_elapsed_ms);

/** Applies controls transactionally; now_ms is an explicit monotonic timestamp. */
bool ui_motion_demo_apply_input(UiMotionDemoState *state, const InputState *input,
                                double now_ms, bool *out_should_exit);

/** Replaces the canvas transactionally with one deterministic sampled frame. */
UiMotionDemoRenderResult ui_motion_demo_render(const UiMotionDemoState *state,
                                               double now_ms, UiCanvas *canvas);

#endif