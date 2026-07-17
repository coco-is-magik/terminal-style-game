/**
 * config.h — Engine configuration struct, enums, and API
 *
 * Defines the EngineConfig struct holding all tunable engine parameters
 * (window size, grid dimensions, lighting, raycasting, etc.), the
 * RunMode, VisualMode, and AppState enums, and the functions for loading
 * and accessing configuration.
 *
 * See config.c for the implementation.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

/**
 * EngineConfig — All tunable engine parameters
 *
 * Organised into logical groups:
 *   - Window & grid
 *   - Lighting
 *   - Raycasting
 *   - Default assets
 *   - Debug options
 */
typedef struct {
    /* ---- Window & grid ---- */
    int window_width;          /* Window width in screen pixels (default 1920) */
    int window_height;         /* Window height in screen pixels (default 1080) */
    int grid_width;            /* Number of glyph columns (default 260) */
    int grid_height;           /* Number of glyph rows (default 160) */
    int cell_width;            /* Glyph width in pixels (default 8) */
    int cell_height;           /* Glyph height in pixels (default 8) */
    int target_fps;            /* Target frames per second (default 120) */

    /* ---- Lighting ---- */
    double ambient_light;             /* Minimum light level (default 0.2 = 20%) */
    double light_bounce_attenuation;  /* Light bleed through obstacles (default 0.2) */
    double light_falloff_default;     /* Parsed setting; currently unused by lighting.c */

    /* ---- Raycasting ---- */
    double raycast_max_distance;      /* Max ray distance in cells (default 20.0) */
    double side_shadow_attenuation;   /* Dimming for EW walls (default 0.6) */

    /* ---- Default assets ---- */
    int default_material_id;   /* Material for non-digit map chars (default 1) */
    int default_palette_id;    /* Parsed setting; currently unused by asset lookup */

    /* ---- Asset Designer ---- */
    int asset_canvas_cols;   /* Canvas width for the decal editor (default 20) */
    int asset_canvas_rows;   /* Canvas height for the decal editor (default 12) */

    /* ---- Debug ---- */
    bool debug_display_enabled;       /* Show HUD overlay (default true) */
} EngineConfig;

/* ---- Configuration API ---- */

/**
 * config_init_defaults() — Populate the global config with hard-coded defaults
 *
 * Sets g_config to the values documented in config.c.  Safe to call multiple
 * times; this is the first step before config_load_from_file() or config_set().
 */
void config_init_defaults(void);

/**
 * config_load_from_file() — Load INI-style overrides from a file
 *
 * Opens filepath and parses key=value lines into the global config.
 * Unknown keys are ignored.  If the file cannot be opened, the function
 * returns false and the existing config values remain unchanged.
 *
 * @param filepath  Path to the configuration file (e.g. "config.ini")
 * @return          true if the file was read, false if it could not be opened
 */
bool config_load_from_file(const char *filepath);

/**
 * config_get() — Return a read-only pointer to the global EngineConfig
 *
 * @return  const pointer to the active global config
 */
const EngineConfig* config_get(void);

/**
 * config_set() — Overwrite the entire global config with a copy
 *
 * Used by tests and headless modes to force settings that differ from
 * defaults and config.ini.  NULL is a no-op.
 *
 * @param new_config  Pointer to the EngineConfig to copy (may be NULL)
 */
void config_set(const EngineConfig *new_config);

/* ---- Run mode enum ---- */

typedef enum {
    RUN_MODE_NORMAL,              /* Interactive play mode */
    RUN_MODE_BENCHMARK_STRESS,    /* Timed stress-test, prints JSON results */
    RUN_MODE_BENCHMARK_RAYCAST,   /* Timed raycast benchmark, prints JSON results */
    RUN_MODE_BENCHMARK_SCENARIO,  /* Deterministic scenario benchmark (fixed frames) */
    RUN_MODE_BENCHMARK_LIGHTING,  /* Timed lighting benchmark, prints JSON results */
    RUN_MODE_STABILITY            /* Stability test (detects leaks/crashes) */
} RunMode;

/* ---- Visual mode enum ---- */

typedef enum {
    VISUAL_NORMAL,       /* Animated sine-wave pattern (debug/testing) */
    VISUAL_STRESS,       /* Chaotic CPU-intensive pattern (benchmark) */
    VISUAL_RAYCAST       /* Full 3D raycasted world rendering */
} VisualMode;

/* ---- App state enum ---- */

typedef enum {
    APP_STATE_MAIN_MENU,         /* Main menu: button selection */
    APP_STATE_PLAYING,           /* In-game: raycast world rendering */
    APP_STATE_EDITOR,            /* Map editor (placeholder) */
    APP_STATE_LIVE_EDITOR,       /* Combined decal/material editor with live preview */
    APP_STATE_ASSET_DESIGNER,    /* Decal canvas editor */
    APP_STATE_MATERIAL_DESIGNER  /* Material field editor */
} AppState;

#endif /* CONFIG_H */
