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
#include "checked_size.h"
#include <math.h>
#include <stdlib.h>
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
bool asset_registry_init(AssetRegistry *reg) {
    if (!reg) return false;
    memset(reg, 0, sizeof(AssetRegistry));
    reg->palettes = calloc(ASSET_ID_CAPACITY, sizeof(*reg->palettes));
    reg->materials = calloc(ASSET_ID_CAPACITY, sizeof(*reg->materials));
    reg->decal_patterns = calloc(ASSET_ID_CAPACITY, sizeof(*reg->decal_patterns));
    reg->material_names = calloc(ASSET_ID_CAPACITY, sizeof(*reg->material_names));
    if (!reg->palettes || !reg->materials || !reg->decal_patterns ||
        !reg->material_names) {
        asset_registry_clear(reg);
        return false;
    }
    return true;
}

void asset_registry_clear(AssetRegistry *reg) {
    if (!reg) return;
    for (size_t id = 0U; id < SPRITE_ID_CAPACITY; id++) {
        free(reg->sprites[id].pattern);
        reg->sprites[id].pattern = NULL;
        if (reg->sprite_animations[id].frames) {
            for (size_t frame = 0U;
                 frame < reg->sprite_animations[id].frame_count; frame++) {
                free(reg->sprite_animations[id].frames[frame].pattern);
            }
            free(reg->sprite_animations[id].frames);
            reg->sprite_animations[id].frames = NULL;
        }
    }
    if (reg->decal_patterns) {
        for (size_t id = 0U; id < ASSET_ID_CAPACITY; id++) {
        free(reg->decal_patterns[id].pattern);
        reg->decal_patterns[id].pattern = NULL;
        }
    }
    free(reg->palettes);
    free(reg->materials);
    free(reg->decal_patterns);
    free(reg->material_names);
    memset(reg, 0, sizeof(*reg));
}

uint16_t asset_registry_allocate_material_id(const AssetRegistry *reg) {
    if (!reg || !reg->material_names) return 0U;
    for (uint32_t id = 1U; id < ASSET_ID_CAPACITY; id++) {
        if (reg->material_names[id][0] == '\0') return (uint16_t)id;
    }
    return 0U;
}

uint16_t asset_registry_allocate_decal_pattern_id(const AssetRegistry *reg) {
    if (!reg || !reg->decal_patterns) return 0U;
    for (uint32_t id = 1U; id < ASSET_ID_CAPACITY; id++) {
        if (!reg->decal_patterns[id].pattern) return (uint16_t)id;
    }
    return 0U;
}

