#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../src/math.h"
#include "../src/vertical_physics.h"

typedef struct {
    Map *map;
    SceneAuthoredCell cells[36];
    SceneHeightView view;
    Camera camera;
    VerticalPhysicsState physics;
} Fixture;

static int setup(void **state) {
    Fixture *fixture = calloc(1U, sizeof(*fixture));
    if (!fixture) return -1;
    fixture->map = map_create(6, 6);
    if (!fixture->map) { free(fixture); return -1; }
    fixture->view = (SceneHeightView){
        fixture->cells, 36U, 6, 6,
        {9.8, SCENE_GRAVITY_DOWN, 0.25, 3.0, 1.0, 0.5, 0.75}
    };
    for (int y = 0; y < 6; y++) {
        for (int x = 0; x < 6; x++) {
            size_t index = (size_t)y * 6U + (size_t)x;
            bool edge = x == 0 || y == 0 || x == 5 || y == 5;
            map_set(fixture->map, x, y, edge ? 1 : 0);
            fixture->cells[index].occupancy = edge
                ? SCENE_CELL_OCCUPANCY_WALL : SCENE_CELL_OCCUPANCY_EMPTY;
            fixture->cells[index].floor_height_step = SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
            fixture->cells[index].ceiling_height_step = SCENE_DEFAULT_CEILING_HEIGHT_STEP;
            fixture->cells[index].floor_present = true;
            fixture->cells[index].ceiling_present = true;
        }
    }
    camera_init(&fixture->camera, 2.5, 2.5, 0.0, PI / 2.0);
    vertical_physics_init(&fixture->physics);
    *state = fixture;
    return 0;
}

static int teardown(void **state) {
    Fixture *fixture = *state;
    map_destroy(fixture->map);
    free(fixture);
    return 0;
}

static SceneAuthoredCell *cell(Fixture *fixture, int x, int y) {
    return &fixture->cells[(size_t)y * 6U + (size_t)x];
}

