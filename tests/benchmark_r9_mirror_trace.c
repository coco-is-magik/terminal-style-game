/** benchmark_r9_mirror_trace.c — P4 one-bounce coverage benchmark */
#define _POSIX_C_SOURCE 200809L

#include "../src/r9_mirror_trace.h"
#include "../src/math.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef R9_OPTICAL_RESEARCH
#error "P4 benchmark requires R9_OPTICAL_RESEARCH=1"
#endif

#define MAP_WIDTH 20
#define MAP_HEIGHT 12
#define VIEWPORT_WIDTH 260
#define VIEWPORT_HEIGHT 160
#define SAMPLE_COUNT (VIEWPORT_WIDTH * VIEWPORT_HEIGHT)
#define WARMUP_FRAMES 5U
#define BENCHMARK_FRAMES 100U

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

static uint64_t hash_sample(uint64_t hash, const R9MirrorSample *sample) {
    uint64_t distance_bits = 0U;
    memcpy(&distance_bits, &sample->reflected_hit.distance, sizeof(distance_bits));
    hash = hash_byte(hash, (uint8_t)sample->kind);
    hash = hash_byte(hash, (uint8_t)sample->reflected_hit.kind);
    hash = hash_byte(hash, (uint8_t)(sample->reflected_hit.material & 0xffU));
    hash = hash_byte(hash, (uint8_t)sample->reflected_hit.map_x);
    hash = hash_byte(hash, (uint8_t)sample->reflected_hit.map_y);
    for (size_t i = 0U; i < sizeof(distance_bits); i++)
        hash = hash_byte(hash, (uint8_t)(distance_bits >> (i * 8U)));
    return hash;
}

static uint64_t run_coverage(const HeightfieldTraceColumn *incoming,
                             const HeightfieldHit *mirror_hit,
                             const R9OpticalResolved *optical,
                             size_t mirror_samples) {
    R9MirrorSample baseline = {R9_MIRROR_RESULT_DARKNESS_FALLBACK, {0},
                               0.0, 0.0, 0U};
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0U; i < SAMPLE_COUNT; i++) {
        R9MirrorSample reflected;
        const R9MirrorSample *sample = &baseline;
        if (i < mirror_samples) {
            if (!r9_mirror_sample_once(incoming, mirror_hit, optical,
                                       VIEWPORT_HEIGHT / 2, 20.0,
                                       &reflected)) return 0U;
            sample = &reflected;
        }
        hash = hash_sample(hash, sample);
    }
    return hash;
}

