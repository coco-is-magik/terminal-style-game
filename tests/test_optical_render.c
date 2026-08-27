#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <cmocka.h>

#include "../src/config.h"
#include "../src/height_projection.h"
#include "../src/heightfield_trace.h"
#include "../src/math.h"
#include "../src/mirror_trace.h"
#include "../src/optical_compositor.h"
#include "../src/raycast_internal.h"
#include "../src/raycast.h"

#define WIDTH 12
#define HEIGHT 5
#define GRID_WIDTH 41
#define GRID_HEIGHT 25
#define GENERATION UINT32_C(41)

typedef struct {
    Grid *grid;
    Grid *reference;
    Map *map;
    AssetRegistry assets;
    WorldState world;
    Camera camera;
    SceneAuthoredCell cells[WIDTH * HEIGHT];
    SceneSurfaceView surfaces;
    SceneHeightView heights;
} Fixture;

static size_t index_at(int x, int y) {
    return (size_t)y * WIDTH + (size_t)x;
}

static void loaded(AssetRegistry *assets, int id, SDL_Color color, const char *glyphs) {
    asset_registry_set_palette(assets, id, color, color, color);
    asset_registry_set_material(assets, id, id, glyphs);
    (void)snprintf(assets->material_names[id], MATERIAL_NAME_CAPACITY, "%d", id);
}

static void fixture_init(Fixture *f) {
    memset(f, 0, sizeof(*f));
    config_init_defaults();
    f->grid = grid_create(GRID_WIDTH, GRID_HEIGHT);
    f->reference = grid_create(GRID_WIDTH, GRID_HEIGHT);
    f->map = map_create(WIDTH, HEIGHT);
    assert_non_null(f->grid);
    assert_non_null(f->reference);
    assert_non_null(f->map);
    assert_true(asset_registry_init(&f->assets));
    world_init(&f->world);
    camera_init(&f->camera, 1.5, 2.5, 0.0, PI / 2.0);
    f->camera.z = 0.75;
    f->surfaces = (SceneSurfaceView){f->cells, WIDTH * HEIGHT, WIDTH, HEIGHT};
    f->heights = (SceneHeightView){
        f->cells, WIDTH * HEIGHT, WIDTH, HEIGHT,
        {9.8, SCENE_GRAVITY_DOWN, 0.25, 3.2, 1.0, 0.5, 0.75}
    };
    loaded(&f->assets, 1, (SDL_Color){40, 40, 40, 255}, "ffff");
    loaded(&f->assets, 2, (SDL_Color){50, 50, 50, 255}, "cccc");
    loaded(&f->assets, 10, (SDL_Color){200, 20, 20, 255}, "gggg");
    loaded(&f->assets, 20, (SDL_Color){20, 20, 200, 255}, "WWWW");
    for (size_t i = 0U; i < WIDTH * HEIGHT; i++) {
        f->cells[i].floor_present = true;
        f->cells[i].ceiling_present = true;
        f->cells[i].floor_height_step = SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
        f->cells[i].ceiling_height_step = SCENE_DEFAULT_CEILING_HEIGHT_STEP;
        f->cells[i].floor_material = 1U;
        f->cells[i].ceiling_material = 2U;
        f->map->light_map[i] = 1.0;
    }
}

static void fixture_wall(Fixture *f, int x, uint16_t material) {
    size_t i = index_at(x, 2);
    f->cells[i].occupancy = SCENE_CELL_OCCUPANCY_WALL;
    f->cells[i].wall_material = material;
    map_set(f->map, x, 2, material);
}

static void fixture_destroy(Fixture *f) {
    world_clear(&f->world);
    asset_registry_clear(&f->assets);
    map_destroy(f->map);
    grid_destroy(f->reference);
    grid_destroy(f->grid);
}

static OpticalExtension transparent(void) {
    OpticalExtension value = {0};
    value.override_mask = OPTICAL_OVERRIDE_RAY_BLOCKS |
                          OPTICAL_OVERRIDE_OPACITY |
                          OPTICAL_OVERRIDE_TRANSMISSION;
    value.opacity = 64U;
    value.transmission = 192U;
    return value;
}

static OpticalExtension mirror(uint8_t reflectivity) {
    OpticalExtension value = {0};
    value.override_mask = OPTICAL_OVERRIDE_REFLECTIVITY;
    value.reflectivity = reflectivity;
    return value;
}

