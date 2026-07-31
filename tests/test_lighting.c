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

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ambient_and_null_inputs),
        cmocka_unit_test(test_positive_and_negative_lights_accumulate),
        cmocka_unit_test(test_invalid_structures_are_unchanged),
        cmocka_unit_test(test_authored_runtime_ambient_overrides_config),
    };
    config_init_defaults();
    return cmocka_run_group_tests(tests, NULL, NULL);
}