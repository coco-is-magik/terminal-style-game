/**
 * ui_underlay.h — Deterministic frozen-frame underlay for layered surfaces
 *
 * A menu or floating panel can be shown over the frame that was on screen when
 * it opened, instead of over a blank or black surface. The underlay is a pure
 * snapshot: capture the frame once, then repaint it with the theme's overlay
 * response so the world behind the controls recedes.
 *
 * The underlay is decoration. It repaints background material only: it never
 * moves a control, a glyph, a focus marker, or a hit target, and reduced motion
 * needs no special case because nothing here is time-based. See
 * UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §1.1 ("keep the controls dependable")
 * and §4.4 (nothing in motion displaces or obscures a control).
 *
 * Deterministic: no clock, no randomness, no frame-count progression, no global
 * state. The same captured frame and the same strengths always produce the same
 * cells, so the same inputs replay exactly.
 */
#ifndef UI_UNDERLAY_H
#define UI_UNDERLAY_H

#include "grid.h"
#include "ui_theme.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    UI_UNDERLAY_OK = 0,
    UI_UNDERLAY_INVALID_ARGUMENT,
    UI_UNDERLAY_SIZE_MISMATCH,
    UI_UNDERLAY_NO_FRAME,
    /* The frame offered was entirely empty, so it is not a background. */
    UI_UNDERLAY_EMPTY_FRAME
} UiUnderlayResult;

typedef struct {
    Cell *cells;
    int width;
    int height;
    size_t cell_count;
    bool captured;
} UiUnderlay;

/** Allocates the snapshot store for a width x height cell frame. */
bool ui_underlay_init(UiUnderlay *underlay, int width, int height);
void ui_underlay_destroy(UiUnderlay *underlay);

/**
 * Captures `frame` as the frozen background.
 *
 * Explicit by design: the caller decides which single frame sits behind the
 * surface, so a later frame cannot silently replace it. A frame whose cells are
 * all empty is refused (UI_UNDERLAY_EMPTY_FRAME) and leaves any previous capture
 * untouched, because an empty frame carries no background colour.
 */
UiUnderlayResult ui_underlay_capture(UiUnderlay *underlay, const Grid *frame);

/** Drops the captured frame so the next surface captures a fresh one. */
void ui_underlay_forget(UiUnderlay *underlay);
bool ui_underlay_has_frame(const UiUnderlay *underlay);

/**
 * Repaints the frozen frame into `grid` through the overlay response.
 *
 * `dim_percent` and `grey_percent` are strengths supplied by the caller (the
 * application passes the ui_theme tokens). 0 for both restores the captured
 * frame exactly, which is how "the underlay is optional" is expressed without a
 * second code path.
 *
 * `grid` must have the captured dimensions.
 */
UiUnderlayResult ui_underlay_apply(const UiUnderlay *underlay, Grid *grid,
                                   int dim_percent, int grey_percent);

/**
 * Applies the overlay response to one colour: desaturate toward its own
 * luminance by `grey_percent`, then darken by `dim_percent`. Pure integer math
 * with explicit rounding, so results are reproducible. Strengths are clamped to
 * 0..100; `out_color` is unchanged on failure.
 */
bool ui_underlay_response(UiThemeColor source, int dim_percent, int grey_percent,
                          UiThemeColor *out_color);

#endif /* UI_UNDERLAY_H */
