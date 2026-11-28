/*
 * test_ui_text.c — the authored-text field rules (V1-6 I3)
 *
 * The point of these tests is not that ASCII works — it did before. It is that the *refusals* carry a
 * position and that each rule refuses for its own reason, because a refusal nobody can locate is a
 * refusal nobody can fix.
 */

#include <cmocka.h>

#include "../src/ui_text.h"

#include <stddef.h>
#include <string.h>

/* U+3042 HIRAGANA LETTER A, the phase's running example: 3 bytes, one cell. */
#define HIRAGANA_A "\xE3\x81\x82"
/* U+0301 COMBINING ACUTE ACCENT: 2 bytes that belong to the character before them. */
#define COMBINING_ACUTE "\xCC\x81"

static void test_plain_ascii_and_empty_are_accepted(void **state) {
    UiTextDiagnostic diagnostic;
    (void)state;

    assert_int_equal(ui_text_check("START", 5U, &diagnostic), UI_TEXT_OK);
    assert_int_equal(diagnostic.offset, 5U);
    /* '=' is fine: the format splits on the first one, so a value may contain more. */
    assert_int_equal(ui_text_check_cstr("a=b", NULL), UI_TEXT_OK);
    /* Space and the printable ends. */
    assert_int_equal(ui_text_check(" ~", 2U, NULL), UI_TEXT_OK);
    assert_int_equal(ui_text_check("", 0U, &diagnostic), UI_TEXT_OK);
    assert_int_equal(diagnostic.offset, 0U);
}

static void test_multibyte_text_is_accepted(void **state) {
    (void)state;

    assert_int_equal(ui_text_check_cstr(HIRAGANA_A, NULL), UI_TEXT_OK);
    /* Mixed scripts in one label: the whole reason authored content moved to UTF-8. */
    assert_int_equal(ui_text_check_cstr("HP " HIRAGANA_A "\xE6\xAD\xA6", NULL), UI_TEXT_OK);
    /* A combining sequence is one cluster whose second codepoint is a mark, not noise. */
    assert_int_equal(ui_text_check_cstr("e" COMBINING_ACUTE, NULL), UI_TEXT_OK);
    /* ZWJ is cluster machinery, not a format character: refusing it would make emoji unrepresentable. */
    assert_int_equal(ui_text_check_cstr("\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9", NULL),
                     UI_TEXT_OK);
}

static void test_malformed_names_the_byte_offset(void **state) {
    UiTextDiagnostic diagnostic;
    (void)state;

    /* A lone continuation byte. */
    assert_int_equal(ui_text_check("AB\x80", 3U, &diagnostic), UI_TEXT_MALFORMED);
    assert_int_equal(diagnostic.offset, 2U);
    assert_int_equal(diagnostic.codepoint, 0U);

    /* A truncated sequence at the very end: the offset is where the sequence started. */
    assert_int_equal(ui_text_check("AB\xE3\x81", 4U, &diagnostic), UI_TEXT_MALFORMED);
    assert_int_equal(diagnostic.offset, 2U);

    /* An overlong form of '/', which a permissive decoder would happily accept. */
    assert_int_equal(ui_text_check("\xC0\xAF", 2U, &diagnostic), UI_TEXT_MALFORMED);
    assert_int_equal(diagnostic.offset, 0U);

    /* A UTF-16 surrogate, and a value above U+10FFFF. Both are refused, not repaired. */
    assert_int_equal(ui_text_check("\xED\xA0\x80", 3U, &diagnostic), UI_TEXT_MALFORMED);
    assert_int_equal(diagnostic.offset, 0U);
    assert_int_equal(ui_text_check("\xF4\x90\x80\x80", 4U, &diagnostic), UI_TEXT_MALFORMED);
    assert_int_equal(diagnostic.offset, 0U);
}


static void test_control_characters_are_refused_by_position(void **state) {
    UiTextDiagnostic diagnostic;
    (void)state;

    assert_int_equal(ui_text_check("a\nb", 3U, &diagnostic), UI_TEXT_CONTROL);
    assert_int_equal(diagnostic.offset, 1U);
    assert_int_equal(diagnostic.codepoint, 0x000AU);

    assert_int_equal(ui_text_check("a\tb", 3U, &diagnostic), UI_TEXT_CONTROL);
    assert_int_equal(diagnostic.codepoint, 0x0009U);

    /* A NUL inside the stated size is text the field cannot hold either. */
    assert_int_equal(ui_text_check("a\0b", 3U, &diagnostic), UI_TEXT_CONTROL);
    assert_int_equal(diagnostic.offset, 1U);

    /* DEL and C1 (U+0085 NEL) are controls too — the old ASCII rule caught DEL by accident of range and
       missed C1 entirely. */
    assert_int_equal(ui_text_check("a\x7F", 2U, &diagnostic), UI_TEXT_CONTROL);
    assert_int_equal(diagnostic.codepoint, 0x007FU);
    assert_int_equal(ui_text_check("a\xC2\x85", 3U, &diagnostic), UI_TEXT_CONTROL);
    assert_int_equal(diagnostic.offset, 1U);
    assert_int_equal(diagnostic.codepoint, 0x0085U);
}

