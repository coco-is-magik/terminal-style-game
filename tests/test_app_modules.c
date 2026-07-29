#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include "../src/frame_dispatch.h"
#include "../src/menu_controller.h"

static void test_menu_actions(void **state) {
    (void)state;
    assert_int_equal(menu_controller_parse_action("start_game"), MENU_ACTION_START_GAME);
    assert_int_equal(menu_controller_parse_action("discard_changes"), MENU_ACTION_DISCARD_CHANGES);
    assert_int_equal(menu_controller_parse_action(NULL), MENU_ACTION_UNKNOWN);
    assert_int_equal(menu_controller_parse_action("other"), MENU_ACTION_UNKNOWN);
}

static void test_handled_menu_action_consumes_confirm(void **state) {
    bool confirm_pressed = true;
    bool editor_confirm_pressed = true;
    (void)state;

    menu_controller_consume_confirm(&confirm_pressed, &editor_confirm_pressed,
                                    true);
    assert_false(confirm_pressed);
    assert_false(editor_confirm_pressed);

    confirm_pressed = true;
    editor_confirm_pressed = true;
    menu_controller_consume_confirm(&confirm_pressed, &editor_confirm_pressed,
                                    false);
    assert_true(confirm_pressed);
    assert_true(editor_confirm_pressed);

    menu_controller_consume_confirm(NULL, NULL, true);
}

static void test_scenario_dispatch(void **state) {
    (void)state;
    Grid *grid = grid_create(2, 2);
    Camera camera;
    assert_non_null(grid);
    camera_init(&camera, 1.0, 1.0, 0.0, 1.0);
    frame_dispatch_apply_scenario(grid, &camera, "camera", 63);
    assert_float_equal(camera.transform.angle, 0.0, 0.00001);
    frame_dispatch_apply_scenario(grid, &camera, "camera", 64);
    assert_float_equal(camera.transform.angle, 0.01047, 0.00001);
    frame_dispatch_apply_scenario(grid, &camera, "fullchange", 65);
    Cell cell;
    assert_true(grid_get(grid, 0, 0, &cell));
    assert_int_equal(cell.bg.r, 0xFF);
    grid_destroy(grid);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_menu_actions),
        cmocka_unit_test(test_handled_menu_action_consumes_confirm),
        cmocka_unit_test(test_scenario_dispatch),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}