uint32_t asset_registry_bump_generation(AssetRegistry *reg) {
    if (!reg) return 0U;
    reg->generation++;
    if (reg->generation == 0U) reg->generation = 1U;
    return reg->generation;
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
    if (!reg || !reg->palettes || id < 0 || id > ASSET_ID_MAX) return;
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
    if (!reg || !reg->materials || id < 0 || id > ASSET_ID_MAX ||
        pal_id < 0 || pal_id > ASSET_ID_MAX) return;
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
SDL_Color palette_sample(const Palette *p, double distance, LightLevel light_level) {
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
    if (light_level.red < 0.0) light_level.red = 0.0;
    if (light_level.red > 1.0) light_level.red = 1.0;
    if (light_level.green < 0.0) light_level.green = 0.0;
    if (light_level.green > 1.0) light_level.green = 1.0;
    if (light_level.blue < 0.0) light_level.blue = 0.0;
    if (light_level.blue > 1.0) light_level.blue = 1.0;

    /* Scale each channel independently */
    base.r = (uint8_t)(base.r * light_level.red);
    base.g = (uint8_t)(base.g * light_level.green);
    base.b = (uint8_t)(base.b * light_level.blue);
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
    if (!reg || !reg->material_names || !name) return -1;
    for (int id = 1; id <= ASSET_ID_MAX; id++) {
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
    if (!reg || !reg->material_names || id < 1 || id > ASSET_ID_MAX) return "UNKNOWN";
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
    if (!reg || !reg->material_names || id < 1 || id > ASSET_ID_MAX) return false;
    return reg->material_names[id][0] != '\0';
}

bool asset_registry_set_decal_pattern(AssetRegistry *reg, int id, int cols,
                                      int rows, const PatternCell *pattern) {
    size_t cells;
    size_t bytes;
    PatternCell *copy;
    if (!reg || !reg->decal_patterns || id < 1 || id > ASSET_ID_MAX || cols <= 0 ||
        cols > DECAL_PATTERN_ASSET_MAX_COLS || rows <= 0 ||
        rows > DECAL_PATTERN_ASSET_MAX_ROWS || !pattern) {
        return false;
    }
    if (!checked_size_2d(cols, rows, &cells) ||
        !checked_size_bytes(cells, sizeof(*copy), &bytes)) {
        return false;
    }
    copy = malloc(bytes);
    if (!copy) return false;
    memcpy(copy, pattern, bytes);
    free(reg->decal_patterns[id].pattern);
    reg->decal_patterns[id].cols = cols;
    reg->decal_patterns[id].rows = rows;
    reg->decal_patterns[id].pattern = copy;
    return true;
}

const DecalPatternAsset *asset_registry_get_decal_pattern(
    const AssetRegistry *reg, int id) {
    if (!reg || !reg->decal_patterns || id < 1 || id > ASSET_ID_MAX ||
        !reg->decal_patterns[id].pattern) {
        return NULL;
    }
    return &reg->decal_patterns[id];
}

const DecalPatternAsset *asset_registry_get_missing_decal_pattern(void) {
    static PatternCell cells[4] = {
        {(uint8_t)'!', UINT16_C(1)}, {(uint8_t)'#', UINT16_C(2)},
        {(uint8_t)'#', UINT16_C(2)}, {(uint8_t)'!', UINT16_C(1)}
    };
    static DecalPatternAsset fallback = {2, 2, cells};
    return &fallback;
}
const SpriteAsset *asset_registry_get_sprite(
    const AssetRegistry *reg, int id) {
    return asset_registry_get_sprite_frame(reg, id, 0U);
}

const SpriteAsset *asset_registry_get_sprite_frame(
    const AssetRegistry *reg, int id, size_t frame_index) {
    const SpriteAnimationAsset *animation;
    if (!reg || id < 1 || id >= (int)SPRITE_ID_CAPACITY) return NULL;
    animation = &reg->sprite_animations[id];
    if (animation->frames && animation->frame_count > 0U) {
        const SpriteAsset *frame = &animation->frames[
            frame_index < animation->frame_count ? frame_index : 0U];
        return frame->pattern && frame->cols > 0 && frame->rows > 0 ? frame : NULL;
    }
    if (!reg->sprites[id].pattern || reg->sprites[id].cols <= 0 ||
        reg->sprites[id].rows <= 0) return NULL;
    return &reg->sprites[id];
}

const SpriteAnimationAsset *asset_registry_get_sprite_animation(
    const AssetRegistry *reg, int id) {
    const SpriteAnimationAsset *animation;
    if (!reg || id < 1 || id >= (int)SPRITE_ID_CAPACITY) return NULL;
    animation = &reg->sprite_animations[id];
    return animation->frames && animation->frame_count > 0U ? animation : NULL;
}

bool sprite_id_is_loaded(const AssetRegistry *reg, int id) {
    return asset_registry_get_sprite(reg, id) != NULL;
}

uint16_t asset_registry_allocate_sprite_id(const AssetRegistry *reg) {
    uint16_t id;
    if (!reg) return 0U;
    for (id = 1U; id < SPRITE_ID_CAPACITY; id++) {
        if (!sprite_id_is_loaded(reg, (int)id)) return id;
    }
    return 0U;
}

bool asset_registry_set_sprite(AssetRegistry *reg, uint16_t id, int cols, int rows,
                               const PatternCell *pattern) {
    PatternCell *copy;
    size_t count;
    size_t bytes;
    if (!reg || id == 0U || id >= SPRITE_ID_CAPACITY || cols <= 0 || rows <= 0 ||
        cols > SPRITE_PATTERN_MAX_COLS || rows > SPRITE_PATTERN_MAX_ROWS || !pattern ||
        !checked_size_2d(cols, rows, &count) ||
        !checked_size_bytes(count, sizeof(*copy), &bytes)) return false;
    copy = malloc(bytes);
    if (!copy) return false;
    memcpy(copy, pattern, bytes);
    if (reg->sprite_animations[id].frames) {
        for (size_t frame = 0U;
             frame < reg->sprite_animations[id].frame_count; frame++)
            free(reg->sprite_animations[id].frames[frame].pattern);
        free(reg->sprite_animations[id].frames);
        memset(&reg->sprite_animations[id], 0, sizeof(reg->sprite_animations[id]));
    }
    free(reg->sprites[id].pattern);
    reg->sprites[id].cols = cols;
    reg->sprites[id].rows = rows;
    reg->sprites[id].pattern = copy;
    return true;
}

bool asset_registry_set_sprite_animation(AssetRegistry *reg, uint16_t id,
                                         const SpriteAsset *frames, size_t frame_count,
                                         double frames_per_second, bool loop) {
    SpriteAsset *copy;
    size_t i;
    if (!reg || id == 0U || id >= SPRITE_ID_CAPACITY || !frames ||
        frame_count < 2U || frame_count > SPRITE_ANIMATION_MAX_FRAMES ||
        !isfinite(frames_per_second) || frames_per_second < 0.1 ||
        frames_per_second > 120.0) return false;
    copy = calloc(frame_count, sizeof(*copy));
    if (!copy) return false;
    for (i = 0U; i < frame_count; i++) {
        size_t count;
        size_t bytes;
        if (frames[i].cols <= 0 || frames[i].rows <= 0 ||
            frames[i].cols > SPRITE_PATTERN_MAX_COLS ||
            frames[i].rows > SPRITE_PATTERN_MAX_ROWS || !frames[i].pattern ||
            !checked_size_2d(frames[i].cols, frames[i].rows, &count) ||
            !checked_size_bytes(count, sizeof(*copy[i].pattern), &bytes)) goto fail;
        copy[i].pattern = malloc(bytes);
        if (!copy[i].pattern) goto fail;
        memcpy(copy[i].pattern, frames[i].pattern, bytes);
        copy[i].cols = frames[i].cols;
        copy[i].rows = frames[i].rows;
    }
    free(reg->sprites[id].pattern);
    memset(&reg->sprites[id], 0, sizeof(reg->sprites[id]));
    if (reg->sprite_animations[id].frames) {
        for (i = 0U; i < reg->sprite_animations[id].frame_count; i++)
            free(reg->sprite_animations[id].frames[i].pattern);
        free(reg->sprite_animations[id].frames);
    }
    reg->sprite_animations[id].frames = copy;
    reg->sprite_animations[id].frame_count = frame_count;
    reg->sprite_animations[id].frames_per_second = frames_per_second;
    reg->sprite_animations[id].loop = loop;
    return true;
fail:
    for (i = 0U; i < frame_count; i++) free(copy[i].pattern);
    free(copy);
    return false;
}

