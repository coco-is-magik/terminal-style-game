/**
 * material_designer.h — Material asset editor state and public API
 *
 * The Material Designer provides an interactive field editor for creating and
 * editing Material assets.  Materials are saved and loaded as named files under
 * assets/materials/ using the same key-value format the engine runtime reads.
 *
 * A material has six editable fields:
 *   0  palette     — palette ID (1–255); cycled from loaded palettes
 *   1  glyph_1     — near-band glyph character
 *   2  glyph_2     — mid-band glyph character
 *   3  glyph_3     — far-band glyph character
 *   4  glyph_4     — very-far-band glyph character
 *   5  explicit_id — integer 0–255; 0 = auto-assign on load, >0 = fixed slot
 *
 * Sub-modes:
 *   MD_EDIT         — field list editing (Tab to focus, arrows to adjust, Enter to text-edit)
 *   MD_SAVE_PROMPT  — user types a basename; Enter saves
 *   MD_LOAD_SELECT  — user navigates a sorted file list; Enter loads selected
 *
 * Lifecycle:
 *   1. Allocate (stack or static):   MaterialDesignerState md;
 *   2. Enter:  material_designer_init(&md, cfg, APP_STATE_MAIN_MENU, assets)
 *   3. Per frame: result = material_designer_update(&md, &input, assets)
 *   4. Per frame: material_designer_render(&md, grid, assets)
 *   5. App handles result (exit, confirm dialog)
 *   6. On leave: material_designer_destroy(&md)
 *
 * Ownership:
 *   MaterialDesignerState contains no heap allocations.
 *   material_designer_destroy() is a no-op (provided for symmetry).
 *
 * Save path convention:
 *   All files are saved to / loaded from "assets/materials/<basename>.txt".
 *   <basename> must satisfy the same constraints as the decal designer.
 *   .txt is appended automatically.
 *
 * Save does NOT mutate the AssetRegistry.  The user must restart or reload
 * assets to see the new material reflected in the running world.
 */

#ifndef MATERIAL_DESIGNER_H
#define MATERIAL_DESIGNER_H

#include "config.h"   /* EngineConfig, AppState */
#include "input.h"    /* InputState */
#include "grid.h"     /* Grid */
#include "assets.h"   /* AssetRegistry, Palette, Material, palette_sample */

/* ---- Constants ---- */

/** Directory where material asset files are stored. */
#define MD_MATERIALS_DIR "assets/materials/"

/** Maximum basename length (excluding ".txt") */
#define MD_FILENAME_MAX 64

/** Maximum number of files shown in the load selector */
#define MD_MAX_LOAD_FILES 64

/** How many frames a status message stays visible */
#define MD_STATUS_FRAMES 180

/** Number of editable metadata rows */
#define MD_FIELD_COUNT 6

/* ---- Sub-mode enum ---- */

/**
 * MaterialDesignerMode — which UI panel is active inside the designer
 *
 * MD_EDIT         — field-list editing
 * MD_SAVE_PROMPT  — filename input overlay
 * MD_LOAD_SELECT  — file list overlay
 */
typedef enum {
    MD_EDIT        = 0,
    MD_SAVE_PROMPT = 1,
    MD_LOAD_SELECT = 2
} MaterialDesignerMode;

/* ---- Result enum ---- */

/**
 * MaterialDesignerResult — what the caller should do after update()
 *
 * MD_RESULT_NONE             — continue normally
 * MD_RESULT_EXIT             — leave the designer; transition to return_state
 * MD_RESULT_CONFIRM_DISCARD  — dirty edits exist; push confirm dialog first
 */
typedef enum {
    MD_RESULT_NONE = 0,
    MD_RESULT_EXIT,
    MD_RESULT_CONFIRM_DISCARD
} MaterialDesignerResult;

/* ---- State struct ---- */

/**
 * MaterialDesignerState — All transient editor state for the material editor
 *
 * No heap allocations; all buffers are fixed-size.
 * Call material_designer_destroy() for symmetry even though it is a no-op.
 */
typedef struct {
    /* Working copy of the material fields */
    int  palette_id;      /* Palette index (1–255) */
    char glyphs[4];       /* Four distance-band glyph characters */
    int  explicit_id;     /* 0 = auto-assign, 1–255 = fixed ID slot */

    /* Where to return on MD_RESULT_EXIT */
    AppState return_state;

    /* Dirty flag (1 = unsaved changes) */
    int dirty;

    /* Active sub-mode */
    MaterialDesignerMode mode;

    /* Current loaded/saved basename (empty string = <unsaved>) */
    char current_filename[MD_FILENAME_MAX];

    /* Save-prompt: user-typed basename buffer */
    char filename_buffer[MD_FILENAME_MAX];
    int  filename_pos;    /* number of chars in filename_buffer */

    /* Field-list focus (0 = no focus; 1 = field list focused) */
    int metadata_focus;
    /* Currently selected field row (0..MD_FIELD_COUNT-1) */
    int metadata_row;

    /* Direct-edit state (Enter on a field row) */
    char meta_edit_buffer[32];   /* mutable text being typed */
    int  meta_edit_len;          /* number of valid chars */
    int  meta_prev_int;          /* snapshot for palette_id, explicit_id */
    char meta_prev_glyph;        /* snapshot for glyph fields */
    int  in_meta_edit;           /* 1 = direct-edit overlay active */

    /* Load-selector: sorted list of basenames from assets/materials/ */
    char file_list[MD_MAX_LOAD_FILES][MD_FILENAME_MAX];
    int  file_count;
    int  file_sel_idx;

    /* Transient status message and display-frame countdown */
    char status_msg[128];
    int  status_frames;
} MaterialDesignerState;

/* ---- Public API ---- */

/**
 * material_designer_init() — Initialise the designer with default values
 *
 * palette_id=1, glyphs="####", explicit_id=0, dirty=1 (new, unsaved).
 *
 * @param s             Designer state to initialise
 * @param cfg           Engine configuration (currently unused but kept for symmetry)
 * @param return_state  AppState to return to on MD_RESULT_EXIT
 * @param assets        AssetRegistry for palette validation (may be NULL)
 */
void material_designer_init(MaterialDesignerState *s,
                             const EngineConfig *cfg,
                             AppState return_state,
                             const AssetRegistry *assets);

/**
 * material_designer_destroy() — Release any resources (currently a no-op)
 *
 * Provided for symmetry with the decal designer lifecycle.
 */
void material_designer_destroy(MaterialDesignerState *s);

/**
 * material_designer_update() — Process one frame of input
 *
 * Dispatches to the active sub-mode handler.  Returns a result telling
 * the caller (app.c) whether to stay, exit cleanly, or push a confirm dialog.
 */
MaterialDesignerResult material_designer_update(MaterialDesignerState *s,
                                                 const InputState *input,
                                                 const AssetRegistry *assets);

/**
 * material_designer_render() — Draw the editor to the grid
 *
 * In MD_EDIT: draws field list, preview pane, status bar.
 * In MD_SAVE_PROMPT: draws field list + filename prompt overlay.
 * In MD_LOAD_SELECT: draws field list + file list overlay.
 */
void material_designer_render(const MaterialDesignerState *s, Grid *grid,
                               const AssetRegistry *assets);

#endif /* MATERIAL_DESIGNER_H */
