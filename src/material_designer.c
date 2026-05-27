/**
 * material_designer.c — Material asset editor implementation
 *
 * Sub-mode dispatch:
 *   MD_EDIT         — field list editing (Tab focus, arrows adjust, Enter text-edit)
 *   MD_SAVE_PROMPT  — text input, backspace, Enter to confirm, Esc to cancel
 *   MD_LOAD_SELECT  — up/down navigation, Enter to load, Esc to cancel
 *
 * All key handling is done via InputState fields from input.c.
 * No SDL keycodes appear in this file.
 *
 * Save does NOT mutate the AssetRegistry.
 */

#include "material_designer.h"

#include <SDL3/SDL.h>   /* SDL_Color */
#include <stdlib.h>     /* calloc, free, qsort, strtol, strtod */
#include <string.h>     /* memset, strncpy, strlen, strcmp, memcpy */
#include <stdio.h>      /* snprintf, fopen, fclose, fgets, fprintf */
#include <stdbool.h>    /* bool */
#include <dirent.h>     /* opendir, readdir, closedir, struct dirent */
#include <ctype.h>      /* isprint */

/* ===================================================================
 *  Constants
 * =================================================================== */

/* Box-drawing coordinates for modal overlays */
#define MD_PANEL_X  4
#define MD_PANEL_Y  4

/* Preview pane x position relative to panel */
#define MD_PREVIEW_X  40
#define MD_PREVIEW_Y  4

/* Field labels — must match MD_FIELD_COUNT */
static const char * const MD_FIELD_LABELS[MD_FIELD_COUNT] = {
    "palette  ", "glyph_1  ", "glyph_2  ", "glyph_3  ", "glyph_4  ", "explicit_id"
};

/* Distance values used for per-band preview sampling */
static const double MD_PREVIEW_DISTANCES[4] = { 2.0, 6.0, 9.0, 12.0 };

/* ===================================================================
 *  Filename helpers (same validation rules as decal designer)
 * =================================================================== */

