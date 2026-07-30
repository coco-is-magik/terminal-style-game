#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <math.h>
#include <string.h>
#include <cmocka.h>

#include "../src/camera.h"
#include "../src/math.h"

static void test_clamp_policy(void **state) {
    (void)state;
    assert_float_equal(camera_clamp_horizon_offset(39.5, 40), 39.5, 0.0001);
    assert_float_equal(camera_clamp_horizon_offset(41.0, 40), 40.0, 0.0001);
    assert_float_equal(camera_clamp_horizon_offset(-41.0, 40), -40.0, 0.0001);
    assert_float_equal(camera_clamp_horizon_offset(999.0, 160), 160.0, 0.0001);
    assert_float_equal(camera_clamp_horizon_offset(-999.0, 240), -240.0, 0.0001);
}

static void test_invalid_policy_inputs_level_view(void **state) {
    (void)state;
    assert_float_equal(camera_clamp_horizon_offset(10.0, 0), 0.0, 0.0001);
    assert_float_equal(camera_clamp_horizon_offset(10.0, -1), 0.0, 0.0001);
    assert_float_equal(camera_clamp_horizon_offset(NAN, 160), 0.0, 0.0001);
    assert_float_equal(camera_clamp_horizon_offset(INFINITY, 160), 0.0, 0.0001);
}

static void test_update_preserves_direction_sensitivity_and_clamps(void **state) {
    Map *map = map_create(5, 5);
    Camera camera;
    InputState input;
    (void)state;
    assert_non_null(map);
    camera_init(&camera, 2.5, 2.5, 0.0, PI / 2.0);

    memset(&input, 0, sizeof(input));
    input.mouse_dy = 20.0f;
    camera_update(&camera, map, &input, 0.0, 40);
    assert_float_equal(camera.pitch, -10.0, 0.0001);

    input.mouse_dy = -200.0f;
    camera_update(&camera, map, &input, 0.0, 40);
    assert_float_equal(camera.pitch, 40.0, 0.0001);
    camera_update(&camera, map, &input, 0.0, 40);
    assert_float_equal(camera.pitch, 40.0, 0.0001);

    input.mouse_dy = 400.0f;
    camera_update(&camera, map, &input, 0.0, 40);
    assert_float_equal(camera.pitch, -40.0, 0.0001);

    map_destroy(map);
}

static void test_update_recovers_non_finite_offset(void **state) {
    Map *map = map_create(3, 3);
    Camera camera;
    InputState input;
    (void)state;
    assert_non_null(map);
    camera_init(&camera, 1.5, 1.5, 0.0, PI / 2.0);
    memset(&input, 0, sizeof(input));
    camera.pitch = NAN;
    camera_update(&camera, map, &input, 0.0, 30);
    assert_float_equal(camera.pitch, 0.0, 0.0001);
    map_destroy(map);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_clamp_policy),
        cmocka_unit_test(test_invalid_policy_inputs_level_view),
        cmocka_unit_test(test_update_preserves_direction_sensitivity_and_clamps),
        cmocka_unit_test(test_update_recovers_non_finite_offset),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}