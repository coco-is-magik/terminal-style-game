/**
 * lighting.c — 2D dynamic lighting system with shadow casting
 *
 * This file implements the engine's per-frame lighting update.  It computes
 * a light level for every tile on the map by combining ambient light with
 * contributions from all point lights defined in the WorldState.
 *
 * The light level is stored in map->light_map[], a double array of the same
 * dimensions as the map grid.  Each cell's light_map value represents the
 * brightness contribution that will be applied to that tile's colour during
 * rendering (see palette_sample() in assets.c).  Values are usually positive,
 * but negative-intensity lights can push them below 0 before sampling clamps.
 *
 * Algorithm per light:
 *   1. Determine a bounding box of tiles that could be affected (light radius)
 *   2. For each tile in that box:
 *      a. Calculate distance from the light's centre to the tile centre
 *      b. If within radius, compute a linear falloff intensity
 *         (1.0 at centre, 0.0 at radius edge)
 *      c. Fire a ray from the light to the tile; if a wall is hit *before*
 *         reaching the tile, the tile is in shadow → apply bounce attenuation
 *      d. Add the light's contribution (intensity × light_power) to the map
 *   3. Light values accumulate additively (multiple lights brighten an area)
 *
 * IMPORTANT: This is called once per frame in VISUAL_RAYCAST mode.  It uses
 * raycast_fire() for shadow testing, which makes it O(num_lights × area × raymarch),
 * potentially expensive for large maps with many lights.
 *
 * Build flags:
 *   USE_LIGHTING_CACHE=1    - Enable lighting shadow ray cache
 *   LIGHTING_CACHE_VALIDATE=1 - Validate cache hits against original computation
 */

#include "lighting.h"    /* lighting_update() declaration, Map, WorldState types */
#include "checked_size.h" /* checked_size_2d() — validates row-major cell count */
#include "raycast.h"     /* raycast_fire(), RayResult — for shadow testing */
#include "camera.h"      /* Camera struct — used to construct a dummy camera
                            positioned at the light source for raycasting */
#include "config.h"      /* config_get() — provides ambient_light and
                            light_bounce_attenuation settings */
#include <math.h>         /* sqrt(), atan2() */
#include <SDL3/SDL.h>     /* SDL_GetPerformanceCounter/Frequency */

#ifdef USE_LIGHTING_CACHE
#include "lighting_cache.h" /* Lighting shadow ray cache */
#endif

/* =================================================================== */
/* Timing instrumentation                                             */
/* =================================================================== */

/* Static counters for profiling - exported for benchmark modes */
uint64_t lighting_shadow_ray_count = 0;
double lighting_total_time_ms = 0.0;

/* Timing helper macros */
#define LIGHTING_TIME_START() uint64_t _start = SDL_GetPerformanceCounter()
#define LIGHTING_TIME_END() uint64_t _end = SDL_GetPerformanceCounter(); \
    lighting_total_time_ms += (double)((_end - _start) * 1000) / SDL_GetPerformanceFrequency()

/* =================================================================== */
/* MIN / MAX helpers (guard against double-definition)                 */
/* =================================================================== */

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

/* =================================================================== */
/* Lighting update                                                     */
/* =================================================================== */

/**
 * lighting_update() — Per-frame light propagation
 *
 * Called once per frame from app.c (only in VISUAL_RAYCAST mode).
 * Resets the map's light_map to the ambient light level, then iterates over
 * every light in the world and "paints" light onto the grid.
 *
 * Shadow test details:
 *   To determine if a tile is in shadow, we construct a temporary Camera
 *   positioned at the light source and fire a ray toward the tile centre.
 *   If raycast_fire() hits a wall *before* reaching the tile (and that wall
 *   is not the tile itself), the tile is considered shadowed.
 *
 *   Shadowed tiles still receive dim light via light_bounce_attenuation,
 *   simulating ambient light bleed / light bouncing off walls.
 *
 * @param map    The Map whose light_map will be updated (must have light_map
 *               allocated, same dimensions as the tile grid)
 * @param world  The WorldState containing all active PointLights
 */
