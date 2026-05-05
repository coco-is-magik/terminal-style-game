#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EngineConfig g_config;

void config_init_defaults(void) {
    g_config.window_width = 1920;
    g_config.window_height = 1080;
    g_config.grid_width = 260;
    g_config.grid_height = 160;
    g_config.cell_width = 8;
    g_config.cell_height = 8;
    g_config.target_fps = 120;
    
    g_config.ambient_light = 0.2;
    g_config.light_bounce_attenuation = 0.2;
    g_config.light_falloff_default = 1.0;
    
    g_config.raycast_max_distance = 20.0;
    g_config.side_shadow_attenuation = 0.6;
    
    g_config.default_material_id = 1;
    g_config.default_palette_id = 1;
    
    g_config.debug_display_enabled = true;
}

static void parse_line(char *line) {
    char *comment = strchr(line, '#');
    if (comment) *comment = '\0';
    
    char *key = strtok(line, "=");
    char *val = strtok(NULL, "=");
    
    if (!key || !val) return;
    
    // Trim whitespace (basic)
    while(*key == ' ' || *key == '\t') key++;
    char *end = key + strlen(key) - 1;
    while(end >= key && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) { *end = '\0'; end--; }
    
    while(*val == ' ' || *val == '\t') val++;
    end = val + strlen(val) - 1;
    while(end >= val && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) { *end = '\0'; end--; }
    
    if (strcmp(key, "window_width") == 0) g_config.window_width = atoi(val);
    else if (strcmp(key, "window_height") == 0) g_config.window_height = atoi(val);
    else if (strcmp(key, "grid_width") == 0) g_config.grid_width = atoi(val);
    else if (strcmp(key, "grid_height") == 0) g_config.grid_height = atoi(val);
    else if (strcmp(key, "cell_width") == 0) g_config.cell_width = atoi(val);
    else if (strcmp(key, "cell_height") == 0) g_config.cell_height = atoi(val);
    else if (strcmp(key, "target_fps") == 0) g_config.target_fps = atoi(val);
    
    else if (strcmp(key, "ambient_light") == 0) g_config.ambient_light = atof(val);
    else if (strcmp(key, "light_bounce_attenuation") == 0) g_config.light_bounce_attenuation = atof(val);
    else if (strcmp(key, "light_falloff_default") == 0) g_config.light_falloff_default = atof(val);
    
    else if (strcmp(key, "raycast_max_distance") == 0) g_config.raycast_max_distance = atof(val);
    else if (strcmp(key, "side_shadow_attenuation") == 0) g_config.side_shadow_attenuation = atof(val);
    
    else if (strcmp(key, "default_material_id") == 0) g_config.default_material_id = atoi(val);
    else if (strcmp(key, "default_palette_id") == 0) g_config.default_palette_id = atoi(val);
    
    else if (strcmp(key, "debug_display_enabled") == 0) g_config.debug_display_enabled = (atoi(val) != 0);
}

bool config_load_from_file(const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        parse_line(line);
    }
    
    fclose(f);
    return true;
}

const EngineConfig* config_get(void) {
    return &g_config;
}

void config_set(const EngineConfig *new_config) {
    if (new_config) {
        g_config = *new_config;
    }
}