static Cell center(const Grid *grid) {
    return grid->cells[(GRID_HEIGHT / 2) * GRID_WIDTH + GRID_WIDTH / 2];
}

static int find_row_with_material(Fixture *f, uint16_t material) {
    HeightfieldTraceColumn column;
    assert_true(heightfield_trace_prepare_column(
        &column, &f->camera, f->map, &f->heights,
        GRID_WIDTH, GRID_HEIGHT, GRID_WIDTH / 2,
        config_get()->raycast_max_distance));
    for (int y = 0; y < GRID_HEIGHT; y++) {
        HeightfieldHit hit = heightfield_trace_prepared_sample(&column, y);
        if (hit.hit && hit.material == material) return y;
    }
    return -1;
}

static void test_opaque_and_stale_views_preserve_complete_frame(void **state) {
    Fixture f;
    OpticalRuntimeView view;
    (void)state;
    fixture_init(&f);
    fixture_wall(&f, 3, 10U);
    assert_true(optical_runtime_view_init(
        &view, WIDTH * HEIGHT, NULL, 0U, NULL, 0U, GENERATION));
    raycast_render_height_optical(
        f.reference, f.map, &f.camera, &f.assets, &f.world,
        &f.surfaces, &f.heights, NULL, 0U);
    raycast_render_height_optical(f.grid, f.map, &f.camera, &f.assets, &f.world,
                                  &f.surfaces, &f.heights, &view, GENERATION);
    assert_memory_equal(f.grid->cells, f.reference->cells,
                        GRID_WIDTH * GRID_HEIGHT * sizeof(*f.grid->cells));
    memset(f.grid->cells, 0x5a, GRID_WIDTH * GRID_HEIGHT * sizeof(*f.grid->cells));
    raycast_render_height_optical(f.grid, f.map, &f.camera, &f.assets, &f.world,
                                  &f.surfaces, &f.heights, &view, GENERATION + 1U);
    assert_memory_equal(f.grid->cells, f.reference->cells,
                        GRID_WIDTH * GRID_HEIGHT * sizeof(*f.grid->cells));
    fixture_destroy(&f);
}

static void test_nonvisual_custom_override_does_not_move_roof(void **state) {
    Fixture f;
    OpticalExtension materials[11] = {0};
    OpticalRuntimeView view;
    size_t count = (size_t)GRID_WIDTH * (size_t)GRID_HEIGHT;
    (void)state;
    fixture_init(&f);
    fixture_wall(&f, 3, 10U);
    for (int y = 0; y < HEIGHT; y++) {
        size_t index = index_at(4, y);
        f.cells[index].ceiling_height_step = INT16_C(0x0140);
    }
    raycast_render_height_optical(
        f.reference, f.map, &f.camera, &f.assets, &f.world,
        &f.surfaces, &f.heights, NULL, 0U);

    materials[10].override_mask = OPTICAL_OVERRIDE_PLAYER_BLOCKS;
    materials[10].player_blocks = 1U;
    assert_true(optical_runtime_view_init(
        &view, WIDTH * HEIGHT, materials, 11U, NULL, 0U, GENERATION));
    raycast_render_height_optical(
        f.grid, f.map, &f.camera, &f.assets, &f.world,
        &f.surfaces, &f.heights, &view, GENERATION);

    assert_memory_equal(f.grid->cells, f.reference->cells,
                        count * sizeof(*f.grid->cells));
    assert_memory_equal(f.grid->world_depths, f.reference->world_depths,
                        count * sizeof(*f.grid->world_depths));
    assert_memory_equal(f.grid->world_hit_keys, f.reference->world_hit_keys,
                        count * sizeof(*f.grid->world_hit_keys));
    fixture_destroy(&f);
}

static void test_transparent_wall_composes_over_far_wall(void **state) {
    Fixture f;
    OpticalExtension materials[21] = {0};
    OpticalRuntimeView view;
    Cell value;
    (void)state;
    fixture_init(&f);
    fixture_wall(&f, 3, 10U);
    fixture_wall(&f, 6, 20U);
    materials[10] = transparent();
    assert_true(optical_runtime_view_init(
        &view, WIDTH * HEIGHT, materials, 21U, NULL, 0U, GENERATION));
    raycast_render_height_optical(f.grid, f.map, &f.camera, &f.assets, &f.world,
                                  &f.surfaces, &f.heights, &view, GENERATION);
    value = center(f.grid);
    assert_int_equal(value.glyph, 'W');
    assert_true(value.fg.r > 20U);
    assert_true(value.fg.b > 20U);
    fixture_destroy(&f);
}

