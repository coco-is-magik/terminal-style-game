/**
 * tests/test_text_edit.c — cluster-aware cursor, deletion and capacity (V1-6 I1)
 *
 * The cases that matter are the ones where a byte offset and a character disagree: combining marks,
 * a regional-indicator flag, and a ZWJ family. A cursor that lands inside any of them is a cursor the
 * user cannot see, and a Backspace that deletes half of one is a bug they can.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>

#include "../src/text_edit.h"

/* One combining sequence, one flag, one ZWJ family — reused across the boundary tests. */
#define E_ACUTE "e\xCC\x81"                                        /* 3 bytes, one cluster */
#define FLAG "\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5"                    /* 8 bytes, one cluster */
#define FAMILY "\xF0\x9F\x91\xA8\xE2\x80\x8D"                      \
               "\xF0\x9F\x91\xA9\xE2\x80\x8D"                      \
               "\xF0\x9F\x91\xA7"                                  /* 18 bytes, one cluster */

static void test_boundaries_of_ascii(void **state) {
    size_t out = 99U;
    (void)state;
    assert_true(text_edit_is_boundary("abc", 3U, 0U));
    assert_true(text_edit_is_boundary("abc", 3U, 2U));
    assert_true(text_edit_is_boundary("abc", 3U, 3U));
    assert_false(text_edit_is_boundary("abc", 3U, 4U));

    assert_int_equal(text_edit_next_boundary("abc", 3U, 1U, &out), TEXT_EDIT_OK);
    assert_int_equal(out, 2U);
    assert_int_equal(text_edit_prev_boundary("abc", 3U, 2U, &out), TEXT_EDIT_OK);
    assert_int_equal(out, 1U);
}

/* The whole point: three bytes, one step. */
static void test_combining_sequence_moves_as_one(void **state) {
    size_t out = 99U;
    (void)state;
    assert_true(text_edit_is_boundary(E_ACUTE, 3U, 0U));
    assert_false(text_edit_is_boundary(E_ACUTE, 3U, 1U)); /* inside the cluster */
    assert_false(text_edit_is_boundary(E_ACUTE, 3U, 2U));
    assert_true(text_edit_is_boundary(E_ACUTE, 3U, 3U));

    assert_int_equal(text_edit_next_boundary(E_ACUTE, 3U, 0U, &out), TEXT_EDIT_OK);
    assert_int_equal(out, 3U); /* one step, not three */
    assert_int_equal(text_edit_prev_boundary(E_ACUTE, 3U, 3U, &out), TEXT_EDIT_OK);
    assert_int_equal(out, 0U);
}

/* Forward from inside a cluster is refused rather than guessed at. */
static void test_forward_from_inside_a_cluster_is_refused(void **state) {
    size_t out = 99U;
    (void)state;
    assert_int_equal(text_edit_next_boundary(E_ACUTE, 3U, 1U, &out), TEXT_EDIT_INVALID_ARGUMENT);
    /* Backwards snaps to the boundary, because that is what "move left" should do with a stale cursor. */
    assert_int_equal(text_edit_prev_boundary(E_ACUTE, 3U, 2U, &out), TEXT_EDIT_OK);
    assert_int_equal(out, 0U);
}

static void test_flags_and_zwj_families_are_single_steps(void **state) {
    size_t out = 99U;
    (void)state;
    assert_true(text_edit_is_boundary(FLAG, sizeof(FLAG) - 1U, 0U));
    assert_false(text_edit_is_boundary(FLAG, sizeof(FLAG) - 1U, 4U));
    assert_true(text_edit_is_boundary(FLAG, sizeof(FLAG) - 1U, sizeof(FLAG) - 1U));
    assert_int_equal(text_edit_next_boundary(FLAG, sizeof(FLAG) - 1U, 0U, &out), TEXT_EDIT_OK);
    assert_int_equal(out, sizeof(FLAG) - 1U);

    assert_int_equal(text_edit_next_boundary(FAMILY, sizeof(FAMILY) - 1U, 0U, &out), TEXT_EDIT_OK);
    assert_int_equal(out, sizeof(FAMILY) - 1U);
}

/* Backspace deletes a character, not a byte — and the cursor lands where the cluster began. */
static void test_backspace_deletes_one_cluster(void **state) {
    char buffer[32];
    size_t size;
    size_t cursor;
    (void)state;

    memcpy(buffer, "ab" E_ACUTE, 6U);
    size = 5U;
    cursor = 5U;
    assert_int_equal(text_edit_backspace(buffer, sizeof(buffer), &size, &cursor), TEXT_EDIT_OK);
    assert_int_equal(size, 2U);    /* the combining sequence went, whole */
    assert_int_equal(cursor, 2U);
    assert_memory_equal(buffer, "ab", 2U);

    /* Now an ASCII character, to prove the same call is not specialised to the hard case. */
    assert_int_equal(text_edit_backspace(buffer, sizeof(buffer), &size, &cursor), TEXT_EDIT_OK);
    assert_int_equal(size, 1U);
    assert_int_equal(cursor, 1U);
}

