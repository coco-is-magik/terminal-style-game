/**
 * asset_loader.c — File-based asset loading system
 *
 * This file provides all the logic to parse text-based asset definition files
 * from disk and populate the AssetRegistry and WorldState structures.
 *
 * Asset file types and their internal formats:
 *
 *   Palettes  (assets/palettes/<id>.txt)
 *     Defines three colour stops — near, mid, far — used to shade walls
 *     based on distance from the camera.  Format:
 *       near=r,g,b,a
 *       mid=r,g,b,a
 *       far=r,g,b,a
 *
 *   Materials (assets/materials/<id>.txt)
 *     Associates a palette ID with up to four distance-based ASCII glyphs.
 *     Format:
 *       palette=<int>
 *       glyphs=<4-char string>   (e.g. "#@:.", each character corresponds
 *                                 to a distance band: near, mid, far, very_far)
 *
 *   Maps      (assets/maps/<id>.txt)
 *     A simple newline-delimited grid of digit characters, where each digit
 *     represents a material ID (0 = empty/void).  Parsed by map_load_from_string().
 *
 *   Decals    (assets/decals/<id>.txt)
 *     A wall / floor / ceiling decal that can be defined in two ways:
 *       1) Key-value header section followed by "art=" and inline ASCII art
 *       2) Full key-value mode with pattern_<row>=... and material_<row>=... lines
 *     See load_decal() for details.
 *
 *   Lights    (assets/lights/<id>.txt)
 *     A point light source.  Format:
 *       x=<double>
 *       y=<double>
 *       color=r,g,b,a
 *       intensity=<double>    (negative → anti-light / darkness)
 *       radius=<double>
 *
 *   Sprites   (assets/sprites/<id>.txt)
 *     A 2D pattern asset intended for billboard-style rendering.  Format uses
 *     the same pattern_<row>/material_<row> scheme as decals.
 */

#include "asset_loader.h"    /* Public API: asset_loader_load_registry(),
                                asset_loader_load_map_data(),
                                asset_loader_load_materials() */
#include "map_loader.h"      /* map_load_from_string() — parses the digit-grid map format */
#include "config.h"          /* config_get() values are used indirectly by map loading */
#include "checked_size.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include "decal_io.h"
#include <stdio.h>           /* FILE, fopen(), fgets(), fclose(), fprintf(), snprintf() */
#include <stdlib.h>          /* strtol(), strtod(), malloc(), free(), calloc() */
#include <string.h>          /* strcmp(), strncmp(), strtok(), strncpy(), memset() */

#define SPRITE_PATTERN_MAX_ROWS 32
#define SPRITE_PATTERN_MAX_COLS 255
#define MAP_FILE_MAX_BYTES (1024U * 1024U)
#include <dirent.h>          /* opendir(), readdir(), closedir(), struct dirent */

/* ===================================================================
 *  Utility helpers (static — not visible outside this file)
 * =================================================================== */

/**
 * parse_color() — Parse an "r,g,b,a" string into an SDL_Color
 *
 * @param val    Comma-separated colour string, e.g. "128,64,32,255"
 * @param color  Output SDL_Color struct that will be filled in
 */
