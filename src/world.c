/**
 * world.c — World state container for dynamic game objects
 *
 * This file implements the WorldState management functions.  The WorldState
 * is a fixed-capacity container that holds all dynamic, non-tilemap elements
 * of the game world:
 *
 *   - Lights:   Point light sources (up to MAX_LIGHTS = 64)
 *   - Sprites:  Intended billboard-style sprite instances (up to MAX_SPRITES = 128)
 *   - Decals:   Surface decorations (up to MAX_DECALS = 256)
 *   - Spawn:    Player starting position and facing angle
 *
 * All three "add" functions report invalid input and capacity exhaustion with
 * WorldInsertResult. Decal pattern ownership transfers only on success.
 *
 * The world_add_decal() function includes a "legacy decal migration" path
 * that converts decals defined with the older map_x/map_y/side/u/v format
 * into the current world-space x/y/z/rotation format.
 */

#include "world.h"        /* WorldState, Light, SpriteEntity, Decal, constants */
#include <math.h>          /* isfinite() */
#include <stdlib.h>        /* free() */
#include <string.h>        /* memset() */

/**
 * world_init() — Initialise a WorldState to defaults
 *
 * Zeroes out the entire structure via memset, which sets num_lights,
 * num_sprites, and num_decals to 0 (all arrays become empty).
 * Then sets a default spawn point at (1.5, 1.5) facing east (angle = 0.0).
 *
 * The spawn position (1.5, 1.5) places the player at the centre of tile
 * (1, 1), which is the typical starting position for map 1.
 *
 * @param world  Pointer to WorldState to initialise (NULL-safe)
 */
void world_init(WorldState *world) {
    if (!world) return;

    /* Zero-initialise everything — this sets all counts to 0 and clears
     * all arrays (lights, sprites, decals) and the spawn fields to 0.0 */
    memset(world, 0, sizeof(WorldState));

    /* Override the spawn position with the default centre-of-tile-(1,1)
     * position, facing east. */
    world->spawn_pos.x = 1.5;
    world->spawn_pos.y = 1.5;
    world->spawn_angle = 0.0;    /* 0 radians = facing east (+X direction) */
}

/**
 * world_clear() — Free dynamic resources owned by a WorldState
 *
 * Decal pattern arrays are heap-allocated by the decal loader or tests and
 * ownership transfers to WorldState when world_add_decal() stores the decal.
 * This function releases those arrays and resets the world to defaults.
 */
void world_clear(WorldState *world) {
    if (!world) return;

    for (int i = 0; i < world->num_decals; i++) {
        free(world->decals[i].pattern);
        world->decals[i].pattern = NULL;
    }

    world_init(world);
}

/**
 * world_add_light() — Add a point light source to the world
 *
 * Creates a new Light entry at the end of the lights array and increments
 * num_lights. If the array is full, WORLD_INSERT_FULL is returned.
 *
 * @param world     WorldState to add to (NULL-safe)
 * @param x         World X position of the light source
 * @param y         World Y position of the light source
 * @param col       Colour of the light (affects wall tinting)
 * @param intensity Brightness (>0) or darkness (<0 for anti-light zones)
 * @param radius    Maximum distance the light reaches (grid cells)
 */
WorldInsertResult world_add_light(WorldState *world, double x, double y,
                                  SDL_Color col, double intensity, double radius) {
    if (!world || !isfinite(x) || !isfinite(y) || !isfinite(intensity) ||
        !isfinite(radius) || radius <= 0.0) return WORLD_INSERT_INVALID;
    if (world->num_lights >= MAX_LIGHTS) return WORLD_INSERT_FULL;

    /* Get a pointer to the next unused slot and increment the counter */
    Light *l = &world->lights[world->num_lights++];

    /* Fill in the light properties */
    l->pos.x     = x;
    l->pos.y     = y;
    l->color     = col;
    l->intensity = intensity;
    l->radius    = radius;
    return WORLD_INSERT_OK;
}

/**
 * world_add_sprite() — Add an intended billboard-style sprite instance to the world
 *
 * Creates a new SpriteEntity at the end of the sprites array and reports
 * capacity exhaustion instead of silently dropping the sprite.
 *
 * Note: The actual sprite visual data (pattern, material) is stored in
 * the AssetRegistry under sprite_id.  This function only stores the
 * position and which sprite to use.
 *
 * @param world     WorldState to add to (NULL-safe)
 * @param x         World X position
 * @param y         World Y position
 * @param sprite_id Index into AssetRegistry.sprites[] (0–255)
 */
