/**
 * config.c — Engine configuration singleton
 *
 * This file holds and manages the single global EngineConfig instance that
 * controls all tunable engine parameters: window size, grid dimensions,
 * cell (glyph) size, target FPS, lighting parameters, raycasting settings,
 * material/palette defaults, and debug overlay toggles.
 *
 * The configuration follows a layered approach:
 *   1. Hard-coded defaults are set by config_init_defaults()
 *   2. These may be overridden by reading config.ini via config_load_from_file()
 *   3. Programmatic overrides can be applied at any time via config_set()
 *
 * There is only ONE global config instance (g_config), accessed via the
 * config_get() function.  This avoids passing a config pointer through
 * every function in the codebase.
 */

#include "config.h"        /* EngineConfig struct, function declarations,
                              RunMode and VisualMode enums */
#include <stdio.h>          /* FILE, fopen(), fgets(), fclose() */
#include <stdlib.h>         /* atoi(), atof() */
#include <string.h>         /* strcmp(), strchr(), strtok() */

/* ===================================================================
 *  Global singleton — static linkage, so it's private to this file
 * =================================================================== */

static EngineConfig g_config;      /* The one and only engine config */

/* ===================================================================
 *  Initialisation
 * =================================================================== */

/**
 * config_init_defaults() — Populate g_config with sensible default values
 *
 * These defaults are chosen to give a reasonable out-of-the-box experience
 * on a typical 1080p display:
 *
 *   Display:
 *     - 1920×1080 window (full HD)
 *     - 260×160 glyph grid (each glyph = an 8×8 pixel cell → 2080×1280,
 *       larger than the default window; SDL logical presentation scales/letterboxes it)
 *     - 120 FPS target (smooth on high-refresh monitors)
 *
 *   Lighting:
 *     - 20% ambient light (so nothing is ever completely black)
 *     - 20% light bounce attenuation (shadowed light bleed)
 *     - 1.0 default falloff exponent (parsed for future use; currently unused)
 *
 *   Raycasting:
 *     - 20 cells max ray distance (good for most map sizes)
 *     - 60% side shadow attenuation (walls lit from the side are dimmer)
 *
 *   Assets:
 *     - Default material ID = 1 (the first material in assets/materials/1.txt)
 *     - Default palette ID = 1  (parsed for future use; currently unused)
 *
 *   Debug:
 *     - Debug overlay enabled by default (shows FPS, frame times, etc.)
 */
void config_init_defaults(void) {
    /* ---- Window & grid ---- */
    g_config.window_width  = 1920;
    g_config.window_height = 1080;
    g_config.grid_width    = 260;
    g_config.grid_height   = 160;
    g_config.cell_width    = 8;
    g_config.cell_height   = 8;
    g_config.target_fps    = 120;

    /* ---- Lighting ---- */
    g_config.ambient_light            = 0.2;
    g_config.light_bounce_attenuation = 0.2;
    g_config.light_falloff_default    = 1.0;

    /* ---- Raycasting ---- */
    g_config.raycast_max_distance     = 20.0;
    g_config.side_shadow_attenuation  = 0.6;

    /* ---- Assets ---- */
    g_config.default_material_id = 1;
    g_config.default_palette_id  = 1;

    /* ---- Asset Designer ---- */
    g_config.asset_canvas_cols = 20;
    g_config.asset_canvas_rows = 12;

    /* ---- Debug ---- */
    g_config.debug_display_enabled = true;
}

/* ===================================================================
 *  Configuration file parser (private helper)
 * =================================================================== */

/**
 * parse_line() — Parse a single "key=value" line from config.ini
 *
 * The line parser handles:
 *   - Comments: anything after a '#' character is ignored (like .ini files)
 *   - Whitespace trimming: leading and trailing spaces/tabs/newlines are
 *     stripped from both the key and the value
 *   - Blanks and comment-only lines: silently skipped (since strtok will
 *     return NULL for missing '=' or key/value)
 *
 * All recognised keys are case-sensitive and must match exactly.
 * Unknown keys are silently ignored (no error message).
 *
 * @param line  Raw line from the config file (will be modified by strtok)
 */