static void test_two_layers_are_deterministic(void **state) {
    Fixture f;
    OpticalExtension materials[21] = {0};
    OpticalRuntimeView view;
    Cell first;
    Cell second;
    (void)state;
    fixture_init(&f);
    fixture_wall(&f, 3, 10U);
    fixture_wall(&f, 4, 10U);
    fixture_wall(&f, 6, 20U);
    materials[10] = transparent();
    assert_true(optical_runtime_view_init(
        &view, WIDTH * HEIGHT, materials, 21U, NULL, 0U, GENERATION));
    raycast_render_height_optical(f.grid, f.map, &f.camera, &f.assets, &f.world,
                                  &f.surfaces, &f.heights, &view, GENERATION);
    first = center(f.grid);
    assert_int_equal(first.glyph, 'W');
    raycast_render_height_optical(f.grid, f.map, &f.camera, &f.assets, &f.world,
                                  &f.surfaces, &f.heights, &view, GENERATION);
    second = center(f.grid);
    assert_memory_equal(&first, &second, sizeof(first));
    fixture_destroy(&f);
}

static void test_transparent_floor_over_opening_uses_darkness(void **state) {
    Fixture f;
    OpticalExtension materials[2] = {0};
    OpticalRuntimeView view;
    int row;
    Cell value;
    (void)state;
    fixture_init(&f);
    f.cells[index_at(5, 2)].floor_present = false;
    materials[1] = transparent();
    assert_true(optical_runtime_view_init(
        &view, WIDTH * HEIGHT, materials, 2U, NULL, 0U, GENERATION));
    row = find_row_with_material(&f, 1U);
    assert_true(row >= 0);
    raycast_render_height_optical(f.grid, f.map, &f.camera, &f.assets, &f.world,
                                  &f.surfaces, &f.heights, &view, GENERATION);
    value = f.grid->cells[(size_t)row * GRID_WIDTH + GRID_WIDTH / 2];
    assert_int_equal(value.glyph, ' ');
    assert_true(value.fg.r > 0U);
    assert_int_equal(value.bg.r, 0U);
    fixture_destroy(&f);
}

static void test_production_compositor_exact_rules(void **state) {
    OpticalCompositeLayer layers[2] = {
        {{'g', {100, 150, 200, 255}, {20, 40, 60, 255}},
         {false, false, false, 64, 192, 0}, false},
        {{'W', {200, 100, 50, 255}, {100, 80, 60, 255}},
         {true, true, true, 255, 0, 0}, false}
    };
    const Cell darkness = {' ', {0, 0, 0, 255}, {0, 0, 0, 255}};
    OpticalCompositeResult result;
    (void)state;
    assert_true(optical_composite_layers(
        layers, 2U, true, false, false, &darkness, &result));
    assert_int_equal(result.cell.glyph, 'W');
    assert_int_equal(result.cell.fg.r, 176U);
    assert_int_equal(result.cell.fg.g, 113U);
    assert_int_equal(result.cell.fg.b, 88U);
    assert_true(result.terminated_by_surface);
}

static void test_compositor_threshold_cap_and_transactional_validation(void **state) {
    OpticalCompositeLayer layers[OPTICAL_COMPOSITOR_MAX_LAYERS];
    const Cell darkness = {' ', {0, 0, 0, 255}, {0, 0, 0, 255}};
    OpticalCompositeResult sentinel;
    OpticalCompositeResult result;
    (void)state;
    for (size_t i = 0U; i < OPTICAL_COMPOSITOR_MAX_LAYERS; i++) {
        layers[i] = (OpticalCompositeLayer){
            {(uint8_t)('a' + i), {40, 20, 10, 255}, {5, 4, 3, 255}},
            {false, false, false, 64, 192, 0}, false
        };
    }
    assert_true(optical_composite_layers(
        layers, OPTICAL_COMPOSITOR_MAX_LAYERS,
        false, false, true, &darkness, &result));
    assert_int_equal(result.cell.glyph, ' ');
    assert_true(result.layer_cap_exhausted);
    memset(&sentinel, 0x5a, sizeof(sentinel));
    result = sentinel;
    layers[1].optical.ray_blocks = true;
    assert_false(optical_composite_layers(
        layers, OPTICAL_COMPOSITOR_MAX_LAYERS,
        false, false, true, &darkness, &result));
    assert_memory_equal(&result, &sentinel, sizeof(result));
    assert_false(optical_composite_layers(
        layers, 0U, false, true, false, &darkness, &result));
    assert_memory_equal(&result, &sentinel, sizeof(result));
}

