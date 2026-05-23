/**
 * asset_designer.c — Decal canvas editor implementation
 *
 * Sub-mode dispatch:
 *   AD_DECAL_EDIT   — cursor, place, erase, palette, F5/F9/F10, ESC
 *   AD_SAVE_PROMPT  — text input, backspace, Enter to confirm, Esc to cancel
 *   AD_LOAD_SELECT  — up/down navigation, Enter to load, Esc to cancel
 *
 * Input rules:
 *   All key handling is done via InputState fields from input.c.
 *   No SDL keycodes appear in this file.
 */

#include "asset_designer.h"
#include "decal_io.h"   /* decal_save_to_file, decal_load_from_file,
                           decal_free, decal_release_contents */

#include <SDL3/SDL.h>   /* SDL_Color */
#include <stdlib.h>     /* calloc, free, qsort */
#include <string.h>     /* memset, strncpy, strlen, strcmp, memcpy */
#include <stdio.h>      /* snprintf */
#include <stdbool.h>    /* bool */
#include <dirent.h>     /* opendir, readdir, closedir, struct dirent */

/* Canvas grid origin — top-left cell of the pattern area */
#define CANVAS_X0  2
#define CANVAS_Y0  4

/* ===================================================================
 *  Filename helpers
 * =================================================================== */

/**
 * ad_validate_basename() — Public; accepts only [A-Za-z0-9_-], non-empty.
 * Returns 1 if valid, 0 otherwise.
 */
int ad_validate_basename(const char *name) {
    if (!name || name[0] == '\0') return 0;
    /* Reject . and .. explicitly */
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;
    for (const char *p = name; *p; p++) {
        char c = *p;
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '_' || c == '-')
            continue;
        return 0;
    }
    return 1;
}

/** Build a full "assets/decals/<basename>.txt" path into out. */
static void ad_build_path(char *out, size_t out_sz, const char *basename) {
    snprintf(out, out_sz, "%s%s.txt", AD_DECALS_DIR, basename);
}

/* ===================================================================
 *  Private helpers
 * =================================================================== */

static void ad_set_status(AssetDesignerState *s, const char *msg) {
    strncpy(s->status_msg, msg, sizeof(s->status_msg) - 1);
    s->status_msg[sizeof(s->status_msg) - 1] = '\0';
    s->status_frames = AD_STATUS_FRAMES;
}

/* ===================================================================
 *  Save
 * =================================================================== */

static void ad_save_named(AssetDesignerState *s, const char *basename) {
    char path[128];
    ad_build_path(path, sizeof(path), basename);

    int ret = decal_save_to_file(path, &s->decal);
    if (ret == 0) {
        strncpy(s->current_filename, basename, AD_FILENAME_MAX - 1);
        s->current_filename[AD_FILENAME_MAX - 1] = '\0';
        s->dirty = 0;
        char msg[128];
        snprintf(msg, sizeof(msg), "Saved: %s.txt", basename);
        ad_set_status(s, msg);
    } else {
        ad_set_status(s, "Error: could not write file.");
    }
}

/* ===================================================================
 *  Directory scan + sort for load selector
 * =================================================================== */

static int cmp_entry(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

static void ad_scan_files(AssetDesignerState *s) {
    s->file_count   = 0;
    s->file_sel_idx = 0;

    DIR *dir = opendir(AD_DECALS_DIR);
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && s->file_count < AD_MAX_LOAD_FILES) {
        const char *name = ent->d_name;
        size_t len = strlen(name);
        /* Must be at least "a.txt" (5 chars) and end with ".txt" */
        if (len < 5) continue;
        if (strcmp(name + len - 4, ".txt") != 0) continue;
        size_t base_len = len - 4;
        if (base_len == 0 || base_len >= AD_FILENAME_MAX) continue;
        char base[AD_FILENAME_MAX];
        memcpy(base, name, base_len);
        base[base_len] = '\0';
        if (!ad_validate_basename(base)) continue;
        memcpy(s->file_list[s->file_count], base, base_len + 1);
        s->file_count++;
    }
    closedir(dir);

    if (s->file_count > 1) {
        qsort(s->file_list, (size_t)s->file_count,
              AD_FILENAME_MAX, cmp_entry);
    }
}

/* ===================================================================
 *  Load
 * =================================================================== */

