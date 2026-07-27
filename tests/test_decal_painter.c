#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <cmocka.h>

#include "../src/decal_painter.h"

static void test_init_destroy_empty(void **state) {
    (void)state;
    DecalPainter painter;

    decal_painter_init(&painter);
    assert_null(painter.cells);
    assert_int_equal(painter.cols, 0);
    assert_int_equal(painter.rows, 0);
    assert_false(painter.owns_cells);

    decal_painter_destroy(&painter);
    decal_painter_destroy(NULL);
}

static void test_create_zero_initializes_owned_canvas(void **state) {
    (void)state;
    DecalPainter painter;
    PatternCell cell = {255, 255};

    decal_painter_init(&painter);
    assert_int_equal(decal_painter_create(&painter, 3, 2), DECAL_PAINTER_OK);
    assert_non_null(painter.cells);
    assert_int_equal(painter.cols, 3);
    assert_int_equal(painter.rows, 2);
    assert_true(painter.owns_cells);

    for (size_t y = 0; y < painter.rows; y++) {
        for (size_t x = 0; x < painter.cols; x++) {
            assert_int_equal(
                decal_painter_get_cell(&painter, x, y, &cell),
                DECAL_PAINTER_OK
            );
            assert_int_equal(cell.glyph, 0);
            assert_int_equal(cell.material_id, 0);
        }
    }
    decal_painter_destroy(&painter);
}

static void test_create_rejects_dimensions_and_overflow_transactionally(void **state) {
    (void)state;
    DecalPainter painter;
    PatternCell original = {'A', 7};

    decal_painter_init(&painter);
    assert_int_equal(decal_painter_attach(&painter, &original, 1, 1), DECAL_PAINTER_OK);

    assert_int_equal(
        decal_painter_create(&painter, 0, 1),
        DECAL_PAINTER_INVALID_DIMENSIONS
    );
    assert_ptr_equal(painter.cells, &original);
    assert_false(painter.owns_cells);

    assert_int_equal(
        decal_painter_create(&painter, SIZE_MAX, 2),
        DECAL_PAINTER_SIZE_OVERFLOW
    );
    assert_ptr_equal(painter.cells, &original);
    assert_int_equal(original.glyph, 'A');
    assert_int_equal(original.material_id, 7);
    decal_painter_destroy(&painter);
}

static void test_attach_borrows_and_destroy_does_not_free(void **state) {
    (void)state;
    PatternCell *cells = calloc(4, sizeof(*cells));
    DecalPainter painter;

    assert_non_null(cells);
    decal_painter_init(&painter);
    assert_int_equal(decal_painter_attach(&painter, cells, 2, 2), DECAL_PAINTER_OK);
    assert_false(painter.owns_cells);

    assert_int_equal(
        decal_painter_paint_cell(&painter, 1, 1, (PatternCell){'X', 4}),
        DECAL_PAINTER_OK
    );
    decal_painter_destroy(&painter);

    assert_int_equal(cells[3].glyph, 'X');
    assert_int_equal(cells[3].material_id, 4);
    free(cells);
}

static void test_attach_rejects_invalid_without_replacing_canvas(void **state) {
    (void)state;
    PatternCell cells[2] = {{'A', 1}, {'B', 2}};
    DecalPainter painter;

    decal_painter_init(&painter);
    assert_int_equal(decal_painter_attach(&painter, cells, 2, 1), DECAL_PAINTER_OK);
    assert_int_equal(
        decal_painter_attach(&painter, NULL, 1, 1),
        DECAL_PAINTER_INVALID_ARGUMENT
    );
    assert_ptr_equal(painter.cells, cells);
    assert_int_equal(
        decal_painter_attach(&painter, cells, 0, 1),
        DECAL_PAINTER_INVALID_DIMENSIONS
    );
    assert_ptr_equal(painter.cells, cells);
    decal_painter_destroy(&painter);
}

static void test_attach_rejects_alias_of_owned_storage(void **state) {
    (void)state;
    DecalPainter painter;

    decal_painter_init(&painter);
    assert_int_equal(decal_painter_create(&painter, 2, 2), DECAL_PAINTER_OK);
    PatternCell *owned = painter.cells;

    assert_int_equal(
        decal_painter_attach(&painter, owned, 2, 2),
        DECAL_PAINTER_INVALID_ARGUMENT
    );
    assert_ptr_equal(painter.cells, owned);
    assert_true(painter.owns_cells);
    decal_painter_destroy(&painter);
}

static void test_paint_uses_row_major_coordinates(void **state) {
    (void)state;
    DecalPainter painter;
    PatternCell readback;

    decal_painter_init(&painter);
    assert_int_equal(decal_painter_create(&painter, 3, 2), DECAL_PAINTER_OK);
    assert_int_equal(
        decal_painter_paint_cell(&painter, 2, 1, (PatternCell){'#', 9}),
        DECAL_PAINTER_OK
    );
    assert_int_equal(painter.cells[5].glyph, '#');
    assert_int_equal(painter.cells[5].material_id, 9);
    assert_int_equal(
        decal_painter_get_cell(&painter, 2, 1, &readback),
        DECAL_PAINTER_OK
    );
    assert_int_equal(readback.glyph, '#');
    assert_int_equal(readback.material_id, 9);
    decal_painter_destroy(&painter);
}

