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
 *   - Pattern resize via metadata row 6/7 (cols/rows)
 *   - Direct glyph typing (printable ASCII in canvas focus)
 *   - asset_designer_render() with NULL and valid AssetRegistry
 *
 * Note: asset_designer_render() requires an initialised Grid (SDL), so render
 * tests use a headless grid created via grid_create().
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
#include "../src/grid.h"
#include "../src/assets.h"

/* Temporary named file used by the roundtrip test */
#define TMP_NAMED_PATH  "assets/decals/test_roundtrip_tmp.txt"
#define TMP_NAMED_BASE  "test_roundtrip_tmp"

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

/** make_test_config — Convenience helper for new tests (20×12 default). */
static EngineConfig make_test_config(void) {
    return make_cfg(20, 12);
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
    assert_int_equal(ad.current_glyph,           '#');

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
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.cursor_col, 1);
    assert_int_equal(ad.cursor_row, 0);

    /* Down */
    in = no_input();
    in.down = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.cursor_col, 1);
    assert_int_equal(ad.cursor_row, 1);

    /* Left */
    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.cursor_col, 0);
    assert_int_equal(ad.cursor_row, 1);

    /* Up */
    in = no_input();
    in.up = true;
    asset_designer_update(&ad, &in, NULL);
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
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.cursor_row, 0);

    in = no_input(); in.arrow_left = true;
    asset_designer_update(&ad, &in, NULL);
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
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.cursor_row, 4);

    in = no_input(); in.arrow_right = true;
    asset_designer_update(&ad, &in, NULL);
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

    /* current_glyph defaults to '#' */
    assert_int_equal(ad.current_glyph, '#');
    assert_int_equal(ad.dirty, 0);

    InputState in = no_input();
    in.place = true;
    asset_designer_update(&ad, &in, NULL);

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
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.dirty, 1);
    assert_int_equal(ad.decal.pattern[0].glyph, ' ');

    asset_designer_destroy(&ad);
}

/**
 * test_place_space_no_dirty — The place action (Space key) paints current_glyph.
 * In the new implementation, place always marks dirty regardless of glyph value.
 * When current_glyph is '#' (default), placing paints '#' and marks dirty.
 */
static void test_place_space_no_dirty(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* Default current_glyph = '#' — placing paints '#' and marks dirty */
    assert_int_equal(ad.current_glyph, '#');
    assert_int_equal(ad.dirty, 0);

    InputState in = no_input();
    in.place = true;
    asset_designer_update(&ad, &in, NULL);

    /* Place always marks dirty and writes current_glyph */
    assert_int_equal(ad.decal.pattern[0].glyph, '#');
    assert_int_equal(ad.dirty, 1);

    asset_designer_destroy(&ad);
}

/* ===================================================================
 *  Tests — Glyph palette
 * =================================================================== */

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
    AssetDesignerResult result = asset_designer_update(&ad, &in, NULL);
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
    AssetDesignerResult result = asset_designer_update(&ad, &in, NULL);
    assert_int_equal((int)result, (int)AD_RESULT_CONFIRM_DISCARD);

    asset_designer_destroy(&ad);
}

/* ===================================================================
 *  Tests — Save / Load
 * =================================================================== */

/**
 * test_save_load_roundtrip — Named save via F5, named load via load-select.
 *
 * Places known glyphs, sets current_filename so F5 saves directly,
 * corrupts the canvas, manually sets the load-select list to the same file,
 * presses Enter to load, and verifies the pattern is restored.
 */
static void test_save_load_roundtrip(void **state) {
    (void)state;

    remove(TMP_NAMED_PATH);

    EngineConfig cfg = make_cfg(4, 3);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* Pre-set current_filename so F5 saves directly without entering prompt */
    strncpy(ad.current_filename, TMP_NAMED_BASE, AD_FILENAME_MAX - 1);

    /* Paint known glyphs */
    ad.decal.pattern[0].glyph = 'A';
    ad.decal.pattern[1].glyph = 'B';
    ad.decal.pattern[4].glyph = 'C';  /* row 1, col 0 of a 4-wide canvas */
    ad.dirty = 1;

    /* F5 — saves directly because current_filename is set */
    InputState in = no_input();
    in.save = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.dirty, 0);

    /* Corrupt canvas to prove the reload actually fires */
    for (int i = 0; i < 4 * 3; i++) {
        ad.decal.pattern[i].glyph = 'Z';
    }

    /* Manually populate the file list (avoids needing opendir in tests) */
    strncpy(ad.file_list[0], TMP_NAMED_BASE, AD_FILENAME_MAX - 1);
    ad.file_count   = 1;
    ad.file_sel_idx = 0;
    ad.mode = AD_LOAD_SELECT;

    /* Enter — load the selected file */
    in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in, NULL);

    /* Pattern must be restored */
    assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);
    assert_int_equal(ad.dirty, 0);
    assert_int_equal(ad.decal.pattern[0].glyph, 'A');
    assert_int_equal(ad.decal.pattern[1].glyph, 'B');
    assert_int_equal(ad.decal.pattern[4].glyph, 'C');

    /* current_filename must be set to the loaded basename */
    assert_string_equal(ad.current_filename, TMP_NAMED_BASE);

    asset_designer_destroy(&ad);
    remove(TMP_NAMED_PATH);
}

/**
 * test_f5_no_filename_enters_save_prompt — F5 with no current_filename opens
 * the save-prompt sub-mode instead of writing a file.
 */
static void test_f5_no_filename_enters_save_prompt(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* No filename set */
    assert_int_equal(ad.current_filename[0], '\0');

    InputState in = no_input();
    in.save = true;
    asset_designer_update(&ad, &in, NULL);

    /* Must enter save-prompt, not stay in edit mode */
    assert_int_equal(ad.mode, (int)AD_SAVE_PROMPT);
    /* filename_buffer must be empty (no prefill with no current name) */
    assert_int_equal(ad.filename_buffer[0], '\0');
    /* Dirty flag is unchanged (nothing was saved) */
    assert_int_equal(ad.dirty, 0);

    asset_designer_destroy(&ad);
}

/**
 * test_f9_enters_load_select — F9 opens the load-select sub-mode.
 * Esc returns to edit mode without loading anything.
 */
static void test_f9_enters_load_select(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* F9 — enter load selector (scan result may be empty, that's fine) */
    InputState in = no_input();
    in.load = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.mode, (int)AD_LOAD_SELECT);
    assert_non_null(ad.decal.pattern);

    /* Esc — cancel, return to edit mode */
    in = no_input();
    in.esc = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);
    assert_int_equal(ad.canvas_cols, 5);
    assert_int_equal(ad.canvas_rows, 5);

    asset_designer_destroy(&ad);
}

/* ===================================================================
 *  Tests — Surface=All cycle and export
 * =================================================================== */

/**
 * test_surface_all_in_cycle — surface cycle passes through all four values
 * including AD_SURFACE_ALL, and wraps back to wall.
 */
static void test_surface_all_in_cycle(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.metadata_focus = 1;
    ad.metadata_row   = 0;

    InputState in;
    in = no_input(); in.arrow_right = true; asset_designer_update(&ad, &in, NULL);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_FLOOR);
    in = no_input(); in.arrow_right = true; asset_designer_update(&ad, &in, NULL);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_CEILING);
    in = no_input(); in.arrow_right = true; asset_designer_update(&ad, &in, NULL);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_ALL);
    in = no_input(); in.arrow_right = true; asset_designer_update(&ad, &in, NULL);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_WALL);

    asset_designer_destroy(&ad);
}

/**
 * test_save_all_creates_three_files — saving with surface=All writes
 * basename_wall.txt, basename_floor.txt, and basename_ceil.txt.
 */
static void test_save_all_creates_three_files(void **state) {
    (void)state;

    const char *basename = "test_all_export_tmp";
    char paths[3][128];
    const char *suffixes[3] = { "wall", "floor", "ceil" };
    for (int i = 0; i < 3; i++) {
        snprintf(paths[i], sizeof(paths[i]),
                 "%s%s_%s.txt", AD_DECALS_DIR, basename, suffixes[i]);
        remove(paths[i]);
    }

    EngineConfig cfg = make_cfg(4, 3);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.editor_surface = AD_SURFACE_ALL;
    ad.decal.pattern[0].glyph = 'A';
    ad.dirty = 1;

    /* Set filename and trigger save via save-prompt commit */
    ad.mode = AD_SAVE_PROMPT;
    strncpy(ad.filename_buffer, basename, AD_FILENAME_MAX - 1);
    ad.filename_pos = (int)strlen(basename);

    InputState in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);
    assert_int_equal(ad.dirty, 0);

    for (int i = 0; i < 3; i++) {
        FILE *f = fopen(paths[i], "r");
        assert_non_null(f);
        if (f) fclose(f);
        remove(paths[i]);
    }

    asset_designer_destroy(&ad);
}

/**
 * test_save_all_files_have_correct_surface — each file exported from
 * surface=All contains the correct concrete surface value.
 */
static void test_save_all_files_have_correct_surface(void **state) {
    (void)state;

    const char *basename = "test_all_surf_tmp";
    const char *suffixes[3]  = { "wall", "floor", "ceil" };
    DecalSurface expected[3] = { DECAL_SURFACE_WALL, DECAL_SURFACE_FLOOR, DECAL_SURFACE_CEILING };
    char paths[3][128];
    for (int i = 0; i < 3; i++) {
        snprintf(paths[i], sizeof(paths[i]),
                 "%s%s_%s.txt", AD_DECALS_DIR, basename, suffixes[i]);
        remove(paths[i]);
    }

    EngineConfig cfg = make_cfg(3, 2);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.editor_surface = AD_SURFACE_ALL;
    ad.dirty = 1;
    ad.mode = AD_SAVE_PROMPT;
    strncpy(ad.filename_buffer, basename, AD_FILENAME_MAX - 1);
    ad.filename_pos = (int)strlen(basename);

    InputState in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in, NULL);

    for (int i = 0; i < 3; i++) {
        Decal *loaded = decal_load_from_file(paths[i]);
        assert_non_null(loaded);
        assert_int_equal((int)loaded->surface, (int)expected[i]);
        decal_free(loaded);
        remove(paths[i]);
    }

    asset_designer_destroy(&ad);
}

/**
 * test_load_one_copy_sets_concrete — loading a _wall.txt sets
 * editor_surface=AD_SURFACE_WALL and decal.surface=DECAL_SURFACE_WALL.
 * Loading _floor.txt sets editor_surface=AD_SURFACE_FLOOR.
 */