static void ad_load_named(AssetDesignerState *s, const char *basename) {
    char path[128];
    ad_build_path(path, sizeof(path), basename);

    Decal *loaded = decal_load_from_file(path);
    if (!loaded) {
        ad_set_status(s, "Error: could not load file.");
        return;
    }
    if (loaded->pattern_cols < 1 || loaded->pattern_cols > AD_MAX_CANVAS_COLS ||
        loaded->pattern_rows < 1 || loaded->pattern_rows > AD_MAX_CANVAS_ROWS) {
        decal_free(loaded);
        ad_set_status(s, "Error: decal dimensions out of range.");
        return;
    }

    /* Swap the pattern pointer into the embedded decal */
    decal_release_contents(&s->decal);
    s->decal = *loaded;
    free(loaded);            /* free wrapper only; pattern now owned by s->decal */

    s->canvas_cols = s->decal.pattern_cols;
    s->canvas_rows = s->decal.pattern_rows;
    if (s->cursor_col >= s->canvas_cols) s->cursor_col = s->canvas_cols - 1;
    if (s->cursor_row >= s->canvas_rows) s->cursor_row = s->canvas_rows - 1;

    strncpy(s->current_filename, basename, AD_FILENAME_MAX - 1);
    s->current_filename[AD_FILENAME_MAX - 1] = '\0';
    s->dirty = 0;

    char msg[128];
    snprintf(msg, sizeof(msg), "Loaded: %s.txt", basename);
    ad_set_status(s, msg);
}

/* ===================================================================
 *  Sub-mode update handlers
 * =================================================================== */

static AssetDesignerResult ad_update_edit(AssetDesignerState *s,
                                           const InputState *input) {
    /* Cursor movement */
    if (input->up         && s->cursor_row > 0)              s->cursor_row--;
    if (input->down       && s->cursor_row < s->canvas_rows - 1) s->cursor_row++;
    if (input->arrow_left && s->cursor_col > 0)              s->cursor_col--;
    if (input->arrow_right&& s->cursor_col < s->canvas_cols - 1) s->cursor_col++;

    /* Glyph palette cycling */
    if (input->prev_glyph)
        s->glyph_idx = (s->glyph_idx - 1 + AD_GLYPH_COUNT) % AD_GLYPH_COUNT;
    if (input->next_glyph)
        s->glyph_idx = (s->glyph_idx + 1) % AD_GLYPH_COUNT;

    /* Place glyph */
    if (input->place) {
        int idx = s->cursor_row * s->canvas_cols + s->cursor_col;
        char g  = AD_GLYPHS[s->glyph_idx];
        if (s->decal.pattern[idx].glyph != (uint8_t)g) {
            s->decal.pattern[idx].glyph = (uint8_t)g;
            s->dirty = 1;
        }
    }

    /* Erase glyph */
    if (input->erase) {
        int idx = s->cursor_row * s->canvas_cols + s->cursor_col;
        if (s->decal.pattern[idx].glyph != (uint8_t)' ') {
            s->decal.pattern[idx].glyph = (uint8_t)' ';
            s->dirty = 1;
        }
    }

    /* F5 — save to current filename, or open save prompt */
    if (input->save) {
        if (s->current_filename[0] != '\0') {
            ad_save_named(s, s->current_filename);
        } else {
            s->filename_buffer[0] = '\0';
            s->filename_pos       = 0;
            s->mode = AD_SAVE_PROMPT;
        }
    }

    /* F10 — always open save prompt; pre-fill with current name */
    if (input->save_as) {
        strncpy(s->filename_buffer, s->current_filename, AD_FILENAME_MAX - 1);
        s->filename_buffer[AD_FILENAME_MAX - 1] = '\0';
        s->filename_pos = (int)strlen(s->filename_buffer);
        s->mode = AD_SAVE_PROMPT;
    }

    /* F9 — scan and open load selector */
    if (input->load) {
        ad_scan_files(s);
        s->mode = AD_LOAD_SELECT;
    }

    /* ESC — exit clean or confirm discard */
    if (input->esc) {
        return s->dirty ? AD_RESULT_CONFIRM_DISCARD : AD_RESULT_EXIT;
    }

    return AD_RESULT_NONE;
}

static AssetDesignerResult ad_update_save_prompt(AssetDesignerState *s,
                                                  const InputState *input) {
    /* Append valid chars from the SDL text-input buffer */
    for (int i = 0; i < input->text_input_len; i++) {
        char c = input->text_input[i];
        if (s->filename_pos >= AD_FILENAME_MAX - 1) break;
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '_' || c == '-') {
            s->filename_buffer[s->filename_pos++] = c;
            s->filename_buffer[s->filename_pos]   = '\0';
        }
    }

    /* Backspace (erase) — delete last char */
    if (input->erase && s->filename_pos > 0) {
        s->filename_buffer[--s->filename_pos] = '\0';
    }

    /* Enter — validate and save */
    if (input->confirm) {
        if (!ad_validate_basename(s->filename_buffer)) {
            ad_set_status(s, "Invalid filename (use A-Za-z0-9_- only).");
        } else {
            ad_save_named(s, s->filename_buffer);
            s->filename_buffer[0] = '\0';
            s->filename_pos = 0;
            s->mode = AD_DECAL_EDIT;
        }
    }

    /* ESC — cancel */
    if (input->esc) {
        s->filename_buffer[0] = '\0';
        s->filename_pos = 0;
        s->mode = AD_DECAL_EDIT;
    }

    return AD_RESULT_NONE;
}

