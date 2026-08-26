#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>

#include <cmocka.h>

#include "../src/heightfield_trace.h"
#include "../src/math.h"

#ifndef R9_OPTICAL_RESEARCH
#error "P2 focused runner requires R9_OPTICAL_RESEARCH=1"
#endif

#define FIXTURE_WIDTH 12
#define FIXTURE_HEIGHT 5
#define VIEWPORT_HEIGHT 40

typedef struct {
    Map map;
    MapCell map_cells[FIXTURE_WIDTH * FIXTURE_HEIGHT];
    double light_map[FIXTURE_WIDTH * FIXTURE_HEIGHT];
    SceneAuthoredCell cells[FIXTURE_WIDTH * FIXTURE_HEIGHT];
    SceneHeightView heights;
    Camera camera;
} TraceFixture;

static size_t fixture_index(int x, int y) {
    return (size_t)y * FIXTURE_WIDTH + (size_t)x;
}

static void fixture_init(TraceFixture *fixture) {
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
    for (size_t i = 0U; i < FIXTURE_WIDTH * FIXTURE_HEIGHT; i++) {
        fixture->cells[i].floor_present = true;
        fixture->cells[i].ceiling_present = true;
        fixture->cells[i].floor_height_step = SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
        fixture->cells[i].ceiling_height_step = SCENE_DEFAULT_CEILING_HEIGHT_STEP;
        fixture->cells[i].floor_material = 1U;
        fixture->cells[i].ceiling_material = 2U;
    }
}

static void fixture_wall(TraceFixture *fixture, int x, uint16_t material) {
    size_t index = fixture_index(x, 2);
    fixture->cells[index].occupancy = SCENE_CELL_OCCUPANCY_WALL;
    fixture->cells[index].wall_material = material;
    fixture->map_cells[index].material_id = material;
}