static void test_backspace_at_zero_is_a_no_op(void **state) {
    char buffer[8];
    size_t size = 3U;
    size_t cursor = 0U;
    (void)state;
    memcpy(buffer, "abc", 3U);
    assert_int_equal(text_edit_backspace(buffer, sizeof(buffer), &size, &cursor), TEXT_EDIT_OK);
    assert_int_equal(size, 3U);
    assert_int_equal(cursor, 0U);
}

static void test_backspace_refuses_a_cursor_inside_a_cluster(void **state) {
    char buffer[8];
    size_t size = 3U;
    size_t cursor = 1U;   /* inside E_ACUTE */
    (void)state;
    memcpy(buffer, E_ACUTE, 3U);
    assert_int_equal(text_edit_backspace(buffer, sizeof(buffer), &size, &cursor),
                     TEXT_EDIT_MALFORMED);
    assert_int_equal(size, 3U);
    assert_int_equal(cursor, 1U);
}

/* Capacity: truncation stops at a boundary, never inside one, and says which happened. */
static void test_fit_truncates_only_at_boundaries(void **state) {
    char out[8];
    size_t size = 99U;
    (void)state;

    /* Exact fit. */
    assert_int_equal(text_edit_fit("abc", 3U, out, sizeof(out), &size), TEXT_EDIT_OK);
    assert_int_equal(size, 3U);
    assert_memory_equal(out, "abc", 3U);

    /* Room for 'a' plus two of the three bytes of the next character: 'a' is what fits. */
    assert_int_equal(text_edit_fit("a" E_ACUTE, 4U, out, 3U, &size), TEXT_EDIT_TRUNCATED);
    assert_int_equal(size, 1U);
    assert_int_equal(out[0], 'a');

    /* Room for exactly the first cluster and nothing more: the whole cluster, not 2 of 3 bytes. */
    assert_int_equal(text_edit_fit(E_ACUTE "x", 4U, out, 3U, &size), TEXT_EDIT_TRUNCATED);
    assert_int_equal(size, 3U);
    assert_memory_equal(out, E_ACUTE, 3U);
}

static void test_fit_reports_nothing_fits(void **state) {
    char out[8];
    size_t size = 99U;
    (void)state;
    memset(out, 'Z', sizeof(out));
    /* Two bytes of room, a three-byte first character: nothing can be written whole. */
    assert_int_equal(text_edit_fit(E_ACUTE, 3U, out, 2U, &size), TEXT_EDIT_NOTHING_FITS);
    assert_int_equal(size, 0U);
    assert_int_equal(out[0], 'Z'); /* untouched: a refusal writes nothing */
}

/* Validation happens before any copy, so a malformed tail cannot leave half a field behind. */
static void test_fit_refuses_malformed_without_writing(void **state) {
    char out[8];
    size_t size = 99U;
    (void)state;
    memset(out, 'Z', sizeof(out));
    assert_int_equal(text_edit_fit("ab\x80", 3U, out, sizeof(out), &size), TEXT_EDIT_MALFORMED);
    assert_int_equal(size, 0U);
    assert_int_equal(out[0], 'Z');
}

static void test_fit_empty_and_arguments(void **state) {
    char out[4];
    size_t size = 99U;
    (void)state;
    assert_int_equal(text_edit_fit("", 0U, out, sizeof(out), &size), TEXT_EDIT_OK);
    assert_int_equal(size, 0U);
    assert_int_equal(text_edit_fit("a", 1U, NULL, 4U, &size), TEXT_EDIT_INVALID_ARGUMENT);
    assert_int_equal(text_edit_fit("a", 1U, out, sizeof(out), NULL), TEXT_EDIT_INVALID_ARGUMENT);

    assert_int_equal(text_edit_prev_boundary(NULL, 1U, 0U, &size), TEXT_EDIT_INVALID_ARGUMENT);
    assert_int_equal(text_edit_next_boundary("a", 1U, 0U, NULL), TEXT_EDIT_INVALID_ARGUMENT);
    assert_int_equal(text_edit_backspace(NULL, 1U, &size, &size), TEXT_EDIT_INVALID_ARGUMENT);
}

