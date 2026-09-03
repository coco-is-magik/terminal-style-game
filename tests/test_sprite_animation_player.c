#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>

#include <cmocka.h>

#include "../src/sprite_animation_player.h"

#include <math.h>
#include <stdlib.h>

static void set_animation(AssetRegistry *assets, int id, size_t count,
                          double fps, bool loop) {
    SpriteAnimationAsset *animation = &assets->sprite_animations[id];
    animation->frames = calloc(count, sizeof(*animation->frames));
    assert_non_null(animation->frames);
    animation->frame_count = count;
    animation->frames_per_second = fps;
    animation->loop = loop;
    for (size_t i = 0U; i < count; i++) {
        animation->frames[i].cols = 1;
        animation->frames[i].rows = 1;
        animation->frames[i].pattern = malloc(sizeof(PatternCell));
        assert_non_null(animation->frames[i].pattern);
        animation->frames[i].pattern[0] =
            (PatternCell){(uint8_t)('A' + i), UINT16_C(1)};
    }
}

static void test_time_based_looping_and_independent_instances(void **state) {
    AssetRegistry assets;
    WorldState world;
    (void)state;
    assert_true(asset_registry_init(&assets));
    world_init(&world);
    set_animation(&assets, 7, 3U, 4.0, true);
    assert_int_equal(world_add_sprite(&world, 1.0, 1.0, 7), WORLD_INSERT_OK);
    assert_int_equal(world_add_sprite(&world, 2.0, 1.0, 7), WORLD_INSERT_OK);

    sprite_animation_player_tick(&world, &assets, 0.24);
    assert_int_equal(world.sprites[0].animation_frame, 0U);
    sprite_animation_player_tick(&world, &assets, 0.01);
    assert_int_equal(world.sprites[0].animation_frame, 1U);
    assert_int_equal(world.sprites[1].animation_frame, 1U);
    world.sprites[1].animation_elapsed = 0.125;
    sprite_animation_player_tick(&world, &assets, 0.125);
    assert_int_equal(world.sprites[0].animation_frame, 1U);
    assert_int_equal(world.sprites[1].animation_frame, 2U);
    sprite_animation_player_tick(&world, &assets, 0.50);
    assert_int_equal(world.sprites[0].animation_frame, 0U);

    world_clear(&world);
    asset_registry_clear(&assets);
}

static void test_non_looping_clamps_and_invalid_delta_is_noop(void **state) {
    AssetRegistry assets;
    WorldState world;
    (void)state;
    assert_true(asset_registry_init(&assets));
    world_init(&world);
    set_animation(&assets, 4, 2U, 10.0, false);
    assert_int_equal(world_add_sprite(&world, 1.0, 1.0, 4), WORLD_INSERT_OK);
    sprite_animation_player_tick(&world, &assets, NAN);
    sprite_animation_player_tick(&world, &assets, -1.0);
    assert_int_equal(world.sprites[0].animation_frame, 0U);
    sprite_animation_player_tick(&world, &assets, 1.0e300);
    assert_true(world.sprites[0].animation_frame < 2U);
    world.sprites[0].animation_frame = 0U;
    world.sprites[0].animation_elapsed = 0.0;
    sprite_animation_player_tick(&world, &assets, 1.0);
    assert_int_equal(world.sprites[0].animation_frame, 1U);
    sprite_animation_player_tick(&world, &assets, 1.0);
    assert_int_equal(world.sprites[0].animation_frame, 1U);
    assert_true(world.sprites[0].animation_elapsed == 0.0);
    world_clear(&world);
    asset_registry_clear(&assets);
}

static void test_static_sprite_and_missing_animation_are_unchanged(void **state) {
    AssetRegistry assets;
    WorldState world;
    PatternCell cell = {(uint8_t)'S', UINT16_C(1)};
    (void)state;
    assert_true(asset_registry_init(&assets));
    assert_true(asset_registry_set_sprite(&assets, 1U, 1, 1, &cell));
    world_init(&world);
    assert_int_equal(world_add_sprite(&world, 1.0, 1.0, 1), WORLD_INSERT_OK);
    sprite_animation_player_tick(&world, &assets, 50.0);
    assert_int_equal(world.sprites[0].animation_frame, 0U);
    assert_true(world.sprites[0].animation_elapsed == 0.0);
    assert_ptr_equal(asset_registry_get_sprite_frame(&assets, 1, 99U),
                     asset_registry_get_sprite(&assets, 1));
    world_clear(&world);
    asset_registry_clear(&assets);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_time_based_looping_and_independent_instances),
        cmocka_unit_test(test_non_looping_clamps_and_invalid_delta_is_noop),
        cmocka_unit_test(test_static_sprite_and_missing_animation_are_unchanged)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}