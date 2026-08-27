#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <cmocka.h>

#include "../src/heightfield_trace.h"
#include "../src/math.h"

#define FIXTURE_WIDTH 12
#define FIXTURE_HEIGHT 5
#define VIEWPORT_HEIGHT 40
#define GENERATION UINT32_C(17)

typedef struct {
    Map map;
    MapCell map_cells[FIXTURE_WIDTH * FIXTURE_HEIGHT];
    LightLevel light_map[FIXTURE_WIDTH * FIXTURE_HEIGHT];
    SceneAuthoredCell cells[FIXTURE_WIDTH * FIXTURE_HEIGHT];
    SceneHeightView heights;
    Camera camera;
} SelectiveFixture;

static size_t fixture_index(int x, int y) {
    return (size_t)y * FIXTURE_WIDTH + (size_t)x;
}

static void fixture_init(SelectiveFixture *fixture) {
    size_t i;
    memset(fixture, 0, sizeof(*fixture));
    fixture->map = (Map){FIXTURE_WIDTH, FIXTURE_HEIGHT,
                         fixture->map_cells, fixture->light_map};
    fixture->heights = (SceneHeightView){
        fixture->cells, FIXTURE_WIDTH * FIXTURE_HEIGHT,
        FIXTURE_WIDTH, FIXTURE_HEIGHT,
        {9.8, SCENE_GRAVITY_DOWN, 0.25, 3.2, 1.0, 0.5, 0.75}
    };
    fixture->camera.transform.pos.x = 1.5;
    fixture->camera.transform.pos.y = 2.5;
    fixture->camera.transform.angle = 0.0;
    fixture->camera.fov = PI / 2.0;
    fixture->camera.z = 0.5;
    for (i = 0U; i < FIXTURE_WIDTH * FIXTURE_HEIGHT; i++) {
        fixture->cells[i].floor_present = true;
        fixture->cells[i].ceiling_present = true;
        fixture->cells[i].floor_height_step = SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
        fixture->cells[i].ceiling_height_step = SCENE_DEFAULT_CEILING_HEIGHT_STEP;
        fixture->cells[i].floor_material = 1U;
        fixture->cells[i].ceiling_material = 2U;
    }
}

static void fixture_wall(SelectiveFixture *fixture, int x, uint16_t material) {
    size_t index = fixture_index(x, 2);
    fixture->cells[index].occupancy = SCENE_CELL_OCCUPANCY_WALL;
    fixture->cells[index].wall_material = material;
    fixture->map_cells[index].material_id = material;
}

static HeightfieldTraceColumn prepare_column(SelectiveFixture *fixture) {
    HeightfieldTraceColumn column;
    assert_true(heightfield_trace_prepare_column(
        &column, &fixture->camera, &fixture->map, &fixture->heights,
        1, VIEWPORT_HEIGHT, 0, 20.0));
    return column;
}

static OpticalExtension transparent_extension(uint8_t transmission) {
    OpticalExtension extension = {0};
    extension.override_mask = OPTICAL_OVERRIDE_RAY_BLOCKS |
                              OPTICAL_OVERRIDE_OPACITY |
                              OPTICAL_OVERRIDE_TRANSMISSION;
    extension.ray_blocks = 0U;
    extension.opacity = 96U;
    extension.transmission = transmission;
    return extension;
}

static void assert_hit_equal(const HeightfieldHit *left,
                             const HeightfieldHit *right) {
    assert_int_equal(left->kind, right->kind);
    assert_float_equal(left->distance, right->distance, 0.0000001);
    assert_float_equal(left->perpendicular_distance,
                       right->perpendicular_distance, 0.0000001);
    assert_float_equal(left->world_x, right->world_x, 0.0000001);
    assert_float_equal(left->world_y, right->world_y, 0.0000001);
    assert_float_equal(left->world_z, right->world_z, 0.0000001);
    assert_int_equal(left->map_x, right->map_x);
    assert_int_equal(left->map_y, right->map_y);
    assert_int_equal(left->side, right->side);
    assert_int_equal(left->material, right->material);
    assert_int_equal(left->generated_boundary, right->generated_boundary);
    assert_int_equal(left->hit, right->hit);
}

static void test_opaque_default_matches_nearest_fast_path(void **state) {
    SelectiveFixture fixture;
    HeightfieldTraceColumn column;
    OpticalRuntimeView view;
    HeightfieldOpticalResult result;
    HeightfieldHit nearest;
    (void)state;
    fixture_init(&fixture);
    fixture_wall(&fixture, 3, 10U);
    fixture_wall(&fixture, 6, 20U);
    column = prepare_column(&fixture);
    nearest = heightfield_trace_prepared_sample(&column, 20);
    assert_true(optical_runtime_view_init(
        &view, fixture.heights.cell_count, NULL, 0U, NULL, 0U, GENERATION));
    assert_true(heightfield_trace_selective(
        &column, &view, GENERATION, 20, &result));
    assert_int_equal(result.count, 1U);
    assert_true(result.terminated_by_surface);
    assert_false(result.reached_opening);
    assert_false(result.layer_cap_exhausted);
    assert_hit_equal(&result.layers[0].hit, &nearest);
    assert_true(result.layers[0].optical.ray_blocks);
    assert_int_equal(result.layers[0].optical.transmission, 0U);
}

