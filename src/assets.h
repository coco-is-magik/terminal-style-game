#ifndef ASSETS_H
#define ASSETS_H
#include <SDL3/SDL.h>
#include <stdint.h>

typedef struct {
    SDL_Color near_color;
    SDL_Color mid_color;
    SDL_Color far_color;
} Palette;

typedef struct {
    int id;
    int palette_id;
    uint8_t glyphs[4]; // 0=near, 1=mid, 2=far, 3=very_far
} Material;

// Shared pattern cell for Decals and Sprites
typedef struct {
    uint8_t glyph;
    uint8_t material_id;
} PatternCell;

typedef struct {
    int cols;
    int rows;
    PatternCell *pattern;
} SpriteAsset;

typedef struct {
    Palette palettes[256];
    Material materials[256];
    SpriteAsset sprites[256];
} AssetRegistry;

void asset_registry_init(AssetRegistry *reg);
void asset_registry_set_palette(AssetRegistry *reg, int id, SDL_Color n, SDL_Color m, SDL_Color f);
void asset_registry_set_material(AssetRegistry *reg, int id, int pal_id, const char* glyph_set);
SDL_Color palette_sample(const Palette *p, double distance, double light_level);

#endif
