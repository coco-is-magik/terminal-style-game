/**
 * assets.h — Runtime asset data structures (palettes, materials, sprites)
 *
 * This header defines the core visual asset types used by the renderer:
 *
 *   Palette       — Three colour stops (near, mid, far) for distance-based shading
 *   Material      — Associates a palette with up to 4 glyphs (one per distance band)
 *   PatternCell   — A single glyph + material_id (used by decals and sprites)
 *   SpriteAsset   — A 2D sprite pattern definition (pattern grid + dimensions)
 *   DecalPatternAsset — Reusable pattern-only decal definition
 *   AssetRegistry — Top-level container for reusable visual definitions
 *
 * Materials, palettes, and decals use IDs 0–65535. Sprite IDs remain 0–255.
 * Assets are loaded from disk by asset_loader.c.
 */

#ifndef ASSETS_H
#define ASSETS_H

#include <SDL3/SDL.h>    /* SDL_Color */
#include <stdint.h>       /* uint8_t */
#include <stdbool.h>      /* bool */
#include <stddef.h>       /* size_t */

#define DECAL_PATTERN_ASSET_MAX_COLS 255
#define DECAL_PATTERN_ASSET_MAX_ROWS 64
#define ASSET_ID_CAPACITY 65536U
#define ASSET_ID_MAX 65535
#define SPRITE_ID_CAPACITY 256U
#define OBJECT_ID_CAPACITY 256U
#define OBJECT_NAME_CAPACITY 64U
#define SPRITE_PATTERN_MAX_COLS 255
#define SPRITE_PATTERN_MAX_ROWS 32
#define MATERIAL_NAME_CAPACITY 64U

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
 *   glyphs[0] — near   (distance ≤ 4 cells): most detailed glyph
 *   glyphs[1] — mid    (4 < distance ≤ 7 cells)
 *   glyphs[2] — far    (7 < distance ≤ 10 cells)
 *   glyphs[3] — very far (distance > 10 cells): simplest glyph
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
    uint16_t material_id;    /* Material index for colouring this cell */
} PatternCell;

/**
 * SpriteAsset — A 2D sprite pattern definition
 *
 * Sprite assets are 2D glyph/material patterns intended for billboard-style
 * rendering.  The pattern grid (cols × rows) stores the glyph and material for
 * each cell. The world-overlay renderer projects this grid as a decorative
 * camera-facing billboard.
 */
typedef struct {
    int cols;                /* Number of columns in the pattern grid */
    int rows;                /* Number of rows in the pattern grid */
    PatternCell *pattern;    /* Dynamically allocated pattern array (cols × rows) */
} SpriteAsset;

typedef enum {
    OBJECT_ATTRIBUTE_NONE = 0,
    OBJECT_ATTRIBUTE_SIMPLE = 1U << 0
} ObjectAttribute;

/** Reusable object definition. I4 supports exactly the `simple` attribute. */
typedef struct {
    char name[OBJECT_NAME_CAPACITY];
    uint16_t sprite_id;
    double front_direction;
    uint8_t attributes;
    bool loaded;
} ObjectAsset;

/** Reusable decal appearance. Placement remains authored by SceneDocument. */
typedef struct {
    int cols;
    int rows;
    PatternCell *pattern;
} DecalPatternAsset;

/**
 * AssetRegistry — Top-level container for all visual assets
 *
 * Holds direct-indexed fixed-capacity storage for palettes, materials, and
 * decals. Sprite storage intentionally remains 256 slots. Index 0 is null.
 *
 * material_names[id] — filename-derived name for material id (e.g. "1", "2").
 *   Non-empty string means the slot is loaded.  Empty string means unloaded.
 *   Populated by asset_loader.c during load; parallel to materials[].
 * material_count — the number of material slots that have been successfully loaded.
 *   IDs are not necessarily contiguous; do NOT assume IDs 1..material_count are loaded.
 */
typedef struct {
    Palette *palettes;                    /* Fixed-capacity, direct-indexed storage */
    Material *materials;                  /* Fixed-capacity, direct-indexed storage */
    SpriteAsset sprites[SPRITE_ID_CAPACITY]; /* Sprite widening remains deferred */
    ObjectAsset objects[OBJECT_ID_CAPACITY];
    DecalPatternAsset *decal_patterns;    /* Fixed-capacity reusable decal storage */
    char (*material_names)[MATERIAL_NAME_CAPACITY];
    size_t material_count;
    uint32_t generation;
} AssetRegistry;