static void test_transparent_material_continues_to_opaque_wall(void **state) {
    SelectiveFixture fixture;
    HeightfieldTraceColumn column;
    OpticalExtension materials[21] = {0};
    OpticalRuntimeView view;
    HeightfieldOpticalResult result;
    (void)state;
    fixture_init(&fixture);
    fixture_wall(&fixture, 3, 10U);
    fixture_wall(&fixture, 6, 20U);
    materials[10] = transparent_extension(180U);
    column = prepare_column(&fixture);
    assert_true(optical_runtime_view_init(
        &view, fixture.heights.cell_count, materials, 21U,
        NULL, 0U, GENERATION));
    assert_true(heightfield_trace_selective(
        &column, &view, GENERATION, 20, &result));
    assert_int_equal(result.count, 2U);
    assert_int_equal(result.layers[0].hit.material, 10U);
    assert_int_equal(result.layers[1].hit.material, 20U);
    assert_true(result.layers[0].hit.distance < result.layers[1].hit.distance);
    assert_false(result.layers[0].optical.ray_blocks);
    assert_int_equal(result.layers[0].optical.transmission, 180U);
    assert_true(result.terminated_by_surface);
}

static void test_zero_transmission_stops_nonblocking_surface(void **state) {
    SelectiveFixture fixture;
    HeightfieldTraceColumn column;
    OpticalExtension materials[11] = {0};
    OpticalRuntimeView view;
    HeightfieldOpticalResult result;
    (void)state;
    fixture_init(&fixture);
    fixture_wall(&fixture, 3, 10U);
    fixture_wall(&fixture, 6, 20U);
    materials[10] = transparent_extension(0U);
    column = prepare_column(&fixture);
    assert_true(optical_runtime_view_init(
        &view, fixture.heights.cell_count, materials, 11U,
        NULL, 0U, GENERATION));
    assert_true(heightfield_trace_selective(
        &column, &view, GENERATION, 20, &result));
    assert_int_equal(result.count, 1U);
    assert_false(result.layers[0].optical.ray_blocks);
    assert_true(result.terminated_by_surface);
}

static void test_ray_blocking_stops_positive_transmission(void **state) {
    SelectiveFixture fixture;
    HeightfieldTraceColumn column;
    OpticalExtension materials[11] = {0};
    OpticalRuntimeView view;
    HeightfieldOpticalResult result;
    (void)state;
    fixture_init(&fixture);
    fixture_wall(&fixture, 3, 10U);
    fixture_wall(&fixture, 6, 20U);
    materials[10] = transparent_extension(180U);
    materials[10].ray_blocks = 1U;
    column = prepare_column(&fixture);
    assert_true(optical_runtime_view_init(
        &view, fixture.heights.cell_count, materials, 11U,
        NULL, 0U, GENERATION));
    assert_true(heightfield_trace_selective(
        &column, &view, GENERATION, 20, &result));
    assert_int_equal(result.count, 1U);
    assert_true(result.layers[0].optical.ray_blocks);
    assert_int_equal(result.layers[0].optical.transmission, 180U);
    assert_true(result.terminated_by_surface);
}

static void test_transparent_layers_reach_proven_opening(void **state) {
    SelectiveFixture fixture;
    HeightfieldTraceColumn column;
    OpticalExtension materials[21] = {0};
    OpticalRuntimeView view;
    HeightfieldOpticalResult result;
    (void)state;
    fixture_init(&fixture);
    fixture_wall(&fixture, 3, 10U);
    fixture_wall(&fixture, 6, 20U);
    fixture.cells[fixture_index(6, 2)].floor_present = false;
    materials[10] = transparent_extension(180U);
    materials[20] = transparent_extension(180U);
    column = prepare_column(&fixture);
    assert_true(optical_runtime_view_init(
        &view, fixture.heights.cell_count, materials, 21U,
        NULL, 0U, GENERATION));
    assert_true(heightfield_trace_selective(
        &column, &view, GENERATION, 20, &result));
    assert_int_equal(result.count, 2U);
    assert_true(result.reached_opening);
    assert_false(result.terminated_by_surface);
    assert_false(result.layer_cap_exhausted);
}

