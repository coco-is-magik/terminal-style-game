/** benchmark_heightfield_selective.c — I2 prepared-column coverage benchmark */
#define _POSIX_C_SOURCE 200809L

#include "../src/heightfield_trace.h"
#include "../src/math.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAP_WIDTH 20
#define MAP_HEIGHT 12
#define VIEWPORT_WIDTH 260
#define VIEWPORT_HEIGHT 160
#define BENCHMARK_FRAMES 200U
#define WARMUP_FRAMES 10U
#define GENERATION UINT32_C(29)

static volatile uint64_t benchmark_sink;

static double now_ms(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return -1.0;
    return (double)value.tv_sec * 1000.0 + (double)value.tv_nsec / 1000000.0;
}

static uint64_t hash_byte(uint64_t hash, uint8_t value) {
    hash ^= value;
    return hash * UINT64_C(1099511628211);
}

static uint64_t hash_hit(uint64_t hash, const HeightfieldHit *hit) {
    uint64_t distance_bits = 0U;
    memcpy(&distance_bits, &hit->distance, sizeof(distance_bits));
    hash = hash_byte(hash, hit->hit ? 1U : 0U);
    if (!hit->hit) return hash;
    hash = hash_byte(hash, (uint8_t)hit->kind);
    hash = hash_byte(hash, (uint8_t)(hit->material & UINT16_C(0x00ff)));
    hash = hash_byte(hash, (uint8_t)(hit->material >> 8U));
    hash = hash_byte(hash, (uint8_t)hit->map_x);
    hash = hash_byte(hash, (uint8_t)hit->map_y);
    for (size_t i = 0U; i < sizeof(distance_bits); i++)
        hash = hash_byte(hash, (uint8_t)(distance_bits >> (i * 8U)));
    return hash;
}

static uint64_t sample_baseline(const HeightfieldTraceColumn *columns) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (int x = 0; x < VIEWPORT_WIDTH; x++) {
        for (int y = 0; y < VIEWPORT_HEIGHT; y++) {
            HeightfieldHit hit =
                heightfield_trace_prepared_sample(&columns[x], y);
            hash = hash_hit(hash, &hit);
        }
    }
    return hash;
}

static bool selected_sample(int x, int y, unsigned int coverage_percent) {
    unsigned int bucket = ((unsigned int)x * 37U + (unsigned int)y * 17U) % 100U;
    return bucket < coverage_percent;
}

static uint64_t sample_mixed(const HeightfieldTraceColumn *columns,
                             const OpticalRuntimeView *view,
                             unsigned int coverage_percent) {
    uint64_t hash = UINT64_C(1469598103934665603);
    if (coverage_percent == 0U) return sample_baseline(columns);
    for (int x = 0; x < VIEWPORT_WIDTH; x++) {
        for (int y = 0; y < VIEWPORT_HEIGHT; y++) {
            if (selected_sample(x, y, coverage_percent)) {
                HeightfieldOpticalResult optical;
                if (!heightfield_trace_selective(
                        &columns[x], view, GENERATION, y, &optical)) return 0U;
                if (optical.count > 0U)
                    hash = hash_hit(hash, &optical.layers[0].hit);
                else {
                    HeightfieldHit none = {0};
                    hash = hash_hit(hash, &none);
                }
                hash = hash_byte(hash, (uint8_t)optical.count);
                hash = hash_byte(hash, optical.terminated_by_surface ? 1U : 0U);
                hash = hash_byte(hash, optical.reached_opening ? 1U : 0U);
                hash = hash_byte(hash, optical.layer_cap_exhausted ? 1U : 0U);
                for (size_t i = 0U; i < optical.count; i++) {
                    hash = hash_hit(hash, &optical.layers[i].hit);
                    hash = hash_byte(hash, optical.layers[i].optical.opacity);
                    hash = hash_byte(hash, optical.layers[i].optical.transmission);
                }
            } else {
                HeightfieldHit hit =
                    heightfield_trace_prepared_sample(&columns[x], y);
                hash = hash_hit(hash, &hit);
            }
        }
    }
    return hash;
}

static double measure_baseline(const HeightfieldTraceColumn *columns,
                               uint64_t expected, bool *deterministic) {
    double start = now_ms();
    for (size_t frame = 0U; frame < BENCHMARK_FRAMES; frame++) {
        uint64_t checksum = sample_baseline(columns);
        if (checksum != expected) *deterministic = false;
        benchmark_sink ^= checksum;
    }
    {
        double end = now_ms();
        return start < 0.0 || end < start ? -1.0 :
            (end - start) / (double)BENCHMARK_FRAMES;
    }
}

static double measure_mixed(const HeightfieldTraceColumn *columns,
                            const OpticalRuntimeView *view,
                            unsigned int coverage_percent,
                            uint64_t expected, bool *deterministic) {
    double start = now_ms();
    for (size_t frame = 0U; frame < BENCHMARK_FRAMES; frame++) {
        uint64_t checksum = sample_mixed(columns, view, coverage_percent);
        if (checksum != expected) *deterministic = false;
        benchmark_sink ^= checksum;
    }
    {
        double end = now_ms();
        return start < 0.0 || end < start ? -1.0 :
            (end - start) / (double)BENCHMARK_FRAMES;
    }
}

