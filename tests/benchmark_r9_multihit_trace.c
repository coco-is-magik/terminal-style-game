/** benchmark_r9_multihit_trace.c — P2 prepared-column collection benchmark */
#define _POSIX_C_SOURCE 200809L

#include "../src/heightfield_trace.h"
#include "../src/math.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef R9_OPTICAL_RESEARCH
#error "P2 benchmark requires R9_OPTICAL_RESEARCH=1"
#endif

#define MAP_WIDTH 20
#define MAP_HEIGHT 12
#define VIEWPORT_WIDTH 260
#define VIEWPORT_HEIGHT 160
#define BENCHMARK_FRAMES 200U
#define WARMUP_FRAMES 10U

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
    hash = hash_byte(hash, (uint8_t)hit->kind);
    hash = hash_byte(hash, (uint8_t)(hit->material & UINT16_C(0x00ff)));
    hash = hash_byte(hash, (uint8_t)(hit->material >> 8U));
    hash = hash_byte(hash, (uint8_t)hit->map_x);
    hash = hash_byte(hash, (uint8_t)hit->map_y);
    for (size_t i = 0U; i < sizeof(distance_bits); i++)
        hash = hash_byte(hash, (uint8_t)(distance_bits >> (i * 8U)));
    return hash;
}

static uint64_t sample_single(const HeightfieldTraceColumn *columns) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (int x = 0; x < VIEWPORT_WIDTH; x++) {
        for (int y = 0; y < VIEWPORT_HEIGHT; y++) {
            HeightfieldHit hit = heightfield_trace_prepared_sample(&columns[x], y);
            hash = hash_byte(hash, hit.hit ? 1U : 0U);
            if (hit.hit) hash = hash_hit(hash, &hit);
        }
    }
    return hash;
}

static uint64_t sample_collect(const HeightfieldTraceColumn *columns,
                               size_t capacity) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (int x = 0; x < VIEWPORT_WIDTH; x++) {
        for (int y = 0; y < VIEWPORT_HEIGHT; y++) {
            R9HeightfieldHitList hits;
            if (!heightfield_trace_collect(&columns[x], y, capacity, &hits)) return 0U;
            hash = hash_byte(hash, (uint8_t)hits.count);
            for (size_t i = 0U; i < hits.count; i++)
                hash = hash_hit(hash, &hits.hits[i]);
        }
    }
    return hash;
}

static double measure_single(const HeightfieldTraceColumn *columns,
                             uint64_t expected, bool *deterministic) {
    double start = now_ms();
    for (size_t i = 0U; i < BENCHMARK_FRAMES; i++) {
        uint64_t checksum = sample_single(columns);
        if (checksum != expected) *deterministic = false;
        benchmark_sink ^= checksum;
    }
    {
        double end = now_ms();
        return start < 0.0 || end < start ? -1.0 :
            (end - start) / (double)BENCHMARK_FRAMES;
    }
}

static double measure_collect(const HeightfieldTraceColumn *columns,
                              size_t capacity, uint64_t expected,
                              bool *deterministic) {
    double start = now_ms();
    for (size_t i = 0U; i < BENCHMARK_FRAMES; i++) {
        uint64_t checksum = sample_collect(columns, capacity);
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
    Map *map = NULL;
    HeightfieldTraceColumn *columns = NULL;
    SceneAuthoredCell cells[MAP_WIDTH * MAP_HEIGHT] = {0};
    SceneHeightView heights = {
        cells, MAP_WIDTH * MAP_HEIGHT, MAP_WIDTH, MAP_HEIGHT,
        {9.8, SCENE_GRAVITY_DOWN, 0.25, 3.2, 1.0, 0.5, 0.75}
    };
    Camera camera;
    uint64_t checksums[R9_HEIGHTFIELD_MAX_HITS + 1U] = {0};
    double averages[R9_HEIGHTFIELD_MAX_HITS + 1U] = {0};
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
            }
        }
    }
    for (int x = 0; x < VIEWPORT_WIDTH; x++) {
        if (!heightfield_trace_prepare_column(
                &columns[x], &camera, map, &heights, VIEWPORT_WIDTH,
                VIEWPORT_HEIGHT, x, 20.0)) goto cleanup;
    }
    checksums[0] = sample_single(columns);
    for (size_t capacity = 1U; capacity <= R9_HEIGHTFIELD_MAX_HITS; capacity++)
        checksums[capacity] = sample_collect(columns, capacity);
    if (checksums[0] == 0U || checksums[1] != checksums[0]) goto cleanup;
    for (size_t frame = 0U; frame < WARMUP_FRAMES; frame++) {
        benchmark_sink ^= sample_single(columns);
        for (size_t capacity = 1U; capacity <= R9_HEIGHTFIELD_MAX_HITS; capacity++)
            benchmark_sink ^= sample_collect(columns, capacity);
    }
    averages[0] = measure_single(columns, checksums[0], &deterministic);
    for (size_t capacity = 1U; capacity <= R9_HEIGHTFIELD_MAX_HITS; capacity++)
        averages[capacity] = measure_collect(
            columns, capacity, checksums[capacity], &deterministic);
    if (!deterministic || averages[0] < 0.0) goto cleanup;
    for (size_t capacity = 1U; capacity <= R9_HEIGHTFIELD_MAX_HITS; capacity++)
        if (averages[capacity] < 0.0) goto cleanup;
    printf("{\n");
    printf("  \"scenario\": \"raised_height_prepared_sampling\",\n");
    printf("  \"scope\": \"sampling_only_columns_prepared_before_timing\",\n");
    printf("  \"warmup_frames\": %u,\n", WARMUP_FRAMES);
    printf("  \"frames\": %u,\n", BENCHMARK_FRAMES);
    printf("  \"samples_per_frame\": %u,\n",
           VIEWPORT_WIDTH * VIEWPORT_HEIGHT);
    printf("  \"single_hit_ms\": %.6f,\n", averages[0]);
    printf("  \"single_hit_ns_per_sample\": %.3f,\n",
           averages[0] * 1000000.0 / (VIEWPORT_WIDTH * VIEWPORT_HEIGHT));
    for (size_t capacity = 1U; capacity <= R9_HEIGHTFIELD_MAX_HITS; capacity++) {
        printf("  \"collect_%zu_ms\": %.6f,\n", capacity, averages[capacity]);
        printf("  \"collect_%zu_ns_per_sample\": %.3f,\n", capacity,
               averages[capacity] * 1000000.0 /
                   (VIEWPORT_WIDTH * VIEWPORT_HEIGHT));
        printf("  \"collect_%zu_ratio\": %.6f,\n", capacity,
               averages[capacity] / averages[0]);
        printf("  \"collect_%zu_checksum\": %llu,%s\n", capacity,
               (unsigned long long)checksums[capacity],
               capacity == R9_HEIGHTFIELD_MAX_HITS ? "" : "");
    }
    printf("  \"single_hit_checksum\": %llu,\n",
           (unsigned long long)checksums[0]);
    printf("  \"n1_parity\": true,\n");
    printf("  \"deterministic\": true\n");
    printf("}\n");
    result = 0;

cleanup:
    free(columns);
    map_destroy(map);
    return result;
}