static double measure(const HeightfieldTraceColumn *incoming,
                      const HeightfieldHit *mirror_hit,
                      const R9OpticalResolved *optical,
                      size_t mirror_samples, uint64_t expected,
                      bool *deterministic) {
    double start = now_ms();
    for (size_t frame = 0U; frame < BENCHMARK_FRAMES; frame++) {
        uint64_t checksum = run_coverage(
            incoming, mirror_hit, optical, mirror_samples);
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
    Map map;
    LightLevel light_map[MAP_WIDTH * MAP_HEIGHT] = {0};
    double light_map[MAP_WIDTH * MAP_HEIGHT] = {0.0};
    SceneAuthoredCell cells[MAP_WIDTH * MAP_HEIGHT] = {0};
    SceneHeightView heights = {
        cells, MAP_WIDTH * MAP_HEIGHT, MAP_WIDTH, MAP_HEIGHT,
        {9.8, SCENE_GRAVITY_DOWN, 0.25, 3.2, 1.0, 0.5, 0.75}
    };
    Camera camera;
    HeightfieldTraceColumn incoming;
    HeightfieldHit mirror_hit;
    R9OpticalResolved optical = {true, true, true, 255U, 0U, 255U};
    const unsigned int coverages[] = {0U, 10U, 25U, 50U, 100U};
    uint64_t checksums[5] = {0U};
    double averages[5] = {0.0};
    bool deterministic = true;
    map = (Map){MAP_WIDTH, MAP_HEIGHT, map_cells, light_map};
    camera.transform.pos = (Vec2){10.5, 6.5};
    camera.transform.angle = 0.0;
    camera.fov = PI / 2.0;
    camera.pitch = 0.0;
    camera.z = 0.5;
    for (size_t i = 0U; i < MAP_WIDTH * MAP_HEIGHT; i++) {
        cells[i].floor_present = true;
        cells[i].ceiling_present = true;
        cells[i].floor_height_step = SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
        cells[i].ceiling_height_step = SCENE_DEFAULT_CEILING_HEIGHT_STEP;
        cells[i].floor_material = 1U;
        cells[i].ceiling_material = 2U;
    }
    {
        size_t mirror_index = 6U * MAP_WIDTH + 12U;
        size_t reflected_index = 6U * MAP_WIDTH + 4U;
        cells[mirror_index].occupancy = SCENE_CELL_OCCUPANCY_WALL;
        cells[mirror_index].wall_material = 10U;
        map_cells[mirror_index].material_id = 10;
        cells[reflected_index].occupancy = SCENE_CELL_OCCUPANCY_WALL;
        cells[reflected_index].wall_material = 20U;
        map_cells[reflected_index].material_id = 20;
    }
    if (!heightfield_trace_prepare_column(
            &incoming, &camera, &map, &heights, 1, VIEWPORT_HEIGHT,
            0, 20.0)) return 1;
    mirror_hit = heightfield_trace_prepared_sample(
        &incoming, VIEWPORT_HEIGHT / 2);
    if (!mirror_hit.hit || mirror_hit.material != 10U) return 1;
    for (size_t i = 0U; i < 5U; i++) {
        size_t mirror_samples = (size_t)SAMPLE_COUNT * coverages[i] / 100U;
        checksums[i] = run_coverage(
            &incoming, &mirror_hit, &optical, mirror_samples);
        if (checksums[i] == 0U) return 1;
    }
    for (size_t frame = 0U; frame < WARMUP_FRAMES; frame++) {
        for (size_t i = 0U; i < 5U; i++) {
            size_t mirror_samples = (size_t)SAMPLE_COUNT * coverages[i] / 100U;
            benchmark_sink ^= run_coverage(
                &incoming, &mirror_hit, &optical, mirror_samples);
        }
    }
    for (size_t i = 0U; i < 5U; i++) {
        size_t mirror_samples = (size_t)SAMPLE_COUNT * coverages[i] / 100U;
        averages[i] = measure(&incoming, &mirror_hit, &optical,
                              mirror_samples, checksums[i], &deterministic);
        if (averages[i] < 0.0) return 1;
    }
    if (!deterministic) return 1;
    printf("{\n");
    printf("  \"scenario\": \"single_bounce_reflected_wall\",\n");
    printf("  \"warmup_frames\": %u,\n", WARMUP_FRAMES);
    printf("  \"frames\": %u,\n", BENCHMARK_FRAMES);
    printf("  \"samples_per_frame\": %u,\n", SAMPLE_COUNT);
    printf("  \"baseline_ms\": %.6f,\n", averages[0]);
    printf("  \"baseline_checksum\": %llu,\n",
           (unsigned long long)checksums[0]);
    for (size_t i = 1U; i < 5U; i++) {
        size_t mirror_samples = (size_t)SAMPLE_COUNT * coverages[i] / 100U;
        printf("  \"coverage_%u_ms\": %.6f,\n", coverages[i], averages[i]);
        printf("  \"coverage_%u_overhead_ms\": %.6f,\n",
               coverages[i], averages[i] - averages[0]);
        printf("  \"coverage_%u_ns_per_mirror_sample\": %.3f,\n",
               coverages[i],
               (averages[i] - averages[0]) * 1000000.0 / mirror_samples);
        printf("  \"coverage_%u_checksum\": %llu,\n", coverages[i],
               (unsigned long long)checksums[i]);
    }
    printf("  \"max_bounces\": %u,\n", R9_MIRROR_MAX_BOUNCES);
    printf("  \"deterministic\": true\n");
    printf("}\n");
    return 0;
}