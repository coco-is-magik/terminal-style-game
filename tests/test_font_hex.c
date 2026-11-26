/**
 * tests/test_font_hex.c — the hexdump reader
 *
 * The two glyphs below are the ones the unscii review used to settle bit order: `U+0045` is
 * `7E60607C60607E00` in Unifont's MSB-first convention, and its second row must land as `0x06`,
 * not `0x60`. That single byte is the difference between an `E` and a mirrored `E`.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>

#include "../src/font_hex.h"
#include "../src/font_fit.h"

#define MAX_ENTRIES 16U

static FontHexResult parse(const char *text, GlyphSetEntry *entries, size_t capacity,
                           size_t *out_count, FontHexError *out_error) {
    return font_hex_parse(text, strlen(text), entries, capacity, out_count, out_error);
}

static void test_documented_glyphs_parse_with_bit_reversal(void **state) {
    static const uint8_t expected_a[GLYPH_SET_CELL_SIZE] = {0x18, 0x3C, 0x66, 0x66,
                                                            0x7E, 0x66, 0x66, 0x00};
    /* Row 1 is 0x60 MSB-first and must arrive as 0x06 LSB-first: bit 0 is the leftmost pixel.
       Row 3 is 0x7C, which is *not* a palindrome either (reversed: 0x3E), so a reader that copied
       bytes through unchanged would fail on both rows rather than on one. */
    static const uint8_t expected_e[GLYPH_SET_CELL_SIZE] = {0x7E, 0x06, 0x06, 0x3E,
                                                            0x06, 0x06, 0x7E, 0x00};
    GlyphSetEntry entries[MAX_ENTRIES];
    FontHexError error;
    size_t count = 0U;

    (void)state;
    memset(&error, 0, sizeof(error));

    /* Deliberately not in codepoint order: the reader owns the ordering the set format requires. */
    assert_int_equal(parse("0045:7E60607C60607E00\n0041:183C66667E666600\n",
                           entries, MAX_ENTRIES, &count, &error), FONT_HEX_OK);
    assert_int_equal(count, 2U);

    assert_int_equal(entries[0].codepoint, 0x0041U);
    assert_memory_equal(entries[0].rows, expected_a, GLYPH_SET_CELL_SIZE);
    assert_int_equal(entries[0].mode, FONT_FIT_MODE_NONE);

    assert_int_equal(entries[1].codepoint, 0x0045U);
    assert_memory_equal(entries[1].rows, expected_e, GLYPH_SET_CELL_SIZE);
    assert_int_equal(entries[1].mode, FONT_FIT_MODE_NONE);
}

static void test_entries_come_back_sorted(void **state) {
    GlyphSetEntry entries[MAX_ENTRIES];
    size_t count = 0U;
    (void)state;

    assert_int_equal(parse("0045:7E60607C60607E00\n0030:0000000000000000\n"
                           "0041:183C66667E666600\n",
                           entries, MAX_ENTRIES, &count, NULL), FONT_HEX_OK);
    assert_int_equal(count, 3U);
    assert_int_equal(entries[0].codepoint, 0x0030U);
    assert_int_equal(entries[1].codepoint, 0x0041U);
    assert_int_equal(entries[2].codepoint, 0x0045U);
}

static void test_blank_and_comment_lines_are_skipped(void **state) {
    GlyphSetEntry entries[MAX_ENTRIES];
    FontHexError error;
    size_t count = 0U;
    (void)state;

    assert_int_equal(parse("\n# a comment\n   \n0041:183C66667E666600\n\n",
                           entries, MAX_ENTRIES, &count, &error), FONT_HEX_OK);
    assert_int_equal(count, 1U);
    assert_int_equal(entries[0].codepoint, 0x0041U);
}

static void test_duplicate_codepoint_is_refused(void **state) {
    GlyphSetEntry entries[MAX_ENTRIES];
    FontHexError error;
    size_t count = 99U;
    (void)state;

    assert_int_equal(parse("0041:183C66667E666600\n0041:0000000000000000\n",
                           entries, MAX_ENTRIES, &count, &error),
                     FONT_HEX_DUPLICATE_CODEPOINT);
    assert_int_equal(error.result, FONT_HEX_DUPLICATE_CODEPOINT);
    assert_int_equal(error.codepoint, 0x0041UL);
    assert_int_equal(count, 0U);
}

