#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <math.h>
#include <cmocka.h>

#include "../src/config.h"
#include "../src/lighting.h"

static void test_ambient_and_null_inputs(void **state) {
    Map *map = map_create(2, 2);
    WorldState world;
    (void)state;

    assert_non_null(map);
    world_init(&world);
    lighting_update(NULL, &world);
    lighting_update(map, NULL);
    lighting_update(map, &world);
    for (int i = 0; i < 4; i++) {
        assert_float_equal(map->light_map[i], config_get()->ambient_light, 0.0001);
    }
    map_destroy(map);
}

static void test_positive_and_negative_lights_accumulate(void **state) {
    Map *map = map_create(3, 3);
    WorldState world;
    SDL_Color white = {255, 255, 255, 255};
    double ambient = config_get()->ambient_light;
    (void)state;

    assert_non_null(map);
    world_init(&world);
    assert_int_equal(world_add_light(&world, 1.5, 1.5, white, 1.0, 2.0), WORLD_INSERT_OK);
    lighting_update(map, &world);
    assert_true(map->light_map[4] > ambient);
    assert_int_equal(world_add_light(&world, 1.5, 1.5, white, -2.0, 2.0), WORLD_INSERT_OK);
    lighting_update(map, &world);
    assert_true(map->light_map[4] < ambient);
    map_destroy(map);
}

static void test_invalid_structures_are_unchanged(void **state) {
    double sentinel = 7.0;
    Map malformed = {2, 2, NULL, &sentinel};
    WorldState world;
    (void)state;

    world_init(&world);
    lighting_update(&malformed, &world);
    assert_float_equal(sentinel, 7.0, 0.0);
    malformed.cells = (MapCell *)&sentinel;
    malformed.width = -1;
    lighting_update(&malformed, &world);
    assert_float_equal(sentinel, 7.0, 0.0);

    Map *map = map_create(1, 1);
    assert_non_null(map);
    map->light_map[0] = sentinel;
    world.num_lights = MAX_LIGHTS + 1;
    lighting_update(map, &world);
    assert_float_equal(map->light_map[0], sentinel, 0.0);
    map_destroy(map);
}

static void test_authored_runtime_ambient_overrides_config(void **state) {
    Map *map = map_create(2, 2);
    WorldState world;
    (void)state;
    assert_non_null(map);
    world_init(&world);
    world.has_authored_ambient = true;
    world.ambient_intensity = 0.73;
    lighting_update(map, &world);
    for (size_t i = 0U; i < 4U; i++) assert_true(map->light_map[i] == 0.73);
    world_clear(&world);
    map_destroy(map);
}

static void test_intensity_radius_and_saturation_are_quantified(void **state) {
    Map *map = map_create(10, 6);
    WorldState world;
    SDL_Color white = {255, 255, 255, 255};
    (void)state;
    assert_non_null(map);
    world_init(&world);
    world.has_authored_ambient = true;
    world.ambient_intensity = 0.01;
    assert_int_equal(world_add_light(
        &world, 4.5, 2.5, white, 1.0, 4.0), WORLD_INSERT_OK);

    lighting_update(map, &world);
    assert_float_equal(map->light_map[2 * 10 + 4], 1.01, 0.000001);
    assert_float_equal(map->light_map[2 * 10 + 6], 0.51, 0.000001);
    assert_float_equal(map->light_map[2 * 10 + 8], 0.01, 0.000001);
    assert_true(map->light_map[2 * 10 + 4] > 1.0); /* Renderer clamps saturation. */

    world.lights[0].intensity = 0.5;
    lighting_update(map, &world);
    assert_float_equal(map->light_map[2 * 10 + 4], 0.51, 0.000001);
    assert_float_equal(map->light_map[2 * 10 + 6], 0.26, 0.000001);

    world.lights[0].radius = 2.0;
    lighting_update(map, &world);
    assert_float_equal(map->light_map[2 * 10 + 6], 0.01, 0.000001);
    map_destroy(map);
}

static void test_optical_light_blocking_is_independent(void **state) {
    Map *map = map_create(5, 3);
    WorldState world;
    SDL_Color white = {255, 255, 255, 255};
    OpticalExtension materials[2] = {0};
    OpticalRuntimeView view;
    double transmitting;
    double blocking;
    (void)state;
    assert_non_null(map);
    world_init(&world);
    world.has_authored_ambient = true;
    world.ambient_intensity = 0.0;
    assert_int_equal(world_add_light(
        &world, 0.5, 1.5, white, 1.0, 5.0), WORLD_INSERT_OK);
    map_set(map, 2, 1, 1);
    materials[1].override_mask = OPTICAL_OVERRIDE_RAY_BLOCKS |
                                 OPTICAL_OVERRIDE_LIGHT_BLOCKS;
    materials[1].ray_blocks = 1U;
    materials[1].light_blocks = 0U;
    assert_true(optical_runtime_view_init(
        &view, 15U, materials, 2U, NULL, 0U, 9U));
    lighting_update_optical(map, &world, &view, 9U);
    transmitting = map->light_map[1U * 5U + 4U];
    materials[1].ray_blocks = 0U;
    materials[1].light_blocks = 1U;
    lighting_update_optical(map, &world, &view, 9U);
    blocking = map->light_map[1U * 5U + 4U];
    assert_true(transmitting > blocking);
    lighting_update_optical(map, &world, &view, 10U);
    assert_true(map->light_map[1U * 5U + 4U] == blocking);
    world_clear(&world);
    map_destroy(map);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ambient_and_null_inputs),
        cmocka_unit_test(test_positive_and_negative_lights_accumulate),
        cmocka_unit_test(test_invalid_structures_are_unchanged),
        cmocka_unit_test(test_authored_runtime_ambient_overrides_config),
        cmocka_unit_test(test_intensity_radius_and_saturation_are_quantified),
        cmocka_unit_test(test_optical_light_blocking_is_independent),
    };
    config_init_defaults();
    return cmocka_run_group_tests(tests, NULL, NULL);
}