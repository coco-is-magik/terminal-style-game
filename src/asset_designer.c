/**
 * asset_designer.c — Decal canvas editor implementation
 *
 * Implements the public API declared in asset_designer.h:
 *   init, destroy, update (input handling), render (grid drawing)
 *
 * Input routing:
 *   Up/Down arrows    — cursor vertical movement
 *   Left/Right arrows — cursor horizontal movement
 *   Space             — place current glyph at cursor
 *   Backspace         — erase cell (set to space)
 *   Q                 — cycle palette backward
 *   E                 — cycle palette forward
 *   F5                — autosave to AD_AUTOSAVE_PATH
 *   F9                — autoload from AD_AUTOSAVE_PATH
 *   ESC               — exit (clean) or request discard confirm (dirty)
 *
 * See asset_designer.h for ownership, lifecycle, and data model docs.
 */

#include "asset_designer.h"
#include "decal_io.h"   /* decal_save_to_file, decal_load_from_file,
                           decal_free, decal_release_contents */

#include <SDL3/SDL.h>   /* SDL_Color */
#include <stdlib.h>     /* calloc, free */
#include <string.h>     /* memset, strncpy */
#include <stdio.h>      /* snprintf */

/* ===================================================================
 *  Private helpers
 * =================================================================== */

/** Canvas grid origin — top-left cell where the pattern is drawn */
#define CANVAS_X0  2
#define CANVAS_Y0  4

static void ad_set_status(AssetDesignerState *s, const char *msg) {
    strncpy(s->status_msg, msg, sizeof(s->status_msg) - 1);
    s->status_msg[sizeof(s->status_msg) - 1] = '\0';
    s->status_frames = AD_STATUS_FRAMES;
}

static void ad_save(AssetDesignerState *s) {
    if (!s->dirty) {
        ad_set_status(s, "No changes to save.");
        return;
    }
    int ret = decal_save_to_file(AD_AUTOSAVE_PATH, &s->decal);
    if (ret == 0) {
        s->dirty = 0;
        ad_set_status(s, "Saved to autosave.txt");
    } else {
        ad_set_status(s, "Save failed!");
    }
}

static void ad_load(AssetDesignerState *s) {
    Decal *loaded = decal_load_from_file(AD_AUTOSAVE_PATH);
    if (!loaded) {
        ad_set_status(s, "No autosave found.");
        return;
    }

    /* Validate dimensions fit our canvas limits */
    if (loaded->pattern_cols < 1 || loaded->pattern_cols > AD_MAX_CANVAS_COLS ||
        loaded->pattern_rows < 1 || loaded->pattern_rows > AD_MAX_CANVAS_ROWS) {
        decal_free(loaded);
        ad_set_status(s, "Load failed: bad dimensions.");
        return;
    }

    /* Replace the embedded decal.  Transfer the pattern pointer. */
    decal_release_contents(&s->decal);  /* free old pattern */
    s->decal = *loaded;                  /* struct copy; transfers pattern ptr */
    free(loaded);                        /* free wrapper only */

    /* Update canvas size to match loaded decal */
    s->canvas_cols = s->decal.pattern_cols;
    s->canvas_rows = s->decal.pattern_rows;

    /* Clamp cursor */
    if (s->cursor_col >= s->canvas_cols) s->cursor_col = s->canvas_cols - 1;
    if (s->cursor_row >= s->canvas_rows) s->cursor_row = s->canvas_rows - 1;

    s->dirty = 0;
    ad_set_status(s, "Loaded autosave.txt");
}

/* ===================================================================
 *  Public API
 * =================================================================== */

void asset_designer_init(AssetDesignerState *s,
                         const EngineConfig *cfg,
                         AppState return_state) {
    memset(s, 0, sizeof(*s));

    s->return_state = return_state;

    /* Copy canvas size from config, clamped to safe bounds */
    s->canvas_cols = cfg->asset_canvas_cols;
    s->canvas_rows = cfg->asset_canvas_rows;
    if (s->canvas_cols < 1)  s->canvas_cols = 1;
    if (s->canvas_cols > AD_MAX_CANVAS_COLS) s->canvas_cols = AD_MAX_CANVAS_COLS;
    if (s->canvas_rows < 1)  s->canvas_rows = 1;
    if (s->canvas_rows > AD_MAX_CANVAS_ROWS) s->canvas_rows = AD_MAX_CANVAS_ROWS;

    /* Allocate the flat pattern — calloc zeroes all cells (glyph=0, mat=0) */
    int cells = s->canvas_cols * s->canvas_rows;
    s->decal.pattern = calloc((size_t)cells, sizeof(PatternCell));
    /* If allocation fails, pattern stays NULL — update/render guard against it */

    /* Fill cells with space (0x20) and default material 1 */
    if (s->decal.pattern) {
        for (int i = 0; i < cells; i++) {
            s->decal.pattern[i].glyph       = ' ';
            s->decal.pattern[i].material_id = 1;
        }
    }

    /* Decal metadata defaults */
    s->decal.surface      = DECAL_SURFACE_WALL;
    s->decal.width        = 1.0f;
    s->decal.height       = 1.0f;
    s->decal.glyph_step_u = 0.0625f;
    s->decal.glyph_step_v = 0.0625f;
    s->decal.depth        = 0.001f;
    s->decal.pattern_cols = s->canvas_cols;
    s->decal.pattern_rows = s->canvas_rows;

    /* Start cursor at (0,0), glyph '#' which is index 2 in AD_GLYPHS " .#@XO+-=*" */
    s->cursor_col = 0;
    s->cursor_row = 0;
    s->glyph_idx  = 2;   /* '#' */

    s->dirty = 0;
}

