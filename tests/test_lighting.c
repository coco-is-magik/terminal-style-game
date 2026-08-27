#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <math.h>
#include <cmocka.h>

#include "../src/config.h"
#include "../src/lighting.h"
#ifdef USE_LIGHTING_CACHE
#include "../src/lighting_cache.h"
#endif

static void lighting_update_current(Map *map, WorldState *world) {
    lighting_update_optical(map, world, NULL, 0U);
}

static void assert_level(LightLevel level, double red, double green, double blue) {
    assert_float_equal(level.red, red, 0.000001);
    assert_float_equal(level.green, green, 0.000001);
    assert_float_equal(level.blue, blue, 0.000001);
}

static void test_ambient_and_null_inputs(void **state) {
    Map *map = map_create(2, 2);
    WorldState world;
    (void)state;

    assert_non_null(map);
    world_init(&world);
    lighting_update_current(NULL, &world);
    lighting_update_current(map, NULL);
    lighting_update_current(map, &world);
    for (int i = 0; i < 4; i++) {
        assert_level(map->light_map[i], config_get()->ambient_light,
                     config_get()->ambient_light, config_get()->ambient_light);
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
    lighting_update_current(map, &world);
    assert_true(map->light_map[4].red > ambient);
    assert_int_equal(world_add_light(&world, 1.5, 1.5, white, -2.0, 2.0), WORLD_INSERT_OK);
    lighting_update_current(map, &world);
    assert_true(map->light_map[4].red < ambient);
    map_destroy(map);
}

static void test_invalid_structures_are_unchanged(void **state) {
    LightLevel sentinel = {7.0, 8.0, 9.0};
    Map malformed = {2, 2, NULL, &sentinel};
    WorldState world;
    (void)state;

    world_init(&world);
    lighting_update_current(&malformed, &world);
    assert_level(sentinel, 7.0, 8.0, 9.0);
    malformed.cells = (MapCell *)(void *)&sentinel;
    malformed.width = -1;
    lighting_update_current(&malformed, &world);
    assert_level(sentinel, 7.0, 8.0, 9.0);

    Map *map = map_create(1, 1);
    assert_non_null(map);
    map->light_map[0] = sentinel;
    world.num_lights = MAX_LIGHTS + 1;
    lighting_update_current(map, &world);
    assert_level(map->light_map[0], 7.0, 8.0, 9.0);
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
    lighting_update_current(map, &world);
    for (size_t i = 0U; i < 4U; i++) assert_level(map->light_map[i], 0.73, 0.73, 0.73);
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

    lighting_update_current(map, &world);
    assert_level(map->light_map[2 * 10 + 4], 1.01, 1.01, 1.01);
    assert_level(map->light_map[2 * 10 + 6], 0.51, 0.51, 0.51);
    assert_level(map->light_map[2 * 10 + 8], 0.01, 0.01, 0.01);
    assert_true(map->light_map[2 * 10 + 4].red > 1.0); /* Sampler clamps saturation. */

    world.lights[0].intensity = 0.5;
    lighting_update_current(map, &world);
    assert_level(map->light_map[2 * 10 + 4], 0.51, 0.51, 0.51);
    assert_level(map->light_map[2 * 10 + 6], 0.26, 0.26, 0.26);

    world.lights[0].radius = 2.0;
    lighting_update_current(map, &world);
    assert_level(map->light_map[2 * 10 + 6], 0.01, 0.01, 0.01);
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
    transmitting = map->light_map[1U * 5U + 4U].red;
    materials[1].ray_blocks = 0U;
    materials[1].light_blocks = 1U;
    lighting_update_optical(map, &world, &view, 9U);
    blocking = map->light_map[1U * 5U + 4U].red;
    assert_true(transmitting > blocking);
    lighting_update_optical(map, &world, &view, 10U);
    assert_true(map->light_map[1U * 5U + 4U].red == blocking);
    world_clear(&world);
    map_destroy(map);
}

static void test_optical_diagonal_wall_does_not_shadow_source_cell(void **state) {
    Map *map = map_create(4, 4);
    WorldState world;
    SDL_Color white = {255, 255, 255, 255};
    OpticalExtension materials[2] = {0};
    OpticalRuntimeView view;
    double source_open = 0.0;
    double source_blocked = 0.0;
    double far_open = 0.0;
    double far_blocked = 0.0;
    (void)state;
    assert_non_null(map);
    world_init(&world);
    world.has_authored_ambient = true;
    world.ambient_intensity = 0.0;
    /* Light sits near the top-left corner of its own tile (0,0); the wall at
     * (1,1) is diagonally adjacent to that tile. */
    assert_int_equal(world_add_light(
        &world, 0.1, 0.1, white, 1.0, 8.0), WORLD_INSERT_OK);
    map_set(map, 1, 1, 1);
    materials[1].override_mask = OPTICAL_OVERRIDE_LIGHT_BLOCKS |
                                 OPTICAL_OVERRIDE_RAY_BLOCKS;
    materials[1].light_blocks = 0U;
    materials[1].ray_blocks = 0U;
    assert_true(optical_runtime_view_init(
        &view, 16U, materials, 2U, NULL, 0U, 9U));

    lighting_update_optical(map, &world, &view, 9U);
    source_open = map->light_map[0U * 4U + 0U].red; /* light's own tile */
    far_open = map->light_map[3U * 4U + 3U].red;    /* beyond diagonal wall */

    /* Make the diagonal wall fully light/ray blocking.  It must still shade a
     * far tile, but must NOT darken the light's own diagonally-adjacent tile. */
    materials[1].light_blocks = 1U;
    materials[1].ray_blocks = 1U;
    lighting_update_optical(map, &world, &view, 9U);
    source_blocked = map->light_map[0U * 4U + 0U].red;
    far_blocked = map->light_map[3U * 4U + 3U].red;

    assert_float_equal(source_blocked, source_open, 0.000001);
    assert_true(far_open > far_blocked);

    world_clear(&world);
    map_destroy(map);
}

static void test_colored_alpha_additive_and_anti_light_math(void **state) {
    Map *map = map_create(1, 1);
    WorldState world;
    (void)state;
    assert_non_null(map);
    world_init(&world);
    world.has_authored_ambient = true;
    world.ambient_intensity = 0.1;

    assert_int_equal(world_add_light(
        &world, 0.5, 0.5, (SDL_Color){255, 0, 0, 128}, 2.0, 1.0),
        WORLD_INSERT_OK);
    assert_int_equal(world_add_light(
        &world, 0.5, 0.5, (SDL_Color){0, 255, 128, 255}, 1.0, 1.0),
        WORLD_INSERT_OK);
    assert_int_equal(world_add_light(
        &world, 0.5, 0.5, (SDL_Color){0, 255, 0, 255}, -0.25, 1.0),
        WORLD_INSERT_OK);

    lighting_update_current(map, &world);
    assert_level(map->light_map[0],
                 0.1 + 2.0 * (128.0 / 255.0),
                 0.1 + 1.0 - 0.25,
                 0.1 + 128.0 / 255.0);
    map_destroy(map);
}

static void test_palette_sample_clamps_channels_independently(void **state) {
    Palette palette = {
        .near_color = {200, 150, 100, 255},
        .mid_color = {0, 0, 0, 255},
        .far_color = {0, 0, 0, 255}
    };
    SDL_Color sampled;
    (void)state;
    sampled = palette_sample(&palette, 1.0, (LightLevel){1.5, 0.5, -1.0});
    assert_int_equal(sampled.r, 200U);
    assert_int_equal(sampled.g, 75U);
    assert_int_equal(sampled.b, 0U);
    assert_int_equal(sampled.a, 255U);
}

static void test_spot_cone_and_radial_falloff_are_exact(void **state) {
    Map *map = map_create(7, 7);
    WorldState world;
    SDL_Color white = {255, 255, 255, 255};
    (void)state;
    assert_non_null(map);
    world_init(&world);
    world.has_authored_ambient = true;
    world.ambient_intensity = 0.0;
    assert_int_equal(world_add_spot_light(
        &world, 3.5, 3.5, white, 1.0, 4.0, 0.0,
        SCENE_LIGHT_DIRECTION_MAX / 4.0, 2.0),
        WORLD_INSERT_OK);
    lighting_update_current(map, &world);
    assert_level(map->light_map[3U * 7U + 4U], 0.5625, 0.5625, 0.5625);
    assert_level(map->light_map[2U * 7U + 3U], 0.0, 0.0, 0.0);
    assert_true(map->light_map[2U * 7U + 4U].red > 0.0); /* +45 degree boundary */
    map_destroy(map);
}

static void test_invalid_spot_parameters_do_not_mutate_world(void **state) {
    WorldState world;
    SDL_Color white = {255, 255, 255, 255};
    (void)state;
    world_init(&world);
    assert_int_equal(world_add_spot_light(
        &world, 0.5, 0.5, white, 1.0, 2.0, -0.1, 1.0, 1.0),
        WORLD_INSERT_INVALID);
    assert_int_equal(world_add_spot_light(
        &world, 0.5, 0.5, white, 1.0, 2.0, 0.0, 0.0, 1.0),
        WORLD_INSERT_INVALID);
    assert_int_equal(world_add_spot_light(
        &world, 0.5, 0.5, white, 1.0, 2.0, 0.0, 1.0, 0.0),
        WORLD_INSERT_INVALID);
    assert_int_equal(world.num_lights, 0);
}

#ifdef USE_LIGHTING_CACHE
static void test_cache_hit_uses_current_color_alpha_and_intensity(void **state) {
    Map *map = map_create(1, 1);
    WorldState world;
    uint64_t hits = 0U;
    (void)state;
    assert_non_null(map);
    world_init(&world);
    world.has_authored_ambient = true;
    world.ambient_intensity = 0.0;
    assert_int_equal(world_add_light(
        &world, 0.5, 0.5, (SDL_Color){255, 0, 0, 255}, 1.0, 1.0),
        WORLD_INSERT_OK);
    lighting_cache_init();
    lighting_update_current(map, &world);
    assert_level(map->light_map[0], 1.0, 0.0, 0.0);

    world.lights[0].color = (SDL_Color){0, 255, 0, 128};
    world.lights[0].intensity = 2.0;
    lighting_update_current(map, &world);
    lighting_cache_get_stats(&hits, NULL, NULL);
    assert_true(hits > 0U);
    assert_level(map->light_map[0], 0.0, 2.0 * (128.0 / 255.0), 0.0);
    map_destroy(map);
}

static void test_cache_key_tracks_spot_geometry(void **state) {
    Map *map = map_create(3, 3);
    WorldState world;
    uint64_t misses = 0U;
    (void)state;
    assert_non_null(map);
    world_init(&world);
    world.has_authored_ambient = true;
    world.ambient_intensity = 0.0;
    assert_int_equal(world_add_spot_light(
        &world, 1.5, 1.5, (SDL_Color){255, 255, 255, 255}, 1.0, 3.0,
        0.0, SCENE_LIGHT_DIRECTION_MAX / 4.0, 1.0), WORLD_INSERT_OK);
    lighting_cache_init();
    lighting_update_current(map, &world);
    assert_true(map->light_map[1U * 3U + 2U].red > 0.0);
    world.lights[0].direction = SCENE_LIGHT_DIRECTION_MAX / 2.0;
    lighting_update_current(map, &world);
    lighting_cache_get_stats(NULL, &misses, NULL);
    assert_true(misses > 0U);
    assert_level(map->light_map[1U * 3U + 2U], 0.0, 0.0, 0.0);
    map_destroy(map);
}
#endif

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ambient_and_null_inputs),
        cmocka_unit_test(test_positive_and_negative_lights_accumulate),
        cmocka_unit_test(test_invalid_structures_are_unchanged),
        cmocka_unit_test(test_authored_runtime_ambient_overrides_config),
        cmocka_unit_test(test_intensity_radius_and_saturation_are_quantified),
        cmocka_unit_test(test_optical_light_blocking_is_independent),
        cmocka_unit_test(test_optical_diagonal_wall_does_not_shadow_source_cell),
        cmocka_unit_test(test_colored_alpha_additive_and_anti_light_math),
        cmocka_unit_test(test_palette_sample_clamps_channels_independently),
        cmocka_unit_test(test_spot_cone_and_radial_falloff_are_exact),
        cmocka_unit_test(test_invalid_spot_parameters_do_not_mutate_world),
#ifdef USE_LIGHTING_CACHE
        cmocka_unit_test(test_cache_hit_uses_current_color_alpha_and_intensity),
        cmocka_unit_test(test_cache_key_tracks_spot_geometry),
#endif
    };
    config_init_defaults();
    return cmocka_run_group_tests(tests, NULL, NULL);
}