static void parse_color(const char *val, SDL_Color *color) {
    int r, g, b, a;
    if (sscanf(val, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
        color->r = (uint8_t)r;
        color->g = (uint8_t)g;
        color->b = (uint8_t)b;
        color->a = (uint8_t)a;
    }
    /* If sscanf returns <4, the colour is left at its default (caller
     * should have zeroed or pre-set it). */
}

/**
 * trim_string() — Remove trailing whitespace / newline characters in-place
 *
 * Walks backward from the end of the string, replacing any space, tab,
 * newline, or carriage-return with a null terminator.
 *
 * @param str  NUL-terminated string to trim (modified in place)
 */
static void trim_string(char *str) {
    int len = (int)strlen(str);
    if (len == 0) return;
    char *end = str + len - 1;
    while(end >= str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
}

static bool parse_bounded_int(const char *text, int minimum, int maximum, int *out) {
    char *end = NULL;
    long value;
    if (!text || !out) return false;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno == ERANGE || end == text) return false;
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
    if (*end != '\0' || value < minimum || value > maximum ||
        value < INT_MIN || value > INT_MAX) return false;
    *out = (int)value;
    return true;
}

static bool parse_finite_double(const char *text, double minimum,
                                bool minimum_inclusive, double *out) {
    char *end = NULL;
    double value;
    if (!text || !out) return false;
    errno = 0;
    value = strtod(text, &end);
    if (errno == ERANGE || end == text || !isfinite(value)) return false;
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
    if (*end != '\0') return false;
    if (minimum_inclusive ? value < minimum : value <= minimum) return false;
    *out = value;
    return true;
}

static bool parse_material_row(const char *text, int count, int *materials) {
    const char *cursor = text;
    if (!text || count < 0 || !materials) return false;
    for (int index = 0; index < count; index++) {
        char *end = NULL;
        long value;
        errno = 0;
        value = strtol(cursor, &end, 10);
        if (errno == ERANGE || end == cursor || value < 0 ||
            value > ASSET_ID_MAX) return false;
        while (*end == ' ' || *end == '\t') end++;
        materials[index] = (int)value;
        if (index + 1 < count) {
            if (*end != ',') return false;
            cursor = end + 1;
        } else {
            while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
            if (*end != '\0') return false;
        }
    }
    return true;
}

/* ===================================================================
 *  Palette loading
 * =================================================================== */

/**
 * load_palette() — Load a palette definition file and register it
 *
 * Reads a file containing three colour entries (near, mid, far) and stores
 * them in the AssetRegistry under the given palette ID.
 *
 * @param reg       AssetRegistry to store the palette into
 * @param id        Palette ID (1–255)
 * @param filepath  Full path to the palette text file
 * @return          true on success, false if the file could not be opened
 */
static bool load_palette(AssetRegistry *reg, int id, const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

    /* Default to black — will be overwritten if the file contains the keys */
    SDL_Color near_col = {0,0,0,255}, mid_col = {0,0,0,255}, far_col = {0,0,0,255};
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        /* Split on '=': key is everything left of '=', value is everything right */
        char *key = strtok(line, "=");
        char *val = strtok(NULL, "=");
        if (key && val) {
            trim_string(val);  /* Remove trailing \n or whitespace */
            if (strcmp(key, "near") == 0) parse_color(val, &near_col);
            else if (strcmp(key, "mid") == 0) parse_color(val, &mid_col);
            else if (strcmp(key, "far") == 0) parse_color(val, &far_col);
        }
    }
    fclose(f);

    /* Store the three colours in the registry under this palette ID */
    asset_registry_set_palette(reg, id, near_col, mid_col, far_col);
    return true;
}

/* ===================================================================
 *  Material loading
 * =================================================================== */

/**
 * load_material() — Load a material definition file and register it
 *
 * A material associates a palette with a set of glyphs; different glyphs
 * are used depending on the wall distance band (near / mid / far / very_far).
 *
 * @param reg       AssetRegistry to store the material into
 * @param id        Material ID (1–255)
 * @param filepath  Full path to the material text file
 * @return          true on success, false if the file could not be opened
 */
static bool load_material(AssetRegistry *reg, int id, const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

    bool valid = true;
    int pal_id = 0;
    char glyphs[5] = "    ";   /* Up to 4 glyphs + NUL; defaults to spaces */
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *key = strtok(line, "=");
        char *val = strtok(NULL, "=");
        if (key && val) {
            trim_string(val);
            if (strcmp(key, "palette") == 0 &&
                !parse_bounded_int(val, 0, ASSET_ID_MAX, &pal_id)) valid = false;
            else if (strcmp(key, "glyphs") == 0) {
                strncpy(glyphs, val, 4);
                glyphs[4] = '\0';
            }
        }
    }
    if (ferror(f)) valid = false;
    if (fclose(f) != 0) valid = false;
    if (!valid) return false;

    asset_registry_set_material(reg, id, pal_id, glyphs);

    /* Derive the material name from the filepath basename, stripping the
     * ".txt" extension.  e.g. "assets/materials/1.txt" -> "1".
     * Stored in the parallel material_names table so the Asset Designer
     * can display and look up materials by name. */
    {
        const char *base = strrchr(filepath, '/');
        base = base ? base + 1 : filepath;
        strncpy(reg->material_names[id], base, 63);
        reg->material_names[id][63] = '\0';
        char *dot = strrchr(reg->material_names[id], '.');
        if (dot) *dot = '\0';
        reg->material_count++;
    }

    return true;
}

