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
    int len = (int)strlen(str);
    if (len == 0) return;
    char *end = str + len - 1;
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

static void parse_key_val(char *line, char **key, char **val) {
    *key = strtok(line, "=");
    *val = strtok(NULL, "");
    if (*key) trim_string(*key);
    if (*val) {
        int len = (int)strlen(*val);
        if (len == 0) return;
        char *end = *val + len - 1;
        while(end >= *val && (*end == '\n' || *end == '\r')) {
            *end = '\0';
            end--;
        }
    }
}

static bool load_decal(WorldState *world, const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

    Decal d;
    memset(&d, 0, sizeof(Decal));
    d.pattern_cols = 1;
    d.pattern_rows = 1;
    int default_material = 1;

    char p_buf[64][256] = {0};
    char m_buf[64][256] = {0};
    bool art_mode = false;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "art=", 4) == 0 || strcmp(line, "art\n") == 0 || strcmp(line, "art\r\n") == 0) {
            art_mode = true;
            break;
        }

        char *key = NULL;
        char *val = NULL;
        parse_key_val(line, &key, &val);
        if (key && val) {
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
            else if (strcmp(key, "pattern_cols") == 0) d.pattern_cols = atoi(val);
            else if (strcmp(key, "pattern_rows") == 0) d.pattern_rows = atoi(val);
            else if (strcmp(key, "default_material") == 0) default_material = atoi(val);
            else if (strncmp(key, "pattern_", 8) == 0) {
                int r = atoi(key + 8);
                if (r >= 0 && r < 64) {
                    strncpy(p_buf[r], val, 255);
                }
            }
            else if (strncmp(key, "material_", 9) == 0) {
                int r = atoi(key + 9);
                if (r >= 0 && r < 64) {
                    strncpy(m_buf[r], val, 255);
                }
            }
        }
    }
    
    d.pattern = calloc(d.pattern_cols * d.pattern_rows, sizeof(PatternCell));
    if (d.pattern) {
        bool success = true;
        if (art_mode) {
            for (int r = 0; r < d.pattern_rows; r++) {
                if (!fgets(line, sizeof(line), f)) {
                    success = false;
                    break;
                }
                int len = (int)strlen(line);
                while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
                    line[--len] = '\0';
                }
                if (len < d.pattern_cols) {
                    success = false;
                    break;
                }
                for (int c = 0; c < d.pattern_cols; c++) {
                    char g = line[c];
                    if (g == '\t') g = ' ';
                    d.pattern[r * d.pattern_cols + c].glyph = g;
                    d.pattern[r * d.pattern_cols + c].material_id = default_material;
                }
            }
        } else {
            for (int r = 0; r < d.pattern_rows; r++) {
                int mats[256];
                for (int i = 0; i < 256; i++) mats[i] = default_material;
                if (m_buf[r][0] != '\0') {
                    char *p = m_buf[r];
                    int c = 0;
                    while (*p && c < d.pattern_cols) {
                        mats[c++] = atoi(p);
                        while (*p && *p != ',') p++;
                        if (*p == ',') p++;
                    }
                }
                for (int c = 0; c < d.pattern_cols; c++) {
                    char glyph = ' ';
                    if (c < (int)strlen(p_buf[r])) glyph = p_buf[r][c];
                    d.pattern[r * d.pattern_cols + c].glyph = glyph;
                    d.pattern[r * d.pattern_cols + c].material_id = mats[c];
                }
            }
        }
        
        if (!success) {
            fprintf(stderr, "Decal load failed: %s (dimensions mismatch)\n", filepath);
            for (int i = 0; i < d.pattern_cols * d.pattern_rows; i++) {
                d.pattern[i].glyph = '!';
                d.pattern[i].material_id = default_material;
            }
        }
    }
    
    fclose(f);
    world_add_decal(world, d);
    return true;
}

static bool load_light(WorldState *world, const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

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

static bool load_sprite(AssetRegistry *reg, int id, const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

    SpriteAsset s;
    memset(&s, 0, sizeof(SpriteAsset));
    s.cols = 1;
    s.rows = 1;
    int default_material = 1;

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
    
    s.pattern = calloc(s.cols * s.rows, sizeof(PatternCell));
    if (s.pattern) {
        for (int r = 0; r < s.rows; r++) {
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
            for (int c = 0; c < s.cols; c++) {
                char glyph = ' ';
                if (c < (int)strlen(p_buf[r])) glyph = p_buf[r][c];
                s.pattern[r * s.cols + c].glyph = glyph;
                s.pattern[r * s.cols + c].material_id = mats[c];
            }
        }
    }
    
    reg->sprites[id] = s;
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
    
    // Load sprites (1 to 255)
    for (int i = 1; i < 256; i++) {
        snprintf(filepath, sizeof(filepath), "%s/sprites/%d.txt", base_path, i);
        if (!load_sprite(reg, i, filepath)) {
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
