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
    asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);

    assert_int_equal(ad.mode, (int)AD_LOAD_SELECT);
    assert_non_null(ad.decal.pattern);

    /* Esc — cancel, return to edit mode */
    in = no_input();
    in.esc = true;
    asset_designer_update(&ad, &in);

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
    in = no_input(); in.arrow_right = true; asset_designer_update(&ad, &in);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_FLOOR);
    in = no_input(); in.arrow_right = true; asset_designer_update(&ad, &in);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_CEILING);
    in = no_input(); in.arrow_right = true; asset_designer_update(&ad, &in);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_ALL);
    in = no_input(); in.arrow_right = true; asset_designer_update(&ad, &in);
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
    asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad2, &in);

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
    asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.mode, (int)AD_META_EDIT);

    /* Clear the pre-filled buffer by faking deletion, then type "3.5" */
    /* Easiest: set buffer directly (testing parse logic, not typing) */
    strncpy(ad.meta_edit_buffer, "3.5", sizeof(ad.meta_edit_buffer) - 1);
    ad.meta_edit_len = 3;

    /* Commit */
    in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in);

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
        asset_designer_update(&ad, &in);
        assert_int_equal(ad.mode, (int)AD_META_EDIT);

        strncpy(ad.meta_edit_buffer, bad_vals[i], sizeof(ad.meta_edit_buffer) - 1);
        ad.meta_edit_len = (int)strlen(bad_vals[i]);

        /* Commit */
        in = no_input();
        in.confirm = true;
        asset_designer_update(&ad, &in);

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
        asset_designer_update(&ad, &in);
        assert_int_equal(ad.mode, (int)AD_META_EDIT);

        strncpy(ad.meta_edit_buffer, vals[i], sizeof(ad.meta_edit_buffer) - 1);
        ad.meta_edit_len = (int)strlen(vals[i]);

        in = no_input();
        in.confirm = true;
        asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.mode, (int)AD_META_EDIT);

    strncpy(ad.meta_edit_buffer, "bogus", sizeof(ad.meta_edit_buffer) - 1);
    ad.meta_edit_len = 5;

    in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.mode, (int)AD_META_EDIT);

    strncpy(ad.meta_edit_buffer, "42", sizeof(ad.meta_edit_buffer) - 1);
    ad.meta_edit_len = 2;

    in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in);

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
        asset_designer_update(&ad, &in);
        assert_int_equal(ad.mode, (int)AD_META_EDIT);

        strncpy(ad.meta_edit_buffer, bad_vals[i], sizeof(ad.meta_edit_buffer) - 1);
        ad.meta_edit_len = (int)strlen(bad_vals[i]);

        in = no_input();
        in.confirm = true;
        asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.mode, (int)AD_META_EDIT);
    assert_true(ad.meta_prev_double == 2.5);

    /* Type something */
    strncpy(ad.meta_edit_buffer, "99.0", sizeof(ad.meta_edit_buffer) - 1);
    ad.meta_edit_len = 4;

    /* Esc — cancel */
    in = no_input();
    in.esc = true;
    asset_designer_update(&ad, &in);

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

    asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);
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
        asset_designer_update(&ad, &in);

        assert_int_equal(ad.mode, (int)AD_SAVE_PROMPT);
        asset_designer_destroy(&ad);
    }

    /* F9 -> load select */
    {
        AssetDesignerState ad;
        asset_designer_init(&ad, &cfg, APP_STATE_MAIN_MENU);

        InputState in = no_input();
        in.load = true;
        asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);

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

    asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);

    assert_int_equal(ad.mode, (int)AD_DECAL_EDIT);
    assert_int_equal(ad.filename_buffer[0], '\0');
    assert_int_equal(ad.filename_pos, 0);

    asset_designer_destroy(&ad);
}