/* ===================================================================
 *  Named material helpers (static — not visible outside this file)
 * =================================================================== */

/**
 * basename_is_numeric() — Return true if the string contains only digit characters
 *
 * Used to distinguish numeric filenames (e.g. "1", "12") from named filenames
 * (e.g. "stone_brick") when scanning the materials directory.
 *
 * @param base  NUL-terminated string to check (typically a filename without extension)
 * @return      true if every character is a digit, false otherwise
 */
static bool basename_is_numeric(const char *base) {
    if (!base || !*base) return false;
    for (const char *p = base; *p; p++) {
        if (*p < '0' || *p > '9') return false;
    }
    return true;
}

/**
 * qsort_str_cmp() — Comparator for qsort over owned string pointers
 *
 * @param a  Pointer to first string element (const char *)
 * @param b  Pointer to second string element (const char *)
 * @return   strcmp result for alphabetic ordering
 */
static int qsort_str_cmp(const void *a, const void *b) {
    const char *const *left = a;
    const char *const *right = b;
    return strcmp(*left, *right);
}

static void free_name_list(char **names, size_t count) {
    if (!names) return;
    for (size_t i = 0U; i < count; i++) free(names[i]);
    free(names);
}

static bool append_name(char ***names, size_t *count, size_t *capacity,
                        const char *name) {
    char **grown;
    char *copy;
    if (*count == *capacity) {
        size_t next = *capacity == 0U ? 16U : *capacity * 2U;
        if (next < *capacity || next > ASSET_ID_CAPACITY) return false;
        grown = realloc(*names, next * sizeof(**names));
        if (!grown) return false;
        *names = grown;
        *capacity = next;
    }
    copy = malloc(strlen(name) + 1U);
    if (!copy) return false;
    memcpy(copy, name, strlen(name) + 1U);
    (*names)[(*count)++] = copy;
    return true;
}

/**
 * load_named_material() — Load a non-numeric material file into a free ID slot
 *
 * Named material files (e.g. "stone_brick.txt") may optionally contain an
 * "id=<n>" field to request a specific slot.  If no id= is present, the
 * first free slot is used.  If the requested slot is already occupied, the
 * file is skipped with a warning to stderr.
 *
 * @param reg       AssetRegistry to store the material into
 * @param filepath  Full path to the material text file
 * @param basename  Filename without the ".txt" extension (used as material name)
 * @return          true on success, false if skipped or file could not be opened
 */
static bool load_named_material(AssetRegistry *reg, const char *filepath,
                                const char *basename) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

    bool valid = true;
    int pal_id = 0;
    char glyphs[5] = "    ";
    int explicit_id = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *key = strtok(line, "=");
        char *val = strtok(NULL, "=");
        if (key && val) {
            trim_string(val);
            if (strcmp(key, "palette") == 0) {
                if (!parse_bounded_int(val, 0, ASSET_ID_MAX, &pal_id)) valid = false;
            } else if (strcmp(key, "glyphs") == 0) {
                strncpy(glyphs, val, 4);
                glyphs[4] = '\0';
            } else if (strcmp(key, "id") == 0) {
                if (!parse_bounded_int(val, 1, ASSET_ID_MAX, &explicit_id)) valid = false;
            }
        }
    }
    if (ferror(f)) valid = false;
    if (fclose(f) != 0) valid = false;
    if (!valid) return false;

    int id;
    if (explicit_id > 0) {
        if (reg->material_names[explicit_id][0] != '\0') {
            fprintf(stderr, "material: ID %d already loaded, skipping '%s'\n",
                    explicit_id, filepath);
            return false;
        }
        id = explicit_id;
    } else {
        id = (int)asset_registry_allocate_material_id(reg);
        if (id == 0) {
            fprintf(stderr, "material: all ID slots full, skipping '%s'\n", filepath);
            return false;
        }
    }

    asset_registry_set_material(reg, id, pal_id, glyphs);
    {
        size_t name_length = strlen(basename);
        if (name_length > 63U) name_length = 63U;
        memcpy(reg->material_names[id], basename, name_length);
        reg->material_names[id][name_length] = '\0';
    }
    reg->material_count++;
    return true;
}