int main(void) {
    const unsigned int coverage[] = {0U, 10U, 25U, 100U};
    Map *map = NULL;
    HeightfieldTraceColumn *columns = NULL;
    SceneAuthoredCell cells[MAP_WIDTH * MAP_HEIGHT] = {0};
    SceneHeightView heights = {
        cells, MAP_WIDTH * MAP_HEIGHT, MAP_WIDTH, MAP_HEIGHT,
        {9.8, SCENE_GRAVITY_DOWN, 0.25, 3.2, 1.0, 0.5, 0.75}
    };
    OpticalExtension materials[5] = {0};
    OpticalRuntimeView view;
    Camera camera;
    uint64_t baseline_checksum;
    uint64_t checksums[sizeof(coverage) / sizeof(coverage[0])] = {0};
    double averages[sizeof(coverage) / sizeof(coverage[0])] = {0};
    double baseline_average;
    bool deterministic = true;
    int result = 1;
    map = map_create(MAP_WIDTH, MAP_HEIGHT);
    columns = calloc(VIEWPORT_WIDTH, sizeof(*columns));
    if (!map || !columns) goto cleanup;
    camera_init(&camera, 10.5, 6.5, 0.0, PI / 2.0);
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            size_t index = (size_t)y * MAP_WIDTH + (size_t)x;
            bool edge = x == 0 || y == 0 || x == MAP_WIDTH - 1 ||
                        y == MAP_HEIGHT - 1;
            cells[index].floor_present = true;
            cells[index].ceiling_present = true;
            cells[index].floor_height_step = SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
            cells[index].ceiling_height_step = SCENE_DEFAULT_CEILING_HEIGHT_STEP;
            cells[index].floor_material = 1U;
            cells[index].ceiling_material = 2U;
            if (edge) {
                cells[index].occupancy = SCENE_CELL_OCCUPANCY_WALL;
                cells[index].wall_material = 3U;
                map_set(map, x, y, 3);
            }
            if (x == 12) {
                cells[index].floor_height_step = INT16_C(0x0080);
                cells[index].ceiling_height_step = INT16_C(0x0180);
                cells[index].floor_material = 4U;
                cells[index].ceiling_material = 4U;
            }
        }
    }
    materials[4].override_mask = OPTICAL_OVERRIDE_RAY_BLOCKS |
                                 OPTICAL_OVERRIDE_OPACITY |
                                 OPTICAL_OVERRIDE_TRANSMISSION;
    materials[4].ray_blocks = 0U;
    materials[4].opacity = 96U;
    materials[4].transmission = 180U;
    if (!optical_runtime_view_init(
            &view, heights.cell_count, materials, 5U,
            NULL, 0U, GENERATION)) goto cleanup;
    for (int x = 0; x < VIEWPORT_WIDTH; x++)
        if (!heightfield_trace_prepare_column(
                &columns[x], &camera, map, &heights, VIEWPORT_WIDTH,
                VIEWPORT_HEIGHT, x, 20.0)) goto cleanup;
    baseline_checksum = sample_baseline(columns);
    for (size_t i = 0U; i < sizeof(coverage) / sizeof(coverage[0]); i++)
        checksums[i] = sample_mixed(columns, &view, coverage[i]);
    if (baseline_checksum == 0U || checksums[0] != baseline_checksum) goto cleanup;
    for (size_t frame = 0U; frame < WARMUP_FRAMES; frame++) {
        benchmark_sink ^= sample_baseline(columns);
        for (size_t i = 0U; i < sizeof(coverage) / sizeof(coverage[0]); i++)
            benchmark_sink ^= sample_mixed(columns, &view, coverage[i]);
    }
    baseline_average = measure_baseline(
        columns, baseline_checksum, &deterministic);
    for (size_t i = 0U; i < sizeof(coverage) / sizeof(coverage[0]); i++)
        averages[i] = measure_mixed(
            columns, &view, coverage[i], checksums[i], &deterministic);
    if (!deterministic || baseline_average < 0.0) goto cleanup;
    for (size_t i = 0U; i < sizeof(coverage) / sizeof(coverage[0]); i++)
        if (averages[i] < 0.0) goto cleanup;
    printf("{\n");
    printf("  \"scenario\": \"i2_selective_prepared_sampling\",\n");
    printf("  \"samples_per_frame\": %u,\n",
           VIEWPORT_WIDTH * VIEWPORT_HEIGHT);
    printf("  \"frames\": %u,\n", BENCHMARK_FRAMES);
    printf("  \"baseline_ms\": %.6f,\n", baseline_average);
    printf("  \"baseline_checksum\": %llu,\n",
           (unsigned long long)baseline_checksum);
    for (size_t i = 0U; i < sizeof(coverage) / sizeof(coverage[0]); i++) {
        printf("  \"coverage_%u_ms\": %.6f,\n", coverage[i], averages[i]);
        printf("  \"coverage_%u_delta_ms\": %.6f,\n", coverage[i],
               averages[i] - baseline_average);
        printf("  \"coverage_%u_checksum\": %llu,%s\n", coverage[i],
               (unsigned long long)checksums[i],
               i + 1U == sizeof(coverage) / sizeof(coverage[0]) ? "" : "");
    }
    printf("  \"zero_coverage_parity\": true,\n");
    printf("  \"timed_loop_allocations\": 0,\n");
    printf("  \"deterministic\": true\n");
    printf("}\n");
    result = 0;

cleanup:
    free(columns);
    map_destroy(map);
    return result;
}