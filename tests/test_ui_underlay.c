#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <cmocka.h>

#include "../src/grid.h"
#include "../src/ui_theme.h"
#include "../src/ui_underlay.h"

#define TEST_WIDTH 8
#define TEST_HEIGHT 4

static void fill_frame(Grid *grid, int salt) {
    int index;

    assert_non_null(grid);
    for (index = 0; index < grid->width * grid->height; index++) {
        Cell *cell = &grid->cells[index];
        cell->glyph = (uint8_t)('!' + ((index + salt) % 20));
        cell->fg.r = (uint8_t)((index * 37 + salt) % 256);
        cell->fg.g = (uint8_t)(255 - ((index * 11 + salt) % 256));
        cell->fg.b = (uint8_t)((index * 7 + salt) % 256);
        cell->fg.a = 0xffU;
        cell->bg.r = (uint8_t)((5 + index * 2 + salt) % 256);
        cell->bg.g = (uint8_t)((30 + index * 4 + salt) % 256);
        cell->bg.b = (uint8_t)((90 + index + salt) % 256);
        cell->bg.a = 0xffU;
    }
}

static int frame_luma(const Grid *grid) {
    int index;
    int total = 0;

    for (index = 0; index < grid->width * grid->height; index++) {
        const Cell *cell = &grid->cells[index];
        total += 299 * (int)cell->bg.r + 587 * (int)cell->bg.g +
                 114 * (int)cell->bg.b;
    }
    return total;
}

static void test_capture_requires_a_matching_surface(void **state) {
    Grid *grid = grid_create(TEST_WIDTH, TEST_HEIGHT);
    Grid *other = grid_create(TEST_WIDTH + 1, TEST_HEIGHT);
    UiUnderlay underlay;

    (void)state;
    assert_non_null(grid);
    assert_non_null(other);
    assert_true(ui_underlay_init(&underlay, TEST_WIDTH, TEST_HEIGHT));
    assert_int_equal(ui_underlay_capture(NULL, grid), UI_UNDERLAY_INVALID_ARGUMENT);
    assert_int_equal(ui_underlay_capture(&underlay, NULL), UI_UNDERLAY_INVALID_ARGUMENT);
    assert_int_equal(ui_underlay_capture(&underlay, other), UI_UNDERLAY_SIZE_MISMATCH);
    assert_false(ui_underlay_has_frame(&underlay));
    ui_underlay_destroy(&underlay);
    grid_destroy(other);
    grid_destroy(grid);
}

static void test_identity_restores_the_captured_frame(void **state) {
    Grid *grid = grid_create(TEST_WIDTH, TEST_HEIGHT);
    UiUnderlay underlay;
    Cell expected[TEST_WIDTH * TEST_HEIGHT];

    (void)state;
    assert_non_null(grid);
    assert_true(ui_underlay_init(&underlay, TEST_WIDTH, TEST_HEIGHT));
    fill_frame(grid, 0);
    memcpy(expected, grid->cells, sizeof(expected));
    assert_int_equal(ui_underlay_capture(&underlay, grid), UI_UNDERLAY_OK);
    assert_true(ui_underlay_has_frame(&underlay));
    /* A later frame never replaces the frozen one. */
    fill_frame(grid, 3);
    assert_int_equal(ui_underlay_apply(&underlay, grid, 0, 0), UI_UNDERLAY_OK);
    assert_memory_equal(expected, grid->cells, sizeof(expected));
    ui_underlay_destroy(&underlay);
    grid_destroy(grid);
}

static void test_empty_frame_is_not_a_background(void **state) {
    Grid *grid = grid_create(TEST_WIDTH, TEST_HEIGHT);
    UiUnderlay underlay;

    (void)state;
    assert_non_null(grid);
    assert_true(ui_underlay_init(&underlay, TEST_WIDTH, TEST_HEIGHT));
    assert_int_equal(ui_underlay_capture(&underlay, grid), UI_UNDERLAY_EMPTY_FRAME);
    assert_false(ui_underlay_has_frame(&underlay));
    /* A capture that exists survives an empty frame being offered. */
    fill_frame(grid, 0);
    assert_int_equal(ui_underlay_capture(&underlay, grid), UI_UNDERLAY_OK);
    assert_true(grid_clear_region_zero(grid, 0, 0, TEST_WIDTH, TEST_HEIGHT));
    assert_int_equal(ui_underlay_capture(&underlay, grid), UI_UNDERLAY_EMPTY_FRAME);
    assert_true(ui_underlay_has_frame(&underlay));
    ui_underlay_destroy(&underlay);
    grid_destroy(grid);
}

static void test_apply_without_a_captured_frame(void **state) {
    Grid *grid = grid_create(TEST_WIDTH, TEST_HEIGHT);
    UiUnderlay underlay;

    (void)state;
    assert_non_null(grid);
    assert_true(ui_underlay_init(&underlay, TEST_WIDTH, TEST_HEIGHT));
    assert_int_equal(ui_underlay_apply(&underlay, grid, 45, 60), UI_UNDERLAY_NO_FRAME);
    fill_frame(grid, 0);
    assert_int_equal(ui_underlay_capture(&underlay, grid), UI_UNDERLAY_OK);
    assert_int_equal(ui_underlay_apply(&underlay, NULL, 45, 60),
                     UI_UNDERLAY_INVALID_ARGUMENT);
    ui_underlay_forget(&underlay);
    assert_false(ui_underlay_has_frame(&underlay));
    assert_int_equal(ui_underlay_apply(&underlay, grid, 45, 60), UI_UNDERLAY_NO_FRAME);
    ui_underlay_destroy(&underlay);
    grid_destroy(grid);
}