/* ===================================================================
 *  Key-value line parser (value-preserving variant)
 * =================================================================== */

/**
 * parse_key_val() — Split a line into key=value, preserving spaces in the value
 *
 * Unlike the simpler strtok-based approach used in load_palette/load_material,
 * this version uses strtok(NULL, "") for the value, which captures everything
 * after the '=' (including spaces).  It then trims only trailing \n / \r.
 *
 * This is needed for decals/sprite where values like pattern rows may contain
 * meaningful spaces.
 *
 * @param line  The input line (will be modified by strtok)
 * @param key   Output: pointer to the key part (within the modified line buffer)
 * @param val   Output: pointer to the value part (within the modified line buffer)
 */
static void parse_key_val(char *line, char **key, char **val) {
    *key = strtok(line, "=");
    *val = strtok(NULL, "");       /* Grab everything after the first '=' */
    if (*key) trim_string(*key);   /* Trim whitespace from key */
    if (*val) {
        /* Only strip trailing newline/carriage-return from the value —
         * NOT regular spaces, since those might be part of the data. */
        int len = (int)strlen(*val);
        if (len == 0) return;
        char *end = *val + len - 1;
        while(end >= *val && (*end == '\n' || *end == '\r')) {
            *end = '\0';
            end--;
        }
    }
}

/* ===================================================================
 *  Decal loading
 * =================================================================== */

/**
 * load_decal() — Load a decal definition and add it to the world
 *
 * Decals are surface decorations (wall posters, floor markings, ceiling
 * textures) that are rendered as part of the 3D scene.
 *
 * The file format supports two modes:
 *
 *   A) Structured key-value mode:
 *      Key-value header lines define properties (surface, x, y, z, map_x,
 *      map_y, side, u, v, width, height, depth, rotation, pattern_cols,
 *      pattern_rows, default_material), followed by explicit pattern_<row>
 *      and material_<row> lines for each row:
 *
 *        pattern_cols=3
 *        pattern_rows=2
 *        pattern_0=ABC
 *        pattern_1=DEF
 *        material_0=1,1,2
 *        material_1=2,1,1
 *
 *   B) Inline-art mode:
 *      Header ends with "art=" on its own line; subsequent lines contain
 *      literal ASCII characters.  Each character becomes a glyph, and all
 *      cells use the default_material.  The number of rows and columns is
 *      determined by pattern_rows/pattern_cols in the header; rows are
 *      read line by line from the file after "art=".
 *
 * @param world     WorldState to add the decal into
 * @param filepath  Full path to the decal definition file
 * @return          true on success, false if the file could not be opened
 */
static bool load_decal(WorldState *world, const char *filepath) {
    Decal *decal = decal_load_from_file(filepath);
    if (!decal) return false;
    WorldInsertResult result = world_add_decal(world, *decal);
    if (result == WORLD_INSERT_OK) decal->pattern = NULL;
    decal_free(decal);
    return result == WORLD_INSERT_OK;
}

static bool load_decal_pattern(AssetRegistry *reg, int id,
                               const char *filepath) {
    Decal *decal = decal_load_from_file(filepath);
    bool stored;
    if (!decal) return false;
    stored = asset_registry_set_decal_pattern(reg, id, decal->pattern_cols,
                                               decal->pattern_rows,
                                               decal->pattern);
    decal_free(decal);
    return stored;
}

