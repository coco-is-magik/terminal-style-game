/**
 * decal_io.c — Per-file decal load, save, and free
 *
 * Implements the public API declared in decal_io.h.
 *
 * Loading mirrors the logic of the internal load_decal() in asset_loader.c:
 * both support structured key-value mode (pattern_N= / material_N= rows)
 * and inline art mode (art= header followed by ASCII rows).  This ensures
 * the same files can be loaded by either the engine's batch loader or the
 * Asset Designer's single-file loader.
 *
 * Saving always writes structured key-value mode (Mode A) so every cell's
 * material_id is preserved through the round-trip.
 */

#include "decal_io.h"
#include "checked_size.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>   /* FILE, fopen, fclose, fgets, fprintf, fputc, remove */
#include <stdlib.h>  /* calloc, free, atoi, atof */
#include <string.h>  /* strcmp, strncmp, strncpy, strlen, memset */

/* ===================================================================
 *  Private helpers
 * =================================================================== */

/**
 * trim_newline() — Strip trailing \\n and \\r from a string in place.
 */
static void trim_newline(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

/**
 * parse_kv() — Split "key=value" in place.
 *
 * Modifies the input buffer.  Sets *key to the part before the first '=',
 * *val to everything after (preserving embedded spaces).  Trailing newline /
 * carriage-return is stripped from *val only.  Trailing whitespace is stripped
 * from *key.
 *
 * If the line has no '=', *key is set and *val is NULL.
 */
static void parse_kv(char *line, char **key, char **val) {
    *key = strtok(line, "=");
    *val = strtok(NULL, "");   /* Grab everything after the first '=' */

    if (*key) {
        /* Strip trailing whitespace from the key */
        int len = (int)strlen(*key);
        while (len > 0 && ((*key)[len - 1] == ' '  || (*key)[len - 1] == '\t' ||
                            (*key)[len - 1] == '\n' || (*key)[len - 1] == '\r')) {
            (*key)[--len] = '\0';
        }
    }

    if (*val) {
        /* Strip trailing newline / CR from value; preserve internal spaces */
        trim_newline(*val);
    }
}

static bool parse_bounded_dimension(const char *text, int maximum, int *out) {
    char *end = NULL;
    long value;
    if (!text || !out) return false;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno == ERANGE || end == text) return false;
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
    if (*end != '\0' || value <= 0 || value > maximum || value > INT_MAX) return false;
    *out = (int)value;
    return true;
}

/* ===================================================================
 *  Public API
 * =================================================================== */

