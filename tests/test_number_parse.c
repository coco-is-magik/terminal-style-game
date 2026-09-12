#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <limits.h>
#include <cmocka.h>

#include "../src/number_parse.h"

static void test_number_parse_int_checks_range_and_consumption(void **state) {
    int value = 7;
    (void)state;

    assert_true(number_parse_int(" -12 ", -20, 20, &value));
    assert_int_equal(value, -12);
    assert_false(number_parse_int("21", -20, 20, &value));
    assert_false(number_parse_int("1tail", INT_MIN, INT_MAX, &value));
    assert_false(number_parse_int("999999999999999999999", INT_MIN, INT_MAX, &value));
    assert_int_equal(value, -12);
}

static void test_number_parse_double_requires_finite_complete_value(void **state) {
    double value = 3.5;
    (void)state;

    assert_true(number_parse_finite_double(" -1.25 ", &value));
    assert_float_equal(value, -1.25, 0.000001);
    assert_false(number_parse_finite_double("nan", &value));
    assert_false(number_parse_finite_double("inf", &value));
    assert_false(number_parse_finite_double("1.5tail", &value));
    assert_false(number_parse_finite_double("", &value));
    assert_float_equal(value, -1.25, 0.000001);
}

static void test_number_parse_rejects_invalid_arguments(void **state) {
    int value = 0;
    (void)state;

    assert_false(number_parse_int(NULL, 0, 1, &value));
    assert_false(number_parse_int("0", 1, 0, &value));
    assert_false(number_parse_int("0", 0, 1, NULL));
    assert_false(number_parse_finite_double(NULL, NULL));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_number_parse_int_checks_range_and_consumption),
        cmocka_unit_test(test_number_parse_double_requires_finite_complete_value),
        cmocka_unit_test(test_number_parse_rejects_invalid_arguments),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}