/** Returns 1 if the basename only contains [A-Za-z0-9_-], non-empty. */
static int md_validate_basename(const char *name) {
    if (!name || name[0] == '\0') return 0;
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

/** Build a full "assets/materials/<basename>.txt" path into out. */
static void md_build_path(char *out, size_t out_sz, const char *basename) {
    snprintf(out, out_sz, "%s%s.txt", MD_MATERIALS_DIR, basename);
}

/* ===================================================================
 *  Private helpers
 * =================================================================== */

static void md_set_status(MaterialDesignerState *s, const char *msg) {
    strncpy(s->status_msg, msg, sizeof(s->status_msg) - 1);
    s->status_msg[sizeof(s->status_msg) - 1] = '\0';
    s->status_frames = MD_STATUS_FRAMES;
}

/* ===================================================================
 *  Palette cycling helpers
 * =================================================================== */

/** Find the next loaded palette ID in direction (+1/-1), wrapping in 1..255. */
static int md_next_palette_id(const AssetRegistry *assets, int current, int dir) {
    if (!assets) {
        int next = current + dir;
        if (next < 1)   next = 255;
        if (next > 255) next = 1;
        return next;
    }
    /* Cycle through palette IDs that have at least one non-zero colour channel.
     * Palette 0 is the always-black default — skip it. */
    int id = current;
    for (int steps = 0; steps < 256; steps++) {
        id += dir;
        if (id < 1)   id = 255;
        if (id > 255) id = 1;
        const Palette *p = &assets->palettes[id];
        if (p->near_color.r || p->near_color.g || p->near_color.b ||
            p->mid_color.r  || p->mid_color.g  || p->mid_color.b  ||
            p->far_color.r  || p->far_color.g  || p->far_color.b) {
            return id;
        }
    }
    /* No loaded palette found — stay put */
    return current;
}

/* ===================================================================
 *  Glyph cycling helpers
 * =================================================================== */

/** Cycle glyph through printable ASCII (33..126), direction +1/-1. */
static char md_next_glyph(char current, int dir) {
    int g = (unsigned char)current;
    if (g < 33 || g > 126) g = 35; /* '#' as fallback */
    g += dir;
    if (g < 33)  g = 126;
    if (g > 126) g = 33;
    return (char)g;
}

/* ===================================================================
 *  Save
 * =================================================================== */

static void md_save_named(MaterialDesignerState *s, const char *basename) {
    char path[256];
    md_build_path(path, sizeof(path), basename);

    FILE *f = fopen(path, "w");
    if (!f) {
        md_set_status(s, "Error: could not write file.");
        return;
    }

    /* palette=<id> */
    fprintf(f, "palette=%d\n", s->palette_id);

    /* glyphs=<4 chars> — space for any unset slot */
    char glyph_str[5];
    for (int i = 0; i < 4; i++) {
        char g = s->glyphs[i];
        glyph_str[i] = (g >= 33 && g <= 126) ? g : ' ';
    }
    glyph_str[4] = '\0';
    fprintf(f, "glyphs=%s\n", glyph_str);

    /* id=<n> only when explicit_id is set */
    if (s->explicit_id > 0) {
        fprintf(f, "id=%d\n", s->explicit_id);
    }

    fclose(f);

    strncpy(s->current_filename, basename, MD_FILENAME_MAX - 1);
    s->current_filename[MD_FILENAME_MAX - 1] = '\0';
    s->dirty = 0;

    char msg[128];
    snprintf(msg, sizeof(msg), "Saved: %s.txt", basename);
    md_set_status(s, msg);
}

/* ===================================================================
 *  Directory scan + sort for load selector
 * =================================================================== */

static int md_cmp_entry(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

static void md_scan_files(MaterialDesignerState *s) {
    s->file_count   = 0;
    s->file_sel_idx = 0;

    DIR *dir = opendir(MD_MATERIALS_DIR);
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && s->file_count < MD_MAX_LOAD_FILES) {
        const char *name = ent->d_name;
        size_t len = strlen(name);
        /* Must be at least "a.txt" (5 chars) and end with ".txt" */
        if (len < 5) continue;
        if (strcmp(name + len - 4, ".txt") != 0) continue;
        size_t base_len = len - 4;
        if (base_len == 0 || base_len >= MD_FILENAME_MAX) continue;
        char base[MD_FILENAME_MAX];
        memcpy(base, name, base_len);
        base[base_len] = '\0';
        if (!md_validate_basename(base)) continue;
        memcpy(s->file_list[s->file_count], base, base_len + 1);
        s->file_count++;
    }
    closedir(dir);

    if (s->file_count > 1) {
        qsort(s->file_list, (size_t)s->file_count,
              MD_FILENAME_MAX, md_cmp_entry);
    }
}

/* ===================================================================
 *  Load
 * =================================================================== */

static void md_load_named(MaterialDesignerState *s, const char *basename) {
    char path[256];
    md_build_path(path, sizeof(path), basename);

    FILE *f = fopen(path, "r");
    if (!f) {
        md_set_status(s, "Error: could not load file.");
        return;
    }

    int  palette_id  = 1;
    char glyphs[4]   = { '#', '#', '#', '#' };
    int  explicit_id = 0;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        /* Split on first '=' */
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;

        if (strcmp(key, "palette") == 0) {
            int v = atoi(val);
            if (v >= 1 && v <= 255) palette_id = v;
        } else if (strcmp(key, "glyphs") == 0) {
            for (int i = 0; i < 4; i++) {
                if (val[i] != '\0') {
                    glyphs[i] = val[i];
                } else {
                    glyphs[i] = ' ';
                }
            }
        } else if (strcmp(key, "id") == 0) {
            int v = atoi(val);
            if (v >= 1 && v <= 255) explicit_id = v;
        }
    }
    fclose(f);

    s->palette_id  = palette_id;
    for (int i = 0; i < 4; i++) s->glyphs[i] = glyphs[i];
    s->explicit_id = explicit_id;

    strncpy(s->current_filename, basename, MD_FILENAME_MAX - 1);
    s->current_filename[MD_FILENAME_MAX - 1] = '\0';
    s->dirty = 0;

    char msg[128];
    snprintf(msg, sizeof(msg), "Loaded: %s.txt", basename);
    md_set_status(s, msg);
}

