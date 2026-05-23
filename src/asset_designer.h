/**
 * asset_designer.h — Decal canvas editor state and public API
 *
 * The Asset Designer provides an interactive ASCII canvas for creating and
 * editing Decal assets.  v2 scope: decal editing only.  Materials and lights
 * are deferred.
 *
 * Lifecycle:
 *   1. Allocate (stack or static):   AssetDesignerState ad;
 *   2. Enter:  asset_designer_init(&ad, cfg, APP_STATE_EDITOR)
 *   3. Per frame: result = asset_designer_update(&ad, &input)
 *   4. Per frame: asset_designer_render(&ad, grid)
 *   5. App.c handles result (exit, confirm dialog)
 *   6. On leave: asset_designer_destroy(&ad)
 *
 * Ownership:
 *   asset_designer_init() allocates ad.decal.pattern on the heap.
 *   asset_designer_destroy() calls decal_release_contents() to free it.
 *   The AssetDesignerState struct itself is caller-owned (typically stack).
 *
 * Save/Load:
 *   Autosave path is fixed at AD_AUTOSAVE_PATH ("assets/decals/autosave.txt").
 *   Save and load use decal_save_to_file() / decal_load_from_file() from
 *   decal_io.h — the same format the engine runtime uses.
 */

#ifndef ASSET_DESIGNER_H
#define ASSET_DESIGNER_H

#include "decal.h"     /* Decal, PatternCell, DecalSurface */
#include "config.h"    /* EngineConfig, AppState */
#include "input.h"     /* InputState */
#include "grid.h"      /* Grid */

/* ---- Constants ---- */

/** Fixed save/load path for v2 (no filename prompts yet) */
#define AD_AUTOSAVE_PATH "assets/decals/autosave.txt"

/** Glyph palette.  Index 0 = space (erase/blank). */
#define AD_GLYPHS      " .#@XO+-=*"
#define AD_GLYPH_COUNT 10

/** Hard bounds on canvas dimensions (prevent runaway allocations) */
#define AD_MAX_CANVAS_COLS 64
#define AD_MAX_CANVAS_ROWS 32

/** How many frames a status message stays visible at 120 fps (~1.5 s) */
#define AD_STATUS_FRAMES 180

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
    /* The active complete decal.  Caller must call asset_designer_destroy()
     * when done to free decal.pattern. */
    Decal decal;

    /* Where to return on AD_RESULT_EXIT (set by caller via init) */
    AppState return_state;

    /* Canvas dimensions copied from EngineConfig on init.
     * Clamped to [1..AD_MAX_CANVAS_COLS] / [1..AD_MAX_CANVAS_ROWS]. */
    int canvas_cols;
    int canvas_rows;

    /* Cursor position (0-based) */
    int cursor_col;
    int cursor_row;

    /* Current selected glyph index into AD_GLYPHS (0 = space) */
    int glyph_idx;

    /* 1 = unsaved changes; 0 = clean */
    int dirty;

    /* Transient status message and display-frame countdown */
    char status_msg[64];
    int  status_frames;
} AssetDesignerState;

/* ---- Public API ---- */

/**
 * asset_designer_init() — Initialise the designer with a blank canvas
 *
 * Allocates decal.pattern, sets all cells to space, applies sane decal
 * defaults, and copies canvas size from cfg.  Sets return_state so the
 * caller does not need a separate setter.
 *
 * @param s             State to initialise (must not be NULL)
 * @param cfg           Engine configuration (canvas size source; must not be NULL)
 * @param return_state  AppState to restore when the designer exits
 */
void asset_designer_init(AssetDesignerState *s,
                         const EngineConfig *cfg,
                         AppState return_state);

/**
 * asset_designer_destroy() — Release the pattern allocation
 *
 * Calls decal_release_contents() on the embedded Decal.  Safe to call
 * multiple times (idempotent via decal_release_contents semantics).
 *
 * @param s  State whose resources to release (must not be NULL)
 */
void asset_designer_destroy(AssetDesignerState *s);

/**
 * asset_designer_update() — Process one frame of input
 *
 * Handles cursor movement, glyph placement, glyph cycling, save, load, and
 * exit.  Does NOT push or modify any menu stack; the caller (app.c) handles
 * menu transitions based on the returned result.
 *
 * @param s      Designer state
 * @param input  Current frame input snapshot
 * @return       AD_RESULT_NONE / AD_RESULT_EXIT / AD_RESULT_CONFIRM_DISCARD
 */
AssetDesignerResult asset_designer_update(AssetDesignerState *s,
                                           const InputState *input);

/**
 * asset_designer_render() — Draw the editor canvas to the grid
 *
 * Clears the grid and renders:
 *   - Title bar and key-hint footer
 *   - The decal pattern cells (space shown as '·')
 *   - The cursor cell (inverted colours)
 *   - The current glyph indicator
 *   - The status message (if active)
 *
 * @param s     Designer state (read-only)
 * @param grid  Destination grid
 */
void asset_designer_render(const AssetDesignerState *s, Grid *grid);

#endif /* ASSET_DESIGNER_H */
