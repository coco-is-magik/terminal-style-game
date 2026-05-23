/**
 * test_asset_designer.c — Tests for asset_designer.h (init, update, save/load)
 *
 * Covers:
 *   - Default initialisation (canvas size, cursor, dirty flag, pattern)
 *   - Canvas size clamping (high and low out-of-range values)
 *   - Cursor movement in all four directions
 *   - Cursor clamping at canvas edges
 *   - Glyph placement marks dirty; correct glyph written
 *   - Erase sets space and marks dirty
 *   - No dirty change when placing space over a space cell
 *   - Glyph palette cycling (forward, backward, wrap)
 *   - ESC on clean state returns AD_RESULT_EXIT
 *   - ESC on dirty state returns AD_RESULT_CONFIRM_DISCARD
 *   - F5 save / F9 load round-trip preserves pattern
 *   - asset_designer_destroy() is idempotent (double-call safe)
 *
 * Note: asset_designer_render() requires an initialised Grid (SDL), so it
 * is not tested here.  Input tests use a heap-free InputState mock.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/asset_designer.h"
#include "../src/decal.h"
#include "../src/decal_io.h"
#include "../src/config.h"
#include "../src/input.h"

/* Path where F5 autosave writes — relative to project root */
#define TMP_AUTOSAVE "assets/decals/autosave.txt"

/* ===================================================================
 *  Helpers
 * =================================================================== */

/**
 * make_cfg() — Return a config with specific canvas dimensions.
 *
 * Calls config_init_defaults() to populate all other fields, then overrides
 * only the canvas size.  This avoids depending on a config.ini file being
 * present in the test environment.
 */
static EngineConfig make_cfg(int cols, int rows) {
    config_init_defaults();
    EngineConfig cfg = *config_get();
    cfg.asset_canvas_cols = cols;
    cfg.asset_canvas_rows = rows;
    return cfg;
}

/** Return a zeroed InputState — nothing pressed. */
static InputState no_input(void) {
    InputState in;
    memset(&in, 0, sizeof(in));
    return in;
}

/* ===================================================================
 *  Tests — Initialisation
 * =================================================================== */

/**
 * test_init_defaults — Fresh init with 20×12 config.
 *
 * Verifies canvas size, cursor start, dirty=0, pattern allocation,
 * decal surface default, and that all cells begin as space.
 */
static void test_init_defaults(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(20, 12);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    assert_int_equal(ad.canvas_cols,              20);
    assert_int_equal(ad.canvas_rows,              12);
    assert_int_equal(ad.cursor_col,                0);
    assert_int_equal(ad.cursor_row,                0);
    assert_int_equal(ad.dirty,                     0);
    assert_non_null(ad.decal.pattern);
    assert_int_equal((int)ad.decal.surface, (int)DECAL_SURFACE_WALL);
    assert_int_equal(ad.decal.pattern_cols,       20);
    assert_int_equal(ad.decal.pattern_rows,       12);

    for (int i = 0; i < 20 * 12; i++) {
        assert_int_equal(ad.decal.pattern[i].glyph, ' ');
        assert_int_equal(ad.decal.pattern[i].material_id, 1);
    }

    asset_designer_destroy(&ad);
}

/**
 * test_init_clamps_large — Canvas exceeding AD_MAX_CANVAS_* is clamped.
 */
static void test_init_clamps_large(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(999, 999);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    assert_int_equal(ad.canvas_cols, AD_MAX_CANVAS_COLS);
    assert_int_equal(ad.canvas_rows, AD_MAX_CANVAS_ROWS);

    asset_designer_destroy(&ad);
}

/**
 * test_init_clamps_zero_or_negative — Values ≤ 0 clamp to 1.
 */
static void test_init_clamps_small(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(0, -5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    assert_int_equal(ad.canvas_cols, 1);
    assert_int_equal(ad.canvas_rows, 1);

    asset_designer_destroy(&ad);
}

/* ===================================================================
 *  Tests — Cursor movement
 * =================================================================== */

/**
 * test_cursor_moves_all_directions — Each arrow key moves the cursor one cell.
 */
static void test_cursor_moves_all_directions(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(10, 8);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    InputState in;

    /* Right */
    in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.cursor_col, 1);
    assert_int_equal(ad.cursor_row, 0);

    /* Down */
    in = no_input();
    in.down = true;
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.cursor_col, 1);
    assert_int_equal(ad.cursor_row, 1);

    /* Left */
    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.cursor_col, 0);
    assert_int_equal(ad.cursor_row, 1);

    /* Up */
    in = no_input();
    in.up = true;
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.cursor_col, 0);
    assert_int_equal(ad.cursor_row, 0);

    asset_designer_destroy(&ad);
}