/* ===================================================================
 *  Field adjustment (left/right arrows)
 * =================================================================== */

static void md_adjust_field(MaterialDesignerState *s, int direction,
                              const AssetRegistry *assets) {
    switch (s->metadata_row) {
        case 0: /* palette — cycle loaded palettes */
            s->palette_id = md_next_palette_id(assets, s->palette_id, direction);
            s->dirty = 1;
            break;
        case 1: s->glyphs[0] = md_next_glyph(s->glyphs[0], direction); s->dirty = 1; break;
        case 2: s->glyphs[1] = md_next_glyph(s->glyphs[1], direction); s->dirty = 1; break;
        case 3: s->glyphs[2] = md_next_glyph(s->glyphs[2], direction); s->dirty = 1; break;
        case 4: s->glyphs[3] = md_next_glyph(s->glyphs[3], direction); s->dirty = 1; break;
        case 5: { /* explicit_id — 0..255 */
            int v = s->explicit_id + direction;
            if (v < 0)   v = 0;
            if (v > 255) v = 255;
            s->explicit_id = v;
            s->dirty = 1;
            break;
        }
        default: break;
    }
}

/* ===================================================================
 *  Direct field text-edit overlay helpers
 * =================================================================== */

/** Snapshot current value into meta_prev_* before entering direct-edit. */
static void md_snapshot_field(MaterialDesignerState *s) {
    switch (s->metadata_row) {
        case 0: s->meta_prev_int   = s->palette_id;  break;
        case 1: s->meta_prev_glyph = s->glyphs[0];   break;
        case 2: s->meta_prev_glyph = s->glyphs[1];   break;
        case 3: s->meta_prev_glyph = s->glyphs[2];   break;
        case 4: s->meta_prev_glyph = s->glyphs[3];   break;
        case 5: s->meta_prev_int   = s->explicit_id; break;
        default: break;
    }
}

/** Pre-fill edit buffer with current value string. */
static void md_prefill_buffer(MaterialDesignerState *s) {
    switch (s->metadata_row) {
        case 0:
            snprintf(s->meta_edit_buffer, sizeof(s->meta_edit_buffer),
                     "%d", s->palette_id);
            break;
        case 1: s->meta_edit_buffer[0] = s->glyphs[0]; s->meta_edit_buffer[1] = '\0'; break;
        case 2: s->meta_edit_buffer[0] = s->glyphs[1]; s->meta_edit_buffer[1] = '\0'; break;
        case 3: s->meta_edit_buffer[0] = s->glyphs[2]; s->meta_edit_buffer[1] = '\0'; break;
        case 4: s->meta_edit_buffer[0] = s->glyphs[3]; s->meta_edit_buffer[1] = '\0'; break;
        case 5:
            if (s->explicit_id > 0) {
                snprintf(s->meta_edit_buffer, sizeof(s->meta_edit_buffer),
                         "%d", s->explicit_id);
            } else {
                s->meta_edit_buffer[0] = '\0';
            }
            break;
        default:
            s->meta_edit_buffer[0] = '\0';
            break;
    }
    s->meta_edit_len = (int)strlen(s->meta_edit_buffer);
}

/** Restore value from meta_prev_* when edit is cancelled or invalid. */
static void md_restore_field(MaterialDesignerState *s) {
    switch (s->metadata_row) {
        case 0: s->palette_id  = s->meta_prev_int;   break;
        case 1: s->glyphs[0]   = s->meta_prev_glyph; break;
        case 2: s->glyphs[1]   = s->meta_prev_glyph; break;
        case 3: s->glyphs[2]   = s->meta_prev_glyph; break;
        case 4: s->glyphs[3]   = s->meta_prev_glyph; break;
        case 5: s->explicit_id = s->meta_prev_int;   break;
        default: break;
    }
}