static void test_load_one_copy_sets_concrete(void **state) {
    (void)state;

    /* First write a wall file using All export */
    const char *basename = "test_load_concrete_tmp";
    char wall_path[128];
    snprintf(wall_path, sizeof(wall_path),
             "%s%s_wall.txt", AD_DECALS_DIR, basename);
    remove(wall_path);

    EngineConfig cfg = make_cfg(3, 2);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.editor_surface = AD_SURFACE_ALL;
    ad.dirty = 1;
    ad.mode = AD_SAVE_PROMPT;
    strncpy(ad.filename_buffer, basename, AD_FILENAME_MAX - 1);
    ad.filename_pos = (int)strlen(basename);
    InputState in = no_input(); in.confirm = true;
    asset_designer_update(&ad, &in, NULL);

    /* Now load the _wall file into a fresh state */
    AssetDesignerState ad2;
    asset_designer_init(&ad2, &cfg, APP_STATE_MAIN_MENU);

    /* Manually populate file list with the wall basename, mode=load */
    char wall_base[AD_FILENAME_MAX];
    snprintf(wall_base, sizeof(wall_base), "%s_wall", basename);
    strncpy(ad2.file_list[0], wall_base, AD_FILENAME_MAX - 1);
    ad2.file_count   = 1;
    ad2.file_sel_idx = 0;
    ad2.mode = AD_LOAD_SELECT;

    in = no_input(); in.confirm = true;
    asset_designer_update(&ad2, &in, NULL);

    assert_int_equal((int)ad2.editor_surface, (int)AD_SURFACE_WALL);
    assert_int_equal((int)ad2.decal.surface,  (int)DECAL_SURFACE_WALL);
    assert_int_equal(ad2.mode, (int)AD_DECAL_EDIT);

    /* Cleanup */
    char floor_path[128], ceil_path[128];
    snprintf(floor_path, sizeof(floor_path), "%s%s_floor.txt", AD_DECALS_DIR, basename);
    snprintf(ceil_path,  sizeof(ceil_path),  "%s%s_ceil.txt",  AD_DECALS_DIR, basename);
    remove(wall_path);
    remove(floor_path);
    remove(ceil_path);

    asset_designer_destroy(&ad);
    asset_designer_destroy(&ad2);
}

/**
 * test_save_all_does_not_mutate_editor_state — After saving with surface=All,
 * editor_surface must still be AD_SURFACE_ALL and decal.surface must not have
 * been modified by the export loop.
 */
static void test_save_all_does_not_mutate_editor_state(void **state) {
    (void)state;

    const char *basename = "test_all_nomut_tmp";
    EngineConfig cfg = make_cfg(3, 2);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* Set a known concrete decal.surface BEFORE All export */
    ad.decal.surface  = DECAL_SURFACE_FLOOR;   /* known sentinel value */
    ad.editor_surface = AD_SURFACE_ALL;
    ad.dirty = 1;
    ad.mode = AD_SAVE_PROMPT;
    strncpy(ad.filename_buffer, basename, AD_FILENAME_MAX - 1);
    ad.filename_pos = (int)strlen(basename);

    InputState in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in, NULL);

    /* editor_surface must remain AD_SURFACE_ALL after export */
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_ALL);
    /* decal.surface must remain the sentinel FLOOR — not mutated by export */
    assert_int_equal((int)ad.decal.surface, (int)DECAL_SURFACE_FLOOR);

    /* Cleanup the three exported files */
    const char *suffixes[3] = { "wall", "floor", "ceil" };
    for (int i = 0; i < 3; i++) {
        char path[128];
        snprintf(path, sizeof(path), "%s%s_%s.txt", AD_DECALS_DIR, basename, suffixes[i]);
        remove(path);
    }

    asset_designer_destroy(&ad);
}

/* ===================================================================
 *  Tests — Metadata direct-edit (AD_META_EDIT mode)
 * =================================================================== */

/**
 * test_meta_edit_enter_enters_mode — Enter on metadata row transitions to
 * AD_META_EDIT and pre-fills the buffer with the current field value.
 */
static void test_meta_edit_enter_enters_mode(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.metadata_focus = 1;
    ad.metadata_row   = 1;   /* width row */
    ad.decal.width    = 1.0;

    InputState in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.mode, (int)AD_META_EDIT);
    /* Buffer pre-filled with "1.0000" (%.4f format) */
    assert_string_equal(ad.meta_edit_buffer, "1.0000");
    assert_int_equal(ad.meta_edit_len, 6);
    /* Snapshot set */
    assert_true(ad.meta_prev_double == 1.0);

    asset_designer_destroy(&ad);
}

/**
 * test_meta_edit_width_valid_commits — entering a valid positive float
 * commits the new width value and returns to AD_DECAL_EDIT.
 */
static void test_meta_edit_width_valid_commits(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* Enter meta-edit mode for width (row 1) */
    ad.metadata_focus = 1;
    ad.metadata_row   = 1;
    InputState in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.mode, (int)AD_META_EDIT);

    /* Clear the pre-filled buffer by faking deletion, then type "3.5" */
    /* Easiest: set buffer directly (testing parse logic, not typing) */
    strncpy(ad.meta_edit_buffer, "3.5", sizeof(ad.meta_edit_buffer) - 1);
    ad.meta_edit_len = 3;

    /* Commit */
    in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);
    assert_true(ad.decal.width == 3.5);
    assert_int_equal(ad.dirty, 1);

    asset_designer_destroy(&ad);
}

/**
 * test_meta_edit_width_invalid_rejects — entering an invalid value (negative
 * or non-numeric) does not change width and returns to AD_DECAL_EDIT.
 */
static void test_meta_edit_width_invalid_rejects(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    const char *bad_vals[] = { "-1", "0", "abc", "" };
    size_t n = sizeof(bad_vals) / sizeof(bad_vals[0]);

    for (size_t i = 0; i < n; i++) {
        ad.metadata_focus = 1;
        ad.metadata_row   = 1;
        ad.decal.width    = 2.0;

        /* Enter meta-edit */
        InputState in = no_input();
        in.confirm = true;
        asset_designer_update(&ad, &in, NULL);
        assert_int_equal(ad.mode, (int)AD_META_EDIT);

        strncpy(ad.meta_edit_buffer, bad_vals[i], sizeof(ad.meta_edit_buffer) - 1);
        ad.meta_edit_len = (int)strlen(bad_vals[i]);

        /* Commit */
        in = no_input();
        in.confirm = true;
        asset_designer_update(&ad, &in, NULL);

        assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);
        assert_true(ad.decal.width == 2.0);   /* unchanged */
        assert_true(ad.status_frames > 0);    /* error status set */
    }

    asset_designer_destroy(&ad);
}

/**
 * test_meta_edit_surface_accepts_all_values — direct edit of surface accepts
 * "wall", "floor", "ceil", and "all".
 */
static void test_meta_edit_surface_accepts_all_values(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);

    const char *vals[4] = { "wall", "floor", "ceil", "all" };
    AdSurface   exp[4]  = { AD_SURFACE_WALL, AD_SURFACE_FLOOR,
                             AD_SURFACE_CEILING, AD_SURFACE_ALL };

    for (int i = 0; i < 4; i++) {
        AssetDesignerState ad;
        asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
        ad.metadata_focus = 1;
        ad.metadata_row   = 0;

        /* Enter meta-edit */
        InputState in = no_input();
        in.confirm = true;
        asset_designer_update(&ad, &in, NULL);
        assert_int_equal(ad.mode, (int)AD_META_EDIT);

        strncpy(ad.meta_edit_buffer, vals[i], sizeof(ad.meta_edit_buffer) - 1);
        ad.meta_edit_len = (int)strlen(vals[i]);

        in = no_input();
        in.confirm = true;
        asset_designer_update(&ad, &in, NULL);

        assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);
        assert_int_equal((int)ad.editor_surface, (int)exp[i]);

        asset_designer_destroy(&ad);
    }
}

/**
 * test_meta_edit_surface_rejects_invalid — direct edit rejects unknown surface names.
 */
static void test_meta_edit_surface_rejects_invalid(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.metadata_focus = 1;
    ad.metadata_row   = 0;
    ad.editor_surface = AD_SURFACE_WALL;

    InputState in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.mode, (int)AD_META_EDIT);

    strncpy(ad.meta_edit_buffer, "bogus", sizeof(ad.meta_edit_buffer) - 1);
    ad.meta_edit_len = 5;

    in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_WALL);  /* unchanged */
    assert_true(ad.status_frames > 0);

    asset_designer_destroy(&ad);
}

/**
 * test_meta_edit_material_id_accepts — direct edit accepts integers 1..255.
 */
static void test_meta_edit_material_id_accepts(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.metadata_focus = 1;
    ad.metadata_row   = 5;

    InputState in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.mode, (int)AD_META_EDIT);

    strncpy(ad.meta_edit_buffer, "42", sizeof(ad.meta_edit_buffer) - 1);
    ad.meta_edit_len = 2;

    in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);
    assert_int_equal(ad.current_material_id, 42);

    asset_designer_destroy(&ad);
}

/**
 * test_meta_edit_material_id_rejects_bounds — rejects 0, 256, and non-numbers.
 */
static void test_meta_edit_material_id_rejects_bounds(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);

    const char *bad_vals[] = { "0", "256", "abc", "" };
    size_t n = sizeof(bad_vals) / sizeof(bad_vals[0]);

    for (size_t i = 0; i < n; i++) {
        AssetDesignerState ad;
        asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
        ad.metadata_focus      = 1;
        ad.metadata_row        = 5;
        ad.current_material_id = 7;  /* sentinel */

        InputState in = no_input();
        in.confirm = true;
        asset_designer_update(&ad, &in, NULL);
        assert_int_equal(ad.mode, (int)AD_META_EDIT);

        strncpy(ad.meta_edit_buffer, bad_vals[i], sizeof(ad.meta_edit_buffer) - 1);
        ad.meta_edit_len = (int)strlen(bad_vals[i]);

        in = no_input();
        in.confirm = true;
        asset_designer_update(&ad, &in, NULL);

        assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);
        assert_int_equal(ad.current_material_id, 7);  /* unchanged */
        assert_true(ad.status_frames > 0);

        asset_designer_destroy(&ad);
    }
}

/**
 * test_meta_edit_esc_cancels — Esc during AD_META_EDIT restores previous value
 * and returns to AD_DECAL_EDIT.
 */
static void test_meta_edit_esc_cancels(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.metadata_focus = 1;
    ad.metadata_row   = 1;  /* width */
    ad.decal.width    = 2.5;

    /* Enter meta-edit */
    InputState in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.mode, (int)AD_META_EDIT);
    assert_true(ad.meta_prev_double == 2.5);

    /* Type something */
    strncpy(ad.meta_edit_buffer, "99.0", sizeof(ad.meta_edit_buffer) - 1);
    ad.meta_edit_len = 4;

    /* Esc — cancel */
    in = no_input();
    in.esc = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);
    /* Width must be restored to 2.5 (from snapshot) */
    assert_true(ad.decal.width == 2.5);
    assert_int_equal(ad.meta_edit_len, 0);

    asset_designer_destroy(&ad);
}

/**
 * test_meta_edit_text_input_chars — valid chars appended; invalid chars filtered.
 */
