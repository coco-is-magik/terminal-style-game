#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>

#include <cmocka.h>

#include <math.h>

#include "../src/ui_pause_motion.h"

static void test_exact_enter_and_exit_boundaries(void **state) {
    UiPauseMotionState motion;
    UiPauseMotionSample sample;
    (void)state;
    assert_true(ui_pause_motion_init(&motion, false, 1000.0, false));
    assert_true(ui_pause_motion_update(&motion, true, 1000.0, false, &sample));
    assert_true(sample.pause_visible);
    assert_true(sample.decoration_visible);
    assert_float_equal(sample.registration, 0.0, 0.000001);
    assert_true(ui_pause_motion_update(&motion, true, 1160.0, false, &sample));
    assert_false(sample.decoration_visible);
    assert_float_equal(sample.registration, 1.0, 0.000001);
    assert_true(ui_pause_motion_update(&motion, false, 1200.0, false, &sample));
    assert_false(sample.pause_visible);
    assert_true(sample.decoration_visible);
    assert_float_equal(sample.registration, 1.0, 0.000001);
    assert_true(ui_pause_motion_update(&motion, false, 1320.0, false, &sample));
    assert_false(sample.decoration_visible);
    assert_float_equal(sample.registration, 0.0, 0.000001);
}

static void test_replay_is_deterministic(void **state) {
    UiPauseMotionState first;
    UiPauseMotionState second;
    UiPauseMotionSample a;
    UiPauseMotionSample b;
    (void)state;
    assert_true(ui_pause_motion_init(&first, false, 20.0, false));
    assert_true(ui_pause_motion_init(&second, false, 20.0, false));
    assert_true(ui_pause_motion_update(&first, true, 20.0, false, &a));
    assert_true(ui_pause_motion_update(&second, true, 20.0, false, &b));
    assert_true(ui_pause_motion_update(&first, true, 100.0, false, &a));
    assert_true(ui_pause_motion_update(&second, true, 100.0, false, &b));
    assert_memory_equal(&a, &b, sizeof(a));
    assert_memory_equal(&first, &second, sizeof(first));
}

static void test_invalid_and_backward_time_are_transactional(void **state) {
    UiPauseMotionState motion;
    UiPauseMotionState before;
    UiPauseMotionSample sample = {true, true, UI_MOTION_PHASE_ACTIVE, 7.0, 9.0};
    UiPauseMotionSample sample_before = sample;
    (void)state;
    assert_true(ui_pause_motion_init(&motion, false, 100.0, false));
    before = motion;
    assert_false(ui_pause_motion_update(&motion, true, NAN, false, &sample));
    assert_memory_equal(&motion, &before, sizeof(motion));
    assert_memory_equal(&sample, &sample_before, sizeof(sample));
    assert_false(ui_pause_motion_update(&motion, true, 99.0, false, &sample));
    assert_memory_equal(&motion, &before, sizeof(motion));
    assert_memory_equal(&sample, &sample_before, sizeof(sample));
    assert_false(ui_pause_motion_init(NULL, false, 0.0, false));
}

static void test_interruption_and_reversal_are_continuous(void **state) {
    UiPauseMotionState motion;
    UiPauseMotionSample before;
    UiPauseMotionSample reversed;
    (void)state;
    assert_true(ui_pause_motion_init(&motion, false, 0.0, false));
    assert_true(ui_pause_motion_update(&motion, true, 0.0, false, &before));
    assert_true(ui_pause_motion_update(&motion, true, 80.0, false, &before));
    assert_true(ui_pause_motion_update(&motion, false, 80.0, false, &reversed));
    assert_float_equal(reversed.registration, before.registration, 0.000001);
    assert_true(ui_pause_motion_update(&motion, false, 140.0, false, &reversed));
    assert_true(reversed.registration < before.registration);
    assert_true(ui_pause_motion_update(&motion, true, 140.0, false, &before));
    assert_float_equal(before.registration, reversed.registration, 0.000001);
}

static void test_reduced_motion_is_immediate_and_decoration_free(void **state) {
    UiPauseMotionState motion;
    UiPauseMotionSample sample;
    (void)state;
    assert_true(ui_pause_motion_init(&motion, false, 0.0, true));
    assert_true(ui_pause_motion_update(&motion, true, 0.0, true, &sample));
    assert_true(sample.pause_visible);
    assert_false(sample.decoration_visible);
    assert_int_equal(sample.phase, UI_MOTION_PHASE_COMPLETE);
    assert_float_equal(sample.registration, 1.0, 0.000001);
    assert_true(ui_pause_motion_update(&motion, false, 0.0, true, &sample));
    assert_false(sample.pause_visible);
    assert_false(sample.decoration_visible);
    assert_float_equal(sample.registration, 0.0, 0.000001);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_exact_enter_and_exit_boundaries),
        cmocka_unit_test(test_replay_is_deterministic),
        cmocka_unit_test(test_invalid_and_backward_time_are_transactional),
        cmocka_unit_test(test_interruption_and_reversal_are_continuous),
        cmocka_unit_test(test_reduced_motion_is_immediate_and_decoration_free)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}