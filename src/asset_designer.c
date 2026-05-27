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

/* Editor surface names indexed by AdSurface enum */
static const char * const AD_SURFACE_NAMES[AD_SURFACE_COUNT] = {
    "wall", "floor", "ceil", "all"
};

/* Concrete surface mapping for save/export — do NOT include AD_SURFACE_ALL here.
 * To add a new concrete surface, add one entry here and one AdSurface enum value. */
#define AD_CONCRETE_SURFACE_COUNT 3
typedef struct {
    AdSurface    editor;
    DecalSurface decal;
    const char  *name;   /* suffix used in All-export filenames */
} AdConcreteSurface;
static const AdConcreteSurface AD_CONCRETE_SURFACES[AD_CONCRETE_SURFACE_COUNT] = {
    { AD_SURFACE_WALL,    DECAL_SURFACE_WALL,    "wall"  },
    { AD_SURFACE_FLOOR,   DECAL_SURFACE_FLOOR,   "floor" },
    { AD_SURFACE_CEILING, DECAL_SURFACE_CEILING, "ceil"  },
};

/* ===================================================================
 *  Save
 * =================================================================== */

static void ad_save_named(AssetDesignerState *s, const char *basename) {
    if (s->editor_surface == AD_SURFACE_ALL) {
        /* Export one file per concrete surface.
         * Use a shallow copy so s->decal.surface is never mutated. */
        int any_error = 0;
        for (int i = 0; i < AD_CONCRETE_SURFACE_COUNT; i++) {
            Decal tmp = s->decal;           /* shallow copy — pattern ptr shared, read-only */
            tmp.surface = AD_CONCRETE_SURFACES[i].decal;
            char expanded[AD_FILENAME_MAX + 8];
            snprintf(expanded, sizeof(expanded), "%s_%s",
                     basename, AD_CONCRETE_SURFACES[i].name);
            char path[160];
            ad_build_path(path, sizeof(path), expanded);
            if (decal_save_to_file(path, &tmp) != 0) {
                any_error = 1;
            }
        }
        if (!any_error) {
            strncpy(s->current_filename, basename, AD_FILENAME_MAX - 1);
            s->current_filename[AD_FILENAME_MAX - 1] = '\0';
            s->dirty = 0;
            char msg[128];
            snprintf(msg, sizeof(msg), "Saved All: %s_[wall/floor/ceil].txt", basename);
            ad_set_status(s, msg);
        } else {
            ad_set_status(s, "Error: one or more files could not be written.");
        }
    } else {
        /* Save a single concrete surface — use a shallow copy */
        Decal tmp = s->decal;
        tmp.surface = AD_CONCRETE_SURFACES[(int)s->editor_surface].decal;
        char path[128];
        ad_build_path(path, sizeof(path), basename);
        int ret = decal_save_to_file(path, &tmp);
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

    /* Map loaded concrete surface to editor_surface */
    switch (s->decal.surface) {
        case DECAL_SURFACE_WALL:    s->editor_surface = AD_SURFACE_WALL;    break;
        case DECAL_SURFACE_FLOOR:   s->editor_surface = AD_SURFACE_FLOOR;   break;
        case DECAL_SURFACE_CEILING: s->editor_surface = AD_SURFACE_CEILING; break;
        default:                    s->editor_surface = AD_SURFACE_WALL;    break;
    }

    strncpy(s->current_filename, basename, AD_FILENAME_MAX - 1);
    s->current_filename[AD_FILENAME_MAX - 1] = '\0';
    s->dirty = 0;

    char msg[128];
    snprintf(msg, sizeof(msg), "Loaded: %s.txt", basename);
    ad_set_status(s, msg);
}

/* ===================================================================
 *  Metadata adjustment
 * =================================================================== */


static void ad_resize_pattern(AssetDesignerState *s, int new_cols, int new_rows) {
    /* Clamp to valid canvas bounds */
    if (new_cols < 1) new_cols = 1;
    if (new_cols > AD_MAX_CANVAS_COLS) new_cols = AD_MAX_CANVAS_COLS;
    if (new_rows < 1) new_rows = 1;
    if (new_rows > AD_MAX_CANVAS_ROWS) new_rows = AD_MAX_CANVAS_ROWS;

    int old_cols = s->canvas_cols;
    int old_rows = s->canvas_rows;
    if (new_cols == old_cols && new_rows == old_rows) return;  /* no-op */

    PatternCell *new_pat = (PatternCell *)calloc(
        (size_t)(new_cols * new_rows), sizeof(PatternCell));
    if (!new_pat) return;  /* allocation failure: leave state unchanged */

    /* Copy the overlapping region from the old pattern */
    int copy_cols = old_cols < new_cols ? old_cols : new_cols;
    int copy_rows = old_rows < new_rows ? old_rows : new_rows;
    for (int r = 0; r < copy_rows; r++) {
        for (int c = 0; c < copy_cols; c++) {
            new_pat[r * new_cols + c] = s->decal.pattern[r * old_cols + c];
        }
    }

    /* Fill expanded cells (not in the overlap) with space + current material */
    for (int r = 0; r < new_rows; r++) {
        for (int c = 0; c < new_cols; c++) {
            if (r >= copy_rows || c >= copy_cols) {
                new_pat[r * new_cols + c].glyph = ' ';
                new_pat[r * new_cols + c].material_id =
                    (uint8_t)s->current_material_id;
            }
        }
    }

    free(s->decal.pattern);
    s->decal.pattern       = new_pat;
    s->decal.pattern_cols  = new_cols;
    s->decal.pattern_rows  = new_rows;
    s->canvas_cols         = new_cols;
    s->canvas_rows         = new_rows;

    /* Clamp cursor into the new bounds */
    if (s->cursor_col >= new_cols) s->cursor_col = new_cols - 1;
    if (s->cursor_row >= new_rows) s->cursor_row = new_rows - 1;

    s->dirty = 1;
}


/* Returns the direction from a single-frame edge-triggered navigation input.
 * 0=up, 1=down, 2=left, 3=right; returns -1 if no direction key was pressed. */
static int ad_dir_from_edge_input(const InputState *input) {
    if (input->up)          return 0;
    if (input->down)        return 1;
    if (input->arrow_left)  return 2;
    if (input->arrow_right) return 3;
    return -1;
}

/* Returns true if the given direction is currently physically held. */
static bool ad_dir_is_held(const InputState *input, int dir) {
    switch (dir) {
        case 0: return input->held_up;
        case 1: return input->held_down;
        case 2: return input->held_arrow_left;
        case 3: return input->held_arrow_right;
        default: return false;
    }
}

/* Moves the cursor one cell in the given direction.
 * Returns true if movement actually occurred (i.e. not already at edge). */
static bool ad_move_cursor_dir(AssetDesignerState *s, int dir) {
    switch (dir) {
        case 0:
            if (s->cursor_row > 0) { s->cursor_row--; return true; }
            break;
        case 1:
            if (s->cursor_row < s->canvas_rows - 1) { s->cursor_row++; return true; }
            break;
        case 2:
            if (s->cursor_col > 0) { s->cursor_col--; return true; }
            break;
        case 3:
            if (s->cursor_col < s->canvas_cols - 1) { s->cursor_col++; return true; }
            break;
        default:
            break;
    }
    return false;
}


/* Returns true if glyph c is allowed by the selected material.
 * If assets == NULL, all printable glyphs are allowed. */
static bool ad_glyph_allowed(const AssetRegistry *assets,
                               int material_id, char c) {
    if (assets == NULL) return true;
    const Material *mat = &assets->materials[material_id];
    for (int g = 0; g < 4; g++) {
        if ((char)mat->glyphs[g] == c) return true;
    }
    return false;
}

/* Returns the first allowed glyph for the material, or fallback if none. */
static char ad_first_allowed_glyph(const AssetRegistry *assets,
                                     int material_id, char fallback) {
    if (assets == NULL) return fallback;
    const Material *mat = &assets->materials[material_id];
    for (int g = 0; g < 4; g++) {
        if (mat->glyphs[g] && (char)mat->glyphs[g] != ' ') {
            return (char)mat->glyphs[g];
        }
    }
    return fallback;
}

/* Resets current_glyph to the first allowed material glyph if the current
 * glyph is no longer allowed by the selected material. */
static void ad_normalize_glyph(AssetDesignerState *s,
                                 const AssetRegistry *assets) {
    if (assets == NULL) return;
    if (!ad_glyph_allowed(assets, s->current_material_id, s->current_glyph)) {
        s->current_glyph = ad_first_allowed_glyph(
            assets, s->current_material_id, '#');
    }
}

/**
 * ad_adjust_metadata() — Adjust the selected metadata field by direction
 *
 * direction: +1 to increase/next, -1 to decrease/prev.
 * Only s->decal fields (rows 0-4) set dirty=1; current_material_id (row 5) does not,
 * because it is editor-only state and does not affect previously placed cells.
 */
static void ad_adjust_metadata(AssetDesignerState *s, int direction,
                                const AssetRegistry *assets) {
    switch (s->metadata_row) {
        case 0: /* surface: cycle wall / floor / ceil / all */
            s->editor_surface = (AdSurface)(
                ((int)s->editor_surface + (int)AD_SURFACE_COUNT + direction)
                % (int)AD_SURFACE_COUNT);
            s->dirty = 1;
            break;
        case 1: /* width — step 0.25, min 0.25 */
            s->decal.width += direction * 0.25;
            if (s->decal.width < 0.25) s->decal.width = 0.25;
            s->dirty = 1;
            break;
        case 2: /* height — step 0.25, min 0.25 */
            s->decal.height += direction * 0.25;
            if (s->decal.height < 0.25) s->decal.height = 0.25;
            s->dirty = 1;
            break;
        case 3: /* glyph_step_u — step 0.0625, min 0.0 */
            s->decal.glyph_step_u += direction * 0.0625;
            if (s->decal.glyph_step_u < 0.0) s->decal.glyph_step_u = 0.0;
            s->dirty = 1;
            break;
        case 4: /* glyph_step_v — step 0.0625, min 0.0 */
            s->decal.glyph_step_v += direction * 0.0625;
            if (s->decal.glyph_step_v < 0.0) s->decal.glyph_step_v = 0.0;
            s->dirty = 1;
            break;
        case 5: { /* current_material_id — editor state only, no dirty change.
                   * Cycle only through loaded IDs; skip unloaded slots. */
            if (assets) {
                int id = s->current_material_id;
                int steps = 0;
                do {
                    id += direction;
                    if (id < 1)   id = 255;
                    if (id > 255) id = 1;
                    steps++;
                } while (!material_id_is_loaded(assets, id) && steps < 256);
                if (material_id_is_loaded(assets, id)) {
                    s->current_material_id = id;
                    ad_normalize_glyph(s, assets);
                }
            } else {
                s->current_material_id += direction;
                if (s->current_material_id < 1)   s->current_material_id = 1;
                if (s->current_material_id > 255)  s->current_material_id = 255;
            }
            break;
        }
        case 6: { /* pattern_cols: clamp to [1, AD_MAX_CANVAS_COLS] */
            int nc = s->canvas_cols + direction;
            ad_resize_pattern(s, nc, s->canvas_rows);
            break;
        }
        case 7: { /* pattern_rows: clamp to [1, AD_MAX_CANVAS_ROWS] */
            int nr = s->canvas_rows + direction;
            ad_resize_pattern(s, s->canvas_cols, nr);
            break;
        }
        default:
            break;
    }
}

/* ===================================================================
 *  Sub-mode update handlers
 * =================================================================== */

static AssetDesignerResult ad_update_meta_edit(AssetDesignerState *s,
                                                const InputState *input,
                                                const AssetRegistry *assets) {
    /* Append valid chars from text input.
     * Accept digits, minus, dot (for floats/ints), lowercase letters (for surface names). */
    for (int i = 0; i < input->text_input_len; i++) {
        char c = input->text_input[i];
        if (s->meta_edit_len >= 31) break;
        if ((c >= '0' && c <= '9') ||
            c == '-' || c == '.' ||
            (c >= 'a' && c <= 'z')) {
            s->meta_edit_buffer[s->meta_edit_len++] = c;
            s->meta_edit_buffer[s->meta_edit_len]   = '\0';
        }
    }

    /* Backspace — delete last char */
    if (input->erase && s->meta_edit_len > 0) {
        s->meta_edit_buffer[--s->meta_edit_len] = '\0';
    }

    /* Enter — parse, validate, commit or reject */
    if (input->confirm) {
        int ok = 0;
        const char *buf = s->meta_edit_buffer;

        switch (s->metadata_row) {
            case 0: { /* surface: "wall", "floor", "ceil", "all" */
                if (strcmp(buf, "wall")  == 0) { s->editor_surface = AD_SURFACE_WALL;    ok = 1; }
                else if (strcmp(buf, "floor") == 0) { s->editor_surface = AD_SURFACE_FLOOR;   ok = 1; }
                else if (strcmp(buf, "ceil")  == 0) { s->editor_surface = AD_SURFACE_CEILING; ok = 1; }
                else if (strcmp(buf, "all")   == 0) { s->editor_surface = AD_SURFACE_ALL;     ok = 1; }
                if (ok) s->dirty = 1;
                break;
            }
            case 1: { /* width: positive float */
                if (buf[0] != '\0') {
                    char *end;
                    double v = strtod(buf, &end);
                    if (end != buf && *end == '\0' && v > 0.0) {
                        s->decal.width = v;
                        ok = 1;
                        s->dirty = 1;
                    }
                }
                break;
            }
            case 2: { /* height: positive float */
                if (buf[0] != '\0') {
                    char *end;
                    double v = strtod(buf, &end);
                    if (end != buf && *end == '\0' && v > 0.0) {
                        s->decal.height = v;
                        ok = 1;
                        s->dirty = 1;
                    }
                }
                break;
            }
            case 3: { /* glyph_step_u: nonnegative float */
                if (buf[0] != '\0') {
                    char *end;
                    double v = strtod(buf, &end);
                    if (end != buf && *end == '\0' && v >= 0.0) {
                        s->decal.glyph_step_u = v;
                        ok = 1;
                        s->dirty = 1;
                    }
                }
                break;
            }
            case 4: { /* glyph_step_v: nonnegative float */
                if (buf[0] != '\0') {
                    char *end;
                    double v = strtod(buf, &end);
                    if (end != buf && *end == '\0' && v >= 0.0) {
                        s->decal.glyph_step_v = v;
                        ok = 1;
                        s->dirty = 1;
                    }
                }
                break;
            }
            case 5: { /* material_id: integer 1..255, or material name */
                if (buf[0] != '\0') {
                    char *end;
                    long v = strtol(buf, &end, 10);
                    if (end != buf && *end == '\0' && v >= 1 && v <= 255) {
                        /* Accepted as a numeric ID */
                        s->current_material_id = (int)v;
                        ok = 1;
                        ad_normalize_glyph(s, assets);
                        /* Does NOT set dirty: current_material_id is editor-only state */
                    } else if (assets) {
                        /* Fall back to name lookup */
                        int found = material_find_by_name(assets, buf);
                        if (found >= 1 && found <= 255) {
                            s->current_material_id = found;
                            ok = 1;
                            ad_normalize_glyph(s, assets);
                        }
                    }
                }
                break;
            }
            case 6: { /* pattern_cols: integer 1..AD_MAX_CANVAS_COLS */
                if (buf[0] != '\0') {
                    char *end;
                    long v = strtol(buf, &end, 10);
                    if (end != buf && *end == '\0' &&
                        v >= 1 && v <= AD_MAX_CANVAS_COLS) {
                        ad_resize_pattern(s, (int)v, s->canvas_rows);
                        ok = 1;
                        /* dirty is set by ad_resize_pattern if dims changed */
                    }
                }
                break;
            }
            case 7: { /* pattern_rows: integer 1..AD_MAX_CANVAS_ROWS */
                if (buf[0] != '\0') {
                    char *end;
                    long v = strtol(buf, &end, 10);
                    if (end != buf && *end == '\0' &&
                        v >= 1 && v <= AD_MAX_CANVAS_ROWS) {
                        ad_resize_pattern(s, s->canvas_cols, (int)v);
                        ok = 1;
                    }
                }
                break;
            }
            default:
                break;
        }

        if (!ok) {
            /* Restore snapshot — field was never modified during buffer edit,
             * but restore explicitly for clarity. */
            switch (s->metadata_row) {
                case 0: s->editor_surface      = s->meta_prev_surface; break;
                case 1: s->decal.width         = s->meta_prev_double;  break;
                case 2: s->decal.height        = s->meta_prev_double;  break;
                case 3: s->decal.glyph_step_u  = s->meta_prev_double;  break;
                case 4: s->decal.glyph_step_v  = s->meta_prev_double;  break;
                case 5: s->current_material_id = s->meta_prev_int;     break;
                case 6: /* cols: ad_resize_pattern not called — no state to restore */ break;
                case 7: /* rows: ad_resize_pattern not called — no state to restore */ break;
                default: break;
            }
            ad_set_status(s, "Invalid value.");
        }

        s->meta_edit_buffer[0] = '\0';
        s->meta_edit_len = 0;
        s->mode = AD_DECAL_EDIT;
    }

    /* Esc — cancel and restore snapshot */
    if (input->esc) {
        switch (s->metadata_row) {
            case 0: s->editor_surface      = s->meta_prev_surface; break;
            case 1: s->decal.width         = s->meta_prev_double;  break;
            case 2: s->decal.height        = s->meta_prev_double;  break;
            case 3: s->decal.glyph_step_u  = s->meta_prev_double;  break;
            case 4: s->decal.glyph_step_v  = s->meta_prev_double;  break;
            case 5: s->current_material_id = s->meta_prev_int;     break;
            case 6: /* no state was mutated during edit */ break;
            case 7: /* no state was mutated during edit */ break;
            default: break;
        }
        s->meta_edit_buffer[0] = '\0';
        s->meta_edit_len = 0;
        s->mode = AD_DECAL_EDIT;
    }

    return AD_RESULT_NONE;
}

static AssetDesignerResult ad_update_edit(AssetDesignerState *s,
                                           const InputState *input,
                                           const AssetRegistry *assets) {
    /* Tab — toggle focus between canvas and metadata panel */
    if (input->tab) {
        s->metadata_focus = !s->metadata_focus;
    }

    if (s->metadata_focus) {
        /* ---- Metadata panel input ---- */
        if (input->up   && s->metadata_row > 0)                      s->metadata_row--;
        if (input->down && s->metadata_row < AD_META_EDIT_COUNT - 1) s->metadata_row++;
        if (input->arrow_left)  ad_adjust_metadata(s, -1, assets);
        if (input->arrow_right) ad_adjust_metadata(s,  1, assets);

        /* Enter on a metadata row — snapshot current value and enter direct-edit */
        if (input->confirm) {
            switch (s->metadata_row) {
                case 0: s->meta_prev_surface = s->editor_surface;        break;
                case 1: s->meta_prev_double  = s->decal.width;           break;
                case 2: s->meta_prev_double  = s->decal.height;          break;
                case 3: s->meta_prev_double  = s->decal.glyph_step_u;    break;
                case 4: s->meta_prev_double  = s->decal.glyph_step_v;    break;
                case 5: s->meta_prev_int     = s->current_material_id;   break;
                case 6: s->meta_prev_int     = s->canvas_cols;           break;
                case 7: s->meta_prev_int     = s->canvas_rows;           break;
                default: break;
            }
            /* Pre-fill buffer with current value */
            switch (s->metadata_row) {
                case 0:
                    snprintf(s->meta_edit_buffer, sizeof(s->meta_edit_buffer),
                             "%s", AD_SURFACE_NAMES[(int)s->editor_surface]);
                    break;
                case 1:
                    snprintf(s->meta_edit_buffer, sizeof(s->meta_edit_buffer),
                             "%.4f", s->decal.width);
                    break;
                case 2:
                    snprintf(s->meta_edit_buffer, sizeof(s->meta_edit_buffer),
                             "%.4f", s->decal.height);
                    break;
                case 3:
                    snprintf(s->meta_edit_buffer, sizeof(s->meta_edit_buffer),
                             "%.4f", s->decal.glyph_step_u);
                    break;
                case 4:
                    snprintf(s->meta_edit_buffer, sizeof(s->meta_edit_buffer),
                             "%.4f", s->decal.glyph_step_v);
                    break;
                case 5:
                    snprintf(s->meta_edit_buffer, sizeof(s->meta_edit_buffer),
                             "%d", s->current_material_id);
                    break;
                case 6:
                    snprintf(s->meta_edit_buffer, sizeof(s->meta_edit_buffer),
                             "%d", s->canvas_cols);
                    break;
                case 7:
                    snprintf(s->meta_edit_buffer, sizeof(s->meta_edit_buffer),
                             "%d", s->canvas_rows);
                    break;
                default:
                    s->meta_edit_buffer[0] = '\0';
                    break;
            }
            s->meta_edit_len = (int)strlen(s->meta_edit_buffer);
            s->mode = AD_META_EDIT;
        }
    } else {
        /* ---- Canvas input ---- */
        /* Cursor movement with held-key auto-repeat.
         *
         * A: Edge trigger → move once immediately, arm the repeat timer
         * B: Else if last direction is still held → decrement timer; move on expiry
         * C: Else → disarm (no key held in the last direction)
         */
        bool moved = false;
        int edge_dir = ad_dir_from_edge_input(input);
        if (edge_dir >= 0) {
            /* A: fresh press — move now and start repeat countdown */
            moved = ad_move_cursor_dir(s, edge_dir);
            s->repeat_dir   = edge_dir;
            s->repeat_timer = AD_REPEAT_DELAY;
        } else if (s->repeat_dir >= 0 && ad_dir_is_held(input, s->repeat_dir)) {
            /* B: still holding last direction — count down and fire when ready */
            if (s->repeat_timer > 0) {
                s->repeat_timer--;
            }
            if (s->repeat_timer == 0) {
                moved = ad_move_cursor_dir(s, s->repeat_dir);
                s->repeat_timer = AD_REPEAT_RATE;
            }
        } else {
            /* C: no relevant key held — disarm */
            s->repeat_dir   = -1;
            s->repeat_timer = 0;
        }

        /* Paint/erase-while-moving: apply to the newly entered cell */
        if (moved) {
            int idx = s->cursor_row * s->canvas_cols + s->cursor_col;
            if (input->held_place) {
                s->decal.pattern[idx].glyph       = (uint8_t)s->current_glyph;
                s->decal.pattern[idx].material_id = (uint8_t)s->current_material_id;
                s->dirty = 1;
            }
            if (input->held_erase) {
                s->decal.pattern[idx].glyph       = ' ';
                s->decal.pattern[idx].material_id = (uint8_t)s->current_material_id;
                s->dirty = 1;
            }
        }

        /* Place glyph — also writes current_material_id to the cell */
        if (input->place) {
            int idx = s->cursor_row * s->canvas_cols + s->cursor_col;
            s->decal.pattern[idx].glyph       = (uint8_t)s->current_glyph;
            s->decal.pattern[idx].material_id = (uint8_t)s->current_material_id;
            s->dirty = 1;
        }

        /* Erase glyph */
        if (input->erase) {
            int idx = s->cursor_row * s->canvas_cols + s->cursor_col;
            if (s->decal.pattern[idx].glyph != (uint8_t)' ') {
                s->decal.pattern[idx].glyph = (uint8_t)' ';
                s->dirty = 1;
            }
        }

        /* Direct glyph typing from text_input.
         * Skip space (handled by INPUT_PLACE),
         * skip non-printable ASCII. */
        {
            for (int i = 0; i < input->text_input_len; i++) {
                char c = input->text_input[i];
                if (c == ' ' || c < 32 || c > 126) continue;
                if (!ad_glyph_allowed(assets, s->current_material_id, c)) {
                    snprintf(s->status_msg, sizeof(s->status_msg),
                             "Glyph '%c' not allowed by material", c);
                    s->status_frames = AD_STATUS_FRAMES;
                    continue;
                }
                /* Place the glyph */
                int idx = s->cursor_row * s->canvas_cols + s->cursor_col;
                s->decal.pattern[idx].glyph       = (uint8_t)c;
                s->decal.pattern[idx].material_id = (uint8_t)s->current_material_id;
                s->dirty = 1;
                s->current_glyph = c;   /* update current glyph on valid typed input */
                /* Do NOT advance cursor */
            }
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
    s->current_glyph       = '#';
    s->editor_surface      = AD_SURFACE_WALL;  /* editor surface default */
    s->current_material_id = 1;   /* default material for newly placed glyphs */
    s->dirty               = 0;
    s->repeat_dir          = -1;
    s->repeat_timer        = 0;
    s->mode                = AD_DECAL_EDIT;
    s->current_filename[0] = '\0';
    s->filename_buffer[0]  = '\0';
    s->filename_pos        = 0;
}

void asset_designer_destroy(AssetDesignerState *s) {
    if (s) decal_release_contents(&s->decal);
}

AssetDesignerResult asset_designer_update(AssetDesignerState *s,
                                           const InputState *input,
                                           const AssetRegistry *assets) {
    if (!s || !input)         return AD_RESULT_NONE;
    if (!s->decal.pattern)    return AD_RESULT_NONE;

    AssetDesignerResult result = AD_RESULT_NONE;

    switch (s->mode) {
        case AD_DECAL_EDIT:
            result = ad_update_edit(s, input, assets);
            break;
        case AD_SAVE_PROMPT:
            result = ad_update_save_prompt(s, input);
            break;
        case AD_LOAD_SELECT:
            result = ad_update_load_select(s, input);
            break;
        case AD_META_EDIT:
            result = ad_update_meta_edit(s, input, assets);
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
 *  Modal box helpers (render only)
 * =================================================================== */

static void ad_fill_box(Grid *grid, int x, int y, int w, int h,
                         SDL_Color fg, SDL_Color bg) {
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            if (x + c < grid->width && y + r < grid->height) {
                grid_set(grid, x + c, y + r, ' ', fg, bg);
            }
        }
    }
}

static void ad_draw_border(Grid *grid, int x, int y, int w, int h,
                            SDL_Color fg, SDL_Color bg) {
    /* corners */
    if (x < grid->width && y < grid->height)
        grid_set(grid, x,         y,         '+', fg, bg);
    if (x + w - 1 < grid->width && y < grid->height)
        grid_set(grid, x + w - 1, y,         '+', fg, bg);
    if (x < grid->width && y + h - 1 < grid->height)
        grid_set(grid, x,         y + h - 1, '+', fg, bg);
    if (x + w - 1 < grid->width && y + h - 1 < grid->height)
        grid_set(grid, x + w - 1, y + h - 1, '+', fg, bg);
    /* top/bottom */
    for (int c = 1; c < w - 1; c++) {
        if (x + c < grid->width) {
            if (y < grid->height)
                grid_set(grid, x + c, y,         '-', fg, bg);
            if (y + h - 1 < grid->height)
                grid_set(grid, x + c, y + h - 1, '-', fg, bg);
        }
    }
    /* left/right */
    for (int r = 1; r < h - 1; r++) {
        if (y + r < grid->height) {
            if (x < grid->width)
                grid_set(grid, x,         y + r, '|', fg, bg);
            if (x + w - 1 < grid->width)
                grid_set(grid, x + w - 1, y + r, '|', fg, bg);
        }
    }
}

void asset_designer_render(const AssetDesignerState *s, Grid *grid,
                                const AssetRegistry *assets) {
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
    if (s->editor_surface == AD_SURFACE_ALL) {
        snprintf(title, sizeof(title),
                 s->dirty ? "=== DECAL DESIGNER [*] File: %s [All export] ===" :
                            "=== DECAL DESIGNER     File: %s [All export] ===",
                 fname);
    } else {
        snprintf(title, sizeof(title),
                 s->dirty ? "=== DECAL DESIGNER [*] File: %s ===" :
                            "=== DECAL DESIGNER     File: %s ===",
                 fname);
    }
    grid_print(grid, CANVAS_X0, 0, title,
               s->dirty ? fg_yellow : fg_white, bg_black);

    /* ---- Key hints (edit mode only) ---- */
    if (s->mode == AD_DECAL_EDIT) {
        grid_print(grid, CANVAS_X0, 1,
                   "Arrows:Move  Spc:Place  Bksp:Erase"
                   "  Tab:Meta  F5:Save  F9:Load  F10:SaveAs  Esc:Exit"
                   "  Type glyph to paint  Space=stamp  Mat restricts glyphs",
                   fg_gray, bg_black);
    }

    /* ---- Glyph indicator ---- */
    char glyph_line[80];
    snprintf(glyph_line, sizeof(glyph_line),
             "Glyph:[%c]  Canvas:%dx%d  Cursor:(%d,%d)",
             s->current_glyph == ' ' ? '_' : s->current_glyph,
             s->canvas_cols, s->canvas_rows,
             s->cursor_col, s->cursor_row);
    grid_print(grid, CANVAS_X0, 2, glyph_line, fg_green, bg_black);

    /* Canvas border (render-only, never saved) */
    {
        int bx = CANVAS_X0 - 1;
        int by = CANVAS_Y0 - 1;
        int bw = s->canvas_cols + 2;
        int bh = s->canvas_rows + 2;
        SDL_Color bfg = {128, 128, 128, 255};
        SDL_Color bbg = {0, 0, 0, 255};
        /* Top-left corner */
        grid_set(grid, bx, by, '+', bfg, bbg);
        /* Top-right corner */
        if (bx + bw - 1 < grid->width)
            grid_set(grid, bx + bw - 1, by, '+', bfg, bbg);
        /* Bottom-left corner */
        if (by + bh - 1 < grid->height)
            grid_set(grid, bx, by + bh - 1, '+', bfg, bbg);
        /* Bottom-right corner */
        if (bx + bw - 1 < grid->width && by + bh - 1 < grid->height)
            grid_set(grid, bx + bw - 1, by + bh - 1, '+', bfg, bbg);
        /* Top and bottom horizontal lines */
        for (int c = 1; c < bw - 1; c++) {
            if (bx + c < grid->width) {
                grid_set(grid, bx + c, by,          '-', bfg, bbg);
                if (by + bh - 1 < grid->height)
                    grid_set(grid, bx + c, by + bh - 1, '-', bfg, bbg);
            }
        }
        /* Left and right vertical lines */
        for (int r = 1; r < bh - 1; r++) {
            if (by + r < grid->height) {
                grid_set(grid, bx,          by + r, '|', bfg, bbg);
                if (bx + bw - 1 < grid->width)
                    grid_set(grid, bx + bw - 1, by + r, '|', bfg, bbg);
            }
        }
    }

    /* ---- Canvas cells ---- */
    if (s->decal.pattern) {
        for (int r = 0; r < s->canvas_rows; r++) {
            for (int c = 0; c < s->canvas_cols; c++) {
                int gx = CANVAS_X0 + c;
                int gy = CANVAS_Y0 + r;
                if (gx >= grid->width || gy >= grid->height) continue;

                PatternCell cell = s->decal.pattern[r * s->canvas_cols + c];
                char ch = (char)cell.glyph;  /* stored glyph, space if empty */
                bool is_cursor = (s->mode == AD_DECAL_EDIT) &&
                                 (r == s->cursor_row && c == s->cursor_col);

                SDL_Color cell_fg, cell_bg;
                cell_bg = is_cursor ? bg_cursor : bg_black;

                if (ch != ' ' && assets != NULL) {
                    /* Use the cell's stored material_id for color */
                    int mid = cell.material_id;
                    if (mid > 0 && mid < 256 &&
                        assets->materials[mid].palette_id >= 0 &&
                        assets->materials[mid].palette_id < 256) {
                        const Material *mat = &assets->materials[mid];
                        const Palette  *pal = &assets->palettes[mat->palette_id];
                        cell_fg = palette_sample(pal, 1.0, 1.0);
                    } else {
                        cell_fg = (SDL_Color){200, 200, 200, 255};
                    }
                } else if (ch != ' ') {
                    /* assets == NULL: fall back to white */
                    cell_fg = fg_white;
                } else {
                    /* Empty cell: dim gray */
                    cell_fg = (SDL_Color){60, 60, 60, 255};
                }

                /* Cursor highlight: invert fg/bg */
                if (is_cursor) {
                    cell_fg = bg_black;
                    cell_bg = bg_cursor;
                }

                grid_set(grid, gx, gy, (uint8_t)ch, cell_fg, cell_bg);
            }
        }
    }

    /* ---- Metadata panel (right of canvas, only in edit mode) ---- */
    if (s->mode == AD_DECAL_EDIT) {
        int meta_x = CANVAS_X0 + s->canvas_cols + 3;
        int meta_y = CANVAS_Y0 - 1;

        const char * const meta_labels[AD_META_EDIT_COUNT] = {
            "surface ", "width   ", "height  ", "step_u  ", "step_v  ", "brush   ",
            "cols    ", "rows    ",
        };

        grid_print(grid, meta_x, meta_y,
                   s->metadata_focus ? "[Metadata*]" : "[Metadata ]",
                   s->metadata_focus ? fg_yellow : fg_cyan, bg_black);
        meta_y++;

        for (int r = 0; r < AD_META_EDIT_COUNT; r++) {
            bool sel = (s->metadata_focus && r == s->metadata_row);
            SDL_Color mfg = sel ? fg_yellow : fg_white;
            char val[24];
            switch (r) {
                case 0:  snprintf(val, sizeof(val), "%s",   AD_SURFACE_NAMES[(int)s->editor_surface]); break;
                case 1:  snprintf(val, sizeof(val), "%.2f", s->decal.width);           break;
                case 2:  snprintf(val, sizeof(val), "%.2f", s->decal.height);          break;
                case 3:  snprintf(val, sizeof(val), "%.4f", s->decal.glyph_step_u);    break;
                case 4:  snprintf(val, sizeof(val), "%.4f", s->decal.glyph_step_v);    break;
                case 5: {
                    const char *mname = assets
                        ? material_name_by_id(assets, s->current_material_id)
                        : NULL;
                    if (mname) {
                        snprintf(val, sizeof(val), "%s", mname);
                    } else {
                        snprintf(val, sizeof(val), "%d", s->current_material_id);
                    }
                    break;
                }
                case 6:  snprintf(val, sizeof(val), "%d",   s->canvas_cols); break;
                case 7:  snprintf(val, sizeof(val), "%d",   s->canvas_rows); break;
                default: val[0] = '\0'; break;
            }
            char line[48];
            snprintf(line, sizeof(line), "%s%s %s", sel ? ">" : " ", meta_labels[r], val);
            if (meta_x < grid->width && meta_y + r < grid->height)
                grid_print(grid, meta_x, meta_y + r, line, mfg, bg_black);

            /* Material (brush) preview: show all non-space glyphs for this material */
            if (r == 5 && assets != NULL) {
                const Material *mat = &assets->materials[s->current_material_id];
                const Palette  *pal = &assets->palettes[mat->palette_id];
                SDL_Color mc = palette_sample(pal, 1.0, 1.0);
                SDL_Color bg0 = {0, 0, 0, 255};
                /* Build preview string: all non-space, nonzero glyphs */
                char preview_buf[8];
                int pi = 0;
                for (int g = 0; g < 4; g++) {
                    if (mat->glyphs[g] && (char)mat->glyphs[g] != ' ') {
                        preview_buf[pi++] = (char)mat->glyphs[g];
                    }
                }
                if (pi == 0) preview_buf[pi++] = '@';  /* fallback */
                preview_buf[pi] = '\0';
                /* Render preview at an appropriate x offset (after the value text) */
                int px = meta_x + 2;  /* use the same x base as the rest of the panel */
                if (px + 12 < grid->width) {
                    grid_print(grid, px + 12, meta_y + r, preview_buf, mc, bg0);
                }
            }
        }

        int mhint_y = meta_y + AD_META_EDIT_COUNT + 1;
        if (meta_x < grid->width && mhint_y < grid->height)
            grid_print(grid, meta_x, mhint_y,
                       "Tab:focus  L/R:adj  U/D:sel  Enter:edit",
                       fg_gray, bg_black);
    }

    /* ---- Status message ---- */
    int status_y = CANVAS_Y0 + s->canvas_rows + 1;
    if (status_y < grid->height && s->status_frames > 0 && s->status_msg[0]) {
        grid_print(grid, CANVAS_X0, status_y, s->status_msg, fg_red, bg_black);
    }

    /* ---- Save-prompt overlay (modal box) ---- */
    if (s->mode == AD_SAVE_PROMPT) {
        /* Box: cols 20-60, rows 10-17 */
        int bx = 20, by = 10, bw = 41, bh = 8;
        SDL_Color box_bg  = {  0,   0,  30, 255};
        SDL_Color box_bdr = {  0, 180, 220, 255};
        ad_fill_box(grid, bx, by, bw, bh, fg_white, box_bg);
        ad_draw_border(grid, bx, by, bw, bh, box_bdr, box_bg);
        /* Title */
        grid_print(grid, bx + 2, by + 1, "[ Save As ]", fg_cyan, box_bg);
        /* Name field */
        char save_name_buf[AD_FILENAME_MAX + 16];
        snprintf(save_name_buf, sizeof(save_name_buf),
                 "Name: %s_", s->filename_buffer);
        grid_print(grid, bx + 2, by + 3, save_name_buf, fg_white, box_bg);
        /* Status */
        if (s->status_frames > 0 && s->status_msg[0]) {
            grid_print(grid, bx + 2, by + 5, s->status_msg, fg_red, box_bg);
        }
        /* Hint */
        grid_print(grid, bx + 2, by + 6,
                   "Enter=save  Esc=cancel", fg_gray, box_bg);
    }

    /* ---- Load-select overlay (modal box) ---- */
    if (s->mode == AD_LOAD_SELECT) {
        /* Box: cols 20-62, rows 8-22 */
        int bx = 20, by = 8, bw = 43, bh = 15;
        SDL_Color box_bg  = {  0,  20,   0, 255};
        SDL_Color box_bdr = { 50, 200,  50, 255};
        ad_fill_box(grid, bx, by, bw, bh, fg_white, box_bg);
        ad_draw_border(grid, bx, by, bw, bh, box_bdr, box_bg);
        /* Title */
        grid_print(grid, bx + 2, by + 1, "[ Load Decal ]", fg_cyan, box_bg);
        if (s->file_count == 0) {
            grid_print(grid, bx + 2, by + 3,
                       "(no decal files found)", fg_gray, box_bg);
        } else {
            int start = s->file_sel_idx - 4;
            if (start < 0) start = 0;
            int end = start + 10;
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
        /* Hint */
        grid_print(grid, bx + 2, by + bh - 2,
                   "Enter=load  Esc=cancel", fg_gray, box_bg);
    }

    /* ---- Metadata direct-edit overlay ---- */
    if (s->mode == AD_META_EDIT) {
        static const char * const meta_field_labels[AD_META_EDIT_COUNT] = {
            "surface", "width", "height", "step_u", "step_v", "brush",
            "cols", "rows"
        };
        int oy = CANVAS_Y0 + 2;
        grid_print(grid, CANVAS_X0, oy,
                   "[ Edit Field ]", fg_cyan, bg_black);
        char label_line[48];
        snprintf(label_line, sizeof(label_line), "  %s:",
                 meta_field_labels[s->metadata_row]);
        grid_print(grid, CANVAS_X0, oy + 1, label_line, fg_white, bg_black);
        char buf_line[48];
        snprintf(buf_line, sizeof(buf_line), "  > %s_", s->meta_edit_buffer);
        grid_print(grid, CANVAS_X0, oy + 2, buf_line, fg_yellow, bg_black);
        grid_print(grid, CANVAS_X0, oy + 3,
                   "  Enter:commit  Esc:cancel  Backspace:delete",
                   fg_gray, bg_black);
    }
}
