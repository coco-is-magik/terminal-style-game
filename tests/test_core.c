#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>       /* FILE, fopen(), fputs(), fclose(), remove() */
#include <stdlib.h>
#include <string.h>
#include <math.h>        /* cos(), atan(), tan() */
#include <sys/stat.h>    /* mkdir() */
#include <unistd.h>      /* rmdir() */
#include <cmocka.h>

#include "../src/grid.h"
#include "../src/scale.h"
#include "../src/timing.h"
#include "../src/renderer.h"
#include "../src/math.h"
#include "../src/map.h"
#include "../src/camera.h"
#include "../src/raycast.h"
#include "../src/heightfield_trace.h"
#include "../src/lighting.h"
#include "../src/config.h"
#include "../src/checked_size.h"
#include "../src/map_loader.h"

static void lighting_update_current(Map *map, WorldState *world) {
    lighting_update_optical(map, world, NULL, 0U);
}

static void render_heightfield_current(
    Grid *grid, Map *map, Camera *camera, AssetRegistry *assets,
    WorldState *world, const SceneSurfaceView *surfaces,
    const SceneHeightView *heights
) {
    raycast_render_height_optical(
        grid, map, camera, assets, world, surfaces, heights, NULL, 0U);
}

// --- GRID TESTS ---

static void test_grid_init(void **state) {
    (void)state;
    Grid *g = grid_create(10, 10);
    assert_non_null(g);
    assert_int_equal(g->width, 10);
    assert_int_equal(g->height, 10);
    assert_non_null(g->column_depths);
    assert_non_null(g->world_depths);
    assert_non_null(g->world_hit_keys);
    grid_destroy(g);
}

static void test_grid_init_invalid(void **state) {
    (void)state;
    Grid *g1 = grid_create(0, 10);
    assert_null(g1);
    Grid *g2 = grid_create(10, -5);
    assert_null(g2);
}

static void test_grid_destroy_safety(void **state) {
    (void)state;
    grid_destroy(NULL);
}

static void test_grid_set_get_bounds(void **state) {
    (void)state;
    Grid *g = grid_create(5, 5);
    SDL_Color fg = {255, 0, 0, 255};
    SDL_Color bg = {0, 255, 0, 255};
    
    assert_true(grid_set(g, 2, 2, 'A', fg, bg));
    
    Cell c;
    assert_true(grid_get(g, 2, 2, &c));
    assert_int_equal(c.glyph, 'A');
    assert_int_equal(c.fg.r, 255);
    assert_int_equal(c.bg.g, 255);
    
    assert_false(grid_set(g, -1, 0, 'X', fg, bg));
    assert_false(grid_set(g, 5, 0, 'X', fg, bg));
    
    assert_false(grid_get(g, -1, 0, &c));
    
    grid_destroy(g);
}

static void test_grid_clear(void **state) {
    (void)state;
    Grid *g = grid_create(3, 3);
    SDL_Color fg = {1, 1, 1, 255};
    SDL_Color bg = {2, 2, 2, 255};
    grid_set(g, 0, 0, 'X', fg, fg);
    
    grid_clear(g, bg);
    
    Cell c;
    grid_get(g, 0, 0, &c);
    assert_int_equal(c.glyph, ' ');
    assert_int_equal(c.bg.r, 2);
    
    grid_destroy(g);
}

static void test_grid_clear_region_zero(void **state) {
    Grid *g = grid_create(5, 4);
    SDL_Color color = {1, 2, 3, 4};
    Cell c;
    (void)state;
    assert_non_null(g);
    for (int y = 0; y < g->height; y++) {
        for (int x = 0; x < g->width; x++) {
            assert_true(grid_set(g, x, y, 'X', color, color));
        }
    }
    assert_true(grid_clear_region_zero(g, 1, 1, 3, 2));
    assert_true(grid_get(g, 0, 0, &c));
    assert_int_equal(c.glyph, 'X');
    assert_true(grid_get(g, 1, 1, &c));
    assert_int_equal(c.glyph, 0);
    assert_true(grid_get(g, 3, 2, &c));
    assert_int_equal(c.glyph, 0);
    assert_true(grid_get(g, 4, 3, &c));
    assert_int_equal(c.glyph, 'X');
    assert_false(grid_clear_region_zero(g, 4, 3, 2, 1));
    grid_destroy(g);
}

// --- SCALE TESTS ---

static void test_scale_calculations(void **state) {
    (void)state;
    ScaleResult r1 = scale_calculate(1920, 1080, 240, 160, 8, 8);
    assert_true(r1.valid);
    assert_int_equal(r1.scale_factor, 1);
}

static void test_scale_invalid(void **state) {
    (void)state;
    ScaleResult r = scale_calculate(0, 720, 80, 40, 8, 8);
    assert_false(r.valid);
}

// --- TIMING TESTS ---

#define DOUBLE_EPSILON 0.0001
static void test_timing_math(void **state) {
    (void)state;
    assert_float_equal(timing_target_ms(120), 8.333333, DOUBLE_EPSILON);
    assert_float_equal(timing_spare_ms(5.0, 16.666), 11.666, DOUBLE_EPSILON);
}

static void test_perf_stats(void **state) {
    (void)state;
    PerfStats stats;
    perf_stats_init(&stats);
    
    for (int i = 0; i < 10; i++) {
        perf_stats_update(&stats, 8.0, 8.0, 0.33);
    }
    
    assert_float_equal(stats.elapsed_time_ms, 80.0, DOUBLE_EPSILON);
    
    perf_stats_update(&stats, 920.0, 920.0, -910.0);
    assert_float_equal(stats.pub_avg_fps, 11.0, DOUBLE_EPSILON);
}

// --- RENDERER BACKEND TESTS ---

static void test_renderer_backend_init_invalid(void **state) {
    (void)state;
    Renderer *ren = renderer_create(1280, 720, -1, 160, 8, 8);
    assert_null(ren);
}

static void test_renderer_backend_instrumentation(void **state) {
    (void)state;
    renderer_destroy(NULL);
}

static void test_renderer_restore_merge_no_overlap(void **state) {
    uint32_t dirty[8] = {1U, 3U};
    uint8_t restore[8] = {0U, 0U, 1U, 0U, 0U, 1U, 0U, 0U};
    size_t count;
    (void)state;

    count = renderer_merge_restore_indices(dirty, 2U, 8U, restore, 8U);
    assert_int_equal(count, 4U);
    assert_int_equal(dirty[0], 1U);
    assert_int_equal(dirty[1], 3U);
    assert_int_equal(dirty[2], 2U);
    assert_int_equal(dirty[3], 5U);
}

static void test_renderer_restore_merge_overlap_and_capacity(void **state) {
    uint32_t dirty[5] = {1U, 3U, 4U, 0U, 0U};
    uint8_t restore[8] = {1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U};
    size_t count;
    (void)state;

    count = renderer_merge_restore_indices(dirty, 3U, 5U, restore, 8U);
    assert_int_equal(count, 5U);
    assert_int_equal(dirty[0], 1U);
    assert_int_equal(dirty[1], 3U);
    assert_int_equal(dirty[2], 4U);
    assert_int_equal(dirty[3], 0U);
    assert_int_equal(dirty[4], 2U);
    assert_int_equal(restore[1], 0U);
    assert_int_equal(restore[3], 0U);
    assert_int_equal(restore[4], 0U);
}

