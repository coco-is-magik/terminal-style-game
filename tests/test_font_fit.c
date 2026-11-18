/**
 * test_font_fit.c — the 8x8 fitting policy, pinned down pixel by pixel
 *
 * These tests are deliberately about exact bytes. "It scaled" is not a property anyone can act
 * on: what matters is that the result is the same on every machine, that a mode which promises
 * not to resample did not resample, and that a refusal names the overshoot rather than producing
 * a plausible-looking cell.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <string.h>

#include "../src/font_fit.h"

static FontFitMask make_mask(unsigned width, unsigned height) {
    FontFitMask mask;
    font_fit_mask_init(&mask);
    mask.width = width;
    mask.height = height;
    return mask;
}

/* Filled rectangle of ink, bounds-checked: a test that writes outside its mask is a broken test,
   not a broken font, and must say so instead of silently testing nothing. */
static void fill_rect(FontFitMask *mask, unsigned x0, unsigned y0, unsigned x1, unsigned y1) {
    unsigned x;
    unsigned y;
    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            assert_true(font_fit_mask_set(mask, x, y));
        }
    }
}

/* A 16x16 one-pixel ring: the classic bitmap-font shape whose strokes a naive majority filter
   erases. */
static FontFitMask make_ring_16(void) {
    FontFitMask mask = make_mask(16U, 16U);
    fill_rect(&mask, 0U, 0U, 16U, 1U);
    fill_rect(&mask, 0U, 15U, 16U, 16U);
    fill_rect(&mask, 0U, 0U, 1U, 16U);
    fill_rect(&mask, 15U, 0U, 16U, 16U);
    return mask;
}

static void test_a_glyph_that_already_fits_is_untouched(void **state) {
    FontFitMask mask = make_mask(8U, 8U);
    FontFitCell cell;
    FontFitReport report;
    (void)state;
    fill_rect(&mask, 0U, 0U, 8U, 8U);
    assert_int_equal(font_fit_probe(&mask, &report), FONT_FIT_OK);
    assert_int_equal(report.overflow_columns, 0U);
    assert_int_equal(report.overflow_rows, 0U);
    assert_int_equal(report.ink_width, 8U);
    assert_int_equal(report.ink_height, 8U);

    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_NONE, &cell, &report), FONT_FIT_OK);
    assert_int_equal(cell.rows[0], 0xFFU);
    assert_int_equal(cell.rows[7], 0xFFU);
    /* Nothing was given up, and the report says so rather than leaving it to be inferred. */
    assert_int_equal(report.dropped_columns, 0U);
    assert_int_equal(report.dropped_rows, 0U);
    assert_false(report.repositioned);
}

static void test_strict_mode_refuses_one_column_too_wide_without_writing(void **state) {
    FontFitMask mask = make_mask(9U, 8U);
    FontFitCell cell;
    FontFitReport report;
    (void)state;
    fill_rect(&mask, 0U, 0U, 1U, 8U);  /* left rail */
    fill_rect(&mask, 8U, 0U, 9U, 8U);  /* right rail, the column that will be dropped */
    assert_int_equal(font_fit_probe(&mask, &report), FONT_FIT_TOO_LARGE);
    assert_int_equal(report.overflow_columns, 1U);
    assert_int_equal(report.overflow_rows, 0U);

    memset(&cell, 0xA5, sizeof(cell));
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_NONE, &cell, &report),
                     FONT_FIT_TOO_LARGE);
    /* A refusal must not leave half a glyph behind for a caller that forgot to check. */
    assert_int_equal(cell.rows[0], 0xA5U);
    assert_int_equal(report.result, FONT_FIT_TOO_LARGE);
}

static void test_squash_drops_the_unsampled_column_and_says_so(void **state) {
    FontFitMask mask = make_mask(9U, 8U);
    FontFitCell cell;
    FontFitReport report;
    (void)state;
    fill_rect(&mask, 0U, 0U, 1U, 8U);
    fill_rect(&mask, 8U, 0U, 9U, 8U);
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_SQUASH, &cell, &report), FONT_FIT_OK);
    assert_int_equal(report.dropped_columns, 1U);
    assert_int_equal(report.dropped_rows, 0U);
    /* x = 7 samples source column 7, so the rail at column 8 is the one lost; the left rail
       survives because the sampled grid stays anchored at 0. */
    assert_int_equal(cell.rows[0], 0x01U);
    assert_int_equal(cell.rows[7], 0x01U);
}

