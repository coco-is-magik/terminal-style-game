/**
 * test_editor_selection.c — Wall face selection / hit-testing
 *
 * Locks cardinal face calculation against camera yaw convention
 * (0 = east, PI/2 = +Y/south) and validates selection helpers.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <math.h>

#include "../src/editor_selection.h"
#include "../src/camera.h"
#include "../src/map.h"
#include "../src/config.h"
#include "../src/math.h"

#define DOUBLE_EPSILON 0.0001

/* ===================================================================
 *  Fixtures
 * =================================================================== */

static int group_setup(void **state) {
    (void)state;
    config_init_defaults();
    return 0;
}

static Map *make_open_map_with_walls(void) {
    /* 5x5 open interior; walls placed per-test. */
    Map *m = map_create(5, 5);
    return m;
}

static SelectionTarget wall_selection(int x, int y, WallFace face) {
    SelectionTarget t;
    t.type = SELECTION_WALL_FACE;
    t.value.wall_face.map_x = x;
    t.value.wall_face.map_y = y;
    t.value.wall_face.face = face;
    return t;
}

/* ===================================================================
 *  Pure face calculation
 * =================================================================== */

static void test_face_x_positive_is_west(void **state) {
    (void)state;
    assert_int_equal(editor_calculate_wall_face(0, 1.0, 0.0), WALL_FACE_WEST);
}

static void test_face_x_negative_is_east(void **state) {
    (void)state;
    assert_int_equal(editor_calculate_wall_face(0, -1.0, 0.0), WALL_FACE_EAST);
}

static void test_face_y_positive_is_north(void **state) {
    (void)state;
    assert_int_equal(editor_calculate_wall_face(1, 0.0, 1.0), WALL_FACE_NORTH);
}

static void test_face_y_negative_is_south(void **state) {
    (void)state;
    assert_int_equal(editor_calculate_wall_face(1, 0.0, -1.0), WALL_FACE_SOUTH);
}

/* ===================================================================
 *  Raycast selection integration
 * =================================================================== */

static void test_no_hit_is_invalid(void **state) {
    (void)state;
    Map *m = make_open_map_with_walls();
    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);

    EditorHit hit = editor_raycast_selection(&cam, m);
    assert_false(hit.valid);
    assert_int_equal(hit.target.type, SELECTION_NONE);

    map_destroy(m);
}

static void test_ray_east_hits_west_face(void **state) {
    (void)state;
    Map *m = make_open_map_with_walls();
    map_set(m, 4, 2, 1); /* wall east of camera */
    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0); /* facing +X/east */

    EditorHit hit = editor_raycast_selection(&cam, m);
    assert_true(hit.valid);
    assert_int_equal(hit.target.type, SELECTION_WALL_FACE);
    assert_int_equal(hit.target.value.wall_face.map_x, 4);
    assert_int_equal(hit.target.value.wall_face.map_y, 2);
    assert_int_equal(hit.target.value.wall_face.face, WALL_FACE_WEST);
    assert_true(hit.distance > 0.0);

    map_destroy(m);
}

static void test_ray_west_hits_east_face(void **state) {
    (void)state;
    Map *m = make_open_map_with_walls();
    map_set(m, 0, 2, 1); /* wall west of camera */
    Camera cam;
    camera_init(&cam, 2.5, 2.5, PI, PI / 2.0); /* facing -X/west */

    EditorHit hit = editor_raycast_selection(&cam, m);
    assert_true(hit.valid);
    assert_int_equal(hit.target.value.wall_face.map_x, 0);
    assert_int_equal(hit.target.value.wall_face.map_y, 2);
    assert_int_equal(hit.target.value.wall_face.face, WALL_FACE_EAST);

    map_destroy(m);
}

static void test_ray_south_hits_north_face(void **state) {
    (void)state;
    Map *m = make_open_map_with_walls();
    map_set(m, 2, 4, 1); /* wall south (+Y) of camera */
    Camera cam;
    camera_init(&cam, 2.5, 2.5, PI / 2.0, PI / 2.0); /* facing +Y/south */

    EditorHit hit = editor_raycast_selection(&cam, m);
    assert_true(hit.valid);
    assert_int_equal(hit.target.value.wall_face.map_x, 2);
    assert_int_equal(hit.target.value.wall_face.map_y, 4);
    assert_int_equal(hit.target.value.wall_face.face, WALL_FACE_NORTH);

    map_destroy(m);
}