/* ===================================================================
 *  Light loading
 * =================================================================== */

/**
 * load_light() — Load a point light definition and add it to the world
 *
 * Lights are 2D point light sources with position, colour, intensity
 * (negative values produce "darkness" / anti-light), and a radius that
 * controls how far the light propagates.
 *
 * @param world     WorldState to add the light into
 * @param filepath  Full path to the light definition file
 * @return          true on success, false if the file could not be opened
 */
static bool load_light(WorldState *world, const char *filepath) {
    bool valid = true;
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

    /* Reasonable defaults */
    double x = 0, y = 0, intensity = 1.0, radius = 4.0;
    SDL_Color color = {255, 255, 255, 255};

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *key = strtok(line, "=");
        char *val = strtok(NULL, "=");
        if (key && val) {
            trim_string(val);
            if (strcmp(key, "x") == 0 &&
                !parse_finite_double(val, -INFINITY, true, &x)) valid = false;
            else if (strcmp(key, "y") == 0 &&
                     !parse_finite_double(val, -INFINITY, true, &y)) valid = false;
            else if (strcmp(key, "color") == 0) parse_color(val, &color);
            else if (strcmp(key, "intensity") == 0 &&
                     !parse_finite_double(val, -INFINITY, true, &intensity)) valid = false;
            else if (strcmp(key, "radius") == 0 &&
                     !parse_finite_double(val, 0.0, false, &radius)) valid = false;
        }
    }
    if (ferror(f)) valid = false;
    if (fclose(f) != 0) valid = false;
    if (!valid) return false;
    
    return world_add_light(world, x, y, color, intensity, radius) == WORLD_INSERT_OK;
}

/* ===================================================================
 *  Sprite loading
 * =================================================================== */

/**
 * load_sprite() — Load a sprite asset and register it
 *
 * Sprite assets are 2D glyph/material patterns intended for billboard-style
 * rendering (for pickup items or enemy sprites).  They are defined
 * using the same pattern_<row>/material_<row> scheme as decals.
 *
 * Unlike decals, sprites are stored in the AssetRegistry (indexed by
 * sprite ID) rather than placed directly into the world.  World sprite
 * instances reference them by ID and the world-overlay pass renders them as
 * decorative camera-facing billboards.
 *
 * @param reg       AssetRegistry to store the sprite into
 * @param id        Sprite ID (1–255)
 * @param filepath  Full path to the sprite definition file
 * @return          true on success, false if the file could not be opened
 */