static bool lighting_ray_blocked_optical(
    const Map *map, double start_x, double start_y, int target_x, int target_y,
    const OpticalRuntimeView *view, uint32_t generation
) {
    (void)generation;
    double target_center_x = target_x + 0.5;
    double target_center_y = target_y + 0.5;
    double dx = target_center_x - start_x;
    double dy = target_center_y - start_y;
    int map_x = (int)floor(start_x);
    int map_y = (int)floor(start_y);
    /* A light never shadows its own cell.  The legacy raycast_fire()
     * shadow path truncates the ray at the distance to the target tile's
     * centre, so for the source tile that distance is ~0 and no wall can
     * block it.  This DDA advances one cell before it is allowed to check
     * the target, so without this early return a light sitting near a tile
     * corner could walk straight past its own cell and be falsely
     * shadowed by a diagonally adjacent wall, dimming the light's own cell
     * by light_bounce_attenuation. */
    if (map_x == target_x && map_y == target_y) return false;

    int step_x = dx < 0.0 ? -1 : 1;
    int step_y = dy < 0.0 ? -1 : 1;
    double delta_x = dx == 0.0 ? INFINITY : fabs(1.0 / dx);
    double delta_y = dy == 0.0 ? INFINITY : fabs(1.0 / dy);
    double side_x = dx < 0.0 ? (start_x - map_x) * delta_x
                             : (map_x + 1.0 - start_x) * delta_x;
    double side_y = dy < 0.0 ? (start_y - map_y) * delta_y
                             : (map_y + 1.0 - start_y) * delta_y;
    size_t limit = (size_t)map->width + (size_t)map->height + 2U;
    for (size_t steps = 0U; steps < limit; steps++) {
        size_t index;
        OpticalResolved resolved;
        const MapCell *cell;
        if (side_x < side_y) { side_x += delta_x; map_x += step_x; }
        else { side_y += delta_y; map_y += step_y; }
        if (!map_in_bounds(map, map_x, map_y) ||
            (map_x == target_x && map_y == target_y)) return false;
        index = (size_t)map_y * (size_t)map->width + (size_t)map_x;
        cell = &map->cells[index];
        if (!optical_runtime_view_resolve(
                view, index,
                (uint16_t)(cell->material_id > 0 ? cell->material_id : 0),
                cell->material_id != 0, &resolved)) return cell->material_id != 0;
        if (resolved.light_blocks) return true;
    }
    return false;
}