/**
 * asset_registry_init() — Zero-initialise the entire AssetRegistry
 *
 * Allocates zeroed fixed-capacity palette/material/decal storage and clears
 * sprites. Must be called before loading assets.
 *
 * @return true on success; false for NULL or allocation failure
 */
bool asset_registry_init(AssetRegistry *reg);

/**
 * asset_registry_clear() — Release registry-owned sprite patterns and reset
 *
 * Frees every registry-owned pattern and fixed-capacity array. The registry
 * object itself is not freed and must be reinitialized before reuse.
 *
 * @param reg  Pointer to AssetRegistry to clear (NULL-safe)
 */
void asset_registry_clear(AssetRegistry *reg);

/** Return the lowest unused material ID, or 0 when unavailable/full. */
uint16_t asset_registry_allocate_material_id(const AssetRegistry *reg);

/** Return the lowest unused reusable decal-pattern ID, or 0 when unavailable/full. */
uint16_t asset_registry_allocate_decal_pattern_id(const AssetRegistry *reg);

/** Increment and return the registry generation after a completed bulk refresh. */
uint32_t asset_registry_bump_generation(AssetRegistry *reg);

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
void asset_registry_set_material(AssetRegistry *reg, int id, int pal_id, const char *glyph_set);

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
typedef struct {
    double red;
    double green;
    double blue;
} LightLevel;

SDL_Color palette_sample(const Palette *p, double distance, LightLevel light_level);

/**
 * material_find_by_name() — Look up a material ID by its filename-derived name
 *
 * Iterates IDs 1..255, compares material_names[id] with name.
 * Returns the first matching ID, or -1 if no loaded material has that name.
 *
 * @param reg   AssetRegistry to search
 * @param name  Name to look up (e.g. "1", "2")
 * @return      Material ID (1–255) on success, -1 if not found
 */
int material_find_by_name(const AssetRegistry *reg, const char *name);

/**
 * material_name_by_id() — Return the name string for a loaded material ID
 *
 * Returns material_names[id] if id is in 1..255 and the slot is loaded
 * (non-empty name).  Returns "UNKNOWN" for invalid or unloaded IDs.
 *
 * @param reg  AssetRegistry to query
 * @param id   Material ID (0–255)
 * @return     Name string (pointer into reg->material_names or literal)
 */
const char *material_name_by_id(const AssetRegistry *reg, int id);

/**
 * material_id_is_loaded() — Check whether a material ID has a loaded name
 *
 * A material slot is considered loaded if material_names[id] is non-empty.
 * This does not require contiguous IDs — gaps are allowed.
 *
 * @param reg  AssetRegistry to query
 * @param id   Material ID (0–255)
 * @return     true if loaded, false otherwise
 */
bool material_id_is_loaded(const AssetRegistry *reg, int id);

/** Copy a reusable decal pattern into a registry-owned ID slot. */
bool asset_registry_set_decal_pattern(
    AssetRegistry *reg,
    int id,
    int cols,
    int rows,
    const PatternCell *pattern
);

/** Return a borrowed loaded definition, or NULL for an invalid/unloaded ID. */
const DecalPatternAsset *asset_registry_get_decal_pattern(
    const AssetRegistry *reg,
    int id
);

/** Return the process-lifetime built-in checker/! missing-reference pattern. */
const DecalPatternAsset *asset_registry_get_missing_decal_pattern(void);

/** Return a borrowed loaded sprite definition, or NULL for an invalid/unloaded ID. */
const SpriteAsset *asset_registry_get_sprite(
    const AssetRegistry *reg,
    int id
);

/** Check whether a sprite ID has a loaded pattern. */
bool sprite_id_is_loaded(const AssetRegistry *reg, int id);
/** Return the lowest unused sprite-pattern ID, or 0 when full. */
uint16_t asset_registry_allocate_sprite_id(const AssetRegistry *reg);
/** Deep-copy a sprite pattern into the registry. */
bool asset_registry_set_sprite(AssetRegistry *reg, uint16_t id, int cols, int rows,
                               const PatternCell *pattern);

#endif /* ASSETS_H */
