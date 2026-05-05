#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

typedef struct {
    int window_width;
    int window_height;
    int grid_width;
    int grid_height;
    int cell_width;
    int cell_height;
    int target_fps;
    
    double ambient_light;
    double light_bounce_attenuation;
    double light_falloff_default;
    
    double raycast_max_distance;
    double side_shadow_attenuation;
    
    int default_material_id;
    int default_palette_id;
    
    bool debug_display_enabled;
} EngineConfig;

// Initialize with hardcoded defaults
void config_init_defaults(void);

// Load from a key=value file, overriding defaults
bool config_load_from_file(const char *filepath);

// Get the current configuration (read-only pointer)
const EngineConfig* config_get(void);

// Set configuration (for programmatic overrides)
void config_set(const EngineConfig *new_config);

typedef enum {
    RUN_MODE_NORMAL,
    RUN_MODE_BENCHMARK_STRESS,
    RUN_MODE_STABILITY
} RunMode;

typedef enum {
    VISUAL_NORMAL,
    VISUAL_STRESS,
    VISUAL_RAYCAST
} VisualMode;

#endif
