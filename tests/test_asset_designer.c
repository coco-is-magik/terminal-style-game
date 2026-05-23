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
        /* --- Destroy --- */
        cmocka_unit_test(test_destroy_idempotent),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
