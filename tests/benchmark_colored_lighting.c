/** benchmark_colored_lighting.c — Headless R10 I1 lighting benchmark */
#define _POSIX_C_SOURCE 200809L

#include "../src/config.h"
#include "../src/lighting.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define BENCH_WIDTH 32
#define BENCH_HEIGHT 24
#define BENCH_ITERATIONS 200U
#define BENCH_PASS_MS 6.0

static double now_ms(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return -1.0;
    return (double)value.tv_sec * 1000.0 + (double)value.tv_nsec / 1000000.0;
}

static uint64_t checksum(const Map *map) {
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t count = (size_t)map->width * (size_t)map->height;
    for (size_t i = 0U; i < count; i++) {
        const unsigned char *bytes = (const unsigned char *)&map->light_map[i];
        for (size_t j = 0U; j < sizeof(map->light_map[i]); j++) {
            hash ^= bytes[j];
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

static bool setup_world(WorldState *world, bool colored, bool spots) {
    static const SDL_Color colors[] = {
        {255, 64, 32, 255}, {32, 255, 64, 192},
        {64, 32, 255, 128}, {255, 255, 64, 255}
    };
    world_init(world);
    world->has_authored_ambient = true;
    world->ambient_intensity = 0.1;
    for (int i = 0; i < 4; i++) {
        SDL_Color color = colored ? colors[i] : (SDL_Color){255, 255, 255, 255};
        WorldInsertResult result = spots
            ? world_add_spot_light(
                world, 5.5 + i * 7.0, 5.5 + (i % 2) * 12.0, color, 1.0, 8.0,
                (double)i * SCENE_LIGHT_DIRECTION_MAX / 4.0,
                SCENE_LIGHT_DIRECTION_MAX / 3.0, 1.5)
            : world_add_light(world, 5.5 + i * 7.0, 5.5 + (i % 2) * 12.0,
                              color, 1.0, 8.0);
        if (result != WORLD_INSERT_OK) return false;
    }
    return true;
}

static double measure(Map *map, WorldState *world, uint64_t *out_checksum) {
    double start = now_ms();
    for (size_t i = 0U; i < BENCH_ITERATIONS; i++)
        lighting_update_optical(map, world, NULL, 0U);
    double end = now_ms();
    if (start < 0.0 || end < start) return -1.0;
    *out_checksum = checksum(map);
    return (end - start) / (double)BENCH_ITERATIONS;
}

int main(void) {
    Map *map = map_create(BENCH_WIDTH, BENCH_HEIGHT);
    WorldState world;
    uint64_t white_first, white_second, colored_first, colored_second;
    uint64_t spot_first, spot_second;
    double white_ms, colored_ms, spot_ms;
    int status = 0;
    config_init_defaults();
    if (!map || !setup_world(&world, false, false)) return 2;
    white_ms = measure(map, &world, &white_first);
    (void)measure(map, &world, &white_second);
    world_clear(&world);
    if (!setup_world(&world, true, false)) status = 2;
    colored_ms = measure(map, &world, &colored_first);
    (void)measure(map, &world, &colored_second);
    world_clear(&world);
    if (!setup_world(&world, true, true)) status = 2;
    spot_ms = measure(map, &world, &spot_first);
    (void)measure(map, &world, &spot_second);
    if (white_first != white_second || colored_first != colored_second ||
        spot_first != spot_second || white_first == colored_first ||
        colored_first == spot_first || white_ms < 0.0 || colored_ms < 0.0 ||
        spot_ms < 0.0 || white_ms > BENCH_PASS_MS ||
        colored_ms > BENCH_PASS_MS || spot_ms > BENCH_PASS_MS) status = 1;
    printf("colored-lighting: white=%.3f ms colored=%.3f ms spot=%.3f ms "
           "white_checksum=%llu colored_checksum=%llu spot_checksum=%llu "
           "budget=%.1f ms %s\n", white_ms, colored_ms, spot_ms,
           (unsigned long long)white_first, (unsigned long long)colored_first,
           (unsigned long long)spot_first, BENCH_PASS_MS,
           status == 0 ? "PASS" : "FAIL");
    world_clear(&world);
    map_destroy(map);
    return status;
}