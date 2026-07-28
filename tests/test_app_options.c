#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "../src/app_options.h"

static void test_defaults_and_modes(void **state) {
    AppOptions options;
    char *defaults[] = {"app"};
    char *mode[] = {"app", "--mode", "stress"};
    (void)state;

    assert_int_equal(app_options_parse(1, defaults, &options), APP_OPTIONS_OK);
    assert_int_equal(options.mode, RUN_MODE_NORMAL);
    assert_int_equal(options.visual_mode, VISUAL_RAYCAST);
    assert_int_equal(options.benchmark_frames, 600);
    assert_int_equal(app_options_parse(3, mode, &options), APP_OPTIONS_OK);
    assert_int_equal(options.visual_mode, VISUAL_STRESS);
}

static void test_benchmark_forms(void **state) {
    AppOptions options;
    char *scenario[] = {"app", "--benchmark-scenario", "idle", "--frames", "2"};
    char *timed[] = {"app", "--benchmark-raycast", "0.25"};
    (void)state;

    assert_int_equal(app_options_parse(5, scenario, &options), APP_OPTIONS_OK);
    assert_int_equal(options.mode, RUN_MODE_BENCHMARK_SCENARIO);
    assert_string_equal(options.benchmark_scenario, "idle");
    assert_int_equal(options.benchmark_frames, 2);
    assert_int_equal(app_options_parse(3, timed, &options), APP_OPTIONS_OK);
    assert_int_equal(options.mode, RUN_MODE_BENCHMARK_RAYCAST);
    assert_float_equal(options.run_duration_seconds, 0.25, 0.0001);
}

static void test_smoke_mode(void **state) {
    AppOptions options;
    char *args[] = {"app", "--smoke-test"};
    (void)state;

    assert_int_equal(app_options_parse(2, args, &options), APP_OPTIONS_OK);
    assert_int_equal(options.mode, RUN_MODE_SMOKE);
}

static void test_rejects_missing_unknown_and_invalid(void **state) {
    AppOptions options;
    char *missing[] = {"app", "--frames"};
    char *unknown[] = {"app", "--wat"};
    char *bad_number[] = {"app", "--benchmark-raycast", "2x"};
    char *zero[] = {"app", "--frames", "0"};
    char *bad_scenario[] = {"app", "--benchmark-scenario", "static"};
    (void)state;

    assert_int_equal(app_options_parse(2, missing, &options), APP_OPTIONS_MISSING_VALUE);
    assert_int_equal(app_options_parse(2, unknown, &options), APP_OPTIONS_UNKNOWN_OPTION);
    assert_int_equal(app_options_parse(3, bad_number, &options), APP_OPTIONS_INVALID_VALUE);
    assert_int_equal(app_options_parse(3, zero, &options), APP_OPTIONS_INVALID_VALUE);
    assert_int_equal(app_options_parse(3, bad_scenario, &options), APP_OPTIONS_INVALID_VALUE);
}

static void test_rejects_conflicts_transactionally(void **state) {
    AppOptions options = {RUN_MODE_STABILITY, VISUAL_STRESS, 9.0, "old", 9};
    char *frames_only[] = {"app", "--frames", "2"};
    char *two_modes[] = {"app", "--benchmark-raycast", "1", "--stability-test", "1"};
    (void)state;

    assert_int_equal(app_options_parse(3, frames_only, &options), APP_OPTIONS_CONFLICT);
    assert_int_equal(options.mode, RUN_MODE_STABILITY);
    assert_int_equal(app_options_parse(5, two_modes, &options), APP_OPTIONS_CONFLICT);
    assert_int_equal(options.mode, RUN_MODE_STABILITY);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_defaults_and_modes),
        cmocka_unit_test(test_benchmark_forms),
        cmocka_unit_test(test_smoke_mode),
        cmocka_unit_test(test_rejects_missing_unknown_and_invalid),
        cmocka_unit_test(test_rejects_conflicts_transactionally),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}