/**
 * assets.c — Runtime asset registry management
 *
 * This file implements the core data structures and lookup functions that
 * manage the game's visual assets at runtime:
 *
 *   - AssetRegistry          : the top-level container holding all palettes,
 *                              materials, and sprites (indexed 0–255 each).
 *   - asset_registry_init()  : zero-initialises the entire registry.
 *   - asset_registry_set_palette() : stores a 3-colour palette under a given ID.
 *   - asset_registry_set_material(): stores material→palette association + glyphs.
 *   - palette_sample()       : resolves a palette colour at a given distance
 *                              and light level, returning the final shaded colour.
 *   - material_find_by_name(): looks up a material ID by its filename-derived name.
 *   - material_name_by_id()  : returns the name for a loaded material ID.
 *   - material_id_is_loaded(): checks whether a material slot has a loaded name.
 *
 * Palettes and materials are loaded from disk by asset_loader.c, but the
 * actual storage lives here.  palette_sample() is the function that the
 * raycast renderer calls for wall material samples and decal cells to
 * determine what colour and brightness that fragment should be.
 */

#include "assets.h"        /* AssetRegistry, Palette, Material, SpriteAsset, PatternCell */
#include <string.h>        /* memset(), strlen(), strcmp() */

/**
 * asset_registry_init() — Zero-initialise the entire asset registry
 *
 * Sets every byte of the AssetRegistry to 0.  This means all palette
 * colours default to {0,0,0,0}, materials have id=0/palette_id=0 and zeroed
 * glyph bytes, sprites are empty, material_names are all empty strings, and
 * material_count is 0.
 *
 * @param reg  Pointer to the AssetRegistry to initialise (NULL-safe)
 */
void asset_registry_init(AssetRegistry *reg) {
    if (!reg) return;
    memset(reg, 0, sizeof(AssetRegistry));
}

/**
 * asset_registry_set_palette() — Register a palette under a given ID
 *
 * Palettes define three colour stops — near, mid, far — that the renderer
 * uses to shade surfaces at different distances.  The "near" colour is used
 * for walls very close to the camera, "mid" for medium distances, and "far"
 * for distant walls, giving a depth-based colouring effect.
 *
 * @param reg  AssetRegistry to write into
 * @param id   Palette index (0–255).  Index 0 is typically unused / black.
 * @param n    Colour for nearby surfaces (0–4 cells away)
 * @param m    Colour for mid-range surfaces (4–8 cells away)
 * @param f    Colour for far-away surfaces (8+ cells away)
 */
void asset_registry_set_palette(AssetRegistry *reg, int id, SDL_Color n, SDL_Color m, SDL_Color f) {
    if (!reg || id < 0 || id > 255) return;
    reg->palettes[id].near_color = n;
    reg->palettes[id].mid_color = m;
    reg->palettes[id].far_color = f;
}

/**
 * asset_registry_set_material() — Register a material under a given ID
 *
 * A material combines:
 *   - A reference to a palette (which provides the colour scheme)
 *   - Up to four glyph characters, one per distance band (near, mid, far, very_far)
 *
 * The renderer picks which glyph to draw based on the wall distance:
 * glyphs[0] = very close, glyphs[1] = medium, glyphs[2] = far,
 * glyphs[3] = very far.
 *
 * The glyph_set string is expected to be 0–4 characters long.  Any missing
 * characters default to a space ' '.
 *
 * @param reg        AssetRegistry to write into
 * @param id         Material index (0–255)
 * @param pal_id     Palette ID that this material's colour comes from
 * @param glyph_set  Pointer to a string of 1–4 glyphs (e.g. "#@:.")
 */
void asset_registry_set_material(AssetRegistry *reg, int id, int pal_id, const char *glyph_set) {
    if (!reg || id < 0 || id > 255) return;
    reg->materials[id].id = id;
    reg->materials[id].palette_id = pal_id;

    /* Copy up to 4 glyph characters; pad remaining positions with spaces.
     * For example glyph_set="#@:."  ->  glyphs = {'#', '@', ':', '.'}
     *                glyph_set="#"     ->  glyphs = {'#', ' ', ' ', ' '} */
    for (int i = 0; i < 4; i++) {
        reg->materials[id].glyphs[i] =
            (glyph_set && i < (int)strlen(glyph_set)) ? (uint8_t)glyph_set[i] : (uint8_t)' ';
    }
}

/**
 * palette_sample() — Sample a colour from a palette at a given distance and light level
 *
 * This is the core colour lookup function called by the raycast renderer for
 * wall material samples and decal cells.  It works in two stages:
 *
 *   1. Distance-based colour selection:
 *        < 4 cells -> near_color
 *        4–8 cells -> mid_color
 *        > 8 cells -> far_color
 *
 *   2. Light-level attenuation:
 *        The selected colour's R, G, B channels are multiplied by light_level,
 *        which ranges from 0.0 (pitch black) to 1.0 (full brightness).
 *        This is what makes walls dimmer in shadows / at night.
 *
 * The alpha channel is always set to 255 (fully opaque).
 *
 * @param p            Pointer to the Palette to sample from
 * @param distance     Distance from camera in grid cells (not screen pixels)
 * @param light_level  Lighting multiplier, clamped to [0.0, 1.0]
 * @return             The final shaded SDL_Color
 */
SDL_Color palette_sample(const Palette *p, double distance, double light_level) {
    /* --- Stage 1: pick the base colour based on distance band --- */
    SDL_Color base;
    if (distance < 4.0) {
        base = p->near_color;       /* Very close -> use "near" colour */
    } else if (distance < 8.0) {
        base = p->mid_color;        /* Medium distance -> use "mid" colour */
    } else {
        base = p->far_color;        /* Far away -> use "far" colour */
    }

    /* --- Stage 2: apply light-level attenuation --- */
    /* Clamp light_level to valid range */
    if (light_level < 0.0) light_level = 0.0;
    if (light_level > 1.0) light_level = 1.0;

    /* Scale each channel independently */
    base.r = (uint8_t)(base.r * light_level);
    base.g = (uint8_t)(base.g * light_level);
    base.b = (uint8_t)(base.b * light_level);
    base.a = 255;                    /* Always fully opaque */
    return base;
}

/**
 * material_find_by_name() — Look up a material ID by its filename-derived name
 *
 * Iterates IDs 1..255 and returns the first ID whose material_names entry
 * matches name exactly.  Returns -1 if no match is found.
 */
int material_find_by_name(const AssetRegistry *reg, const char *name) {
    if (!reg || !name) return -1;
    for (int id = 1; id <= 255; id++) {
        if (reg->material_names[id][0] != '\0' &&
            strcmp(reg->material_names[id], name) == 0) {
            return id;
        }
    }
    return -1;
}

/**
 * material_name_by_id() — Return the name string for a loaded material ID
 *
 * Returns the stored name for id if it is in 1..255 and the slot is loaded.
 * Returns the literal string "UNKNOWN" for invalid or unloaded IDs.
 */
const char *material_name_by_id(const AssetRegistry *reg, int id) {
    if (!reg || id < 1 || id > 255) return "UNKNOWN";
    if (reg->material_names[id][0] == '\0') return "UNKNOWN";
    return reg->material_names[id];
}

/**
 * material_id_is_loaded() — Check whether a material ID has a loaded name
 *
 * Returns true if material_names[id] is non-empty (i.e. the slot was
 * populated by the loader).  IDs outside 1..255 always return false.
 */
bool material_id_is_loaded(const AssetRegistry *reg, int id) {
    if (!reg || id < 1 || id > 255) return false;
    return reg->material_names[id][0] != '\0';
}