Decal *decal_load_from_file(const char *path) {
    if (!path) return NULL;

    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    Decal *d = calloc(1, sizeof(Decal));
    if (!d) {
        fclose(f);
        return NULL;
    }

    /* Sane defaults for optional fields */
    d->pattern_cols   = 1;
    d->pattern_rows   = 1;
    int default_material = 1;

    /*
     * Temporary row buffers: up to 64 rows, 255 chars each.
     * p_buf[r] holds the glyph string for row r.
     * m_buf[r] holds the comma-separated material IDs for row r.
     */
    char p_buf[DECAL_PATTERN_MAX_ROWS][DECAL_PATTERN_MAX_COLS + 1];
    char m_buf[DECAL_PATTERN_MAX_ROWS][DECAL_MATERIAL_ROW_MAX_BYTES + 1];
    memset(p_buf, 0, sizeof(p_buf));
    memset(m_buf, 0, sizeof(m_buf));

    int art_mode = 0;

    char line[DECAL_MATERIAL_ROW_MAX_BYTES + 32];
    while (fgets(line, sizeof(line), f)) {
        /* Detect the inline-art header (stops the key-value parse loop) */
        if (strncmp(line, "art=", 4) == 0 ||
            strcmp(line, "art\n")    == 0 ||
            strcmp(line, "art\r\n")  == 0) {
            art_mode = 1;
            break;
        }

        char *key = NULL;
        char *val = NULL;
        parse_kv(line, &key, &val);
        if (!key || !val) continue;

        /* Spatial properties */
        if      (strcmp(key, "surface")     == 0) d->surface      = (DecalSurface)atoi(val);
        else if (strcmp(key, "x")           == 0) d->x            = atof(val);
        else if (strcmp(key, "y")           == 0) d->y            = atof(val);
        else if (strcmp(key, "z")           == 0) d->z            = atof(val);
        else if (strcmp(key, "map_x")       == 0) d->map_x        = atoi(val);
        else if (strcmp(key, "map_y")       == 0) d->map_y        = atoi(val);
        else if (strcmp(key, "side")        == 0) d->side         = atoi(val);
        else if (strcmp(key, "u")           == 0) d->u            = atof(val);
        else if (strcmp(key, "v")           == 0) d->v            = atof(val);
        else if (strcmp(key, "width")       == 0) d->width        = atof(val);
        else if (strcmp(key, "height")      == 0) d->height       = atof(val);
        else if (strcmp(key, "glyph_step_u")== 0) d->glyph_step_u = atof(val);
        else if (strcmp(key, "glyph_step_v")== 0) d->glyph_step_v = atof(val);
        else if (strcmp(key, "depth")       == 0) d->depth        = atof(val);
        else if (strcmp(key, "rotation")    == 0) d->rotation     = atof(val);

        /* Pattern layout */
        else if (strcmp(key, "pattern_cols") == 0) {
            if (!parse_bounded_dimension(val, DECAL_PATTERN_MAX_COLS, &d->pattern_cols)) goto fail;
        }
        else if (strcmp(key, "pattern_rows") == 0) {
            if (!parse_bounded_dimension(val, DECAL_PATTERN_MAX_ROWS, &d->pattern_rows)) goto fail;
        }
        else if (strcmp(key, "default_material")== 0) {
            char *end = NULL;
            long parsed = strtol(val, &end, 10);
            if (end == val || *end != '\0' || parsed < 0 || parsed > ASSET_ID_MAX) goto fail;
            default_material = (int)parsed;
        }

        /* Per-row pattern data: keys are "pattern_0", "pattern_1", ... */
        else if (strncmp(key, "pattern_", 8) == 0) {
            int r = atoi(key + 8);
            if (r >= 0 && r < DECAL_PATTERN_MAX_ROWS) {
                strncpy(p_buf[r], val, 255);
                p_buf[r][255] = '\0';
            }
        }

        /* Per-row material data: keys are "material_0", "material_1", ... */
        else if (strncmp(key, "material_", 9) == 0) {
            int r = atoi(key + 9);
            if (r >= 0 && r < DECAL_PATTERN_MAX_ROWS) {
                strncpy(m_buf[r], val, DECAL_MATERIAL_ROW_MAX_BYTES);
                m_buf[r][DECAL_MATERIAL_ROW_MAX_BYTES] = '\0';
            }
        }
    }

    /* Allocate the flat pattern array (calloc zeroes it) */
    size_t cells;
    size_t pattern_bytes;
    if (!checked_size_2d(d->pattern_cols, d->pattern_rows, &cells) ||
        !checked_size_bytes(cells, sizeof(PatternCell), &pattern_bytes)) goto fail;
    (void)pattern_bytes;
    d->pattern = calloc(cells, sizeof(PatternCell));
    if (!d->pattern) goto fail;

    if (art_mode) {
        /* --- Inline ASCII art mode ---
         * Each remaining file line is one pattern row. */
        int success = 1;
        for (int r = 0; r < d->pattern_rows; r++) {
            if (!fgets(line, sizeof(line), f)) {
                success = 0;
                break;
            }
            int len = (int)strlen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
                line[--len] = '\0';
            }
            if (len < d->pattern_cols) {
                success = 0;
                break;
            }
            for (int c = 0; c < d->pattern_cols; c++) {
                char g = line[c];
                if (g == '\t') g = ' ';
                d->pattern[r * d->pattern_cols + c].glyph       = (uint8_t)g;
                d->pattern[r * d->pattern_cols + c].material_id = (uint16_t)default_material;
            }
        }
        if (!success && d->pattern) {
            /* Fill with error markers on dimension mismatch */
            for (size_t i = 0; i < cells; i++) {
                d->pattern[i].glyph       = '!';
                d->pattern[i].material_id = (uint16_t)default_material;
            }
        }
    } else {
        /* --- Structured key-value mode ---
         * Each row's glyphs come from p_buf[r]; materials from m_buf[r]. */
        for (int r = 0; r < d->pattern_rows; r++) {
            /* Parse comma-separated material IDs for this row */
            int mats[256];
            for (int i = 0; i < 256; i++) mats[i] = default_material;
            if (m_buf[r][0] != '\0') {
                char *p = m_buf[r];
                int c = 0;
                while (*p && c < d->pattern_cols) {
                    char *end = NULL;
                    long parsed = strtol(p, &end, 10);
                    if (end == p || parsed < 0 || parsed > ASSET_ID_MAX) goto fail;
                    mats[c++] = (int)parsed;
                    p = end;
                    while (*p == ' ' || *p == '\t') p++;
                    if (*p != '\0' && *p != ',') goto fail;
                    if (*p == ',') p++;
                }
            }
            for (int c = 0; c < d->pattern_cols; c++) {
                char glyph = ' ';
                if (c < (int)strlen(p_buf[r])) glyph = p_buf[r][c];
                d->pattern[r * d->pattern_cols + c].glyph       = (uint8_t)glyph;
                d->pattern[r * d->pattern_cols + c].material_id = (uint16_t)mats[c];
            }
        }
    }

    fclose(f);
    return d;

