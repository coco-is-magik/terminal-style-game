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
                                asset_loader_load_map_data() */
#include "map_loader.h"      /* map_load_from_string() — parses the digit-grid map format */
#include "config.h"          /* config_get() values are used indirectly by map loading */
#include <stdio.h>           /* FILE, fopen(), fgets(), fclose(), fprintf(), snprintf() */
#include <stdlib.h>          /* atoi(), atof(), malloc(), free(), calloc() */
#include <string.h>          /* strcmp(), strncmp(), strtok(), strncpy(), memset() */

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

    int pal_id = 0;
    char glyphs[5] = "    ";   /* Up to 4 glyphs + NUL; defaults to spaces */
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *key = strtok(line, "=");
        char *val = strtok(NULL, "=");
        if (key && val) {
            trim_string(val);
            if (strcmp(key, "palette") == 0) pal_id = atoi(val);
            else if (strcmp(key, "glyphs") == 0) {
                strncpy(glyphs, val, 4);
                glyphs[4] = '\0';
            }
        }
    }
    fclose(f);

    asset_registry_set_material(reg, id, pal_id, glyphs);
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
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

    Decal d;
    memset(&d, 0, sizeof(Decal));          /* Zero out everything */
    d.pattern_cols = 1;                    /* Default to 1×1 if not specified */
    d.pattern_rows = 1;
    int default_material = 1;

    /* Buffers to hold pattern rows (up to 64) and material rows */
    char p_buf[64][256] = {0};
    char m_buf[64][256] = {0};
    bool art_mode = false;                 /* Becomes true once we hit "art=" */

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        /* Check if this line signals the start of inline ASCII art */
        if (strncmp(line, "art=", 4) == 0 || strcmp(line, "art\n") == 0 || strcmp(line, "art\r\n") == 0) {
            art_mode = true;
            break;                         /* Exit parse loop — remaining lines are art */
        }

        char *key = NULL;
        char *val = NULL;
        parse_key_val(line, &key, &val);
        if (key && val) {
            /* --- Spatial properties --- */
            if (strcmp(key, "surface") == 0) d.surface = atoi(val);
            else if (strcmp(key, "x") == 0) d.x = atof(val);
            else if (strcmp(key, "y") == 0) d.y = atof(val);
            else if (strcmp(key, "z") == 0) d.z = atof(val);
            else if (strcmp(key, "map_x") == 0) d.map_x = atoi(val);
            else if (strcmp(key, "map_y") == 0) d.map_y = atoi(val);
            else if (strcmp(key, "side") == 0) d.side = atoi(val);
            else if (strcmp(key, "u") == 0) d.u = atof(val);
            else if (strcmp(key, "v") == 0) d.v = atof(val);
            else if (strcmp(key, "width") == 0) d.width = atof(val);
            else if (strcmp(key, "height") == 0) d.height = atof(val);
            else if (strcmp(key, "depth") == 0) d.depth = atof(val);
            else if (strcmp(key, "rotation") == 0) d.rotation = atof(val);

            /* --- Pattern layout --- */
            else if (strcmp(key, "pattern_cols") == 0) d.pattern_cols = atoi(val);
            else if (strcmp(key, "pattern_rows") == 0) d.pattern_rows = atoi(val);
            else if (strcmp(key, "default_material") == 0) default_material = atoi(val);

            /* --- Per-row pattern data (key formats: pattern_0, pattern_1, ...) --- */
            else if (strncmp(key, "pattern_", 8) == 0) {
                int r = atoi(key + 8);     /* Extract row index from key suffix */
                if (r >= 0 && r < 64) {
                    strncpy(p_buf[r], val, 255);
                }
            }
            /* --- Per-row material data (key formats: material_0, material_1, ...) --- */
            else if (strncmp(key, "material_", 9) == 0) {
                int r = atoi(key + 9);
                if (r >= 0 && r < 64) {
                    strncpy(m_buf[r], val, 255);
                }
            }
        }
    }
    
    /* Allocate the pattern array (calloc zeros out memory) */
    d.pattern = calloc(d.pattern_cols * d.pattern_rows, sizeof(PatternCell));
    if (d.pattern) {
        bool success = true;
        if (art_mode) {
            /* --- Inline ASCII art mode --- */
            /* Read exactly pattern_rows lines from the file after "art=" */
            for (int r = 0; r < d.pattern_rows; r++) {
                if (!fgets(line, sizeof(line), f)) {
                    success = false;
                    break;
                }
                /* Strip trailing newline/carriage-return */
                int len = (int)strlen(line);
                while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
                    line[--len] = '\0';
                }
                /* Each line must be at least pattern_cols wide */
                if (len < d.pattern_cols) {
                    success = false;
                    break;
                }
                for (int c = 0; c < d.pattern_cols; c++) {
                    char g = line[c];
                    if (g == '\t') g = ' ';     /* Replace tabs with spaces */
                    d.pattern[r * d.pattern_cols + c].glyph = g;
                    d.pattern[r * d.pattern_cols + c].material_id = default_material;
                }
            }
        } else {
            /* --- Structured key-value mode --- */
            for (int r = 0; r < d.pattern_rows; r++) {
                /* Parse the comma-separated material IDs for this row */
                int mats[256];
                for (int i = 0; i < 256; i++) mats[i] = default_material;
                if (m_buf[r][0] != '\0') {
                    char *p = m_buf[r];
                    int c = 0;
                    while (*p && c < d.pattern_cols) {
                        mats[c++] = atoi(p);
                        /* Skip past current digit and the comma */
                        while (*p && *p != ',') p++;
                        if (*p == ',') p++;
                    }
                }
                /* Assign glyphs and material IDs from the parsed buffers */
                for (int c = 0; c < d.pattern_cols; c++) {
                    char glyph = ' ';
                    if (c < (int)strlen(p_buf[r])) glyph = p_buf[r][c];
                    d.pattern[r * d.pattern_cols + c].glyph = glyph;
                    d.pattern[r * d.pattern_cols + c].material_id = mats[c];
                }
            }
        }
        
        /* If something went wrong (e.g. mismatched dimensions), fill with error markers */
        if (!success) {
            fprintf(stderr, "Decal load failed: %s (dimensions mismatch)\n", filepath);
            for (int i = 0; i < d.pattern_cols * d.pattern_rows; i++) {
                d.pattern[i].glyph = '!';               /* Visual error indicator */
                d.pattern[i].material_id = default_material;
            }
        }
    }
    
    fclose(f);
    world_add_decal(world, d);     /* Copy decal into the world (pattern pointer is retained) */
    return true;
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
            if (strcmp(key, "x") == 0) x = atof(val);
            else if (strcmp(key, "y") == 0) y = atof(val);
            else if (strcmp(key, "color") == 0) parse_color(val, &color);
            else if (strcmp(key, "intensity") == 0) intensity = atof(val);
            else if (strcmp(key, "radius") == 0) radius = atof(val);
        }
    }
    fclose(f);
    
    world_add_light(world, x, y, color, intensity, radius);
    return true;
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
 * instances can reference them by ID, although generic sprite rendering is
 * not currently implemented in raycast_render().
 *
 * @param reg       AssetRegistry to store the sprite into
 * @param id        Sprite ID (1–255)
 * @param filepath  Full path to the sprite definition file
 * @return          true on success, false if the file could not be opened
 */