static void test_compositor_natural_exhaustion_is_valid(void **state) {
    OpticalCompositeLayer layer = {
        {'g', {100, 150, 200, 255}, {20, 40, 60, 255}},
        {false, false, false, 64, 192, 0}, false
    };
    const Cell darkness = {' ', {0, 0, 0, 255}, {0, 0, 0, 255}};
    OpticalCompositeResult result;
    (void)state;
    assert_true(optical_composite_layers(
        &layer, 1U, false, false, false, &darkness, &result));
    assert_int_equal(result.cell.glyph, ' ');
    assert_true(result.cell.fg.r > 0U);
    assert_false(result.terminated_by_surface);
    assert_false(result.reached_opening);
    assert_false(result.layer_cap_exhausted);
}

static void test_generated_discontinuity_matches_direct_composition(void **state) {
    Fixture f;
    OpticalExtension materials[21] = {0};
    OpticalRuntimeView view;
    HeightfieldTraceColumn column;
    int selected_row = -1;
    HeightfieldOpticalResult traced = {0};
    OpticalCompositeLayer layers[OPTICAL_COMPOSITOR_MAX_LAYERS];
    OpticalCompositeResult expected;
    const Cell darkness = {' ', {0, 0, 0, 255}, {0, 0, 0, 255}};
    Cell actual;
    (void)state;
    fixture_init(&f);
    f.cells[index_at(2, 2)].floor_height_step = INT16_C(0x0080);
    f.cells[index_at(3, 2)].floor_height_step = INT16_C(0x0000);
    fixture_wall(&f, 6, 20U);
    materials[1] = transparent();
    assert_true(optical_runtime_view_init(
        &view, WIDTH * HEIGHT, materials, 21U, NULL, 0U, GENERATION));
    assert_true(heightfield_trace_prepare_column(
        &column, &f.camera, f.map, &f.heights,
        GRID_WIDTH, GRID_HEIGHT, GRID_WIDTH / 2,
        config_get()->raycast_max_distance));
    for (int y = 0; y < GRID_HEIGHT; y++) {
        if (!heightfield_trace_selective(
                &column, &view, GENERATION, y, &traced)) continue;
        for (size_t i = 0U; i < traced.count; i++) {
            if (traced.layers[i].hit.generated_boundary && traced.count > 1U) {
                selected_row = y;
                break;
            }
        }
        if (selected_row >= 0) break;
    }
    assert_true(selected_row >= 0);
    for (size_t i = 0U; i < traced.count; i++) {
        layers[i].sampled_cell = raycast_sample_heightfield_hit(
            f.map, &f.assets, &traced.layers[i].hit);
        layers[i].optical = traced.layers[i].optical;
        layers[i].generated_boundary = traced.layers[i].hit.generated_boundary;
    }
    assert_true(optical_composite_layers(
        layers, traced.count, traced.terminated_by_surface,
        traced.reached_opening, traced.layer_cap_exhausted,
        &darkness, &expected));
    raycast_render_height_optical(f.grid, f.map, &f.camera, &f.assets, &f.world,
                                  &f.surfaces, &f.heights, &view, GENERATION);
    actual = f.grid->cells[(size_t)selected_row * GRID_WIDTH + GRID_WIDTH / 2];
    assert_memory_equal(&actual, &expected.cell, sizeof(actual));
    fixture_destroy(&f);
}

static void test_four_transparent_walls_render_cap_result(void **state) {
    Fixture f;
    OpticalExtension materials[21] = {0};
    OpticalRuntimeView view;
    Cell value;
    (void)state;
    fixture_init(&f);
    for (int x = 2; x <= 5; x++) fixture_wall(&f, x, 10U);
    materials[10] = transparent();
    assert_true(optical_runtime_view_init(
        &view, WIDTH * HEIGHT, materials, 21U, NULL, 0U, GENERATION));
    raycast_render_height_optical(f.grid, f.map, &f.camera, &f.assets, &f.world,
                                  &f.surfaces, &f.heights, &view, GENERATION);
    value = center(f.grid);
    assert_int_equal(value.glyph, ' ');
    assert_true(value.fg.r > 0U);
    fixture_destroy(&f);
}

