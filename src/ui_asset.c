/**
 * ui_asset.c — Screen-space UI button asset loading and rendering
 *
 * Loads button assets from assets/ui/<id>.txt and renders them into the
 * terminal Grid.  Each button has a fixed width×height art grid for
 * three visual states: normal, selected, and (optionally) disabled.
 *
 * File format:
 *   id=<int>
 *   width=<int>
 *   height=<int>
 *   normal_0=<exactly width chars>
 *   normal_1=<exactly width chars>
 *   ...
 *   selected_0=<exactly width chars>
 *   selected_1=<exactly width chars>
 *   ...
 *   disabled_0=<exactly width chars>   (optional)
 *   disabled_1=<exactly width chars>   (optional)
 *   ...
 *
 * Splitting on the FIRST '=' preserves art rows that contain '=' characters.
 */

#include "ui_asset.h"
#include <stdio.h>      /* FILE, fopen, fgets, fclose, fprintf, snprintf */
#include <stdlib.h>     /* malloc, free, atoi */
#include <string.h>     /* strchr, strcmp, strncmp, strlen, strncpy, memset, memcpy */
#include <stdint.h>     /* uint8_t */
#include <SDL3/SDL.h>   /* SDL_Color */

/* ---- Button-state colours ---- */
static const SDL_Color NORMAL_FG   = {200, 200, 200, 255};
static const SDL_Color NORMAL_BG   = {  0,   0,   0, 255};
static const SDL_Color SELECTED_FG = { 50, 255,  50, 255};
static const SDL_Color SELECTED_BG = {  0,  40,   0, 255};
static const SDL_Color DISABLED_FG = { 80,  80,  80, 255};
static const SDL_Color DISABLED_BG = {  0,   0,   0, 255};

/* Max rows supported per button — files with more rows are rejected */
#define MAX_BUTTON_ROWS 64

/* ===================================================================
 *  Static helpers
 * =================================================================== */

/**
 * trim_newline() — Strip trailing CR/LF characters in-place
 *
 * Only removes '\n' and '\r'; does NOT remove spaces, which may be
 * meaningful in art rows.
 *
 * @param s  NUL-terminated string to modify
 */
