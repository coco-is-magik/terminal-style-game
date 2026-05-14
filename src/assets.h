/**
 * assets.h — Runtime asset data structures (palettes, materials, sprites)
 *
 * This header defines the core visual asset types used by the renderer:
 *
 *   Palette       — Three colour stops (near, mid, far) for distance-based shading
 *   Material      — Associates a palette with up to 4 glyphs (one per distance band)
 *   PatternCell   — A single glyph + material_id (used by decals and sprites)
 *   SpriteAsset   — A 2D sprite pattern definition (pattern grid + dimensions)
 *   AssetRegistry — Top-level container (256 palettes + 256 materials + 256 sprites)
 *
 * All assets are indexed by ID (0–255).  ID 0 is typically unused / default.
 * Assets are loaded from disk by asset_loader.c.
 */

#ifndef ASSETS_H
#define ASSETS_H

#include <SDL3/SDL.h>    /* SDL_Color */
#include <stdint.h>       /* uint8_t */

/**
 * Palette — Three-colour distance-based shading palette
 *
 * Defines colours for three distance bands from the camera:
 *   near_color — used for walls 0–4 cells away
 *   mid_color  — used for walls 4–8 cells away
 *   far_color  — used for walls 8+ cells away
 */
typedef struct {
    SDL_Color near_color;    /* Colour for nearby surfaces */
    SDL_Color mid_color;     /* Colour for mid-range surfaces */
    SDL_Color far_color;     /* Colour for distant surfaces */
} Palette;

/**
 * Material — A surface material combining a palette and distance-based glyphs
 *
 * The glyphs array provides four characters, one per distance band:
 *   glyphs[0] — near  (0–4 cells): most detailed glyph
 *   glyphs[1] — mid   (4–7 cells)
 *   glyphs[2] — far   (7–10 cells)
 *   glyphs[3] — very far (10+ cells): simplest glyph
 */
typedef struct {
    int id;                  /* Material ID (1–255) */
    int palette_id;          /* Index into AssetRegistry.palettes[] */
    uint8_t glyphs[4];       /* Distance-ordered glyph characters */
} Material;

/**
 * PatternCell — A single cell in a decal or sprite pattern grid
 *
 * Combining a glyph character and a material reference allows each
 * cell of a pattern to have its own colour scheme independent of
 * the other cells in the same decal/sprite.
 */
typedef struct {
    uint8_t glyph;           /* Character to render */
    uint8_t material_id;     /* Material index for colouring this cell */
} PatternCell;

/**
 * SpriteAsset — A 2D sprite pattern definition
 *
 * Sprite assets are 2D glyph/material patterns intended for billboard-style
 * rendering.  The pattern grid (cols × rows) stores the glyph and material for
 * each cell.  Generic sprite rendering is not currently implemented.
 */
typedef struct {
    int cols;                /* Number of columns in the pattern grid */
    int rows;                /* Number of rows in the pattern grid */
    PatternCell *pattern;    /* Dynamically allocated pattern array (cols × rows) */
} SpriteAsset;

/**
 * AssetRegistry — Top-level container for all visual assets
 *
 * Holds 256 slots each for palettes, materials, and sprites.
 * Indices 1–255 are used; index 0 is typically left as default/zero.
 */
typedef struct {
    Palette      palettes[256];     /* Distance-based colour palettes */
    Material     materials[256];    /* Surface materials (palette ref + glyphs) */
    SpriteAsset  sprites[256];      /* 2D sprite pattern definitions */
} AssetRegistry;

/**
 * asset_registry_init() — Zero-initialise the entire AssetRegistry
 *
 * Sets all palette colours to {0,0,0,0}, materials to id=0/palette_id=0,
 * and sprites to empty.  Must be called before loading assets.
 *
 * @param reg  Pointer to AssetRegistry to initialise (NULL-safe)
 */
void asset_registry_init(AssetRegistry *reg);

/**
 * asset_registry_set_palette() — Register a palette under a given ID
 *
 * @param reg  AssetRegistry to write into
 * @param id   Palette index (0–255)
 * @param n    Colour for nearby surfaces
 * @param m    Colour for mid-range surfaces
 * @param f    Colour for far-away surfaces
 */
void asset_registry_set_palette(AssetRegistry *reg, int id, SDL_Color n, SDL_Color m, SDL_Color f);

/**
 * asset_registry_set_material() — Register a material
 *
 * Associates a palette with up to 4 glyphs for distance-based rendering.
 * Missing glyphs default to space ' '.
 *
 * @param reg        AssetRegistry to write into
 * @param id         Material index (0–255)
 * @param pal_id     Palette ID for colouring
 * @param glyph_set  String of 1–4 glyphs (e.g. "#@:.")
 */
void asset_registry_set_material(AssetRegistry *reg, int id, int pal_id, const char* glyph_set);

/**
 * palette_sample() — Sample a colour from a palette at a given distance and light level
 *
 * Selects near/mid/far colour based on distance, then attenuates R/G/B
 * channels by light_level [0.0, 1.0].  This is the core colour lookup
 * called by the raycast renderer for wall material samples and decal cells.
 *
 * @param p            Palette to sample from
 * @param distance     Distance from camera in grid cells
 * @param light_level  Lighting multiplier, clamped to [0.0, 1.0]
 * @return             The final shaded SDL_Color
 */
SDL_Color palette_sample(const Palette *p, double distance, double light_level);

#endif /* ASSETS_H */