static void test_renderer_restore_merge_ignores_invalid_dirty_index(void **state) {
    uint32_t dirty[4] = {99U};
    uint8_t restore[4] = {1U, 0U, 0U, 1U};
    size_t count;
    (void)state;
    count = renderer_merge_restore_indices(dirty, 1U, 4U, restore, 4U);
    assert_int_equal(count, 3U);
    assert_int_equal(dirty[0], 99U);
    assert_int_equal(dirty[1], 0U);
    assert_int_equal(dirty[2], 3U);
}

static void test_checked_size_boundaries(void **state) {
    (void)state;
    size_t result = 99;

    assert_true(checked_size_2d(1, 1, &result));
    assert_int_equal(result, 1);
    assert_true(checked_size_2d(INT_MAX, INT_MAX, &result));
    assert_int_equal(result, (size_t)INT_MAX * (size_t)INT_MAX);
    assert_false(checked_size_2d(0, 1, &result));
    assert_false(checked_size_2d(1, -1, &result));
    assert_false(checked_size_2d(1, 1, NULL));

    assert_true(checked_size_bytes(SIZE_MAX, 1, &result));
    assert_int_equal(result, SIZE_MAX);
    assert_false(checked_size_bytes(SIZE_MAX, 2, &result));
    assert_false(checked_size_bytes(0, 1, &result));
    assert_false(checked_size_bytes(1, 0, &result));
    assert_false(checked_size_bytes(1, 1, NULL));
}

static void test_renderer_preflight_boundaries(void **state) {
    (void)state;
    int logical_w = -1;
    int logical_h = -1;
    size_t bytes = 0;

    assert_true(renderer_preflight(640, 480, 1, 1, 8, 8,
                                   &logical_w, &logical_h, &bytes));
    assert_int_equal(logical_w, 8);
    assert_int_equal(logical_h, 8);
    assert_int_equal(bytes, 8U * 8U * sizeof(uint32_t));
    assert_false(renderer_preflight(640, 480, 1, 1, 1, 8,
                                    &logical_w, &logical_h, &bytes));
    assert_false(renderer_preflight(640, 480, 1, 1, 8, 1,
                                    &logical_w, &logical_h, &bytes));
    assert_false(renderer_preflight(640, 480, INT_MAX, 1, 8, 8,
                                    &logical_w, &logical_h, &bytes));
    assert_false(renderer_preflight(640, 480, 1, INT_MAX, 8, 8,
                                    &logical_w, &logical_h, &bytes));
    assert_false(renderer_preflight(0, 480, 1, 1, 8, 8,
                                    &logical_w, &logical_h, &bytes));
    assert_false(renderer_preflight(640, 480, 1, 1, 8, 8,
                                    NULL, &logical_h, &bytes));
}

// --- ENGINE REFACTOR TESTS ---

#include "../src/asset_loader.h"

static void test_asset_loader(void **state) {
    (void)state;
    AssetRegistry assets;
    assert_true(asset_registry_init(&assets));
    
    // Test loading palettes and materials
    asset_loader_load_registry(&assets, "assets");
    
    // Verify palette 1 was loaded
    SDL_Color near_col = palette_sample(
        &assets.palettes[1], 1.0, (LightLevel){1.0, 1.0, 1.0});
    assert_int_equal(near_col.r, 255);
    
    // Verify material 1 was loaded
    assert_int_equal(assets.materials[1].palette_id, 1);
    assert_int_equal(assets.materials[1].glyphs[0], '#');
    
    WorldState world;
    world_init(&world);
    
    Map *map = asset_loader_load_map_data(&world, "assets", 1);
    assert_non_null(map);
    assert_int_equal(map->width, 10);
    assert_int_equal(map->height, 6);
    
    // Verify decals and lights
    assert_true(world.num_decals > 0);
    assert_true(world.num_lights > 0);
    
    world_clear(&world);
    map_destroy(map);
    asset_registry_clear(&assets);
}

