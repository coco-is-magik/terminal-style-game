#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>

#include <cmocka.h>

#include "../src/r9_mirror_trace.h"
#include "../src/math.h"

#ifndef R9_OPTICAL_RESEARCH
#error "P4 focused runner requires R9_OPTICAL_RESEARCH=1"
#endif

#define FIXTURE_WIDTH 9
#define FIXTURE_HEIGHT 7
#define VIEWPORT_HEIGHT 40

typedef struct {
    Map map;
    MapCell map_cells[FIXTURE_WIDTH * FIXTURE_HEIGHT];
    LightLevel light_map[FIXTURE_WIDTH * FIXTURE_HEIGHT];
    SceneAuthoredCell cells[FIXTURE_WIDTH * FIXTURE_HEIGHT];
    SceneHeightView heights;
    Camera camera;
} MirrorFixture;

static size_t fixture_index(int x, int y) {
    return (size_t)y * FIXTURE_WIDTH + (size_t)x;
}

static void fixture_init(MirrorFixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));
    fixture->map = (Map){FIXTURE_WIDTH, FIXTURE_HEIGHT,
                         fixture->map_cells, fixture->light_map};
    fixture->heights = (SceneHeightView){
        fixture->cells, FIXTURE_WIDTH * FIXTURE_HEIGHT,
        FIXTURE_WIDTH, FIXTURE_HEIGHT,
        {9.8, SCENE_GRAVITY_DOWN, 0.25, 3.2, 1.0, 0.5, 0.75}
    };
    fixture->camera.transform.pos = (Vec2){3.5, 3.5};
    fixture->camera.transform.angle = 0.0;
    fixture->camera.fov = PI / 2.0;
    fixture->camera.pitch = 0.0;
    fixture->camera.z = 0.5;
    for (size_t i = 0U; i < FIXTURE_WIDTH * FIXTURE_HEIGHT; i++) {
        fixture->cells[i].floor_present = true;
        fixture->cells[i].ceiling_present = true;
        fixture->cells[i].floor_height_step = SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
        fixture->cells[i].ceiling_height_step = SCENE_DEFAULT_CEILING_HEIGHT_STEP;
        fixture->cells[i].floor_material = 1U;
        fixture->cells[i].ceiling_material = 2U;
    }
}

static void fixture_wall(MirrorFixture *fixture, int x, int y,
                         uint16_t material) {
    size_t index = fixture_index(x, y);
    fixture->cells[index].occupancy = SCENE_CELL_OCCUPANCY_WALL;
    fixture->cells[index].wall_material = material;
    fixture->map_cells[index].material_id = material;
}

static HeightfieldTraceColumn prepare_incoming(MirrorFixture *fixture) {
    HeightfieldTraceColumn column;
    assert_true(heightfield_trace_prepare_column(
        &column, &fixture->camera, &fixture->map, &fixture->heights,
        1, VIEWPORT_HEIGHT, 0, 20.0));
    return column;
}

static R9OpticalResolved mirror_optical(void) {
    return (R9OpticalResolved){true, true, true, 255U, 0U, 255U};
}

static void test_reflect_direction_cardinal_and_oblique(void **state) {
    double x;
    double y;
    double inv_sqrt_two = 0.7071067811865475;
    (void)state;
    assert_true(r9_mirror_reflect_direction(1.0, 0.0, 0, &x, &y));
    assert_float_equal(x, -1.0, 0.0000001);
    assert_float_equal(y, 0.0, 0.0000001);
    assert_true(r9_mirror_reflect_direction(0.0, 1.0, 1, &x, &y));
    assert_float_equal(x, 0.0, 0.0000001);
    assert_float_equal(y, -1.0, 0.0000001);
    assert_true(r9_mirror_reflect_direction(2.0, 2.0, 0, &x, &y));
    assert_float_equal(x, -inv_sqrt_two, 0.0000001);
    assert_float_equal(y, inv_sqrt_two, 0.0000001);
    assert_true(r9_mirror_reflect_direction(2.0, 2.0, 1, &x, &y));
    assert_float_equal(x, inv_sqrt_two, 0.0000001);
    assert_float_equal(y, -inv_sqrt_two, 0.0000001);
}

static void test_reflected_wall_hit(void **state) {
    MirrorFixture fixture;
    HeightfieldTraceColumn incoming;
    HeightfieldHit mirror;
    R9OpticalResolved optical = mirror_optical();
    R9MirrorSample result;
    (void)state;
    fixture_init(&fixture);
    fixture_wall(&fixture, 5, 3, 10U);
    fixture_wall(&fixture, 1, 3, 20U);
    incoming = prepare_incoming(&fixture);
    mirror = heightfield_trace_prepared_sample(&incoming, 20);
    assert_true(mirror.hit);
    assert_int_equal(mirror.material, 10U);
    assert_int_equal(mirror.side, 0);
    assert_true(r9_mirror_sample_once(
        &incoming, &mirror, &optical, 20, 20.0, &result));
    assert_int_equal(result.kind, R9_MIRROR_RESULT_REFLECTED_HIT);
    assert_int_equal(result.reflected_hit.kind, HEIGHTFIELD_HIT_WALL);
    assert_int_equal(result.reflected_hit.material, 20U);
    assert_int_equal(result.reflected_hit.map_x, 1);
    assert_float_equal(result.reflected_direction_x, -1.0, 0.0000001);
    assert_float_equal(result.reflected_direction_y, 0.0, 0.0000001);
    assert_int_equal(result.bounce_count, 1U);
}