static void test_meta_edit_text_input_chars(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.mode = AD_META_EDIT;
    ad.meta_edit_buffer[0] = '\0';
    ad.meta_edit_len = 0;

    /* Valid chars: digits, minus, dot, lowercase letters */
    /* Invalid chars: uppercase, space, special */
    InputState in = no_input();
    const char *typed = "1.25 A!";
    strncpy(in.text_input, typed, sizeof(in.text_input) - 1);
    in.text_input_len = (int)strlen(typed);

    asset_designer_update(&ad, &in, NULL);

    /* Only "1.25" should have been appended (space, A, ! filtered) */
    assert_string_equal(ad.meta_edit_buffer, "1.25");
    assert_int_equal(ad.meta_edit_len, 4);

    asset_designer_destroy(&ad);
}

/**
 * test_meta_edit_backspace — backspace removes the last char.
 */
static void test_meta_edit_backspace(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.mode = AD_META_EDIT;
    strncpy(ad.meta_edit_buffer, "1.5", sizeof(ad.meta_edit_buffer) - 1);
    ad.meta_edit_len = 3;

    InputState in = no_input();
    in.erase = true;
    asset_designer_update(&ad, &in, NULL);

    assert_string_equal(ad.meta_edit_buffer, "1.");
    assert_int_equal(ad.meta_edit_len, 2);

    asset_designer_destroy(&ad);
}

/**
 * test_left_right_tweak_still_works — with metadata focus and no Enter,
 * Left/Right still cycles editor_surface as before.
 */
static void test_left_right_tweak_still_works(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.metadata_focus = 1;
    ad.metadata_row   = 0;

    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_WALL);

    InputState in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_FLOOR);
    assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);  /* must NOT have entered meta-edit */

    asset_designer_destroy(&ad);
}

/**
 * test_filename_prompts_unaffected — F5 with no current filename enters
 * AD_SAVE_PROMPT (not AD_META_EDIT). F9 enters AD_LOAD_SELECT.
 */
static void test_filename_prompts_unaffected(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);

    /* F5 -> save prompt */
    {
        AssetDesignerState ad;
        asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
        assert_int_equal(ad.current_filename[0], '\0');

        InputState in = no_input();
        in.save = true;
        asset_designer_update(&ad, &in, NULL);

        assert_int_equal(ad.mode, (int)AD_SAVE_PROMPT);
        asset_designer_destroy(&ad);
    }

    /* F9 -> load select */
    {
        AssetDesignerState ad;
        asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

        InputState in = no_input();
        in.load = true;
        asset_designer_update(&ad, &in, NULL);

        assert_int_equal(ad.mode, (int)AD_LOAD_SELECT);
        asset_designer_destroy(&ad);
    }
}

/* ===================================================================
 *  Tests — Destroy
 * =================================================================== */

/**
 * test_destroy_idempotent — Calling destroy twice must not crash.
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
 *  Tests — Filename validation
 * =================================================================== */

static void test_validate_basename_accepts_valid(void **state) {
    (void)state;
    assert_int_equal(ad_validate_basename("my_decal"),      1);
    assert_int_equal(ad_validate_basename("decal-01"),      1);
    assert_int_equal(ad_validate_basename("ABC"),           1);
    assert_int_equal(ad_validate_basename("abc123"),        1);
    assert_int_equal(ad_validate_basename("a"),             1);
    assert_int_equal(ad_validate_basename("_"),             1);
    assert_int_equal(ad_validate_basename("-"),             1);
}

static void test_validate_basename_rejects_empty(void **state) {
    (void)state;
    assert_int_equal(ad_validate_basename(""),   0);
    assert_int_equal(ad_validate_basename(NULL), 0);
}

static void test_validate_basename_rejects_dotdot(void **state) {
    (void)state;
    assert_int_equal(ad_validate_basename("."),  0);
    assert_int_equal(ad_validate_basename(".."), 0);
}

static void test_validate_basename_rejects_slash(void **state) {
    (void)state;
    assert_int_equal(ad_validate_basename("a/b"),   0);
    assert_int_equal(ad_validate_basename("a\\b"),  0);
    assert_int_equal(ad_validate_basename("/etc"),  0);
}

static void test_validate_basename_rejects_bad_chars(void **state) {
    (void)state;
    assert_int_equal(ad_validate_basename("my file"),    0);  /* space */
    assert_int_equal(ad_validate_basename("my.decal"),   0);  /* dot */
    assert_int_equal(ad_validate_basename("my!decal"),   0);  /* bang */
    assert_int_equal(ad_validate_basename("my\tdecal"),  0);  /* tab */
}

/* ===================================================================
 *  Tests — Save-prompt sub-mode
 * =================================================================== */

/**
 * test_f5_with_current_filename_saves_directly — When current_filename is set,
 * F5 saves immediately without entering save-prompt mode.
 */
static void test_f5_with_current_filename_saves_directly(void **state) {
    (void)state;

    char tmp_path[128];
    snprintf(tmp_path, sizeof(tmp_path), "%s%s.txt", AD_DECALS_DIR, TMP_NAMED_BASE);
    remove(tmp_path);

    EngineConfig cfg = make_cfg(4, 3);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    strncpy(ad.current_filename, TMP_NAMED_BASE, AD_FILENAME_MAX - 1);
    ad.decal.pattern[0].glyph = 'X';
    ad.dirty = 1;

    InputState in = no_input();
    in.save = true;
    asset_designer_update(&ad, &in, NULL);

    /* Must stay in edit mode, not enter save-prompt */
    assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);
    assert_int_equal(ad.dirty, 0);
    /* File must have been created */
    FILE *f = fopen(tmp_path, "r");
    assert_non_null(f);
    if (f) fclose(f);

    asset_designer_destroy(&ad);
    remove(tmp_path);
}

/**
 * test_f10_always_enters_save_prompt — F10 opens save-prompt even when
 * current_filename is already set.
 */
static void test_f10_always_enters_save_prompt(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* With a current filename */
    strncpy(ad.current_filename, "existing", AD_FILENAME_MAX - 1);

    InputState in = no_input();
    in.save_as = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.mode, (int)AD_SAVE_PROMPT);

    asset_designer_destroy(&ad);
}

/**
 * test_f10_prefills_current_filename — F10 pre-fills filename_buffer with
 * current_filename so the user can edit it.
 */
static void test_f10_prefills_current_filename(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    strncpy(ad.current_filename, "myfile", AD_FILENAME_MAX - 1);

    InputState in = no_input();
    in.save_as = true;
    asset_designer_update(&ad, &in, NULL);

    assert_string_equal(ad.filename_buffer, "myfile");
    assert_int_equal(ad.filename_pos, 6);

    asset_designer_destroy(&ad);
}

/**
 * test_save_prompt_text_input — Chars typed via text_input are appended to
 * filename_buffer, filtered to [A-Za-z0-9_-].
 */
static void test_save_prompt_text_input(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.mode = AD_SAVE_PROMPT;
    ad.filename_buffer[0] = '\0';
    ad.filename_pos = 0;

    /* Simulate typing "my-decal" plus invalid chars (space, dot) */
    InputState in = no_input();
    const char *typed = "my-decal .";  /* space and dot must be rejected */
    strncpy(in.text_input, typed, sizeof(in.text_input) - 1);
    in.text_input_len = (int)strlen(typed);

    asset_designer_update(&ad, &in, NULL);

    /* Only [A-Za-z0-9_-] chars survive: "my-decal" */
    assert_string_equal(ad.filename_buffer, "my-decal");
    assert_int_equal(ad.filename_pos, 8);

    asset_designer_destroy(&ad);
}

/**
 * test_save_prompt_backspace — Erase key removes the last char.
 */
static void test_save_prompt_backspace(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.mode = AD_SAVE_PROMPT;
    strncpy(ad.filename_buffer, "abc", AD_FILENAME_MAX - 1);
    ad.filename_pos = 3;

    InputState in = no_input();
    in.erase = true;
    asset_designer_update(&ad, &in, NULL);

    assert_string_equal(ad.filename_buffer, "ab");
    assert_int_equal(ad.filename_pos, 2);

    asset_designer_destroy(&ad);
}

/**
 * test_save_prompt_esc_cancels — Esc in save-prompt returns to edit mode
 * and clears the filename buffer.
 */
static void test_save_prompt_esc_cancels(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.mode = AD_SAVE_PROMPT;
    strncpy(ad.filename_buffer, "partial", AD_FILENAME_MAX - 1);
    ad.filename_pos = 7;

    InputState in = no_input();
    in.esc = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);
    assert_int_equal(ad.filename_buffer[0], '\0');
    assert_int_equal(ad.filename_pos, 0);

    asset_designer_destroy(&ad);
}

/**
 * test_save_prompt_enter_commits_valid — Enter in AD_SAVE_PROMPT with a valid
 * filename saves the file, sets current_filename, clears dirty, and returns
 * mode to AD_DECAL_EDIT.
 */
static void test_save_prompt_enter_commits_valid(void **state) {
    (void)state;

    const char *basename = "test_commit_tmp";
    char path[128];
    snprintf(path, sizeof(path), "%s%s.txt", AD_DECALS_DIR, basename);
    remove(path);

    EngineConfig cfg = make_cfg(4, 3);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* Enter save-prompt with a pre-typed valid name */
    ad.mode = AD_SAVE_PROMPT;
    strncpy(ad.filename_buffer, basename, AD_FILENAME_MAX - 1);
    ad.filename_pos = (int)strlen(basename);
    ad.dirty = 1;

    /* Press Enter */
    InputState in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in, NULL);

    /* Mode must return to edit */
    assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);
    /* Dirty must be cleared after successful save */
    assert_int_equal(ad.dirty, 0);
    /* current_filename must be set */
    assert_string_equal(ad.current_filename, basename);
    /* Buffer must be cleared after successful save */
    assert_int_equal(ad.filename_buffer[0], '\0');
    assert_int_equal(ad.filename_pos, 0);
    /* The file must exist on disk */
    FILE *f = fopen(path, "r");
    assert_non_null(f);
    if (f) fclose(f);

    asset_designer_destroy(&ad);
    remove(path);
}

/**
 * test_save_prompt_enter_rejects_invalid — Enter in AD_SAVE_PROMPT with an
 * invalid filename sets a status message and stays in save-prompt mode.
 * No file should be created.
 */
static void test_save_prompt_enter_rejects_invalid(void **state) {
    (void)state;

    const char *bad_names[] = { "", "my file", "..", "../etc", "bad.name" };
    size_t n = sizeof(bad_names) / sizeof(bad_names[0]);

    EngineConfig cfg = make_cfg(4, 3);

    for (size_t i = 0; i < n; i++) {
        AssetDesignerState ad;
        asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

        ad.mode = AD_SAVE_PROMPT;
        strncpy(ad.filename_buffer, bad_names[i], AD_FILENAME_MAX - 1);
        ad.filename_pos = (int)strlen(bad_names[i]);

        InputState in = no_input();
        in.confirm = true;
        asset_designer_update(&ad, &in, NULL);

        /* Must stay in save-prompt */
        assert_int_equal(ad.mode, (int)AD_SAVE_PROMPT);
        /* Status message must be set */
        assert_true(ad.status_frames > 0);

        asset_designer_destroy(&ad);
    }
}

