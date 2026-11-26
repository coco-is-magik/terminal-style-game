/**
 * tests/test_text_utf8.c — strict validation, canonical NFC, bounded clusters
 *
 * The cases are chosen for the failures a hand-rolled decoder gets wrong: overlong forms, surrogates,
 * truncated sequences, and clusters that are legitimately one grapheme but longer than a cell.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>

#include "../src/text_utf8.h"

/* One cluster, walked explicitly, so the test says what the API contract is rather than restating it. */
static TextUtf8Result clusters_of(const char *bytes, size_t size, uint32_t *out, size_t capacity,
                                  size_t *out_count) {
    size_t offset = 0U;
    size_t count = 0U;
    for (;;) {
        uint32_t codepoint = 0U;
        size_t length = 0U;
        TextUtf8Result result = text_utf8_next_cluster(bytes, size, &offset, &codepoint, &length);
        if (result == TEXT_UTF8_END) break;
        if (result != TEXT_UTF8_OK) return result;
        if (count >= capacity) return TEXT_UTF8_BUFFER_TOO_SMALL;
        out[count++] = codepoint;
    }
    *out_count = count;
    return TEXT_UTF8_OK;
}

static void test_valid_utf8_of_each_length(void **state) {
    static const char two[] = "\xC3\xA9";                 /* é, precomposed */
    static const char three[] = "\xE3\x81\x82";           /* あ */
    static const char four[] = "\xF0\x9F\x98\x80";        /* 😀 */
    (void)state;

    assert_int_equal(text_utf8_validate("", 0U), TEXT_UTF8_OK);
    assert_int_equal(text_utf8_validate("A", 1U), TEXT_UTF8_OK);
    assert_int_equal(text_utf8_validate(two, sizeof(two) - 1U), TEXT_UTF8_OK);
    assert_int_equal(text_utf8_validate(three, sizeof(three) - 1U), TEXT_UTF8_OK);
    assert_int_equal(text_utf8_validate(four, sizeof(four) - 1U), TEXT_UTF8_OK);
}

static void test_malformed_inputs_are_refused(void **state) {
    static const char lone_continuation[] = "\x80";
    static const char truncated_three[] = "\xE3\x81";
    static const char overlong_two[] = "\xC0\x80";        /* overlong NUL: the classic bypass */
    static const char overlong_three[] = "\xE0\x80\xAF";
    static const char surrogate[] = "\xED\xA0\x80";       /* U+D800, never a character */
    static const char above_max[] = "\xF4\x90\x80\x80";   /* U+110000 */
    static const char valid_then_junk[] = "\x41\xFF";
    (void)state;

    assert_int_equal(text_utf8_validate(lone_continuation, sizeof(lone_continuation) - 1U),
                     TEXT_UTF8_MALFORMED);
    assert_int_equal(text_utf8_validate(truncated_three, sizeof(truncated_three) - 1U),
                     TEXT_UTF8_MALFORMED);
    assert_int_equal(text_utf8_validate(overlong_two, sizeof(overlong_two) - 1U),
                     TEXT_UTF8_MALFORMED);
    assert_int_equal(text_utf8_validate(overlong_three, sizeof(overlong_three) - 1U),
                     TEXT_UTF8_MALFORMED);
    assert_int_equal(text_utf8_validate(surrogate, sizeof(surrogate) - 1U), TEXT_UTF8_MALFORMED);
    assert_int_equal(text_utf8_validate(above_max, sizeof(above_max) - 1U), TEXT_UTF8_MALFORMED);
    assert_int_equal(text_utf8_validate(valid_then_junk, sizeof(valid_then_junk) - 1U),
                     TEXT_UTF8_MALFORMED);
}

static void test_nfc_composes_and_is_stable(void **state) {
    static const char decomposed[] = "e\xCC\x81";         /* e + combining acute */
    static const char composed[] = "\xC3\xA9";            /* é */
    char out[32];
    char twice[32];
    size_t size = 0U;
    size_t again = 0U;
    (void)state;

    assert_int_equal(text_utf8_normalize_nfc(decomposed, sizeof(decomposed) - 1U, out, sizeof(out),
                                             &size), TEXT_UTF8_OK);
    assert_int_equal(size, sizeof(composed) - 1U);
    assert_memory_equal(out, composed, size);

    /* Idempotent: normalising the result changes nothing, which is what "canonical" means. */
    assert_int_equal(text_utf8_normalize_nfc(out, size, twice, sizeof(twice), &again), TEXT_UTF8_OK);
    assert_int_equal(again, size);
    assert_memory_equal(twice, out, size);

    /* Nothing to compose stays put. */
    assert_int_equal(text_utf8_normalize_nfc("A", 1U, out, sizeof(out), &size), TEXT_UTF8_OK);
    assert_int_equal(size, 1U);
    assert_int_equal(out[0], 'A');
}

static void test_normalize_reports_small_buffers(void **state) {
    static const char decomposed[] = "e\xCC\x81";
    char tiny[1];
    size_t size = 99U;
    (void)state;

    tiny[0] = 'x';
    assert_int_equal(text_utf8_normalize_nfc(decomposed, sizeof(decomposed) - 1U, tiny,
                                             sizeof(tiny), &size),
                     TEXT_UTF8_BUFFER_TOO_SMALL);
    assert_int_equal(size, 0U);
    assert_int_equal(tiny[0], 'x'); /* nothing partial written */

    /* Malformed input is refused here too, not silently passed through — and it is refused for the
       right reason, which is why this uses a buffer large enough to have succeeded otherwise. */
    {
        char roomy[16];
        size_t malformed_size = 99U;
        assert_int_equal(text_utf8_normalize_nfc("\x80", 1U, roomy, sizeof(roomy), &malformed_size),
                         TEXT_UTF8_MALFORMED);
        assert_int_equal(malformed_size, 0U);
    }
}

