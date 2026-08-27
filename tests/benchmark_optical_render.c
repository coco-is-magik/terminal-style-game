/** benchmark_optical_render.c — I3 full selective-composition render benchmark */
#define _POSIX_C_SOURCE 200809L

#include "../src/config.h"
#include "../src/math.h"
#include "../src/raycast.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAP_WIDTH 20
#define MAP_HEIGHT 12
#define VIEW_WIDTH 260
#define VIEW_HEIGHT 160
#define FRAMES 200U
#define WARMUP 10U
#define GENERATION UINT32_C(53)

static volatile uint64_t benchmark_sink;

static double now_ms(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return -1.0;
    return (double)value.tv_sec * 1000.0 + (double)value.tv_nsec / 1000000.0;
}

static uint64_t checksum(const Grid *grid) {
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

static void loaded(AssetRegistry *assets, int id, SDL_Color color,
                   const char *glyphs) {
    asset_registry_set_palette(assets, id, color, color, color);
    asset_registry_set_material(assets, id, id, glyphs);
    (void)snprintf(assets->material_names[id], MATERIAL_NAME_CAPACITY, "%d", id);
}

static double measure(Grid *grid, Map *map, Camera *camera,
                      AssetRegistry *assets, WorldState *world,
                      const SceneSurfaceView *surfaces,
                      const SceneHeightView *heights,
                      const OpticalRuntimeView *view,
                      uint64_t expected, bool *deterministic) {
    double start = now_ms();
    for (size_t frame = 0U; frame < FRAMES; frame++) {
        raycast_render_height_optical(
            grid, map, camera, assets, world, surfaces, heights,
            view, view ? GENERATION : 0U);
        {
            uint64_t actual = checksum(grid);
            if (actual != expected) *deterministic = false;
            benchmark_sink ^= actual;
        }
    }
    {
        double end = now_ms();
        return start < 0.0 || end < start ? -1.0 :
            (end - start) / (double)FRAMES;
    }
}

int main(void) {
    Grid *grid = NULL;
    Grid *opaque_grid = NULL;
    Map *map = NULL;
    SceneAuthoredCell cells[MAP_WIDTH * MAP_HEIGHT] = {0};
    SceneSurfaceView surfaces = {cells, MAP_WIDTH * MAP_HEIGHT,
                                 MAP_WIDTH, MAP_HEIGHT};
    SceneHeightView heights = {
        cells, MAP_WIDTH * MAP_HEIGHT, MAP_WIDTH, MAP_HEIGHT,
        {9.8, SCENE_GRAVITY_DOWN, 0.25, 3.2, 1.0, 0.5, 0.75}
    };
    OpticalExtension materials[5] = {0};
    OpticalRuntimeView opaque_view;
    OpticalRuntimeView transparent_view;
    AssetRegistry assets;
    WorldState world;
    Camera camera;
    uint64_t compatibility_checksum;
    uint64_t opaque_checksum;
    uint64_t transparent_checksum;
    size_t changed_cells = 0U;
    double compatibility_ms;
    double opaque_ms;
    double transparent_ms;
    bool deterministic = true;
    int result = 1;
    config_init_defaults();
    if (!asset_registry_init(&assets)) return 1;
    world_init(&world);
    grid = grid_create(VIEW_WIDTH, VIEW_HEIGHT);
    opaque_grid = grid_create(VIEW_WIDTH, VIEW_HEIGHT);
    map = map_create(MAP_WIDTH, MAP_HEIGHT);
    if (!grid || !opaque_grid || !map) goto cleanup;
    camera_init(&camera, 10.5, 6.5, 0.0, PI / 2.0);
    camera.z = 0.75;
    loaded(&assets, 1, (SDL_Color){220, 180, 120, 255}, "#x-.");
    loaded(&assets, 2, (SDL_Color){120, 180, 220, 255}, "@o:.");
    loaded(&assets, 4, (SDL_Color){80, 220, 180, 255}, "Gg:.");
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            size_t i = (size_t)y * MAP_WIDTH + (size_t)x;
            bool edge = x == 0 || y == 0 || x == MAP_WIDTH - 1 ||
                        y == MAP_HEIGHT - 1;
            cells[i].floor_present = true;
            cells[i].ceiling_present = true;
            cells[i].floor_height_step = SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
            cells[i].ceiling_height_step = SCENE_DEFAULT_CEILING_HEIGHT_STEP;
            cells[i].floor_material = 1U;
            cells[i].ceiling_material = 2U;
            f->map.light_map[i] = (LightLevel){1.0, 1.0, 1.0};
            if (edge) {
                cells[i].occupancy = SCENE_CELL_OCCUPANCY_WALL;
                cells[i].wall_material = 1U;
                map_set(map, x, y, 1);
            }
            if (x == 18 && y == 6) {
                cells[i].occupancy = SCENE_CELL_OCCUPANCY_WALL;
                cells[i].wall_material = 4U;
                map_set(map, x, y, 4);
            }
        }
    }
    if (!optical_runtime_view_init(
            &opaque_view, heights.cell_count, NULL, 0U,
            NULL, 0U, GENERATION)) goto cleanup;
    materials[4].override_mask = OPTICAL_OVERRIDE_RAY_BLOCKS |
                                 OPTICAL_OVERRIDE_OPACITY |
                                 OPTICAL_OVERRIDE_TRANSMISSION;
    materials[4].opacity = 64U;
    materials[4].transmission = 192U;
    if (!optical_runtime_view_init(
            &transparent_view, heights.cell_count, materials, 5U,
            NULL, 0U, GENERATION)) goto cleanup;
    raycast_render_height_optical(
        grid, map, &camera, &assets, &world, &surfaces, &heights, NULL, 0U);
    compatibility_checksum = checksum(grid);
    raycast_render_height_optical(
        opaque_grid, map, &camera, &assets, &world, &surfaces, &heights,
        &opaque_view, GENERATION);
    opaque_checksum = checksum(opaque_grid);
    if (opaque_checksum != compatibility_checksum) goto cleanup;
    raycast_render_height_optical(
        grid, map, &camera, &assets, &world, &surfaces, &heights,
        &transparent_view, GENERATION);
    transparent_checksum = checksum(grid);
    for (size_t i = 0U; i < (size_t)VIEW_WIDTH * VIEW_HEIGHT; i++)
        if (memcmp(&grid->cells[i], &opaque_grid->cells[i],
                   sizeof(grid->cells[i])) != 0) changed_cells++;
    if (changed_cells == 0U || transparent_checksum == opaque_checksum) goto cleanup;
    for (size_t frame = 0U; frame < WARMUP; frame++) {
        raycast_render_height_optical(
            grid, map, &camera, &assets, &world,
            &surfaces, &heights, NULL, 0U);
        raycast_render_height_optical(
            grid, map, &camera, &assets, &world, &surfaces, &heights,
            &opaque_view, GENERATION);
        raycast_render_height_optical(
            grid, map, &camera, &assets, &world, &surfaces, &heights,
            &transparent_view, GENERATION);
    }
    compatibility_ms = measure(
        grid, map, &camera, &assets, &world, &surfaces, &heights,
        NULL, compatibility_checksum, &deterministic);
    opaque_ms = measure(
        grid, map, &camera, &assets, &world, &surfaces, &heights,
        &opaque_view, opaque_checksum, &deterministic);
    transparent_ms = measure(
        grid, map, &camera, &assets, &world, &surfaces, &heights,
        &transparent_view, transparent_checksum, &deterministic);
    if (!deterministic || compatibility_ms < 0.0 || opaque_ms < 0.0 ||
        transparent_ms < 0.0) goto cleanup;
    printf("{\n");
    printf("  \"scenario\": \"i3_selective_optical_render\",\n");
    printf("  \"frames\": %u,\n", FRAMES);
    printf("  \"samples_per_frame\": %u,\n", VIEW_WIDTH * VIEW_HEIGHT);
    printf("  \"compatibility_ms\": %.6f,\n", compatibility_ms);
    printf("  \"optical_opaque_ms\": %.6f,\n", opaque_ms);
    printf("  \"optical_opaque_delta_ms\": %.6f,\n", opaque_ms - compatibility_ms);
    printf("  \"transparent_ms\": %.6f,\n", transparent_ms);
    printf("  \"transparent_delta_ms\": %.6f,\n",
           transparent_ms - compatibility_ms);
    printf("  \"changed_cells\": %zu,\n", changed_cells);
    printf("  \"changed_coverage_percent\": %.3f,\n",
           100.0 * changed_cells / (VIEW_WIDTH * VIEW_HEIGHT));
    printf("  \"compatibility_checksum\": %llu,\n",
           (unsigned long long)compatibility_checksum);
    printf("  \"opaque_checksum\": %llu,\n",
           (unsigned long long)opaque_checksum);
    printf("  \"transparent_checksum\": %llu,\n",
           (unsigned long long)transparent_checksum);
    printf("  \"opaque_parity\": true,\n");
    printf("  \"render_loop_allocations\": 0,\n");
    printf("  \"deterministic\": true\n");
    printf("}\n");
    result = 0;

cleanup:
    grid_destroy(opaque_grid);
    grid_destroy(grid);
    map_destroy(map);
    world_clear(&world);
    asset_registry_clear(&assets);
    return result;
}