/**
 * test_init_mode_and_filename — Fresh init has AD_DECAL_EDIT mode and
 * empty current_filename.
 */
static void test_init_mode_and_filename(void **state) {
    (void)state;

    EngineConfig cfg = make_cfg(10, 8);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);
    assert_int_equal(ad.current_filename[0], '\0');
    assert_int_equal(ad.filename_buffer[0],  '\0');
    assert_int_equal(ad.filename_pos, 0);

    asset_designer_destroy(&ad);
}

/* ===================================================================
 *  Tests — Metadata panel (Tab focus, field editing, dirty flag)
 * =================================================================== */

/**
 * test_init_current_material_id — Fresh init sets current_material_id = 1.
 */
static void test_init_current_material_id(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    assert_int_equal(ad.current_material_id, 1);
    assert_int_equal(ad.metadata_focus, 0);
    assert_int_equal(ad.metadata_row, 0);
    asset_designer_destroy(&ad);
}

/**
 * test_metadata_focus_tab — Tab toggles metadata_focus between 0 and 1.
 */
static void test_metadata_focus_tab(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    assert_int_equal(ad.metadata_focus, 0);

    InputState in = no_input();
    in.tab = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.metadata_focus, 1);

    in = no_input();
    in.tab = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.metadata_focus, 0);

    asset_designer_destroy(&ad);
}

/**
 * test_metadata_canvas_arrows_blocked — When focused on metadata, arrow keys
 * do NOT move the canvas cursor.
 */
static void test_metadata_canvas_arrows_blocked(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(10, 8);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.metadata_focus = 1;

    InputState in = no_input();
    in.arrow_right = true;
    in.down        = true;
    asset_designer_update(&ad, &in, NULL);

    /* Canvas cursor must not have moved */
    assert_int_equal(ad.cursor_col, 0);
    assert_int_equal(ad.cursor_row, 0);

    asset_designer_destroy(&ad);
}

/**
 * test_metadata_row_up_down — Up/Down navigates metadata rows while focused.
 */
static void test_metadata_row_up_down(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.metadata_focus = 1;
    ad.metadata_row   = 0;

    InputState in = no_input();
    in.down = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.metadata_row, 1);

    in = no_input();
    in.down = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.metadata_row, 2);

    in = no_input();
    in.up = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.metadata_row, 1);

    asset_designer_destroy(&ad);
}

/**
 * test_metadata_row_clamps — metadata_row stays in [0, AD_META_EDIT_COUNT-1].
 */
static void test_metadata_row_clamps(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.metadata_focus = 1;
    ad.metadata_row   = 0;

    /* Up at row 0 stays 0 */
    InputState in = no_input();
    in.up = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.metadata_row, 0);

    /* Down to last row, then Down again stays at last row */
    ad.metadata_row = AD_META_EDIT_COUNT - 1;
    in = no_input();
    in.down = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.metadata_row, AD_META_EDIT_COUNT - 1);

    asset_designer_destroy(&ad);
}

/**
 * test_metadata_surface_cycle_right — Right cycles surface wall->floor->ceil->wall.
 */
static void test_metadata_surface_cycle_right(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.metadata_focus = 1;
    ad.metadata_row   = 0;  /* surface row */

    /* editor_surface cycles: wall(0) -> floor(1) -> ceil(2) -> all(3) -> wall(0) */
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_WALL);

    InputState in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_FLOOR);

    in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_CEILING);

    in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_ALL);

    in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_WALL);

    /* decal.surface must NOT have been modified */
    assert_int_equal((int)ad.decal.surface, (int)DECAL_SURFACE_WALL);

    asset_designer_destroy(&ad);
}

/**
 * test_metadata_surface_cycle_left — Left wraps backward ceil->floor->wall->ceil.
 */
static void test_metadata_surface_cycle_left(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.metadata_focus = 1;
    ad.metadata_row   = 0;

    /* Starting at WALL(0): Left wraps to ALL(3) (mod 4). */
    InputState in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_ALL);

    /* ALL(3) - 1 = CEILING(2) */
    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_CEILING);

    /* decal.surface must NOT have been modified */
    assert_int_equal((int)ad.decal.surface, (int)DECAL_SURFACE_WALL);

    asset_designer_destroy(&ad);
}

/**
 * test_metadata_width_adjust — Right increases width by 0.25; Left decreases;
 * width clamps at 0.25 minimum.
 */
static void test_metadata_width_adjust(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.metadata_focus = 1;
    ad.metadata_row   = 1;  /* width row */

    /* Initial width = 1.0 */
    assert_true(ad.decal.width == 1.0);

    InputState in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in, NULL);
    assert_true(ad.decal.width == 1.25);

    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in, NULL);
    assert_true(ad.decal.width == 1.0);

    /* Clamp: drive width down to 0.25 then try to go lower */
    ad.decal.width = 0.25;
    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in, NULL);
    assert_true(ad.decal.width == 0.25);

    asset_designer_destroy(&ad);
}

/**
 * test_metadata_height_adjust — Right increases height by 0.25; clamps at 0.25.
 */
static void test_metadata_height_adjust(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.metadata_focus = 1;
    ad.metadata_row   = 2;  /* height row */

    assert_true(ad.decal.height == 1.0);

    InputState in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in, NULL);
    assert_true(ad.decal.height == 1.25);

    ad.decal.height = 0.25;
    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in, NULL);
    assert_true(ad.decal.height == 0.25);

    asset_designer_destroy(&ad);
}

/**
 * test_metadata_glyph_step_adjust — step_u and step_v adjust by 0.0625;
 * clamped at 0.0.
 */
static void test_metadata_glyph_step_adjust(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.metadata_focus = 1;

    /* step_u row = 3 */
    ad.metadata_row = 3;
    assert_true(ad.decal.glyph_step_u == 0.0625);

    InputState in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in, NULL);
    assert_true(ad.decal.glyph_step_u == 0.125);

    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in, NULL);
    assert_true(ad.decal.glyph_step_u == 0.0625);

    /* Clamp at 0.0 */
    ad.decal.glyph_step_u = 0.0;
    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in, NULL);
    assert_true(ad.decal.glyph_step_u == 0.0);

    /* step_v row = 4 */
    ad.metadata_row = 4;
    assert_true(ad.decal.glyph_step_v == 0.0625);

    in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in, NULL);
    assert_true(ad.decal.glyph_step_v == 0.125);

    asset_designer_destroy(&ad);
}

/**
 * test_metadata_material_id_adjust — Left/Right adjusts current_material_id;
 * clamps to [1, 255].
 */
static void test_metadata_material_id_adjust(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.metadata_focus      = 1;
    ad.metadata_row        = 5;  /* material_id row */

    assert_int_equal(ad.current_material_id, 1);

    InputState in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.current_material_id, 2);

    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.current_material_id, 1);

    /* Clamp at 1 */
    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.current_material_id, 1);

    /* Clamp at 255 */
    ad.current_material_id = 255;
    in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.current_material_id, 255);

    asset_designer_destroy(&ad);
}

/**
 * test_metadata_decal_fields_mark_dirty — Changing surface, width, height,
 * or glyph_step sets dirty = 1.
 */
static void test_metadata_decal_fields_mark_dirty(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);

    /* surface */
    {
        AssetDesignerState ad;
        asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
        ad.metadata_focus = 1; ad.metadata_row = 0;
        assert_int_equal(ad.dirty, 0);
        InputState in = no_input(); in.arrow_right = true;
        asset_designer_update(&ad, &in, NULL);
        assert_int_equal(ad.dirty, 1);
        asset_designer_destroy(&ad);
    }

    /* width */
    {
        AssetDesignerState ad;
        asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
        ad.metadata_focus = 1; ad.metadata_row = 1;
        assert_int_equal(ad.dirty, 0);
        InputState in = no_input(); in.arrow_right = true;
        asset_designer_update(&ad, &in, NULL);
        assert_int_equal(ad.dirty, 1);
        asset_designer_destroy(&ad);
    }

    /* height */
    {
        AssetDesignerState ad;
        asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
        ad.metadata_focus = 1; ad.metadata_row = 2;
        assert_int_equal(ad.dirty, 0);
        InputState in = no_input(); in.arrow_right = true;
        asset_designer_update(&ad, &in, NULL);
        assert_int_equal(ad.dirty, 1);
        asset_designer_destroy(&ad);
    }

    /* glyph_step_u */
    {
        AssetDesignerState ad;
        asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
        ad.metadata_focus = 1; ad.metadata_row = 3;
        assert_int_equal(ad.dirty, 0);
        InputState in = no_input(); in.arrow_right = true;
        asset_designer_update(&ad, &in, NULL);
        assert_int_equal(ad.dirty, 1);
        asset_designer_destroy(&ad);
    }

    /* glyph_step_v */
    {
        AssetDesignerState ad;
        asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
        ad.metadata_focus = 1; ad.metadata_row = 4;
        assert_int_equal(ad.dirty, 0);
        InputState in = no_input(); in.arrow_right = true;
        asset_designer_update(&ad, &in, NULL);
        assert_int_equal(ad.dirty, 1);
        asset_designer_destroy(&ad);
    }
}

/**
 * test_metadata_material_id_no_dirty — Changing current_material_id alone
 * does NOT set dirty, because it is editor state only.
 */
static void test_metadata_material_id_no_dirty(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.metadata_focus = 1;
    ad.metadata_row   = 5;

    assert_int_equal(ad.dirty, 0);

    InputState in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.current_material_id, 2);
    assert_int_equal(ad.dirty, 0);   /* must NOT be dirty */

    asset_designer_destroy(&ad);
}

/**
 * test_metadata_place_uses_material_id — Placing a glyph writes
 * current_material_id to the cell's material_id field.
 */
static void test_metadata_place_uses_material_id(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.cursor_col          = 0;
    ad.cursor_row          = 0;
    ad.current_glyph       = '#';
    ad.current_material_id = 7;
    ad.metadata_focus      = 0;   /* canvas focus */

    InputState in = no_input();
    in.place = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.decal.pattern[0].glyph,       '#');
    assert_int_equal(ad.decal.pattern[0].material_id, 7);

    asset_designer_destroy(&ad);
}

/**
 * test_metadata_existing_cells_unchanged — Changing current_material_id does
 * NOT retroactively alter already-placed cells.
 */
static void test_metadata_existing_cells_unchanged(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* Place a glyph at (0,0) with material_id = 3 */
    ad.current_material_id = 3;
    ad.current_glyph       = '#';
    InputState in = no_input();
    in.place = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.decal.pattern[0].material_id, 3);

    /* Now change current_material_id to 9 via metadata panel */
    ad.metadata_focus = 1;
    ad.metadata_row   = 5;
    ad.current_material_id = 9;
    in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.current_material_id, 10);

    /* Previously placed cell must still have material_id = 3 */
    assert_int_equal(ad.decal.pattern[0].material_id, 3);

    asset_designer_destroy(&ad);
}

