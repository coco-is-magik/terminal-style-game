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
#include "scene_types.h"
#include "decal.h"        /* Decal struct and DecalSurface enum */
#include <SDL3/SDL.h>     /* SDL_Color — used by Light */
#include <stdbool.h>

typedef enum {
    WORLD_INSERT_OK = 0,
    WORLD_INSERT_INVALID,
    WORLD_INSERT_FULL
} WorldInsertResult;

/* ===================================================================
 *  Dynamic entity types
 * =================================================================== */

/**
 * Light — A point or spot light source in the world
 *
 * Lights are 2D point lights that cast light onto the map's light_map.
 * The intensity can be negative to create "anti-light" (darkness) that
 * subtracts from the ambient light level, e.g. for dark zones.
 *
 * The renderer draws lights as '*' billboards in their colour.
 */
typedef struct {
    Vec2 pos;               /* World position (fractional grid coordinates) */
    SDL_Color color;        /* Billboard colour and RGB/A illumination weights */
    double intensity;       /* Brightness multiplier (>0 = light, <0 = anti-light) */
    double radius;          /* Maximum distance the light reaches (grid cells) */
    SceneLightType type;    /* Point or directional spot */
    double direction;       /* Spot direction in radians [0, 2pi) */
    double cone;            /* Full spot cone angle in radians */
    double falloff;         /* Radial falloff exponent */
} Light;

/**
 * SpriteEntity — An instance of a billboard sprite in the world
 *
 * References a sprite definition in the AssetRegistry by sprite_id.
 * Rendered as a decorative, light-map-lit billboard by the world-overlay pass.
 * Sprite instances do not collide, block rays or light, or appear in mirrors.
 */
typedef struct {
    Vec2 pos;               /* World position */
    int sprite_id;          /* Index into AssetRegistry.sprites[] */
    size_t animation_frame; /* Transient current frame; never scene-authored. */
    double animation_elapsed;
} SpriteEntity;

typedef struct {
    Vec2 pos;
    double front_direction;
    uint16_t object_id;
    uint16_t sprite_id;        /* Per-instance sprite reference for rendering. */
} ObjectEntity;

/* ===================================================================
 *  Capacity constants
 * =================================================================== */

#define MAX_LIGHTS  64     /* Maximum number of simultaneous lights */
#define MAX_SPRITES 128    /* Maximum number of sprite instances */
#define MAX_DECALS  256    /* Maximum number of decal instances */
#define MAX_OBJECTS 128

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

    ObjectEntity  objects[MAX_OBJECTS];
    int           num_objects;

    Decal         decals[MAX_DECALS];     /* Surface decorations */
    int           num_decals;             /* Number of active decals */

    /* Authored scene ambient, when this runtime view was derived from a scene. */
    double        ambient_intensity;
    bool          has_authored_ambient;

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
 * Appends a Light to the lights array. Returns WORLD_INSERT_FULL at capacity
 * and WORLD_INSERT_INVALID for a NULL world, non-finite numeric input, or a
 * non-positive radius; the world is unchanged on failure.
 *
 * @param world     WorldState to add to (NULL-safe)
 * @param x         World X position of the light
 * @param y         World Y position of the light
 * @param col       Colour of the light
 * @param intensity Brightness (>0) or darkness (<0 for anti-light)
 * @param radius    Max distance the light reaches (grid cells)
 */
WorldInsertResult world_add_light(WorldState *world, double x, double y,
                                  SDL_Color col, double intensity, double radius);
WorldInsertResult world_add_spot_light(
    WorldState *world, double x, double y, SDL_Color col, double intensity,
    double radius, double direction, double cone, double falloff);

/**
 * world_add_sprite() — Add an intended billboard-style sprite instance to the world
 *
 * Appends a SpriteEntity to the sprites array. Returns WORLD_INSERT_FULL at
 * capacity and WORLD_INSERT_INVALID for a NULL world, non-finite position, or
 * a sprite ID outside 1..255.
 *
 * @param world     WorldState to add to (NULL-safe)
 * @param x         World X position
 * @param y         World Y position
 * @param sprite_id Index into AssetRegistry.sprites[]
 */
WorldInsertResult world_add_sprite(WorldState *world, double x, double y, int sprite_id);
WorldInsertResult world_add_object(WorldState *world, double x, double y,
                                   double front_direction, uint16_t object_id,
                                   uint16_t sprite_id);

/**
 * world_add_decal() — Add a surface decoration to the world
 *
 * Appends a Decal to the decals array and takes ownership of decal.pattern only
 * on WORLD_INSERT_OK. On WORLD_INSERT_INVALID or WORLD_INSERT_FULL, ownership
 * remains with the caller and the world is unchanged.
 *
 * IMPORTANT: This function performs "legacy decal migration" — if the
 * decal was defined with the old file format (using map_x/map_y/side/u/v
 * instead of world-space x/y/z), it converts the coordinates to world
 * space and sets a default depth of 0.1.  See the implementation for details.
 *
 * @param world  WorldState to add to (NULL-safe)
 * @param decal  The Decal struct to add (copied into the array)
 */
WorldInsertResult world_add_decal(WorldState *world, Decal decal);

/* Inserts an already world-space runtime decal without legacy migration. */
WorldInsertResult world_add_resolved_decal(WorldState *world, Decal decal);

#endif /* WORLD_H */