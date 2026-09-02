#define _POSIX_C_SOURCE 200809L

#include "../src/assets.h"
#include "../src/camera.h"
#include "../src/config.h"
#include "../src/grid.h"
#include "../src/map.h"
#include "../src/math.h"
#include "../src/raycast.h"
#include "../src/world.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAP_WIDTH 20
#define MAP_HEIGHT 12
#define VIEW_WIDTH 160
#define VIEW_HEIGHT 90
#define ITERATIONS 200U
#define SPRITE_RENDER_PASS_MS 6.0

static double now_ms(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return -1.0;
    return (double)value.tv_sec * 1000.0 +
           (double)value.tv_nsec / 1000000.0;
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

static double measure(
    Grid *grid, Map *map, Camera *camera, AssetRegistry *assets,
    WorldState *world, const SceneSurfaceView *surfaces,
    const SceneHeightView *heights, uint64_t expected_checksum,
    bool *deterministic
) {
    double elapsed = 0.0;
    for (unsigned int i = 0U; i < ITERATIONS; i++) {
        double start = now_ms();
        double end;
        raycast_render_height_optical(
            grid, map, camera, assets, world, surfaces, heights, NULL, 0U);
        end = now_ms();
        if (start < 0.0 || end < start ||
            grid_checksum(grid) != expected_checksum) {
            *deterministic = false;
            return -1.0;
        }
        elapsed += end - start;
    }
    return elapsed / ITERATIONS;
}

int main(void) {
    Grid *grid = NULL;
    Map *map = NULL;
    AssetRegistry assets;
    WorldState world;
    Camera camera;
    SceneAuthoredCell cells[MAP_WIDTH * MAP_HEIGHT] = {0};
    SceneSurfaceView surfaces = {
        cells, MAP_WIDTH * MAP_HEIGHT, MAP_WIDTH, MAP_HEIGHT
    };
    SceneHeightView heights = {
        cells, MAP_WIDTH * MAP_HEIGHT, MAP_WIDTH, MAP_HEIGHT,
        {9.8, SCENE_GRAVITY_DOWN, 0.25, 3.0, 1.0, 0.5, 0.75}
    };
    PatternCell pattern[15] = {
        {' ', 1}, {'@', 1}, {' ', 1},
        {'/', 1}, {'#', 2}, {'\\', 1},
        {' ', 1}, {'#', 2}, {' ', 1},
        {'/', 1}, {'#', 2}, {'\\', 1},
        {'/', 1}, {' ', 1}, {'\\', 1}
    };
    uint64_t baseline_checksum;
    uint64_t sprite_checksum;
    double baseline_average;
    double sprite_average;
    bool deterministic = true;
    int result = 1;

    config_init_defaults();
    if (!asset_registry_init(&assets)) return 1;
    world_init(&world);
    grid = grid_create(VIEW_WIDTH, VIEW_HEIGHT);
    map = map_create(MAP_WIDTH, MAP_HEIGHT);
    if (!grid || !map) goto cleanup;
    camera_init(&camera, 2.5, 6.0, 0.0, PI / 2.0);
    camera.z = 0.75;
    asset_registry_set_palette(
        &assets, 1, (SDL_Color){220, 180, 120, 255},
        (SDL_Color){160, 120, 80, 255}, (SDL_Color){80, 60, 40, 255});
    asset_registry_set_palette(
        &assets, 2, (SDL_Color){80, 220, 160, 255},
        (SDL_Color){50, 160, 110, 255}, (SDL_Color){25, 80, 55, 255});
    asset_registry_set_material(&assets, 1, 1, "#x-.");
    asset_registry_set_material(&assets, 2, 2, "@o:.");
    (void)snprintf(assets.material_names[1], MATERIAL_NAME_CAPACITY, "1");
    (void)snprintf(assets.material_names[2], MATERIAL_NAME_CAPACITY, "2");
    assets.sprites[1].cols = 3;
    assets.sprites[1].rows = 5;
    assets.sprites[1].pattern = malloc(sizeof(pattern));
    if (!assets.sprites[1].pattern) goto cleanup;
    memcpy(assets.sprites[1].pattern, pattern, sizeof(pattern));

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            size_t index = (size_t)y * MAP_WIDTH + (size_t)x;
            bool edge = x == 0 || y == 0 || x == MAP_WIDTH - 1 ||
                        y == MAP_HEIGHT - 1;
            if (edge) map_set(map, x, y, 1);
            map->light_map[index] = (LightLevel){1.0, 0.8, 0.6};
            cells[index].floor_material = 1U;
            cells[index].ceiling_material = 2U;
            cells[index].floor_height_step = SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
            cells[index].ceiling_height_step = SCENE_DEFAULT_CEILING_HEIGHT_STEP;
            cells[index].floor_present = true;
            cells[index].ceiling_present = true;
            if (edge) {
                cells[index].occupancy = SCENE_CELL_OCCUPANCY_WALL;
                cells[index].wall_material = 1U;
            }
        }
    }

    raycast_render_height_optical(
        grid, map, &camera, &assets, &world, &surfaces, &heights, NULL, 0U);
    baseline_checksum = grid_checksum(grid);
    baseline_average = measure(
        grid, map, &camera, &assets, &world, &surfaces, &heights,
        baseline_checksum, &deterministic);
    if (baseline_average < 0.0) goto cleanup;

    for (int i = 0; i < MAX_SPRITES; i++) {
        double x = 4.0 + (i % 8) * 1.5;
        double y = 1.5 + ((i / 8) % 7) * 1.5;
        if (world_add_sprite(&world, x, y, 1) != WORLD_INSERT_OK) goto cleanup;
    }
    raycast_render_height_optical(
        grid, map, &camera, &assets, &world, &surfaces, &heights, NULL, 0U);
    sprite_checksum = grid_checksum(grid);
    if (sprite_checksum == baseline_checksum) goto cleanup;
    sprite_average = measure(
        grid, map, &camera, &assets, &world, &surfaces, &heights,
        sprite_checksum, &deterministic);
    if (sprite_average < 0.0) goto cleanup;

    printf("{\n");
    printf("  \"iterations_per_path\": %u,\n", ITERATIONS);
    printf("  \"sprite_count\": %d,\n", MAX_SPRITES);
    printf("  \"baseline_avg_ms\": %.6f,\n", baseline_average);
    printf("  \"sprites_avg_ms\": %.6f,\n", sprite_average);
    printf("  \"sprite_overhead_ms\": %.6f,\n",
           sprite_average - baseline_average);
    printf("  \"pass_budget_ms\": %.3f,\n", SPRITE_RENDER_PASS_MS);
    printf("  \"baseline_checksum\": %llu,\n",
           (unsigned long long)baseline_checksum);
    printf("  \"sprite_checksum\": %llu,\n",
           (unsigned long long)sprite_checksum);
    printf("  \"deterministic\": %s,\n", deterministic ? "true" : "false");
    printf("  \"result\": \"%s\"\n",
           deterministic && baseline_average <= SPRITE_RENDER_PASS_MS &&
               sprite_average <= SPRITE_RENDER_PASS_MS ? "pass" : "fail");
    printf("}\n");
    result = deterministic && baseline_average <= SPRITE_RENDER_PASS_MS &&
        sprite_average <= SPRITE_RENDER_PASS_MS ? 0 : 1;

cleanup:
    grid_destroy(grid);
    map_destroy(map);
    world_clear(&world);
    asset_registry_clear(&assets);
    return result;
}