static void test_wall_decal_remains_after_optical_geometry(void **state) {
    Fixture f;
    OpticalExtension materials[21] = {0};
    OpticalRuntimeView view;
    Decal decal = {0};
    bool found = false;
    (void)state;
    fixture_init(&f);
    fixture_wall(&f, 3, 10U);
    fixture_wall(&f, 6, 20U);
    materials[10] = transparent();
    decal.surface = DECAL_SURFACE_WALL;
    decal.x = 3.0; decal.y = 2.5; decal.z = 0.5;
    decal.map_x = 3; decal.map_y = 2; decal.side = 0;
    decal.rotation = PI;
    decal.width = 1.0; decal.height = 1.0; decal.depth = 0.1;
    decal.pattern_cols = 1; decal.pattern_rows = 1;
    decal.pattern = malloc(sizeof(*decal.pattern));
    assert_non_null(decal.pattern);
    decal.pattern[0] = (PatternCell){'D', 20U};
    assert_int_equal(world_add_decal(&f.world, decal), WORLD_INSERT_OK);
    assert_true(optical_runtime_view_init(
        &view, WIDTH * HEIGHT, materials, 21U, NULL, 0U, GENERATION));
    raycast_render_height_optical(f.grid, f.map, &f.camera, &f.assets, &f.world,
                                  &f.surfaces, &f.heights, &view, GENERATION);
    for (int i = 0; i < GRID_WIDTH * GRID_HEIGHT; i++)
        if (f.grid->cells[i].glyph == 'D') found = true;
    assert_true(found);
    fixture_destroy(&f);
}

static void test_light_billboard_remains_after_optical_geometry(void **state) {
    Fixture f;
    OpticalExtension materials[21] = {0};
    OpticalRuntimeView view;
    (void)state;
    fixture_init(&f);
    fixture_wall(&f, 3, 10U);
    fixture_wall(&f, 6, 20U);
    materials[10] = transparent();
    assert_int_equal(world_add_light(
        &f.world, 2.0, 2.5, (SDL_Color){1, 255, 1, 255}, 1.0, 2.0), WORLD_INSERT_OK);
    assert_true(optical_runtime_view_init(
        &view, WIDTH * HEIGHT, materials, 21U, NULL, 0U, GENERATION));
    raycast_render_height_optical(f.grid, f.map, &f.camera, &f.assets, &f.world,
                                  &f.surfaces, &f.heights, &view, GENERATION);
    assert_int_equal(center(f.grid).glyph, '*');
    fixture_destroy(&f);
}

static void test_reflection_mix_exact_math_and_glyph_threshold(void **state) {
    Cell direct = {'D', {200U, 20U, 10U, 255U}, {100U, 40U, 20U, 255U}};
    Cell reflected = {'R', {20U, 100U, 220U, 255U}, {10U, 80U, 160U, 255U}};
    Cell output;
    Cell sentinel;
    (void)state;
    assert_true(optical_mix_reflection(&direct, &reflected, 127U, &output));
    assert_int_equal(output.glyph, 'D');
    assert_int_equal(output.fg.r, 110U);
    assert_int_equal(output.fg.g, 60U);
    assert_int_equal(output.fg.b, 115U);
    assert_true(optical_mix_reflection(&direct, &reflected, 128U, &output));
    assert_int_equal(output.glyph, 'R');
    assert_true(optical_mix_reflection(&direct, &reflected, 255U, &output));
    assert_memory_equal(&output, &reflected, sizeof(output));
    memset(&sentinel, 0x5a, sizeof(sentinel));
    output = sentinel;
    assert_false(optical_mix_reflection(&direct, &reflected, 0U, &output));
    assert_memory_equal(&output, &sentinel, sizeof(output));
}

static void test_full_mirror_renders_reflected_wall_and_keeps_frontier(void **state) {
    Fixture f;
    OpticalExtension materials[21] = {0};
    OpticalRuntimeView view;
    size_t center_index = (GRID_HEIGHT / 2) * GRID_WIDTH + GRID_WIDTH / 2;
    (void)state;
    fixture_init(&f);
    fixture_wall(&f, 3, 10U);
    fixture_wall(&f, 0, 20U);
    materials[10] = mirror(255U);
    assert_true(optical_runtime_view_init(
        &view, WIDTH * HEIGHT, materials, 21U, NULL, 0U, GENERATION));
    raycast_render_height_optical(f.grid, f.map, &f.camera, &f.assets, &f.world,
                                  &f.surfaces, &f.heights, &view, GENERATION);
    assert_int_equal(center(f.grid).glyph, 'W');
    assert_true(f.grid->world_depths[center_index] < 2.0);
    assert_true(f.grid->world_hit_keys[center_index] != 0U);
    fixture_destroy(&f);
}