static void test_ray_north_hits_south_face(void **state) {
    (void)state;
    Map *m = make_open_map_with_walls();
    map_set(m, 2, 0, 1); /* wall north (-Y) of camera */
    Camera cam;
    camera_init(&cam, 2.5, 2.5, -PI / 2.0, PI / 2.0); /* facing -Y/north */

    EditorHit hit = editor_raycast_selection(&cam, m);
    assert_true(hit.valid);
    assert_int_equal(hit.target.value.wall_face.map_x, 2);
    assert_int_equal(hit.target.value.wall_face.map_y, 0);
    assert_int_equal(hit.target.value.wall_face.face, WALL_FACE_SOUTH);

    map_destroy(m);
}

static void test_boundary_coordinates_safe(void **state) {
    (void)state;
    Map *m = make_open_map_with_walls();
    /* Wall on map edge; camera near opposite edge. */
    map_set(m, 0, 0, 1);
    Camera cam;
    camera_init(&cam, 0.5, 0.5, PI, PI / 2.0); /* facing west into edge wall */

    EditorHit hit = editor_raycast_selection(&cam, m);
    /* May or may not hit depending on starting cell; must not crash. */
    if (hit.valid) {
        assert_int_equal(hit.target.type, SELECTION_WALL_FACE);
        assert_true(map_in_bounds(m,
            hit.target.value.wall_face.map_x,
            hit.target.value.wall_face.map_y));
    }

    /* Null args are invalid, not crash. */
    hit = editor_raycast_selection(NULL, m);
    assert_false(hit.valid);
    hit = editor_raycast_selection(&cam, NULL);
    assert_false(hit.valid);

    map_destroy(m);
}


static EditorHit no_wall_hit(void) {
    EditorHit hit = {0};
    hit.target.type = SELECTION_NONE;
    return hit;
}

static void test_light_pick_nearest_and_stable_tie(void **state) {
    SceneLight lights[3] = {0};
    Camera cam;
    EditorHit hit;
    (void)state;

    camera_init(&cam, 1.0, 1.0, 0.0, PI / 2.0);
    lights[0].id = 9; lights[0].x = 3.0; lights[0].y = 1.1;
    lights[1].id = 7; lights[1].x = 3.0; lights[1].y = 1.1;
    lights[2].id = 5; lights[2].x = 4.0; lights[2].y = 1.0;

    hit = editor_pick_light_selection(&cam, lights, 3U, no_wall_hit(),
                                      20.0, 0.25);
    assert_true(hit.valid);
    assert_int_equal(hit.target.type, SELECTION_LIGHT);
    assert_int_equal(hit.target.value.light.id, 7);
    assert_float_equal(hit.distance, 2.0, DOUBLE_EPSILON);
}

static void test_light_pick_rejects_invalid_geometry(void **state) {
    SceneLight lights[5] = {0};
    Camera cam;
    EditorHit hit;
    (void)state;

    camera_init(&cam, 1.0, 1.0, 0.0, PI / 2.0);
    lights[0].id = 1; lights[0].x = 0.0; lights[0].y = 1.0; /* behind */
    lights[1].id = 2; lights[1].x = 3.0; lights[1].y = 2.0; /* off ray */
    lights[2].id = 3; lights[2].x = 30.0; lights[2].y = 1.0; /* range */
    lights[3].id = 0; lights[3].x = 2.0; lights[3].y = 1.0; /* invalid ID */
    lights[4].id = 4; lights[4].x = NAN; lights[4].y = 1.0; /* non-finite */

    hit = editor_pick_light_selection(&cam, lights, 5U, no_wall_hit(),
                                      20.0, 0.25);
    assert_false(hit.valid);
    assert_int_equal(hit.target.type, SELECTION_NONE);
}

static void test_light_pick_respects_wall_occlusion_and_fallback(void **state) {
    SceneLight light = {0};
    Camera cam;
    EditorHit wall = no_wall_hit();
    EditorHit hit;
    (void)state;

    camera_init(&cam, 1.0, 1.0, 0.0, PI / 2.0);
    wall.valid = true;
    wall.distance = 2.0;
    wall.target = wall_selection(3, 1, WALL_FACE_WEST);
    light.id = 8;
    light.x = 4.0;
    light.y = 1.0;

    hit = editor_pick_light_selection(&cam, &light, 1U, wall, 20.0, 0.25);
    assert_true(hit.valid);
    assert_int_equal(hit.target.type, SELECTION_WALL_FACE);
    assert_int_equal(hit.target.value.wall_face.map_x, 3);

    light.x = 2.0;
    hit = editor_pick_light_selection(&cam, &light, 1U, wall, 20.0, 0.25);
    assert_int_equal(hit.target.type, SELECTION_LIGHT);
    assert_int_equal(hit.target.value.light.id, 8);
}

