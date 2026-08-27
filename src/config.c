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
#include "assets.h"
#include "scene_types.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===================================================================
 *  Global singleton — static linkage, so it's private to this file
 * =================================================================== */

static EngineConfig g_config;      /* The one and only engine config */

bool config_validate(const EngineConfig *config) {
    return config != NULL &&
           config->window_width > 0 && config->window_height > 0 &&
           config->grid_width > 0 && config->grid_height > 0 &&
           config->cell_width == 8 && config->cell_height == 8 &&
           config->target_fps > 0 &&
           isfinite(config->ambient_light) && config->ambient_light >= 0.0 &&
           config->ambient_light <= 1.0 &&
           isfinite(config->light_bounce_attenuation) &&
           config->light_bounce_attenuation >= 0.0 &&
           config->light_bounce_attenuation <= 1.0 &&
           isfinite(config->light_falloff_default) &&
           config->light_falloff_default >= SCENE_LIGHT_FALLOFF_MIN &&
           config->light_falloff_default <= SCENE_LIGHT_FALLOFF_MAX &&
           isfinite(config->raycast_max_distance) &&
           config->raycast_max_distance > 0.0 &&
           isfinite(config->side_shadow_attenuation) &&
           config->side_shadow_attenuation >= 0.0 &&
           config->side_shadow_attenuation <= 1.0 &&
           config->default_material_id >= 1 && config->default_material_id <= ASSET_ID_MAX &&
           config->default_palette_id >= 1 && config->default_palette_id <= ASSET_ID_MAX &&
           config->asset_canvas_cols > 0 && config->asset_canvas_rows > 0;
}

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
 *     - 1.0 default falloff exponent for newly authored lights
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
static char *trim(char *text) {
    char *end;
    while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') text++;
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\n' || end[-1] == '\r')) {
        *--end = '\0';
    }
    return text;
}

static bool parse_int_range(const char *text, int minimum, int maximum, int *out) {
    char *end = NULL;
    long value;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' ||
        value < minimum || value > maximum || value < INT_MIN || value > INT_MAX) {
        return false;
    }
    *out = (int)value;
    return true;
}

static bool parse_double_range(const char *text, double minimum, double maximum,
                               bool maximum_is_inclusive, double *out) {
    char *end = NULL;
    double value;
    errno = 0;
    value = strtod(text, &end);
    if (errno == ERANGE || end == text || *end != '\0' || !isfinite(value) ||
        value < minimum || (maximum_is_inclusive ? value > maximum : value >= maximum)) {
        return false;
    }
    *out = value;
    return true;
}

static bool parse_line(char *line, EngineConfig *candidate) {
    char *comment = strchr(line, '#');
    char *separator;
    char *key;
    char *val;
    int parsed_int;
    double parsed_double;

    if (comment) *comment = '\0';
    key = trim(line);
    if (*key == '\0') return true;
    separator = strchr(key, '=');
    if (!separator) return false;
    *separator = '\0';
    val = trim(separator + 1);
    key = trim(key);
    if (*key == '\0' || *val == '\0' || strchr(val, '=') != NULL) return false;

#define PARSE_POSITIVE_INT(name, field) \
    if (strcmp(key, name) == 0) { \
        if (!parse_int_range(val, 1, INT_MAX, &parsed_int)) return false; \
        candidate->field = parsed_int; \
        return true; \
    }
    PARSE_POSITIVE_INT("window_width", window_width)
    PARSE_POSITIVE_INT("window_height", window_height)
    PARSE_POSITIVE_INT("grid_width", grid_width)
    PARSE_POSITIVE_INT("grid_height", grid_height)
    PARSE_POSITIVE_INT("target_fps", target_fps)
    PARSE_POSITIVE_INT("asset_canvas_cols", asset_canvas_cols)
    PARSE_POSITIVE_INT("asset_canvas_rows", asset_canvas_rows)
#undef PARSE_POSITIVE_INT

    if (strcmp(key, "cell_width") == 0 || strcmp(key, "cell_height") == 0) {
        if (!parse_int_range(val, 8, 8, &parsed_int)) return false;
        if (key[5] == 'w') candidate->cell_width = parsed_int;
        else candidate->cell_height = parsed_int;
        return true;
    }
    if (strcmp(key, "default_material_id") == 0 || strcmp(key, "default_palette_id") == 0) {
        if (!parse_int_range(val, 1, ASSET_ID_MAX, &parsed_int)) return false;
        if (key[8] == 'm') candidate->default_material_id = parsed_int;
        else candidate->default_palette_id = parsed_int;
        return true;
    }
    if (strcmp(key, "debug_display_enabled") == 0) {
        if (!parse_int_range(val, 0, 1, &parsed_int)) return false;
        candidate->debug_display_enabled = parsed_int != 0;
        return true;
    }
    if (strcmp(key, "ambient_light") == 0 ||
        strcmp(key, "light_bounce_attenuation") == 0 ||
        strcmp(key, "side_shadow_attenuation") == 0) {
        if (!parse_double_range(val, 0.0, 1.0, true, &parsed_double)) return false;
        if (strcmp(key, "ambient_light") == 0) candidate->ambient_light = parsed_double;
        else if (strcmp(key, "light_bounce_attenuation") == 0) {
            candidate->light_bounce_attenuation = parsed_double;
        } else candidate->side_shadow_attenuation = parsed_double;
        return true;
    }
    if (strcmp(key, "light_falloff_default") == 0 ||
        strcmp(key, "raycast_max_distance") == 0) {
        if (!parse_double_range(val, 0.0, HUGE_VAL, false, &parsed_double) || parsed_double == 0.0) {
            return false;
        }
        if (strcmp(key, "light_falloff_default") == 0) {
            candidate->light_falloff_default = parsed_double;
        } else candidate->raycast_max_distance = parsed_double;
        return true;
    }
    return true;
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
    EngineConfig candidate;
    bool valid = true;
    char line[256];
    if (!filepath) return false;
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

    candidate = g_config;
    while (fgets(line, sizeof(line), f)) {
        size_t length = strlen(line);
        if (length > 0 && line[length - 1] != '\n' && !feof(f)) {
            valid = false;
            break;
        }
        if (!parse_line(line, &candidate)) {
            valid = false;
            break;
        }
    }
    if (ferror(f)) valid = false;
    if (fclose(f) != 0) valid = false;
    if (!valid || !config_validate(&candidate)) return false;
    g_config = candidate;
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
bool config_set(const EngineConfig *new_config) {
    if (!config_validate(new_config)) return false;
    g_config = *new_config;
    return true;
}