/* ===================================================================
 *  Sub-mode update handlers
 * =================================================================== */

/** Handle the inline direct-edit text overlay. */
static void md_update_meta_edit(MaterialDesignerState *s,
                                  const InputState *input) {
    /* Accumulate text */
    for (int i = 0; i < input->text_input_len; i++) {
        char c = input->text_input[i];
        if (s->meta_edit_len >= 31) break;

        /* Glyph fields: accept any single printable ASCII */
        if (s->metadata_row >= 1 && s->metadata_row <= 4) {
            if (c >= 33 && c <= 126) {
                s->meta_edit_buffer[0] = c;
                s->meta_edit_buffer[1] = '\0';
                s->meta_edit_len = 1;
            }
        } else {
            /* Integer fields (palette, explicit_id): digits only */
            if (c >= '0' && c <= '9') {
                s->meta_edit_buffer[s->meta_edit_len++] = c;
                s->meta_edit_buffer[s->meta_edit_len]   = '\0';
            }
        }
    }

    /* Backspace */
    if (input->erase && s->meta_edit_len > 0) {
        s->meta_edit_buffer[--s->meta_edit_len] = '\0';
    }

    /* Enter — commit */
    if (input->confirm) {
        int ok = 0;
        const char *buf = s->meta_edit_buffer;

        switch (s->metadata_row) {
            case 0: { /* palette: integer 1..255 */
                if (buf[0] != '\0') {
                    char *end;
                    long v = strtol(buf, &end, 10);
                    if (end != buf && *end == '\0' && v >= 1 && v <= 255) {
                        s->palette_id = (int)v;
                        ok = 1;
                        s->dirty = 1;
                    }
                }
                break;
            }
            case 1: case 2: case 3: case 4: { /* glyph: single printable ASCII */
                if (buf[0] != '\0' &&
                    (unsigned char)buf[0] >= 33 && (unsigned char)buf[0] <= 126) {
                    s->glyphs[s->metadata_row - 1] = buf[0];
                    ok = 1;
                    s->dirty = 1;
                }
                break;
            }
            case 5: { /* explicit_id: integer 0..255; empty = 0 (auto) */
                if (buf[0] == '\0') {
                    s->explicit_id = 0;
                    ok = 1;
                    s->dirty = 1;
                } else {
                    char *end;
                    long v = strtol(buf, &end, 10);
                    if (end != buf && *end == '\0' && v >= 0 && v <= 255) {
                        s->explicit_id = (int)v;
                        ok = 1;
                        s->dirty = 1;
                    }
                }
                break;
            }
            default: break;
        }

        if (!ok) {
            md_restore_field(s);
            md_set_status(s, "Invalid value.");
        }

        s->meta_edit_buffer[0] = '\0';
        s->meta_edit_len = 0;
        s->in_meta_edit = 0;
    }

    /* Esc — cancel */
    if (input->esc) {
        md_restore_field(s);
        s->meta_edit_buffer[0] = '\0';
        s->meta_edit_len = 0;
        s->in_meta_edit = 0;
    }
}