static AssetDesignerResult ad_update_load_select(AssetDesignerState *s,
                                                  const InputState *input) {
    if (s->file_count > 0) {
        if (input->up   && s->file_sel_idx > 0)               s->file_sel_idx--;
        if (input->down && s->file_sel_idx < s->file_count - 1) s->file_sel_idx++;
    }

    if (input->confirm && s->file_count > 0) {
        ad_load_named(s, s->file_list[s->file_sel_idx]);
        s->mode = AD_DECAL_EDIT;
    }

    if (input->esc) {
        s->mode = AD_DECAL_EDIT;
    }

    return AD_RESULT_NONE;
}

/* ===================================================================
 *  Public API — init / destroy / update / render
 * =================================================================== */

void asset_designer_init(AssetDesignerState *s,
                         const EngineConfig *cfg,
                         AppState return_state) {
    memset(s, 0, sizeof(*s));

    s->return_state = return_state;

    s->canvas_cols = cfg->asset_canvas_cols;
    s->canvas_rows = cfg->asset_canvas_rows;
    if (s->canvas_cols < 1)                   s->canvas_cols = 1;
    if (s->canvas_cols > AD_MAX_CANVAS_COLS)  s->canvas_cols = AD_MAX_CANVAS_COLS;
    if (s->canvas_rows < 1)                   s->canvas_rows = 1;
    if (s->canvas_rows > AD_MAX_CANVAS_ROWS)  s->canvas_rows = AD_MAX_CANVAS_ROWS;

    int cells = s->canvas_cols * s->canvas_rows;
    s->decal.pattern = calloc((size_t)cells, sizeof(PatternCell));

    if (s->decal.pattern) {
        for (int i = 0; i < cells; i++) {
            s->decal.pattern[i].glyph       = (uint8_t)' ';
            s->decal.pattern[i].material_id = 1;
        }
    }

    s->decal.surface      = DECAL_SURFACE_WALL;
    s->decal.width        = 1.0f;
    s->decal.height       = 1.0f;
    s->decal.glyph_step_u = 0.0625f;
    s->decal.glyph_step_v = 0.0625f;
    s->decal.depth        = 0.001f;
    s->decal.pattern_cols = s->canvas_cols;
    s->decal.pattern_rows = s->canvas_rows;

    s->cursor_col          = 0;
    s->cursor_row          = 0;
    s->glyph_idx           = 2;   /* '#' in " .#@XO+-=*" */
    s->dirty               = 0;
    s->mode                = AD_DECAL_EDIT;
    s->current_filename[0] = '\0';
    s->filename_buffer[0]  = '\0';
    s->filename_pos        = 0;
}

void asset_designer_destroy(AssetDesignerState *s) {
    if (s) decal_release_contents(&s->decal);
}

AssetDesignerResult asset_designer_update(AssetDesignerState *s,
                                           const InputState *input) {
    if (!s || !input)         return AD_RESULT_NONE;
    if (!s->decal.pattern)    return AD_RESULT_NONE;

    AssetDesignerResult result = AD_RESULT_NONE;

    switch (s->mode) {
        case AD_DECAL_EDIT:
            result = ad_update_edit(s, input);
            break;
        case AD_SAVE_PROMPT:
            result = ad_update_save_prompt(s, input);
            break;
        case AD_LOAD_SELECT:
            result = ad_update_load_select(s, input);
            break;
        default:
            break;
    }

    /* Status timer counts down in all modes */
    if (s->status_frames > 0) {
        s->status_frames--;
        if (s->status_frames == 0) s->status_msg[0] = '\0';
    }

    return result;
}