static unsigned long grid_region_luminance(const Grid *grid, int y0, int y1) {
    unsigned long total = 0UL;
    for (int y = y0; y < y1; y++) {
        for (int x = 0; x < grid->width; x++) {
            const Cell *cell = &grid->cells[y * grid->width + x];
            total += cell->fg.r + cell->fg.g + cell->fg.b;
            total += cell->bg.r + cell->bg.g + cell->bg.b;
        }
    }
    return total;
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

static void prepare_surface_baseline(
    Grid **out_grid,
    Map **out_map,
    Camera *camera,
    AssetRegistry *assets,
    WorldState *world,
    int map_width,
    int map_height,
    double pitch
) {
    size_t count;
    *out_grid = grid_create(11, 9);
    *out_map = map_create(map_width, map_height);
    assert_non_null(*out_grid);
    assert_non_null(*out_map);
    camera_init(camera, map_width / 2.0, map_height / 2.0, 0.0, PI / 2.0);
    camera->pitch = pitch;
    assert_true(asset_registry_init(assets));
    world_init(world);
    count = (size_t)map_width * (size_t)map_height;
    for (size_t i = 0U; i < count; i++)
        (*out_map)->light_map[i] = (LightLevel){1.0, 1.0, 1.0};
    raycast_render(*out_grid, *out_map, camera, assets, world, NULL);
}

static void assert_surface_cell(
    const Grid *grid,
    int x,
    int y,
    uint8_t background
) {
    Cell cell;
    assert_true(grid_get((Grid *)grid, x, y, &cell));
    assert_int_equal(cell.glyph, ' ');
    assert_int_equal(cell.bg.r, background);
    assert_int_equal(cell.bg.g, background);
    assert_int_equal(cell.bg.b, background);
    assert_int_equal(cell.bg.a, 255U);
}

static void mark_test_material_loaded(
    AssetRegistry *assets,
    int id,
    int palette_id,
    const char *glyphs
) {
    asset_registry_set_material(assets, id, palette_id, glyphs);
    assert_true(snprintf(assets->material_names[id],
                         sizeof(assets->material_names[id]), "%d", id) > 0);
}

static void test_horizontal_surface_constant_baseline_level(void **state) {
    Grid *grid;
    Map *map;
    Camera camera;
    AssetRegistry assets;
    WorldState world;
    (void)state;
    prepare_surface_baseline(&grid, &map, &camera, &assets, &world, 9, 9, 0.0);
    assert_surface_cell(grid, 5, 0, 50U);
    assert_surface_cell(grid, 5, 8, 30U);
    assert_int_equal(grid_checksum(grid), UINT64_C(5221556747871135785));
    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(map);
    grid_destroy(grid);
}

static void test_horizontal_surface_constant_baseline_negative_pitch(void **state) {
    Grid *grid;
    Map *map;
    Camera camera;
    AssetRegistry assets;
    WorldState world;
    (void)state;
    prepare_surface_baseline(&grid, &map, &camera, &assets, &world, 9, 9, -9.0);
    assert_surface_cell(grid, 5, 0, 30U);
    assert_surface_cell(grid, 5, 8, 30U);
    assert_int_equal(grid_checksum(grid), UINT64_C(15900799000682171264));
    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(map);
    grid_destroy(grid);
}

static void test_horizontal_surface_constant_baseline_positive_pitch(void **state) {
    Grid *grid;
    Map *map;
    Camera camera;
    AssetRegistry assets;
    WorldState world;
    (void)state;
    prepare_surface_baseline(&grid, &map, &camera, &assets, &world, 9, 9, 9.0);
    assert_surface_cell(grid, 5, 0, 50U);
    assert_surface_cell(grid, 5, 8, 50U);
    assert_int_equal(grid_checksum(grid), UINT64_C(17892659974333048492));
    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(map);
    grid_destroy(grid);
}

static void test_horizontal_surface_constant_baseline_out_of_bounds(void **state) {
    Grid *grid;
    Map *map;
    Camera camera;
    AssetRegistry assets;
    WorldState world;
    (void)state;
    prepare_surface_baseline(&grid, &map, &camera, &assets, &world, 1, 1, 0.0);
    map->light_map[0] = (LightLevel){0.0, 0.0, 0.0};
    raycast_render(grid, map, &camera, &assets, &world, NULL);
    assert_surface_cell(grid, 0, 0, 50U);
    assert_surface_cell(grid, 0, 8, 30U);
    assert_int_equal(grid_checksum(grid), UINT64_C(5221556747871135785));
    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(map);
    grid_destroy(grid);
}

static void test_authored_horizontal_materials_and_lighting(void **state) {
    Grid *grid = grid_create(11, 9);
    Map *map = map_create(9, 9);
    SceneAuthoredCell cells[81] = {0};
    SceneSurfaceView surfaces = {cells, 81U, 9, 9};
    Camera camera;
    AssetRegistry assets;
    WorldState world;
    Cell ceiling;
    Cell floor_cell;
    (void)state;
    assert_non_null(grid);
    assert_non_null(map);
    camera_init(&camera, 4.5, 4.5, 0.0, PI / 2.0);
    asset_registry_init(&assets);
    asset_registry_set_palette(
        &assets, 7, (SDL_Color){200, 20, 10, 255},
        (SDL_Color){180, 20, 10, 255}, (SDL_Color){160, 20, 10, 255});
    asset_registry_set_palette(
        &assets, 2, (SDL_Color){20, 200, 10, 255},
        (SDL_Color){20, 180, 10, 255}, (SDL_Color){20, 160, 10, 255});
    mark_test_material_loaded(&assets, 7, 7, "Ff-.");
    mark_test_material_loaded(&assets, 2, 2, "Cc-.");
    for (size_t i = 0U; i < 81U; i++) {
        cells[i].floor_material = 7U;
        cells[i].ceiling_material = 2U;
        map->light_map[i] = (LightLevel){0.5, 0.5, 0.5};
    }
    world_init(&world);

    raycast_render(grid, map, &camera, &assets, &world, &surfaces);
    assert_true(grid_get(grid, 5, 0, &ceiling));
    assert_true(grid_get(grid, 5, 8, &floor_cell));
    assert_int_equal(ceiling.glyph, 'C');
    assert_int_equal(ceiling.fg.r, 10U);
    assert_int_equal(ceiling.fg.g, 100U);
    assert_int_equal(ceiling.fg.b, 5U);
    assert_int_equal(ceiling.bg.r, 0U);
    assert_int_equal(floor_cell.glyph, 'F');
    assert_int_equal(floor_cell.fg.r, 100U);
    assert_int_equal(floor_cell.fg.g, 10U);
    assert_int_equal(floor_cell.fg.b, 5U);
    assert_int_equal(floor_cell.bg.r, 0U);
    assert_int_equal(grid_checksum(grid), UINT64_C(5266099492592105087));

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(map);
    grid_destroy(grid);
}

static void test_missing_horizontal_material_is_obvious_and_unlit(void **state) {
    Grid *dark_grid = grid_create(11, 9);
    Grid *bright_grid = grid_create(11, 9);
    Map *map = map_create(9, 9);
    SceneAuthoredCell cells[81] = {0};
    SceneSurfaceView surfaces = {cells, 81U, 9, 9};
    Camera camera;
    AssetRegistry assets;
    WorldState world;
    Cell dark;
    Cell bright;
    (void)state;
    assert_non_null(dark_grid);
    assert_non_null(bright_grid);
    assert_non_null(map);
    camera_init(&camera, 4.5, 4.5, 0.0, PI / 2.0);
    asset_registry_init(&assets);
    world_init(&world);
    for (size_t i = 0U; i < 81U; i++) {
        cells[i].floor_material = 9U;
        cells[i].ceiling_material = 9U;
        map->light_map[i] = (LightLevel){0.0, 0.0, 0.0};
    }
    raycast_render(dark_grid, map, &camera, &assets, &world, &surfaces);
    for (size_t i = 0U; i < 81U; i++)
        map->light_map[i] = (LightLevel){1.0, 1.0, 1.0};
    raycast_render(bright_grid, map, &camera, &assets, &world, &surfaces);

    assert_true(grid_get(dark_grid, 5, 0, &dark));
    assert_true(grid_get(bright_grid, 5, 0, &bright));
    assert_memory_equal(&dark, &bright, sizeof(dark));
    assert_int_equal(dark.glyph, '.');
    assert_int_equal(dark.fg.r, 0U);
    assert_int_equal(dark.fg.g, 0U);
    assert_int_equal(dark.fg.b, 0U);
    assert_int_equal(dark.bg.r, 128U);
    assert_int_equal(dark.bg.g, 0U);
    assert_int_equal(dark.bg.b, 255U);
    assert_int_equal(dark.bg.a, 255U);
    assert_true(grid_get(dark_grid, 5, 8, &dark));
    assert_int_equal(dark.glyph, '.');
    assert_int_equal(dark.bg.r, 128U);
    assert_int_equal(dark.bg.b, 255U);

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(map);
    grid_destroy(dark_grid);
    grid_destroy(bright_grid);
}

static void test_invalid_surface_view_preserves_constant_backgrounds(void **state) {
    Grid *grid;
    Map *map;
    Camera camera;
    AssetRegistry assets;
    WorldState world;
    SceneAuthoredCell cell = {0};
    SceneSurfaceView mismatched = {&cell, 1U, 1, 1};
    (void)state;
    prepare_surface_baseline(&grid, &map, &camera, &assets, &world, 9, 9, 0.0);
    raycast_render(grid, map, &camera, &assets, &world, &mismatched);
    assert_surface_cell(grid, 5, 0, 50U);
    assert_surface_cell(grid, 5, 8, 30U);
    assert_int_equal(grid_checksum(grid), UINT64_C(5221556747871135785));
    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(map);
    grid_destroy(grid);
}

static void test_valid_surface_view_preserves_out_of_bounds_backgrounds(void **state) {
    Grid *grid;
    Map *map;
    Camera camera;
    AssetRegistry assets;
    WorldState world;
    SceneAuthoredCell cell = {
        .floor_material = 9U,
        .ceiling_material = 9U
    };
    SceneSurfaceView surfaces = {&cell, 1U, 1, 1};
    (void)state;
    prepare_surface_baseline(&grid, &map, &camera, &assets, &world, 1, 1, 0.0);
    map->light_map[0] = (LightLevel){0.0, 0.0, 0.0};
    raycast_render(grid, map, &camera, &assets, &world, &surfaces);
    assert_surface_cell(grid, 0, 0, 50U);
    assert_surface_cell(grid, 0, 8, 30U);
    assert_int_equal(grid_checksum(grid), UINT64_C(5221556747871135785));
    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(map);
    grid_destroy(grid);
}

static void test_height_view_flat_parity_and_authored_difference(void **state) {
    Grid *legacy = grid_create(41, 25);
    Grid *height = grid_create(41, 25);
    Map *map = map_create(9, 9);
    SceneAuthoredCell cells[81] = {0};
    SceneSurfaceView surfaces = {cells, 81U, 9, 9};
    SceneHeightView heights = {
        cells, 81U, 9, 9, {9.8, SCENE_GRAVITY_DOWN, 0.25, 3.0, 1.0, 0.5, 0.75}
    };
    Camera camera;
    AssetRegistry assets;
    WorldState world;
    uint64_t flat_checksum;
    (void)state;
    assert_non_null(legacy);
    assert_non_null(height);
    assert_non_null(map);
    camera_init(&camera, 4.5, 4.5, 0.0, PI / 2.0);
    assert_true(asset_registry_init(&assets));
    world_init(&world);
    mark_test_material_loaded(&assets, 1, 1, "#x:.");
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            size_t index = (size_t)y * (size_t)map->width + (size_t)x;
            bool wall = x == 0 || y == 0 || x == map->width - 1 ||
                        y == map->height - 1 || x == 7;
            map_set(map, x, y, wall ? 1 : 0);
            map->light_map[index] = (LightLevel){1.0, 1.0, 1.0};
            cells[index].occupancy = wall ? SCENE_CELL_OCCUPANCY_WALL
                                          : SCENE_CELL_OCCUPANCY_EMPTY;
            cells[index].wall_material = wall ? 1U : 0U;
            cells[index].floor_material = 1U;
            cells[index].ceiling_material = 1U;
            cells[index].floor_height_step = SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
            cells[index].ceiling_height_step = SCENE_DEFAULT_CEILING_HEIGHT_STEP;
            cells[index].floor_present = true;
            cells[index].ceiling_present = true;
        }
    }

    raycast_render(legacy, map, &camera, &assets, &world, &surfaces);
    render_heightfield_current(height, map, &camera, &assets, &world, &surfaces, &heights);
    flat_checksum = grid_checksum(legacy);
    assert_int_equal(grid_checksum(height), flat_checksum);
    assert_memory_equal(height->cells, legacy->cells,
                        (size_t)height->width * (size_t)height->height *
                            sizeof(*height->cells));

    camera.z = 0.75;
    render_heightfield_current(height, map, &camera, &assets, &world, &surfaces, &heights);
    {
        uint64_t airborne_flat_checksum = grid_checksum(height);
        render_heightfield_current(height, map, &camera, &assets, &world, &surfaces,
                              &heights);
        assert_int_equal(grid_checksum(height), airborne_flat_checksum);
    }
    camera.z = 0.5;

    for (int y = 0; y < map->height; y++) {
        size_t index = (size_t)y * (size_t)map->width + 5U;
        cells[index].floor_height_step = UINT16_C(0x0040);
        cells[index].ceiling_height_step = UINT16_C(0x0140);
    }
    assert_false(heightfield_view_is_flat_default(&heights, map->width, map->height));
    render_heightfield_current(height, map, &camera, &assets, &world, &surfaces, &heights);
    {
        uint64_t raised_checksum = grid_checksum(height);
        render_heightfield_current(height, map, &camera, &assets, &world, &surfaces,
                              &heights);
        assert_int_equal(grid_checksum(height), raised_checksum);
        camera.z = 0.75;
        render_heightfield_current(height, map, &camera, &assets, &world, &surfaces,
                              &heights);
        {
            uint64_t airborne_checksum = grid_checksum(height);
            render_heightfield_current(height, map, &camera, &assets, &world,
                                  &surfaces, &heights);
            assert_int_equal(grid_checksum(height), airborne_checksum);
        }
    }

    {
        SceneHeightView invalid = heights;
        invalid.cell_count--;
        camera.z = 0.5;
        render_heightfield_current(height, map, &camera, &assets, &world, &surfaces,
                              &invalid);
        raycast_render(legacy, map, &camera, &assets, &world, &surfaces);
        assert_memory_equal(height->cells, legacy->cells,
                            (size_t)height->width * (size_t)height->height *
                                sizeof(*height->cells));
    }

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(map);
    grid_destroy(height);
    grid_destroy(legacy);
}