static bool load_sprite(AssetRegistry *reg, int id, const char *filepath) {
    bool valid = true;
    FILE *f = fopen(filepath, "r");
    if (!reg || id < 1 || id > 255 || !f) return false;

    SpriteAsset s;
    memset(&s, 0, sizeof(SpriteAsset));
    s.cols = 1;          /* Default to 1×1 */
    s.rows = 1;
    int default_material = 1;

    /* Pattern and material buffers (up to 32 rows) */
    char p_buf[SPRITE_PATTERN_MAX_ROWS][SPRITE_PATTERN_MAX_COLS + 1] = {0};
    char m_buf[SPRITE_PATTERN_MAX_ROWS][SPRITE_PATTERN_MAX_COLS + 1] = {0};

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *key = NULL;
        char *val = NULL;
        parse_key_val(line, &key, &val);
        if (key && val) {
            if (strcmp(key, "cols") == 0) {
                if (!parse_bounded_int(val, 1, SPRITE_PATTERN_MAX_COLS, &s.cols)) valid = false;
            }
            else if (strcmp(key, "rows") == 0) {
                if (!parse_bounded_int(val, 1, SPRITE_PATTERN_MAX_ROWS, &s.rows)) valid = false;
            }
            else if (strcmp(key, "default_material") == 0) {
                if (!parse_bounded_int(val, 0, ASSET_ID_MAX, &default_material)) valid = false;
            }
            else if (strncmp(key, "pattern_", 8) == 0) {
                int r;
                if (parse_bounded_int(key + 8, 0, SPRITE_PATTERN_MAX_ROWS - 1, &r)) {
                    strncpy(p_buf[r], val, 255);
                } else valid = false;
            }
            else if (strncmp(key, "material_", 9) == 0) {
                int r;
                if (parse_bounded_int(key + 9, 0, SPRITE_PATTERN_MAX_ROWS - 1, &r)) {
                    strncpy(m_buf[r], val, 255);
                } else valid = false;
            }
        }
    }
    if (ferror(f)) valid = false;
    if (fclose(f) != 0) valid = false;
    if (!valid) return false;
    
    /* Allocate pattern array */
    size_t cells;
    size_t bytes;
    if (!checked_size_2d(s.cols, s.rows, &cells) ||
        !checked_size_bytes(cells, sizeof(PatternCell), &bytes)) return false;
    (void)bytes;
    s.pattern = calloc(cells, sizeof(PatternCell));
    if (!s.pattern) return false;
    for (int r = 0; r < s.rows; r++) {
            /* Parse material IDs from the comma-separated material row */
            int mats[256];
            for (int i = 0; i < 256; i++) mats[i] = default_material;
            if (m_buf[r][0] != '\0') {
                if (!parse_material_row(m_buf[r], s.cols, mats)) {
                    free(s.pattern);
                    return false;
                }
            }
            /* Assign glyphs and material IDs */
            for (int c = 0; c < s.cols; c++) {
                char glyph = ' ';
                if (c < (int)strlen(p_buf[r])) glyph = p_buf[r][c];
                s.pattern[r * s.cols + c].glyph = glyph;
                s.pattern[r * s.cols + c].material_id = (uint16_t)mats[c];
            }
    }
    
    free(reg->sprites[id].pattern);
    reg->sprites[id] = s;             /* Store directly (struct copy) into registry */
    return true;
}

/* ===================================================================
 *  Public API — materials directory scan
 * =================================================================== */

/**
 * asset_loader_load_materials() — Load all *.txt files from a materials directory
 *
 * Uses a two-pass algorithm for deterministic ID assignment:
 *
 *   Pass 1 — Numeric filenames (e.g. "1.txt", "12.txt"):
 *     The numeric basename is used directly as the material ID.  These IDs
 *     are locked in before any named files are processed.
 *
 *   Pass 2 — Named filenames (e.g. "stone_brick.txt"):
 *     If the file contains an "id=<n>" field, that slot is requested.
 *     If the slot is already occupied, the file is skipped (warning to stderr).
 *     If no id= field is present, the first free slot (lowest unoccupied ID
 *     in 1..65535) is assigned.
 *
 * Both passes sort their file lists alphabetically before loading, so the
 * result is deterministic regardless of the directory enumeration order.
 *
 * Falls back to the legacy contiguous numeric probe if opendir() fails.
 *
 * @param reg           AssetRegistry to populate
 * @param materials_dir Full path to the materials directory (e.g. "assets/materials")
 */