/**
 * test_metadata_save_load_preserves — Set surface/width/height/steps, save,
 * load into a fresh state, verify all fields survived the round-trip.
 */
static void test_metadata_save_load_preserves(void **state) {
    (void)state;

    const char *basename = "test_metadata_rt_tmp";
    char path[128];
    snprintf(path, sizeof(path), "%s%s.txt", AD_DECALS_DIR, basename);
    remove(path);

    EngineConfig cfg = make_cfg(6, 4);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* Write known metadata */
    ad.editor_surface     = AD_SURFACE_FLOOR;    /* editor-facing surface */
    ad.decal.width        = 2.0;
    ad.decal.height       = 1.5;
    ad.decal.glyph_step_u = 0.125;
    ad.decal.glyph_step_v = 0.0625;

    /* Place a glyph with material_id = 5 */
    ad.current_material_id = 5;
    ad.current_glyph       = '#';
    InputState in = no_input();
    in.place = true;
    asset_designer_update(&ad, &in, NULL);

    /* Save */
    ad.dirty = 1;
    ad.mode  = AD_SAVE_PROMPT;
    strncpy(ad.filename_buffer, basename, AD_FILENAME_MAX - 1);
    ad.filename_pos = (int)strlen(basename);
    in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);
    assert_int_equal(ad.dirty, 0);

    /* Reload directly to verify saved data — decal_load_from_file uses the
     * same format as the engine runtime, so this tests the full round-trip. */
    Decal *loaded = decal_load_from_file(path);
    assert_non_null(loaded);

    assert_int_equal((int)loaded->surface, (int)DECAL_SURFACE_FLOOR);
    /* Verify per-cell material_id survived the save/load cycle */
    assert_int_equal(loaded->pattern[0].material_id, 5);
    assert_int_equal(loaded->pattern[0].glyph, '#');

    decal_free(loaded);
    asset_designer_destroy(&ad);
    remove(path);
}

/* ===================================================================
 *  Tests — Pattern resize (meta rows 6 and 7)
 * =================================================================== */

/**
 * test_resize_increase_cols — Navigating to meta row 6 and pressing Right
 * increases canvas_cols by 1, preserves existing cells, fills new cells
 * with space, and sets dirty=1.
 */
static void test_resize_increase_cols(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(3, 3);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    int old_cols = ad.canvas_cols;  /* 3 */

    /* Place known glyph at (0,0) */
    ad.decal.pattern[0].glyph = 'A';
    ad.dirty = 0;

    /* Navigate to meta row 6 (pattern_cols) and press Right */
    ad.metadata_focus = 1;
    ad.metadata_row   = 6;
    InputState in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.canvas_cols, old_cols + 1);
    assert_int_equal(ad.decal.pattern_cols, old_cols + 1);

    /* Old cell at (0,0) preserved */
    assert_int_equal(ad.decal.pattern[0].glyph, 'A');

    /* New rightmost cell in row 0 is space */
    int new_last_col = ad.canvas_cols - 1;
    assert_int_equal(ad.decal.pattern[0 * ad.canvas_cols + new_last_col].glyph, ' ');

    assert_int_equal(ad.dirty, 1);

    asset_designer_destroy(&ad);
}

/**
 * test_resize_decrease_cols — Pressing Left on meta row 6 decreases canvas_cols
 * by 1 and sets dirty=1.
 */
static void test_resize_decrease_cols(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(3, 3);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    int old_cols = ad.canvas_cols;  /* 3 */
    ad.dirty = 0;

    ad.metadata_focus = 1;
    ad.metadata_row   = 6;
    InputState in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.canvas_cols, old_cols - 1);
    assert_int_equal(ad.decal.pattern_cols, old_cols - 1);
    assert_int_equal(ad.dirty, 1);

    asset_designer_destroy(&ad);
}

/**
 * test_resize_increase_rows — Pressing Right on meta row 7 increases
 * canvas_rows by 1 and sets dirty=1.
 */
static void test_resize_increase_rows(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(3, 3);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    int old_rows = ad.canvas_rows;  /* 3 */

    /* Place known glyph at (0,0) */
    ad.decal.pattern[0].glyph = 'B';
    ad.dirty = 0;

    ad.metadata_focus = 1;
    ad.metadata_row   = 7;
    InputState in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.canvas_rows, old_rows + 1);
    assert_int_equal(ad.decal.pattern_rows, old_rows + 1);

    /* Old cell at (0,0) preserved */
    assert_int_equal(ad.decal.pattern[0].glyph, 'B');

    /* New bottom-row cell at (new_last_row, 0) is space */
    int new_last_row = ad.canvas_rows - 1;
    assert_int_equal(ad.decal.pattern[new_last_row * ad.canvas_cols + 0].glyph, ' ');

    assert_int_equal(ad.dirty, 1);

    asset_designer_destroy(&ad);
}

/**
 * test_resize_decrease_rows — Pressing Left on meta row 7 decreases
 * canvas_rows by 1 and sets dirty=1.
 */
static void test_resize_decrease_rows(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(3, 3);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    int old_rows = ad.canvas_rows;  /* 3 */
    ad.dirty = 0;

    ad.metadata_focus = 1;
    ad.metadata_row   = 7;
    InputState in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.canvas_rows, old_rows - 1);
    assert_int_equal(ad.decal.pattern_rows, old_rows - 1);
    assert_int_equal(ad.dirty, 1);

    asset_designer_destroy(&ad);
}

/**
 * test_resize_cursor_clamps — When decreasing cols puts the cursor out of
 * bounds, cursor_col is clamped to the new last column.
 */
static void test_resize_cursor_clamps(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(3, 3);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* Move cursor to last column */
    ad.cursor_col = ad.canvas_cols - 1;  /* 2 */
    ad.cursor_row = 0;

    /* Decrease cols: new last col = canvas_cols - 2 = 1 */
    ad.metadata_focus = 1;
    ad.metadata_row   = 6;
    InputState in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in, NULL);

    /* cursor_col must be clamped to new last col */
    assert_int_equal(ad.cursor_col, ad.canvas_cols - 1);

    asset_designer_destroy(&ad);
}

/**
 * test_resize_no_change_when_at_min_cols — Cannot decrease cols below 1.
 */
static void test_resize_no_change_when_at_min_cols(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(1, 3);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    assert_int_equal(ad.canvas_cols, 1);
    ad.dirty = 0;

    ad.metadata_focus = 1;
    ad.metadata_row   = 6;
    InputState in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in, NULL);

    /* Still 1 — no-op resize should not set dirty */
    assert_int_equal(ad.canvas_cols, 1);
    assert_int_equal(ad.dirty, 0);

    asset_designer_destroy(&ad);
}

/**
 * test_resize_no_change_when_at_min_rows — Cannot decrease rows below 1.
 */
static void test_resize_no_change_when_at_min_rows(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(3, 1);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    assert_int_equal(ad.canvas_rows, 1);
    ad.dirty = 0;

    ad.metadata_focus = 1;
    ad.metadata_row   = 7;
    InputState in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.canvas_rows, 1);
    assert_int_equal(ad.dirty, 0);

    asset_designer_destroy(&ad);
}

/**
 * test_resize_save_load_preserves — Resize then save; load into fresh state;
 * verify new dimensions match.
 */
static void test_resize_save_load_preserves(void **state) {
    (void)state;

    const char *basename = "test_resize_rt_tmp";
    char path[128];
    snprintf(path, sizeof(path), "%s%s.txt", AD_DECALS_DIR, basename);
    remove(path);

    EngineConfig cfg = make_cfg(3, 3);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* Increase cols by 1 via meta row 6 */
    ad.metadata_focus = 1;
    ad.metadata_row   = 6;
    InputState in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in, NULL);

    int saved_cols = ad.canvas_cols;  /* 4 */
    int saved_rows = ad.canvas_rows;  /* 3 */

    /* Save */
    ad.mode = AD_SAVE_PROMPT;
    strncpy(ad.filename_buffer, basename, AD_FILENAME_MAX - 1);
    ad.filename_pos = (int)strlen(basename);
    in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);

    /* Load into a fresh state */
    AssetDesignerState ad2;
    asset_designer_init(&ad2, &cfg, APP_STATE_MAIN_MENU);

    strncpy(ad2.file_list[0], basename, AD_FILENAME_MAX - 1);
    ad2.file_count   = 1;
    ad2.file_sel_idx = 0;
    ad2.mode = AD_LOAD_SELECT;

    in = no_input();
    in.confirm = true;
    asset_designer_update(&ad2, &in, NULL);

    assert_int_equal(ad2.canvas_cols, saved_cols);
    assert_int_equal(ad2.canvas_rows, saved_rows);

    asset_designer_destroy(&ad);
    asset_designer_destroy(&ad2);
    remove(path);
}

/* ===================================================================
 *  Tests — Direct glyph typing
 * =================================================================== */

/**
 * test_typing_places_glyph — A printable non-space char typed in canvas focus
 * is written to the cell under the cursor.
 */
static void test_typing_places_glyph(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* Canvas focus (default) */
    assert_int_equal(ad.metadata_focus, 0);

    InputState in = no_input();
    in.text_input[0] = 'X';
    in.text_input_len = 1;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.decal.pattern[0].glyph, 'X');

    asset_designer_destroy(&ad);
}

/**
 * test_typing_uses_current_material — Typed glyph gets current_material_id.
 */
static void test_typing_uses_current_material(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.current_material_id = 3;

    InputState in = no_input();
    in.text_input[0] = 'A';
    in.text_input_len = 1;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.decal.pattern[0].material_id, 3);

    asset_designer_destroy(&ad);
}

/**
 * test_typing_advances_cursor — Typing a char does NOT advance the cursor
 * (cursor stays in place; direct typing is now stationary).
 */
static void test_typing_advances_cursor(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    assert_int_equal(ad.cursor_col, 0);

    InputState in = no_input();
    in.text_input[0] = 'A';
    in.text_input_len = 1;
    asset_designer_update(&ad, &in, NULL);

    /* Cursor must NOT have advanced — typing is now stationary */
    assert_int_equal(ad.cursor_col, 0);

    asset_designer_destroy(&ad);
}

/**
 * test_typing_clamps_at_last_col — Cursor does not advance past the last column.
 */
static void test_typing_clamps_at_last_col(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.cursor_col = ad.canvas_cols - 1;  /* already at last col */

    InputState in = no_input();
    in.text_input[0] = 'B';
    in.text_input_len = 1;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.cursor_col, ad.canvas_cols - 1);

    asset_designer_destroy(&ad);
}

/**
 * test_typing_ignores_space — Space character is not placed by typing
 * (handled separately by INPUT_PLACE).  Glyph and cursor stay unchanged.
 */