/* A cluster is the unit a cell stores, so these are the cases where "one character" and "one cluster"
   differ: combining marks, regional-indicator flags, and ZWJ sequences. */
static void test_clusters_split_correctly(void **state) {
    static const char combining[] = "e\xCC\x81";                     /* e + U+0301: one cluster */
    static const char flag[] = "\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5";    /* U+1F1EF U+1F1F5: one */
    static const char family[] = "\xF0\x9F\x91\xA8\xE2\x80\x8D"       /* U+1F468 ZWJ */
                                 "\xF0\x9F\x91\xA9\xE2\x80\x8D"       /* U+1F469 ZWJ */
                                 "\xF0\x9F\x91\xA7";                  /* U+1F467 */
    uint32_t codepoints[8];
    size_t count = 0U;
    (void)state;

    /* Two ASCII letters are two clusters; the list is what makes "boundary" testable. */
    assert_int_equal(clusters_of("ab", 2U, codepoints, 8U, &count), TEXT_UTF8_OK);
    assert_int_equal(count, 2U);
    assert_int_equal(codepoints[0], 0x0061U);
    assert_int_equal(codepoints[1], 0x0062U);

    assert_int_equal(clusters_of(combining, sizeof(combining) - 1U, codepoints, 8U, &count),
                     TEXT_UTF8_OK);
    assert_int_equal(count, 1U);
    assert_int_equal(codepoints[0], 0x0065U); /* the base answers for the cluster */

    assert_int_equal(clusters_of(flag, sizeof(flag) - 1U, codepoints, 8U, &count), TEXT_UTF8_OK);
    assert_int_equal(count, 1U);
    assert_int_equal(codepoints[0], 0x1F1EFU);

    assert_int_equal(clusters_of(family, sizeof(family) - 1U, codepoints, 8U, &count), TEXT_UTF8_OK);
    assert_int_equal(count, 1U);
    assert_int_equal(codepoints[0], 0x1F468U);
}

/* The bound that keeps a cluster a cell. A base with more combining marks than the limit allows is
   refused, because silently accepting it would mean a cell with no defined image. */
static void test_cluster_bound_is_enforced(void **state) {
    char text[1U + (TEXT_UTF8_MAX_CLUSTER_BYTES * 2U)];
    size_t offset = 0U;
    uint32_t codepoint = 0U;
    size_t length = 0U;
    size_t i = 0U;
    (void)state;

    text[i++] = 'a';
    while (i + 1U < sizeof(text)) {   /* U+0301 repeated: combining, so still one cluster */
        text[i++] = (char)0xCC;
        text[i++] = (char)0x81;
    }

    assert_int_equal(text_utf8_next_cluster(text, i, &offset, &codepoint, &length),
                     TEXT_UTF8_CLUSTER_TOO_LONG);
    assert_int_equal(offset, 0U);     /* refused, not partially consumed */
    assert_int_equal(length, 0U);
}

/* Malformed bytes inside a cluster walk are refused at the point they appear. */
static void test_cluster_walk_refuses_malformed(void **state) {
    static const char bad[] = "\xE3\x81";   /* truncated */
    size_t offset = 0U;
    uint32_t codepoint = 0U;
    size_t length = 0U;
    (void)state;

    assert_int_equal(text_utf8_next_cluster(bad, sizeof(bad) - 1U, &offset, &codepoint, &length),
                     TEXT_UTF8_MALFORMED);
    assert_int_equal(offset, 0U);
}

static void test_end_and_arguments(void **state) {
    size_t offset = 0U;
    uint32_t codepoint = 0U;
    size_t length = 0U;
    (void)state;

    /* An empty input is at the end immediately: a status, not an error. */
    assert_int_equal(text_utf8_next_cluster("", 0U, &offset, &codepoint, &length), TEXT_UTF8_END);
    assert_int_equal(offset, 0U);

    assert_int_equal(text_utf8_next_cluster(NULL, 1U, &offset, &codepoint, &length),
                     TEXT_UTF8_INVALID_ARGUMENT);
    assert_int_equal(text_utf8_next_cluster("A", 1U, NULL, &codepoint, &length),
                     TEXT_UTF8_INVALID_ARGUMENT);
    assert_int_equal(text_utf8_next_cluster("A", 1U, &offset, NULL, &length),
                     TEXT_UTF8_INVALID_ARGUMENT);
    assert_int_equal(text_utf8_next_cluster("A", 1U, &offset, &codepoint, NULL),
                     TEXT_UTF8_INVALID_ARGUMENT);

    offset = 5U; /* past the end */
    assert_int_equal(text_utf8_next_cluster("A", 1U, &offset, &codepoint, &length),
                     TEXT_UTF8_INVALID_ARGUMENT);

    assert_int_equal(text_utf8_validate(NULL, 1U), TEXT_UTF8_INVALID_ARGUMENT);
    assert_int_equal(text_utf8_validate(NULL, 0U), TEXT_UTF8_OK);
    assert_int_equal(text_utf8_normalize_nfc("A", 1U, NULL, 8U, &length),
                     TEXT_UTF8_INVALID_ARGUMENT);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_valid_utf8_of_each_length),
        cmocka_unit_test(test_malformed_inputs_are_refused),
        cmocka_unit_test(test_nfc_composes_and_is_stable),
        cmocka_unit_test(test_normalize_reports_small_buffers),
        cmocka_unit_test(test_clusters_split_correctly),
        cmocka_unit_test(test_cluster_bound_is_enforced),
        cmocka_unit_test(test_cluster_walk_refuses_malformed),
        cmocka_unit_test(test_end_and_arguments),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