static MaterialDesignerResult md_update_edit(MaterialDesignerState *s,
                                              const InputState *input,
                                              const AssetRegistry *assets) {
    /* If direct-edit overlay is active, route input there */
    if (s->in_meta_edit) {
        md_update_meta_edit(s, input);
        return MD_RESULT_NONE;
    }

    /* Tab — toggle metadata focus */
    if (input->tab) {
        s->metadata_focus = !s->metadata_focus;
    }

    if (s->metadata_focus) {
        /* Up/down to change selected field */
        if (input->up   && s->metadata_row > 0)                     s->metadata_row--;
        if (input->down && s->metadata_row < MD_FIELD_COUNT - 1)    s->metadata_row++;

        /* Left/right to adjust current field */
        if (input->arrow_left)  md_adjust_field(s, -1, assets);
        if (input->arrow_right) md_adjust_field(s,  1, assets);

        /* Enter — enter direct-edit overlay for this field */
        if (input->confirm) {
            md_snapshot_field(s);
            md_prefill_buffer(s);
            s->in_meta_edit = 1;
        }
    }

    /* F5 — save to current filename, or open save prompt */
    if (input->save) {
        if (s->current_filename[0] != '\0') {
            md_save_named(s, s->current_filename);
        } else {
            s->filename_buffer[0] = '\0';
            s->filename_pos       = 0;
            s->mode = MD_SAVE_PROMPT;
        }
    }

    /* F10 — always open save prompt; pre-fill with current name */
    if (input->save_as) {
        strncpy(s->filename_buffer, s->current_filename, MD_FILENAME_MAX - 1);
        s->filename_buffer[MD_FILENAME_MAX - 1] = '\0';
        s->filename_pos = (int)strlen(s->filename_buffer);
        s->mode = MD_SAVE_PROMPT;
    }

    /* F9 — scan and open load selector */
    if (input->load) {
        md_scan_files(s);
        s->mode = MD_LOAD_SELECT;
    }

    /* ESC — exit clean or confirm discard */
    if (input->esc) {
        return s->dirty ? MD_RESULT_CONFIRM_DISCARD : MD_RESULT_EXIT;
    }

    return MD_RESULT_NONE;
}

static MaterialDesignerResult md_update_save_prompt(MaterialDesignerState *s,
                                                     const InputState *input) {
    /* Accumulate filename chars */
    for (int i = 0; i < input->text_input_len; i++) {
        char c = input->text_input[i];
        if (s->filename_pos >= MD_FILENAME_MAX - 1) break;
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '_' || c == '-') {
            s->filename_buffer[s->filename_pos++] = c;
            s->filename_buffer[s->filename_pos]   = '\0';
        }
    }

    /* Backspace */
    if (input->erase && s->filename_pos > 0) {
        s->filename_buffer[--s->filename_pos] = '\0';
    }

    /* Enter — validate and save */
    if (input->confirm) {
        if (!md_validate_basename(s->filename_buffer)) {
            md_set_status(s, "Invalid filename (use A-Za-z0-9_- only).");
        } else {
            md_save_named(s, s->filename_buffer);
            s->filename_buffer[0] = '\0';
            s->filename_pos = 0;
            s->mode = MD_EDIT;
        }
    }

    /* ESC — cancel */
    if (input->esc) {
        s->filename_buffer[0] = '\0';
        s->filename_pos = 0;
        s->mode = MD_EDIT;
    }

    return MD_RESULT_NONE;
}

static MaterialDesignerResult md_update_load_select(MaterialDesignerState *s,
                                                     const InputState *input) {
    if (s->file_count > 0) {
        if (input->up   && s->file_sel_idx > 0)                s->file_sel_idx--;
        if (input->down && s->file_sel_idx < s->file_count - 1) s->file_sel_idx++;
    }

    if (input->confirm && s->file_count > 0) {
        md_load_named(s, s->file_list[s->file_sel_idx]);
        s->mode = MD_EDIT;
    }

    if (input->esc) {
        s->mode = MD_EDIT;
    }

    return MD_RESULT_NONE;
}

/* ===================================================================
 *  Public API — init / destroy / update
 * =================================================================== */

void material_designer_init(MaterialDesignerState *s,
                             const EngineConfig *cfg,
                             AppState return_state,
                             const AssetRegistry *assets) {
    /* cfg unused currently — kept for future expansion */
    (void)cfg;
    (void)assets;

    memset(s, 0, sizeof(*s));

    s->return_state = return_state;
    s->palette_id   = 1;
    s->glyphs[0]    = '#';
    s->glyphs[1]    = '#';
    s->glyphs[2]    = '#';
    s->glyphs[3]    = '#';
    s->explicit_id  = 0;
    s->dirty        = 1;   /* blank material is unsaved */
    s->mode         = MD_EDIT;
    s->metadata_focus = 0;
    s->metadata_row   = 0;
    s->in_meta_edit   = 0;
}

void material_designer_destroy(MaterialDesignerState *s) {
    /* No heap allocations — no-op provided for lifecycle symmetry. */
    (void)s;
}

