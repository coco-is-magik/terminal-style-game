#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "../src/app_options.h"
#include "../src/display_acceptance.h"

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
    char *layered[] = {"app", "--benchmark-scenario", "ui-layered", "--frames", "120"};
    (void)state;

    assert_int_equal(app_options_parse(5, scenario, &options), APP_OPTIONS_OK);
    assert_int_equal(options.mode, RUN_MODE_BENCHMARK_SCENARIO);
    assert_string_equal(options.benchmark_scenario, "idle");
    assert_int_equal(options.benchmark_frames, 2);
    assert_int_equal(app_options_parse(3, timed, &options), APP_OPTIONS_OK);
    assert_int_equal(options.mode, RUN_MODE_BENCHMARK_RAYCAST);
    assert_float_equal(options.run_duration_seconds, 0.25, 0.0001);
    assert_int_equal(app_options_parse(5, layered, &options), APP_OPTIONS_OK);
    assert_string_equal(options.benchmark_scenario, "ui-layered");
    assert_int_equal(options.benchmark_frames, 120);
}

static void test_smoke_mode(void **state) {
    AppOptions options;
    char *args[] = {"app", "--smoke-test"};
    (void)state;

    assert_int_equal(app_options_parse(2, args, &options), APP_OPTIONS_OK);
    assert_int_equal(options.mode, RUN_MODE_SMOKE);
}

static void test_display_acceptance_mode(void **state) {
    AppOptions options;
    char *valid[] = {"app", "--display-acceptance", "20"};
    char *missing[] = {"app", "--display-acceptance"};
    char *invalid[] = {"app", "--display-acceptance", "0"};
    char *conflict[] = {"app", "--display-acceptance", "20", "--smoke-test"};
    (void)state;

    assert_int_equal(app_options_parse(3, valid, &options), APP_OPTIONS_OK);
    assert_int_equal(options.mode, RUN_MODE_DISPLAY_ACCEPTANCE);
    assert_float_equal(options.run_duration_seconds, 20.0, 0.0001);
    assert_int_equal(app_options_parse(2, missing, &options), APP_OPTIONS_MISSING_VALUE);
    assert_int_equal(app_options_parse(3, invalid, &options), APP_OPTIONS_INVALID_VALUE);
    assert_int_equal(app_options_parse(4, conflict, &options), APP_OPTIONS_CONFLICT);
}

static void test_ui_theme_demo_mode_and_conflicts(void **state) {
    AppOptions options;
    AppOptions before;
    char *valid[] = {"app", "--ui-theme-demo"};
    char *mode_conflict[] = {"app", "--ui-theme-demo", "--mode", "normal"};
    char *frames_conflict[] = {"app", "--ui-theme-demo", "--frames", "2"};
    char *other_conflict[] = {"app", "--ui-theme-demo", "--smoke-test"};
    (void)state;
    assert_int_equal(app_options_parse(2, valid, &options), APP_OPTIONS_OK);
    assert_int_equal(options.mode, RUN_MODE_UI_THEME_DEMO);
    before = options;
    assert_int_equal(app_options_parse(4, mode_conflict, &options), APP_OPTIONS_CONFLICT);
    assert_memory_equal(&options, &before, sizeof(options));
    assert_int_equal(app_options_parse(4, frames_conflict, &options), APP_OPTIONS_CONFLICT);
    assert_memory_equal(&options, &before, sizeof(options));
    assert_int_equal(app_options_parse(3, other_conflict, &options), APP_OPTIONS_CONFLICT);
    assert_memory_equal(&options, &before, sizeof(options));
}

static void test_ui_motion_demo_mode_and_conflicts(void **state) {
    AppOptions options;
    AppOptions before;
    char *valid[] = {"app", "--ui-motion-demo"};
    char *mode_conflict[] = {"app", "--ui-motion-demo", "--mode", "normal"};
    char *other_conflict[] = {"app", "--ui-motion-demo", "--ui-theme-demo"};
    (void)state;
    assert_int_equal(app_options_parse(2, valid, &options), APP_OPTIONS_OK);
    assert_int_equal(options.mode, RUN_MODE_UI_MOTION_DEMO);
    before = options;
    assert_int_equal(app_options_parse(4, mode_conflict, &options), APP_OPTIONS_CONFLICT);
    assert_memory_equal(&options, &before, sizeof(options));
    assert_int_equal(app_options_parse(3, other_conflict, &options), APP_OPTIONS_CONFLICT);
    assert_memory_equal(&options, &before, sizeof(options));
}

static void test_display_acceptance_requires_all_observations(void **state) {
    DisplayAcceptance acceptance = {0};
    InputState input = {0};
    (void)state;

    assert_false(display_acceptance_passed(&acceptance));
    input.confirm = true;
    input.mouse_dx = 2.0f;
    input.mouse_left_pressed = true;
    input.mouse_left_released = true;
    display_acceptance_observe_input(&acceptance, &input);
    display_acceptance_observe_transition(
        &acceptance, APP_STATE_MAIN_MENU, APP_STATE_PLAYING);
    display_acceptance_observe_presentation(&acceptance);
    acceptance.resized = true;
    assert_true(display_acceptance_passed(&acceptance));

    display_acceptance_observe_input(NULL, &input);
    display_acceptance_observe_input(&acceptance, NULL);
    display_acceptance_observe_transition(NULL, APP_STATE_MAIN_MENU, APP_STATE_PLAYING);
    display_acceptance_observe_presentation(NULL);
    assert_false(display_acceptance_passed(NULL));
}

static void test_display_acceptance_json_text_is_bounded(void **state) {
    char output[32];
    char small[4] = "old";
    (void)state;

    assert_true(display_acceptance_json_text(output, sizeof(output), "a\"b\\c\n"));
    assert_string_equal(output, "a\\\"b\\\\c\\u000a");
    assert_true(display_acceptance_json_text(output, sizeof(output), NULL));
    assert_string_equal(output, "unknown");
    assert_false(display_acceptance_json_text(small, sizeof(small), "long"));
    assert_string_equal(small, "lon");
    assert_false(display_acceptance_json_text(NULL, sizeof(output), "x"));
    assert_false(display_acceptance_json_text(output, 0, "x"));
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
        cmocka_unit_test(test_display_acceptance_mode),
        cmocka_unit_test(test_ui_theme_demo_mode_and_conflicts),
        cmocka_unit_test(test_ui_motion_demo_mode_and_conflicts),
        cmocka_unit_test(test_display_acceptance_requires_all_observations),
        cmocka_unit_test(test_display_acceptance_json_text_is_bounded),
        cmocka_unit_test(test_rejects_missing_unknown_and_invalid),
        cmocka_unit_test(test_rejects_conflicts_transactionally),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