void asset_designer_destroy(AssetDesignerState *s) {
    decal_release_contents(&s->decal);
}

AssetDesignerResult asset_designer_update(AssetDesignerState *s,
                                           const InputState *input) {
    if (!s || !input) return AD_RESULT_NONE;
    if (!s->decal.pattern) return AD_RESULT_NONE;

    /* ---- Cursor movement ---- */
    if (input->up && s->cursor_row > 0)
        s->cursor_row--;
    if (input->down && s->cursor_row < s->canvas_rows - 1)
        s->cursor_row++;
    if (input->arrow_left && s->cursor_col > 0)
        s->cursor_col--;
    if (input->arrow_right && s->cursor_col < s->canvas_cols - 1)
        s->cursor_col++;

    /* ---- Glyph palette cycling ---- */
    if (input->prev_glyph) {
        s->glyph_idx = (s->glyph_idx - 1 + AD_GLYPH_COUNT) % AD_GLYPH_COUNT;
    }
    if (input->next_glyph) {
        s->glyph_idx = (s->glyph_idx + 1) % AD_GLYPH_COUNT;
    }

    /* ---- Place glyph ---- */
    if (input->place) {
        int idx = s->cursor_row * s->canvas_cols + s->cursor_col;
        char g = AD_GLYPHS[s->glyph_idx];
        if (s->decal.pattern[idx].glyph != (uint8_t)g) {
            s->decal.pattern[idx].glyph = (uint8_t)g;
            s->dirty = 1;
        }
    }

    /* ---- Erase glyph ---- */
    if (input->erase) {
        int idx = s->cursor_row * s->canvas_cols + s->cursor_col;
        if (s->decal.pattern[idx].glyph != ' ') {
            s->decal.pattern[idx].glyph = ' ';
            s->dirty = 1;
        }
    }

    /* ---- Save / Load ---- */
    if (input->save) ad_save(s);
    if (input->load) ad_load(s);

    /* ---- Status timer countdown ---- */
    if (s->status_frames > 0) s->status_frames--;
    if (s->status_frames == 0) s->status_msg[0] = '\0';

    /* ---- Exit / cancel ---- */
    if (input->esc) {
        if (s->dirty) {
            return AD_RESULT_CONFIRM_DISCARD;
        }
        return AD_RESULT_EXIT;
    }

    return AD_RESULT_NONE;
}

void asset_designer_render(const AssetDesignerState *s, Grid *grid) {
    if (!s || !grid) return;

    /* Colours */
    SDL_Color bg_black  = {  0,   0,   0, 255};
    SDL_Color fg_white  = {255, 255, 255, 255};
    SDL_Color fg_gray   = {150, 150, 150, 255};
    SDL_Color fg_yellow = {255, 255,   0, 255};
    SDL_Color fg_green  = { 50, 255,  50, 255};
    SDL_Color fg_red    = {255,  80,  80, 255};
    SDL_Color bg_cursor = {  0,  80, 150, 255};

    grid_clear(grid, bg_black);

    /* ---- Title bar ---- */
    char title[64];
    snprintf(title, sizeof(title),
             s->dirty ? "=== DECAL DESIGNER [unsaved] ===" : "=== DECAL DESIGNER ===");
    grid_print(grid, CANVAS_X0, 0, title,
               s->dirty ? fg_yellow : fg_white, bg_black);

    /* ---- Key hints ---- */
    grid_print(grid, CANVAS_X0, 1,
               "Arrows:Move  Spc:Place  Bksp:Erase  Q/E:Glyph  F5:Save  F9:Load  Esc:Exit",
               fg_gray, bg_black);

    /* ---- Current glyph indicator ---- */
    char glyph_line[64];
    char cur_glyph = AD_GLYPHS[s->glyph_idx];
    snprintf(glyph_line, sizeof(glyph_line),
             "Glyph: [%c]   Canvas: %dx%d   Cursor: (%d,%d)",
             cur_glyph == ' ' ? '_' : cur_glyph,
             s->canvas_cols, s->canvas_rows,
             s->cursor_col, s->cursor_row);
    grid_print(grid, CANVAS_X0, 2, glyph_line, fg_green, bg_black);

    /* ---- Canvas cells ---- */
    if (s->decal.pattern) {
        for (int r = 0; r < s->canvas_rows; r++) {
            for (int c = 0; c < s->canvas_cols; c++) {
                int gx = CANVAS_X0 + c;
                int gy = CANVAS_Y0 + r;
                if (gx >= grid->width || gy >= grid->height) continue;

                uint8_t glyph = s->decal.pattern[r * s->canvas_cols + c].glyph;
                bool is_cursor = (r == s->cursor_row && c == s->cursor_col);

                SDL_Color cell_fg = is_cursor ? bg_black : fg_white;
                SDL_Color cell_bg = is_cursor ? bg_cursor : bg_black;
                uint8_t   display = (glyph == ' ' && !is_cursor) ? (uint8_t)'.' : glyph;

                if (glyph == ' ' && !is_cursor) {
                    cell_fg = fg_gray;
                }

                grid_set(grid, gx, gy, display, cell_fg, cell_bg);
            }
        }
    }

    /* ---- Status message ---- */
    int status_y = CANVAS_Y0 + s->canvas_rows + 1;
    if (status_y < grid->height && s->status_frames > 0 && s->status_msg[0] != '\0') {
        grid_print(grid, CANVAS_X0, status_y, s->status_msg, fg_red, bg_black);
    }
}