/**
 * test_save_prompt_enter_commits_valid — Enter in AD_SAVE_PROMPT with a valid
 * filename saves the file, sets current_filename, clears dirty, and returns
 * mode to AD_DECAL_EDIT.
 *
 * This is the primary regression test for the text-input bug:
 * if SDL_StartTextInput() was never called, text_input stays empty and
 * this flow is never exercised at runtime.  The logic path is tested
 * independently of SDL's text event pipeline.
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
    asset_designer_update(&ad, &in);

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
        asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.metadata_focus, 1);

    in = no_input();
    in.tab = true;
    asset_designer_update(&ad, &in);
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
    asset_designer_update(&ad, &in);

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
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.metadata_row, 1);

    in = no_input();
    in.down = true;
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.metadata_row, 2);

    in = no_input();
    in.up = true;
    asset_designer_update(&ad, &in);
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
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.metadata_row, 0);

    /* Down to last row, then Down again stays at last row */
    ad.metadata_row = AD_META_EDIT_COUNT - 1;
    in = no_input();
    in.down = true;
    asset_designer_update(&ad, &in);
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
    asset_designer_update(&ad, &in);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_FLOOR);

    in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_CEILING);

    in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_ALL);

    in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in);
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
    asset_designer_update(&ad, &in);
    assert_int_equal((int)ad.editor_surface, (int)AD_SURFACE_ALL);

    /* ALL(3) - 1 = CEILING(2) */
    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in);
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
    asset_designer_update(&ad, &in);
    assert_true(ad.decal.width == 1.25);

    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in);
    assert_true(ad.decal.width == 1.0);

    /* Clamp: drive width down to 0.25 then try to go lower */
    ad.decal.width = 0.25;
    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in);
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
    asset_designer_update(&ad, &in);
    assert_true(ad.decal.height == 1.25);

    ad.decal.height = 0.25;
    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in);
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
    asset_designer_update(&ad, &in);
    assert_true(ad.decal.glyph_step_u == 0.125);

    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in);
    assert_true(ad.decal.glyph_step_u == 0.0625);

    /* Clamp at 0.0 */
    ad.decal.glyph_step_u = 0.0;
    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in);
    assert_true(ad.decal.glyph_step_u == 0.0);

    /* step_v row = 4 */
    ad.metadata_row = 4;
    assert_true(ad.decal.glyph_step_v == 0.0625);

    in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in);
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
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.current_material_id, 2);

    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.current_material_id, 1);

    /* Clamp at 1 */
    in = no_input();
    in.arrow_left = true;
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.current_material_id, 1);

    /* Clamp at 255 */
    ad.current_material_id = 255;
    in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in);
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
        asset_designer_update(&ad, &in);
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
        asset_designer_update(&ad, &in);
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
        asset_designer_update(&ad, &in);
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
        asset_designer_update(&ad, &in);
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
        asset_designer_update(&ad, &in);
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
    asset_designer_update(&ad, &in);

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
    ad.glyph_idx           = 2;   /* '#' */
    ad.current_material_id = 7;
    ad.metadata_focus      = 0;   /* canvas focus */

    InputState in = no_input();
    in.place = true;
    asset_designer_update(&ad, &in);

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
    ad.glyph_idx           = 2;   /* '#' */
    InputState in = no_input();
    in.place = true;
    asset_designer_update(&ad, &in);
    assert_int_equal(ad.decal.pattern[0].material_id, 3);

    /* Now change current_material_id to 9 via metadata panel */
    ad.metadata_focus = 1;
    ad.metadata_row   = 5;
    ad.current_material_id = 9;
    in = no_input();
    in.arrow_right = true;
    asset_designer_update(&ad, &in);
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
    ad.glyph_idx           = 2;
    InputState in = no_input();
    in.place = true;
    asset_designer_update(&ad, &in);

    /* Save */
    ad.dirty = 1;
    ad.mode  = AD_SAVE_PROMPT;
    strncpy(ad.filename_buffer, basename, AD_FILENAME_MAX - 1);
    ad.filename_pos = (int)strlen(basename);
    in = no_input();
    in.confirm = true;
    asset_designer_update(&ad, &in);
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
        /* --- Glyph palette --- */
        cmocka_unit_test(test_glyph_cycle_forward),
        cmocka_unit_test(test_glyph_cycle_backward),
        cmocka_unit_test(test_glyph_cycle_wraps),
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
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