void asset_loader_load_materials(AssetRegistry *reg, const char *materials_dir) {
    DIR *dir = opendir(materials_dir);
    if (!dir) {
        /* Directory missing or unreadable — fall back to numeric probe */
        char filepath[512];
        for (int i = 1; i <= ASSET_ID_MAX; i++) {
            snprintf(filepath, sizeof(filepath), "%s/%d.txt", materials_dir, i);
            if (!load_material(reg, i, filepath)) {
                if (i > 10) break;
            }
        }
        return;
    }

    /* Collect filenames that end in ".txt", split into numeric vs named. */
    char **numeric = NULL;
    size_t n_num = 0U, numeric_capacity = 0U;
    char **named = NULL;
    size_t n_named = 0U, named_capacity = 0U;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        /* Must end in ".txt" (at least 5 chars: "x.txt") */
        if (len < 5 || strcmp(name + len - 4, ".txt") != 0) continue;
        if (len >= 256U) continue;  /* Skip unreasonably long names */

        /* Extract basename (filename without ".txt") */
        size_t baselen = len - 4;
        if (baselen >= 64) continue;

        char base[64];
        memcpy(base, name, baselen);
        base[baselen] = '\0';

        if (basename_is_numeric(base)) {
            if (!append_name(&numeric, &n_num, &numeric_capacity, name)) break;
        } else {
            if (!append_name(&named, &n_named, &named_capacity, name)) break;
        }
    }
    closedir(dir);

    /* Sort both lists alphabetically for deterministic ordering. */
    if (n_num > 1U) qsort(numeric, n_num, sizeof(*numeric), qsort_str_cmp);
    if (n_named > 1U) qsort(named, n_named, sizeof(*named), qsort_str_cmp);

    char filepath[512];

    /* --- Pass 1: numeric files — ID comes from the basename integer --- */
    for (size_t i = 0U; i < n_num; i++) {
        size_t len     = strlen(numeric[i]);
        size_t baselen = len - 4;
        char base[64];
        memcpy(base, numeric[i], baselen);
        base[baselen] = '\0';

        int id;
        if (parse_bounded_int(base, 1, ASSET_ID_MAX, &id) &&
            strlen(materials_dir) + 1 + strlen(numeric[i]) < sizeof(filepath)) {
            memcpy(filepath, materials_dir, strlen(materials_dir));
            filepath[strlen(materials_dir)] = '/';
            strcpy(filepath + strlen(materials_dir) + 1, numeric[i]);
            load_material(reg, id, filepath);
        }
    }

    /* --- Pass 2: named files — ID from id= field or first free slot --- */
    for (size_t i = 0U; i < n_named; i++) {
        size_t len     = strlen(named[i]);
        size_t baselen = len - 4;
        char base[64];
        memcpy(base, named[i], baselen);
        base[baselen] = '\0';

        if (strlen(materials_dir) + 1 + strlen(named[i]) < sizeof(filepath)) {
            memcpy(filepath, materials_dir, strlen(materials_dir));
            filepath[strlen(materials_dir)] = '/';
            strcpy(filepath + strlen(materials_dir) + 1, named[i]);
            load_named_material(reg, filepath, base);
        }
    }

    free_name_list(numeric, n_num);
    free_name_list(named, n_named);
}

typedef bool (*NumericAssetLoader)(AssetRegistry *, int, const char *);

static void load_numeric_asset_directory(AssetRegistry *reg, const char *directory,
                                         NumericAssetLoader loader) {
    DIR *dir = opendir(directory);
    struct dirent *entry;
    if (!dir) return;
    while ((entry = readdir(dir)) != NULL) {
        size_t length = strlen(entry->d_name);
        char base[16];
        char canonical[24];
        char path[512];
        int id;
        if (length < 5U || length >= sizeof(base) + 4U ||
            strcmp(entry->d_name + length - 4U, ".txt") != 0) continue;
        memcpy(base, entry->d_name, length - 4U);
        base[length - 4U] = '\0';
        if (!parse_bounded_int(base, 1, ASSET_ID_MAX, &id)) continue;
        if (snprintf(canonical, sizeof(canonical), "%d.txt", id) < 0 ||
            strcmp(canonical, entry->d_name) != 0) continue;
        if (snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name) < 0 ||
            strlen(path) >= sizeof(path)) continue;
        (void)loader(reg, id, path);
    }
    closedir(dir);
}

/* ===================================================================
 *  Public API — bulk asset loading
 * =================================================================== */

/**
 * asset_loader_load_registry() — Load all generic assets from disk
 *
 * Enumerates existing palette, material, and decal files across IDs 1–65535.
 * Sprite loading intentionally remains the legacy contiguous 1–255 probe.
 *
 * @param reg       AssetRegistry to populate
 * @param base_path Root directory for assets (e.g. "assets")
 */
