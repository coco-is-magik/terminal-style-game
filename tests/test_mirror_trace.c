#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <cmocka.h>

#include "../src/mirror_trace.h"

static void test_mirror_trace_column_cache_init(void **state) {
    MirrorTraceColumnCache cache;
    (void)state;
    memset(&cache, 0x5a, sizeof(cache));
    mirror_trace_column_cache_init(&cache);
    assert_false(cache.prepared);
    assert_int_equal(cache.preparation_count, 0U);
    mirror_trace_column_cache_init(NULL);
}

static void test_mirror_trace_reflect_direction_cardinal(void **state) {
    double rx, ry;
    (void)state;
    assert_true(mirror_trace_reflect_direction(1.0, 0.0, 0, &rx, &ry));
    assert_float_equal(rx, -1.0, 0.0001);
    assert_float_equal(ry, 0.0, 0.0001);
    assert_true(mirror_trace_reflect_direction(1.0, 0.0, 1, &rx, &ry));
    assert_float_equal(rx, 1.0, 0.0001);
    assert_float_equal(ry, 0.0, 0.0001);
    assert_true(mirror_trace_reflect_direction(0.0, 1.0, 0, &rx, &ry));
    assert_float_equal(rx, 0.0, 0.0001);
    assert_float_equal(ry, 1.0, 0.0001);
    assert_true(mirror_trace_reflect_direction(0.0, 1.0, 1, &rx, &ry));
    assert_float_equal(rx, 0.0, 0.0001);
    assert_float_equal(ry, -1.0, 0.0001);
}

static void test_mirror_trace_reflect_direction_oblique(void **state) {
    double rx, ry;
    double expected = sqrt(0.5);
    (void)state;
    assert_true(mirror_trace_reflect_direction(1.0, 1.0, 0, &rx, &ry));
    assert_float_equal(rx, -expected, 0.0001);
    assert_float_equal(ry, expected, 0.0001);
    assert_true(mirror_trace_reflect_direction(1.0, 1.0, 1, &rx, &ry));
    assert_float_equal(rx, expected, 0.0001);
    assert_float_equal(ry, -expected, 0.0001);
}

static void test_mirror_trace_reflect_direction_invalid(void **state) {
    double rx, ry;
    (void)state;
    assert_false(mirror_trace_reflect_direction(0.0, 0.0, 0, &rx, &ry));
    assert_false(mirror_trace_reflect_direction(1.0, 0.0, -1, &rx, &ry));
    assert_false(mirror_trace_reflect_direction(1.0, 0.0, 2, &rx, &ry));
    assert_false(mirror_trace_reflect_direction(INFINITY, 0.0, 0, &rx, &ry));
    assert_false(mirror_trace_reflect_direction(1.0, 0.0, 0, NULL, &ry));
    assert_false(mirror_trace_reflect_direction(1.0, 0.0, 0, &rx, NULL));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_mirror_trace_column_cache_init),
        cmocka_unit_test(test_mirror_trace_reflect_direction_cardinal),
        cmocka_unit_test(test_mirror_trace_reflect_direction_oblique),
        cmocka_unit_test(test_mirror_trace_reflect_direction_invalid)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
