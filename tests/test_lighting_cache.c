#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include "../src/lighting_cache.h"

static LightShadowKey key_for(int revision) {
    LightShadowKey key = {
        .map_identity = UINT64_C(0x1234),
        .map_revision = revision,
        .lighting_revision = revision,
        .light_id = 2,
        .light_x_bits = UINT64_C(11),
        .light_y_bits = UINT64_C(12),
        .light_radius_bits = UINT64_C(13),
        .light_direction_bits = UINT64_C(14),
        .light_cone_bits = UINT64_C(15),
        .light_falloff_bits = UINT64_C(16),
        .light_type = 0,
        .target_tile_x = 3,
        .target_tile_y = 4
    };
    return key;
}

static LightSampleResult sample(void) {
    LightSampleResult result = {true, 2.5, 0.5, 0.75};
    return result;
}

static void test_lookup_store_and_stats(void **state) {
    LightSampleResult out = {0};
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    (void)state;

    lighting_cache_init();
    assert_false(lighting_cache_lookup(key_for(1), &out));
    lighting_cache_store(key_for(1), sample());
    assert_true(lighting_cache_lookup(key_for(1), &out));
    assert_true(out.blocked);
    assert_float_equal(out.intensity, 0.75, 0.0001);
    lighting_cache_get_stats(&hits, &misses, &evictions);
    assert_int_equal(hits, 1);
    assert_int_equal(misses, 1);
    assert_int_equal(evictions, 0);
}

static void test_invalidation_and_reset(void **state) {
    uint64_t evictions;
    (void)state;

    lighting_cache_init();
    lighting_cache_store(key_for(1), sample());
    lighting_cache_invalidate_map(2);
    assert_false(lighting_cache_lookup(key_for(1), NULL));
    lighting_cache_invalidate_lighting(2);
    lighting_cache_get_stats(NULL, NULL, &evictions);
    assert_true(evictions > 0);
    lighting_cache_reset_stats();
    lighting_cache_get_stats(NULL, NULL, &evictions);
    assert_int_equal(evictions, 0);
}

static void test_revision_is_part_of_key(void **state) {
    (void)state;
    lighting_cache_init();
    lighting_cache_store(key_for(1), sample());
    assert_false(lighting_cache_lookup(key_for(2), NULL));
    assert_true(lighting_cache_lookup(key_for(1), NULL));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_lookup_store_and_stats),
        cmocka_unit_test(test_invalidation_and_reset),
        cmocka_unit_test(test_revision_is_part_of_key),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}