/**
 * test_cursor_clamps_at_zero — Arrow keys at (0,0) don't underflow.
 */
static void test_cursor_clamps_at_zero(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    InputState in;

    in = no_input(); in.up = true;
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.cursor_row, 0);

    in = no_input(); in.arrow_left = true;
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.cursor_col, 0);

    asset_designer_destroy(&ad);
}

/**
 * test_cursor_clamps_at_max — Arrow keys at max position don't overflow.
 */
static void test_cursor_clamps_at_max(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.cursor_col = 4;
    ad.cursor_row = 4;

    InputState in;

    in = no_input(); in.down = true;
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.cursor_row, 4);

    in = no_input(); in.arrow_right = true;
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.cursor_col, 4);

    asset_designer_destroy(&ad);
}

/* ===================================================================
 *  Tests — Place and erase
 * =================================================================== */

/**
 * test_place_glyph_marks_dirty — Placing a non-space glyph sets dirty=1
 * and writes the correct glyph to the pattern cell.
 */
static void test_place_glyph_marks_dirty(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* Default glyph_idx = 2 → '#' in " .#@XO+-=*" */
    assert_int_equal(ad.glyph_idx, 2);
    assert_int_equal(AD_GLYPHS[2], '#');
    assert_int_equal(ad.dirty, 0);

    InputState in = no_input();
    in.place = true;
    asset_designer_update(&ad, &in);

    assert_int_equal(ad.dirty, 1);
    assert_int_equal(ad.decal.pattern[0].glyph, '#');

    asset_designer_destroy(&ad);
}

/**
 * test_erase_marks_dirty — Erasing a non-space cell sets dirty=1
 * and writes space back.
 */
static void test_erase_marks_dirty(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.decal.pattern[0].glyph = 'X';
    ad.dirty = 0;

    InputState in = no_input();
    in.erase = true;
    asset_designer_update(&ad, &in);

    assert_int_equal(ad.dirty, 1);
    assert_int_equal(ad.decal.pattern[0].glyph, ' ');

    asset_designer_destroy(&ad);
}

/**
 * test_place_space_no_dirty — Placing space over an already-space cell
 * does not change dirty.
 */
static void test_place_space_no_dirty(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* Glyph index 0 = space */
    ad.glyph_idx = 0;
    assert_int_equal(AD_GLYPHS[0], ' ');

    InputState in = no_input();
    in.place = true;
    asset_designer_update(&ad, &in);

    assert_int_equal(ad.dirty, 0);

    asset_designer_destroy(&ad);
}

/* ===================================================================
 *  Tests — Glyph palette
 * =================================================================== */

/**
 * test_glyph_cycle_forward — E key advances the glyph index.
 */
static void test_glyph_cycle_forward(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    assert_int_equal(ad.glyph_idx, 2);

    InputState in = no_input();
    in.next_glyph = true;
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.glyph_idx, 3);

    asset_designer_destroy(&ad);
}

/**
 * test_glyph_cycle_backward — Q key decrements the glyph index.
 */
static void test_glyph_cycle_backward(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    assert_int_equal(ad.glyph_idx, 2);

    InputState in = no_input();
    in.prev_glyph = true;
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.glyph_idx, 1);

    asset_designer_destroy(&ad);
}

/**
 * test_glyph_cycle_wraps — Forward wraps from last to 0; backward from 0 to last.
 */
static void test_glyph_cycle_wraps(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    InputState in;

    /* Forward wrap: last → 0 */
    ad.glyph_idx = AD_GLYPH_COUNT - 1;
    in = no_input(); in.next_glyph = true;
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.glyph_idx, 0);

    /* Backward wrap: 0 → last */
    ad.glyph_idx = 0;
    in = no_input(); in.prev_glyph = true;
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.glyph_idx, AD_GLYPH_COUNT - 1);

    asset_designer_destroy(&ad);
}

/* ===================================================================
 *  Tests — Exit results
 * =================================================================== */

/**
 * test_esc_clean_returns_exit — ESC on a clean (dirty=0) state returns EXIT.
 */
static void test_esc_clean_returns_exit(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    assert_int_equal(ad.dirty, 0);

    InputState in = no_input();
    in.esc = true;
    AssetDesignerResult result = asset_designer_update(&ad, &in);
    assert_int_equal((int)result, (int)AD_RESULT_EXIT);

    asset_designer_destroy(&ad);
}

/**
 * test_esc_dirty_returns_confirm — ESC on a dirty state returns CONFIRM_DISCARD.
 */
static void test_esc_dirty_returns_confirm(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.dirty = 1;

    InputState in = no_input();
    in.esc = true;
    AssetDesignerResult result = asset_designer_update(&ad, &in);
    assert_int_equal((int)result, (int)AD_RESULT_CONFIRM_DISCARD);

    asset_designer_destroy(&ad);
}

