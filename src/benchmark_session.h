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

bool benchmark_session_is_active(RunMode mode);
bool benchmark_session_should_stop(RunMode mode, uint64_t frame_count,
                                   int frame_limit, double elapsed_seconds,
                                   double duration_seconds);
BenchmarkResult benchmark_session_classify(double average_render_ms,
                                           bool allocation_detected);
const char *benchmark_session_result_name(BenchmarkResult result);
int benchmark_session_exit_code(BenchmarkResult result);

#endif