static void test_horizontal_decal_follows_authored_floor_height(void **state) {
    Grid *grid = grid_create(81, 45);
    Map *map = map_create(9, 9);
    SceneAuthoredCell cells[81] = {0};
    SceneSurfaceView surfaces = {cells, 81U, 9, 9};
    SceneHeightView heights = {
        cells, 81U, 9, 9, scene_movement_parameters_default()
    };
    Camera camera;
    AssetRegistry assets;
    WorldState world;
    Decal decal = {0};
    int before_x = -1;
    int before_y = -1;
    int after_x = -1;
    int after_y = -1;
    size_t decal_index = 4U * 9U + 6U;
    (void)state;
    assert_non_null(grid);
    assert_non_null(map);
    assert_true(asset_registry_init(&assets));
    world_init(&world);
    camera_init(&camera, 4.5, 4.5, 0.0, PI / 2.0);
    camera.pitch = -8.0;
    mark_test_material_loaded(&assets, 1, 1, "#x:.");
    for (int y = 0; y < 9; y++) {
        for (int x = 0; x < 9; x++) {
            size_t index = (size_t)y * 9U + (size_t)x;
            bool wall = x == 0 || y == 0 || x == 8 || y == 8;
            map_set(map, x, y, wall ? 1 : 0);
            map->light_map[index] = (LightLevel){1.0, 1.0, 1.0};
            cells[index].occupancy = wall
                ? SCENE_CELL_OCCUPANCY_WALL : SCENE_CELL_OCCUPANCY_EMPTY;
            cells[index].wall_material = wall ? 1U : 0U;
            cells[index].floor_material = 1U;
            cells[index].ceiling_material = 1U;
            cells[index].floor_height_step = SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
            cells[index].ceiling_height_step = SCENE_DEFAULT_CEILING_HEIGHT_STEP;
            cells[index].floor_present = true;
            cells[index].ceiling_present = true;
        }
    }
    decal.surface = DECAL_SURFACE_FLOOR;
    decal.x = 6.5;
    decal.y = 4.5;
    decal.z = 0.0;
    decal.width = 0.5;
    decal.height = 0.5;
    decal.depth = 0.1;
    decal.pattern_cols = 1;
    decal.pattern_rows = 1;
    decal.pattern = malloc(sizeof(*decal.pattern));
    assert_non_null(decal.pattern);
    decal.pattern[0] = (PatternCell){'D', 1U};
    assert_int_equal(world_add_decal(&world, decal), WORLD_INSERT_OK);
    render_heightfield_current(grid, map, &camera, &assets, &world, &surfaces, &heights);
    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            if (grid->cells[(size_t)y * (size_t)grid->width + (size_t)x].glyph == 'D') {
                before_x = x;
                before_y = y;
            }
        }
    }
    assert_true(before_x >= 0 && before_y >= 0);
    cells[decal_index].floor_height_step = INT16_C(0x0040);
    render_heightfield_current(grid, map, &camera, &assets, &world, &surfaces, &heights);
    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            if (grid->cells[(size_t)y * (size_t)grid->width + (size_t)x].glyph == 'D') {
                after_x = x;
                after_y = y;
            }
        }
    }
    assert_true(after_x >= 0 && after_y >= 0);
    assert_int_equal(after_x, before_x);
    assert_true(after_y < before_y);
    assert_float_equal(world.decals[0].z, 0.0, 0.000001);
    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(map);
    grid_destroy(grid);
}