MaterialDesignerResult material_designer_update(MaterialDesignerState *s,
                                                 const InputState *input,
                                                 const AssetRegistry *assets) {
    if (!s || !input) return MD_RESULT_NONE;

    MaterialDesignerResult result = MD_RESULT_NONE;

    switch (s->mode) {
        case MD_EDIT:
            result = md_update_edit(s, input, assets);
            break;
        case MD_SAVE_PROMPT:
            result = md_update_save_prompt(s, input);
            break;
        case MD_LOAD_SELECT:
            result = md_update_load_select(s, input);
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

/* ===================================================================
 *  Render helpers
 * =================================================================== */

/** Fill a rectangular area with space characters and given colours. */
static void md_fill_box(Grid *grid, int x, int y, int w, int h,
                         SDL_Color fg, SDL_Color bg) {
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            if (x + c < grid->width && y + r < grid->height)
                grid_set(grid, x + c, y + r, ' ', fg, bg);
        }
    }
}

/** Draw a '+', '-', '|' border around a rectangle. */
static void md_draw_border(Grid *grid, int x, int y, int w, int h,
                            SDL_Color fg, SDL_Color bg) {
    if (x < grid->width && y < grid->height)
        grid_set(grid, x, y, '+', fg, bg);
    if (x + w - 1 < grid->width && y < grid->height)
        grid_set(grid, x + w - 1, y, '+', fg, bg);
    if (x < grid->width && y + h - 1 < grid->height)
        grid_set(grid, x, y + h - 1, '+', fg, bg);
    if (x + w - 1 < grid->width && y + h - 1 < grid->height)
        grid_set(grid, x + w - 1, y + h - 1, '+', fg, bg);

    for (int c = 1; c < w - 1; c++) {
        if (x + c < grid->width) {
            if (y < grid->height)
                grid_set(grid, x + c, y, '-', fg, bg);
            if (y + h - 1 < grid->height)
                grid_set(grid, x + c, y + h - 1, '-', fg, bg);
        }
    }
    for (int r = 1; r < h - 1; r++) {
        if (y + r < grid->height) {
            if (x < grid->width)
                grid_set(grid, x, y + r, '|', fg, bg);
            if (x + w - 1 < grid->width)
                grid_set(grid, x + w - 1, y + r, '|', fg, bg);
        }
    }
}

/** Render the preview pane: 4 glyphs with palette colours per distance band. */
static void md_render_preview(const MaterialDesignerState *s, Grid *grid,
                               const AssetRegistry *assets) {
    SDL_Color bg_black = {0, 0, 0, 255};
    SDL_Color fg_white = {255, 255, 255, 255};
    SDL_Color fg_gray  = {120, 120, 120, 255};

    int px = MD_PREVIEW_X;
    int py = MD_PREVIEW_Y;

    if (px >= grid->width || py >= grid->height) return;

    grid_print(grid, px, py, "[Preview]", fg_white, bg_black);
    py++;

    /* Band labels */
    const char * const band_labels[4] = { "near", "mid ", "far ", "vfar" };

    const Palette *pal = assets ? &assets->palettes[s->palette_id] : NULL;

    for (int g = 0; g < 4; g++) {
        char glyph = s->glyphs[g];
        if (glyph < 33 || glyph > 126) glyph = ' ';

        SDL_Color glyph_fg;
        if (pal) {
            glyph_fg = palette_sample(pal, MD_PREVIEW_DISTANCES[g], 1.0);
        } else {
            glyph_fg = fg_white;
        }

        char label[32];
        snprintf(label, sizeof(label), "  %s  ", band_labels[g]);
        if (px < grid->width && py + g < grid->height) {
            grid_print(grid, px, py + g, label, fg_gray, bg_black);
            /* Glyph rendered after the label */
            if (px + 7 < grid->width)
                grid_set(grid, px + 7, py + g, (uint8_t)glyph, glyph_fg, bg_black);
        }
    }
}

/* ===================================================================
 *  Public API — render
 * =================================================================== */

