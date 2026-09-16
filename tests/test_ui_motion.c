#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>

#include <cmocka.h>

#include <math.h>

#include "../src/ui_motion.h"

static UiMotionTransition transition(UiThemeMotionRole role) {
    return (UiMotionTransition){0x51a7U, role, 1000.0, 0.0, 1.0, false};
}

static void test_exact_phase_boundaries_and_stable_endpoints(void **state) {
    UiMotionTransition value = transition(UI_THEME_MOTION_MAJOR_ENTER);
    UiMotionSample sample;
    (void)state;

    assert_true(ui_motion_sample(&value, 999.0, &sample));
    assert_int_equal(sample.phase, UI_MOTION_PHASE_PENDING);
    assert_float_equal(sample.progress, 0.0, 0.000001);
    assert_true(ui_motion_sample(&value, 1000.0, &sample));
    assert_int_equal(sample.phase, UI_MOTION_PHASE_ACTIVE);
    assert_float_equal(sample.value, 0.0, 0.000001);
    assert_true(ui_motion_sample(&value, 1159.999, &sample));
    assert_int_equal(sample.phase, UI_MOTION_PHASE_ACTIVE);
    assert_true(sample.value < 1.0);
    assert_true(ui_motion_sample(&value, 1160.0, &sample));
    assert_int_equal(sample.phase, UI_MOTION_PHASE_COMPLETE);
    assert_float_equal(sample.value, 1.0, 0.000001);
    assert_true(ui_motion_sample(&value, 1000000.0, &sample));
    assert_int_equal(sample.phase, UI_MOTION_PHASE_COMPLETE);
    assert_float_equal(sample.value, 1.0, 0.000001);
}

static void test_deterministic_paths_use_stable_identity(void **state) {
    UiMotionOffset first;
    UiMotionOffset repeated;
    UiMotionOffset other;
    (void)state;

    assert_true(ui_motion_glyph_offset(17U, 2U, 8U, 0.25, false, &first));
    assert_true(ui_motion_glyph_offset(17U, 2U, 8U, 0.25, false, &repeated));
    assert_memory_equal(&first, &repeated, sizeof(first));
    assert_true(ui_motion_glyph_offset(18U, 2U, 8U, 0.25, false, &other));
    assert_true(first.x != other.x || first.y != other.y);
    assert_true(ui_motion_glyph_offset(17U, 2U, 8U, 1.0, false, &other));
    assert_int_equal(other.x, 0);
    assert_int_equal(other.y, 0);
}

static void test_invalid_time_and_arguments_are_transactional(void **state) {
    UiMotionTransition value = transition(UI_THEME_MOTION_MAJOR_ENTER);
    UiMotionSample sample = {UI_MOTION_PHASE_COMPLETE, 7.0, 9.0};
    UiMotionSample before = sample;
    UiMotionOffset offset = {7, 9};
    UiMotionOffset offset_before = offset;
    (void)state;

    assert_false(ui_motion_sample(&value, NAN, &sample));
    assert_memory_equal(&sample, &before, sizeof(sample));
    assert_false(ui_motion_sample(&value, INFINITY, &sample));
    assert_memory_equal(&sample, &before, sizeof(sample));
    value.stable_id = 0U;
    assert_false(ui_motion_sample(&value, 1000.0, &sample));
    assert_memory_equal(&sample, &before, sizeof(sample));
    assert_false(ui_motion_glyph_offset(17U, 8U, 8U, 0.5, false, &offset));
    assert_memory_equal(&offset, &offset_before, sizeof(offset));
    assert_false(ui_motion_glyph_offset(17U, 0U, 8U, -0.1, false, &offset));
    assert_memory_equal(&offset, &offset_before, sizeof(offset));
}

static void test_interruption_and_reversal_have_no_jump(void **state) {
    UiMotionTransition entering = transition(UI_THEME_MOTION_MAJOR_ENTER);
    UiMotionTransition exiting;
    UiMotionSample before;
    UiMotionSample after;
    (void)state;

    assert_true(ui_motion_sample(&entering, 1080.0, &before));
    assert_true(ui_motion_redirect(&entering, 1080.0, UI_THEME_MOTION_MAJOR_EXIT,
                                   0.0, false, &exiting));
    assert_true(ui_motion_sample(&exiting, 1080.0, &after));
    assert_float_equal(after.value, before.value, 0.000001);
    assert_float_equal(exiting.start_value, before.value, 0.000001);
    assert_true(ui_motion_sample(&exiting, 1140.0, &after));
    assert_true(after.value < before.value);
    assert_true(ui_motion_sample(&exiting, 1200.0, &after));
    assert_float_equal(after.value, 0.0, 0.000001);
}

static void test_reduced_motion_is_immediate_and_non_spatial(void **state) {
    UiMotionTransition value = transition(UI_THEME_MOTION_RELATIONSHIP);
    UiMotionSample sample;
    UiMotionOffset offset;
    (void)state;

    value.reduced_motion = true;
    assert_true(ui_motion_sample(&value, 0.0, &sample));
    assert_int_equal(sample.phase, UI_MOTION_PHASE_COMPLETE);
    assert_float_equal(sample.value, 1.0, 0.000001);
    assert_true(ui_motion_glyph_offset(29U, 3U, 9U, 0.0, true, &offset));
    assert_int_equal(offset.x, 0);
    assert_int_equal(offset.y, 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_exact_phase_boundaries_and_stable_endpoints),
        cmocka_unit_test(test_deterministic_paths_use_stable_identity),
        cmocka_unit_test(test_invalid_time_and_arguments_are_transactional),
        cmocka_unit_test(test_interruption_and_reversal_have_no_jump),
        cmocka_unit_test(test_reduced_motion_is_immediate_and_non_spatial)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}