static void test_response_dims_greys_and_preserves_alpha(void **state) {
    UiThemeColor saturated = {255U, 32U, 32U, 0x80U};
    UiThemeColor greyed;
    UiThemeColor dimmed;
    UiThemeColor dimmed_grey;

    (void)state;
    assert_true(ui_underlay_response(saturated, 0, 100, &greyed));
    /* Rec.601 luma of (255, 32, 32) is 99: the neutral axis is exact. */
    assert_int_equal(greyed.red, 99);
    assert_int_equal(greyed.green, 99);
    assert_int_equal(greyed.blue, 99);
    assert_int_equal(greyed.alpha, saturated.alpha);

    assert_true(ui_underlay_response(saturated, 50, 0, &dimmed));
    assert_int_equal(dimmed.red, 128);
    assert_int_equal(dimmed.green, 16);
    assert_int_equal(dimmed.blue, 16);
    assert_int_equal(dimmed.alpha, saturated.alpha);

    /* Full dim is black whatever the hue, and grey cannot resurrect it. */
    assert_true(ui_underlay_response(saturated, 100, 0, &dimmed_grey));
    assert_int_equal(dimmed_grey.red, 0);
    assert_int_equal(dimmed_grey.green, 0);
    assert_int_equal(dimmed_grey.blue, 0);
    assert_true(ui_underlay_response(saturated, 100, 100, &dimmed_grey));
    assert_int_equal(dimmed_grey.red, 0);
    assert_int_equal(dimmed_grey.green, 0);
    assert_int_equal(dimmed_grey.blue, 0);

    assert_false(ui_underlay_response(saturated, 0, 0, NULL));
}

static void test_strengths_clamp_and_dim_monotonically(void **state) {
    UiThemeColor source = {200U, 200U, 200U, 255U};
    UiThemeColor below;
    UiThemeColor quarter;
    UiThemeColor half;
    UiThemeColor full;

    (void)state;
    /* Out-of-range strengths saturate instead of wrapping or failing. */
    assert_true(ui_underlay_response(source, -50, 0, &below));
    assert_int_equal(below.red, source.red);
    assert_true(ui_underlay_response(source, 1000, 0, &full));
    assert_int_equal(full.red, 0);

    assert_true(ui_underlay_response(source, 25, 0, &quarter));
    assert_true(ui_underlay_response(source, 50, 0, &half));
    assert_true(quarter.red > half.red);
    assert_true(half.red > full.red);
}

static void test_theme_overlay_strengths_are_usable(void **state) {
    const UiThemeTokens *tokens = ui_theme_provisional_tokens();

    (void)state;
    assert_non_null(tokens);
    /* The application passes these straight through, so both must be strengths
       and at least one must actually recess the frame. */
    assert_true(tokens->overlay.dim_percent <= 100U);
    assert_true(tokens->overlay.grey_percent <= 100U);
    assert_true(tokens->overlay.dim_percent > 0U ||
                tokens->overlay.grey_percent > 0U);
}

static void test_apply_recesses_and_repeats_exactly(void **state) {
    Grid *grid = grid_create(TEST_WIDTH, TEST_HEIGHT);
    UiUnderlay underlay;
    const UiThemeTokens *tokens = ui_theme_provisional_tokens();
    Cell first[TEST_WIDTH * TEST_HEIGHT];
    int source_luma;

    (void)state;
    assert_non_null(grid);
    assert_non_null(tokens);
    assert_true(ui_underlay_init(&underlay, TEST_WIDTH, TEST_HEIGHT));
    fill_frame(grid, 0);
    source_luma = frame_luma(grid);
    assert_int_equal(ui_underlay_capture(&underlay, grid), UI_UNDERLAY_OK);
    assert_int_equal(ui_underlay_apply(&underlay, grid,
                                       (int)tokens->overlay.dim_percent,
                                       (int)tokens->overlay.grey_percent),
                     UI_UNDERLAY_OK);
    memcpy(first, grid->cells, sizeof(first));
    assert_true(frame_luma(grid) < source_luma);
    /* Nothing here reads a clock, so a second application is identical and needs
       no reduced-motion special case. */
    assert_int_equal(ui_underlay_apply(&underlay, grid,
                                       (int)tokens->overlay.dim_percent,
                                       (int)tokens->overlay.grey_percent),
                     UI_UNDERLAY_OK);
    assert_memory_equal(first, grid->cells, sizeof(first));
    ui_underlay_destroy(&underlay);
    grid_destroy(grid);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_capture_requires_a_matching_surface),
        cmocka_unit_test(test_identity_restores_the_captured_frame),
        cmocka_unit_test(test_empty_frame_is_not_a_background),
        cmocka_unit_test(test_apply_without_a_captured_frame),
        cmocka_unit_test(test_response_dims_greys_and_preserves_alpha),
        cmocka_unit_test(test_strengths_clamp_and_dim_monotonically),
        cmocka_unit_test(test_theme_overlay_strengths_are_usable),
        cmocka_unit_test(test_apply_recesses_and_repeats_exactly),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