static void test_partial_mirror_and_reflected_opening_darkness(void **state) {
    Fixture f;
    OpticalExtension materials[21] = {0};
    OpticalRuntimeView view;
    HeightfieldTraceColumn column;
    int mirror_row = -1;
    size_t sample_index;
    Cell partial;
    (void)state;
    fixture_init(&f);
    fixture_wall(&f, 3, 10U);
    fixture_wall(&f, 0, 20U);
    materials[10] = mirror(64U);
    assert_true(optical_runtime_view_init(
        &view, WIDTH * HEIGHT, materials, 21U, NULL, 0U, GENERATION));
    raycast_render_height_optical(f.grid, f.map, &f.camera, &f.assets, &f.world,
                                  &f.surfaces, &f.heights, &view, GENERATION);
    partial = center(f.grid);
    assert_int_equal(partial.glyph, 'g');
    assert_true(partial.fg.r > partial.fg.b);
    assert_true(heightfield_trace_prepare_column(
        &column, &f.camera, f.map, &f.heights, GRID_WIDTH, GRID_HEIGHT,
        GRID_WIDTH / 2, config_get()->raycast_max_distance));
    for (int y = 0; y < GRID_HEIGHT; y++) {
        HeightfieldHit hit = heightfield_trace_prepared_sample(&column, y);
        if (hit.hit && hit.kind == HEIGHTFIELD_HIT_WALL && hit.material == 10U) {
            double row_delta = y + 0.5 - camera_horizon_row(&f.camera, GRID_HEIGHT);
            if (fabs(row_delta) > 1.0) { mirror_row = y; break; }
        }
    }
    assert_true(mirror_row >= 0);
    map_set(f.map, 0, 2, 0);
    f.cells[index_at(0, 2)].occupancy = SCENE_CELL_OCCUPANCY_EMPTY;
    f.cells[index_at(0, 2)].wall_material = 0U;
    for (size_t i = 0U; i < WIDTH * HEIGHT; i++) {
        f.cells[i].floor_present = false;
        f.cells[i].ceiling_present = false;
    }
    materials[10] = mirror(255U);
    raycast_render_height_optical(f.grid, f.map, &f.camera, &f.assets, &f.world,
                                  &f.surfaces, &f.heights, &view, GENERATION);
    sample_index = (size_t)mirror_row * (size_t)GRID_WIDTH + (size_t)GRID_WIDTH / 2;
    assert_int_equal(f.grid->cells[sample_index].glyph, ' ');
    assert_int_equal(f.grid->cells[sample_index].fg.r, 0U);
    assert_int_equal(f.grid->cells[sample_index].bg.r, 0U);
    fixture_destroy(&f);
}