static void test_typing_ignores_space(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* Initial glyph is space; note initial cursor_col */
    assert_int_equal(ad.cursor_col, 0);

    /* Put a known non-space glyph at (0,0) first so we can tell if it changed */
    ad.decal.pattern[0].glyph = 'Z';

    InputState in = no_input();
    in.text_input[0] = ' ';
    in.text_input_len = 1;
    asset_designer_update(&ad, &in, NULL);

    /* Cell at (0,0) must NOT have been overwritten with space via typing */
    assert_int_equal(ad.decal.pattern[0].glyph, 'Z');
    /* Cursor must NOT have advanced */
    assert_int_equal(ad.cursor_col, 0);

    asset_designer_destroy(&ad);
}

/**
 * test_typing_suppressed_by_glyph_cycle — (glyph cycle removed; 'q' can now
 * be typed freely as a canvas glyph).  This test verifies 'q' is placed.
 */
static void test_typing_suppressed_by_glyph_cycle(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* Initial glyph at (0,0) is space */
    assert_int_equal(ad.decal.pattern[0].glyph, ' ');

    /* 'q' is now a regular typed glyph (no palette cycle to suppress it) */
    InputState in = no_input();
    in.text_input[0]  = 'q';
    in.text_input_len = 1;
    asset_designer_update(&ad, &in, NULL);

    /* 'q' must have been placed at (0,0) */
    assert_int_equal(ad.decal.pattern[0].glyph, 'q');

    asset_designer_destroy(&ad);
}

/**
 * test_typing_marks_dirty — Typing a valid glyph sets dirty=1.
 */
static void test_typing_marks_dirty(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    assert_int_equal(ad.dirty, 0);

    InputState in = no_input();
    in.text_input[0] = 'T';
    in.text_input_len = 1;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.dirty, 1);

    asset_designer_destroy(&ad);
}

/* ===================================================================
 *  Tests — asset_designer_render() with AssetRegistry parameter
 * =================================================================== */

/**
 * test_render_null_assets — asset_designer_render() must not crash when
 * assets is NULL.
 */
static void test_render_null_assets(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(10, 8);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    Grid *grid = grid_create(80, 24);
    assert_non_null(grid);

    /* Must complete without assertion failure or crash */
    asset_designer_render(&ad, grid, NULL);

    grid_destroy(grid);
    asset_designer_destroy(&ad);
}

/**
 * test_render_valid_assets — asset_designer_render() with a populated
 * AssetRegistry must not crash.
 */
static void test_render_valid_assets(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(10, 8);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.current_material_id = 1;

    AssetRegistry reg;
    asset_registry_init(&reg);
    asset_registry_set_material(&reg, 1, 1, "#@:.");
    SDL_Color red   = {255, 0, 0, 255};
    SDL_Color red2  = {200, 0, 0, 255};
    SDL_Color red3  = {150, 0, 0, 255};
    asset_registry_set_palette(&reg, 1, red, red2, red3);

    Grid *grid = grid_create(80, 24);
    assert_non_null(grid);

    asset_designer_render(&ad, grid, &reg);

    grid_destroy(grid);
    asset_designer_destroy(&ad);
}

/**
 * test_render_invalid_material_id — current_material_id out of loaded range
 * (but still 0..255) must not crash — it maps to an all-zero Material/Palette
 * entry which should fall back gracefully.
 */
static void test_render_invalid_material_id(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(10, 8);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    ad.current_material_id = 200;  /* in range for 256-slot array, but unset */

    AssetRegistry reg;
    asset_registry_init(&reg);

    Grid *grid = grid_create(80, 24);
    assert_non_null(grid);

    asset_designer_render(&ad, grid, &reg);

    grid_destroy(grid);
    asset_designer_destroy(&ad);
}

/* ===================================================================
 *  Tests — Typing: new behavior (no cursor advance, q/e allowed)
 * =================================================================== */

/**
 * test_typing_q_places_glyph — 'q' is no longer reserved; it can be typed
 * as a canvas glyph when in canvas focus.
 */
static void test_typing_q_places_glyph(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    assert_int_equal(ad.metadata_focus, 0);  /* canvas focus */

    InputState in = no_input();
    in.text_input[0]  = 'q';
    in.text_input_len = 1;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.decal.pattern[0].glyph, 'q');

    asset_designer_destroy(&ad);
}

/**
 * test_typing_e_places_glyph — 'e' is no longer reserved; it can be typed
 * as a canvas glyph when in canvas focus.
 */
static void test_typing_e_places_glyph(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    assert_int_equal(ad.metadata_focus, 0);  /* canvas focus */

    InputState in = no_input();
    in.text_input[0]  = 'e';
    in.text_input_len = 1;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.decal.pattern[0].glyph, 'e');

    asset_designer_destroy(&ad);
}

/**
 * test_typing_does_not_advance_cursor — Typing a char places it at the
 * current cursor position and the cursor stays in place.
 */
static void test_typing_does_not_advance_cursor(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    assert_int_equal(ad.cursor_col, 0);

    InputState in = no_input();
    in.text_input[0]  = 'A';
    in.text_input_len = 1;
    asset_designer_update(&ad, &in, NULL);

    /* Glyph placed */
    assert_int_equal(ad.decal.pattern[0].glyph, 'A');
    /* Cursor did NOT advance */
    assert_int_equal(ad.cursor_col, 0);

    asset_designer_destroy(&ad);
}

/**
 * test_typing_replaces_same_cell — Typing twice at the same position
 * overwrites the first glyph with the second at that same cell.
 */
static void test_typing_replaces_same_cell(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* First type 'A' */
    InputState in = no_input();
    in.text_input[0]  = 'A';
    in.text_input_len = 1;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.decal.pattern[0].glyph, 'A');

    /* Second type 'B' — cursor still at (0,0), so B replaces A */
    in = no_input();
    in.text_input[0]  = 'B';
    in.text_input_len = 1;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.decal.pattern[0].glyph, 'B');
    assert_int_equal(ad.cursor_col, 0);

    asset_designer_destroy(&ad);
}

/* ===================================================================
 *  Tests — Held movement / auto-repeat
 * =================================================================== */

/**
 * test_held_movement_repeat_fires — After an edge press (frame 1) the cursor
 * moves once.  Holding held_up for AD_REPEAT_DELAY more frames causes a
 * second move.  Frame count: 1 (edge press) + 15 (countdown) + 1 (fire) = 17.
 */
static void test_held_movement_repeat_fires(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 20);   /* tall canvas so row 10 is in bounds */
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.cursor_row = 10;

    /* Frame 1: edge press — cursor moves once, repeat armed */
    InputState in = no_input();
    in.up      = true;
    in.held_up = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.cursor_row, 9);
    assert_int_equal(ad.repeat_dir,   0);
    assert_int_equal(ad.repeat_timer, AD_REPEAT_DELAY);

    /* Frames 2 .. AD_REPEAT_DELAY+1: held only, timer counts down.
     * After AD_REPEAT_DELAY decrements the timer reaches 0 and fires. */
    int expected_row = 9;
    for (int frame = 2; frame <= AD_REPEAT_DELAY + 1; frame++) {
        in = no_input();
        in.held_up = true;   /* held but no edge */
        asset_designer_update(&ad, &in, NULL);
        /* Timer fires on the frame when it reaches 0 */
        if (frame == AD_REPEAT_DELAY + 1) {
            expected_row = 8;
        }
        assert_int_equal(ad.cursor_row, expected_row);
    }

    asset_designer_destroy(&ad);
}

/**
 * test_held_movement_stops_at_edge — Holding a direction at a canvas boundary
 * does not move the cursor beyond the edge.
 */
static void test_held_movement_stops_at_edge(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.cursor_row = 0;  /* already at top edge */

    /* Frame 1: edge press — can't move (already at 0) */
    InputState in = no_input();
    in.up      = true;
    in.held_up = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.cursor_row, 0);
    /* repeat still armed */
    assert_int_equal(ad.repeat_dir,   0);
    assert_int_equal(ad.repeat_timer, AD_REPEAT_DELAY);

    /* Run enough frames for the repeat to fire once; cursor must stay at 0 */
    for (int frame = 2; frame <= AD_REPEAT_DELAY + 2; frame++) {
        in = no_input();
        in.held_up = true;
        asset_designer_update(&ad, &in, NULL);
        assert_int_equal(ad.cursor_row, 0);
    }

    asset_designer_destroy(&ad);
}

/**
 * test_repeat_resets_when_released — Releasing the key resets repeat_dir to -1
 * and repeat_timer to 0.
 */
static void test_repeat_resets_when_released(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 10);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.cursor_row = 5;

    /* Frame 1: edge press, arm repeat */
    InputState in = no_input();
    in.up      = true;
    in.held_up = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.repeat_dir,   0);
    assert_int_equal(ad.repeat_timer, AD_REPEAT_DELAY);

    /* Frame 2: key released */
    in = no_input();
    /* held_up = false, no edge */
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.repeat_dir,   -1);
    assert_int_equal(ad.repeat_timer,  0);

    asset_designer_destroy(&ad);
}

/* ===================================================================
 *  Tests — Paint while moving
 * =================================================================== */

/**
 * test_hold_place_paints_while_moving — Holding held_place while pressing right
 * paints the glyph onto the new cell the cursor entered.
 */
static void test_hold_place_paints_while_moving(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(10, 10);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.cursor_row = 5;
    ad.cursor_col = 5;
    ad.current_glyph = '#';
    ad.current_material_id = 1;

    InputState in = no_input();
    in.arrow_right     = true;
    in.held_arrow_right = true;
    in.held_place       = true;
    asset_designer_update(&ad, &in, NULL);

    /* Cursor moved to col 6 */
    assert_int_equal(ad.cursor_col, 6);
    /* '#' painted at the new position */
    assert_int_equal(ad.decal.pattern[5 * ad.canvas_cols + 6].glyph, '#');
    assert_int_equal(ad.dirty, 1);

    asset_designer_destroy(&ad);
}

/**
 * test_hold_erase_erases_while_moving — Holding held_erase while pressing right
 * erases the glyph on the new cell the cursor entered.
 */
static void test_hold_erase_erases_while_moving(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(10, 10);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.cursor_row = 5;
    ad.cursor_col = 5;

    /* Pre-populate the cell we will move into */
    ad.decal.pattern[5 * ad.canvas_cols + 6].glyph = 'X';
    ad.dirty = 0;

    InputState in = no_input();
    in.arrow_right      = true;
    in.held_arrow_right = true;
    in.held_erase       = true;
    asset_designer_update(&ad, &in, NULL);

    /* Cursor moved to col 6 */
    assert_int_equal(ad.cursor_col, 6);
    /* Cell erased (set to space) */
    assert_int_equal(ad.decal.pattern[5 * ad.canvas_cols + 6].glyph, ' ');
    assert_int_equal(ad.dirty, 1);

    asset_designer_destroy(&ad);
}

/**
 * test_no_paint_without_movement — held_place does NOT paint if the cursor
 * could not move (e.g. already at a canvas edge).
 */
