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
#include "../src/heightfield_trace.h"
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

static void test_horizontal_pick_floor_ceiling_and_cardinal_directions(void **state) {
    Map *map = map_create(12, 12);
    const double angles[] = {0.0, PI / 2.0, PI, -PI / 2.0};
    const int floor_x[] = {7, 5, 3, 5};
    const int floor_y[] = {5, 7, 5, 3};
    Camera camera;
    EditorHit hit;
    size_t i;
    (void)state;
    assert_non_null(map);
    for (i = 0U; i < 4U; i++) {
        camera_init(&camera, 5.5, 5.5, angles[i], PI / 2.0);
        camera.pitch = -10.0;
        hit = editor_pick_horizontal_surface_selection(
            &camera, map, no_wall_hit(), 40, 20.0);
        assert_true(hit.valid);
        assert_int_equal(hit.target.type, SELECTION_FLOOR);
        assert_int_equal(hit.target.value.horizontal.map_x, floor_x[i]);
        assert_int_equal(hit.target.value.horizontal.map_y, floor_y[i]);
        camera.pitch = 10.0;
        hit = editor_pick_horizontal_surface_selection(
            &camera, map, no_wall_hit(), 40, 20.0);
        assert_true(hit.valid);
        assert_int_equal(hit.target.type, SELECTION_CEILING);
        assert_int_equal(hit.target.value.horizontal.map_x, floor_x[i]);
        assert_int_equal(hit.target.value.horizontal.map_y, floor_y[i]);
    }
    map_destroy(map);
}

static void test_height_aware_horizontal_projection_uses_authored_plane(void **state) {
    Map *map = map_create(12, 12);
    SceneAuthoredCell cells[12U * 12U] = {0};
    SceneHeightView heights = {
        cells, 12U * 12U, 12, 12,
        {9.8, SCENE_GRAVITY_DOWN, 0.25, 3.0, 1.0, 0.5, 0.75}
    };
    Camera camera;
    EditorHit flat;
    EditorHit raised;
    size_t raised_index;
    (void)state;
    assert_non_null(map);
    camera_init(&camera, 5.5, 5.5, 0.0, PI / 2.0);
    camera.pitch = -10.0;
    for (size_t i = 0U; i < 12U * 12U; i++) {
        cells[i].floor_height_step = SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
        cells[i].ceiling_height_step = SCENE_DEFAULT_CEILING_HEIGHT_STEP;
        cells[i].floor_present = true;
        cells[i].ceiling_present = true;
    }
    flat = editor_pick_horizontal_surface_selection_height(
        &camera, map, &heights, no_wall_hit(), 40, 20.0);
    assert_true(flat.valid);
    assert_int_equal(flat.target.type, SELECTION_FLOOR);
    raised_index = (size_t)flat.target.value.horizontal.map_y * 12U +
                   (size_t)flat.target.value.horizontal.map_x;
    cells[raised_index].floor_height_step = UINT16_C(0x0040);
    raised = editor_pick_horizontal_surface_selection_height(
        &camera, map, &heights, no_wall_hit(), 40, 20.0);
    assert_true(raised.valid);
    assert_true(raised.distance < flat.distance);
    assert_int_equal(raised.target.value.horizontal.map_x,
                     flat.target.value.horizontal.map_x);
    assert_int_equal(raised.target.value.horizontal.map_y,
                     flat.target.value.horizontal.map_y);
    map_destroy(map);
}

static void test_removed_surfaces_open_to_darkness_and_are_not_pickable(void **state) {
    Map *map = map_create(12, 12);
    SceneAuthoredCell cells[12U * 12U] = {0};
    SceneHeightView heights = {
        cells, 12U * 12U, 12, 12,
        {9.8, SCENE_GRAVITY_DOWN, 0.25, 3.0, 1.0, 0.5, 0.75}
    };
    Camera camera;
    EditorHit hit;
    (void)state;
    assert_non_null(map);
    for (size_t i = 0U; i < 12U * 12U; i++) {
        cells[i].floor_height_step = SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
        cells[i].ceiling_height_step = SCENE_DEFAULT_CEILING_HEIGHT_STEP;
        cells[i].ceiling_present = true;
    }
    camera_init(&camera, 5.5, 5.5, 0.0, PI / 2.0);
    camera.pitch = -10.0;
    hit = editor_pick_horizontal_surface_selection_height(
        &camera, map, &heights, no_wall_hit(), 40, 20.0);
    assert_false(hit.valid);
    for (size_t i = 0U; i < 12U * 12U; i++) {
        cells[i].floor_present = true;
        cells[i].ceiling_present = false;
    }
    camera.pitch = 10.0;
    hit = editor_pick_horizontal_surface_selection_height(
        &camera, map, &heights, no_wall_hit(), 40, 20.0);
    assert_false(hit.valid);
    map_destroy(map);
}

