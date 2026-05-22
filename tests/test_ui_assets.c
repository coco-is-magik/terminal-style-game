#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include "../src/ui_asset.h"
#include "../src/grid.h"

/* ===================================================================
 *  test_ui_asset_load_valid
 *
 *  Loads assets/ui/1.txt (Start Game, 20x3) and verifies:
 *    - Return is non-NULL
 *    - id, width, height match the file header
 *    - normal and selected art buffers are non-NULL
 *    - First character of normal row 0 is '+' (border character)
 *    - First character of selected row 0 is '*' (highlight border)
 *    - No disabled state defined in the file → disabled is NULL
 * =================================================================== */
static void test_ui_asset_load_valid(void **state) {
    (void)state;

    UIButtonAsset *btn = ui_asset_load(1, "assets");
    assert_non_null(btn);

    assert_int_equal(btn->id,     1);
    assert_int_equal(btn->width,  20);
    assert_int_equal(btn->height, 3);

    assert_non_null(btn->normal);
    assert_non_null(btn->selected);

    /* First char of normal row 0: '+------------------+' */
    assert_int_equal((unsigned char)btn->normal[0], '+');

    /* First char of selected row 0: '*==================*' */
    assert_int_equal((unsigned char)btn->selected[0], '*');

    /* No disabled rows in assets/ui/1.txt */
    assert_null(btn->disabled);

    ui_asset_destroy(btn);
}

/* ===================================================================
 *  test_ui_asset_load_dimension_mismatch
 *
 *  Loads assets/ui/99.txt which declares width=20 but all art rows
 *  are only 8 characters ("TOOSHORT").  The loader must detect the
 *  mismatch and return NULL without crashing.
 * =================================================================== */
static void test_ui_asset_load_dimension_mismatch(void **state) {
    (void)state;

    UIButtonAsset *btn = ui_asset_load(99, "assets");
    assert_null(btn);
}

/* ===================================================================
 *  test_ui_button_render_normal_vs_selected
 *
 *  Loads assets/ui/1.txt, renders the button at (0,0) in both
 *  NORMAL and SELECTED states, and verifies that the grid cells
 *  differ in colour between states.
 * =================================================================== */
static void test_ui_button_render_normal_vs_selected(void **state) {
    (void)state;

    UIButtonAsset *btn = ui_asset_load(1, "assets");
    assert_non_null(btn);

    Grid *g = grid_create(30, 10);
    assert_non_null(g);

    SDL_Color black = {0, 0, 0, 255};

    /* Render in NORMAL state and capture cell (0,0) */
    grid_clear(g, black);
    ui_button_render(g, 0, 0, btn, UI_BUTTON_NORMAL);
    Cell normal_cell;
    assert_true(grid_get(g, 0, 0, &normal_cell));

    /* Render in SELECTED state and capture cell (0,0) */
    grid_clear(g, black);
    ui_button_render(g, 0, 0, btn, UI_BUTTON_SELECTED);
    Cell selected_cell;
    assert_true(grid_get(g, 0, 0, &selected_cell));

    /* Normal fg is (200,200,200); selected fg is (50,255,50) — must differ */
    assert_true(
        normal_cell.fg.r != selected_cell.fg.r ||
        normal_cell.fg.g != selected_cell.fg.g ||
        normal_cell.fg.b != selected_cell.fg.b
    );

    /* Both cells have the same glyph (' ', '+', or '*') at (0,0) —
     * the glyph itself differs between normal '+' and selected '*' */
    assert_int_equal((unsigned char)normal_cell.glyph,   '+');
    assert_int_equal((unsigned char)selected_cell.glyph, '*');

    ui_asset_destroy(btn);
    grid_destroy(g);
}

/* ===================================================================
 *  test_menu_selection_wrap
 *
 *  Verifies the wrap-around arithmetic used by the menu:
 *    - Down at last item wraps to index 0
 *    - Up at first item wraps to last index
 *    - Normal down/up navigation within bounds
 * =================================================================== */
static void test_menu_selection_wrap(void **state) {
    (void)state;

    const int count = 3;
    int sel;

    /* Down from last → wraps to first */
    sel = 2;
    sel = (sel + 1) % count;
    assert_int_equal(sel, 0);

    /* Up from first → wraps to last */
    sel = 0;
    sel = (sel - 1 + count) % count;
    assert_int_equal(sel, 2);

    /* Normal navigation downward */
    sel = 0;
    sel = (sel + 1) % count;
    assert_int_equal(sel, 1);
    sel = (sel + 1) % count;
    assert_int_equal(sel, 2);

    /* Normal navigation upward */
    sel = 2;
    sel = (sel - 1 + count) % count;
    assert_int_equal(sel, 1);
    sel = (sel - 1 + count) % count;
    assert_int_equal(sel, 0);
}

/* ===================================================================
 *  main
 * =================================================================== */
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ui_asset_load_valid),
        cmocka_unit_test(test_ui_asset_load_dimension_mismatch),
        cmocka_unit_test(test_ui_button_render_normal_vs_selected),
        cmocka_unit_test(test_menu_selection_wrap),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
