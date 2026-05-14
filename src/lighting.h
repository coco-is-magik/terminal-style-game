/**
 * lighting.h — 2D dynamic lighting system
 *
 * Declares the per-frame lighting update function that computes light
 * levels for every map tile by combining ambient light with contributions
 * from point lights, including shadow testing via raycasting.
 *
 * See lighting.c for the implementation.
 */

#ifndef LIGHTING_H
#define LIGHTING_H

#include "map.h"       /* Map — provides the light_map array to write into */
#include "world.h"     /* WorldState — provides the list of active lights */

/**
 * lighting_update() — Per-frame light propagation across the map
 *
 * Resets every tile in map->light_map to the ambient light level, then
 * iterates over all point lights in the world and "paints" light onto
 * the grid using distance-based falloff and DDA raycasting for shadows.
 *
 * Called once per frame from app.c (only in VISUAL_RAYCAST mode).
 *
 * @param map    The Map whose light_map will be updated
 * @param world  The WorldState containing all active point lights
 */
void lighting_update(Map *map, WorldState *world);

#endif /* LIGHTING_H */