static void test_squash_keeps_an_axis_that_already_fits(void **state) {
    FontFitMask mask = make_mask(9U, 8U);
    FontFitCell cell;
    FontFitReport report;
    (void)state;
    /* One ink pixel in the first column and one in the last: only the width overflows, so the
       height must come through untouched rather than being resampled with it. */
    assert_true(font_fit_mask_set(&mask, 0U, 2U));
    assert_true(font_fit_mask_set(&mask, 8U, 5U));
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_SQUASH, &cell, &report), FONT_FIT_OK);
    assert_int_equal(cell.rows[2], 0x01U);
    assert_int_equal(cell.rows[5], 0x00U);
    assert_int_equal(report.dropped_rows, 0U);
}

static void test_squash_one_row_too_tall(void **state) {
    FontFitMask mask = make_mask(8U, 9U);
    FontFitCell cell;
    FontFitReport report;
    (void)state;
    fill_rect(&mask, 0U, 0U, 8U, 1U); /* top row */
    fill_rect(&mask, 0U, 8U, 8U, 9U); /* bottom row, will be dropped */
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_SQUASH, &cell, &report), FONT_FIT_OK);
    assert_int_equal(report.dropped_rows, 1U);
    assert_int_equal(report.dropped_columns, 0U);
    assert_int_equal(cell.rows[0], 0xFFU);
    assert_int_equal(cell.rows[7], 0x00U);
}

static void test_scale_preserves_a_hairline_ring_at_half_size(void **state) {
    FontFitMask mask = make_ring_16();
    FontFitCell cell;
    FontFitReport report;
    unsigned row;
    (void)state;
    assert_int_equal(font_fit_probe(&mask, &report), FONT_FIT_TOO_LARGE);
    assert_int_equal(report.overflow_columns, 8U);
    assert_int_equal(report.overflow_rows, 8U);
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_SCALE, &cell, &report), FONT_FIT_OK);
    assert_int_equal(report.scale_factor, 2U);
    /* The reason for the quarter-coverage threshold: a 1px ring scaled by 2 has 2 or 3 of 4
       subpixels covered, so it survives as a 1px ring instead of vanishing. A half-coverage
       threshold would leave a blank cell here, and a blank glyph reads as a font bug. */
    assert_int_equal(cell.rows[0], 0xFFU);
    assert_int_equal(cell.rows[7], 0xFFU);
    for (row = 1U; row < 7U; row++) {
        assert_int_equal(cell.rows[row], 0x81U);
    }
}

static void test_scale_of_solid_16_is_solid(void **state) {
    FontFitMask mask = make_mask(16U, 16U);
    FontFitCell cell;
    FontFitReport report;
    unsigned row;
    (void)state;
    fill_rect(&mask, 0U, 0U, 16U, 16U);
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_SCALE, &cell, &report), FONT_FIT_OK);
    assert_int_equal(report.scale_factor, 2U);
    for (row = 0U; row < 8U; row++) {
        assert_int_equal(cell.rows[row], 0xFFU);
    }
}

static void test_scale_reports_factor_one_when_nothing_to_scale(void **state) {
    FontFitMask mask = make_mask(6U, 6U);
    FontFitCell cell;
    FontFitReport report;
    (void)state;
    fill_rect(&mask, 0U, 0U, 6U, 6U);
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_SCALE, &cell, &report), FONT_FIT_OK);
    /* Not 0, and not a lie: no reduction happened. */
    assert_int_equal(report.scale_factor, 1U);
    assert_int_equal(cell.rows[0], 0x3FU);
}