static void test_bidi_formatting_is_a_control_not_text(void **state) {
    UiTextDiagnostic diagnostic;
    (void)state;

    /* LRM, RLM, and an override. They are category Cf: presentable-looking bytes that reorder what a
       reader sees, so they are refused rather than interpreted. */
    assert_int_equal(ui_text_check("\xE2\x80\x8E", 3U, &diagnostic), UI_TEXT_CONTROL);
    assert_int_equal(diagnostic.codepoint, 0x200EU);
    assert_int_equal(ui_text_check("\xE2\x80\x8F", 3U, &diagnostic), UI_TEXT_CONTROL);
    assert_int_equal(diagnostic.codepoint, 0x200FU);
    assert_int_equal(ui_text_check("a\xE2\x80\xAE" "b", 5U, &diagnostic), UI_TEXT_CONTROL);
    assert_int_equal(diagnostic.offset, 1U);
    assert_int_equal(diagnostic.codepoint, 0x202EU);
}

static void test_right_to_left_is_refused_with_its_own_reason(void **state) {
    UiTextDiagnostic diagnostic;
    (void)state;

    /* U+05D0 HEBREW LETTER ALEF and U+0627 ARABIC LETTER ALEF: valid UTF-8, visible characters, and
       text this phase will not draw backwards. Refusing them is a recorded deviation (D-C). */
    assert_int_equal(ui_text_check_cstr("\xD7\x90", &diagnostic), UI_TEXT_RIGHT_TO_LEFT);
    assert_int_equal(diagnostic.codepoint, 0x05D0U);
    assert_int_equal(ui_text_check_cstr("A\xD8\xA7", &diagnostic), UI_TEXT_RIGHT_TO_LEFT);
    assert_int_equal(diagnostic.offset, 1U);
    assert_int_equal(diagnostic.codepoint, 0x0627U);
}

static void test_backslash_is_refused_because_the_format_cannot_carry_it(void **state) {
    UiTextDiagnostic diagnostic;
    (void)state;

    assert_int_equal(ui_text_check("a\\b", 3U, &diagnostic), UI_TEXT_BACKSLASH);
    assert_int_equal(diagnostic.offset, 1U);
    assert_int_equal(diagnostic.codepoint, 0x005CU);
}


static void test_cluster_longer_than_a_cell_is_refused(void **state) {
    UiTextDiagnostic diagnostic;
    (void)state;

    /* Eleven emoji joined by ZWJ: one grapheme cluster of 11*4 + 10*3 = 74 bytes, past the 64-byte cell
       bound. One cluster is one cell (D-B), so this text cannot be stored for drawing. */
    assert_int_equal(ui_text_check_cstr(
        "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7\xE2\x80\x8D"
        "\xF0\x9F\x91\xA6\xE2\x80\x8D\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D"
        "\xF0\x9F\x91\xA7\xE2\x80\x8D\xF0\x9F\x91\xA6\xE2\x80\x8D\xF0\x9F\x91\xA8\xE2\x80\x8D"
        "\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7\xE2\x80\x8D\xF0\x9F\x91\xA6", &diagnostic),
        UI_TEXT_CLUSTER_TOO_LONG);
    assert_int_equal(diagnostic.offset, 0U);

    /* The same rule does not fire for a short cluster preceded by ordinary text. */
    assert_int_equal(ui_text_check_cstr("ab" "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9",
                                        &diagnostic),
                     UI_TEXT_OK);
}

static void test_unusable_arguments_are_refused(void **state) {
    UiTextDiagnostic diagnostic;
    (void)state;

    assert_int_equal(ui_text_check_cstr(NULL, &diagnostic), UI_TEXT_INVALID_ARGUMENT);
    assert_int_equal(ui_text_check(NULL, 4U, &diagnostic), UI_TEXT_INVALID_ARGUMENT);
    /* A NULL output pointer is allowed everywhere: the rules do not depend on being watched. */
    assert_int_equal(ui_text_check("ok", 2U, NULL), UI_TEXT_OK);
}

static void test_describe_names_the_rule_and_the_position(void **state) {
    UiTextDiagnostic diagnostic;
    char text[96];
    (void)state;

    assert_int_equal(ui_text_check("ab\t", 3U, &diagnostic), UI_TEXT_CONTROL);
    ui_text_describe(&diagnostic, text, sizeof(text));
    assert_string_equal(text, "CONTROL at byte 2 (U+0009)");

    assert_int_equal(ui_text_check("ab\x80", 3U, &diagnostic), UI_TEXT_MALFORMED);
    ui_text_describe(&diagnostic, text, sizeof(text));
    assert_string_equal(text, "MALFORMED at byte 2");

    /* A short buffer must still be terminated rather than overflowed. */
    ui_text_describe(&diagnostic, text, 8U);
    assert_int_equal(strlen(text), 7U);
    assert_string_equal(text, "MALFORM");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_plain_ascii_and_empty_are_accepted),
        cmocka_unit_test(test_multibyte_text_is_accepted),
        cmocka_unit_test(test_malformed_names_the_byte_offset),
        cmocka_unit_test(test_control_characters_are_refused_by_position),
        cmocka_unit_test(test_bidi_formatting_is_a_control_not_text),
        cmocka_unit_test(test_right_to_left_is_refused_with_its_own_reason),
        cmocka_unit_test(test_backslash_is_refused_because_the_format_cannot_carry_it),
        cmocka_unit_test(test_cluster_longer_than_a_cell_is_refused),
        cmocka_unit_test(test_unusable_arguments_are_refused),
        cmocka_unit_test(test_describe_names_the_rule_and_the_position)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