static void test_rendered_point_light_affects_ceiling_walls_and_floor(void **state) {
    Grid *ambient_grid = grid_create(41, 25);
    Grid *lit_grid = grid_create(41, 25);
    Map *map = map_create(7, 7);
    Camera camera;
    AssetRegistry assets;
    WorldState world;
    SDL_Color white = {255, 255, 255, 255};
    unsigned long ambient_top, ambient_middle, ambient_bottom;
    (void)state;

    assert_non_null(ambient_grid);
    assert_non_null(lit_grid);
    assert_non_null(map);
    for (int x = 0; x < 7; x++) {
        map_set(map, x, 0, 1);
        map_set(map, x, 6, 1);
    }
    for (int y = 0; y < 7; y++) {
        map_set(map, 0, y, 1);
        map_set(map, 6, y, 1);
    }
    camera_init(&camera, 1.5, 3.5, 0.0, PI / 2.0);
    asset_registry_init(&assets);
    asset_registry_set_palette(&assets, 1,
        (SDL_Color){255, 255, 255, 255},
        (SDL_Color){150, 150, 150, 255},
        (SDL_Color){50, 50, 50, 255});
    asset_registry_set_material(&assets, 1, 1, "#x-.");
    world_init(&world);
    world.has_authored_ambient = true;
    world.ambient_intensity = 0.05;

    lighting_update_current(map, &world);
    raycast_render(ambient_grid, map, &camera, &assets, &world, NULL);
    ambient_top = grid_region_luminance(ambient_grid, 0, 8);
    ambient_middle = grid_region_luminance(ambient_grid, 8, 17);
    ambient_bottom = grid_region_luminance(ambient_grid, 17, 25);

    assert_int_equal(world_add_light(
        &world, 4.5, 3.5, white, 1.0, 4.0), WORLD_INSERT_OK);
    lighting_update_current(map, &world);
    raycast_render(lit_grid, map, &camera, &assets, &world, NULL);
    assert_true(grid_region_luminance(lit_grid, 0, 8) > ambient_top);
    assert_true(grid_region_luminance(lit_grid, 8, 17) > ambient_middle);
    assert_true(grid_region_luminance(lit_grid, 17, 25) > ambient_bottom);

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(map);
    grid_destroy(ambient_grid);
    grid_destroy(lit_grid);
}

static void test_config_parsing(void **state) {
    (void)state;
    // We already called config_init_defaults() in main()
    // It should have grid_width 260
    assert_int_equal(config_get()->grid_width, 260);
    
    // Check loading the real config file
    assert_true(config_load_from_file("config.ini"));
    assert_int_equal(config_get()->target_fps, 120);
    assert_float_equal(config_get()->ambient_light, 0.2, DOUBLE_EPSILON);
}

static void test_config_transactional_valid_override(void **state) {
    (void)state;
    const char *path = "tests/tmp_config_valid.ini";
    EngineConfig saved = *config_get();
    FILE *file = fopen(path, "w");
    assert_non_null(file);
    assert_true(fputs("grid_width = 80\nambient_light = 0.35\ndebug_display_enabled = 0\n", file) >= 0);
    assert_int_equal(fclose(file), 0);

    assert_true(config_load_from_file(path));
    assert_int_equal(config_get()->grid_width, 80);
    assert_float_equal(config_get()->ambient_light, 0.35, DOUBLE_EPSILON);
    assert_false(config_get()->debug_display_enabled);

    config_set(&saved);
    assert_int_equal(remove(path), 0);
}

static void test_config_invalid_file_rolls_back(void **state) {
    (void)state;
    const char *path = "tests/tmp_config_invalid.ini";
    EngineConfig saved = *config_get();
    FILE *file = fopen(path, "w");
    assert_non_null(file);
    assert_true(fputs("grid_width = 80\ncell_width = 16\n", file) >= 0);
    assert_int_equal(fclose(file), 0);

    assert_false(config_load_from_file(path));
    assert_int_equal(config_get()->grid_width, saved.grid_width);
    assert_int_equal(config_get()->cell_width, saved.cell_width);

    assert_int_equal(remove(path), 0);
}

static void test_config_rejects_malformed_numbers(void **state) {
    (void)state;
    const char *path = "tests/tmp_config_malformed.ini";
    EngineConfig saved = *config_get();
    FILE *file = fopen(path, "w");
    assert_non_null(file);
    assert_true(fputs("target_fps = 120fps\n", file) >= 0);
    assert_int_equal(fclose(file), 0);

    assert_false(config_load_from_file(path));
    assert_int_equal(config_get()->target_fps, saved.target_fps);

    assert_int_equal(remove(path), 0);
}

static void test_config_effective_validation(void **state) {
    (void)state;
    EngineConfig saved = *config_get();
    EngineConfig candidate = saved;

    assert_true(config_validate(&candidate));
    assert_false(config_validate(NULL));
    candidate.grid_width = 0;
    assert_false(config_set(&candidate));
    assert_int_equal(config_get()->grid_width, saved.grid_width);
    candidate = saved;
    candidate.ambient_light = NAN;
    assert_false(config_set(&candidate));
    assert_float_equal(config_get()->ambient_light, saved.ambient_light, DOUBLE_EPSILON);
    assert_true(config_set(&saved));
}

static void test_math_normalize(void **state) {
    (void)state;
    assert_float_equal(normalize_angle(0.0), 0.0, DOUBLE_EPSILON);
    assert_float_equal(normalize_angle(3 * PI), PI, DOUBLE_EPSILON);
    assert_float_equal(normalize_angle(-PI), PI, DOUBLE_EPSILON);
}

static void test_map_creation(void **state) {
    (void)state;
    Map *m = map_create(5, 5);
    assert_non_null(m);
    assert_int_equal(m->width, 5);
    assert_int_equal(m->height, 5);
    map_destroy(m);
}

static void test_map_bounds(void **state) {
    (void)state;
    Map *m = map_create(5, 5);
    assert_true(map_in_bounds(m, 0, 0));
    assert_true(map_in_bounds(m, 4, 4));
    assert_false(map_in_bounds(m, -1, 0));
    assert_false(map_in_bounds(m, 5, 5));
    map_destroy(m);
}

static void test_map_lookup(void **state) {
    (void)state;
    Map *m = map_create(5, 5);
    map_set(m, 2, 2, 1);
    assert_int_equal(map_get(m, 2, 2)->material_id, 1);
    assert_int_equal(map_get(m, 0, 0)->material_id, 0);
    map_destroy(m);
}

static void test_map_text_ragged_padding(void **state) {
    (void)state;
    Map *map = map_load_from_string("12\n3\n");
    assert_non_null(map);
    assert_int_equal(map->width, 2);
    assert_int_equal(map->height, 2);
    assert_int_equal(map_get(map, 0, 0)->material_id, 1);
    assert_int_equal(map_get(map, 1, 0)->material_id, 2);
    assert_int_equal(map_get(map, 0, 1)->material_id, 3);
    assert_int_equal(map_get(map, 1, 1)->material_id, 0);
    map_destroy(map);
}