static void test_wall_face_closes_lower_floor_and_raised_ceiling_gaps(void **state) {
    Map *map = map_create(7, 5);
    SceneAuthoredCell cells[35] = {0};
    SceneHeightView heights = {
        cells, 35U, 7, 5,
        {9.8, SCENE_GRAVITY_DOWN, 0.25, 3.2, 1.0, 0.5, 0.75}
    };
    Camera camera;
    HeightfieldHit lower;
    HeightfieldHit upper;
    (void)state;
    assert_non_null(map);
    for (size_t i = 0U; i < 35U; i++) {
        cells[i].floor_present = true;
        cells[i].ceiling_present = true;
        cells[i].floor_height_step = SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
        cells[i].ceiling_height_step = SCENE_DEFAULT_CEILING_HEIGHT_STEP;
        cells[i].floor_material = 1U;
        cells[i].ceiling_material = 1U;
    }
    cells[2U * 7U + 3U].floor_height_step = INT16_C(-0x0080);
    cells[2U * 7U + 3U].ceiling_height_step = INT16_C(0x0180);
    cells[2U * 7U + 4U].occupancy = SCENE_CELL_OCCUPANCY_WALL;
    cells[2U * 7U + 4U].wall_material = 2U;
    map_set(map, 4, 2, 2);
    camera_init(&camera, 2.5, 2.5, 0.0, PI / 2.0);
    lower = heightfield_trace_screen_sample(
        &camera, map, &heights, 1, 40, 0, 39, 20.0);
    upper = heightfield_trace_screen_sample(
        &camera, map, &heights, 1, 40, 0, 0, 20.0);
    assert_true(lower.hit);
    assert_int_equal(lower.kind, HEIGHTFIELD_HIT_WALL);
    assert_int_equal(lower.material, 2U);
    assert_true(upper.hit);
    assert_int_equal(upper.kind, HEIGHTFIELD_HIT_WALL);
    assert_int_equal(upper.material, 2U);
    map_destroy(map);
}

static void test_horizontal_pick_horizon_bounds_range_and_invalid(void **state) {
    Map *map = map_create(5, 5);
    Camera camera;
    EditorHit existing = no_wall_hit();
    EditorHit hit;
    (void)state;
    assert_non_null(map);
    camera_init(&camera, 2.5, 2.5, 0.0, PI / 2.0);
    camera.pitch = 0.0;
    assert_false(editor_pick_horizontal_surface_selection(
        &camera, map, existing, 25, 20.0).valid);
    camera.pitch = -0.0000001;
    assert_false(editor_pick_horizontal_surface_selection(
        &camera, map, existing, 25, 20.0).valid);
    camera.pitch = -1.0;
    assert_false(editor_pick_horizontal_surface_selection(
        &camera, map, existing, 25, 2.0).valid);
    camera.pitch = -10.0;
    camera.transform.pos.x = 4.8;
    assert_false(editor_pick_horizontal_surface_selection(
        &camera, map, existing, 40, 20.0).valid);
    hit = editor_pick_horizontal_surface_selection(NULL, map, existing, 40, 20.0);
    assert_false(hit.valid);
    hit = editor_pick_horizontal_surface_selection(&camera, NULL, existing, 40, 20.0);
    assert_false(hit.valid);
    hit = editor_pick_horizontal_surface_selection(&camera, map, existing, 0, 20.0);
    assert_false(hit.valid);
    camera.transform.pos.x = NAN;
    hit = editor_pick_horizontal_surface_selection(&camera, map, existing, 40, 20.0);
    assert_false(hit.valid);
    map_destroy(map);
}

static void test_horizontal_pick_occlusion_and_precedence(void **state) {
    Map *map = map_create(10, 5);
    Camera camera;
    EditorHit existing = no_wall_hit();
    EditorHit hit;
    (void)state;
    assert_non_null(map);
    camera_init(&camera, 1.5, 2.5, 0.0, PI / 2.0);
    camera.pitch = -10.0;
    hit = editor_pick_horizontal_surface_selection(
        &camera, map, existing, 40, 20.0);
    assert_true(hit.valid);
    assert_int_equal(hit.target.type, SELECTION_FLOOR);
    assert_float_equal(hit.distance, 2.0, DOUBLE_EPSILON);

    map_set(map, 3, 2, 1);
    existing = editor_raycast_selection(&camera, map);
    hit = editor_pick_horizontal_surface_selection(
        &camera, map, existing, 40, 20.0);
    assert_int_equal(hit.target.type, SELECTION_WALL_FACE);

    existing = no_wall_hit();
    existing.valid = true;
    existing.distance = 1.0;
    existing.target.type = SELECTION_LIGHT;
    existing.target.value.light.id = 7U;
    hit = editor_pick_horizontal_surface_selection(
        &camera, map, existing, 40, 20.0);
    assert_int_equal(hit.target.type, SELECTION_LIGHT);
    existing.distance = 3.0;
    hit = editor_pick_horizontal_surface_selection(
        &camera, map, existing, 40, 20.0);
    assert_int_equal(hit.target.type, SELECTION_FLOOR);
    map_destroy(map);
}