static void test_natural_range_exhaustion_is_not_proven_opening(void **state) {
    SelectiveFixture fixture;
    HeightfieldTraceColumn column;
    OpticalExtension materials[4] = {0};
    OpticalRuntimeView view;
    HeightfieldOpticalResult result;
    (void)state;
    fixture_init(&fixture);
    materials[0] = transparent_extension(180U);
    materials[3] = transparent_extension(180U);
    column = prepare_column(&fixture);
    assert_true(optical_runtime_view_init(
        &view, fixture.heights.cell_count, materials, 4U,
        NULL, 0U, GENERATION));
    assert_true(heightfield_trace_selective(
        &column, &view, GENERATION, 20, &result));
    assert_int_equal(result.count, 1U);
    assert_false(result.reached_opening);
    assert_false(result.terminated_by_surface);
    assert_false(result.layer_cap_exhausted);
}

static void test_four_transparent_layers_exhaust_cap(void **state) {
    SelectiveFixture fixture;
    HeightfieldTraceColumn column;
    OpticalExtension materials[16] = {0};
    OpticalRuntimeView view;
    HeightfieldOpticalResult result;
    int x;
    (void)state;
    fixture_init(&fixture);
    for (x = 2; x <= 5; x++) {
        uint16_t material = (uint16_t)(10 + x);
        fixture_wall(&fixture, x, material);
        materials[material] = transparent_extension(180U);
    }
    column = prepare_column(&fixture);
    assert_true(optical_runtime_view_init(
        &view, fixture.heights.cell_count, materials, 16U,
        NULL, 0U, GENERATION));
    assert_true(heightfield_trace_selective(
        &column, &view, GENERATION, 20, &result));
    assert_int_equal(result.count, HEIGHTFIELD_OPTICAL_MAX_LAYERS);
    assert_true(result.layer_cap_exhausted);
    assert_false(result.reached_opening);
    assert_false(result.terminated_by_surface);
    for (size_t i = 1U; i < result.count; i++)
        assert_true(result.layers[i - 1U].hit.distance <
                    result.layers[i].hit.distance);
}

static void test_sparse_owner_cell_override_beats_material(void **state) {
    SelectiveFixture fixture;
    HeightfieldTraceColumn column;
    OpticalExtension materials[11] = {0};
    OpticalCellOverride override = {0};
    OpticalRuntimeView view;
    HeightfieldOpticalResult result;
    (void)state;
    fixture_init(&fixture);
    fixture_wall(&fixture, 3, 10U);
    fixture_wall(&fixture, 6, 10U);
    materials[10] = transparent_extension(180U);
    override.cell_index = (uint32_t)fixture_index(3, 2);
    override.optical.override_mask = OPTICAL_OVERRIDE_RAY_BLOCKS;
    override.optical.ray_blocks = 1U;
    column = prepare_column(&fixture);
    assert_true(optical_runtime_view_init(
        &view, fixture.heights.cell_count, materials, 11U,
        &override, 1U, GENERATION));
    assert_true(heightfield_trace_selective(
        &column, &view, GENERATION, 20, &result));
    assert_int_equal(result.count, 1U);
    assert_int_equal(result.layers[0].hit.map_x, 3);
    assert_true(result.layers[0].optical.ray_blocks);
    assert_int_equal(result.layers[0].optical.transmission, 180U);
    assert_true(result.terminated_by_surface);
}

static void test_invalid_inputs_preserve_output(void **state) {
    SelectiveFixture fixture;
    HeightfieldTraceColumn column;
    OpticalRuntimeView view;
    HeightfieldOpticalResult sentinel;
    HeightfieldOpticalResult output;
    (void)state;
    fixture_init(&fixture);
    fixture_wall(&fixture, 3, 10U);
    column = prepare_column(&fixture);
    assert_true(optical_runtime_view_init(
        &view, fixture.heights.cell_count, NULL, 0U, NULL, 0U, GENERATION));
    memset(&sentinel, 0x5a, sizeof(sentinel));
    output = sentinel;
    assert_false(heightfield_trace_selective(
        NULL, &view, GENERATION, 20, &output));
    assert_false(heightfield_trace_selective(
        &column, &view, GENERATION + 1U, 20, &output));
    assert_false(heightfield_trace_selective(
        &column, &view, GENERATION, -1, &output));
    assert_false(heightfield_trace_selective(
        &column, &view, GENERATION, VIEWPORT_HEIGHT, &output));
    view.cell_count--;
    assert_false(heightfield_trace_selective(
        &column, &view, GENERATION, 20, &output));
    assert_memory_equal(&output, &sentinel, sizeof(output));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_opaque_default_matches_nearest_fast_path),
        cmocka_unit_test(test_transparent_material_continues_to_opaque_wall),
        cmocka_unit_test(test_zero_transmission_stops_nonblocking_surface),
        cmocka_unit_test(test_ray_blocking_stops_positive_transmission),
        cmocka_unit_test(test_transparent_layers_reach_proven_opening),
        cmocka_unit_test(test_natural_range_exhaustion_is_not_proven_opening),
        cmocka_unit_test(test_four_transparent_layers_exhaust_cap),
        cmocka_unit_test(test_sparse_owner_cell_override_beats_material),
        cmocka_unit_test(test_invalid_inputs_preserve_output)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}