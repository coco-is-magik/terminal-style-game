#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include "../src/rgba_parse.h"

static void test_rgba_parse_accepts_bounded_channels(void **state) {
    uint8_t channels[4] = {0U, 0U, 0U, 0U};
    (void)state;

    assert_true(rgba_parse("0, 127,255, 42", channels));
    assert_int_equal(channels[0], 0U);
    assert_int_equal(channels[1], 127U);
    assert_int_equal(channels[2], 255U);
    assert_int_equal(channels[3], 42U);
}

static void test_rgba_parse_rejects_invalid_text_without_mutation(void **state) {
    static const char *const invalid[] = {
        NULL, "", "1,2,3", "1,2,3,4,5", "1,2,3,4tail",
        "-1,2,3,4", "1,256,3,4", "1,,3,4"
    };
    (void)state;

    for (size_t i = 0U; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        uint8_t channels[4] = {9U, 8U, 7U, 6U};
        const uint8_t expected[4] = {9U, 8U, 7U, 6U};
        assert_false(rgba_parse(invalid[i], channels));
        assert_memory_equal(channels, expected, sizeof(channels));
    }
}

static void test_rgba_parse_rejects_null_output(void **state) {
    (void)state;
    assert_false(rgba_parse("1,2,3,4", NULL));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_rgba_parse_accepts_bounded_channels),
        cmocka_unit_test(test_rgba_parse_rejects_invalid_text_without_mutation),
        cmocka_unit_test(test_rgba_parse_rejects_null_output),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