static void test_paint_reports_no_change(void **state) {
    (void)state;
    DecalPainter painter;
    PatternCell cell = {'@', 3};

    decal_painter_init(&painter);
    assert_int_equal(decal_painter_create(&painter, 1, 1), DECAL_PAINTER_OK);
    assert_int_equal(decal_painter_paint_cell(&painter, 0, 0, cell), DECAL_PAINTER_OK);
    assert_int_equal(
        decal_painter_paint_cell(&painter, 0, 0, cell),
        DECAL_PAINTER_NO_CHANGE
    );
    decal_painter_destroy(&painter);
}

static void test_out_of_bounds_preserves_canvas_and_output(void **state) {
    (void)state;
    DecalPainter painter;
    PatternCell sentinel = {'S', 8};

    decal_painter_init(&painter);
    assert_int_equal(decal_painter_create(&painter, 2, 2), DECAL_PAINTER_OK);
    assert_int_equal(
        decal_painter_paint_cell(&painter, 0, 0, (PatternCell){'A', 1}),
        DECAL_PAINTER_OK
    );
    assert_int_equal(
        decal_painter_paint_cell(&painter, 2, 0, (PatternCell){'B', 2}),
        DECAL_PAINTER_OUT_OF_BOUNDS
    );
    assert_int_equal(
        decal_painter_get_cell(&painter, 0, 2, &sentinel),
        DECAL_PAINTER_OUT_OF_BOUNDS
    );
    assert_int_equal(painter.cells[0].glyph, 'A');
    assert_int_equal(painter.cells[0].material_id, 1);
    assert_int_equal(sentinel.glyph, 'S');
    assert_int_equal(sentinel.material_id, 8);
    decal_painter_destroy(&painter);
}

static void test_erase_clears_glyph_and_material(void **state) {
    (void)state;
    DecalPainter painter;

    decal_painter_init(&painter);
    assert_int_equal(decal_painter_create(&painter, 1, 1), DECAL_PAINTER_OK);
    assert_int_equal(
        decal_painter_paint_cell(&painter, 0, 0, (PatternCell){'E', 5}),
        DECAL_PAINTER_OK
    );
    assert_int_equal(decal_painter_erase_cell(&painter, 0, 0), DECAL_PAINTER_OK);
    assert_int_equal(painter.cells[0].glyph, 0);
    assert_int_equal(painter.cells[0].material_id, 0);
    assert_int_equal(
        decal_painter_erase_cell(&painter, 0, 0),
        DECAL_PAINTER_NO_CHANGE
    );
    decal_painter_destroy(&painter);
}

static void test_fill_and_clear(void **state) {
    (void)state;
    DecalPainter painter;
    PatternCell fill = {'+', 6};

    decal_painter_init(&painter);
    assert_int_equal(decal_painter_create(&painter, 4, 3), DECAL_PAINTER_OK);
    assert_int_equal(decal_painter_fill(&painter, fill), DECAL_PAINTER_OK);
    assert_int_equal(decal_painter_fill(&painter, fill), DECAL_PAINTER_NO_CHANGE);
    for (size_t i = 0; i < 12; i++) {
        assert_int_equal(painter.cells[i].glyph, '+');
        assert_int_equal(painter.cells[i].material_id, 6);
    }

    assert_int_equal(decal_painter_clear(&painter), DECAL_PAINTER_OK);
    assert_int_equal(decal_painter_clear(&painter), DECAL_PAINTER_NO_CHANGE);
    for (size_t i = 0; i < 12; i++) {
        assert_int_equal(painter.cells[i].glyph, 0);
        assert_int_equal(painter.cells[i].material_id, 0);
    }
    decal_painter_destroy(&painter);
}

static void test_invalid_arguments(void **state) {
    (void)state;
    DecalPainter painter;
    PatternCell cell;

    decal_painter_init(&painter);
    assert_int_equal(
        decal_painter_create(NULL, 1, 1),
        DECAL_PAINTER_INVALID_ARGUMENT
    );
    assert_int_equal(
        decal_painter_attach(NULL, &cell, 1, 1),
        DECAL_PAINTER_INVALID_ARGUMENT
    );
    assert_int_equal(
        decal_painter_get_cell(&painter, 0, 0, &cell),
        DECAL_PAINTER_INVALID_ARGUMENT
    );
    assert_int_equal(
        decal_painter_get_cell(&painter, 0, 0, NULL),
        DECAL_PAINTER_INVALID_ARGUMENT
    );
    assert_int_equal(
        decal_painter_paint_cell(NULL, 0, 0, cell),
        DECAL_PAINTER_INVALID_ARGUMENT
    );
    assert_int_equal(
        decal_painter_fill(&painter, cell),
        DECAL_PAINTER_INVALID_ARGUMENT
    );
    decal_painter_destroy(&painter);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_destroy_empty),
        cmocka_unit_test(test_create_zero_initializes_owned_canvas),
        cmocka_unit_test(test_create_rejects_dimensions_and_overflow_transactionally),
        cmocka_unit_test(test_attach_borrows_and_destroy_does_not_free),
        cmocka_unit_test(test_attach_rejects_invalid_without_replacing_canvas),
        cmocka_unit_test(test_attach_rejects_alias_of_owned_storage),
        cmocka_unit_test(test_paint_uses_row_major_coordinates),
        cmocka_unit_test(test_paint_reports_no_change),
        cmocka_unit_test(test_out_of_bounds_preserves_canvas_and_output),
        cmocka_unit_test(test_erase_clears_glyph_and_material),
        cmocka_unit_test(test_fill_and_clear),
        cmocka_unit_test(test_invalid_arguments),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}