static void test_no_paint_without_movement(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);
    ad.cursor_row = 0;   /* already at top edge */
    ad.cursor_col = 0;
    ad.current_glyph = '#';

    InputState in = no_input();
    in.up      = true;
    in.held_up = true;
    in.held_place = true;
    asset_designer_update(&ad, &in, NULL);

    /* Cursor must still be at (0,0) */
    assert_int_equal(ad.cursor_row, 0);
    /* Cell must NOT have been painted — movement did not occur */
    assert_int_equal(ad.decal.pattern[0].glyph, ' ');

    asset_designer_destroy(&ad);
}

/* ===================================================================
 *  Tests — Empty cells default to space
 * =================================================================== */

/**
 * test_empty_cell_is_space — A freshly-initialised pattern cell has glyph ' '.
 */
static void test_empty_cell_is_space(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    assert_int_equal(ad.decal.pattern[0].glyph, ' ');

    asset_designer_destroy(&ad);
}

/**
 * test_period_glyph_is_drawable — '.' is a valid printable glyph and can be
 * typed directly onto the canvas.
 */
static void test_period_glyph_is_drawable(void **state) {
    (void)state;
    EngineConfig cfg = make_cfg(5, 5);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    InputState in = no_input();
    in.text_input[0]  = '.';
    in.text_input_len = 1;
    asset_designer_update(&ad, &in, NULL);

    assert_int_equal(ad.decal.pattern[0].glyph, '.');

    asset_designer_destroy(&ad);
}

/* ===================================================================
 *  Tests — Canvas border is render-only (not saved in pattern)
 * =================================================================== */

/**
 * test_border_not_saved — Saving and reloading a decal must not contain
 * '+', '-', or '|' characters in the pattern unless explicitly placed.
 */
static void test_border_not_saved(void **state) {
    (void)state;

    const char *basename = "test_border_check_tmp";
    char path[128];
    snprintf(path, sizeof(path), "%s%s.txt", AD_DECALS_DIR, basename);
    remove(path);

    EngineConfig cfg = make_cfg(4, 3);
    AssetDesignerState ad;
    asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

    /* Place a known glyph — no border chars */
    ad.decal.pattern[0].glyph = 'A';
    ad.dirty = 1;

    /* Save */
    ad.mode = AD_SAVE_PROMPT;
    strncpy(ad.filename_buffer, basename, AD_FILENAME_MAX - 1);
    ad.filename_pos = (int)strlen(basename);
    InputState in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in, NULL);
    assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);

    /* Load back and scan every pattern cell */
    Decal *loaded = decal_load_from_file(path);
    assert_non_null(loaded);

    int cells = loaded->pattern_cols * loaded->pattern_rows;
    int found_border = 0;
    for (int i = 0; i < cells; i++) {
        char g = (char)loaded->pattern[i].glyph;
        if (g == '+' || g == '-' || g == '|') {
            found_border = 1;
            break;
        }
    }
    assert_int_equal(found_border, 0);

    decal_free(loaded);
    asset_designer_destroy(&ad);
    remove(path);
}


/* ===================================================================
 *  Tests — Disallowed glyph validation and material normalization
 * =================================================================== */

/* Test: typing disallowed glyph is rejected */
static void test_typing_disallowed_rejected(void **state) {
    (void)state;
    EngineConfig cfg = make_test_config();
    AssetDesignerState s;
    asset_designer_init(&s, &cfg, APP_STATE_MAIN_MENU);

    /* Build a minimal AssetRegistry with one material that only allows '#' */
    AssetRegistry assets;
    memset(&assets, 0, sizeof(assets));
    /* material_id 1 (current default) allows '#' only */
    assets.materials[1].glyphs[0] = '#';
    assets.materials[1].glyphs[1] = 0;
    assets.materials[1].glyphs[2] = 0;
    assets.materials[1].glyphs[3] = 0;
    assets.materials[1].palette_id = 0;

    InputState input;
    memset(&input, 0, sizeof(input));
    /* Type 'Z' which is NOT in material 1's glyph list */
    input.text_input[0] = 'Z';
    input.text_input_len = 1;

    asset_designer_update(&s, &input, &assets);

    /* Cell should NOT be modified */
    assert_int_equal(s.decal.pattern[0].glyph, ' ');
    /* Status message should be set */
    assert_true(s.status_frames > 0);

    asset_designer_destroy(&s);
}

/* Test: typing disallowed glyph does not update current_glyph */
static void test_typing_disallowed_no_glyph_update(void **state) {
    (void)state;
    EngineConfig cfg = make_test_config();
    AssetDesignerState s;
    asset_designer_init(&s, &cfg, APP_STATE_MAIN_MENU);

    AssetRegistry assets;
    memset(&assets, 0, sizeof(assets));
    assets.materials[1].glyphs[0] = '#';
    assets.materials[1].palette_id = 0;

    char original_glyph = s.current_glyph;  /* '#' from init */

    InputState input;
    memset(&input, 0, sizeof(input));
    input.text_input[0] = 'Z';
    input.text_input_len = 1;

    asset_designer_update(&s, &input, &assets);

    assert_int_equal(s.current_glyph, original_glyph);

    asset_designer_destroy(&s);
}

/* Test: typing disallowed glyph sets status message */
static void test_typing_disallowed_sets_status(void **state) {
    (void)state;
    EngineConfig cfg = make_test_config();
    AssetDesignerState s;
    asset_designer_init(&s, &cfg, APP_STATE_MAIN_MENU);

    AssetRegistry assets;
    memset(&assets, 0, sizeof(assets));
    assets.materials[1].glyphs[0] = '#';
    assets.materials[1].palette_id = 0;

    InputState input;
    memset(&input, 0, sizeof(input));
    input.text_input[0] = 'Z';
    input.text_input_len = 1;

    asset_designer_update(&s, &input, &assets);

    assert_true(s.status_frames > 0);
    assert_true(strlen(s.status_msg) > 0);

    asset_designer_destroy(&s);
}

/* Test: typing allowed glyph updates current_glyph */
static void test_typing_updates_current_glyph(void **state) {
    (void)state;
    EngineConfig cfg = make_test_config();
    AssetDesignerState s;
    asset_designer_init(&s, &cfg, APP_STATE_MAIN_MENU);

    /* assets=NULL means all printable glyphs allowed */
    InputState input;
    memset(&input, 0, sizeof(input));
    input.text_input[0] = 'A';
    input.text_input_len = 1;

    asset_designer_update(&s, &input, NULL);

    assert_int_equal(s.current_glyph, 'A');

    asset_designer_destroy(&s);
}

/* Test: Space paints current_glyph */
static void test_space_paints_current_glyph(void **state) {
    (void)state;
    EngineConfig cfg = make_test_config();
    AssetDesignerState s;
    asset_designer_init(&s, &cfg, APP_STATE_MAIN_MENU);
    s.current_glyph = '#';

    InputState input;
    memset(&input, 0, sizeof(input));
    input.place = true;  /* Space edge-triggered */

    asset_designer_update(&s, &input, NULL);

    assert_int_equal(s.decal.pattern[0].glyph, '#');
    assert_int_equal(s.dirty, 1);

    asset_designer_destroy(&s);
}

/* Test: material change normalizes current_glyph if not allowed */
static void test_material_change_normalizes_glyph(void **state) {
    (void)state;
    EngineConfig cfg = make_test_config();
    AssetDesignerState s;
    asset_designer_init(&s, &cfg, APP_STATE_MAIN_MENU);

    /* Build materials:
     * material 1: allows '#' only
     * material 2: allows 'X' only */
    AssetRegistry assets;
    memset(&assets, 0, sizeof(assets));
    assets.materials[1].glyphs[0] = '#';
    assets.materials[1].palette_id = 0;
    assets.materials[2].glyphs[0] = 'X';
    assets.materials[2].palette_id = 0;

    /* Start on material 2 with 'X' (which IS allowed by material 2) */
    s.current_material_id = 2;
    s.current_glyph = 'X';

    /* Focus metadata panel, row 5 = material_id */
    s.metadata_focus = 1;
    s.metadata_row = 5;

    InputState input;
    memset(&input, 0, sizeof(input));
    /* Press Left to decrease material_id from 2 to 1 */
    input.arrow_left = true;
    asset_designer_update(&s, &input, &assets);

    /* material_id should now be 1 */
    assert_int_equal(s.current_material_id, 1);
    /* Material 1 does not allow 'X', so current_glyph must be normalized to '#' */
    assert_int_equal(s.current_glyph, '#');

    asset_designer_destroy(&s);
}


/* ===================================================================
 *  Tests — Bug-fix verification (glyph cycling removal, space cells,
 *          per-cell material, save-prompt modal)
 * =================================================================== */

/**
 * test_prev_glyph_no_longer_cycles — After the glyph-cycling removal,
 * sending prev_glyph=true must NOT change current_glyph.
 */
static void test_prev_glyph_no_longer_cycles(void **state) {
    (void)state;
    EngineConfig cfg = make_test_config();
    AssetDesignerState s;
    asset_designer_init(&s, &cfg, APP_STATE_MAIN_MENU);
    char glyph_before = s.current_glyph;

    InputState input = no_input();
    input.prev_glyph = true;

    asset_designer_update(&s, &input, NULL);

    /* current_glyph must not change from a palette cycle */
    assert_int_equal((int)s.current_glyph, (int)glyph_before);

    asset_designer_destroy(&s);
}

/**
 * test_next_glyph_no_longer_cycles — Sending next_glyph=true must NOT
 * change current_glyph after the cycling feature was removed.
 */
static void test_next_glyph_no_longer_cycles(void **state) {
    (void)state;
    EngineConfig cfg = make_test_config();
    AssetDesignerState s;
    asset_designer_init(&s, &cfg, APP_STATE_MAIN_MENU);
    char glyph_before = s.current_glyph;

    InputState input = no_input();
    input.next_glyph = true;

    asset_designer_update(&s, &input, NULL);

    /* current_glyph must not change from a palette cycle */
    assert_int_equal((int)s.current_glyph, (int)glyph_before);

    asset_designer_destroy(&s);
}

/**
 * test_empty_cell_renders_as_space — After init, non-cursor canvas cells
 * store space (not '.').  Verified via the pattern array directly.
 */
static void test_empty_cell_renders_as_space(void **state) {
    (void)state;
    EngineConfig cfg = make_test_config();
    AssetDesignerState s;
    asset_designer_init(&s, &cfg, APP_STATE_MAIN_MENU);

    /* Cell index 1 is not the cursor (cursor is at 0,0 = index 0). */
    assert_int_equal((int)s.decal.pattern[1].glyph, (int)' ');

    asset_designer_destroy(&s);
}

/**
 * test_stored_period_remains_period — A '.' explicitly written to the pattern
 * must stay '.' and not be overwritten with anything else at rest.
 */
static void test_stored_period_remains_period(void **state) {
    (void)state;
    EngineConfig cfg = make_test_config();
    AssetDesignerState s;
    asset_designer_init(&s, &cfg, APP_STATE_MAIN_MENU);

    s.decal.pattern[0].glyph = '.';

    assert_int_equal((int)s.decal.pattern[0].glyph, (int)'.');

    asset_designer_destroy(&s);
}