static void test_malformed_lines_name_the_line(void **state) {
    GlyphSetEntry entries[MAX_ENTRIES];
    FontHexError error;
    size_t count = 99U;
    (void)state;

    /* No colon on the second line. */
    assert_int_equal(parse("0041:183C66667E666600\n0041\n", entries, MAX_ENTRIES, &count, &error),
                     FONT_HEX_LINE_SYNTAX);
    assert_int_equal(error.line, 2U);

    /* Too few digits. */
    assert_int_equal(parse("0041:183C\n", entries, MAX_ENTRIES, &count, &error),
                     FONT_HEX_LINE_WIDTH);
    assert_int_equal(error.line, 1U);

    /* Not hexadecimal. */
    assert_int_equal(parse("0041:183C66667E6666ZZ\n", entries, MAX_ENTRIES, &count, &error),
                     FONT_HEX_LINE_WIDTH);
    assert_int_equal(error.line, 1U);

    /* A full cell followed by junk. */
    assert_int_equal(parse("0041:183C66667E666600 junk\n", entries, MAX_ENTRIES, &count, &error),
                     FONT_HEX_LINE_SYNTAX);
    assert_int_equal(error.line, 1U);
    assert_int_equal(count, 0U);
}

static void test_over_ceiling_codepoint_is_refused(void **state) {
    GlyphSetEntry entries[MAX_ENTRIES];
    FontHexError error;
    size_t count = 99U;
    (void)state;

    assert_int_equal(parse("0041:183C66667E666600\n1F600:0000000000000000\n",
                           entries, MAX_ENTRIES, &count, &error),
                     FONT_HEX_CODEPOINT_RANGE);
    assert_int_equal(error.line, 2U);
    assert_int_equal(error.codepoint, 0x1F600UL);
    assert_int_equal(count, 0U);
}

static void test_capacity_and_arguments(void **state) {
    GlyphSetEntry entries[MAX_ENTRIES];
    FontHexError error;
    size_t count = 99U;
    (void)state;

    /* Three glyphs into room for two. */
    assert_int_equal(parse("0041:183C66667E666600\n0042:183C66667E666600\n"
                           "0043:183C66667E666600\n",
                           entries, 2U, &count, &error), FONT_HEX_TOO_MANY);
    assert_int_equal(count, 0U);

    /* Nothing to read is its own answer, distinct from a caller error. */
    assert_int_equal(parse("", entries, MAX_ENTRIES, &count, &error), FONT_HEX_EMPTY);
    assert_int_equal(parse("\n\n# only a comment\n", entries, MAX_ENTRIES, &count, &error),
                     FONT_HEX_EMPTY);

    assert_int_equal(font_hex_parse(NULL, 0U, entries, MAX_ENTRIES, &count, &error),
                     FONT_HEX_INVALID_ARGUMENT);
    assert_int_equal(parse("0041:183C66667E666600\n", NULL, MAX_ENTRIES, &count, &error),
                     FONT_HEX_INVALID_ARGUMENT);
    assert_int_equal(parse("0041:183C66667E666600\n", entries, 0U, &count, &error),
                     FONT_HEX_INVALID_ARGUMENT);
    assert_int_equal(parse("0041:183C66667E666600\n", entries, MAX_ENTRIES, NULL, &error),
                     FONT_HEX_INVALID_ARGUMENT);
    assert_int_equal(count, 0U);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_documented_glyphs_parse_with_bit_reversal),
        cmocka_unit_test(test_entries_come_back_sorted),
        cmocka_unit_test(test_blank_and_comment_lines_are_skipped),
        cmocka_unit_test(test_duplicate_codepoint_is_refused),
        cmocka_unit_test(test_malformed_lines_name_the_line),
        cmocka_unit_test(test_over_ceiling_codepoint_is_refused),
        cmocka_unit_test(test_capacity_and_arguments),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