void lighting_update_optical(
    Map *map, WorldState *world, const OpticalRuntimeView *optical_view,
    uint32_t optical_generation
) {
    size_t cell_count;
    int light_count;
    bool optical_current;

    if (!map || !map->cells || !map->light_map || !world ||
        !checked_size_2d(map->width, map->height, &cell_count)) {
        return;
    }

    light_count = world->num_lights;
    if (light_count < 0 || light_count > MAX_LIGHTS) {
        return;
    }
    optical_current = optical_runtime_view_is_current(
        optical_view, optical_generation);

    LIGHTING_TIME_START();

#ifdef USE_LIGHTING_CACHE
    lighting_cache_reset_stats();
#endif

    /* ---- Step 1: Reset light map to ambient level ---- */
    /* Every tile starts at the ambient light level (e.g. 0.2 = 20% brightness).
     * This ensures no tile is ever completely black. */
    for (size_t i = 0; i < cell_count; i++) {
        map->light_map[i] = world->has_authored_ambient
            ? world->ambient_intensity
            : config_get()->ambient_light;
    }

    /* ---- Step 2: Process each light in the world ---- */
    for (int i = 0; i < light_count; i++) {
        Light *l = &world->lights[i];
        if (l->radius <= 0.0) continue;    /* Zero-radius light = no effect */

        /* ---- Step 2a: Compute bounding box of affected tiles ---- */
        /* Clamp to map boundaries so we don't iterate out of bounds.
         * The bounding box is a square of side (2 × radius) centred on the light. */
        int min_x = (int)MAX(0, l->pos.x - l->radius);
        int max_x = (int)MIN(map->width - 1,  l->pos.x + l->radius);
        int min_y = (int)MAX(0, l->pos.y - l->radius);
        int max_y = (int)MIN(map->height - 1, l->pos.y + l->radius);

        /* ---- Step 2b: Iterate over every tile in the bounding box ---- */
        for (int y = min_y; y <= max_y; y++) {
            for (int x = min_x; x <= max_x; x++) {
                /* Centre of the tile being evaluated */
                double cx = x + 0.5;
                double cy = y + 0.5;

                /* Vector from light position to tile centre */
                double dx = cx - l->pos.x;
                double dy = cy - l->pos.y;
                double dist = sqrt(dx * dx + dy * dy);

                /* Only process tiles within the light's radius */
                if (dist <= l->radius) {
#ifdef USE_LIGHTING_CACHE
                    /* Check cache first */
                    LightShadowKey key;
                    key.map_revision = 1;  /* TODO: Get from map */
                    key.lighting_revision = 1;  /* TODO: Get from world */
                    key.light_id = i;
                    key.target_tile_x = x;
                    key.target_tile_y = y;
                    
                    LightSampleResult cached;
                    if (!optical_current && lighting_cache_lookup(key, &cached)) {
                        /* Cache hit - use cached values */
                        map->light_map[y * map->width + x] += cached.intensity;
                        continue;
                    }
#endif

                    lighting_shadow_ray_count++;

                    /* Linear distance-based falloff:
                     *   At distance 0:   intensity = 1.0 (full brightness)
                     *   At distance R:   intensity = 0.0 (darkness) */
                    double intensity = 1.0 - (dist / l->radius);

                    /* ---- Step 2c: Shadow test ---- */
                    bool blocked = false;
                    double ray_angle = atan2(dy, dx);   /* Angle from light → tile */

                    /* Construct a temporary Camera positioned at the light source.
                     * We only need the transform (pos + angle) for raycast_fire(). */
                    Camera dummy_cam;
                    dummy_cam.transform.pos.x = l->pos.x;
                    dummy_cam.transform.pos.y = l->pos.y;
                    dummy_cam.transform.angle = ray_angle;

                    /* Fire a ray from the light toward the tile.
                     * If the ray hits a wall at coordinates (map_x, map_y) that
                     * are NOT the tile we're checking, then some other wall is
                     * casting a shadow onto this tile. */
                    if (optical_current) {
                        blocked = lighting_ray_blocked_optical(
                            map, l->pos.x, l->pos.y, x, y,
                            optical_view, optical_generation);
                    } else {
                        RayResult res = raycast_fire(map, &dummy_cam, ray_angle, dist);
                        if (res.hit && (res.map_x != x || res.map_y != y))
                            blocked = true;
                    }

                    /* ---- Step 2d: Calculate and accumulate the light contribution ---- */
                    double contribution = intensity * l->intensity;

                    if (blocked) {
                        /* Tile is in shadow — only a fraction of the light makes it
                         * through via bounce / ambient bleed.  This attenuation factor
                         * (default 0.2 = 20%) is configured in config.ini as
                         * light_bounce_attenuation. */
                        contribution *= config_get()->light_bounce_attenuation;
                    }

#ifdef USE_LIGHTING_CACHE
                    /* Store result in cache */
                    LightSampleResult result;
                    result.blocked = blocked;
                    result.distance = dist;
                    result.attenuation = intensity;
                    result.intensity = contribution;
                    if (!optical_current) lighting_cache_store(key, result);
#endif

                    /* Accumulate into the map's light map.
                     * Multiple lights add their contributions together, so a tile
                     * lit by two lights will be brighter than one lit by a single light. */
                    map->light_map[y * map->width + x] += contribution;
                }
            }
        }
    }

    LIGHTING_TIME_END();
}

void lighting_update(Map *map, WorldState *world) {
    lighting_update_optical(map, world, NULL, 0U);
}