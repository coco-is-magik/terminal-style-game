/**
 * tests/test_grid_print.c — V1-6 I0: one grapheme cluster, one cell
 *
 * The regression this guards is concrete: a UTF-8 `あ` used to be drawn as three unrelated Latin-1
 * glyphs because the writer read one byte per cell. The ASCII cases are here to prove the change is
 * invisible to the text that dominates the interface.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "../src/grid.h"
#include "../src/text_utf8.h"   /* TEXT_UTF8_MAX_CLUSTER_BYTES, used by the bound case */

static const SDL_Color k_fg = {255, 255, 255, 255};
static const SDL_Color k_bg = {0, 0, 0, 255};

static uint16_t cell_at(Grid *grid, int x, int y) {
    Cell cell;
    assert_true(grid_get(grid, x, y, &cell));
    return cell.glyph;
}

static void test_ascii_is_unchanged(void **state) {
    Grid *grid = grid_create(8, 4);
    (void)state;
    assert_non_null(grid);
    assert_true(grid_print(grid, 1, 1, "AB", k_fg, k_bg));
    assert_int_equal(cell_at(grid, 1, 1), 0x0041U);
    assert_int_equal(cell_at(grid, 2, 1), 0x0042U);
    grid_destroy(grid);
}

static void test_newline_still_breaks_the_line(void **state) {
    Grid *grid = grid_create(8, 4);
    (void)state;
    assert_non_null(grid);
    assert_true(grid_print(grid, 0, 0, "A\nB", k_fg, k_bg));
    assert_int_equal(cell_at(grid, 0, 0), 0x0041U);
    assert_int_equal(cell_at(grid, 0, 1), 0x0042U);
    grid_destroy(grid);
}

/* The bug this increment exists for: three bytes must be one cell, not three. */
static void test_multibyte_is_one_cell(void **state) {
    Grid *grid = grid_create(8, 4);
    (void)state;
    assert_non_null(grid);
    assert_true(grid_print(grid, 0, 0, "\xE3\x81\x82", k_fg, k_bg));   /* あ */
    assert_int_equal(cell_at(grid, 0, 0), 0x3042U);
    assert_int_equal(cell_at(grid, 1, 0), 0x0000U);                     /* nothing spilled into column 1 */
    grid_destroy(grid);
}

static void test_two_byte_sequence_is_one_cell(void **state) {
    Grid *grid = grid_create(8, 4);
    (void)state;
    assert_non_null(grid);
    assert_true(grid_print(grid, 0, 0, "\xC3\xA9", k_fg, k_bg));   /* é */
    assert_int_equal(cell_at(grid, 0, 0), 0x00E9U);
    assert_int_equal(cell_at(grid, 1, 0), 0x0000U);
    grid_destroy(grid);
}

/* A combining mark belongs to the cluster before it, so it costs no column of its own. */
static void test_combining_mark_shares_the_base_cell(void **state) {
    Grid *grid = grid_create(8, 4);
    (void)state;
    assert_non_null(grid);
    assert_true(grid_print(grid, 0, 0, "e\xCC\x81" "X", k_fg, k_bg));   /* e + U+0301, then X */
    assert_int_equal(cell_at(grid, 0, 0), 0x0065U);   /* the cluster's first codepoint */
    assert_int_equal(cell_at(grid, 1, 0), 0x0058U);   /* X is the very next column */
    grid_destroy(grid);
}

/* U+1F600 is above what a 16-bit cell can address, so it is refused rather than truncated to a
   different character. Nothing is written, and the caller learns it did not all fit. */
static void test_astral_codepoint_is_refused_not_truncated(void **state) {
    Grid *grid = grid_create(8, 4);
    (void)state;
    assert_non_null(grid);
    assert_false(grid_print(grid, 0, 0, "\xF0\x9F\x98\x80", k_fg, k_bg));
    assert_int_equal(cell_at(grid, 0, 0), 0x0000U);
    grid_destroy(grid);
}

static void test_malformed_text_stops_the_walk(void **state) {
    Grid *grid = grid_create(8, 4);
    (void)state;
    assert_non_null(grid);
    /* A cluster is atomic: the malformed byte appears while the cluster starting at 'A' is still being
       read (a cluster ends at the *next* codepoint's boundary, and that codepoint is what fails), so
       neither the cluster nor anything after it is written. The trade-off is deliberate — committing
       half a cluster is worse than committing none of it — and it is why callers that care validate
       first rather than inspecting the grid afterwards. */
    assert_false(grid_print(grid, 0, 0, "A\x80" "B", k_fg, k_bg));
    assert_int_equal(cell_at(grid, 0, 0), 0x0000U);
    grid_destroy(grid);
}

static void test_over_long_cluster_stops_the_walk(void **state) {
    /* Two bytes per combining mark, plus the base, plus room for the terminator that strlen needs. */
    char text[2U + (TEXT_UTF8_MAX_CLUSTER_BYTES * 2U)];
    Grid *grid = grid_create(8, 4);
    size_t i = 0U;
    (void)state;
    assert_non_null(grid);
    text[i++] = 'a';
    /* The loop writes two bytes per iteration, so it must stop two clear of the end to leave the
       terminator inside the buffer — the arithmetic that GCC's stringop analysis caught here. */
    while (i + 2U < sizeof(text)) {
        text[i++] = (char)0xCC;
        text[i++] = (char)0x81;
    }
    text[i] = '\0';
    assert_false(grid_print(grid, 0, 0, text, k_fg, k_bg));
    grid_destroy(grid);
}

static void test_null_arguments_are_refused(void **state) {
    Grid *grid = grid_create(4, 2);
    (void)state;
    assert_non_null(grid);
    assert_false(grid_print(NULL, 0, 0, "A", k_fg, k_bg));
    assert_false(grid_print(grid, 0, 0, NULL, k_fg, k_bg));
    grid_destroy(grid);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ascii_is_unchanged),
        cmocka_unit_test(test_newline_still_breaks_the_line),
        cmocka_unit_test(test_multibyte_is_one_cell),
        cmocka_unit_test(test_two_byte_sequence_is_one_cell),
        cmocka_unit_test(test_combining_mark_shares_the_base_cell),
        cmocka_unit_test(test_astral_codepoint_is_refused_not_truncated),
        cmocka_unit_test(test_malformed_text_stops_the_walk),
        cmocka_unit_test(test_over_long_cluster_stops_the_walk),
        cmocka_unit_test(test_null_arguments_are_refused),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