void asset_designer_render(const AssetDesignerState *s, Grid *grid) {
    if (!s || !grid) return;

    SDL_Color bg_black  = {  0,   0,   0, 255};
    SDL_Color fg_white  = {255, 255, 255, 255};
    SDL_Color fg_gray   = {150, 150, 150, 255};
    SDL_Color fg_yellow = {255, 255,   0, 255};
    SDL_Color fg_green  = { 50, 255,  50, 255};
    SDL_Color fg_red    = {255,  80,  80, 255};
    SDL_Color fg_cyan   = {  0, 220, 220, 255};
    SDL_Color bg_cursor = {  0,  80, 150, 255};
    SDL_Color bg_sel    = { 50, 100,  50, 255};

    grid_clear(grid, bg_black);

    /* ---- Title bar: filename and dirty indicator ---- */
    const char *fname = s->current_filename[0] ? s->current_filename : "<unsaved>";
    char title[128];
    snprintf(title, sizeof(title),
             s->dirty ? "=== DECAL DESIGNER [*] File: %s ===" :
                        "=== DECAL DESIGNER     File: %s ===",
             fname);
    grid_print(grid, CANVAS_X0, 0, title,
               s->dirty ? fg_yellow : fg_white, bg_black);

    /* ---- Key hints (edit mode only) ---- */
    if (s->mode == AD_DECAL_EDIT) {
        grid_print(grid, CANVAS_X0, 1,
                   "Arrows:Move  Spc:Place  Bksp:Erase  Q/E:Glyph"
                   "  F5:Save  F9:Load  F10:SaveAs  Esc:Exit",
                   fg_gray, bg_black);
    }

    /* ---- Glyph indicator ---- */
    char cur_glyph = AD_GLYPHS[s->glyph_idx];
    char glyph_line[80];
    snprintf(glyph_line, sizeof(glyph_line),
             "Glyph:[%c]  Canvas:%dx%d  Cursor:(%d,%d)",
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
                bool is_cursor = (s->mode == AD_DECAL_EDIT) &&
                                 (r == s->cursor_row && c == s->cursor_col);

                SDL_Color cell_fg = is_cursor ? bg_black : fg_white;
                SDL_Color cell_bg = is_cursor ? bg_cursor : bg_black;
                uint8_t   display = (glyph == (uint8_t)' ' && !is_cursor)
                                    ? (uint8_t)'.' : glyph;

                if (glyph == (uint8_t)' ' && !is_cursor) cell_fg = fg_gray;

                grid_set(grid, gx, gy, display, cell_fg, cell_bg);
            }
        }
    }

    /* ---- Status message ---- */
    int status_y = CANVAS_Y0 + s->canvas_rows + 1;
    if (status_y < grid->height && s->status_frames > 0 && s->status_msg[0]) {
        grid_print(grid, CANVAS_X0, status_y, s->status_msg, fg_red, bg_black);
    }

    /* ---- Save-prompt overlay ---- */
    if (s->mode == AD_SAVE_PROMPT) {
        int oy = CANVAS_Y0;
        grid_print(grid, CANVAS_X0, oy,
                   "[ Save As: type name then Enter ]", fg_cyan, bg_black);
        char buf[80];
        snprintf(buf, sizeof(buf), "  Name: %s_", s->filename_buffer);
        grid_print(grid, CANVAS_X0, oy + 1, buf, fg_white, bg_black);
        grid_print(grid, CANVAS_X0, oy + 2,
                   "  Enter:save  Esc:cancel  Backspace:delete",
                   fg_gray, bg_black);
    }

    /* ---- Load-select overlay ---- */
    if (s->mode == AD_LOAD_SELECT) {
        int oy = CANVAS_Y0;
        grid_print(grid, CANVAS_X0, oy, "[ Load Decal ]", fg_cyan, bg_black);
        if (s->file_count == 0) {
            grid_print(grid, CANVAS_X0, oy + 1,
                       "  (no decal files found)", fg_gray, bg_black);
        } else {
            int start = s->file_sel_idx - 4;
            if (start < 0) start = 0;
            int end = start + 10;
            if (end > s->file_count) end = s->file_count;

            for (int i = start; i < end && (oy + 1 + i - start) < grid->height; i++) {
                int row = oy + 1 + (i - start);
                bool sel = (i == s->file_sel_idx);
                SDL_Color fg = sel ? bg_black : fg_white;
                SDL_Color bg = sel ? bg_sel   : bg_black;
                char entry[72];
                snprintf(entry, sizeof(entry), "%s%s.txt",
                         sel ? "> " : "  ", s->file_list[i]);
                grid_print(grid, CANVAS_X0, row, entry, fg, bg);
            }
        }
        int hint_y = oy + 12;
        if (hint_y < grid->height) {
            grid_print(grid, CANVAS_X0, hint_y,
                       "  Arrows:navigate  Enter:load  Esc:cancel",
                       fg_gray, bg_black);
        }
    }
}
