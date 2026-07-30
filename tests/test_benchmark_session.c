#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include "../src/benchmark_session.h"

static void test_policy(void **state) {
    (void)state;
    assert_false(benchmark_session_is_active(RUN_MODE_NORMAL));
    assert_true(benchmark_session_is_active(RUN_MODE_BENCHMARK_RAYCAST));
    assert_false(benchmark_session_should_stop(RUN_MODE_NORMAL, 99, 1, 99.0, 1.0));
    assert_true(benchmark_session_should_stop(RUN_MODE_BENCHMARK_SCENARIO, 10, 10, 0.0, 5.0));
    assert_true(benchmark_session_should_stop(RUN_MODE_STABILITY, 0, 0, 5.0, 5.0));
    assert_false(benchmark_session_frame_is_measured(BENCHMARK_WARMUP_FRAMES - 1U));
    assert_true(benchmark_session_frame_is_measured(BENCHMARK_WARMUP_FRAMES));
    assert_int_equal(benchmark_session_total_scenario_frames(120), 184);
    assert_int_equal(benchmark_session_measured_frames(63), 0);
    assert_int_equal(benchmark_session_measured_frames(184), 120);
    assert_int_equal(benchmark_session_classify(4.0, false), BENCHMARK_RESULT_IDEAL);
    assert_int_equal(benchmark_session_classify(6.0, false), BENCHMARK_RESULT_PASS_MINIMUM);
    assert_int_equal(benchmark_session_classify(6.01, false), BENCHMARK_RESULT_FAIL_PERFORMANCE);
    assert_int_equal(benchmark_session_classify(1.0, true), BENCHMARK_RESULT_FAIL_ALLOCATION);
    assert_string_equal(benchmark_session_result_name(BENCHMARK_RESULT_IDEAL), "ideal");
    assert_int_equal(benchmark_session_exit_code(BENCHMARK_RESULT_IDEAL), 0);
    assert_int_equal(benchmark_session_exit_code(BENCHMARK_RESULT_FAIL_PERFORMANCE), 1);
}

int main(void) {
    const struct CMUnitTest tests[] = {cmocka_unit_test(test_policy)};
    return cmocka_run_group_tests(tests, NULL, NULL);
}