static void test_map_text_dimension_limits(void **state) {
    (void)state;
    size_t max_size = (size_t)MAP_TEXT_MAX_HEIGHT *
                      ((size_t)MAP_TEXT_MAX_WIDTH + 1U) + 1U;
    char *text = malloc(max_size);
    assert_non_null(text);

    size_t offset = 0;
    for (int y = 0; y < MAP_TEXT_MAX_HEIGHT; y++) {
        memset(text + offset, '1', MAP_TEXT_MAX_WIDTH);
        offset += MAP_TEXT_MAX_WIDTH;
        text[offset++] = '\n';
    }
    text[offset] = '\0';
    Map *map = map_load_from_string(text);
    assert_non_null(map);
    assert_int_equal(map->width, MAP_TEXT_MAX_WIDTH);
    assert_int_equal(map->height, MAP_TEXT_MAX_HEIGHT);
    map_destroy(map);

    text[MAP_TEXT_MAX_WIDTH] = '1';
    text[MAP_TEXT_MAX_WIDTH + 1] = '\0';
    assert_null(map_load_from_string(text));

    offset = 0;
    for (int y = 0; y <= MAP_TEXT_MAX_HEIGHT; y++) {
        text[offset++] = '1';
        text[offset++] = '\n';
    }
    text[offset] = '\0';
    assert_null(map_load_from_string(text));
    free(text);
}

static void test_camera_init(void **state) {
    (void)state;
    Camera cam;
    camera_init(&cam, 2.5, 3.5, PI, PI/2);
    assert_float_equal(cam.transform.pos.x, 2.5, DOUBLE_EPSILON);
    assert_float_equal(cam.transform.pos.y, 3.5, DOUBLE_EPSILON);
    assert_float_equal(cam.transform.angle, PI, DOUBLE_EPSILON);
    assert_float_equal(cam.fov, PI/2, DOUBLE_EPSILON);
}

static void test_raycast_hit(void **state) {
    (void)state;
    Map *m = map_create(5, 5);
    map_set(m, 4, 2, 1); // Wall directly to the right
    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI/2); // facing right (angle 0)
    
    RayResult res = raycast_fire(m, &cam, 0.0, 10.0);
    assert_true(res.hit);
    assert_int_equal(res.map_x, 4);
    assert_int_equal(res.map_y, 2);
    assert_float_equal(res.distance, 1.5, DOUBLE_EPSILON); // 4.0 - 2.5
    
    map_destroy(m);
}

static void test_raycast_miss(void **state) {
    (void)state;
    Map *m = map_create(5, 5);
    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI/2);
    
    RayResult res = raycast_fire(m, &cam, 0.0, 10.0);
    assert_false(res.hit);
    assert_float_equal(res.distance, 10.0, DOUBLE_EPSILON);
    
    map_destroy(m);
}

static void test_raycast_perpendicular_correction(void **state) {
    (void)state;
    Map *m = map_create(10, 10);
    // Wall at x = 5
    for(int y=0; y<10; y++) map_set(m, 5, y, 1);
    
    Camera cam;
    camera_init(&cam, 2.5, 5.5, 0.0, PI/2.0); // Facing right
    
    // Center ray
    double center_angle = 0.0;
    RayResult res_center = raycast_fire(m, &cam, center_angle, 10.0);
    assert_true(res_center.hit);
    double perp_dist_center = res_center.distance * cos(center_angle - cam.transform.angle);
    
    // Edge ray
    double edge_angle = atan(1.0 * tan(cam.fov / 2.0));
    RayResult res_edge = raycast_fire(m, &cam, edge_angle, 10.0);
    assert_true(res_edge.hit);
    double perp_dist_edge = res_edge.distance * cos(edge_angle - cam.transform.angle);
    
    // Perpendicular distance to a straight wall should be identical
    assert_float_equal(perp_dist_center, perp_dist_edge, DOUBLE_EPSILON);
    
    map_destroy(m);
}

static void test_raycast_near_plane_clipping(void **state) {
    (void)state;
    Map *m = map_create(5, 5);
    map_set(m, 2, 2, 1);
    Camera cam;
    camera_init(&cam, 1.99, 2.5, 0.0, PI/2); // Very close to the wall at x=2
    
    RayResult res = raycast_fire(m, &cam, 0.0, 10.0);
    assert_true(res.hit);
    assert_float_equal(res.distance, 0.01, DOUBLE_EPSILON);
    
    // Simulate render logic clip clamp
    double perp_dist = res.distance * cos(0.0);
    if (perp_dist < 0.001) perp_dist = 0.001; // Not clamped yet
    int line_height = (int)(10 / perp_dist);
    assert_true(line_height >= 999 && line_height <= 1000); // 10 / 0.01

    // Now push it so close it clamps
    cam.transform.pos.x = 1.99999;
    res = raycast_fire(m, &cam, 0.0, 10.0);
    assert_true(res.hit);
    perp_dist = res.distance * cos(0.0);
    if (perp_dist < 0.001) perp_dist = 0.001; // Clamps!
    line_height = (int)(10 / perp_dist);
    assert_int_equal(line_height, 10000); // 10 / 0.001
    
    map_destroy(m);
}

static void test_raycast_render_output(void **state) {
    (void)state;
    Grid *g = grid_create(10, 10);
    Map *m = map_create(5, 5);
    for(int x=0; x<5; x++) map_set(m, x, 0, 1); // front wall at y=0
    Camera cam;
    camera_init(&cam, 2.5, 2.5, -PI/2, PI/2); // facing up (-y direction)
    
    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_registry_set_palette(&assets, 1, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255}, (SDL_Color){255,255,255,255});
    asset_registry_set_material(&assets, 1, 1, "#x-.");

    WorldState world;
    world_init(&world);

    lighting_update_current(m, &world);
    raycast_render(g, m, &cam, &assets, &world, NULL);
    
    Cell c;
    // Check ceiling (top row)
    grid_get(g, 5, 0, &c);
    assert_int_equal(c.glyph, ' ');
    assert_int_equal(c.bg.r, 10); // 50 * 0.2 ambient
    
    // Check floor (bottom row)
    grid_get(g, 5, 9, &c);
    assert_int_equal(c.glyph, ' ');
    assert_int_equal(c.bg.r, 6); // 30 * 0.2 ambient
    
    // Check wall (middle)
    grid_get(g, 5, 5, &c);
    assert_true(c.glyph == '#' || c.glyph == 'x' || c.glyph == '+' || c.glyph == '-' || c.glyph == '.');
    assert_int_equal(c.bg.r, 0); // Wall bg is solid black
    
    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
}

static void test_raycast_render_extreme_horizon_offsets(void **state) {
    Grid *g = grid_create(41, 25);
    Map *m = map_create(5, 5);
    Camera cam;
    AssetRegistry assets;
    WorldState world;
    (void)state;

    assert_non_null(g);
    assert_non_null(m);
    for (int x = 0; x < 5; x++) map_set(m, x, 0, 1);
    camera_init(&cam, 2.5, 2.5, -PI / 2, PI / 2);
    asset_registry_init(&assets);
    asset_registry_set_palette(
        &assets, 1,
        (SDL_Color){255, 255, 255, 255},
        (SDL_Color){255, 255, 255, 255},
        (SDL_Color){255, 255, 255, 255});
    asset_registry_set_material(&assets, 1, 1, "#x-.");
    world_init(&world);
    lighting_update_current(m, &world);

    cam.pitch = -(double)g->height;
    raycast_render(g, m, &cam, &assets, &world, NULL);
    cam.pitch = 0.0;
    raycast_render(g, m, &cam, &assets, &world, NULL);
    cam.pitch = (double)g->height;
    raycast_render(g, m, &cam, &assets, &world, NULL);

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
}