static void trim_newline(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

/**
 * split_key_val() — Split "key=value" in-place on the FIRST '='
 *
 * Splits on the first '=' so that values may contain additional '='
 * characters (e.g. selected art rows using '=' as a glyph).
 *
 * @param line     Input line (modified in place: '=' → '\0')
 * @param key_out  Output pointer to the key portion of line
 * @param val_out  Output pointer to the value portion of line
 * @return         true if a '=' was found, false otherwise
 */
static bool split_key_val(char *line, char **key_out, char **val_out) {
    char *eq = strchr(line, '=');
    if (!eq) return false;
    *eq      = '\0';
    *key_out = line;
    *val_out = eq + 1;
    trim_newline(*val_out);
    return true;
}

/* ===================================================================
 *  Public API
 * =================================================================== */

UIButtonAsset *ui_asset_load(int id, const char *base_path) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/ui/%d.txt", base_path, id);

    FILE *f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "ui_asset_load: cannot open %s\n", filepath);
        return NULL;
    }

    int file_id = 0;
    int width   = 0;
    int height  = 0;

    /* Row buffers (up to MAX_BUTTON_ROWS per state) */
    char n_buf[MAX_BUTTON_ROWS][512];   /* normal_<row>   */
    char s_buf[MAX_BUTTON_ROWS][512];   /* selected_<row> */
    char d_buf[MAX_BUTTON_ROWS][512];   /* disabled_<row> */
    bool has_disabled[MAX_BUTTON_ROWS];

    memset(n_buf,        0, sizeof(n_buf));
    memset(s_buf,        0, sizeof(s_buf));
    memset(d_buf,        0, sizeof(d_buf));
    memset(has_disabled, 0, sizeof(has_disabled));

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *key = NULL;
        char *val = NULL;
        if (!split_key_val(line, &key, &val)) continue;

        if (strcmp(key, "id") == 0) {
            file_id = atoi(val);
        } else if (strcmp(key, "width") == 0) {
            width = atoi(val);
        } else if (strcmp(key, "height") == 0) {
            height = atoi(val);
        } else if (strncmp(key, "normal_", 7) == 0) {
            int r = atoi(key + 7);
            if (r >= 0 && r < MAX_BUTTON_ROWS) {
                strncpy(n_buf[r], val, 511);
                n_buf[r][511] = '\0';
            }
        } else if (strncmp(key, "selected_", 9) == 0) {
            int r = atoi(key + 9);
            if (r >= 0 && r < MAX_BUTTON_ROWS) {
                strncpy(s_buf[r], val, 511);
                s_buf[r][511] = '\0';
            }
        } else if (strncmp(key, "disabled_", 9) == 0) {
            int r = atoi(key + 9);
            if (r >= 0 && r < MAX_BUTTON_ROWS) {
                strncpy(d_buf[r], val, 511);
                d_buf[r][511] = '\0';
                has_disabled[r] = true;
            }
        }
    }
    fclose(f);

    /* ---- Validate dimensions ---- */
    if (width <= 0 || height <= 0 || height > MAX_BUTTON_ROWS) {
        fprintf(stderr,
                "ui_asset_load: invalid dimensions %dx%d in %s\n",
                width, height, filepath);
        return NULL;
    }

    /* Validate each normal and selected row is exactly width chars */
    for (int r = 0; r < height; r++) {
        int nlen = (int)strlen(n_buf[r]);
        if (nlen != width) {
            fprintf(stderr,
                    "ui_asset_load: normal row %d length %d != width %d in %s\n",
                    r, nlen, width, filepath);
            return NULL;
        }
        int slen = (int)strlen(s_buf[r]);
        if (slen != width) {
            fprintf(stderr,
                    "ui_asset_load: selected row %d length %d != width %d in %s\n",
                    r, slen, width, filepath);
            return NULL;
        }
    }

    /* ---- Allocate the asset ---- */
    UIButtonAsset *asset = malloc(sizeof(UIButtonAsset));
    if (!asset) return NULL;

    int size       = width * height;
    asset->id      = file_id;
    asset->width   = width;
    asset->height  = height;
    asset->normal   = malloc((size_t)size);
    asset->selected = malloc((size_t)size);
    asset->disabled = NULL;

    if (!asset->normal || !asset->selected) {
        free(asset->normal);
        free(asset->selected);
        free(asset);
        return NULL;
    }

    /* Copy art rows into flat row-major buffers */
    for (int r = 0; r < height; r++) {
        memcpy(asset->normal   + r * width, n_buf[r], (size_t)width);
        memcpy(asset->selected + r * width, s_buf[r], (size_t)width);
    }

    /* ---- Optional disabled state ---- */
    /* Only stored if all rows are present and all have the correct width */
    bool all_disabled = true;
    for (int r = 0; r < height; r++) {
        if (!has_disabled[r]) {
            all_disabled = false;
            break;
        }
    }
    if (all_disabled) {
        bool valid = true;
        for (int r = 0; r < height; r++) {
            if ((int)strlen(d_buf[r]) != width) {
                valid = false;
                break;
            }
        }
        if (valid) {
            asset->disabled = malloc((size_t)size);
            if (asset->disabled) {
                for (int r = 0; r < height; r++) {
                    memcpy(asset->disabled + r * width, d_buf[r], (size_t)width);
                }
            }
        }
    }

    return asset;
}

void ui_asset_destroy(UIButtonAsset *asset) {
    if (!asset) return;
    free(asset->normal);
    free(asset->selected);
    free(asset->disabled);
    free(asset);
}

void ui_button_render(Grid *grid, int x, int y,
                      const UIButtonAsset *asset, UIButtonState state) {
    SDL_Color fg;
    SDL_Color bg;
    const char *art;

    switch (state) {
        case UI_BUTTON_SELECTED:
            fg  = SELECTED_FG;
            bg  = SELECTED_BG;
            art = asset->selected;
            break;
        case UI_BUTTON_DISABLED:
            fg  = DISABLED_FG;
            bg  = DISABLED_BG;
            art = asset->disabled ? asset->disabled : asset->normal;
            break;
        case UI_BUTTON_NORMAL:
        default:
            fg  = NORMAL_FG;
            bg  = NORMAL_BG;
            art = asset->normal;
            break;
    }

    for (int r = 0; r < asset->height; r++) {
        for (int c = 0; c < asset->width; c++) {
            uint8_t glyph = (uint8_t)art[r * asset->width + c];
            grid_set(grid, x + c, y + r, glyph, fg, bg);
        }
    }
}
