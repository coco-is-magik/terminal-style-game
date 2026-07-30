#include "benchmark_session.h"

bool benchmark_session_is_active(RunMode mode) {
    return mode == RUN_MODE_BENCHMARK_STRESS ||
           mode == RUN_MODE_BENCHMARK_RAYCAST ||
           mode == RUN_MODE_STABILITY ||
           mode == RUN_MODE_BENCHMARK_SCENARIO;
}

bool benchmark_session_should_stop(RunMode mode, uint64_t frame_count,
                                   uint64_t frame_limit, double elapsed_seconds,
                                   double duration_seconds) {
    if (mode == RUN_MODE_BENCHMARK_SCENARIO)
        return frame_count >= frame_limit;
    return mode != RUN_MODE_NORMAL && elapsed_seconds >= duration_seconds;
}

bool benchmark_session_frame_is_measured(uint64_t frame_count) {
    return frame_count >= BENCHMARK_WARMUP_FRAMES;
}

uint64_t benchmark_session_total_scenario_frames(int measured_frames) {
    if (measured_frames <= 0) return BENCHMARK_WARMUP_FRAMES;
    return BENCHMARK_WARMUP_FRAMES + (uint64_t)measured_frames;
}

uint64_t benchmark_session_measured_frames(uint64_t total_frames) {
    return total_frames > BENCHMARK_WARMUP_FRAMES
        ? total_frames - BENCHMARK_WARMUP_FRAMES : 0;
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