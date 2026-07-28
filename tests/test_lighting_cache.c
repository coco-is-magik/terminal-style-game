#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include "../src/lighting_cache.h"

static LightShadowKey key_for(int revision) {
    LightShadowKey key = {revision, revision, 2, 3, 4};
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