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
 *   AD_META_EDIT    — user types a replacement value for a metadata field
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
 *
 * Surface ownership model:
 *   editor_surface  — editor-facing source of truth; may be AD_SURFACE_ALL.
 *   decal.surface   — runtime/export field; only written at save/load boundaries
 *                     via a temporary copy — never mutated during editing.
 *
 * Surface=All export:
 *   When editor_surface == AD_SURFACE_ALL, saving writes one file per concrete
 *   surface: <basename>_wall.txt, <basename>_floor.txt, <basename>_ceil.txt.
 *   Each file contains its respective concrete surface value.
 *   current_filename is set to <basename> (the group name).
 */

#ifndef ASSET_DESIGNER_H
#define ASSET_DESIGNER_H

#include "decal.h"     /* Decal, PatternCell, DecalSurface */
#include "config.h"    /* EngineConfig, AppState */
#include "input.h"     /* InputState */
#include "grid.h"      /* Grid */
#include "assets.h"    /* AssetRegistry, Material, Palette, palette_sample */

/* ---- Editor surface enum ---- */

/**
 * AdSurface — Editor-facing surface selector
 *
 * Includes AD_SURFACE_ALL as an export-only convenience value.
 * Does NOT correspond to any runtime DecalSurface value.
 *
 * Map to DecalSurface via the AD_CONCRETE_SURFACES table in asset_designer.c.
 */
typedef enum {
    AD_SURFACE_WALL    = 0,   /* maps to DECAL_SURFACE_WALL    */
    AD_SURFACE_FLOOR   = 1,   /* maps to DECAL_SURFACE_FLOOR   */
    AD_SURFACE_CEILING = 2,   /* maps to DECAL_SURFACE_CEILING */
    AD_SURFACE_ALL     = 3,   /* editor-only: export one file per concrete surface */
    AD_SURFACE_COUNT   = 4    /* total number of AdSurface values (for cycling) */
} AdSurface;

/* ---- Constants ---- */

/** Directory where decal asset files are stored. */
#define AD_DECALS_DIR "assets/decals/"

/** Hard bounds on canvas dimensions (prevent runaway allocations) */
#define AD_MAX_CANVAS_COLS 64
#define AD_MAX_CANVAS_ROWS 32

/** Maximum basename length (excluding ".txt") */
#define AD_FILENAME_MAX 48

/** Maximum number of files shown in the load selector */
#define AD_MAX_LOAD_FILES 64

/** How many frames a status message stays visible (~1.5 s at 120 fps) */
#define AD_STATUS_FRAMES 180

/** Frames before first auto-repeat cursor move fires */
#define AD_REPEAT_DELAY  15
/** Frames between subsequent auto-repeat cursor moves */
#define AD_REPEAT_RATE    4

/** Number of editable rows in the metadata panel (surface..pattern_rows) */
#define AD_META_EDIT_COUNT  8
/** Total metadata rows (all rows are editable) */
#define AD_META_TOTAL_COUNT 8

/* ---- Sub-mode enum ---- */

/**
 * AssetDesignerMode — which UI panel is active inside the designer
 *
 * AD_DECAL_EDIT   — normal canvas editor
 * AD_SAVE_PROMPT  — filename input overlay (press Enter to save, Esc to cancel)
 * AD_LOAD_SELECT  — file list overlay (arrows to navigate, Enter to load)
 * AD_META_EDIT    — metadata field text-entry overlay (Enter commits, Esc cancels)
 */
typedef enum {
    AD_DECAL_EDIT  = 0,
    AD_SAVE_PROMPT = 1,
    AD_LOAD_SELECT = 2,
    AD_META_EDIT   = 3
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

    /* Currently selected glyph for painting.
     * Set by direct typing; defaults to '#'.
     * Must be in the selected material's allowed glyph set. */
    char current_glyph;

    /* 1 = unsaved changes; 0 = clean */
    int dirty;

    /* Cursor auto-repeat for held navigation (see AD_REPEAT_DELAY / AD_REPEAT_RATE) */
    int repeat_timer;   /* frames remaining until next auto-repeat move; 0 = idle */
    int repeat_dir;     /* held direction: 0=up, 1=down, 2=left, 3=right; -1=none */

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

    /* Metadata panel focus and selection */
    int metadata_focus;      /* 0 = canvas focus, 1 = metadata panel focus */
    int metadata_row;        /* currently selected metadata row, 0..AD_META_EDIT_COUNT-1 */

    /* Editor-facing surface (source of truth for surface during editing).
     * May be AD_SURFACE_ALL.  decal.surface is only set at save/load boundaries. */
    AdSurface editor_surface;

    /* Material to apply to newly placed glyphs (1..255).
     * Editor-only state; does not affect existing cells unless the user re-places them. */
    int current_material_id;

    /* Metadata direct-edit (AD_META_EDIT mode) state.
     * Entered by pressing Enter on an editable metadata row in metadata focus.
     * edit_buffer holds the user-typed text; previous value is snapshotted
     * in meta_prev_* fields so Esc can restore it. */
    char meta_edit_buffer[32];   /* mutable text being typed */
    int  meta_edit_len;          /* number of valid chars in meta_edit_buffer */
    double  meta_prev_double;    /* snapshot for width/height/step_u/step_v */
    int     meta_prev_int;       /* snapshot for material_id */
    AdSurface meta_prev_surface; /* snapshot for surface */

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
                                           const InputState *input,
                                           const AssetRegistry *assets);

/**
 * asset_designer_render() — Draw the editor to the grid
 *
 * In AD_DECAL_EDIT: draws canvas, cursor, glyph indicator, status bar.
 * In AD_SAVE_PROMPT: draws canvas + filename prompt overlay.
 * In AD_LOAD_SELECT: draws canvas + file list overlay.
 * In AD_META_EDIT:   draws canvas + metadata edit prompt overlay.
 */
void asset_designer_render(const AssetDesignerState *s, Grid *grid,
                                const AssetRegistry *assets);

#endif /* ASSET_DESIGNER_H */
