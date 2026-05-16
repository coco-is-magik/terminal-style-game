/**
 * world.h — World state container for dynamic game objects
 *
 * This header defines the WorldState struct, which holds all dynamic,
 * non-tilemap elements of the game world:
 *
 *   Lights    — Point light sources with position, colour, intensity,
 *               and radius.  Negative intensity creates anti-light/darkness.
 *   Sprites   — Intended billboard-style sprite instances referenced by sprite_id
 *               (index into AssetRegistry.sprites[]).
 *   Decals    — Surface decorations (wall posters, floor markings, etc.)
 *               with position, orientation, rotation, and pattern data.
 *   Spawn     — The initial player spawn point (position + angle).
 *
 * Capacity limits:
 *   MAX_LIGHTS = 64    — bounded for performance (each light raycasts
 *                        against the entire bounding box every frame)
 *   MAX_SPRITES = 128  — enough for a moderately populated level
 *   MAX_DECALS  = 256  — plenty for detailed maps
 */

#ifndef WORLD_H
#define WORLD_H

#include "entity.h"       /* Vec2 type — used by Light, SpriteEntity, spawn_pos */
#include "decal.h"        /* Decal struct and DecalSurface enum */
#include <SDL3/SDL.h>     /* SDL_Color — used by Light */
#include <stdbool.h>      /* (included for future use) */

/* ===================================================================
 *  Dynamic entity types
 * =================================================================== */

/**
 * Light — A point light source in the world
 *
 * Lights are 2D point lights that cast light onto the map's light_map.
 * The intensity can be negative to create "anti-light" (darkness) that
 * subtracts from the ambient light level, e.g. for dark zones.
 *
 * The renderer draws lights as '*' billboards in their colour.
 */
typedef struct {
    Vec2 pos;               /* World position (fractional grid coordinates) */
    SDL_Color color;        /* Colour of the light (affects wall tint) */
    double intensity;       /* Brightness multiplier (>0 = light, <0 = anti-light) */
    double radius;          /* Maximum distance the light reaches (grid cells) */
} Light;

/**
 * SpriteEntity — An instance of a billboard sprite in the world
 *
 * References a sprite definition in the AssetRegistry by sprite_id.
 * Intended for billboard-style sprite instances.  Generic sprite rendering
 * is not currently implemented by raycast_render().
 */
typedef struct {
    Vec2 pos;               /* World position */
    int sprite_id;          /* Index into AssetRegistry.sprites[] */
} SpriteEntity;

/* ===================================================================
 *  Capacity constants
 * =================================================================== */

#define MAX_LIGHTS  64     /* Maximum number of simultaneous lights */
#define MAX_SPRITES 128    /* Maximum number of sprite instances */
#define MAX_DECALS  256    /* Maximum number of decal instances */

/* ===================================================================
 *  WorldState — the top-level world container
 * =================================================================== */

/**
 * WorldState — Holds all dynamic objects and the spawn point
 *
 * This is the runtime container populated by asset_loader.c when
 * loading a map.  It's passed to the raycast renderer and lighting
 * system each frame.
 */
typedef struct {
    /* --- Dynamic arrays (fixed capacity) --- */
    Light         lights[MAX_LIGHTS];     /* Point light sources */
    int           num_lights;             /* Number of active lights */

    SpriteEntity  sprites[MAX_SPRITES];   /* Intended billboard-style sprite instances */
    int           num_sprites;            /* Number of active sprites */

    Decal         decals[MAX_DECALS];     /* Surface decorations */
    int           num_decals;             /* Number of active decals */

    /* --- Player spawn point --- */
    Vec2          spawn_pos;              /* Default: (1.5, 1.5) */
    double        spawn_angle;            /* Default: 0.0 (east) */
} WorldState;

/* ===================================================================
 *  Public API
 * =================================================================== */

/**
 * world_init() — Zero-initialise a WorldState and set default spawn point
 *
 * Sets all arrays to zero (num_lights/sprites/decals = 0) and sets
 * the spawn point to (1.5, 1.5) facing east (angle = 0.0).
 *
 * @param world  Pointer to WorldState to initialise (NULL-safe)
 */
void world_init(WorldState *world);

/**
 * world_clear() — Free WorldState-owned dynamic resources and reset it
 *
 * Frees decal pattern arrays owned by the world, then returns the WorldState
 * to the same default state produced by world_init().  The WorldState object
 * itself is not freed; callers may allocate it on the stack or embed it.
 *
 * @param world  Pointer to WorldState to clear (NULL-safe)
 */
void world_clear(WorldState *world);

/**
 * world_add_light() — Add a point light to the world
 *
 * Appends a Light to the lights array.  Silently drops the light if
 * MAX_LIGHTS has been reached.
 *
 * @param world     WorldState to add to (NULL-safe)
 * @param x         World X position of the light
 * @param y         World Y position of the light
 * @param col       Colour of the light
 * @param intensity Brightness (>0) or darkness (<0 for anti-light)
 * @param radius    Max distance the light reaches (grid cells)
 */
void world_add_light(WorldState *world, double x, double y, SDL_Color col, double intensity, double radius);

/**
 * world_add_sprite() — Add an intended billboard-style sprite instance to the world
 *
 * Appends a SpriteEntity to the sprites array.  Silently drops if
 * MAX_SPRITES has been reached.
 *
 * @param world     WorldState to add to (NULL-safe)
 * @param x         World X position
 * @param y         World Y position
 * @param sprite_id Index into AssetRegistry.sprites[]
 */
void world_add_sprite(WorldState *world, double x, double y, int sprite_id);

/**
 * world_add_decal() — Add a surface decoration to the world
 *
 * Appends a Decal to the decals array.  Silently drops if MAX_DECALS
 * has been reached.
 *
 * IMPORTANT: This function performs "legacy decal migration" — if the
 * decal was defined with the old file format (using map_x/map_y/side/u/v
 * instead of world-space x/y/z), it converts the coordinates to world
 * space and sets a default depth of 0.1.  See the implementation for details.
 *
 * @param world  WorldState to add to (NULL-safe)
 * @param decal  The Decal struct to add (copied into the array)
 */
void world_add_decal(WorldState *world, Decal decal);

#endif /* WORLD_H */