static void test_normalize_repositions_without_resampling(void **state) {
    FontFitMask mask = make_mask(8U, 8U);
    FontFitCell cell;
    FontFitReport report;
    unsigned row;
    (void)state;
    /* 7x7 ink pinned to the top-left of an 8x8 strike. */
    fill_rect(&mask, 0U, 0U, 7U, 7U);
    assert_int_equal(font_fit_probe(&mask, &report), FONT_FIT_OK);
    assert_int_equal(report.ink_width, 7U);
    assert_int_equal(report.ink_height, 7U);
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_NORMALIZE, &cell, &report), FONT_FIT_OK);
    assert_true(report.repositioned);
    /* Horizontal centre of 7 in 8 is column 0; baseline alignment puts the ink bottom on row 7,
       so the ink occupies rows 1..7 and not 0..6. */
    assert_int_equal(cell.rows[0], 0x00U);
    for (row = 1U; row < 8U; row++) {
        assert_int_equal(cell.rows[row], 0x7FU);
    }
}

static void test_normalize_refuses_ink_larger_than_the_cell(void **state) {
    FontFitMask mask = make_mask(16U, 16U);
    FontFitCell cell;
    FontFitReport report;
    char text[192];
    (void)state;
    fill_rect(&mask, 0U, 0U, 10U, 10U); /* strike 16x16, ink 10x10: too big for the cell */
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_NORMALIZE, &cell, &report),
                     FONT_FIT_TOO_LARGE);
    /* The refusal has to name the remedies, because "too large" without "use scale" leaves the
       user guessing at a mode they have not been told about. */
    assert_true(font_fit_describe(&report, text, sizeof(text)));
    assert_non_null(strstr(text, "squash"));
    assert_non_null(strstr(text, "scale"));
    assert_non_null(strstr(text, "2 column"));
}


static void test_blank_glyph_is_a_glyph_not_a_failure(void **state) {
    FontFitMask mask = make_mask(8U, 8U);
    FontFitCell cell;
    FontFitReport report;
    char text[128];
    (void)state;
    /* Space has no ink. Rejecting it would reject every font that has a space. */
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_NONE, &cell, &report), FONT_FIT_OK);
    assert_int_equal(report.ink_width, 0U);
    assert_int_equal(cell.rows[0], 0x00U);
    assert_true(font_fit_describe(&report, text, sizeof(text)));
    assert_non_null(strstr(text, "no ink"));
}

static void test_source_that_fits_is_not_moved_by_strict_mode(void **state) {
    FontFitMask mask = make_mask(7U, 7U);
    FontFitCell cell;
    FontFitReport report;
    (void)state;
    fill_rect(&mask, 0U, 0U, 7U, 7U);
    /* A strike smaller than the cell keeps its own origin under NONE: centring it here would be a
       silent design change, and normalize exists for callers who want it. */
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_NONE, &cell, &report), FONT_FIT_OK);
    assert_int_equal(cell.rows[0], 0x7FU);
    assert_int_equal(cell.rows[7], 0x00U);
    assert_false(report.repositioned);
}

static void test_apply_does_not_modify_its_input(void **state) {
    FontFitMask mask = make_ring_16();
    FontFitMask before = mask;
    FontFitCell cell;
    (void)state;
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_SQUASH, &cell, NULL), FONT_FIT_OK);
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_SCALE, &cell, NULL), FONT_FIT_OK);
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_NORMALIZE, &cell, NULL),
                     FONT_FIT_TOO_LARGE);
    assert_memory_equal(&mask, &before, sizeof(mask));
}

static void test_determinism_and_bounds(void **state) {
    FontFitMask mask = make_ring_16();
    FontFitCell first;
    FontFitCell second;
    FontFitMask too_wide = make_mask(FONT_FIT_SOURCE_MAX + 1U, 8U);
    FontFitMask empty;
    (void)state;
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_SCALE, &first, NULL), FONT_FIT_OK);
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_SCALE, &second, NULL), FONT_FIT_OK);
    assert_memory_equal(&first, &second, sizeof(first));

    assert_int_equal(font_fit_probe(NULL, NULL), FONT_FIT_INVALID_ARGUMENT);
    assert_int_equal(font_fit_apply(NULL, FONT_FIT_MODE_NONE, &first, NULL),
                     FONT_FIT_INVALID_ARGUMENT);
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_NONE, NULL, NULL),
                     FONT_FIT_INVALID_ARGUMENT);
    assert_int_equal(font_fit_probe(&too_wide, NULL), FONT_FIT_INVALID_ARGUMENT);
    font_fit_mask_init(&empty);
    assert_int_equal(font_fit_probe(&empty, NULL), FONT_FIT_INVALID_ARGUMENT);
    assert_int_equal(font_fit_apply(&mask, (FontFitMode)99, &first, NULL),
                     FONT_FIT_INVALID_ARGUMENT);
    /* Writing outside a mask is refused rather than growing it. */
    assert_false(font_fit_mask_set(&empty, 0U, 0U));
    assert_false(font_fit_mask_get(NULL, 0U, 0U));
}