static bool load_sprite(AssetRegistry *reg, int id, const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

    SpriteAsset s;
    memset(&s, 0, sizeof(SpriteAsset));
    s.cols = 1;          /* Default to 1×1 */
    s.rows = 1;
    int default_material = 1;

    /* Pattern and material buffers (up to 32 rows) */
    char p_buf[32][256] = {0};
    char m_buf[32][256] = {0};

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *key = NULL;
        char *val = NULL;
        parse_key_val(line, &key, &val);
        if (key && val) {
            if (strcmp(key, "cols") == 0) s.cols = atoi(val);
            else if (strcmp(key, "rows") == 0) s.rows = atoi(val);
            else if (strcmp(key, "default_material") == 0) default_material = atoi(val);
            else if (strncmp(key, "pattern_", 8) == 0) {
                int r = atoi(key + 8);
                if (r >= 0 && r < 32) {
                    strncpy(p_buf[r], val, 255);
                }
            }
            else if (strncmp(key, "material_", 9) == 0) {
                int r = atoi(key + 9);
                if (r >= 0 && r < 32) {
                    strncpy(m_buf[r], val, 255);
                }
            }
        }
    }
    fclose(f);
    
    /* Allocate pattern array */
    s.pattern = calloc(s.cols * s.rows, sizeof(PatternCell));
    if (s.pattern) {
        for (int r = 0; r < s.rows; r++) {
            /* Parse material IDs from the comma-separated material row */
            int mats[256];
            for (int i = 0; i < 256; i++) mats[i] = default_material;
            if (m_buf[r][0] != '\0') {
                char *p = m_buf[r];
                int c = 0;
                while (*p && c < s.cols) {
                    mats[c++] = atoi(p);
                    while (*p && *p != ',') p++;
                    if (*p == ',') p++;
                }
            }
            /* Assign glyphs and material IDs */
            for (int c = 0; c < s.cols; c++) {
                char glyph = ' ';
                if (c < (int)strlen(p_buf[r])) glyph = p_buf[r][c];
                s.pattern[r * s.cols + c].glyph = glyph;
                s.pattern[r * s.cols + c].material_id = mats[c];
            }
        }
    }
    
    reg->sprites[id] = s;             /* Store directly (struct copy) into registry */
    return true;
}

