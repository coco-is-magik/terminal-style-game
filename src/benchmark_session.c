#include "benchmark_session.h"

bool benchmark_session_is_active(RunMode mode) {
    return mode == RUN_MODE_BENCHMARK_STRESS ||
           mode == RUN_MODE_BENCHMARK_RAYCAST ||
           mode == RUN_MODE_STABILITY ||
           mode == RUN_MODE_BENCHMARK_SCENARIO;
}

bool benchmark_session_should_stop(RunMode mode, uint64_t frame_count,
                                   int frame_limit, double elapsed_seconds,
                                   double duration_seconds) {
    if (mode == RUN_MODE_BENCHMARK_SCENARIO)
        return frame_limit >= 0 && frame_count >= (uint64_t)frame_limit;
    return mode != RUN_MODE_NORMAL && elapsed_seconds >= duration_seconds;
}

BenchmarkResult benchmark_session_classify(double average_render_ms,
                                           bool allocation_detected) {
    if (allocation_detected) return BENCHMARK_RESULT_FAIL_ALLOCATION;
    if (average_render_ms <= 4.0) return BENCHMARK_RESULT_IDEAL;
    if (average_render_ms <= 6.0) return BENCHMARK_RESULT_PASS_MINIMUM;
    return BENCHMARK_RESULT_FAIL_PERFORMANCE;
}

const char *benchmark_session_result_name(BenchmarkResult result) {
    switch (result) {
        case BENCHMARK_RESULT_IDEAL: return "ideal";
        case BENCHMARK_RESULT_PASS_MINIMUM: return "pass_minimum";
        case BENCHMARK_RESULT_FAIL_PERFORMANCE: return "fail_performance";
        case BENCHMARK_RESULT_FAIL_ALLOCATION: return "fail_allocation_detected";
        case BENCHMARK_RESULT_NOT_APPLICABLE:
        default: return "fail";
    }
}

int benchmark_session_exit_code(BenchmarkResult result) {
    return result == BENCHMARK_RESULT_IDEAL || result == BENCHMARK_RESULT_PASS_MINIMUM
           ? 0 : 1;
}