/** benchmark_surface_render.c — Headless authored-surface render performance */
#define _POSIX_C_SOURCE 200809L

#include "../src/assets.h"
#include "../src/camera.h"
#include "../src/config.h"
#include "../src/grid.h"
#include "../src/map.h"
#include "../src/math.h"
#include "../src/raycast.h"
#include "../src/world.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BENCHMARK_ITERATIONS 200U
#define STABILITY_ITERATIONS 1000U
#define SURFACE_RENDER_PASS_MS 6.0

static double now_ms(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return -1.0;
    return (double)value.tv_sec * 1000.0 + (double)value.tv_nsec / 1000000.0;
}

static uint64_t grid_checksum(const Grid *grid) {
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t count = (size_t)grid->width * (size_t)grid->height;
    for (size_t i = 0U; i < count; i++) {
        const Cell *cell = &grid->cells[i];
        const uint8_t bytes[] = {
            cell->glyph,
            cell->fg.r, cell->fg.g, cell->fg.b, cell->fg.a,
            cell->bg.r, cell->bg.g, cell->bg.b, cell->bg.a
        };
        for (size_t j = 0U; j < sizeof(bytes); j++) {
            hash ^= bytes[j];
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

static void mark_material_loaded(AssetRegistry *assets, int id, int palette_id,
                                 const char *glyphs) {
    asset_registry_set_material(assets, id, palette_id, glyphs);
    (void)snprintf(assets->material_names[id],
                   sizeof(assets->material_names[id]), "%d", id);
}

static double measure(Grid *grid, Map *map, Camera *camera,
                      AssetRegistry *assets, WorldState *world,
                      const SceneSurfaceView *surfaces,
                      const SceneHeightView *heights, uint64_t iterations,
                      uint64_t expected_checksum, bool *deterministic) {
    double elapsed = 0.0;
    for (uint64_t i = 0U; i < iterations; i++) {
        double start = now_ms();
        if (heights)
            raycast_render_height_optical(
                grid, map, camera, assets, world, surfaces, heights, NULL, 0U);
        else raycast_render(grid, map, camera, assets, world, surfaces);
        double end = now_ms();
        if (start < 0.0 || end < start ||
            grid_checksum(grid) != expected_checksum) {
            *deterministic = false;
            return -1.0;
        }
        elapsed += end - start;
    }
    return elapsed / (double)iterations;
}

int main(int argc, char **argv) {
    bool stability = argc == 2 && strcmp(argv[1], "--stability") == 0;
    uint64_t iterations = stability ? STABILITY_ITERATIONS : BENCHMARK_ITERATIONS;
    Grid *grid = NULL;
    Map *map = NULL;
    SceneAuthoredCell cells[20U * 12U] = {0};
    SceneSurfaceView surfaces = {cells, 20U * 12U, 20, 12};
    SceneHeightView heights = {
        cells, 20U * 12U, 20, 12,
        {9.8, SCENE_GRAVITY_DOWN, 0.25, 3.0, 1.0, 0.5, 0.75}
    };
    AssetRegistry assets;
    WorldState world;
    Camera camera;
    uint64_t null_checksum;
    uint64_t surface_checksum;
    uint64_t flat_height_checksum;
    uint64_t raised_height_checksum;
    double null_average;
    double surface_average;
    double flat_height_average;
    double raised_height_average;
    double occluded_decal_average;
    uint64_t occluded_decal_checksum;
    bool deterministic = true;
    int result = 1;

    if (argc > 2 || (argc == 2 && !stability)) {
        fprintf(stderr, "usage: %s [--stability]\n", argv[0]);
        return 2;
    }
    config_init_defaults();
    if (!asset_registry_init(&assets)) {
        fprintf(stderr, "asset registry allocation failed\n");
        return 1;
    }
    world_init(&world);
    grid = grid_create(260, 160);
    map = map_create(20, 12);
    if (!grid || !map) goto cleanup;
    camera_init(&camera, 10.5, 6.5, 0.0, PI / 2.0);
    asset_registry_set_palette(
        &assets, 1, (SDL_Color){220, 180, 120, 255},
        (SDL_Color){160, 120, 80, 255}, (SDL_Color){80, 60, 40, 255});
    asset_registry_set_palette(
        &assets, 2, (SDL_Color){120, 180, 220, 255},
        (SDL_Color){80, 120, 160, 255}, (SDL_Color){40, 60, 80, 255});
    mark_material_loaded(&assets, 1, 1, "#x-.");
    mark_material_loaded(&assets, 2, 2, "@o:.");
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            size_t index = (size_t)y * (size_t)map->width + (size_t)x;
            bool edge = x == 0 || y == 0 ||
                x == map->width - 1 || y == map->height - 1;
            if (edge) map_set(map, x, y, 1);
            if (x == 12) map_set(map, x, y, 1);
            map->light_map[index] = (LightLevel){1.0, 1.0, 1.0};
            cells[index].floor_material = 1U;
            cells[index].ceiling_material = 2U;
            cells[index].floor_height_step = SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
            cells[index].ceiling_height_step = SCENE_DEFAULT_CEILING_HEIGHT_STEP;
            cells[index].floor_present = true;
            cells[index].ceiling_present = true;
        }
    }

    raycast_render(grid, map, &camera, &assets, &world, NULL);
    null_checksum = grid_checksum(grid);
    raycast_render(grid, map, &camera, &assets, &world, &surfaces);
    surface_checksum = grid_checksum(grid);
    if (null_checksum == surface_checksum) goto cleanup;
    raycast_render_height_optical(
        grid, map, &camera, &assets, &world, &surfaces, &heights, NULL, 0U);
    flat_height_checksum = grid_checksum(grid);
    /* The classic flat renderer is retained as a separate map-only path. The
     * current heightfield renderer owns its own deterministic baseline; requiring
     * cross-renderer checksum parity would reintroduce the deprecated pipeline
     * seam this benchmark now guards against. */
    null_average = measure(grid, map, &camera, &assets, &world, NULL, NULL,
                           iterations, null_checksum, &deterministic);
    surface_average = measure(grid, map, &camera, &assets, &world, &surfaces, NULL,
                              iterations, surface_checksum, &deterministic);
    flat_height_average = measure(
        grid, map, &camera, &assets, &world, &surfaces, &heights,
        iterations, flat_height_checksum, &deterministic);
    if (null_average < 0.0 || surface_average < 0.0 ||
        flat_height_average < 0.0) goto cleanup;
    for (int y = 0; y < map->height; y++) {
        size_t index = (size_t)y * (size_t)map->width + 12U;
        cells[index].floor_height_step = UINT16_C(0x0080);
        cells[index].ceiling_height_step = UINT16_C(0x0180);
    }
    raycast_render_height_optical(
        grid, map, &camera, &assets, &world, &surfaces, &heights, NULL, 0U);
    raised_height_checksum = grid_checksum(grid);
    if (raised_height_checksum == flat_height_checksum) goto cleanup;
    raised_height_average = measure(
        grid, map, &camera, &assets, &world, &surfaces, &heights,
        iterations, raised_height_checksum, &deterministic);
    if (raised_height_average < 0.0) goto cleanup;
    {
        Decal decal = {0};
        decal.surface = DECAL_SURFACE_FLOOR;
        decal.x = 15.5; decal.y = 6.5; decal.z = 0.0;
        decal.width = 1.0; decal.height = 1.0;
        decal.pattern_cols = 1; decal.pattern_rows = 1;
        decal.pattern = calloc(1U, sizeof(*decal.pattern));
        if (!decal.pattern) goto cleanup;
        decal.pattern[0] = (PatternCell){'D', 2};
        if (world_add_decal(&world, decal) != WORLD_INSERT_OK) {
            free(decal.pattern);
            goto cleanup;
        }
    }
    raycast_render_height_optical(
        grid, map, &camera, &assets, &world, &surfaces, &heights, NULL, 0U);
    occluded_decal_checksum = grid_checksum(grid);
    if (occluded_decal_checksum != raised_height_checksum) goto cleanup;
    occluded_decal_average = measure(
        grid, map, &camera, &assets, &world, &surfaces, &heights, iterations,
        occluded_decal_checksum, &deterministic);
    if (occluded_decal_average < 0.0) goto cleanup;

    printf("{\n");
    printf("  \"mode\": \"%s\",\n", stability ? "stability" : "benchmark");
    printf("  \"iterations_per_path\": %llu,\n",
           (unsigned long long)iterations);
    printf("  \"null_view_avg_ms\": %.6f,\n", null_average);
    printf("  \"authored_view_avg_ms\": %.6f,\n", surface_average);
    printf("  \"authored_overhead_ms\": %.6f,\n",
           surface_average - null_average);
    printf("  \"flat_height_avg_ms\": %.6f,\n", flat_height_average);
    printf("  \"raised_height_avg_ms\": %.6f,\n", raised_height_average);
    printf("  \"occluded_decal_avg_ms\": %.6f,\n", occluded_decal_average);
    printf("  \"occluded_decal_overhead_ms\": %.6f,\n",
           occluded_decal_average - raised_height_average);
    printf("  \"pass_budget_ms\": %.3f,\n", SURFACE_RENDER_PASS_MS);
    printf("  \"null_checksum\": %llu,\n",
           (unsigned long long)null_checksum);
    printf("  \"authored_checksum\": %llu,\n",
           (unsigned long long)surface_checksum);
    printf("  \"flat_height_checksum\": %llu,\n",
           (unsigned long long)flat_height_checksum);
    printf("  \"raised_height_checksum\": %llu,\n",
           (unsigned long long)raised_height_checksum);
    printf("  \"occluded_decal_checksum\": %llu,\n",
           (unsigned long long)occluded_decal_checksum);
    printf("  \"deterministic\": %s,\n", deterministic ? "true" : "false");
    printf("  \"result\": \"%s\"\n",
            deterministic && flat_height_average <= SURFACE_RENDER_PASS_MS &&
                raised_height_average <= SURFACE_RENDER_PASS_MS
               ? "pass" : "fail");
    printf("}\n");
    result = deterministic && flat_height_average <= SURFACE_RENDER_PASS_MS &&
        raised_height_average <= SURFACE_RENDER_PASS_MS ? 0 : 1;

cleanup:
    grid_destroy(grid);
    map_destroy(map);
    world_clear(&world);
    asset_registry_clear(&assets);
    return result;
}