static void test_raycast_render_has_no_fixed_width_cutoff(void **state) {
    (void)state;
    Grid *g = grid_create(1100, 10);
    Map *m = map_create(5, 5);
    Camera cam;
    AssetRegistry assets;
    WorldState world;
    Cell c;

    assert_non_null(g);
    assert_non_null(m);
    for (int x = 0; x < 5; x++) map_set(m, x, 0, 1);
    camera_init(&cam, 2.5, 2.5, -PI / 2, PI / 2);
    asset_registry_init(&assets);
    asset_registry_set_palette(
        &assets, 1,
        (SDL_Color){255, 255, 255, 255},
        (SDL_Color){255, 255, 255, 255},
        (SDL_Color){255, 255, 255, 255}
    );
    asset_registry_set_material(&assets, 1, 1, "#x-.");
    world_init(&world);

    lighting_update_current(m, &world);
    raycast_render(g, m, &cam, &assets, &world, NULL);

    assert_true(grid_get(g, 1099, 9, &c));
    assert_int_equal(c.glyph, ' ');
    assert_int_equal(c.bg.r, 6);
    assert_true(g->column_depths[1099] > 0.0);

    world_clear(&world);
    asset_registry_clear(&assets);
    map_destroy(m);
    grid_destroy(g);
}

/* ===================================================================
 *  Material name lookup tests
 * =================================================================== */

static void test_material_name_storage(void **state) {
    (void)state;
    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_loader_load_registry(&assets, "assets");

    /* Filename-derived names: "assets/materials/1.txt" -> "1", etc. */
    assert_string_equal(assets.material_names[1], "1");
    assert_string_equal(assets.material_names[2], "2");
    asset_registry_clear(&assets);
}

static void test_material_find_by_name(void **state) {
    (void)state;
    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_loader_load_registry(&assets, "assets");

    assert_int_equal(material_find_by_name(&assets, "1"), 1);
    assert_int_equal(material_find_by_name(&assets, "nonexistent"), -1);

    asset_registry_clear(&assets);
    asset_registry_clear(&assets);
}

static void test_material_name_by_id(void **state) {
    (void)state;
    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_loader_load_registry(&assets, "assets");

    assert_string_equal(material_name_by_id(&assets, 1), "1");
    assert_string_equal(material_name_by_id(&assets, 0), "UNKNOWN");
    /* ID 999 is out of range */
    assert_string_equal(material_name_by_id(&assets, 999), "UNKNOWN");
    asset_registry_clear(&assets);
}

static void test_material_id_is_loaded(void **state) {
    (void)state;
    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_loader_load_registry(&assets, "assets");

    assert_true(material_id_is_loaded(&assets, 1));
    /* ID 255 is not expected to be loaded */
    assert_false(material_id_is_loaded(&assets, 255));
    asset_registry_clear(&assets);
}

static void test_material_count(void **state) {
    (void)state;
    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_loader_load_registry(&assets, "assets");

    /* At least 4 material files exist in assets/materials/ */
    assert_true(assets.material_count >= 4);
    asset_registry_clear(&assets);
}

/* ===================================================================
 *  Named material loading tests
 *  These tests use isolated temp directories under /tmp to avoid
 *  touching the real assets and to keep results deterministic.
 * =================================================================== */

/* Helper: write content string to a file (overwrites if exists) */
static void write_mat_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (f) { fputs(content, f); fclose(f); }
}

static void test_named_material_auto_assign(void **state) {
    /* 2.txt locks ID 2; stone_brick.txt has no id= field → first free = ID 1 */
    (void)state;
    mkdir("/tmp/tst_mat1", 0755);
    write_mat_file("/tmp/tst_mat1/2.txt",          "palette=1\nglyphs=##\n");
    write_mat_file("/tmp/tst_mat1/stone_brick.txt", "palette=1\nglyphs=XX\n");

    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_loader_load_materials(&assets, "/tmp/tst_mat1");

    assert_int_equal(material_find_by_name(&assets, "stone_brick"), 1);
    assert_int_equal(material_find_by_name(&assets, "2"),           2);
    asset_registry_clear(&assets);

    remove("/tmp/tst_mat1/2.txt");
    remove("/tmp/tst_mat1/stone_brick.txt");
    rmdir("/tmp/tst_mat1");
}

static void test_named_material_gap_assign(void **state) {
    /* 1.txt and 3.txt occupy IDs 1 and 3; extra.txt → first free = ID 2 */
    (void)state;
    mkdir("/tmp/tst_mat2", 0755);
    write_mat_file("/tmp/tst_mat2/1.txt",    "palette=1\nglyphs=##\n");
    write_mat_file("/tmp/tst_mat2/3.txt",    "palette=1\nglyphs=##\n");
    write_mat_file("/tmp/tst_mat2/extra.txt","palette=1\nglyphs=EE\n");

    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_loader_load_materials(&assets, "/tmp/tst_mat2");

    assert_int_equal(material_find_by_name(&assets, "extra"), 2);

    asset_registry_clear(&assets);

    remove("/tmp/tst_mat2/1.txt");
    remove("/tmp/tst_mat2/3.txt");
    remove("/tmp/tst_mat2/extra.txt");
    rmdir("/tmp/tst_mat2");
}

static void test_named_material_explicit_id(void **state) {
    /* wood.txt requests id=8 explicitly */
    (void)state;
    mkdir("/tmp/tst_mat3", 0755);
    write_mat_file("/tmp/tst_mat3/wood.txt", "palette=1\nglyphs=WW\nid=8\n");

    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_loader_load_materials(&assets, "/tmp/tst_mat3");

    assert_int_equal(material_find_by_name(&assets, "wood"), 8);
    assert_true(material_id_is_loaded(&assets, 8));

    asset_registry_clear(&assets);

    remove("/tmp/tst_mat3/wood.txt");
    rmdir("/tmp/tst_mat3");
}

static void test_named_material_high_explicit_id(void **state) {
    AssetRegistry assets;
    (void)state;
    mkdir("/tmp/tst_mat_high", 0755);
    write_mat_file("/tmp/tst_mat_high/high.txt",
                   "palette=65535\nglyphs=HH\nid=60000\n");

    assert_true(asset_registry_init(&assets));
    asset_loader_load_materials(&assets, "/tmp/tst_mat_high");
    assert_int_equal(material_find_by_name(&assets, "high"), 60000);
    assert_int_equal(assets.materials[60000].palette_id, 65535);
    asset_registry_clear(&assets);

    remove("/tmp/tst_mat_high/high.txt");
    rmdir("/tmp/tst_mat_high");
}

static void test_named_material_collision_skipped(void **state) {
    /* 1.txt loads ID 1; conflict.txt requests id=1 → collision, skipped */
    (void)state;
    mkdir("/tmp/tst_mat4", 0755);
    write_mat_file("/tmp/tst_mat4/1.txt",        "palette=1\nglyphs=##\n");
    write_mat_file("/tmp/tst_mat4/conflict.txt",  "palette=1\nglyphs=CC\nid=1\n");

    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_loader_load_materials(&assets, "/tmp/tst_mat4");

    /* ID 1 still holds the numeric file's name, not "conflict" */
    assert_string_equal(material_name_by_id(&assets, 1), "1");
    assert_int_equal(material_find_by_name(&assets, "conflict"), -1);

    asset_registry_clear(&assets);

    remove("/tmp/tst_mat4/1.txt");
    remove("/tmp/tst_mat4/conflict.txt");
    rmdir("/tmp/tst_mat4");
}