bool asset_loader_load_registry(AssetRegistry *reg, const char *base_path) {
    char filepath[512];
    char directory[512];
    DIR *root;
    if (!reg || !reg->palettes || !reg->materials || !reg->decal_patterns ||
        !reg->material_names || !base_path || base_path[0] == '\0') return false;
    root = opendir(base_path);
    if (!root) return false;
    if (closedir(root) != 0) return false;
    
    /* ---- Load existing palette files (IDs 1 to 65535) ---- */
    snprintf(directory, sizeof(directory), "%s/palettes", base_path);
    load_numeric_asset_directory(reg, directory, load_palette);
    
    /* ---- Load materials via two-pass directory scan ---- */
    {
        char mat_dir[512];
        snprintf(mat_dir, sizeof(mat_dir), "%s/materials", base_path);
        asset_loader_load_materials(reg, mat_dir);
    }
    
    /* ---- Load sprites (IDs 1 to 255) ---- */
    for (int i = 1; i < (int)SPRITE_ID_CAPACITY; i++) {
        snprintf(filepath, sizeof(filepath), "%s/sprites/%d.txt", base_path, i);
        if (!load_sprite(reg, i, filepath)) {
            if (i > 10) break;
        }
    }

    /* ---- Load existing reusable decal patterns (IDs 1 to 65535) ---- */
    snprintf(directory, sizeof(directory), "%s/decals", base_path);
    load_numeric_asset_directory(reg, directory, load_decal_pattern);
    (void)asset_registry_bump_generation(reg);
    return true;
}

/**
 * asset_loader_load_map_data() — Load a map and its associated world objects
 *
 * This function:
 *   1. Reads the full map text file into memory
 *   2. Parses it via map_load_from_string() to produce a Map structure
 *   3. Loads all decals (IDs 1–255) from assets/decals/
 *   4. Loads all lights (IDs 1–255) from assets/lights/
 *
 * Note: In a more advanced system, decals and lights would be linked
 * directly to specific maps.  For now, all decals and lights defined in
 * the global assets directory are loaded regardless of which map is loaded.
 *
 * @param world     WorldState to populate with lights and decals
 * @param base_path Root asset directory (e.g. "assets")
 * @param map_id    Numeric ID of the map to load
 * @return          Pointer to the loaded Map, or NULL on failure
 */
Map* asset_loader_load_map_data(WorldState *world, const char *base_path, int map_id) {
    char filepath[512];
    int path_length;
    if (!world || !base_path || map_id < 0) return NULL;
    path_length = snprintf(filepath, sizeof(filepath), "%s/maps/%d.txt", base_path, map_id);
    if (path_length < 0 || (size_t)path_length >= sizeof(filepath)) return NULL;
    
    /* ---- Read entire map file into a heap-allocated buffer ---- */
    FILE *f = fopen(filepath, "r");
    if (!f) return NULL;
    
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long fsize = ftell(f);
    if (fsize < 0 || (unsigned long)fsize > MAP_FILE_MAX_BYTES ||
        fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    
    size_t file_size = (size_t)fsize;
    char *map_str = malloc(file_size + 1U);
    if (!map_str) {
        fclose(f);
        return NULL;
    }
    if (file_size > 0 && fread(map_str, 1, file_size, f) != file_size) {
        free(map_str);
        fclose(f);
        return NULL;
    }
    if (fclose(f) != 0) {
        free(map_str);
        return NULL;
    }
    map_str[file_size] = '\0';
    
    /* Parse the digit-grid into a Map struct */
    Map *map = map_load_from_string(map_str);
    free(map_str);
    if (!map) return NULL;
    
    /* ---- Load decals associated with this world ---- */
    for (int i = 1; i < 256; i++) {
        snprintf(filepath, sizeof(filepath), "%s/decals/%d.txt", base_path, i);
        if (!load_decal(world, filepath)) {
            if (i > 10) break;
        }
    }
    
    /* ---- Load lights associated with this world ---- */
    for (int i = 1; i < 256; i++) {
        snprintf(filepath, sizeof(filepath), "%s/lights/%d.txt", base_path, i);
        if (!load_light(world, filepath)) {
            if (i > 10) break;
        }
    }
    
    return map;
}