static void test_horizontal_selection_validation(void **state) {
    Map *map = map_create(3, 3);
    SelectionTarget target = {0};
    (void)state;
    assert_non_null(map);
    target.type = SELECTION_FLOOR;
    target.value.horizontal = (HorizontalSurfaceRef){1, 2};
    assert_true(editor_selection_is_valid_for_map(target, map));
    target.type = SELECTION_CEILING;
    assert_true(editor_selection_is_valid_for_map(target, map));
    target.value.horizontal.map_x = 3;
    assert_false(editor_selection_is_valid_for_map(target, map));
    map_destroy(map);
}

static void test_selection_set_primary_capacity_and_revalidate(void **state) {
    EditorSelectionSet set;
    Map *map = map_create(10, 2);
    SelectionTarget target;
    size_t i;
    (void)state;
    assert_non_null(map);
    for (i = 0U; i < 9U; i++) map_set(map, (int)i, 0, 1);
    target = wall_selection(0, 0, WALL_FACE_NORTH);
    assert_true(editor_selection_set_reset(&set, target));
    for (i = 1U; i < EDITOR_SELECTION_SET_CAPACITY; i++) {
        target = wall_selection((int)i, 0, WALL_FACE_NORTH);
        assert_true(editor_selection_set_add(&set, target));
    }
    assert_int_equal(set.count, EDITOR_SELECTION_SET_CAPACITY);
    assert_int_equal(editor_selection_set_primary(&set)->value.wall_face.map_x, 7);
    assert_false(editor_selection_set_add(
        &set, wall_selection(8, 0, WALL_FACE_NORTH)));
    assert_true(editor_selection_set_add(
        &set, wall_selection(3, 0, WALL_FACE_NORTH)));
    assert_int_equal(set.count, EDITOR_SELECTION_SET_CAPACITY);
    assert_int_equal(editor_selection_set_primary(&set)->value.wall_face.map_x, 3);
    map_set(map, 3, 0, 0);
    editor_selection_set_revalidate(&set, map);
    assert_int_equal(set.count, EDITOR_SELECTION_SET_CAPACITY - 1U);
    assert_non_null(editor_selection_set_primary(&set));
    assert_int_not_equal(editor_selection_set_primary(&set)->value.wall_face.map_x, 3);
    set.primary_index = set.count - 1U;
    map_set(map, set.members[set.primary_index].value.wall_face.map_x, 0, 0);
    editor_selection_set_revalidate(&set, map);
    assert_int_equal(set.count, EDITOR_SELECTION_SET_CAPACITY - 2U);
    assert_int_equal(editor_selection_set_primary(&set)->value.wall_face.map_x,
                     set.members[set.count - 1U].value.wall_face.map_x);
    map_destroy(map);
}

static void test_trigger_pick_nearest_and_respects_existing_hit(void **state) {
    Camera camera;
    SceneTrigger triggers[2] = {
        {.id = 9U, .min_x = 3.0, .min_y = 2.0, .max_x = 4.0, .max_y = 3.0},
        {.id = 8U, .min_x = 1.0, .min_y = 2.0, .max_x = 2.0, .max_y = 3.0}
    };
    EditorHit hit = {0};
    (void)state;
    camera_init(&camera, 0.5, 2.5, 0.0, PI / 2.0);
    hit = editor_pick_trigger_selection(&camera, triggers, 2U, hit, 10.0);
    assert_true(hit.valid);
    assert_int_equal(hit.target.type, SELECTION_TRIGGER);
    assert_int_equal(hit.target.value.trigger.id, 8U);
    hit.distance = 0.25;
    hit.target.type = SELECTION_WALL_FACE;
    hit = editor_pick_trigger_selection(&camera, triggers, 2U, hit, 10.0);
    assert_int_equal(hit.target.type, SELECTION_WALL_FACE);
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
        cmocka_unit_test(test_horizontal_pick_floor_ceiling_and_cardinal_directions),
        cmocka_unit_test(test_height_aware_horizontal_projection_uses_authored_plane),
        cmocka_unit_test(test_removed_surfaces_open_to_darkness_and_are_not_pickable),
        cmocka_unit_test(test_wall_face_closes_lower_floor_and_raised_ceiling_gaps),
        cmocka_unit_test(test_horizontal_pick_horizon_bounds_range_and_invalid),
        cmocka_unit_test(test_horizontal_pick_occlusion_and_precedence),
        cmocka_unit_test(test_horizontal_selection_validation),
        cmocka_unit_test(test_selection_set_primary_capacity_and_revalidate),
        cmocka_unit_test(test_trigger_pick_nearest_and_respects_existing_hit),
    };

    return cmocka_run_group_tests(tests, group_setup, NULL);
}
