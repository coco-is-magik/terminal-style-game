/**
 * asset_designer.h — Decal canvas editor state and public API
 *
 * The Asset Designer provides an interactive ASCII canvas for creating and
 * editing Decal assets.  Decals are saved and loaded as named files under
 * assets/decals/ using the same key-value format the engine runtime reads.
 *
 * Sub-modes:
 *   AD_DECAL_EDIT   — normal canvas editing (cursor, place, erase, palette)
 *   AD_SAVE_PROMPT  — user types a basename for the save file; Enter saves
 *   AD_LOAD_SELECT  — user navigates a sorted file list; Enter loads selected
 *
 * Lifecycle:
 *   1. Allocate (stack or static):   AssetDesignerState ad;
 *   2. Enter:  asset_designer_init(&ad, cfg, APP_STATE_MAIN_MENU)
 *   3. Per frame: result = asset_designer_update(&ad, &input)
 *   4. Per frame: asset_designer_render(&ad, grid)
 *   5. App handles result (exit, confirm dialog)
 *   6. On leave: asset_designer_destroy(&ad)
 *
 * Ownership:
 *   asset_designer_init() allocates ad.decal.pattern on the heap.
 *   asset_designer_destroy() calls decal_release_contents() to free it.
 *   The AssetDesignerState struct itself is caller-owned (typically stack).
 *
 * Save/Load path convention:
 *   All files are saved to / loaded from "assets/decals/<basename>.txt".
 *   <basename> must satisfy ad_validate_basename(): [A-Za-z0-9_-] only,
 *   no dots, no slashes, no empty string.  .txt is appended automatically.
 */

#ifndef ASSET_DESIGNER_H
#define ASSET_DESIGNER_H

#include "decal.h"     /* Decal, PatternCell, DecalSurface */
#include "config.h"    /* EngineConfig, AppState */
#include "input.h"     /* InputState */
#include "grid.h"      /* Grid */

/* ---- Constants ---- */

/** Directory where decal asset files are stored. */
#define AD_DECALS_DIR "assets/decals/"

/** Glyph palette.  Index 0 = space (erase/blank). */
#define AD_GLYPHS      " .#@XO+-=*"
#define AD_GLYPH_COUNT 10

/** Hard bounds on canvas dimensions (prevent runaway allocations) */
#define AD_MAX_CANVAS_COLS 64
#define AD_MAX_CANVAS_ROWS 32

/** Maximum basename length (excluding ".txt") */
#define AD_FILENAME_MAX 48

/** Maximum number of files shown in the load selector */
#define AD_MAX_LOAD_FILES 64

/** How many frames a status message stays visible (~1.5 s at 120 fps) */
#define AD_STATUS_FRAMES 180

/* ---- Sub-mode enum ---- */

/**
 * AssetDesignerMode — which UI panel is active inside the designer
 *
 * AD_DECAL_EDIT   — normal canvas editor
 * AD_SAVE_PROMPT  — filename input overlay (press Enter to save, Esc to cancel)
 * AD_LOAD_SELECT  — file list overlay (arrows to navigate, Enter to load)
 */
typedef enum {
    AD_DECAL_EDIT  = 0,
    AD_SAVE_PROMPT = 1,
    AD_LOAD_SELECT = 2
} AssetDesignerMode;

/* ---- Result enum ---- */

/**
 * AssetDesignerResult — what the caller should do after update()
 *
 * AD_RESULT_NONE             — continue normally
 * AD_RESULT_EXIT             — leave the designer; transition to return_state
 * AD_RESULT_CONFIRM_DISCARD  — dirty edits exist; push confirm dialog first
 */
typedef enum {
    AD_RESULT_NONE = 0,
    AD_RESULT_EXIT,
    AD_RESULT_CONFIRM_DISCARD
} AssetDesignerResult;

/* ---- State struct ---- */

/**
 * AssetDesignerState — All transient editor state for the decal canvas editor
 *
 * The embedded Decal is stack-owned; decal.pattern is heap-allocated by
 * asset_designer_init().  Call asset_designer_destroy() before discarding.
 */
typedef struct {
    /* The active complete decal.  Pattern is heap-allocated; call destroy(). */
    Decal decal;

    /* Where to return on AD_RESULT_EXIT (set by caller via init) */
    AppState return_state;

    /* Canvas dimensions — copied from EngineConfig on init,
     * clamped to [1..AD_MAX_CANVAS_COLS] / [1..AD_MAX_CANVAS_ROWS] */
    int canvas_cols;
    int canvas_rows;

    /* Cursor position (0-based) */
    int cursor_col;
    int cursor_row;

    /* Current selected glyph index into AD_GLYPHS (0 = space) */
    int glyph_idx;

    /* 1 = unsaved changes; 0 = clean */
    int dirty;

    /* Active sub-mode */
    AssetDesignerMode mode;

    /* Current loaded/saved basename (empty string = <unsaved>) */
    char current_filename[AD_FILENAME_MAX];

    /* Save-prompt: user-typed basename buffer */
    char filename_buffer[AD_FILENAME_MAX];
    int  filename_pos;           /* number of chars in filename_buffer */

    /* Load-selector: sorted list of basenames from assets/decals/ */
    char file_list[AD_MAX_LOAD_FILES][AD_FILENAME_MAX];
    int  file_count;
    int  file_sel_idx;           /* currently highlighted entry */

    /* Transient status message and display-frame countdown */
    char status_msg[128];
    int  status_frames;
} AssetDesignerState;

/* ---- Filename validation (public for test access) ---- */

/**
 * ad_validate_basename() — Check that a string is a safe decal filename stem
 *
 * Accepts only [A-Za-z0-9_-], non-empty, no slashes, no dot sequences.
 * Returns 1 if valid, 0 if invalid.
 *
 * @param name  Candidate basename string (must not be NULL)
 */
int ad_validate_basename(const char *name);

/* ---- Public API ---- */

/**
 * asset_designer_init() — Initialise the designer with a blank canvas
 *
 * Allocates decal.pattern, sets all cells to space, applies sane decal
 * defaults, and copies canvas size from cfg.
 */
void asset_designer_init(AssetDesignerState *s,
                         const EngineConfig *cfg,
                         AppState return_state);

/**
 * asset_designer_destroy() — Release the pattern allocation (idempotent)
 */
void asset_designer_destroy(AssetDesignerState *s);

/**
 * asset_designer_update() — Process one frame of input
 *
 * Dispatches to the active sub-mode handler.  Returns a result telling
 * the caller (app.c) whether to stay, exit cleanly, or push a confirm dialog.
 */
AssetDesignerResult asset_designer_update(AssetDesignerState *s,
                                           const InputState *input);

/**
 * asset_designer_render() — Draw the editor to the grid
 *
 * In AD_DECAL_EDIT: draws canvas, cursor, glyph indicator, status bar.
 * In AD_SAVE_PROMPT: draws canvas dimmed + filename prompt overlay.
 * In AD_LOAD_SELECT: draws canvas dimmed + file list overlay.
 */
void asset_designer_render(const AssetDesignerState *s, Grid *grid);

#endif /* ASSET_DESIGNER_H */