/* The range primitive typed text needs: insert at a caret, delete a selection, replace one. */
static void test_replace_range_inserts_deletes_and_replaces(void **state) {
    char buffer[16];
    size_t size;
    size_t cursor;
    (void)state;

    /* Insert at a caret. */
    memcpy(buffer, "abcd", sizeof("abcd"));
    size = 4U;
    assert_int_equal(text_edit_replace_range(buffer, sizeof(buffer), &size, 2U, 2U, "XY", 2U,
                                             &cursor),
                     TEXT_EDIT_OK);
    assert_string_equal(buffer, "abXYcd");
    assert_int_equal(size, 6U);
    assert_int_equal(cursor, 4U);

    /* Delete a range (the Y and c). */
    assert_int_equal(text_edit_replace_range(buffer, sizeof(buffer), &size, 3U, 5U, NULL, 0U,
                                             &cursor),
                     TEXT_EDIT_OK);
    assert_string_equal(buffer, "abXd");
    assert_int_equal(size, 4U);
    assert_int_equal(cursor, 3U);

    /* Replace a range with a longer one, which shifts the tail right. */
    memcpy(buffer, "a\xE3\x81\x82" "b", 5U + 1U);
    size = 5U;
    assert_int_equal(text_edit_replace_range(buffer, sizeof(buffer), &size, 1U, 4U, "zzz", 3U,
                                             &cursor),
                     TEXT_EDIT_OK);
    assert_string_equal(buffer, "azzzb");
    assert_int_equal(cursor, 4U);
}

static void test_replace_range_refuses_what_it_cannot_do(void **state) {
    char buffer[8];
    size_t size;
    size_t before;
    (void)state;

    /* A range that splits the three-byte character: 2 is not a boundary. */
    memcpy(buffer, "a\xE3\x81\x82", 4U + 1U);
    size = 4U;
    before = size;
    assert_int_equal(text_edit_replace_range(buffer, sizeof(buffer), &size, 2U, 4U, "x", 1U, NULL),
                     TEXT_EDIT_MALFORMED);
    assert_int_equal(size, before);
    assert_string_equal(buffer, "a\xE3\x81\x82");

    /* Malformed replacement text is refused before anything moves. */
    assert_int_equal(text_edit_replace_range(buffer, sizeof(buffer), &size, 0U, 0U, "\x80", 1U, NULL),
                     TEXT_EDIT_MALFORMED);
    assert_string_equal(buffer, "a\xE3\x81\x82");

    /* No room: capacity counts the terminator, so seven bytes need eight. */
    memcpy(buffer, "abcdef", sizeof("abcdef"));
    size = 6U;
    assert_int_equal(text_edit_replace_range(buffer, 7U, &size, 0U, 0U, "g", 1U, NULL),
                     TEXT_EDIT_NO_ROOM);
    assert_string_equal(buffer, "abcdef");
    assert_int_equal(size, 6U);
    /* The same insert fits when the buffer has room for the terminator. */
    assert_int_equal(text_edit_replace_range(buffer, 8U, &size, 0U, 0U, "g", 1U, NULL), TEXT_EDIT_OK);
    assert_string_equal(buffer, "gabcdef");
}

static void test_selection_orders_its_two_ends(void **state) {
    size_t begin;
    size_t end;
    (void)state;

    text_edit_selection(2U, 7U, &begin, &end);
    assert_int_equal(begin, 2U);
    assert_int_equal(end, 7U);
    text_edit_selection(7U, 2U, &begin, &end);
    assert_int_equal(begin, 2U);
    assert_int_equal(end, 7U);
    text_edit_selection(3U, 3U, &begin, &end);
    assert_int_equal(begin, 3U);
    assert_int_equal(end, 3U);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_boundaries_of_ascii),
        cmocka_unit_test(test_combining_sequence_moves_as_one),
        cmocka_unit_test(test_forward_from_inside_a_cluster_is_refused),
        cmocka_unit_test(test_flags_and_zwj_families_are_single_steps),
        cmocka_unit_test(test_backspace_deletes_one_cluster),
        cmocka_unit_test(test_backspace_at_zero_is_a_no_op),
        cmocka_unit_test(test_backspace_refuses_a_cursor_inside_a_cluster),
        cmocka_unit_test(test_fit_truncates_only_at_boundaries),
        cmocka_unit_test(test_fit_reports_nothing_fits),
        cmocka_unit_test(test_fit_refuses_malformed_without_writing),
        cmocka_unit_test(test_fit_empty_and_arguments),
        cmocka_unit_test(test_replace_range_inserts_deletes_and_replaces),
        cmocka_unit_test(test_replace_range_refuses_what_it_cannot_do),
        cmocka_unit_test(test_selection_orders_its_two_ends),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