static void test_reset_and_flat_grounding(void **state) {
    Fixture *fixture = *state;
    fixture->camera.z = 7.0;
    assert_int_equal(vertical_physics_reset(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    assert_true(fixture->physics.initialized);
    assert_true(fixture->physics.grounded);
    assert_float_equal(fixture->camera.z, 0.5, 0.000001);
}

static void test_step_up_and_blocked_step_rolls_back(void **state) {
    Fixture *fixture = *state;
    assert_int_equal(vertical_physics_reset(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    cell(fixture, 3, 2)->floor_height_step = UINT16_C(0x0040);
    cell(fixture, 3, 2)->ceiling_height_step = UINT16_C(0x0140);
    fixture->camera.transform.pos.x = 3.1;
    assert_int_equal(vertical_physics_step(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view,
        2.5, 2.5, 0.0), VERTICAL_PHYSICS_OK);
    assert_float_equal(fixture->camera.z, 0.75, 0.000001);
    assert_true(fixture->physics.grounded);

    cell(fixture, 4, 2)->floor_height_step = UINT16_C(0x00C0);
    cell(fixture, 4, 2)->ceiling_height_step = UINT16_C(0x01C0);
    fixture->camera.transform.pos.x = 4.1;
    assert_int_equal(vertical_physics_step(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view,
        3.1, 2.5, 0.0), VERTICAL_PHYSICS_BLOCKED_STEP);
    assert_float_equal(fixture->camera.transform.pos.x, 3.1, 0.000001);
    assert_float_equal(fixture->camera.z, 0.75, 0.000001);
}

static void test_lower_floor_fall_and_land(void **state) {
    Fixture *fixture = *state;
    cell(fixture, 2, 2)->floor_height_step = UINT16_C(0x0080);
    cell(fixture, 2, 2)->ceiling_height_step = UINT16_C(0x0180);
    cell(fixture, 3, 2)->ceiling_height_step = UINT16_C(0x0180);
    assert_int_equal(vertical_physics_reset(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    assert_float_equal(fixture->camera.z, 1.0, 0.000001);
    fixture->camera.transform.pos.x = 3.1;
    assert_int_equal(vertical_physics_step(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view,
        2.5, 2.5, 0.0), VERTICAL_PHYSICS_OK);
    assert_false(fixture->physics.grounded);
    assert_int_equal(vertical_physics_step(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view,
        3.1, 2.5, 1.0), VERTICAL_PHYSICS_OK);
    assert_true(fixture->physics.grounded);
    assert_float_equal(fixture->camera.z, 0.5, 0.000001);
}

static void test_first_step_validates_from_previous_cell(void **state) {
    Fixture *fixture = *state;
    cell(fixture, 3, 2)->floor_height_step = UINT16_C(0x0080);
    cell(fixture, 3, 2)->ceiling_height_step = UINT16_C(0x0180);
    fixture->camera.transform.pos.x = 3.1;
    assert_false(fixture->physics.initialized);
    assert_int_equal(vertical_physics_step(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view,
        2.5, 2.5, 0.0), VERTICAL_PHYSICS_BLOCKED_STEP);
    assert_float_equal(fixture->camera.transform.pos.x, 2.5, 0.000001);
    assert_float_equal(fixture->camera.z, 0.5, 0.000001);
}

static void test_drop_blocked_when_target_ceiling_cannot_fit_body(void **state) {
    Fixture *fixture = *state;
    cell(fixture, 2, 2)->floor_height_step = UINT16_C(0x0080);
    cell(fixture, 2, 2)->ceiling_height_step = UINT16_C(0x0180);
    assert_int_equal(vertical_physics_reset(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    fixture->camera.transform.pos.x = 3.1;
    assert_int_equal(vertical_physics_step(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view,
        2.5, 2.5, 0.0), VERTICAL_PHYSICS_BLOCKED_CLEARANCE);
    assert_float_equal(fixture->camera.transform.pos.x, 2.5, 0.000001);
    assert_true(fixture->physics.grounded);
}

static void test_clearance_blocks_and_rolls_back(void **state) {
    Fixture *fixture = *state;
    assert_int_equal(vertical_physics_reset(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    cell(fixture, 3, 2)->ceiling_height_step = UINT16_C(0x0080);
    fixture->camera.transform.pos.x = 3.1;
    assert_int_equal(vertical_physics_step(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view,
        2.5, 2.5, 0.0), VERTICAL_PHYSICS_BLOCKED_CLEARANCE);
    assert_float_equal(fixture->camera.transform.pos.x, 2.5, 0.000001);
}

static void test_local_gravity_scale_changes_fall(void **state) {
    Fixture *fixture = *state;
    VerticalPhysicsState full;
    Camera full_camera;
    assert_int_equal(vertical_physics_reset(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    fixture->physics.grounded = false;
    fixture->camera.z = 0.9;
    full = fixture->physics;
    full_camera = fixture->camera;
    assert_int_equal(vertical_physics_step(
        &full, &full_camera, fixture->map, &fixture->view,
        2.5, 2.5, 0.1), VERTICAL_PHYSICS_OK);
    cell(fixture, 2, 2)->gravity_scale_step = UINT16_C(0x0080);
    assert_int_equal(vertical_physics_step(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view,
        2.5, 2.5, 0.1), VERTICAL_PHYSICS_OK);
    assert_true(fixture->camera.z > full_camera.z);
}

static void test_up_and_lateral_gravity_affect_airborne_body(void **state) {
    Fixture *fixture = *state;
    assert_int_equal(vertical_physics_reset(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    fixture->physics.grounded = false;
    fixture->camera.z = 0.7;
    cell(fixture, 2, 2)->gravity_orientation = SCENE_GRAVITY_UP;
    assert_int_equal(vertical_physics_step(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view,
        2.5, 2.5, 0.1), VERTICAL_PHYSICS_OK);
    assert_true(fixture->camera.z > 0.7);
    fixture->physics.velocity_z = 0.0;
    fixture->physics.grounded = false;
    cell(fixture, 2, 2)->gravity_orientation = SCENE_GRAVITY_EAST;
    {
        double old_x = fixture->camera.transform.pos.x;
        assert_int_equal(vertical_physics_step(
            &fixture->physics, &fixture->camera, fixture->map, &fixture->view,
            old_x, 2.5, 0.1), VERTICAL_PHYSICS_OK);
        assert_true(fixture->camera.transform.pos.x > old_x);
    }
}

static void test_substep_determinism_and_invalid_inputs(void **state) {
    Fixture *fixture = *state;
    VerticalPhysicsState a;
    VerticalPhysicsState b;
    Camera camera_a;
    Camera camera_b;
    assert_int_equal(vertical_physics_reset(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    fixture->physics.grounded = false;
    fixture->camera.z = 0.9;
    a = fixture->physics; b = fixture->physics;
    camera_a = fixture->camera; camera_b = fixture->camera;
    assert_int_equal(vertical_physics_step(
        &a, &camera_a, fixture->map, &fixture->view, 2.5, 2.5, 0.1),
        VERTICAL_PHYSICS_OK);
    for (int i = 0; i < 10; i++) {
        assert_int_equal(vertical_physics_step(
            &b, &camera_b, fixture->map, &fixture->view,
            camera_b.transform.pos.x, camera_b.transform.pos.y, 0.01),
            VERTICAL_PHYSICS_OK);
    }
    assert_float_equal(camera_a.z, camera_b.z, 0.000001);
    assert_float_equal(a.velocity_z, b.velocity_z, 0.000001);
    assert_int_equal(vertical_physics_step(
        NULL, &camera_a, fixture->map, &fixture->view, 2.5, 2.5, 0.1),
        VERTICAL_PHYSICS_INVALID_ARGUMENT);
    assert_int_equal(vertical_physics_step(
        &a, &camera_a, fixture->map, &fixture->view, 2.5, 2.5, NAN),
        VERTICAL_PHYSICS_INVALID_ARGUMENT);
}

static void test_jump_impulse_opposes_local_gravity(void **state) {
    Fixture *fixture = *state;
    assert_int_equal(vertical_physics_reset(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    assert_int_equal(vertical_physics_jump(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    assert_float_equal(fixture->physics.velocity_z, 3.0, 0.000001);
    assert_false(fixture->physics.grounded);
    assert_int_equal(vertical_physics_jump(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_NOT_GROUNDED);

    assert_int_equal(vertical_physics_reset(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    cell(fixture, 2, 2)->gravity_orientation = SCENE_GRAVITY_UP;
    assert_int_equal(vertical_physics_jump(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    assert_float_equal(fixture->physics.velocity_z, -3.0, 0.000001);

    assert_int_equal(vertical_physics_reset(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    cell(fixture, 2, 2)->gravity_orientation = SCENE_GRAVITY_EAST;
    assert_int_equal(vertical_physics_jump(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    assert_float_equal(fixture->physics.velocity_x, -3.0, 0.000001);
    assert_float_equal(fixture->physics.velocity_z, 0.0, 0.000001);

    assert_int_equal(vertical_physics_reset(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    cell(fixture, 2, 2)->gravity_orientation = SCENE_GRAVITY_NORTH;
    assert_int_equal(vertical_physics_jump(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    assert_float_equal(fixture->physics.velocity_y, 3.0, 0.000001);
    assert_int_equal(vertical_physics_reset(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    cell(fixture, 2, 2)->gravity_orientation = SCENE_GRAVITY_SOUTH;
    assert_int_equal(vertical_physics_jump(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    assert_float_equal(fixture->physics.velocity_y, -3.0, 0.000001);
    assert_int_equal(vertical_physics_reset(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    cell(fixture, 2, 2)->gravity_orientation = SCENE_GRAVITY_WEST;
    assert_int_equal(vertical_physics_jump(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    assert_float_equal(fixture->physics.velocity_x, 3.0, 0.000001);
}

static void test_low_gravity_jump_is_higher_and_ceiling_caps(void **state) {
    Fixture *fixture = *state;
    Camera normal_camera;
    Camera low_camera;
    VerticalPhysicsState normal;
    VerticalPhysicsState low;
    cell(fixture, 2, 2)->ceiling_height_step = UINT16_C(0x0200);
    assert_int_equal(vertical_physics_reset(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    assert_int_equal(vertical_physics_jump(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    normal_camera = fixture->camera;
    normal = fixture->physics;
    low_camera = fixture->camera;
    low = fixture->physics;
    assert_int_equal(vertical_physics_step(
        &normal, &normal_camera, fixture->map, &fixture->view,
        2.5, 2.5, 0.2), VERTICAL_PHYSICS_OK);
    cell(fixture, 2, 2)->gravity_scale_step = UINT16_C(0x0080);
    assert_int_equal(vertical_physics_step(
        &low, &low_camera, fixture->map, &fixture->view,
        2.5, 2.5, 0.2), VERTICAL_PHYSICS_OK);
    assert_true(low_camera.z > normal_camera.z);

    cell(fixture, 2, 2)->gravity_scale_step = 0U;
    fixture->view.movement.head_clearance = 0.25;
    assert_int_equal(vertical_physics_reset(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    assert_int_equal(vertical_physics_jump(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    assert_int_equal(vertical_physics_step(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view,
        2.5, 2.5, 1.0), VERTICAL_PHYSICS_OK);
    assert_true(fixture->camera.z <= 1.25 + 0.000001);
    assert_true(fixture->physics.velocity_z <= 0.0);
}

static void test_airborne_lateral_entry_requires_body_fit(void **state) {
    Fixture *fixture = *state;
    assert_int_equal(vertical_physics_reset(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    assert_int_equal(vertical_physics_jump(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view),
        VERTICAL_PHYSICS_OK);
    fixture->camera.z = 0.9;
    cell(fixture, 3, 2)->ceiling_height_step = UINT16_C(0x00C0);
    fixture->camera.transform.pos.x = 3.1;
    assert_int_equal(vertical_physics_step(
        &fixture->physics, &fixture->camera, fixture->map, &fixture->view,
        2.5, 2.5, 0.0), VERTICAL_PHYSICS_OK);
    assert_float_equal(fixture->camera.transform.pos.x, 2.5, 0.000001);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_reset_and_flat_grounding, setup, teardown),
        cmocka_unit_test_setup_teardown(test_step_up_and_blocked_step_rolls_back, setup, teardown),
        cmocka_unit_test_setup_teardown(test_lower_floor_fall_and_land, setup, teardown),
        cmocka_unit_test_setup_teardown(test_first_step_validates_from_previous_cell, setup, teardown),
        cmocka_unit_test_setup_teardown(test_drop_blocked_when_target_ceiling_cannot_fit_body, setup, teardown),
        cmocka_unit_test_setup_teardown(test_clearance_blocks_and_rolls_back, setup, teardown),
        cmocka_unit_test_setup_teardown(test_local_gravity_scale_changes_fall, setup, teardown),
        cmocka_unit_test_setup_teardown(test_up_and_lateral_gravity_affect_airborne_body, setup, teardown),
        cmocka_unit_test_setup_teardown(test_substep_determinism_and_invalid_inputs, setup, teardown),
        cmocka_unit_test_setup_teardown(test_jump_impulse_opposes_local_gravity, setup, teardown),
        cmocka_unit_test_setup_teardown(test_low_gravity_jump_is_higher_and_ceiling_caps, setup, teardown),
        cmocka_unit_test_setup_teardown(test_airborne_lateral_entry_requires_body_fit, setup, teardown)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}