void material_designer_render(const MaterialDesignerState *s, Grid *grid,
                               const AssetRegistry *assets) {
    if (!s || !grid) return;

    SDL_Color bg_black  = {  0,   0,   0, 255};
    SDL_Color fg_white  = {255, 255, 255, 255};
    SDL_Color fg_gray   = {150, 150, 150, 255};
    SDL_Color fg_yellow = {255, 255,   0, 255};
    SDL_Color fg_red    = {255,  80,  80, 255};
    SDL_Color fg_cyan   = {  0, 220, 220, 255};
    SDL_Color bg_sel    = { 50, 100,  50, 255};

    grid_clear(grid, bg_black);

    /* ---- Title bar ---- */
    const char *fname = s->current_filename[0] ? s->current_filename : "<unsaved>";
    char title[128];
    snprintf(title, sizeof(title),
             s->dirty ? "=== MATERIAL DESIGNER [*] File: %s ===" :
                        "=== MATERIAL DESIGNER     File: %s ===",
             fname);
    grid_print(grid, MD_PANEL_X, 0, title,
               s->dirty ? fg_yellow : fg_white, bg_black);

    /* ---- Key hints (edit mode only) ---- */
    if (s->mode == MD_EDIT && !s->in_meta_edit) {
        grid_print(grid, MD_PANEL_X, 1,
                   "Tab:Focus  L/R:Adj  U/D:Sel  Enter:Edit"
                   "  F5:Save  F9:Load  F10:SaveAs  Esc:Exit",
                   fg_gray, bg_black);
    }

    /* ---- Field list panel ---- */
    int panel_x = MD_PANEL_X;
    int panel_y = MD_PANEL_Y;

    /* Panel header */
    grid_print(grid, panel_x, panel_y,
               s->metadata_focus ? "[Fields*]" : "[Fields ]",
               s->metadata_focus ? fg_yellow : fg_cyan, bg_black);
    panel_y++;

    for (int r = 0; r < MD_FIELD_COUNT; r++) {
        bool sel = (s->metadata_focus && r == s->metadata_row);
        SDL_Color mfg = sel ? fg_yellow : fg_white;
        SDL_Color mbg = sel ? bg_sel    : bg_black;
        char val[24];

        switch (r) {
            case 0: snprintf(val, sizeof(val), "%d", s->palette_id);  break;
            case 1: val[0] = s->glyphs[0]; val[1] = '\0'; break;
            case 2: val[0] = s->glyphs[1]; val[1] = '\0'; break;
            case 3: val[0] = s->glyphs[2]; val[1] = '\0'; break;
            case 4: val[0] = s->glyphs[3]; val[1] = '\0'; break;
            case 5:
                if (s->explicit_id > 0) {
                    snprintf(val, sizeof(val), "%d", s->explicit_id);
                } else {
                    strncpy(val, "(auto)", sizeof(val) - 1);
                    val[sizeof(val) - 1] = '\0';
                }
                break;
            default: val[0] = '\0'; break;
        }

        char line[64];
        snprintf(line, sizeof(line), "%s%s %s",
                 sel ? ">" : " ", MD_FIELD_LABELS[r], val);
        if (panel_x < grid->width && panel_y + r < grid->height)
            grid_print(grid, panel_x, panel_y + r, line, mfg, mbg);
    }

    /* Field editing hint */
    int hint_y = panel_y + MD_FIELD_COUNT + 1;
    if (panel_x < grid->width && hint_y < grid->height) {
        grid_print(grid, panel_x, hint_y,
                   "Tab:focus  L/R:adj  U/D:sel  Enter:edit",
                   fg_gray, bg_black);
    }

    /* ---- Preview pane ---- */
    md_render_preview(s, grid, assets);

    /* ---- Status message ---- */
    int status_y = MD_PANEL_Y + MD_FIELD_COUNT + 3;
    if (status_y < grid->height && s->status_frames > 0 && s->status_msg[0]) {
        grid_print(grid, panel_x, status_y, s->status_msg, fg_red, bg_black);
    }

    /* ---- Direct-edit overlay ---- */
    if (s->in_meta_edit) {
        static const char * const edit_labels[MD_FIELD_COUNT] = {
            "palette", "glyph_1", "glyph_2", "glyph_3", "glyph_4", "explicit_id"
        };
        int oy = MD_PANEL_Y + MD_FIELD_COUNT + 3;
        if (oy < grid->height)
            grid_print(grid, panel_x, oy, "[ Edit Field ]", fg_cyan, bg_black);
        oy++;
        if (s->metadata_row < MD_FIELD_COUNT && oy < grid->height) {
            char label_line[48];
            snprintf(label_line, sizeof(label_line), "  %s:",
                     edit_labels[s->metadata_row]);
            grid_print(grid, panel_x, oy, label_line, fg_white, bg_black);
        }
        oy++;
        if (oy < grid->height) {
            char buf_line[48];
            snprintf(buf_line, sizeof(buf_line), "  > %s_",
                     s->meta_edit_buffer);
            grid_print(grid, panel_x, oy, buf_line, fg_yellow, bg_black);
        }
        oy++;
        if (oy < grid->height) {
            grid_print(grid, panel_x, oy,
                       "  Enter:commit  Esc:cancel  Backspace:delete",
                       fg_gray, bg_black);
        }
    }

    /* ---- Save-prompt overlay ---- */
    if (s->mode == MD_SAVE_PROMPT) {
        int bx = 20, by = 10, bw = 41, bh = 8;
        SDL_Color box_bg  = {  0,   0,  30, 255};
        SDL_Color box_bdr = {  0, 180, 220, 255};
        md_fill_box(grid, bx, by, bw, bh, fg_white, box_bg);
        md_draw_border(grid, bx, by, bw, bh, box_bdr, box_bg);
        grid_print(grid, bx + 2, by + 1, "[ Save As ]", fg_cyan, box_bg);
        char save_name_buf[MD_FILENAME_MAX + 16];
        snprintf(save_name_buf, sizeof(save_name_buf),
                 "Name: %s_", s->filename_buffer);
        grid_print(grid, bx + 2, by + 3, save_name_buf, fg_white, box_bg);
        if (s->status_frames > 0 && s->status_msg[0])
            grid_print(grid, bx + 2, by + 5, s->status_msg, fg_red, box_bg);
        grid_print(grid, bx + 2, by + 6,
                   "Enter=save  Esc=cancel", fg_gray, box_bg);
    }

    /* ---- Load-select overlay ---- */
    if (s->mode == MD_LOAD_SELECT) {
        int bx = 20, by = 8, bw = 43, bh = 15;
        SDL_Color box_bg  = {  0,  20,   0, 255};
        SDL_Color box_bdr = { 50, 200,  50, 255};
        md_fill_box(grid, bx, by, bw, bh, fg_white, box_bg);
        md_draw_border(grid, bx, by, bw, bh, box_bdr, box_bg);
        grid_print(grid, bx + 2, by + 1, "[ Load Material ]", fg_cyan, box_bg);

        if (s->file_count == 0) {
            grid_print(grid, bx + 2, by + 3,
                       "(no material files found)", fg_gray, box_bg);
        } else {
            int start = s->file_sel_idx - 4;
            if (start < 0) start = 0;
            int end   = start + 10;
            if (end > s->file_count) end = s->file_count;

            for (int i = start; i < end; i++) {
                int list_row = by + 3 + (i - start);
                if (list_row >= by + bh - 2) break;
                bool sel = (i == s->file_sel_idx);
                SDL_Color lfg = sel ? bg_black : fg_white;
                SDL_Color lbg = sel ? bg_sel   : box_bg;
                char entry[72];
                snprintf(entry, sizeof(entry), "%s%s.txt",
                         sel ? "> " : "  ", s->file_list[i]);
                grid_print(grid, bx + 2, list_row, entry, lfg, lbg);
            }
        }
        grid_print(grid, bx + 2, by + bh - 2,
                   "Enter=load  Esc=cancel", fg_gray, box_bg);
    }
}
