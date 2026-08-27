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
#include "optical_runtime_view.h"
#include <stdint.h>    /* uint64_t */

#ifdef __cplusplus
extern "C" {
#endif

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
#if defined(__GNUC__) || defined(__clang__)
#define LIGHTING_DEPRECATED(message) __attribute__((deprecated(message)))
#else
#define LIGHTING_DEPRECATED(message)
#endif

/** Deprecated compatibility wrapper retained only for an explicit rollback. */
LIGHTING_DEPRECATED("use lighting_update_optical")
void lighting_update(Map *map, WorldState *world);
void lighting_update_optical(
    Map *map, WorldState *world, const OpticalRuntimeView *optical_view,
    uint32_t optical_generation
);

#undef LIGHTING_DEPRECATED

/**
 * Profiling variables - exported for benchmark modes.
 * These are incremented/reset during lighting_update().
 */
extern uint64_t lighting_shadow_ray_count;
extern double lighting_total_time_ms;

#ifdef __cplusplus
}
#endif

#endif /* LIGHTING_H */