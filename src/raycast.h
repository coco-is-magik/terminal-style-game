/**
 * raycast.h — 3D raycasting renderer (DDA algorithm)
 *
 * Declares the RayResult struct, the DDA ray-march function, and the
 * main per-frame render function that produces the first-person view.
 *
 * See raycast.c for the implementation.
 */

#ifndef RAYCAST_H
#define RAYCAST_H

#include "map.h"       /* Map — tile grid for wall detection */
#include "camera.h"    /* Camera — viewpoint for ray origin */
#include "grid.h"      /* Grid — render target (output framebuffer) */
#include "assets.h"    /* AssetRegistry — palettes and materials */
#include "world.h"     /* WorldState — lights and decals */

/**
 * RayResult — The result of firing a single ray into the map
 *
 * hit      — true if the ray hit a solid wall (material_id > 0)
 * distance — Distance along the ray from camera to the wall hit
 * map_x    — X coordinate of the wall tile that was hit
 * map_y    — Y coordinate of the wall tile that was hit
 * side     — Which axis was crossed: 0 = NS (X-aligned), 1 = EW (Y-aligned)
 */
typedef struct {
    bool hit;                /* Did the ray hit a wall? */
    double distance;         /* Along-ray wall distance */
    int map_x;               /* Hit tile X coordinate */
    int map_y;               /* Hit tile Y coordinate */
    int side;                /* Wall orientation: 0 = NS, 1 = EW */
} RayResult;

/**
 * raycast_fire() — DDA ray-march against the map
 *
 * Fires a single ray from the camera at the given angle and marches
 * it through the map grid.  Returns the nearest wall intersection
 * (or hit=false if no wall is found within max_dist).
 *
 * @param map       The tile map to march through
 * @param cam       Camera (provides starting position)
 * @param ray_angle Direction to fire the ray (radians)
 * @param max_dist  Maximum search distance
 * @return          RayResult with hit details
 */
RayResult raycast_fire(Map *map, Camera *cam, double ray_angle, double max_dist);

/**
 * raycast_render() — Render a complete 3D frame into the Grid
 *
 * The main per-frame render function.  For each column:
 *   1. Fires a ray to find the nearest wall
 *   2. Draws the wall slice (glyph, lighting, decals)
 *   3. Draws ceiling and floor with distance-based shading
 *
 * Post-passes render floor/ceiling decals and light markers.
 *
 * @param grid    The character grid to render into
 * @param map     The tile map (walls + light_map)
 * @param cam     The camera (position, angle, FOV, pitch)
 * @param assets  Asset registry (palettes, materials)
 * @param world   World state (lights, decals)
 */
void raycast_render(Grid *grid, Map *map, Camera *cam, AssetRegistry *assets, WorldState *world);

#endif /* RAYCAST_H */