static void test_describe_is_bounded_and_named(void **state) {
    FontFitMask mask = make_mask(9U, 8U);
    FontFitReport report;
    char tiny[8];
    char text[192];
    (void)state;
    fill_rect(&mask, 0U, 0U, 9U, 8U);
    assert_int_equal(font_fit_probe(&mask, &report), FONT_FIT_TOO_LARGE);
    /* A truncated reason is worse than none: it looks complete. */
    assert_false(font_fit_describe(&report, tiny, sizeof(tiny)));
    assert_int_equal(tiny[0], '\0');
    assert_false(font_fit_describe(NULL, text, sizeof(text)));
    assert_false(font_fit_describe(&report, NULL, sizeof(text)));
    assert_true(font_fit_describe(&report, text, sizeof(text)));
    assert_non_null(strstr(text, "9x8"));
    assert_non_null(strstr(text, "1 column"));
    assert_string_equal(font_fit_mode_name(FONT_FIT_MODE_NONE), "none");
    assert_string_equal(font_fit_mode_name(FONT_FIT_MODE_SQUASH), "squash");
    assert_string_equal(font_fit_mode_name(FONT_FIT_MODE_SCALE), "scale");
    assert_string_equal(font_fit_mode_name(FONT_FIT_MODE_NORMALIZE), "normalize");
}

/* The cell's bit order is not a detail: it is the renderer's convention, and reversing it would
   mirror every converted glyph in the game while every test that only checked "some ink exists"
   still passed. renderer.c rasterises with `row_pixels[0] = (row & 1)` … `row_pixels[7] =
   (row & 128)`, so bit 0 is the leftmost pixel. This test pins that, and names where it comes
   from, so a future change to either side has to change both deliberately. */
static void test_leftmost_pixel_is_bit_zero_matching_the_renderer(void **state) {
    FontFitMask mask = make_mask(8U, 8U);
    FontFitCell cell;
    (void)state;
    assert_true(font_fit_mask_set(&mask, 0U, 0U)); /* top-left corner only */
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_NONE, &cell, NULL), FONT_FIT_OK);
    assert_int_equal(cell.rows[0], 0x01U);
    font_fit_mask_init(&mask);
    mask.width = 8U;
    mask.height = 8U;
    assert_true(font_fit_mask_set(&mask, 7U, 0U)); /* top-right corner only */
    assert_int_equal(font_fit_apply(&mask, FONT_FIT_MODE_NONE, &cell, NULL), FONT_FIT_OK);
    assert_int_equal(cell.rows[0], 0x80U);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_a_glyph_that_already_fits_is_untouched),
        cmocka_unit_test(test_strict_mode_refuses_one_column_too_wide_without_writing),
        cmocka_unit_test(test_squash_drops_the_unsampled_column_and_says_so),
        cmocka_unit_test(test_squash_keeps_an_axis_that_already_fits),
        cmocka_unit_test(test_squash_one_row_too_tall),
        cmocka_unit_test(test_scale_preserves_a_hairline_ring_at_half_size),
        cmocka_unit_test(test_scale_of_solid_16_is_solid),
        cmocka_unit_test(test_scale_reports_factor_one_when_nothing_to_scale),
        cmocka_unit_test(test_normalize_repositions_without_resampling),
        cmocka_unit_test(test_normalize_refuses_ink_larger_than_the_cell),
        cmocka_unit_test(test_blank_glyph_is_a_glyph_not_a_failure),
        cmocka_unit_test(test_source_that_fits_is_not_moved_by_strict_mode),
        cmocka_unit_test(test_apply_does_not_modify_its_input),
        cmocka_unit_test(test_determinism_and_bounds),
        cmocka_unit_test(test_describe_is_bounded_and_named),
        cmocka_unit_test(test_leftmost_pixel_is_bit_zero_matching_the_renderer)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