static void test_named_material_count_is_count(void **state) {
    /* material_count == number of loaded slots, not highest ID */
    (void)state;
    mkdir("/tmp/tst_mat5", 0755);
    write_mat_file("/tmp/tst_mat5/1.txt",   "palette=1\nglyphs=##\n");
    write_mat_file("/tmp/tst_mat5/wood.txt","palette=1\nglyphs=WW\n");

    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_loader_load_materials(&assets, "/tmp/tst_mat5");

    /* Two files loaded → count == 2, not 2 from highest-ID tracking */
    assert_int_equal(assets.material_count, 2);

    asset_registry_clear(&assets);

    remove("/tmp/tst_mat5/1.txt");
    remove("/tmp/tst_mat5/wood.txt");
    rmdir("/tmp/tst_mat5");
}

static void test_numeric_material_regression(void **state) {
    /* Numeric loading via asset_loader_load_registry still works correctly */
    (void)state;
    AssetRegistry assets;
    asset_registry_init(&assets);
    asset_loader_load_registry(&assets, "assets");

    assert_string_equal(material_name_by_id(&assets, 1), "1");
    assert_string_equal(material_name_by_id(&assets, 2), "2");
    assert_true(assets.material_count >= 4);
    asset_registry_clear(&assets);
}

static void test_sparse_high_id_bulk_asset_loading(void **state) {
    const char *root = "/tmp/tst_assets_high";
    AssetRegistry assets;
    const DecalPatternAsset *decal;
    SDL_Color sampled;
    (void)state;

    mkdir(root, 0755);
    mkdir("/tmp/tst_assets_high/palettes", 0755);
    mkdir("/tmp/tst_assets_high/materials", 0755);
    mkdir("/tmp/tst_assets_high/sprites", 0755);
    mkdir("/tmp/tst_assets_high/decals", 0755);
    write_mat_file("/tmp/tst_assets_high/palettes/50000.txt",
                   "near=10,20,30,255\nmid=4,5,6,255\nfar=1,2,3,255\n");
    write_mat_file("/tmp/tst_assets_high/decals/50000.txt",
                   "pattern_cols=1\npattern_rows=1\ndefault_material=65535\n"
                   "pattern_0=X\nmaterial_0=65535\n");

    assert_true(asset_registry_init(&assets));
    assert_int_equal(assets.generation, 0U);
    asset_loader_load_registry(&assets, root);
    assert_int_equal(assets.generation, 1U);
    sampled = palette_sample(
        &assets.palettes[50000], 1.0, (LightLevel){1.0, 1.0, 1.0});
    assert_int_equal(sampled.r, 10U);
    assert_int_equal(sampled.g, 20U);
    assert_int_equal(sampled.b, 30U);
    decal = asset_registry_get_decal_pattern(&assets, 50000);
    assert_non_null(decal);
    assert_int_equal(decal->pattern[0].glyph, 'X');
    assert_int_equal(decal->pattern[0].material_id, UINT16_MAX);
    asset_registry_clear(&assets);

    remove("/tmp/tst_assets_high/palettes/50000.txt");
    remove("/tmp/tst_assets_high/decals/50000.txt");
    rmdir("/tmp/tst_assets_high/palettes");
    rmdir("/tmp/tst_assets_high/materials");
    rmdir("/tmp/tst_assets_high/sprites");
    rmdir("/tmp/tst_assets_high/decals");
    rmdir(root);
}

int main(void) {
    config_init_defaults();
    
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_grid_init),
        cmocka_unit_test(test_grid_init_invalid),
        cmocka_unit_test(test_grid_destroy_safety),
        cmocka_unit_test(test_grid_set_get_bounds),
        cmocka_unit_test(test_grid_clear),
        cmocka_unit_test(test_grid_clear_region_zero),
        cmocka_unit_test(test_scale_calculations),
        cmocka_unit_test(test_scale_invalid),
        cmocka_unit_test(test_timing_math),
        cmocka_unit_test(test_perf_stats),
        cmocka_unit_test(test_renderer_backend_init_invalid),
        cmocka_unit_test(test_renderer_backend_instrumentation),
        cmocka_unit_test(test_renderer_restore_merge_no_overlap),
        cmocka_unit_test(test_renderer_restore_merge_overlap_and_capacity),
        cmocka_unit_test(test_renderer_restore_merge_ignores_invalid_dirty_index),
        cmocka_unit_test(test_checked_size_boundaries),
        cmocka_unit_test(test_renderer_preflight_boundaries),
        // NEW ENGINE REFACTOR & RAYCAST TESTS
        cmocka_unit_test(test_asset_loader),
        cmocka_unit_test(test_horizontal_surface_constant_baseline_level),
        cmocka_unit_test(test_horizontal_surface_constant_baseline_negative_pitch),
        cmocka_unit_test(test_horizontal_surface_constant_baseline_positive_pitch),
        cmocka_unit_test(test_horizontal_surface_constant_baseline_out_of_bounds),
        cmocka_unit_test(test_authored_horizontal_materials_and_lighting),
        cmocka_unit_test(test_missing_horizontal_material_is_obvious_and_unlit),
        cmocka_unit_test(test_invalid_surface_view_preserves_constant_backgrounds),
        cmocka_unit_test(test_valid_surface_view_preserves_out_of_bounds_backgrounds),
        cmocka_unit_test(test_height_view_flat_parity_and_authored_difference),
        cmocka_unit_test(test_horizontal_decal_follows_authored_floor_height),
        cmocka_unit_test(test_rendered_point_light_affects_ceiling_walls_and_floor),
        cmocka_unit_test(test_config_parsing),
        cmocka_unit_test(test_config_transactional_valid_override),
        cmocka_unit_test(test_config_invalid_file_rolls_back),
        cmocka_unit_test(test_config_rejects_malformed_numbers),
        cmocka_unit_test(test_config_effective_validation),
        cmocka_unit_test(test_math_normalize),
        cmocka_unit_test(test_map_creation),
        cmocka_unit_test(test_map_bounds),
        cmocka_unit_test(test_map_lookup),
        cmocka_unit_test(test_map_text_ragged_padding),
        cmocka_unit_test(test_map_text_dimension_limits),
        cmocka_unit_test(test_camera_init),
        cmocka_unit_test(test_raycast_hit),
        cmocka_unit_test(test_raycast_miss),
        cmocka_unit_test(test_raycast_perpendicular_correction),
        cmocka_unit_test(test_raycast_near_plane_clipping),
        cmocka_unit_test(test_raycast_render_output),
        cmocka_unit_test(test_raycast_render_extreme_horizon_offsets),
        cmocka_unit_test(test_raycast_render_has_no_fixed_width_cutoff),
        /* --- Material name lookup --- */
        cmocka_unit_test(test_material_name_storage),
        cmocka_unit_test(test_material_find_by_name),
        cmocka_unit_test(test_material_name_by_id),
        cmocka_unit_test(test_material_id_is_loaded),
        cmocka_unit_test(test_material_count),
        /* --- Named material loading --- */
        cmocka_unit_test(test_named_material_auto_assign),
        cmocka_unit_test(test_named_material_gap_assign),
        cmocka_unit_test(test_named_material_explicit_id),
        cmocka_unit_test(test_named_material_high_explicit_id),
        cmocka_unit_test(test_named_material_collision_skipped),
        cmocka_unit_test(test_named_material_count_is_count),
        cmocka_unit_test(test_numeric_material_regression),
        cmocka_unit_test(test_sparse_high_id_bulk_asset_loading),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