static void test_mirror_cache_reuses_column_and_reflected_mirror_is_terminal(void **state) {
    Fixture f;
    OpticalExtension materials[21] = {0};
    OpticalRuntimeView view;
    HeightfieldTraceColumn incoming;
    MirrorTraceColumnCache cache;
    MirrorTraceResult first;
    MirrorTraceResult second;
    HeightfieldHit mirror_hit;
    int first_row = -1;
    int second_row = -1;
    (void)state;
    fixture_init(&f);
    fixture_wall(&f, 3, 10U);
    fixture_wall(&f, 0, 10U);
    materials[10] = mirror(255U);
    assert_true(optical_runtime_view_init(
        &view, WIDTH * HEIGHT, materials, 21U, NULL, 0U, GENERATION));
    assert_true(heightfield_trace_prepare_column(
        &incoming, &f.camera, f.map, &f.heights, GRID_WIDTH, GRID_HEIGHT,
        GRID_WIDTH / 2, config_get()->raycast_max_distance));
    mirror_trace_column_cache_init(&cache);
    for (int y = 0; y < GRID_HEIGHT; y++) {
        HeightfieldHit hit = heightfield_trace_prepared_sample(&incoming, y);
        if (hit.hit && hit.kind == HEIGHTFIELD_HIT_WALL && hit.material == 10U) {
            MirrorTraceResult candidate;
            if (!mirror_trace_sample_once(
                    &cache, &incoming, &hit, &view, GENERATION, y,
                    config_get()->raycast_max_distance, &candidate)) continue;
            if (candidate.darkness_fallback) {
                if (first_row < 0) first_row = y;
                else { second_row = y; break; }
            }
        }
    }
    assert_true(first_row >= 0 && second_row >= 0);
    mirror_hit = heightfield_trace_prepared_sample(&incoming, first_row);
    mirror_trace_column_cache_init(&cache);
    assert_true(mirror_trace_sample_once(
        &cache, &incoming, &mirror_hit, &view, GENERATION, first_row,
        config_get()->raycast_max_distance, &first));
    mirror_hit = heightfield_trace_prepared_sample(&incoming, second_row);
    assert_true(mirror_trace_sample_once(
        &cache, &incoming, &mirror_hit, &view, GENERATION, second_row,
        config_get()->raycast_max_distance, &second));
    assert_int_equal(cache.preparation_count, 1U);
    assert_true(first.darkness_fallback);
    assert_true(second.darkness_fallback);
    assert_int_equal(first.bounce_count, 1U);
    assert_int_equal(second.bounce_count, 1U);
    fixture_destroy(&f);
}

static void test_second_mirror_plane_in_column_is_bounded_darkness(void **state) {
    Fixture f;
    OpticalExtension materials[21] = {0};
    OpticalRuntimeView view;
    HeightfieldTraceColumn incoming;
    MirrorTraceColumnCache cache;
    MirrorTraceResult result;
    HeightfieldHit first;
    HeightfieldHit other;
    (void)state;
    fixture_init(&f);
    fixture_wall(&f, 3, 10U);
    materials[10] = mirror(255U);
    assert_true(optical_runtime_view_init(
        &view, WIDTH * HEIGHT, materials, 21U, NULL, 0U, GENERATION));
    assert_true(heightfield_trace_prepare_column(
        &incoming, &f.camera, f.map, &f.heights, GRID_WIDTH, GRID_HEIGHT,
        GRID_WIDTH / 2, config_get()->raycast_max_distance));
    first = heightfield_trace_prepared_sample(&incoming, GRID_HEIGHT / 2);
    assert_true(first.hit && first.kind == HEIGHTFIELD_HIT_WALL);
    mirror_trace_column_cache_init(&cache);
    assert_true(mirror_trace_sample_once(
        &cache, &incoming, &first, &view, GENERATION, GRID_HEIGHT / 2,
        config_get()->raycast_max_distance, &result));
    other = first;
    other.world_x += 1.0;
    assert_true(mirror_trace_sample_once(
        &cache, &incoming, &other, &view, GENERATION, GRID_HEIGHT / 2,
        config_get()->raycast_max_distance, &result));
    assert_true(result.darkness_fallback);
    assert_int_equal(cache.preparation_count, 1U);
    fixture_destroy(&f);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_opaque_and_stale_views_preserve_complete_frame),
        cmocka_unit_test(test_nonvisual_custom_override_does_not_move_roof),
        cmocka_unit_test(test_transparent_wall_composes_over_far_wall),
        cmocka_unit_test(test_two_layers_are_deterministic),
        cmocka_unit_test(test_transparent_floor_over_opening_uses_darkness),
        cmocka_unit_test(test_production_compositor_exact_rules),
        cmocka_unit_test(test_compositor_threshold_cap_and_transactional_validation),
        cmocka_unit_test(test_compositor_natural_exhaustion_is_valid),
        cmocka_unit_test(test_generated_discontinuity_matches_direct_composition),
        cmocka_unit_test(test_four_transparent_walls_render_cap_result),
        cmocka_unit_test(test_wall_decal_remains_after_optical_geometry),
        cmocka_unit_test(test_light_billboard_remains_after_optical_geometry),
        cmocka_unit_test(test_reflection_mix_exact_math_and_glyph_threshold),
        cmocka_unit_test(test_full_mirror_renders_reflected_wall_and_keeps_frontier),
        cmocka_unit_test(test_partial_mirror_and_reflected_opening_darkness),
        cmocka_unit_test(test_mirror_cache_reuses_column_and_reflected_mirror_is_terminal),
        cmocka_unit_test(test_second_mirror_plane_in_column_is_bounded_darkness)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}