static void test_reflected_opening_uses_darkness_fallback(void **state) {
    MirrorFixture fixture;
    HeightfieldTraceColumn incoming;
    HeightfieldHit mirror;
    R9OpticalResolved optical = mirror_optical();
    R9MirrorSample result;
    (void)state;
    fixture_init(&fixture);
    fixture_wall(&fixture, 5, 3, 10U);
    incoming = prepare_incoming(&fixture);
    mirror = heightfield_trace_prepared_sample(&incoming, 30);
    assert_true(mirror.hit);
    fixture.cells[fixture_index(4, 3)].floor_present = false;
    assert_true(r9_mirror_sample_once(
        &incoming, &mirror, &optical, 30, 20.0, &result));
    assert_int_equal(result.kind, R9_MIRROR_RESULT_DARKNESS_FALLBACK);
    assert_false(result.reflected_hit.hit);
    assert_int_equal(result.bounce_count, 1U);
}

static void test_reflected_mirror_is_terminal_no_recursion(void **state) {
    MirrorFixture fixture;
    HeightfieldTraceColumn incoming;
    HeightfieldHit mirror;
    R9OpticalResolved optical = mirror_optical();
    R9MirrorSample result;
    (void)state;
    fixture_init(&fixture);
    fixture_wall(&fixture, 5, 3, 10U);
    fixture_wall(&fixture, 1, 3, 10U);
    incoming = prepare_incoming(&fixture);
    mirror = heightfield_trace_prepared_sample(&incoming, 20);
    assert_true(r9_mirror_sample_once(
        &incoming, &mirror, &optical, 20, 20.0, &result));
    assert_int_equal(result.kind, R9_MIRROR_RESULT_REFLECTED_HIT);
    assert_int_equal(result.reflected_hit.material, 10U);
    assert_int_equal(result.bounce_count, R9_MIRROR_MAX_BOUNCES);
}

static void test_non_wall_and_zero_reflectivity_reject_transactionally(void **state) {
    MirrorFixture fixture;
    HeightfieldTraceColumn incoming;
    HeightfieldHit mirror;
    R9OpticalResolved optical = mirror_optical();
    R9MirrorSample sentinel;
    R9MirrorSample output;
    (void)state;
    fixture_init(&fixture);
    fixture_wall(&fixture, 5, 3, 10U);
    incoming = prepare_incoming(&fixture);
    mirror = heightfield_trace_prepared_sample(&incoming, 20);
    memset(&sentinel, 0x5a, sizeof(sentinel));
    output = sentinel;
    mirror.kind = HEIGHTFIELD_HIT_FLOOR;
    assert_false(r9_mirror_sample_once(
        &incoming, &mirror, &optical, 20, 20.0, &output));
    assert_memory_equal(&output, &sentinel, sizeof(output));
    mirror.kind = HEIGHTFIELD_HIT_WALL;
    optical.reflectivity = 0U;
    assert_false(r9_mirror_sample_once(
        &incoming, &mirror, &optical, 20, 20.0, &output));
    assert_memory_equal(&output, &sentinel, sizeof(output));
}

static void test_invalid_inputs_reject(void **state) {
    double x = 3.0;
    double y = 4.0;
    (void)state;
    assert_false(r9_mirror_reflect_direction(0.0, 0.0, 0, &x, &y));
    assert_false(r9_mirror_reflect_direction(1.0, 0.0, -1, &x, &y));
    assert_false(r9_mirror_reflect_direction(1.0, 0.0, 2, &x, &y));
    assert_false(r9_mirror_reflect_direction(1.0, 0.0, 0, NULL, &y));
    assert_float_equal(x, 3.0, 0.0000001);
    assert_float_equal(y, 4.0, 0.0000001);
    assert_false(r9_mirror_sample_once(NULL, NULL, NULL, 0, 1.0, NULL));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_reflect_direction_cardinal_and_oblique),
        cmocka_unit_test(test_reflected_wall_hit),
        cmocka_unit_test(test_reflected_opening_uses_darkness_fallback),
        cmocka_unit_test(test_reflected_mirror_is_terminal_no_recursion),
        cmocka_unit_test(test_non_wall_and_zero_reflectivity_reject_transactionally),
        cmocka_unit_test(test_invalid_inputs_reject)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}