WorldInsertResult world_add_sprite(WorldState *world, double x, double y, int sprite_id) {
    if (!world || !isfinite(x) || !isfinite(y) || sprite_id < 1 || sprite_id > 255) {
        return WORLD_INSERT_INVALID;
    }
    if (world->num_sprites >= MAX_SPRITES) return WORLD_INSERT_FULL;

    SpriteEntity *s = &world->sprites[world->num_sprites++];
    s->pos.x     = x;
    s->pos.y     = y;
    s->sprite_id = sprite_id;
    return WORLD_INSERT_OK;
}

/**
 * world_add_decal() — Add a surface decoration to the world
 *
 * Appends a Decal to the decals array. Pattern ownership transfers to the
 * world only when WORLD_INSERT_OK is returned.
 *
 * LEGACY DECAL MIGRATION:
 *   Older decal definition files used map_x, map_y, side, u, and v to
 *   position wall decals, rather than world-space x, y, z, and rotation.
 *   This function detects such legacy decals (depth == 0.0 for walls)
 *   and converts them:
 *
 *   For wall decals on a North-South wall (side == 0):
 *     - rotation = 0.0  (normal faces +X)
 *     - x = map_x + 1.0 (front face of the wall tile)
 *     - y = map_y + u + width/2
 *
 *   For wall decals on an East-West wall (side == 1):
 *     - rotation = PI/2 (normal faces +Y)
 *     - x = map_x + u + width/2
 *     - y = map_y + 1.0 (front face of the wall tile)
 *
 *   For any decal with depth == 0.0 (including floor/ceiling), a default
 *   depth of 0.1 is applied to prevent rendering artifacts.
 *
 *   Ceiling decals with z == 0.0 get z set to 1.0 (top of the tile).
 *
 * @param world  WorldState to add to (NULL-safe)
 * @param decal  The Decal struct to add (copied by value into the array)
 */
WorldInsertResult world_add_decal(WorldState *world, Decal decal) {
    if (!world || !decal.pattern || decal.pattern_cols <= 0 || decal.pattern_rows <= 0) {
        return WORLD_INSERT_INVALID;
    }
    if (world->num_decals >= MAX_DECALS) return WORLD_INSERT_FULL;

    /* ---- Legacy decal migration to world space ---- */

    /* Wall decals without a depth value (legacy format) need world-space
     * coordinates computed from the old map_x/map_y/side/u/v fields. */
    if (decal.surface == DECAL_SURFACE_WALL && decal.depth == 0.0) {
        decal.depth = 0.1;     /* Default small depth */

        /* v (vertical UV) goes from bottom (0) to top (1) of the wall.
         * z (world height) is computed as v plus half the decal height. */
        decal.z = decal.v + decal.height * 0.5;

        if (decal.side == 0) {
            /* North-South wall (faces east/west):
             *   - Normal points along +X
             *   - Decal centre X = far side of the tile (map_x + 1.0)
             *   - Decal centre Y = tile origin + u (horizontal UV) + half width */
            decal.rotation = 0.0;
            decal.x = decal.map_x + 1.0;
            decal.y = decal.map_y + decal.u + decal.width * 0.5;
        } else {
            /* East-West wall (faces north/south):
             *   - Normal points along +Y
             *   - Decal centre Y = far side of the tile (map_y + 1.0)
             *   - Decal centre X = tile origin + u + half width */
            decal.rotation = PI / 2.0;
            decal.x = decal.map_x + decal.u + decal.width * 0.5;
            decal.y = decal.map_y + 1.0;
        }
    } else if (decal.depth == 0.0) {
        /* Floor or ceiling decal without a depth — set a sensible default */
        decal.depth = 0.1;
    }

    /* Ceiling decals with default z=0 get moved to the ceiling height.
     * In this engine's coordinate system, the ceiling is at z = 1.0
     * (one full tile height above the floor at z = 0). */
    if (decal.surface == DECAL_SURFACE_CEILING && decal.z == 0.0) {
        decal.z = 1.0;
    }

    /* Store the (possibly migrated) decal in the array.  This is a struct
     * copy — the caller's original is not modified. */
    world->decals[world->num_decals++] = decal;
    return WORLD_INSERT_OK;
}