#include "asset_loader.h"
#include "map_loader.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void parse_color(const char *val, SDL_Color *color) {
    int r, g, b, a;
    if (sscanf(val, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
        color->r = (uint8_t)r;
        color->g = (uint8_t)g;
        color->b = (uint8_t)b;
        color->a = (uint8_t)a;
    }
}

static void trim_string(char *str) {
    char *end = str + strlen(str) - 1;
    while(end >= str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
}

static bool load_palette(AssetRegistry *reg, int id, const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

    SDL_Color near_col = {0,0,0,255}, mid_col = {0,0,0,255}, far_col = {0,0,0,255};
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *key = strtok(line, "=");
        char *val = strtok(NULL, "=");
        if (key && val) {
            trim_string(val);
            if (strcmp(key, "near") == 0) parse_color(val, &near_col);
            else if (strcmp(key, "mid") == 0) parse_color(val, &mid_col);
            else if (strcmp(key, "far") == 0) parse_color(val, &far_col);
        }
    }
    fclose(f);
    asset_registry_set_palette(reg, id, near_col, mid_col, far_col);
    return true;
}

static bool load_material(AssetRegistry *reg, int id, const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

    int pal_id = 0;
    char glyphs[5] = "    ";
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

static bool load_decal(WorldState *world, const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

    Decal d;
    memset(&d, 0, sizeof(Decal));
    char text_buf[256] = {0};

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *key = strtok(line, "=");
        char *val = strtok(NULL, ""); // Text can have spaces
        if (key && val) {
            trim_string(val);
            if (strcmp(key, "surface") == 0) d.surface = atoi(val);
            else if (strcmp(key, "x") == 0) d.x = atof(val);
            else if (strcmp(key, "y") == 0) d.y = atof(val);
            else if (strcmp(key, "map_x") == 0) d.map_x = atoi(val);
            else if (strcmp(key, "map_y") == 0) d.map_y = atoi(val);
            else if (strcmp(key, "side") == 0) d.side = atoi(val);
            else if (strcmp(key, "u") == 0) d.u = atof(val);
            else if (strcmp(key, "v") == 0) d.v = atof(val);
            else if (strcmp(key, "width") == 0) d.width = atof(val);
            else if (strcmp(key, "height") == 0) d.height = atof(val);
            else if (strcmp(key, "fg") == 0) parse_color(val, &d.fg);
            else if (strcmp(key, "use_bg") == 0) d.use_bg = atoi(val) != 0;
            else if (strcmp(key, "bg") == 0) parse_color(val, &d.bg);
            else if (strcmp(key, "text") == 0) {
                strncpy(text_buf, val, sizeof(text_buf)-1);
            }
        }
    }
    fclose(f);
    
    // Allocate text dynamically so it persists
    if (strlen(text_buf) > 0) {
        char *text_copy = malloc(strlen(text_buf) + 1);
        if (text_copy) {
            strcpy(text_copy, text_buf);
            d.text = text_copy;
        }
    } else {
        d.text = " ";
    }
    
    world_add_decal(world, d);
    return true;
}

static bool load_light(WorldState *world, const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

    double x = 0, y = 0, intensity = 1.0, radius = 4.0;
    SDL_Color color = {255, 255, 255, 255};
    bool is_god_ray = false;

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
            else if (strcmp(key, "is_god_ray") == 0) is_god_ray = atoi(val) != 0;
        }
    }
    fclose(f);
    
    world_add_light(world, x, y, color, intensity, radius, is_god_ray);
    return true;
}

void asset_loader_load_registry(AssetRegistry *reg, const char *base_path) {
    char filepath[512];
    
    // Load palettes (1 to 255)
    for (int i = 1; i < 256; i++) {
        snprintf(filepath, sizeof(filepath), "%s/palettes/%d.txt", base_path, i);
        if (!load_palette(reg, i, filepath)) {
            // Stop at first missing ID to save time
            if (i > 10) break;
        }
    }
    
    // Load materials (1 to 255)
    for (int i = 1; i < 256; i++) {
        snprintf(filepath, sizeof(filepath), "%s/materials/%d.txt", base_path, i);
        if (!load_material(reg, i, filepath)) {
            if (i > 10) break;
        }
    }
}

Map* asset_loader_load_map_data(WorldState *world, const char *base_path, int map_id) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/maps/%d.txt", base_path, map_id);
    
    FILE *f = fopen(filepath, "r");
    if (!f) return NULL;
    
    // Read whole file into buffer
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *map_str = malloc(fsize + 1);
    fread(map_str, fsize, 1, f);
    fclose(f);
    map_str[fsize] = 0;
    
    Map *map = map_load_from_string(map_str);
    free(map_str);
    
    // For lights and decals, in a real system these might be linked per map (e.g. assets/maps/1_lights/1.txt)
    // For now, load all from the global assets/decals/ and assets/lights/
    for (int i = 1; i < 256; i++) {
        snprintf(filepath, sizeof(filepath), "%s/decals/%d.txt", base_path, i);
        if (!load_decal(world, filepath)) {
            if (i > 10) break;
        }
    }
    
    for (int i = 1; i < 256; i++) {
        snprintf(filepath, sizeof(filepath), "%s/lights/%d.txt", base_path, i);
        if (!load_light(world, filepath)) {
            if (i > 10) break;
        }
    }
    
    return map;
}