static void parse_line(char *line) {
    /* Strip inline comments — anything from '#' to end-of-line */
    char *comment = strchr(line, '#');
    if (comment) *comment = '\0';

    /* Split on '=': key is the left part, value is the right part */
    char *key = strtok(line, "=");
    char *val = strtok(NULL, "=");

    if (!key || !val) return;          /* Malformed line — skip */

    /* ---- Trim whitespace from key ---- */
    while (*key == ' ' || *key == '\t') key++;
    char *end = key + strlen(key) - 1;
    while (end >= key && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }

    /* ---- Trim whitespace from value ---- */
    while (*val == ' ' || *val == '\t') val++;
    end = val + strlen(val) - 1;
    while (end >= val && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }

    /* ---- Match key and assign ---- */

    /* --- Window & grid (integer values) --- */
    if      (strcmp(key, "window_width")  == 0) g_config.window_width  = atoi(val);
    else if (strcmp(key, "window_height") == 0) g_config.window_height = atoi(val);
    else if (strcmp(key, "grid_width")    == 0) g_config.grid_width    = atoi(val);
    else if (strcmp(key, "grid_height")   == 0) g_config.grid_height   = atoi(val);
    else if (strcmp(key, "cell_width")    == 0) g_config.cell_width    = atoi(val);
    else if (strcmp(key, "cell_height")   == 0) g_config.cell_height   = atoi(val);
    else if (strcmp(key, "target_fps")    == 0) g_config.target_fps    = atoi(val);

    /* --- Lighting (floating-point values) --- */
    else if (strcmp(key, "ambient_light")            == 0) g_config.ambient_light            = atof(val);
    else if (strcmp(key, "light_bounce_attenuation") == 0) g_config.light_bounce_attenuation = atof(val);
    else if (strcmp(key, "light_falloff_default")    == 0) g_config.light_falloff_default    = atof(val);

    /* --- Raycasting (floating-point values) --- */
    else if (strcmp(key, "raycast_max_distance")    == 0) g_config.raycast_max_distance    = atof(val);
    else if (strcmp(key, "side_shadow_attenuation") == 0) g_config.side_shadow_attenuation = atof(val);

    /* --- Default assets (integer values) --- */
    else if (strcmp(key, "default_material_id") == 0) g_config.default_material_id = atoi(val);
    else if (strcmp(key, "default_palette_id")  == 0) g_config.default_palette_id  = atoi(val);

    /* --- Asset Designer canvas size (integer values) --- */
    else if (strcmp(key, "asset_canvas_cols") == 0) g_config.asset_canvas_cols = atoi(val);
    else if (strcmp(key, "asset_canvas_rows") == 0) g_config.asset_canvas_rows = atoi(val);

    /* --- Debug toggle (0 = false, anything else = true) --- */
    else if (strcmp(key, "debug_display_enabled") == 0) g_config.debug_display_enabled = (atoi(val) != 0);
    /* Unknown keys are silently ignored */
}

/* ===================================================================
 *  Public API functions
 * =================================================================== */

/**
 * config_load_from_file() — Load configuration overrides from a file
 *
 * Opens the given file and reads it line by line, feeding each line to
 * parse_line().  If the file cannot be opened (e.g. doesn't exist), the
 * function returns false and the defaults remain in place — this is NOT
 * treated as a fatal error.
 *
 * The file format is simple INI-style:
 *   key = value
 *   # comment lines start with #
 *
 * @param filepath  Path to the configuration file (e.g. "config.ini")
 * @return          true if the file was opened and read, false otherwise
 */
bool config_load_from_file(const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;               /* File doesn't exist — not an error */

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        parse_line(line);
    }

    fclose(f);
    return true;
}

/**
 * config_get() — Return a read-only pointer to the global EngineConfig
 *
 * Because the returned pointer is const-qualified, callers cannot modify
 * the configuration directly.  Use config_set() if a programmatic override
 * is needed.
 *
 * @return  const pointer to the global EngineConfig struct
 */
const EngineConfig* config_get(void) {
    return &g_config;
}

/**
 * config_set() — Programmatically overwrite the entire configuration
 *
 * This is used when a test harness or headless mode needs to force
 * specific settings that differ from both the defaults and config.ini.
 *
 * @param new_config  Pointer to the new EngineConfig to apply (NULL-safe)
 */
void config_set(const EngineConfig *new_config) {
    if (new_config) {
        g_config = *new_config;          /* Struct copy */
    }
}