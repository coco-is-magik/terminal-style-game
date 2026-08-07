/**
 * test_editor_highlight.c — Editor world-outline and crosshair regressions
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <math.h>

#include "../src/config.h"
#include "../src/editor_highlight.h"
#include "../src/math.h"

static int group_setup(void **state) {
    (void)state;
    config_init_defaults();
    return 0;
}

static SelectionTarget wall_target(int x, int y, WallFace face) {
    SelectionTarget target = {0};
    target.type = SELECTION_WALL_FACE;
    target.value.wall_face.map_x = x;
    target.value.wall_face.map_y = y;
    target.value.wall_face.face = face;
    return target;
}

static SelectionTarget light_target(SceneInstanceId id) {
    SelectionTarget target = {0};
    target.type = SELECTION_LIGHT;
    target.value.light.id = id;
    return target;
}

static EditorHit hover_hit(SelectionTarget target) {
    EditorHit hit = {0};
    hit.target = target;
    hit.distance = 1.5;
    hit.valid = true;
    return hit;
}

static void fill_grid(Grid *grid, uint8_t glyph, SDL_Color fg, SDL_Color bg) {
    int x;
    int y;

    for (y = 0; y < grid->height; y++) {
        for (x = 0; x < grid->width; x++) {
            assert_true(grid_set(grid, x, y, glyph, fg, bg));
        }
    }
}

static int count_glyph(const Grid *grid, uint8_t glyph) {
    int count = 0;
    int x;
    int y;

    for (y = 0; y < grid->height; y++) {
        for (x = 0; x < grid->width; x++) {
            const Cell *cell = &grid->cells[y * grid->width + x];
            if (cell->glyph == glyph) {
                count++;
            }
        }
    }
    return count;
}

static Cell first_glyph(const Grid *grid, uint8_t glyph) {
    int x;
    int y;

    for (y = 0; y < grid->height; y++) {
        for (x = 0; x < grid->width; x++) {
            Cell cell = grid->cells[y * grid->width + x];
            if (cell.glyph == glyph) {
                return cell;
            }
        }
    }
    fail_msg("%s", "expected glyph was not rendered");
    return (Cell){0};
}

static void render_cardinal_case(double camera_x, double camera_y,
                                 double angle, int wall_x, int wall_y,
                                 WallFace face) {
    Grid *grid = grid_create(41, 25);
    Map *map = map_create(7, 7);
    Camera camera;
    SelectionTarget selection = wall_target(wall_x, wall_y, face);
    EditorHit hover = {0};
    SDL_Color dark = {0, 0, 0, 255};

    assert_non_null(grid);
    assert_non_null(map);
    map_set(map, wall_x, wall_y, 1);
    camera_init(&camera, camera_x, camera_y, angle, PI / 2.0);
    fill_grid(grid, 'w', dark, dark);

    editor_highlight_render(grid, map, &camera, NULL, 0U, selection, hover);
    assert_true(count_glyph(grid, EDITOR_HIGHLIGHT_SELECTED_GLYPH) > 0);
    assert_true(count_glyph(grid, 'w') > 0);

    map_destroy(map);
    grid_destroy(grid);
}

static void test_selected_outline_all_cardinal_faces(void **state) {
    (void)state;
    render_cardinal_case(2.5, 3.5, 0.0, 5, 3, WALL_FACE_WEST);
    render_cardinal_case(4.5, 3.5, PI, 1, 3, WALL_FACE_EAST);
    render_cardinal_case(3.5, 2.5, PI / 2.0, 3, 5, WALL_FACE_NORTH);
    render_cardinal_case(3.5, 4.5, -PI / 2.0, 3, 1, WALL_FACE_SOUTH);
}

static void test_hover_is_dashed_and_selection_wins_same_target(void **state) {
    Grid *grid = grid_create(41, 25);
    Map *map = map_create(7, 7);
    Camera camera;
    SelectionTarget target = wall_target(5, 3, WALL_FACE_WEST);
    SelectionTarget none = {0};
    EditorHit hover = hover_hit(target);
    SDL_Color dark = {0, 0, 0, 255};

    (void)state;
    assert_non_null(grid);
    assert_non_null(map);
    map_set(map, 5, 3, 1);
    camera_init(&camera, 2.5, 3.5, 0.0, PI / 2.0);
    fill_grid(grid, 'w', dark, dark);

    editor_highlight_render(grid, map, &camera, NULL, 0U, none, hover);
    assert_true(count_glyph(grid, EDITOR_HIGHLIGHT_HOVER_GLYPH) > 0);
    assert_int_equal(count_glyph(grid, EDITOR_HIGHLIGHT_SELECTED_GLYPH), 0);

    fill_grid(grid, 'w', dark, dark);
    editor_highlight_render(grid, map, &camera, NULL, 0U, target, hover);
    assert_true(count_glyph(grid, EDITOR_HIGHLIGHT_SELECTED_GLYPH) > 0);
    assert_int_equal(count_glyph(grid, EDITOR_HIGHLIGHT_HOVER_GLYPH), 0);

    map_destroy(map);
    grid_destroy(grid);
}

static void test_nearer_wall_occludes_target(void **state) {
    Grid *grid = grid_create(41, 25);
    Map *map = map_create(7, 7);
    Camera camera;
    SelectionTarget target = wall_target(5, 3, WALL_FACE_WEST);
    EditorHit hover = {0};
    SDL_Color dark = {0, 0, 0, 255};

    (void)state;
    assert_non_null(grid);
    assert_non_null(map);
    map_set(map, 4, 3, 1);
    map_set(map, 5, 3, 1);
    camera_init(&camera, 2.5, 3.5, 0.0, PI / 2.0);
    fill_grid(grid, 'w', dark, dark);

    editor_highlight_render(grid, map, &camera, NULL, 0U, target, hover);
    assert_int_equal(count_glyph(grid, EDITOR_HIGHLIGHT_SELECTED_GLYPH), 0);

    map_destroy(map);
    grid_destroy(grid);
}

static void test_wrong_face_and_invalid_target_draw_nothing(void **state) {
    Grid *grid = grid_create(41, 25);
    Map *map = map_create(7, 7);
    Camera camera;
    SelectionTarget wrong_face = wall_target(5, 3, WALL_FACE_EAST);
    SelectionTarget empty = wall_target(4, 3, WALL_FACE_WEST);
    EditorHit hover = {0};
    SDL_Color dark = {0, 0, 0, 255};

    (void)state;
    assert_non_null(grid);
    assert_non_null(map);
    map_set(map, 5, 3, 1);
    camera_init(&camera, 2.5, 3.5, 0.0, PI / 2.0);
    fill_grid(grid, 'w', dark, dark);

    editor_highlight_render(grid, map, &camera, NULL, 0U, wrong_face, hover);
    editor_highlight_render(grid, map, &camera, NULL, 0U, empty, hover);
    assert_int_equal(count_glyph(grid, EDITOR_HIGHLIGHT_SELECTED_GLYPH), 0);

    map_destroy(map);
    grid_destroy(grid);
}

static void test_contrast_adapts_to_dark_and_bright_cells(void **state) {
    Grid *grid = grid_create(41, 25);
    Map *map = map_create(7, 7);
    Camera camera;
    SelectionTarget target = wall_target(5, 3, WALL_FACE_WEST);
    EditorHit hover = {0};
    SDL_Color dark = {0, 0, 0, 255};
    SDL_Color bright = {255, 255, 255, 255};
    Cell cell;

    (void)state;
    assert_non_null(grid);
    assert_non_null(map);
    map_set(map, 5, 3, 1);
    camera_init(&camera, 2.5, 3.5, 0.0, PI / 2.0);

    fill_grid(grid, 'w', dark, dark);
    editor_highlight_render(grid, map, &camera, NULL, 0U, target, hover);
    cell = first_glyph(grid, EDITOR_HIGHLIGHT_SELECTED_GLYPH);
    assert_int_equal(cell.fg.r, 255);
    assert_int_equal(cell.bg.r, 0);

    fill_grid(grid, 'w', bright, bright);
    editor_highlight_render(grid, map, &camera, NULL, 0U, target, hover);
    cell = first_glyph(grid, EDITOR_HIGHLIGHT_SELECTED_GLYPH);
    assert_int_equal(cell.fg.r, 0);
    assert_int_equal(cell.bg.r, 255);

    map_destroy(map);
    grid_destroy(grid);
}

static void test_render_does_not_mutate_map_or_interior(void **state) {
    Grid *grid = grid_create(41, 25);
    Map *map = map_create(7, 7);
    Camera camera;
    SelectionTarget target = wall_target(5, 3, WALL_FACE_WEST);
    EditorHit hover = {0};
    SDL_Color dark = {0, 0, 0, 255};
    Cell center_before;
    Cell center_after;

    (void)state;
    assert_non_null(grid);
    assert_non_null(map);
    map_set(map, 5, 3, 7);
    camera_init(&camera, 2.5, 3.5, 0.0, PI / 2.0);
    fill_grid(grid, 'w', dark, dark);
    assert_true(grid_get(grid, grid->width / 2, grid->height / 2,
                         &center_before));

    editor_highlight_render(grid, map, &camera, NULL, 0U, target, hover);

    assert_int_equal(map_get(map, 5, 3)->material_id, 7);
    assert_true(grid_get(grid, grid->width / 2, grid->height / 2,
                         &center_after));
    assert_int_equal(center_after.glyph, center_before.glyph);

    map_destroy(map);
    grid_destroy(grid);
}

static void test_crosshair_is_centered_and_adaptive(void **state) {
    Grid *grid = grid_create(40, 24);
    SDL_Color dark = {0, 0, 0, 255};
    Cell center;

    (void)state;
    assert_non_null(grid);
    fill_grid(grid, 'w', dark, dark);
    editor_crosshair_render(grid);
    assert_true(grid_get(grid, 20, 12, &center));
    assert_int_equal(center.glyph, EDITOR_CROSSHAIR_GLYPH);
    assert_int_equal(center.fg.r, 255);
    assert_int_equal(count_glyph(grid, EDITOR_CROSSHAIR_GLYPH), 1);
    grid_destroy(grid);
}

static void test_wide_grid_has_no_fixed_width_cutoff(void **state) {
    Grid *grid = grid_create(1100, 25);
    Map *map = map_create(7, 7);
    Camera camera;
    SelectionTarget target = wall_target(5, 3, WALL_FACE_WEST);
    EditorHit hover = {0};
    SDL_Color dark = {0, 0, 0, 255};
    int right_half_marks = 0;
    int x;
    int y;

    (void)state;
    assert_non_null(grid);
    assert_non_null(map);
    map_set(map, 5, 3, 1);
    camera_init(&camera, 2.5, 3.5, 0.0, PI / 2.0);
    fill_grid(grid, 'w', dark, dark);

    editor_highlight_render(grid, map, &camera, NULL, 0U, target, hover);
    for (y = 0; y < grid->height; y++) {
        for (x = grid->width / 2; x < grid->width; x++) {
            if (grid->cells[y * grid->width + x].glyph ==
                EDITOR_HIGHLIGHT_SELECTED_GLYPH) {
                right_half_marks++;
            }
        }
    }
    assert_true(right_half_marks > 0);

    map_destroy(map);
    grid_destroy(grid);
}

static void test_extreme_horizon_offsets_clip_safely(void **state) {
    Grid *grid = grid_create(41, 25);
    Map *map = map_create(7, 7);
    Camera camera;
    SelectionTarget target = wall_target(5, 3, WALL_FACE_WEST);
    EditorHit hover = hover_hit(target);
    SDL_Color dark = {0, 0, 0, 255};
    (void)state;

    assert_non_null(grid);
    assert_non_null(map);
    map_set(map, 5, 3, 1);
    camera_init(&camera, 2.5, 3.5, 0.0, PI / 2.0);

    fill_grid(grid, 'w', dark, dark);
    camera.pitch = (double)grid->height;
    editor_highlight_render(grid, map, &camera, NULL, 0U, target, hover);

    fill_grid(grid, 'w', dark, dark);
    camera.pitch = -(double)grid->height;
    editor_highlight_render(grid, map, &camera, NULL, 0U, target, hover);

    map_destroy(map);
    grid_destroy(grid);
}

static void test_light_marker_resolves_stable_id_and_projects(void **state) {
    Grid *grid = grid_create(41, 25);
    Map *map = map_create(7, 7);
    Camera camera;
    SceneLight lights[2] = {
        {.id = 90U, .x = 4.5, .y = 4.5, .radius = 1.0},
        {.id = 11U, .x = 4.5, .y = 3.5, .red = 30U, .green = 80U,
         .blue = 160U, .radius = 1.0}
    };
    SceneLight before[2];
    SelectionTarget selection = light_target(11U);
    EditorHit hover = {0};
    SDL_Color dark = {0, 0, 0, 255};
    Cell marker;
    (void)state;

    assert_non_null(grid);
    assert_non_null(map);
    memcpy(before, lights, sizeof(lights));
    camera_init(&camera, 1.5, 3.5, 0.0, PI / 2.0);
    fill_grid(grid, 'w', dark, dark);
    editor_highlight_render(grid, map, &camera, lights, 2U,
                            selection, hover);
    assert_int_equal(count_glyph(
        grid, EDITOR_LIGHT_HIGHLIGHT_SELECTED_GLYPH), 3);
    assert_true(grid_get(grid, grid->width / 2, grid->height / 2, &marker));
    assert_int_equal(marker.glyph, EDITOR_LIGHT_HIGHLIGHT_SELECTED_GLYPH);
    assert_int_equal(marker.fg.r, 30U);
    assert_int_equal(marker.fg.g, 80U);
    assert_int_equal(marker.fg.b, 160U);
    assert_memory_equal(lights, before, sizeof(lights));

    map_destroy(map);
    grid_destroy(grid);
}

static void test_light_hover_and_selection_precedence(void **state) {
    Grid *grid = grid_create(41, 25);
    Map *map = map_create(7, 7);
    Camera camera;
    SceneLight light = {.id = 11U, .x = 4.5, .y = 3.5, .radius = 1.0};
    SelectionTarget none = {0};
    SelectionTarget target = light_target(11U);
    EditorHit hover = hover_hit(target);
    SDL_Color dark = {0, 0, 0, 255};
    (void)state;

    assert_non_null(grid);
    assert_non_null(map);
    camera_init(&camera, 1.5, 3.5, 0.0, PI / 2.0);
    fill_grid(grid, 'w', dark, dark);
    editor_highlight_render(grid, map, &camera, &light, 1U, none, hover);
    assert_int_equal(count_glyph(grid, EDITOR_LIGHT_HIGHLIGHT_HOVER_GLYPH), 3);

    fill_grid(grid, 'w', dark, dark);
    editor_highlight_render(grid, map, &camera, &light, 1U, target, hover);
    assert_int_equal(count_glyph(
        grid, EDITOR_LIGHT_HIGHLIGHT_SELECTED_GLYPH), 3);
    assert_int_equal(count_glyph(grid, EDITOR_LIGHT_HIGHLIGHT_HOVER_GLYPH), 0);

    map_destroy(map);
    grid_destroy(grid);
}

static void test_light_marker_obeys_wall_occlusion(void **state) {
    Grid *grid = grid_create(41, 25);
    Map *map = map_create(7, 7);
    Camera camera;
    SceneLight light = {.id = 11U, .x = 4.5, .y = 3.5, .radius = 1.0};
    SelectionTarget target = light_target(11U);
    EditorHit hover = {0};
    SDL_Color dark = {0, 0, 0, 255};
    (void)state;

    assert_non_null(grid);
    assert_non_null(map);
    map_set(map, 3, 3, 1);
    camera_init(&camera, 1.5, 3.5, 0.0, PI / 2.0);
    fill_grid(grid, 'w', dark, dark);
    editor_highlight_render(grid, map, &camera, &light, 1U, target, hover);
    assert_int_equal(count_glyph(
        grid, EDITOR_LIGHT_HIGHLIGHT_SELECTED_GLYPH), 0);

    map_destroy(map);
    grid_destroy(grid);
}

static void test_light_marker_rejects_invalid_or_invisible_targets(void **state) {
    Grid *grid = grid_create(41, 25);
    Map *map = map_create(7, 7);
    Camera camera;
    SceneLight lights[2] = {
        {.id = 11U, .x = 0.5, .y = 3.5, .radius = 1.0},
        {.id = 12U, .x = NAN, .y = 3.5, .radius = 1.0}
    };
    EditorHit hover = {0};
    SDL_Color dark = {0, 0, 0, 255};
    (void)state;

    assert_non_null(grid);
    assert_non_null(map);
    camera_init(&camera, 1.5, 3.5, 0.0, PI / 2.0);
    fill_grid(grid, 'w', dark, dark);
    editor_highlight_render(grid, map, &camera, lights, 2U,
                            light_target(99U), hover);
    editor_highlight_render(grid, map, &camera, lights, 2U,
                            light_target(11U), hover);
    editor_highlight_render(grid, map, &camera, lights, 2U,
                            light_target(12U), hover);
    assert_int_equal(count_glyph(
        grid, EDITOR_LIGHT_HIGHLIGHT_SELECTED_GLYPH), 0);

    camera.pitch = (double)grid->height;
    lights[0].x = 4.5;
    editor_highlight_render(grid, map, &camera, lights, 2U,
                            light_target(11U), hover);
    assert_int_equal(count_glyph(
        grid, EDITOR_LIGHT_HIGHLIGHT_SELECTED_GLYPH), 0);

    map_destroy(map);
    grid_destroy(grid);
}

static void test_null_inputs_are_safe(void **state) {
    SelectionTarget none = {0};
    EditorHit hover = {0};

    (void)state;
    editor_highlight_render(NULL, NULL, NULL, NULL, 0U, none, hover);
    editor_crosshair_render(NULL);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_selected_outline_all_cardinal_faces),
        cmocka_unit_test(test_hover_is_dashed_and_selection_wins_same_target),
        cmocka_unit_test(test_nearer_wall_occludes_target),
        cmocka_unit_test(test_wrong_face_and_invalid_target_draw_nothing),
        cmocka_unit_test(test_contrast_adapts_to_dark_and_bright_cells),
        cmocka_unit_test(test_render_does_not_mutate_map_or_interior),
        cmocka_unit_test(test_crosshair_is_centered_and_adaptive),
        cmocka_unit_test(test_wide_grid_has_no_fixed_width_cutoff),
        cmocka_unit_test(test_extreme_horizon_offsets_clip_safely),
        cmocka_unit_test(test_light_marker_resolves_stable_id_and_projects),
        cmocka_unit_test(test_light_hover_and_selection_precedence),
        cmocka_unit_test(test_light_marker_obeys_wall_occlusion),
        cmocka_unit_test(test_light_marker_rejects_invalid_or_invisible_targets),
        cmocka_unit_test(test_null_inputs_are_safe),
    };

    return cmocka_run_group_tests(tests, group_setup, NULL);
}