static void test_light_pick_invalid_arguments_preserve_wall(void **state) {
    EditorHit wall = no_wall_hit();
    Camera cam;
    (void)state;

    camera_init(&cam, 1.0, 1.0, 0.0, PI / 2.0);
    wall.valid = true;
    wall.distance = 2.0;
    wall.target = wall_selection(3, 1, WALL_FACE_WEST);

    assert_int_equal(editor_pick_light_selection(NULL, NULL, 0U, wall,
                                                 20.0, 0.25).target.type,
                     SELECTION_WALL_FACE);
    assert_int_equal(editor_pick_light_selection(&cam, NULL, 1U, wall,
                                                 20.0, 0.25).target.type,
                     SELECTION_WALL_FACE);
    assert_int_equal(editor_pick_light_selection(&cam, NULL, 0U, wall,
                                                 20.0, 0.0).target.type,
                     SELECTION_WALL_FACE);
}

static void test_light_pick_accepts_expanded_editor_tolerance(void **state) {
    SceneLight light = {.id = 8U, .x = 3.0, .y = 1.45};
    Camera cam;
    EditorHit hit;
    (void)state;
    camera_init(&cam, 1.0, 1.0, 0.0, PI / 2.0);
    hit = editor_pick_light_selection(&cam, &light, 1U, no_wall_hit(),
                                      20.0, 0.50);
    assert_true(hit.valid);
    assert_int_equal(hit.target.type, SELECTION_LIGHT);
    hit = editor_pick_light_selection(&cam, &light, 1U, no_wall_hit(),
                                      20.0, 0.25);
    assert_false(hit.valid);
}

/* ===================================================================
 *  Conversion and validation
 * =================================================================== */

static void test_face_to_material_ref_drops_face(void **state) {
    (void)state;
    WallFaceRef face;
    face.map_x = 3;
    face.map_y = 7;
    face.face = WALL_FACE_SOUTH;

    WallMaterialRef ref = editor_wall_face_to_material_ref(face);
    assert_int_equal(ref.map_x, 3);
    assert_int_equal(ref.map_y, 7);
}

static void test_selection_validation_rejects_oob_and_empty(void **state) {
    (void)state;
    Map *m = make_open_map_with_walls();
    map_set(m, 2, 2, 3);

    SelectionTarget ok = wall_selection(2, 2, WALL_FACE_WEST);
    assert_true(editor_selection_is_valid_for_map(ok, m));

    SelectionTarget oob = wall_selection(-1, 0, WALL_FACE_EAST);
    assert_false(editor_selection_is_valid_for_map(oob, m));

    SelectionTarget oob2 = wall_selection(5, 5, WALL_FACE_NORTH);
    assert_false(editor_selection_is_valid_for_map(oob2, m));

    SelectionTarget empty = wall_selection(1, 1, WALL_FACE_SOUTH);
    assert_false(editor_selection_is_valid_for_map(empty, m));

    SelectionTarget none;
    none.type = SELECTION_NONE;
    assert_false(editor_selection_is_valid_for_map(none, m));

    assert_false(editor_selection_is_valid_for_map(ok, NULL));

    map_destroy(m);
}

/* ===================================================================
 *  Runner
 * =================================================================== */

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_face_x_positive_is_west),
        cmocka_unit_test(test_face_x_negative_is_east),
        cmocka_unit_test(test_face_y_positive_is_north),
        cmocka_unit_test(test_face_y_negative_is_south),
        cmocka_unit_test(test_no_hit_is_invalid),
        cmocka_unit_test(test_ray_east_hits_west_face),
        cmocka_unit_test(test_ray_west_hits_east_face),
        cmocka_unit_test(test_ray_south_hits_north_face),
        cmocka_unit_test(test_ray_north_hits_south_face),
        cmocka_unit_test(test_boundary_coordinates_safe),
        cmocka_unit_test(test_light_pick_nearest_and_stable_tie),
        cmocka_unit_test(test_light_pick_rejects_invalid_geometry),
        cmocka_unit_test(test_light_pick_respects_wall_occlusion_and_fallback),
        cmocka_unit_test(test_light_pick_invalid_arguments_preserve_wall),
        cmocka_unit_test(test_light_pick_accepts_expanded_editor_tolerance),
        cmocka_unit_test(test_face_to_material_ref_drops_face),
        cmocka_unit_test(test_selection_validation_rejects_oob_and_empty),
    };

    return cmocka_run_group_tests(tests, group_setup, NULL);
}