/* ===================================================================
 *  Public API — bulk asset loading
 * =================================================================== */

/**
 * asset_loader_load_registry() — Load all generic assets from disk
 *
 * Iterates through palette IDs (1–255), material IDs (1–255), and sprite
 * IDs (1–255), attempting to load each one.  To avoid stat() calls for
 * files that don't exist, we simply try to fopen() each and treat a NULL
 * return as "file not found".  IDs 1–10 are always probed; after that,
 * the first missing file stops that asset-type scan to save time.
 *
 * @param reg       AssetRegistry to populate
 * @param base_path Root directory for assets (e.g. "assets")
 */
void asset_loader_load_registry(AssetRegistry *reg, const char *base_path) {
    char filepath[512];
    
    /* ---- Load palettes (IDs 1 to 255) ---- */
    for (int i = 1; i < 256; i++) {
        snprintf(filepath, sizeof(filepath), "%s/palettes/%d.txt", base_path, i);
        if (!load_palette(reg, i, filepath)) {
            /* IDs 1–10 are always probed; after that, stop at the first
             * missing file to avoid many unnecessary failed fopen() calls. */
            if (i > 10) break;
        }
    }
    
    /* ---- Load materials (IDs 1 to 255) ---- */
    for (int i = 1; i < 256; i++) {
        snprintf(filepath, sizeof(filepath), "%s/materials/%d.txt", base_path, i);
        if (!load_material(reg, i, filepath)) {
            if (i > 10) break;
        }
    }
    
    /* ---- Load sprites (IDs 1 to 255) ---- */
    for (int i = 1; i < 256; i++) {
        snprintf(filepath, sizeof(filepath), "%s/sprites/%d.txt", base_path, i);
        if (!load_sprite(reg, i, filepath)) {
            if (i > 10) break;
        }
    }
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
    snprintf(filepath, sizeof(filepath), "%s/maps/%d.txt", base_path, map_id);
    
    /* ---- Read entire map file into a heap-allocated buffer ---- */
    FILE *f = fopen(filepath, "r");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *map_str = malloc(fsize + 1);
    fread(map_str, fsize, 1, f);
    fclose(f);
    map_str[fsize] = 0;          /* NUL-terminate */
    
    /* Parse the digit-grid into a Map struct */
    Map *map = map_load_from_string(map_str);
    free(map_str);
    
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