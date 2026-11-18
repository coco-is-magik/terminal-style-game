/**
 * font_fit.h — fitting one decoded glyph into the 8x8 cell, and saying exactly why it will not
 *
 * The converter's job is font -> 8x8 (V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md, D6). That
 * means most of the interesting behaviour is not "convert" but "what happens when the font does
 * not fit", and D11 is explicit about it: never silent, name the glyph and the reason, offer
 * remedies that are off by default and explain what they cost.
 *
 * This module is the policy and none of the plumbing. It takes a decoded 1bpp mask — whatever
 * produced it, FreeType or a test — and either produces a cell or refuses with enough detail to
 * print a sentence a user can act on. No file access, no font parsing, no allocation, no printing:
 * deterministic integer arithmetic only, so two machines given the same mask agree byte for byte.
 *
 * The four fit modes are genuinely different policies, not an ordering:
 *
 *   NONE       the glyph's mask must already be at most 8x8. Ink may sit anywhere in it.
 *   SQUASH     anisotropic nearest-neighbour: map width and height onto 8 independently. Keeps
 *              1px strokes and distorts proportion. Right for 9x8, wrong for 16x16.
 *   SCALE      isotropic box filter by the integer factor that brings the longer axis to 8, then
 *              centre. Keeps proportion, thickens hairlines. Right for 16x16, wrong for 9x8.
 *   NORMALIZE  no resampling at all: crop to ink and reposition (centre horizontally, baseline to
 *              the bottom row). Only valid when the ink itself already fits, so it is the remedy
 *              for "present but placed for a different box", never for "too big".
 *
 * None of these is applied implicitly. A mode that changes pixels is a choice the caller makes,
 * because the alternative — fitting silently — hides an unsuitable font until the text is
 * unreadable, which is the failure D11 exists to prevent.
 */

#ifndef FONT_FIT_H
#define FONT_FIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FONT_FIT_CELL_SIZE 8U

/* Largest strike this module will consider. 32 covers 16x16 and 24x24 strikes; anything bigger is
   a font whose design is far from 8x8, and refusing it outright is more honest than resampling it
   into illegibility. */
#define FONT_FIT_SOURCE_MAX 32U

/* Below this fraction of covered subpixels a scaled cell is empty. A quarter, not a half: at 8x8 a
   hairline that survives is worth a slightly bolder stroke, whereas a threshold that erases
   strokes yields a legible-looking font with missing strokes — which reads as a broken glyph
   rather than a scaled one. */
#define FONT_FIT_SCALE_COVERAGE_NUMERATOR 1U
#define FONT_FIT_SCALE_COVERAGE_DENOMINATOR 4U

typedef enum {
    FONT_FIT_MODE_NONE = 0,
    FONT_FIT_MODE_SQUASH,
    FONT_FIT_MODE_SCALE,
    FONT_FIT_MODE_NORMALIZE
} FontFitMode;

typedef enum {
    FONT_FIT_OK = 0,
    /* The glyph is larger than the cell and the requested mode cannot or may not change it. */
    FONT_FIT_TOO_LARGE,
    /* Dimensions exceed FONT_FIT_SOURCE_MAX, or a NULL argument. */
    FONT_FIT_INVALID_ARGUMENT
} FontFitResult;

/* A decoded glyph, 1bpp. Bit x of row y is the pixel at (x, y), x = 0 leftmost. `width`/`height`
   describe the strike the font produced, which is not the same as its ink: a 16x16 strike holding
   a 6x8 letter is 16x16 for fitting purposes, because that is the design the font is drawing in. */
typedef struct {
    unsigned width;
    unsigned height;
    uint32_t rows[FONT_FIT_SOURCE_MAX];
} FontFitMask;

/* A fitted cell: one byte per row, bit 0 leftmost, matching the renderer's 8x8 glyph layout. */
typedef struct {
    uint8_t rows[FONT_FIT_CELL_SIZE];
} FontFitCell;

/* What happened, in fields rather than prose, so a caller can aggregate and a test can assert. */
typedef struct {
    FontFitResult result;
    FontFitMode mode;
    unsigned source_width;
    unsigned source_height;
    /* Ink bounding box within the strike. An empty glyph (space) has ink_width 0. */
    unsigned ink_left;
    unsigned ink_top;
    unsigned ink_width;
    unsigned ink_height;
    /* How far past the cell the strike is; 0 when it fits. */
    unsigned overflow_columns;
    unsigned overflow_rows;
    /* What the applied mode discarded or changed, so the cost can be stated. */
    unsigned dropped_columns;
    unsigned dropped_rows;
    unsigned scale_factor;
    bool repositioned;
} FontFitReport;

/* Total; zeroes the mask. */
void font_fit_mask_init(FontFitMask *mask);

/* Bounds-checked pixel access, for building masks and for tests. Out-of-range reads are false;
   out-of-range writes are refused rather than clamped, because a caller with a coordinate bug
   should not get a plausible-looking glyph. */
bool font_fit_mask_set(FontFitMask *mask, unsigned x, unsigned y);
bool font_fit_mask_get(const FontFitMask *mask, unsigned x, unsigned y);

/* Answer "does this fit, and if not, exactly how" without changing anything. Reports the ink box
   and the overflow for every mode, so one probe can drive the message for all remedies. */
FontFitResult font_fit_probe(const FontFitMask *source, FontFitReport *report);

/* Produce a cell under `mode`. On FONT_FIT_TOO_LARGE nothing is written to `out_cell` and the
   report says how much must be given up and by which remedy. `report` may be NULL. */
FontFitResult font_fit_apply(const FontFitMask *source, FontFitMode mode,
                             FontFitCell *out_cell, FontFitReport *report);

/* One bounded sentence describing a report, for tool output and for test failure messages.
 * Returns false (and an empty string) when the buffer is too small or an argument is NULL, so a
 * caller never prints a truncated reason. */
bool font_fit_describe(const FontFitReport *report, char *buffer, size_t buffer_size);

/* Stable short name of a mode, for command lines and diagnostics. */
const char *font_fit_mode_name(FontFitMode mode);

#endif /* FONT_FIT_H */