static HeightfieldTraceColumn prepared_column(TraceFixture *fixture) {
    HeightfieldTraceColumn column;
    assert_true(heightfield_trace_prepare_column(
        &column, &fixture->camera, &fixture->map, &fixture->heights,
        1, VIEWPORT_HEIGHT, 0, 20.0));
    return column;
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

static void test_glass_pane_before_wall_is_ordered(void **state) {
    TraceFixture fixture;
    HeightfieldTraceColumn column;
    R9HeightfieldHitList hits;
    (void)state;
    fixture_init(&fixture);
    fixture_wall(&fixture, 3, 10U);
    fixture_wall(&fixture, 6, 20U);
    fixture.cells[fixture_index(6, 2)].floor_present = false;
    column = prepared_column(&fixture);
    assert_true(heightfield_trace_collect(&column, 20, 4U, &hits));
    assert_int_equal(hits.count, 2U);
    assert_false(hits.truncated);
    assert_true(hits.terminal_opening);
    assert_int_equal(hits.hits[0].kind, HEIGHTFIELD_HIT_WALL);
    assert_int_equal(hits.hits[0].material, 10U);
    assert_int_equal(hits.hits[0].map_x, 3);
    assert_int_equal(hits.hits[1].kind, HEIGHTFIELD_HIT_WALL);
    assert_int_equal(hits.hits[1].material, 20U);
    assert_int_equal(hits.hits[1].map_x, 6);
    assert_true(hits.hits[0].distance < hits.hits[1].distance);
}

static void test_height_discontinuities_produce_ordered_planes(void **state) {
    TraceFixture fixture;
    HeightfieldTraceColumn column;
    R9HeightfieldHitList hits;
    (void)state;
    fixture_init(&fixture);
    fixture.cells[fixture_index(2, 2)].floor_height_step = INT16_C(0x0080);
    fixture.cells[fixture_index(3, 2)].floor_height_step = INT16_C(0x0000);
    fixture.cells[fixture_index(4, 2)].floor_height_step = INT16_C(0x0080);
    fixture.cells[fixture_index(5, 2)].floor_height_step = INT16_C(0x0000);
    column = prepared_column(&fixture);
    assert_true(heightfield_trace_collect(&column, 30, 4U, &hits));
    assert_true(hits.count >= 2U);
    for (size_t i = 1U; i < hits.count; i++)
        assert_true(hits.hits[i - 1U].distance <= hits.hits[i].distance);
    assert_true(hits.hits[0].kind == HEIGHTFIELD_HIT_FLOOR);
    assert_true(hits.hits[1].kind == HEIGHTFIELD_HIT_FLOOR);
}

static void test_missing_floor_is_terminal_opening(void **state) {
    TraceFixture fixture;
    HeightfieldTraceColumn column;
    R9HeightfieldHitList hits;
    (void)state;
    fixture_init(&fixture);
    fixture.cells[fixture_index(1, 2)].floor_present = false;
    fixture_wall(&fixture, 4, 9U);
    column = prepared_column(&fixture);
    assert_true(heightfield_trace_collect(&column, 30, 4U, &hits));
    assert_int_equal(hits.count, 0U);
    assert_true(hits.terminal_opening);
    assert_false(hits.truncated);
}

static void test_capacity_four_reports_truncation(void **state) {
    TraceFixture fixture;
    HeightfieldTraceColumn column;
    R9HeightfieldHitList hits;
    (void)state;
    fixture_init(&fixture);
    for (int x = 2; x <= 6; x++) fixture_wall(&fixture, x, (uint16_t)(10 + x));
    column = prepared_column(&fixture);
    assert_true(heightfield_trace_collect(&column, 20, 4U, &hits));
    assert_int_equal(hits.count, 4U);
    assert_true(hits.truncated);
    for (size_t i = 1U; i < hits.count; i++)
        assert_true(hits.hits[i - 1U].distance < hits.hits[i].distance);
}

static void test_n_one_matches_shipping_nearest_hit(void **state) {
    TraceFixture fixture;
    HeightfieldTraceColumn column;
    HeightfieldHit nearest;
    R9HeightfieldHitList hits;
    (void)state;
    fixture_init(&fixture);
    fixture_wall(&fixture, 3, 10U);
    fixture_wall(&fixture, 6, 20U);
    column = prepared_column(&fixture);
    nearest = heightfield_trace_prepared_sample(&column, 20);
    assert_true(heightfield_trace_collect(&column, 20, 1U, &hits));
    assert_int_equal(hits.count, 1U);
    assert_true(hits.truncated);
    assert_hit_equal(&hits.hits[0], &nearest);
}

static void test_invalid_inputs_preserve_output(void **state) {
    TraceFixture fixture;
    HeightfieldTraceColumn column;
    R9HeightfieldHitList sentinel;
    R9HeightfieldHitList output;
    (void)state;
    fixture_init(&fixture);
    column = prepared_column(&fixture);
    memset(&sentinel, 0x5a, sizeof(sentinel));
    output = sentinel;
    assert_false(heightfield_trace_collect(NULL, 20, 1U, &output));
    assert_memory_equal(&output, &sentinel, sizeof(output));
    assert_false(heightfield_trace_collect(&column, -1, 1U, &output));
    assert_false(heightfield_trace_collect(&column, VIEWPORT_HEIGHT, 1U, &output));
    assert_false(heightfield_trace_collect(&column, 20, 0U, &output));
    assert_false(heightfield_trace_collect(
        &column, 20, R9_HEIGHTFIELD_MAX_HITS + 1U, &output));
    assert_false(heightfield_trace_collect(&column, 20, 1U, NULL));
    assert_memory_equal(&output, &sentinel, sizeof(output));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_glass_pane_before_wall_is_ordered),
        cmocka_unit_test(test_height_discontinuities_produce_ordered_planes),
        cmocka_unit_test(test_missing_floor_is_terminal_opening),
        cmocka_unit_test(test_capacity_four_reports_truncation),
        cmocka_unit_test(test_n_one_matches_shipping_nearest_hit),
        cmocka_unit_test(test_invalid_inputs_preserve_output)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}