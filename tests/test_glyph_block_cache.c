#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include "../src/font8x8.h"
#include "../src/glyph_block_cache.h"

static void test_compute_is_deterministic(void **state) {
    GlyphBlockKey key = {'A', 0x11223344u, 0x55667788u};
    GlyphBlock first = {{0}};
    GlyphBlock second = {{0}};
    (void)state;

    glyph_block_cache_compute(key, &first);
    glyph_block_cache_compute(key, &second);
    assert_memory_equal(&first, &second, sizeof(first));
    glyph_block_cache_compute(key, NULL);
}

static void test_lookup_stats_and_store(void **state) {
    GlyphBlockKey key = {'B', 1u, 2u};
    GlyphBlock block = {{0}};
    GlyphBlock out = {{0}};
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    (void)state;

    glyph_block_cache_init();
    glyph_block_cache_compute(key, &block);
    assert_false(glyph_block_cache_lookup(key, &out));
    glyph_block_cache_store(key, block);
    assert_true(glyph_block_cache_lookup(key, &out));
    assert_memory_equal(&block, &out, sizeof(block));
    glyph_block_cache_get_stats(&hits, &misses, &evictions);
    assert_int_equal(hits, 1);
    assert_int_equal(misses, 1);
    assert_int_equal(evictions, 0);
}

static void test_replacement_counts_eviction(void **state) {
    GlyphBlockKey key = {'C', 3u, 4u};
    GlyphBlock block = {{0}};
    uint64_t evictions;
    (void)state;

    glyph_block_cache_init();
    glyph_block_cache_store(key, block);
    glyph_block_cache_store(key, block);
    glyph_block_cache_get_stats(NULL, NULL, &evictions);
    assert_int_equal(evictions, 1);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_compute_is_deterministic),
        cmocka_unit_test(test_lookup_stats_and_store),
        cmocka_unit_test(test_replacement_counts_eviction),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}