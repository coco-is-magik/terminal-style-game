#include "assets.h"
#include <string.h>

void asset_registry_init(AssetRegistry *reg) {
    if (!reg) return;
    memset(reg, 0, sizeof(AssetRegistry));
}

void asset_registry_set_palette(AssetRegistry *reg, int id, SDL_Color n, SDL_Color m, SDL_Color f) {
    if (!reg || id < 0 || id > 255) return;
    reg->palettes[id].near_color = n;
    reg->palettes[id].mid_color = m;
    reg->palettes[id].far_color = f;
}

void asset_registry_set_material(AssetRegistry *reg, int id, int pal_id, const char* glyph_set) {
    if (!reg || id < 0 || id > 255) return;
    reg->materials[id].id = id;
    reg->materials[id].palette_id = pal_id;
    
    // Default to spaces if not provided
    for(int i=0; i<4; i++) {
        reg->materials[id].glyphs[i] = (glyph_set && i < (int)strlen(glyph_set)) ? glyph_set[i] : ' ';
    }
}

SDL_Color palette_sample(const Palette *p, double distance, double light_level) {
    SDL_Color base;
    if (distance < 4.0) {
        base = p->near_color;
    } else if (distance < 8.0) {
        base = p->mid_color;
    } else {
        base = p->far_color;
    }
    
    if (light_level < 0.0) light_level = 0.0;
    if (light_level > 1.0) light_level = 1.0;

    base.r = (uint8_t)(base.r * light_level);
    base.g = (uint8_t)(base.g * light_level);
    base.b = (uint8_t)(base.b * light_level);
    base.a = 255;
    return base;
}
