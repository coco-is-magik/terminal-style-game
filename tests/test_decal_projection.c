#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "../src/decal_projection.h"

static void test_shared_surface_mapping(void **state) {
    (void)state;
    Decal decal = {0};
    decal.x = 10.0; decal.y = 20.0; decal.z = 1.0;
    decal.width = 16.0; decal.height = 8.0;
    decal.pattern_cols = 2; decal.pattern_rows = 2;
    decal.surface = DECAL_SURFACE_WALL;
    DecalBasis basis = decal_projection_basis(&decal);
    double x0, y0, z0, x1, y1, z1;
    decal_projection_glyph_world(&decal, &basis, 0, 0, 8.0, &x0, &y0, &z0);
    decal_projection_glyph_world(&decal, &basis, 1, 0, 8.0, &x1, &y1, &z1);
    assert_float_equal(x0, x1, 0.00001);
    assert_float_equal(y0 - y1, 1.0, 0.00001);
    assert_float_equal(z0, z1, 0.00001);
    decal.surface = DECAL_SURFACE_FLOOR;
    basis = decal_projection_basis(&decal);
    decal_projection_glyph_world(&decal, &basis, 0, 0, 8.0, &x0, &y0, &z0);
    decal_projection_glyph_world(&decal, &basis, 1, 0, 8.0, &x1, &y1, &z1);
    assert_float_equal(x1 - x0, 1.0, 0.00001);
    assert_float_equal(z0, z1, 0.00001);
}

int main(void) {
    const struct CMUnitTest tests[] = {cmocka_unit_test(test_shared_surface_mapping)};
    return cmocka_run_group_tests(tests, NULL, NULL);
}