/**
 * test_canvas_stores_per_cell_material — Each placed cell independently
 * records the current_material_id at the time of placement.
 */
static void test_canvas_stores_per_cell_material(void **state) {
    (void)state;
    EngineConfig cfg = make_test_config();
    AssetDesignerState s;
    asset_designer_init(&s, &cfg, APP_STATE_MAIN_MENU);

    /* Place at (0,0) with material 1 */
    s.current_material_id = 1;
    InputState input = no_input();
    input.place = true;
    asset_designer_update(&s, &input, NULL);
    int mat_at_0 = (int)s.decal.pattern[0].material_id;

    /* Move cursor right */
    input = no_input();
    input.arrow_right = true;
    asset_designer_update(&s, &input, NULL);

    /* Place at (1,0) with material 2 */
    s.current_material_id = 2;
    input = no_input();
    input.place = true;
    asset_designer_update(&s, &input, NULL);
    int mat_at_1 = (int)s.decal.pattern[1].material_id;

    assert_int_equal(mat_at_0, 1);
    assert_int_equal(mat_at_1, 2);

    asset_designer_destroy(&s);
}

/**
 * test_save_prompt_still_accepts_text_after_modal — Opening save-prompt via
 * F10 and typing characters must append them to filename_buffer.
 */
static void test_save_prompt_still_accepts_text_after_modal(void **state) {
    (void)state;
    EngineConfig cfg = make_test_config();
    AssetDesignerState s;
    asset_designer_init(&s, &cfg, APP_STATE_MAIN_MENU);

    /* Enter save prompt via F10 */
    InputState input = no_input();
    input.save_as = true;
    asset_designer_update(&s, &input, NULL);
    assert_int_equal(s.mode, (int)AD_SAVE_PROMPT);

    /* Type a filename */
    input = no_input();
    input.text_input[0] = 'm';
    input.text_input[1] = 'y';
    input.text_input_len = 2;
    asset_designer_update(&s, &input, NULL);
    assert_int_equal(s.filename_pos, 2);
    assert_int_equal((int)s.filename_buffer[0], (int)'m');
    assert_int_equal((int)s.filename_buffer[1], (int)'y');

    asset_designer_destroy(&s);
}

/**
 * test_save_prompt_esc_closes_modal — Pressing Esc in AD_SAVE_PROMPT must
 * return to AD_DECAL_EDIT and clear the filename buffer.
 */
static void test_save_prompt_esc_closes_modal(void **state) {
    (void)state;
    EngineConfig cfg = make_test_config();
    AssetDesignerState s;
    asset_designer_init(&s, &cfg, APP_STATE_MAIN_MENU);

    /* Open save prompt */
    InputState input = no_input();
    input.save_as = true;
    asset_designer_update(&s, &input, NULL);
    assert_int_equal(s.mode, (int)AD_SAVE_PROMPT);

    /* Esc must close it */
    input = no_input();
    input.esc = true;
    asset_designer_update(&s, &input, NULL);
    assert_int_equal(s.mode, (int)AD_DECAL_EDIT);

    asset_designer_destroy(&s);
}

/* ===================================================================
 *  Entry point
 * =================================================================== */

int main(void) {
    const struct CMUnitTest tests[] = {
        /* --- Init --- */
        cmocka_unit_test(test_init_defaults),
        cmocka_unit_test(test_init_clamps_large),
        cmocka_unit_test(test_init_clamps_small),
        cmocka_unit_test(test_init_mode_and_filename),
        /* --- Cursor --- */
        cmocka_unit_test(test_cursor_moves_all_directions),
        cmocka_unit_test(test_cursor_clamps_at_zero),
        cmocka_unit_test(test_cursor_clamps_at_max),
        /* --- Place / erase --- */
        cmocka_unit_test(test_place_glyph_marks_dirty),
        cmocka_unit_test(test_erase_marks_dirty),
        cmocka_unit_test(test_place_space_no_dirty),
        /* --- Exit --- */
        cmocka_unit_test(test_esc_clean_returns_exit),
        cmocka_unit_test(test_esc_dirty_returns_confirm),
        /* --- Save / Load --- */
        cmocka_unit_test(test_save_load_roundtrip),
        cmocka_unit_test(test_f5_no_filename_enters_save_prompt),
        cmocka_unit_test(test_f5_with_current_filename_saves_directly),
        cmocka_unit_test(test_f9_enters_load_select),
        /* --- Save-prompt sub-mode --- */
        cmocka_unit_test(test_f10_always_enters_save_prompt),
        cmocka_unit_test(test_f10_prefills_current_filename),
        cmocka_unit_test(test_save_prompt_text_input),
        cmocka_unit_test(test_save_prompt_backspace),
        cmocka_unit_test(test_save_prompt_esc_cancels),
        cmocka_unit_test(test_save_prompt_enter_commits_valid),
        cmocka_unit_test(test_save_prompt_enter_rejects_invalid),
        /* --- Filename validation --- */
        cmocka_unit_test(test_validate_basename_accepts_valid),
        cmocka_unit_test(test_validate_basename_rejects_empty),
        cmocka_unit_test(test_validate_basename_rejects_dotdot),
        cmocka_unit_test(test_validate_basename_rejects_slash),
        cmocka_unit_test(test_validate_basename_rejects_bad_chars),
        /* --- Surface=All cycle and export --- */
        cmocka_unit_test(test_surface_all_in_cycle),
        cmocka_unit_test(test_save_all_creates_three_files),
        cmocka_unit_test(test_save_all_files_have_correct_surface),
        cmocka_unit_test(test_load_one_copy_sets_concrete),
        cmocka_unit_test(test_save_all_does_not_mutate_editor_state),
        /* --- Metadata direct-edit --- */
        cmocka_unit_test(test_meta_edit_enter_enters_mode),
        cmocka_unit_test(test_meta_edit_width_valid_commits),
        cmocka_unit_test(test_meta_edit_width_invalid_rejects),
        cmocka_unit_test(test_meta_edit_surface_accepts_all_values),
        cmocka_unit_test(test_meta_edit_surface_rejects_invalid),
        cmocka_unit_test(test_meta_edit_material_id_accepts),
        cmocka_unit_test(test_meta_edit_material_id_rejects_bounds),
        cmocka_unit_test(test_meta_edit_esc_cancels),
        cmocka_unit_test(test_meta_edit_text_input_chars),
        cmocka_unit_test(test_meta_edit_backspace),
        cmocka_unit_test(test_left_right_tweak_still_works),
        cmocka_unit_test(test_filename_prompts_unaffected),
        /* --- Destroy --- */
        cmocka_unit_test(test_destroy_idempotent),
        /* --- Metadata panel --- */
        cmocka_unit_test(test_init_current_material_id),
        cmocka_unit_test(test_metadata_focus_tab),
        cmocka_unit_test(test_metadata_canvas_arrows_blocked),
        cmocka_unit_test(test_metadata_row_up_down),
        cmocka_unit_test(test_metadata_row_clamps),
        cmocka_unit_test(test_metadata_surface_cycle_right),
        cmocka_unit_test(test_metadata_surface_cycle_left),
        cmocka_unit_test(test_metadata_width_adjust),
        cmocka_unit_test(test_metadata_height_adjust),
        cmocka_unit_test(test_metadata_glyph_step_adjust),
        cmocka_unit_test(test_metadata_material_id_adjust),
        cmocka_unit_test(test_metadata_decal_fields_mark_dirty),
        cmocka_unit_test(test_metadata_material_id_no_dirty),
        cmocka_unit_test(test_metadata_place_uses_material_id),
        cmocka_unit_test(test_metadata_existing_cells_unchanged),
        cmocka_unit_test(test_metadata_save_load_preserves),
        /* --- Pattern resize --- */
        cmocka_unit_test(test_resize_increase_cols),
        cmocka_unit_test(test_resize_decrease_cols),
        cmocka_unit_test(test_resize_increase_rows),
        cmocka_unit_test(test_resize_decrease_rows),
        cmocka_unit_test(test_resize_cursor_clamps),
        cmocka_unit_test(test_resize_no_change_when_at_min_cols),
        cmocka_unit_test(test_resize_no_change_when_at_min_rows),
        cmocka_unit_test(test_resize_save_load_preserves),
        /* --- Direct glyph typing --- */
        cmocka_unit_test(test_typing_places_glyph),
        cmocka_unit_test(test_typing_uses_current_material),
        cmocka_unit_test(test_typing_advances_cursor),
        cmocka_unit_test(test_typing_clamps_at_last_col),
        cmocka_unit_test(test_typing_ignores_space),
        cmocka_unit_test(test_typing_suppressed_by_glyph_cycle),
        cmocka_unit_test(test_typing_marks_dirty),
        /* --- Typing: new behavior (no cursor advance, q/e allowed) --- */
        cmocka_unit_test(test_typing_q_places_glyph),
        cmocka_unit_test(test_typing_e_places_glyph),
        cmocka_unit_test(test_typing_does_not_advance_cursor),
        cmocka_unit_test(test_typing_replaces_same_cell),
        /* --- Held movement / auto-repeat --- */
        cmocka_unit_test(test_held_movement_repeat_fires),
        cmocka_unit_test(test_held_movement_stops_at_edge),
        cmocka_unit_test(test_repeat_resets_when_released),
        /* --- Paint while moving --- */
        cmocka_unit_test(test_hold_place_paints_while_moving),
        cmocka_unit_test(test_hold_erase_erases_while_moving),
        cmocka_unit_test(test_no_paint_without_movement),
        /* --- Empty cells default to space --- */
        cmocka_unit_test(test_empty_cell_is_space),
        cmocka_unit_test(test_period_glyph_is_drawable),
        /* --- Canvas border is render-only --- */
        cmocka_unit_test(test_border_not_saved),
        /* --- Render with AssetRegistry --- */
        cmocka_unit_test(test_render_null_assets),
        cmocka_unit_test(test_render_valid_assets),
        cmocka_unit_test(test_render_invalid_material_id),
        /* --- Disallowed glyph validation and material normalization --- */
        cmocka_unit_test(test_typing_disallowed_rejected),
        cmocka_unit_test(test_typing_disallowed_no_glyph_update),
        cmocka_unit_test(test_typing_disallowed_sets_status),
        cmocka_unit_test(test_typing_updates_current_glyph),
        cmocka_unit_test(test_space_paints_current_glyph),
        cmocka_unit_test(test_material_change_normalizes_glyph),
        /* --- Bug-fix verification --- */
        cmocka_unit_test(test_prev_glyph_no_longer_cycles),
        cmocka_unit_test(test_next_glyph_no_longer_cycles),
        cmocka_unit_test(test_empty_cell_renders_as_space),
        cmocka_unit_test(test_stored_period_remains_period),
        cmocka_unit_test(test_canvas_stores_per_cell_material),
        cmocka_unit_test(test_save_prompt_still_accepts_text_after_modal),
        cmocka_unit_test(test_save_prompt_esc_closes_modal),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
