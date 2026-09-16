/** ui_theme_demo.h — Accepted full-palette diagnostic specimen. */
#ifndef UI_THEME_DEMO_H
#define UI_THEME_DEMO_H

#include "input.h"
#include "ui_canvas.h"

#include <stdbool.h>

#define UI_THEME_DEMO_WIDTH 96
#define UI_THEME_DEMO_HEIGHT 56

typedef struct {
    int scale_percent;
} UiThemeDemoState;

typedef enum {
    UI_THEME_DEMO_RENDER_OK = 0,
    UI_THEME_DEMO_RENDER_INVALID_ARGUMENT,
    UI_THEME_DEMO_RENDER_OUT_OF_MEMORY,
    UI_THEME_DEMO_RENDER_INVARIANT_FAILED
} UiThemeDemoRenderResult;

void ui_theme_demo_state_init(UiThemeDemoState *state);

/** Applies session-only controls; outputs are unchanged on invalid arguments. */
bool ui_theme_demo_apply_input(UiThemeDemoState *state, const InputState *input,
                               bool *out_should_exit);

/** Replaces canvas contents transactionally with the deterministic specimen. */
UiThemeDemoRenderResult ui_theme_demo_render(const UiThemeDemoState *state,
                                              UiCanvas *canvas);

#endif