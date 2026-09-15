#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <float.h>
#include <locale.h>
#include <math.h>
#include <string.h>

#include "../src/platform_number.h"
#include "../src/platform_number_internal.h"

typedef struct {
    const char *input;
    const char *canonical;
} NumberCase;

static const NumberCase NUMBER_CASES[] = {
    {"0", "0"},
    {"-0", "0"},
    {"+0.0", "0"},
    {".5", "0.5"},
    {"1.", "1"},
    {"0.2", "0.20000000000000001"},
    {"-123.5", "-123.5"},
    {"9007199254740991", "9007199254740991"},
    {"1.0000000000000002", "1.0000000000000002"},
    {"1e20", "1e+20"},
    {"-1e20", "-1e+20"},
    {"1e-4", "0.0001"},
    {"1e-5", "1.0000000000000001e-05"},
    {"2.2250738585072014e-308", "2.2250738585072014e-308"},
    {"1.7976931348623157e+308", "1.7976931348623157e+308"}
};

static void test_fixed_parse_format_corpus(void **state) {
    (void)state;
    for (size_t i = 0U; i < sizeof(NUMBER_CASES) / sizeof(NUMBER_CASES[0]); i++) {
        double parsed = -42.0;
        double reparsed = 0.0;
        char text[32];
        size_t length = SIZE_MAX;
        assert_int_equal(platform_number_parse_double(NUMBER_CASES[i].input, &parsed),
                         PLATFORM_NUMBER_OK);
        assert_int_equal(platform_number_format_double(parsed, text, sizeof(text), &length),
                         PLATFORM_NUMBER_OK);
        assert_int_equal(length, strlen(NUMBER_CASES[i].canonical));
        assert_memory_equal(text, NUMBER_CASES[i].canonical, length + 1U);
        assert_int_equal(platform_number_parse_double(text, &reparsed), PLATFORM_NUMBER_OK);
        if (parsed == 0.0) {
            assert_true(reparsed == 0.0);
            assert_false(signbit(reparsed));
        } else {
            assert_memory_equal(&parsed, &reparsed, sizeof(parsed));
        }
    }
}

static void test_invalid_and_range_inputs_preserve_output(void **state) {
    static const char *const invalid[] = {
        "", "+", "-", ".", "e1", "1e", "1e+", "nan", "inf",
        "1,5", " 1", "1 ", "1x"
    };
    static const char *const range[] = {"1e309", "-1e309", "1e-400"};
    (void)state;
    for (size_t i = 0U; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        double output = 42.5;
        assert_int_equal(platform_number_parse_double(invalid[i], &output),
                         PLATFORM_NUMBER_INVALID_TEXT);
        assert_true(output == 42.5);
    }
    for (size_t i = 0U; i < sizeof(range) / sizeof(range[0]); i++) {
        double output = 42.5;
        assert_int_equal(platform_number_parse_double(range[i], &output),
                         PLATFORM_NUMBER_OUT_OF_RANGE);
        assert_true(output == 42.5);
    }
}

static void test_format_failures_preserve_outputs(void **state) {
    char output[8] = "keep";
    size_t length = 77U;
    (void)state;
    assert_int_equal(platform_number_format_double(INFINITY, output, sizeof(output), &length),
                     PLATFORM_NUMBER_OUT_OF_RANGE);
    assert_string_equal(output, "keep");
    assert_int_equal(length, 77U);
    assert_int_equal(platform_number_format_double(NAN, output, sizeof(output), &length),
                     PLATFORM_NUMBER_OUT_OF_RANGE);
    assert_string_equal(output, "keep");
    assert_int_equal(platform_number_format_double(0.2, output, 2U, &length),
                     PLATFORM_NUMBER_BUFFER_TOO_SMALL);
    assert_string_equal(output, "keep");
    assert_int_equal(length, 77U);
}

static void test_faults_and_invalid_arguments_are_typed(void **state) {
    double value = 17.0;
    char output[32] = "unchanged";
    size_t length = 23U;
    (void)state;
    assert_int_equal(platform_number_parse_double(NULL, &value),
                     PLATFORM_NUMBER_INVALID_ARGUMENT);
    assert_int_equal(platform_number_parse_double("1", NULL),
                     PLATFORM_NUMBER_INVALID_ARGUMENT);
    assert_int_equal(platform_number_internal_parse_double(
                         "1", &value, PLATFORM_NUMBER_FAULT_LOCALE),
                     PLATFORM_NUMBER_LOCALE_FAILED);
    assert_int_equal(platform_number_internal_parse_double(
                         "1", &value, PLATFORM_NUMBER_FAULT_PARSE),
                     PLATFORM_NUMBER_INVALID_TEXT);
    assert_true(value == 17.0);
    assert_int_equal(platform_number_internal_format_double(
                         1.0, output, sizeof(output), &length,
                         PLATFORM_NUMBER_FAULT_LOCALE),
                     PLATFORM_NUMBER_LOCALE_FAILED);
    assert_int_equal(platform_number_internal_format_double(
                         1.0, output, sizeof(output), &length,
                         PLATFORM_NUMBER_FAULT_FORMAT),
                     PLATFORM_NUMBER_FORMAT_FAILED);
    assert_string_equal(output, "unchanged");
    assert_int_equal(length, 23U);
}

static void test_process_locale_is_unchanged(void **state) {
    const char *current;
    char saved[128];
    double value;
    char output[32];
    size_t length;
    (void)state;
    current = setlocale(LC_NUMERIC, NULL);
    assert_non_null(current);
    assert_true(strlen(current) < sizeof(saved));
    memcpy(saved, current, strlen(current) + 1U);
    assert_int_equal(platform_number_parse_double("0.2", &value), PLATFORM_NUMBER_OK);
    assert_int_equal(platform_number_format_double(value, output, sizeof(output), &length),
                     PLATFORM_NUMBER_OK);
    assert_int_equal(platform_number_internal_parse_double(
                         "1", &value, PLATFORM_NUMBER_FAULT_LOCALE),
                     PLATFORM_NUMBER_LOCALE_FAILED);
    assert_int_equal(platform_number_internal_format_double(
                         1.0, output, sizeof(output), &length,
                         PLATFORM_NUMBER_FAULT_FORMAT),
                     PLATFORM_NUMBER_FORMAT_FAILED);
    assert_string_equal(setlocale(LC_NUMERIC, NULL), saved);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_fixed_parse_format_corpus),
        cmocka_unit_test(test_invalid_and_range_inputs_preserve_output),
        cmocka_unit_test(test_format_failures_preserve_outputs),
        cmocka_unit_test(test_faults_and_invalid_arguments_are_typed),
        cmocka_unit_test(test_process_locale_is_unchanged)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