/* ===================================================================
 *  Tests — Save / Load
 * =================================================================== */

/**
 * test_save_load_roundtrip — F5 saves to autosave; F9 reloads it.
 *
 * Places known glyphs, saves, corrupts the canvas, reloads, then
 * verifies the original glyphs are restored.
 */
static void test_save_load_roundtrip(void **state) {
    (void)state;

    /* Remove any stale autosave from a previous test run */
    remove(TMP_AUTOSAVE);

    EngineConfig cfg = make_cfg(4, 3);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* Paint known glyphs at specific indices */
    ad.decal.pattern[0].glyph = 'A';
    ad.decal.pattern[1].glyph = 'B';
    ad.decal.pattern[4].glyph = 'C';  /* row 1, col 0 of a 4-wide canvas */
    ad.dirty = 1;

    /* Press F5 to autosave */
    InputState in = no_input();
    in.save = true;
    asset_designer_update(&ad, &in);

    /* After save the dirty flag must be cleared */
    assert_int_equal(ad.dirty, 0);

    /* Corrupt the in-memory canvas to prove the reload actually fires */
    for (int i = 0; i < 4 * 3; i++) {
        ad.decal.pattern[i].glyph = 'Z';
    }

    /* Press F9 to autoload */
    in = no_input();
    in.load = true;
    asset_designer_update(&ad, &in);

    /* Verify the original pattern was restored */
    assert_int_equal(ad.dirty, 0);
    assert_int_equal(ad.decal.pattern[0].glyph, 'A');
    assert_int_equal(ad.decal.pattern[1].glyph, 'B');
    assert_int_equal(ad.decal.pattern[4].glyph, 'C');

    asset_designer_destroy(&ad);
    remove(TMP_AUTOSAVE);
}

/**
 * test_save_no_dirty_no_overwrite — F5 when dirty=0 shows a status message
 * and does NOT create a file if one did not exist.
 */
static void test_save_no_dirty_no_overwrite(void **state) {
    (void)state;

    remove(TMP_AUTOSAVE);

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    assert_int_equal(ad.dirty, 0);

    InputState in = no_input();
    in.save = true;
    asset_designer_update(&ad, &in);

    /* File must NOT have been created (no dirty changes) */
    FILE *f = fopen(TMP_AUTOSAVE, "r");
    assert_null(f);

    asset_designer_destroy(&ad);
}

/**
 * test_load_missing_no_crash — F9 when autosave does not exist sets a status
 * message but does not crash or corrupt state.
 */
static void test_load_missing_no_crash(void **state) {
    (void)state;

    remove(TMP_AUTOSAVE);

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    InputState in = no_input();
    in.load = true;
    asset_designer_update(&ad, &in);

    /* State must still be valid */
    assert_non_null(ad.decal.pattern);
    assert_int_equal(ad.canvas_cols, 5);
    assert_int_equal(ad.canvas_rows, 5);

    asset_designer_destroy(&ad);
}

/* ===================================================================
 *  Tests — Destroy
 * =================================================================== */

/**
 * test_destroy_idempotent — Calling destroy twice must not crash.
 *
 * The second call sees pattern == NULL; free(NULL) is defined behaviour,
 * so this must be safe.
 */
static void test_destroy_idempotent(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    asset_designer_destroy(&ad);  /* frees pattern, sets to NULL */
    asset_designer_destroy(&ad);  /* pattern is NULL: free(NULL) is safe */
}

/* ===================================================================
 *  Entry point
 * =================================================================== */

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_defaults),
        cmocka_unit_test(test_init_clamps_large),
        cmocka_unit_test(test_init_clamps_small),
        cmocka_unit_test(test_cursor_moves_all_directions),
        cmocka_unit_test(test_cursor_clamps_at_zero),
        cmocka_unit_test(test_cursor_clamps_at_max),
        cmocka_unit_test(test_place_glyph_marks_dirty),
        cmocka_unit_test(test_erase_marks_dirty),
        cmocka_unit_test(test_place_space_no_dirty),
        cmocka_unit_test(test_glyph_cycle_forward),
        cmocka_unit_test(test_glyph_cycle_backward),
        cmocka_unit_test(test_glyph_cycle_wraps),
        cmocka_unit_test(test_esc_clean_returns_exit),
        cmocka_unit_test(test_esc_dirty_returns_confirm),
        cmocka_unit_test(test_save_load_roundtrip),
        cmocka_unit_test(test_save_no_dirty_no_overwrite),
        cmocka_unit_test(test_load_missing_no_crash),
        cmocka_unit_test(test_destroy_idempotent),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
