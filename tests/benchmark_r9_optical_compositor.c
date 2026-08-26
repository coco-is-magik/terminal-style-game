/** benchmark_r9_optical_compositor.c — P3 bounded terminal-cell composition */
#define _POSIX_C_SOURCE 200809L

#include "../src/r9_optical_compositor.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifndef R9_OPTICAL_RESEARCH
#error "P3 benchmark requires R9_OPTICAL_RESEARCH=1"
#endif

#define SAMPLE_COUNT 41600U
#define WARMUP_FRAMES 20U
#define BENCHMARK_FRAMES 500U

static volatile uint64_t benchmark_sink;

static double now_ms(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return -1.0;
    return (double)value.tv_sec * 1000.0 + (double)value.tv_nsec / 1000000.0;
}

static uint64_t hash_cell(uint64_t hash, const Cell *cell) {
    const uint8_t bytes[] = {
        cell->glyph,
        cell->fg.r, cell->fg.g, cell->fg.b, cell->fg.a,
        cell->bg.r, cell->bg.g, cell->bg.b, cell->bg.a
    };
    for (size_t i = 0U; i < sizeof(bytes); i++) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t run_baseline(const R9OpticalLayer *layers) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0U; i < SAMPLE_COUNT; i++)
        hash = hash_cell(hash, &layers[0].sampled_cell);
    return hash;
}

static uint64_t run_compositor(const R9OpticalLayer *layers, size_t count,
                               const Cell *darkness) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0U; i < SAMPLE_COUNT; i++) {
        R9OpticalComposite output;
        if (!r9_optical_composite(layers, count, false, darkness, &output)) return 0U;
        hash = hash_cell(hash, &output.cell);
    }
    return hash;
}

static double measure_baseline(const R9OpticalLayer *layers, uint64_t expected,
                               bool *deterministic) {
    double start = now_ms();
    for (size_t frame = 0U; frame < BENCHMARK_FRAMES; frame++) {
        uint64_t checksum = run_baseline(layers);
        if (checksum != expected) *deterministic = false;
        benchmark_sink ^= checksum;
    }
    {
        double end = now_ms();
        return start < 0.0 || end < start ? -1.0 :
            (end - start) / BENCHMARK_FRAMES;
    }
}

static double measure_compositor(const R9OpticalLayer *layers, size_t count,
                                 const Cell *darkness, uint64_t expected,
                                 bool *deterministic) {
    double start = now_ms();
    for (size_t frame = 0U; frame < BENCHMARK_FRAMES; frame++) {
        uint64_t checksum = run_compositor(layers, count, darkness);
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
    const Cell darkness = {' ', {0U, 0U, 0U, 255U}, {0U, 0U, 0U, 255U}};
    R9OpticalLayer layers[R9_OPTICAL_COMPOSITOR_MAX_LAYERS] = {
        {{'A', {200U, 40U, 20U, 255U}, {20U, 10U, 5U, 255U}},
         {false, false, false, 64U, 192U, 0U}, false},
        {{'B', {40U, 200U, 60U, 255U}, {5U, 30U, 10U, 255U}},
         {false, false, false, 80U, 175U, 0U}, false},
        {{'C', {30U, 60U, 220U, 255U}, {10U, 20U, 50U, 255U}},
         {false, false, false, 96U, 159U, 0U}, true},
        {{'D', {180U, 160U, 100U, 255U}, {40U, 30U, 20U, 255U}},
         {false, true, true, 255U, 0U, 0U}, false}
    };
    uint64_t checksums[R9_OPTICAL_COMPOSITOR_MAX_LAYERS + 1U] = {0U};
    double averages[R9_OPTICAL_COMPOSITOR_MAX_LAYERS + 1U] = {0.0};
    bool deterministic = true;
    layers[0].optical = (R9OpticalResolved){false, true, true, 255U, 0U, 0U};
    checksums[0] = run_baseline(layers);
    checksums[1] = run_compositor(layers, 1U, &darkness);
    layers[0].optical = (R9OpticalResolved){false, false, false, 64U, 192U, 0U};
    for (size_t count = 2U; count <= R9_OPTICAL_COMPOSITOR_MAX_LAYERS; count++) {
        R9OpticalResolved saved = layers[count - 1U].optical;
        layers[count - 1U].optical =
            (R9OpticalResolved){false, true, true, 255U, 0U, 0U};
        checksums[count] = run_compositor(layers, count, &darkness);
        layers[count - 1U].optical = saved;
    }
    if (checksums[0] == 0U || checksums[1] != checksums[0]) return 1;
    for (size_t frame = 0U; frame < WARMUP_FRAMES; frame++) {
        layers[0].optical = (R9OpticalResolved){false, true, true, 255U, 0U, 0U};
        benchmark_sink ^= run_baseline(layers);
        benchmark_sink ^= run_compositor(layers, 1U, &darkness);
        layers[0].optical = (R9OpticalResolved){false, false, false, 64U, 192U, 0U};
        for (size_t count = 2U; count <= R9_OPTICAL_COMPOSITOR_MAX_LAYERS; count++) {
            R9OpticalResolved saved = layers[count - 1U].optical;
            layers[count - 1U].optical =
                (R9OpticalResolved){false, true, true, 255U, 0U, 0U};
            benchmark_sink ^= run_compositor(layers, count, &darkness);
            layers[count - 1U].optical = saved;
        }
    }
    layers[0].optical = (R9OpticalResolved){false, true, true, 255U, 0U, 0U};
    averages[0] = measure_baseline(layers, checksums[0], &deterministic);
    averages[1] = measure_compositor(layers, 1U, &darkness,
                                     checksums[1], &deterministic);
    layers[0].optical = (R9OpticalResolved){false, false, false, 64U, 192U, 0U};
    for (size_t count = 2U; count <= R9_OPTICAL_COMPOSITOR_MAX_LAYERS; count++) {
        R9OpticalResolved saved = layers[count - 1U].optical;
        layers[count - 1U].optical =
            (R9OpticalResolved){false, true, true, 255U, 0U, 0U};
        averages[count] = measure_compositor(
            layers, count, &darkness, checksums[count], &deterministic);
        layers[count - 1U].optical = saved;
    }
    if (!deterministic) return 1;
    for (size_t i = 0U; i <= R9_OPTICAL_COMPOSITOR_MAX_LAYERS; i++)
        if (averages[i] < 0.0) return 1;
    printf("{\n");
    printf("  \"scenario\": \"bounded_synthetic_terminal_cells\",\n");
    printf("  \"warmup_frames\": %u,\n", WARMUP_FRAMES);
    printf("  \"frames\": %u,\n", BENCHMARK_FRAMES);
    printf("  \"samples_per_frame\": %u,\n", SAMPLE_COUNT);
    printf("  \"baseline_ms\": %.6f,\n", averages[0]);
    printf("  \"baseline_checksum\": %llu,\n",
           (unsigned long long)checksums[0]);
    for (size_t count = 1U; count <= R9_OPTICAL_COMPOSITOR_MAX_LAYERS; count++) {
        printf("  \"compose_%zu_ms\": %.6f,\n", count, averages[count]);
        printf("  \"compose_%zu_overhead_ms\": %.6f,\n", count,
               averages[count] - averages[0]);
        printf("  \"compose_%zu_ns_per_sample\": %.3f,\n", count,
               averages[count] * 1000000.0 / SAMPLE_COUNT);
        printf("  \"compose_%zu_checksum\": %llu,\n", count,
               (unsigned long long)checksums[count]);
    }
    printf("  \"n1_opaque_parity\": true,\n");
    printf("  \"deterministic\": true\n");
    printf("}\n");
    return 0;
}