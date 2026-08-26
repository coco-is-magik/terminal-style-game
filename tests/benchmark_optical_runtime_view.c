/** benchmark_optical_runtime_view.c — I1 default optical lookup overhead */
#define _POSIX_C_SOURCE 200809L

#include "../src/optical_runtime_view.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define SAMPLE_COUNT 41600U
#define WARMUP_FRAMES 20U
#define BENCHMARK_FRAMES 500U
#define OPTICAL_DEFAULT_OVERHEAD_PASS_MS 0.100

static volatile uint64_t benchmark_sink;

static double now_ms(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return -1.0;
    return (double)value.tv_sec * 1000.0 + (double)value.tv_nsec / 1000000.0;
}

static uint64_t hash_resolved(uint64_t hash, const OpticalResolved *resolved) {
    const uint8_t bytes[] = {
        resolved->player_blocks ? 1U : 0U,
        resolved->ray_blocks ? 1U : 0U,
        resolved->light_blocks ? 1U : 0U,
        resolved->opacity,
        resolved->transmission,
        resolved->reflectivity
    };
    for (size_t i = 0U; i < sizeof(bytes); i++) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t run_legacy(void) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0U; i < SAMPLE_COUNT; i++) {
        OpticalResolved resolved = optical_resolved_legacy((i & 3U) == 0U);
        hash = hash_resolved(hash, &resolved);
    }
    return hash;
}

static uint64_t run_view(const OpticalRuntimeView *view) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0U; i < SAMPLE_COUNT; i++) {
        OpticalResolved resolved;
        if (!optical_runtime_view_resolve(
                view, i, (uint16_t)(i & UINT16_C(0xffff)),
                (i & 3U) == 0U, &resolved)) return 0U;
        hash = hash_resolved(hash, &resolved);
    }
    return hash;
}

static double measure_legacy(uint64_t expected, bool *deterministic) {
    double start = now_ms();
    for (size_t frame = 0U; frame < BENCHMARK_FRAMES; frame++) {
        uint64_t checksum = run_legacy();
        if (checksum != expected) *deterministic = false;
        benchmark_sink ^= checksum;
    }
    {
        double end = now_ms();
        return start < 0.0 || end < start ? -1.0 :
            (end - start) / BENCHMARK_FRAMES;
    }
}

static double measure_view(const OpticalRuntimeView *view, uint64_t expected,
                           bool *deterministic) {
    double start = now_ms();
    for (size_t frame = 0U; frame < BENCHMARK_FRAMES; frame++) {
        uint64_t checksum = run_view(view);
        if (checksum != expected) *deterministic = false;
        benchmark_sink ^= checksum;
    }
    {
        double end = now_ms();
        return start < 0.0 || end < start ? -1.0 :
            (end - start) / BENCHMARK_FRAMES;
    }
}

int main(void) {
    OpticalRuntimeView view;
    uint64_t legacy_checksum;
    uint64_t view_checksum;
    double legacy_ms;
    double view_ms;
    double overhead_ms;
    bool deterministic = true;
    bool pass;
    if (!optical_runtime_view_init(
            &view, SAMPLE_COUNT, NULL, 0U, NULL, 0U, UINT32_C(1))) return 1;
    legacy_checksum = run_legacy();
    view_checksum = run_view(&view);
    if (legacy_checksum == 0U || view_checksum != legacy_checksum) return 1;
    for (size_t frame = 0U; frame < WARMUP_FRAMES; frame++) {
        benchmark_sink ^= run_legacy();
        benchmark_sink ^= run_view(&view);
    }
    legacy_ms = measure_legacy(legacy_checksum, &deterministic);
    view_ms = measure_view(&view, view_checksum, &deterministic);
    if (legacy_ms < 0.0 || view_ms < 0.0) return 1;
    overhead_ms = view_ms - legacy_ms;
    pass = deterministic && overhead_ms <= OPTICAL_DEFAULT_OVERHEAD_PASS_MS;
    printf("{\n");
    printf("  \"scenario\": \"default_optical_lookup\",\n");
    printf("  \"warmup_frames\": %u,\n", WARMUP_FRAMES);
    printf("  \"frames\": %u,\n", BENCHMARK_FRAMES);
    printf("  \"queries_per_frame\": %u,\n", SAMPLE_COUNT);
    printf("  \"legacy_ms\": %.6f,\n", legacy_ms);
    printf("  \"view_ms\": %.6f,\n", view_ms);
    printf("  \"overhead_ms\": %.6f,\n", overhead_ms);
    printf("  \"overhead_ns_per_query\": %.3f,\n",
           overhead_ms * 1000000.0 / SAMPLE_COUNT);
    printf("  \"pass_overhead_ms\": %.3f,\n", OPTICAL_DEFAULT_OVERHEAD_PASS_MS);
    printf("  \"legacy_checksum\": %llu,\n",
           (unsigned long long)legacy_checksum);
    printf("  \"view_checksum\": %llu,\n",
           (unsigned long long)view_checksum);
    printf("  \"exact_parity\": true,\n");
    printf("  \"deterministic\": %s,\n", deterministic ? "true" : "false");
    printf("  \"result\": \"%s\"\n", pass ? "pass" : "fail");
    printf("}\n");
    return pass ? 0 : 1;
}