fail:
    fclose(f);
    decal_free(d);
    return NULL;
}

int decal_save_to_file(const char *path, const Decal *decal) {
    if (!path || !decal) return -1;

    FILE *f = fopen(path, "w");
    if (!f) return -1;

    /* All fields in the same key=value format the engine parser expects */
    fprintf(f, "surface=%d\n",        (int)decal->surface);
    fprintf(f, "x=%f\n",              decal->x);
    fprintf(f, "y=%f\n",              decal->y);
    fprintf(f, "z=%f\n",              decal->z);
    fprintf(f, "map_x=%d\n",          decal->map_x);
    fprintf(f, "map_y=%d\n",          decal->map_y);
    fprintf(f, "side=%d\n",           decal->side);
    fprintf(f, "u=%f\n",              decal->u);
    fprintf(f, "v=%f\n",              decal->v);
    fprintf(f, "width=%f\n",          decal->width);
    fprintf(f, "height=%f\n",         decal->height);
    fprintf(f, "glyph_step_u=%f\n",   decal->glyph_step_u);
    fprintf(f, "glyph_step_v=%f\n",   decal->glyph_step_v);
    fprintf(f, "depth=%f\n",          decal->depth);
    fprintf(f, "rotation=%f\n",       decal->rotation);
    fprintf(f, "pattern_cols=%d\n",   decal->pattern_cols);
    fprintf(f, "pattern_rows=%d\n",   decal->pattern_rows);
    fprintf(f, "default_material=1\n");

    if (decal->pattern && decal->pattern_cols > 0 && decal->pattern_rows > 0) {
        /* Glyph rows */
        for (int r = 0; r < decal->pattern_rows; r++) {
            fprintf(f, "pattern_%d=", r);
            for (int c = 0; c < decal->pattern_cols; c++) {
                uint8_t glyph = decal->pattern[r * decal->pattern_cols + c].glyph;
                fputc(glyph == 0U ? ' ' : (int)glyph, f);
            }
            fputc('\n', f);
        }
        /* Material rows */
        for (int r = 0; r < decal->pattern_rows; r++) {
            fprintf(f, "material_%d=", r);
            for (int c = 0; c < decal->pattern_cols; c++) {
                if (c > 0) fputc(',', f);
                fprintf(f, "%d",
                        (int)decal->pattern[r * decal->pattern_cols + c].material_id);
            }
            fputc('\n', f);
        }
    }

    int failed = ferror(f) != 0;
    if (fclose(f) != 0) failed = 1;
    return failed ? -1 : 0;
}

void decal_free(Decal *decal) {
    if (!decal) return;
    free(decal->pattern);
    free(decal);
}

void decal_release_contents(Decal *decal) {
    if (!decal) return;
    free(decal->pattern);
    decal->pattern = NULL;
}
