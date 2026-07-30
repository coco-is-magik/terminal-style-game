#ifndef BENCHMARK_SESSION_H
#define BENCHMARK_SESSION_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BENCHMARK_RESULT_NOT_APPLICABLE = 0,
    BENCHMARK_RESULT_IDEAL,
    BENCHMARK_RESULT_PASS_MINIMUM,
    BENCHMARK_RESULT_FAIL_PERFORMANCE,
    BENCHMARK_RESULT_FAIL_ALLOCATION
} BenchmarkResult;

#define BENCHMARK_WARMUP_FRAMES 64U

bool benchmark_session_is_active(RunMode mode);
bool benchmark_session_should_stop(RunMode mode, uint64_t frame_count,
                                   uint64_t frame_limit, double elapsed_seconds,
                                   double duration_seconds);
bool benchmark_session_frame_is_measured(uint64_t frame_count);
uint64_t benchmark_session_total_scenario_frames(int measured_frames);
uint64_t benchmark_session_measured_frames(uint64_t total_frames);
BenchmarkResult benchmark_session_classify(double average_render_ms,
                                           bool allocation_detected);
const char *benchmark_session_result_name(BenchmarkResult result);
int benchmark_session_exit_code(BenchmarkResult result);

#endif