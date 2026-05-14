/**
 * raycast.c — 3D raycasting renderer (DDA algorithm + decal/light compositing)
 *
 * This is the visual heart of the engine.  It implements a Wolfenstein-3D-style
 * raycasting renderer that produces a first-person perspective view from a
 * top-down 2D tile map.
 *
 * The file contains three main components:
 *
 *   1. sample_decal()    — Tests whether a world-space point falls on a decal
 *                          surface and, if so, returns the glyph and colour.
 *
 *   2. raycast_fire()    — The core DDA (Digital Differential Analyzer) ray-march
 *                          algorithm.  Fires a ray along a given angle and returns
 *                          the nearest wall intersection (distance + coordinates).
 *
 *   3. raycast_render()  — The main per-frame render function.  For each column
 *                          of the screen grid, it:
 *     a. Fires a ray to find the nearest wall
 *     b. Draws the wall slice with correct height, glyph, lighting, and decals
 *     c. Draws the ceiling and floor with distance-based shading
 *     d. After all columns, renders floor/ceiling decals by projecting pattern cells
 *     e. Renders light sources as '*' billboards
 *
 * The coordinate system assumes:
 *   - Map grid: integer tiles (0,0) = top-left, X = column, Y = row
 *   - World positions: floating-point (x+0.5, y+0.5) = centre of tile (x, y)
 *   - Angles: 0 radians = +X/east, PI/2 = +Y, measured CCW
 */

#include "raycast.h"        /* RayResult, raycast_fire(), raycast_render() */
#include "config.h"          /* config_get() — raycast_max_distance,
                                side_shadow_attenuation */
#include <math.h>             /* cos(), sin(), tan(), atan(), atan2(), fabs(),
                                sqrt(), floor() */
#include "math.h"             /* PI, normalize_angle() */
#include <stdlib.h>           /* (included for future use) */
#include <string.h>           /* (included for future use) */
#include <stdio.h>            /* printf() — used only when DECAL_DEBUG_MODE is 1 */

/* ===================================================================
 *  Debug flag
 * =================================================================== */

/* Set to 1 to enable verbose printf output when a decal is hit.
 * Useful for debugging decal positioning and UV mapping. */
#define DECAL_DEBUG_MODE 0

/* ===================================================================
 *  Decal sampling (wall decals)
 * =================================================================== */

/**
 * sample_decal() — Test whether a world-space point lies on a decal surface
 *
 * This is the core decal-hit-test function for WALL decals.  It:
 *   1. Computes the vector from the decal origin to the world point
 *   2. Builds a local coordinate system (tangent, bitangent, normal) based
 *      on the decal's surface type and rotation
 *   3. Rejects the point if the surface normal doesn't face the decal normal
 *      (dot product < 0.5) — prevents decals from being visible from the back
 *   4. Projects the world point into decal-local UV coordinates
 *   5. Checks if (u, v) is within the decal's [0,1) bounds and within depth
 *   6. Samples the decal's pattern at the quantised (px, py) cell
 *   7. If a non-space glyph is found, sets out_glyph and out_fg with the
 *      properly lit colour from the decal's material palette
 *
 * @param d           Pointer to the Decal to test
 * @param world_x     World-space X of the point being tested
 * @param world_y     World-space Y of the point being tested
 * @param world_z     World-space Z (height) of the point being tested
 * @param surf_nx     Surface normal X at the hit point (from raycast)
 * @param surf_ny     Surface normal Y at the hit point
 * @param surf_nz     Surface normal Z at the hit point (always 0 for walls)
 * @param out_glyph   Output: the glyph character to render (if hit)
 * @param out_fg      Output: the foreground colour to render (if hit)
 * @param assets      AssetRegistry for looking up palettes and materials
 * @param distance    Distance from camera (for distance-based palette sampling)
 * @param light_level Current light level at the wall tile
 * @return            true if the point hits the decal, false otherwise
 */
static bool sample_decal(Decal *d, double world_x, double world_y, double world_z,
                        double surf_nx, double surf_ny, double surf_nz,
                        uint8_t *out_glyph, SDL_Color *out_fg, AssetRegistry *assets,
                        double distance, double light_level) {

    /* ---- Vector from decal origin to world point ---- */
    double dx = world_x - d->x;
    double dy = world_y - d->y;
    double dz = world_z - d->z;

    /* ---- Build decal-local coordinate system ---- */
    /* Each decal has three orthogonal axes:
     *   Normal (N)   — points out of the surface
     *   Tangent (T)  — horizontal along the surface
     *   Bitangent (B) — vertical along the surface (downward for walls)
     *
     * These are pre-rotated by d->rotation around the normal axis. */

    double decal_nx = 0, decal_ny = 0, decal_nz = 0;
    double decal_tx = 0, decal_ty = 0, decal_tz = 0;
    double decal_bx = 0, decal_by = 0, decal_bz = 0;

    double cos_r = cos(d->rotation);
    double sin_r = sin(d->rotation);

    switch (d->surface) {
        case DECAL_SURFACE_WALL:
            /* Wall decal: N = horizontal (in the X/Y plane), T = perpendicular
             * to N in the X/Y plane, B = +Z (downward) */
            decal_nx = cos_r; decal_ny = sin_r; decal_nz = 0;
            decal_tx = -sin_r; decal_ty = cos_r; decal_tz = 0;
            decal_bx = 0; decal_by = 0; decal_bz = 1.0;
            break;

        case DECAL_SURFACE_FLOOR:
            /* Floor decal: N = +Z (upward), T = +X, B = +Y (both rotated) */
            decal_nx = 0; decal_ny = 0; decal_nz = 1.0;
            decal_tx = cos_r; decal_ty = sin_r; decal_tz = 0;
            decal_bx = -sin_r; decal_by = cos_r; decal_bz = 0;
            break;

        default: // CEILING
            /* Ceiling decal: N = -Z (downward), T = +X, B = +Y (both rotated) */
            decal_nx = 0; decal_ny = 0; decal_nz = -1.0;
            decal_tx = cos_r; decal_ty = sin_r; decal_tz = 0;
            decal_bx = -sin_r; decal_by = cos_r; decal_bz = 0;
            break;
    }

    /* ---- Normal filtering ---- */
    /* Dot product of the surface normal and decal normal.
     * If the surface is facing away from the decal (nd < 0.5), we reject
     * the hit — this prevents the decal from rendering on walls facing
     * the "wrong" way (e.g. a poster on the north side of a wall visible
     * from the south side through the backface). */
    double nd = surf_nx * decal_nx + surf_ny * decal_ny + surf_nz * decal_nz;
    if (nd < 0.5) return false;

    /* ---- Project world position into decal-local coordinates ---- */
    /* Local X = distance along decal tangent (horizontal on the decal) */
    double local_x = dx * decal_tx + dy * decal_ty + dz * decal_tz;
    /* Local Y = distance along decal bitangent (vertical on the decal) */
    double local_y = dx * decal_bx + dy * decal_by + dz * decal_bz;
    /* Local Z = signed distance from the decal plane (depth) */
    double local_z = dx * decal_nx + dy * decal_ny + dz * decal_nz;

    /* ---- UV coordinate calculation and bounds check ---- */
    /* Convert local position to normalised UV coordinates centred on the decal:
     *   u = 0 at left edge, u = 1 at right edge
     *   v = 0 at bottom edge, v = 1 at top edge */
    double u = local_x / d->width + 0.5;
    double v = local_y / d->height + 0.5;

    if (u < 0.0 || u >= 1.0 || v < 0.0 || v >= 1.0) return false;

    /* Depth clipping: reject if the point is too far in front of or behind
     * the decal plane (prevents the decal from "bleeding" onto walls that
     * are set back from the decal surface). */
    double depth_val = (d->depth > 0) ? d->depth : 0.1;
    if (fabs(local_z) > depth_val * 0.5) return false;

    /* ---- Sample the decal's pattern grid ---- */
    /* Quantise UV coordinates to integer (px, py) indices into the pattern */
    int px = (int)floor(u * d->pattern_cols);
    int py = (int)floor(v * d->pattern_rows);

    if (px >= 0 && px < d->pattern_cols && py >= 0 && py < d->pattern_rows) {
        PatternCell pc = d->pattern[py * d->pattern_cols + px];

        /* Skip empty/transparent cells */
        if (pc.glyph != ' ' && pc.glyph != '\0') {
            Material *d_mat = &assets->materials[pc.material_id];
            /* Sample the material's palette at this distance and light level */
            *out_fg = palette_sample(&assets->palettes[d_mat->palette_id], distance, light_level);
            *out_glyph = pc.glyph;

            if (DECAL_DEBUG_MODE) {
                printf("Decal Hit: surface=%d d=(%.2f, %.2f, %.2f) world=(%.2f, %.2f, %.2f) "
                       "local=(%.2f, %.2f, %.2f) uv=(%.2f, %.2f) px=%d py=%d\n",
                       d->surface, d->x, d->y, d->z, world_x, world_y, world_z,
                       local_x, local_y, local_z, u, v, px, py);
            }

            return true;
        }
    }

    return false; /* No glyph at this cell or out of range */
}

/* ===================================================================
 *  DDA Raycasting
 * =================================================================== */

/**
 * raycast_fire() — DDA (Digital Differential Analyzer) ray-march against the map
 *
 * Fires a single ray from the camera position at the given angle and marches
 * it through the map grid using the DDA algorithm until it either:
 *   - Hits a solid wall (material_id > 0), or
 *   - Exceeds max_dist (no wall found).
 *
 * The DDA algorithm works by stepping tile-by-tile along the ray, alternating
 * between X and Y axis steps based on which side distance is smaller.  This is
 * the classic algorithm from Lode's Raycasting Tutorial and Wolfenstein 3D.
 *
 * @param map       The map to ray-march through (must not be NULL)
 * @param cam       The camera (used for starting position only; angle is overridden)
 * @param ray_angle The direction to fire the ray (radians)
 * @param max_dist  Maximum ray distance (walls beyond this are ignored)
 * @return          RayResult struct with hit info (or hit=false if no wall found)
 */
RayResult raycast_fire(Map *map, Camera *cam, double ray_angle, double max_dist) {
    RayResult res = {0};
    res.hit = false;
    res.distance = max_dist;

    if (!map || !cam) return res;

    /* ---- Direction vector for this ray ---- */
    double dir_x = cos(ray_angle);
    double dir_y = sin(ray_angle);

    /* ---- Starting map tile (integer coordinates) ---- */
    int map_x = (int)cam->transform.pos.x;
    int map_y = (int)cam->transform.pos.y;

    /* ---- Delta distances ---- */
    /* How far along the ray we must travel to cross from one X-side to the next.
     * Formula: delta_dist = |1 / dir| — derived from the ray equation.
     * Guard against division by zero by using a very small number (1e-30). */
    double delta_dist_x = fabs(1.0 / (dir_x == 0 ? 1e-30 : dir_x));
    double delta_dist_y = fabs(1.0 / (dir_y == 0 ? 1e-30 : dir_y));

    /* ---- Step direction and initial side distances ---- */
    /* step_x/step_y = which direction (+1 or -1) we step in each axis.
     * side_dist_x/side_dist_y = distance from the starting position to the
     * next tile boundary in each axis, measured in ray-length units. */
    double side_dist_x, side_dist_y;
    int step_x, step_y;

    if (dir_x < 0) {
        step_x = -1;
        side_dist_x = (cam->transform.pos.x - map_x) * delta_dist_x;
    } else {
        step_x = 1;
        side_dist_x = (map_x + 1.0 - cam->transform.pos.x) * delta_dist_x;
    }

    if (dir_y < 0) {
        step_y = -1;
        side_dist_y = (cam->transform.pos.y - map_y) * delta_dist_y;
    } else {
        step_y = 1;
        side_dist_y = (map_y + 1.0 - cam->transform.pos.y) * delta_dist_y;
    }

    /* ---- DDA loop ---- */
    bool hit = false;
    int side = 0;          /* 0 = NS wall hit (X-aligned), 1 = EW wall hit (Y-aligned) */
    double perp_wall_dist = 0;

    while (!hit && perp_wall_dist < max_dist) {
        /* Jump to the next tile boundary — whichever axis has the smaller
         * side_dist value gets stepped first (this is the DDA core). */
        if (side_dist_x < side_dist_y) {
            side_dist_x += delta_dist_x;
            map_x += step_x;
            side = 0;  /* Hit a wall that runs North-South (X-aligned) */
            perp_wall_dist = side_dist_x - delta_dist_x;
        } else {
            side_dist_y += delta_dist_y;
            map_y += step_y;
            side = 1;  /* Hit a wall that runs East-West (Y-aligned) */
            perp_wall_dist = side_dist_y - delta_dist_y;
        }

        /* Check if the stepped-to tile is within the map bounds */
        if (!map_in_bounds(map, map_x, map_y)) {
            break;   /* Walked off the map edge — no wall hit */
        }

        /* Check if this tile is solid (material_id > 0) */
        MapCell *cell = map_get(map, map_x, map_y);
        if (cell && cell->material_id > 0) {
            hit = true;
        }
    }

    /* ---- Populate result struct ---- */
    if (hit && perp_wall_dist < max_dist) {
        res.hit      = true;
        res.distance = perp_wall_dist;    /* Perpendicular distance (not Euclidean) */
        res.map_x    = map_x;
        res.map_y    = map_y;
        res.side     = side;
    } else {
        res.hit      = false;
        res.distance = max_dist;
    }

    return res;
}

/* ===================================================================
 *  Main render function — the full 3D view
 * =================================================================== */

/**
 * raycast_render() — Render a complete 3D frame into the Grid
 *
 * This is the main rendering entry point, called once per frame from app.c.
 * For each column of the grid, it:
 *
 *   1. Calculates the ray angle for this column (from FOV and column index)
 *   2. Fires a ray via raycast_fire()
 *   3. If a wall is hit, computes the wall slice height and draws it,
 *      applying distance-based glyph selection and lighting
 *   4. Checks wall decals at each pixel of the wall slice
 *   5. Draws ceiling and floor with distance-based shading from the light map
 *
 * After all columns are processed, a second pass renders:
 *   6. Floor/ceiling decals (discrete sprites projected into screen space)
 *   7. Light source '*' markers
 *
 * @param grid    The character grid to render into
 * @param map     The tile map (for wall data + light_map)
 * @param cam     The camera (position, angle, FOV, pitch)
 * @param assets  Asset registry (palettes, materials)
 * @param world   World state (lights, decals)
 */
void raycast_render(Grid *grid, Map *map, Camera *cam, AssetRegistry *assets, WorldState *world) {
    if (!grid || !map || !cam || !assets || !world) return;

    /* ---- Z-buffer: stores the camera-forward perpendicular distance of the
     *     nearest wall for each column.  Used later for decal/light occlusion.
     *     Hardcoded max width of 1024 columns. ---- */
    double z_buffer[1024];
    int max_x_idx = grid->width < 1024 ? grid->width : 1024;

    /* ================================================================
     *  MAIN RAYCASTING LOOP — one iteration per screen column
     * ================================================================ */

    for (int x = 0; x < max_x_idx; x++) {
        /* ---- a. Calculate ray angle for this column ---- */
        /* Map the grid column index to a normalised camera-space X:
         *   -1 = left edge of screen, 0 = centre, +1 = right edge */
        double camera_x = 2 * (x + 0.5) / (double)grid->width - 1;

        /* Compute the actual world angle for this column based on the
         * camera's forward direction and FOV.  The atan(tan(fov/2) * cx)
         * gives the angle offset from centre. */
        double ray_angle = cam->transform.angle + atan(camera_x * tan(cam->fov / 2.0));

        /* ---- Direction vector for this ray column ---- */
        double dir_x = cos(ray_angle);
        double dir_y = sin(ray_angle);

        /* ---- b. Fire the ray ---- */
        RayResult ray = raycast_fire(map, cam, ray_angle, config_get()->raycast_max_distance);

        int line_height = 0;
        int material_id = 0;
        double perp_dist = ray.distance;

        /* ---- c. Compute wall slice properties ---- */
        if (ray.hit) {
            MapCell *cell = map_get(map, ray.map_x, ray.map_y);
            if (cell) material_id = cell->material_id;

            /* Ray direction vector for this column (used in wall decal
             * reconstruction, ceiling, and floor rendering below) */
            dir_x = cos(ray_angle);
            dir_y = sin(ray_angle);

            /* Correct the along-ray distance to avoid fisheye distortion.
             * raycast_fire() returns distance along the ray; multiplying by
             * cos(ray_angle - camera_angle) converts it to camera-forward
             * perpendicular distance so walls remain visually straight. */
            perp_dist = ray.distance * cos(ray_angle - cam->transform.angle);
            if (perp_dist < 0.001) perp_dist = 0.001;  /* Prevent division by zero */

            /* Wall slice height in cells: taller = closer */
            line_height = (int)(grid->height / perp_dist);
        }

        /* Store in z-buffer for decal/light occlusion checks */
        z_buffer[x] = perp_dist;

        /* ---- Vertical bounds of the wall slice ---- */
        /* Centre the wall vertically and apply camera pitch (looking up/down) */
        int draw_start = -line_height / 2 + grid->height / 2 + (int)cam->pitch;
        if (draw_start < 0) draw_start = 0;
        int draw_end = line_height / 2 + grid->height / 2 + (int)cam->pitch;
        if (draw_end >= grid->height) draw_end = grid->height - 1;

        /* ================================================================
         *  WALL RENDERING
         * ================================================================ */
        if (ray.hit) {
            Material *mat = &assets->materials[material_id];

            /* Distance-based glyph selection:
             *   0–4  cells away → glyphs[0] (near, most detailed)
             *   4–7  cells away → glyphs[1]
             *   7–10 cells away → glyphs[2]
             *   10+  cells away → glyphs[3] (far, simplest) */
            int glyph_idx = 0;
            if (ray.distance > 10.0) glyph_idx = 3;
            else if (ray.distance > 7.0) glyph_idx = 2;
            else if (ray.distance > 4.0) glyph_idx = 1;

            uint8_t wall_glyph = mat->glyphs[glyph_idx];

            /* ---- Lighting for this wall tile ---- */
            double light_level = 1.0;
            if (map->light_map) {
                light_level = map->light_map[ray.map_y * map->width + ray.map_x];
            }
            /* East-West facing walls (side == 1) are dimmer to simulate
             * directional shadowing — light hits them at a grazing angle. */
            if (ray.side == 1) light_level *= config_get()->side_shadow_attenuation;

            /* Sample the wall colour from the material's palette */
            SDL_Color wall_color = palette_sample(&assets->palettes[mat->palette_id],
                                                   ray.distance, light_level);

            /* Exact (floating-point) line height for UV calculation */
            double exact_line_height = grid->height / perp_dist;

            /* ---- Draw each pixel of the wall slice ---- */
            for (int y = draw_start; y <= draw_end; y++) {
                /* Vertical texture coordinate (v): 0.0 = top, 1.0 = bottom */
                double v = (y - (grid->height / 2.0 + cam->pitch - exact_line_height / 2.0))
                            / exact_line_height;

                uint8_t glyph = wall_glyph;
                SDL_Color fg = wall_color;
                SDL_Color bg = {0, 0, 0, 255};

                /* ---- Wall decal check ---- */
                /* Reconstruct the world-space coordinates of this wall pixel
                 * so we can test it against wall decals. */
                double world_x = cam->transform.pos.x + ray.distance * dir_x;
                double world_y = cam->transform.pos.y + ray.distance * dir_y;
                double world_z = v;   /* Height on the wall */

                /* Surface normal (pointing away from the wall face) */
                int step_x = (dir_x > 0) ? 1 : -1;
                int step_y = (dir_y > 0) ? 1 : -1;
                double surf_nx = (ray.side == 0) ? -step_x : 0;
                double surf_ny = (ray.side == 1) ? -step_y : 0;
                double surf_nz = 0;

                /* Iterate over all wall decals and check each one */
                for (int i = 0; i < world->num_decals; i++) {
                    Decal *d = &world->decals[i];
                    if (d->surface == DECAL_SURFACE_WALL) {
                        if (sample_decal(d, world_x, world_y, world_z,
                                         surf_nx, surf_ny, surf_nz,
                                         &glyph, &fg, assets,
                                         ray.distance, light_level)) {
                            break;   /* First matching decal wins */
                        }
                    }
                }

                grid_set(grid, x, y, glyph, fg, bg);
            }
        }

        /* ================================================================
         *  CEILING RENDERING
         * ================================================================ */
        /* The ceiling is rendered as a plane above the camera using inverse
         * perspective projection.  Rows closer to the horizon are farther away. */
        for (int y = 0; y < draw_start; y++) {
            double denom = grid->height - 2.0 * (y - cam->pitch);
            if (fabs(denom) < 0.001) denom = 0.001;
            double currentDist = grid->height / denom;
            double trueDist = currentDist / cos(ray_angle - cam->transform.angle);

            /* World coordinates of this ceiling point */
            double curX = cam->transform.pos.x + trueDist * dir_x;
            double curY = cam->transform.pos.y + trueDist * dir_y;

            uint8_t glyph = ' ';
            SDL_Color fg = {255, 255, 255, 255};
            SDL_Color bg = {50, 50, 50, 255};   /* Grey ceiling */

            /* Apply light map */
            if (map->light_map) {
                int map_x = (int)curX;
                int map_y = (int)curY;
                if (map_in_bounds(map, map_x, map_y)) {
                    double light_level = map->light_map[map_y * map->width + map_x];
                    if (light_level > 1.0) light_level = 1.0;
                    fg.r = (uint8_t)(fg.r * light_level);
                    fg.g = (uint8_t)(fg.g * light_level);
                    fg.b = (uint8_t)(fg.b * light_level);
                    bg.r = (uint8_t)(bg.r * light_level);
                    bg.g = (uint8_t)(bg.g * light_level);
                    bg.b = (uint8_t)(bg.b * light_level);
                }
            }

            grid_set(grid, x, y, glyph, fg, bg);
        }

        /* ================================================================
         *  FLOOR RENDERING
         * ================================================================ */
        for (int y = draw_end + 1; y < grid->height; y++) {
            double denom = 2.0 * (y - cam->pitch) - grid->height;
            if (fabs(denom) < 0.001) denom = 0.001;
            double currentDist = grid->height / denom;
            double trueDist = currentDist / cos(ray_angle - cam->transform.angle);

            double curX = cam->transform.pos.x + trueDist * dir_x;
            double curY = cam->transform.pos.y + trueDist * dir_y;

            uint8_t glyph = ' ';
            SDL_Color fg = {255, 255, 255, 255};
            SDL_Color bg = {30, 30, 30, 255};   /* Darker floor */

            if (map->light_map) {
                int map_x = (int)curX;
                int map_y = (int)curY;
                if (map_in_bounds(map, map_x, map_y)) {
                    double light_level = map->light_map[map_y * map->width + map_x];
                    if (light_level > 1.0) light_level = 1.0;
                    fg.r = (uint8_t)(fg.r * light_level);
                    fg.g = (uint8_t)(fg.g * light_level);
                    fg.b = (uint8_t)(fg.b * light_level);
                    bg.r = (uint8_t)(bg.r * light_level);
                    bg.g = (uint8_t)(bg.g * light_level);
                    bg.b = (uint8_t)(bg.b * light_level);
                }
            }

            grid_set(grid, x, y, glyph, fg, bg);
        }
    }

    /* ================================================================
     *  POST-PASS: DISCRETE DECALS (Floor/Ceiling)
     *  Rendered by projecting each decal pattern cell into screen space.
     * ================================================================ */

    for (int i = 0; i < world->num_decals; i++) {
        Decal *d = &world->decals[i];
        if (d->surface == DECAL_SURFACE_WALL) continue; /* Already handled above */

        /* Build rotation matrix for this decal's orientation */
        double cos_r = cos(d->rotation);
        double sin_r = sin(d->rotation);

        /* Base Tangent (+X) and Bitangent (+Y) for the ground plane */
        double tx = cos_r, ty = sin_r;
        double bx = -sin_r, by = cos_r;

        /* Iterate over each cell in the decal's pattern grid */
        for (int py = 0; py < d->pattern_rows; py++) {
            for (int px = 0; px < d->pattern_cols; px++) {
                PatternCell pc = d->pattern[py * d->pattern_cols + px];
                if (pc.glyph == ' ' || pc.glyph == '\0') continue; /* Skip empty */

                /* UV coordinate of the centre of this pattern cell */
                double u = (px + 0.5) / d->pattern_cols;
                double v = (py + 0.5) / d->pattern_rows;

                /* Local coordinates relative to decal centre */
                double local_x = (u - 0.5) * d->width;
                double local_y = (v - 0.5) * d->height;

                /* World coordinates */
                double world_x = d->x + local_x * tx + local_y * bx;
                double world_y = d->y + local_x * ty + local_y * by;

                /* ---- Project world point into screen space ---- */
                double dx = world_x - cam->transform.pos.x;
                double dy = world_y - cam->transform.pos.y;

                /* Camera basis vectors */
                double dir_x = cos(cam->transform.angle);
                double dir_y = sin(cam->transform.angle);
                double plane_x = -sin(cam->transform.angle) * tan(cam->fov / 2.0);
                double plane_y =  cos(cam->transform.angle) * tan(cam->fov / 2.0);

                /* Transform world point into camera space using the camera's
                 * basis, similar to a sprite/decal projection matrix:
                 *   transform_x = horizontal screen position
                 *   transform_y = depth (distance from camera plane) */
                double inv_det = 1.0 / (plane_x * dir_y - dir_x * plane_y);
                double transform_x = inv_det * (dir_y * dx - dir_x * dy);
                double transform_y = inv_det * (-plane_y * dx + plane_x * dy);

                if (transform_y <= 0) continue; /* Behind the camera — skip */

                /* Screen X coordinate */
                int screen_x = (int)((grid->width / 2.0) * (1.0 + transform_x / transform_y));
                if (screen_x < 0 || screen_x >= grid->width) continue;

                /* Z-buffer occlusion check */
                if (transform_y >= z_buffer[screen_x]) continue; /* Hidden behind a wall */

                /* ---- Compute screen Y for floor vs ceiling ---- */
                int screen_y;
                if (d->surface == DECAL_SURFACE_FLOOR) {
                    double floor_dist = transform_y;
                    double denom = grid->height / floor_dist;
                    screen_y = (int)((denom + grid->height) / 2.0 + cam->pitch);
                } else { /* CEILING */
                    double ceil_dist = transform_y;
                    double denom = grid->height / ceil_dist;
                    screen_y = (int)((grid->height - denom) / 2.0 + cam->pitch);
                }

                /* ---- Draw the decal cell on screen ---- */
                if (screen_y >= 0 && screen_y < grid->height) {
                    Cell existing;
                    if (grid_get(grid, screen_x, screen_y, &existing)) {
                        Material *d_mat = &assets->materials[pc.material_id];

                        /* Lighting */
                        double light_level = 1.0;
                        if (map->light_map && map_in_bounds(map, (int)world_x, (int)world_y)) {
                            light_level = map->light_map[(int)world_y * map->width + (int)world_x];
                            if (light_level > 1.0) light_level = 1.0;
                        }

                        SDL_Color fg = palette_sample(&assets->palettes[d_mat->palette_id],
                                                       transform_y, light_level);
                        grid_set(grid, screen_x, screen_y, pc.glyph, fg, existing.bg);
                    }
                }
            }
        }
    }

    /* ================================================================
     *  POST-PASS: LIGHT SOURCE RENDERING
     *  Renders each light source as a '*' billboard in the world.
     * ================================================================ */

    for (int i = 0; i < world->num_lights; i++) {
        Light *l = &world->lights[i];

        /* Vector from camera to light */
        double sprite_x = l->pos.x - cam->transform.pos.x;
        double sprite_y = l->pos.y - cam->transform.pos.y;

        /* Angle from camera forward direction to the light */
        double angle_to_light = atan2(sprite_y, sprite_x);
        double angle_diff = angle_to_light - cam->transform.angle;

        /* Normalise angle difference to [-PI, PI] */
        while (angle_diff > PI)  angle_diff -= 2.0 * PI;
        while (angle_diff < -PI) angle_diff += 2.0 * PI;

        /* Skip if the light is behind the camera or at an extreme angle */
        if (cos(angle_diff) < 0.1) continue;

        /* Distance and screen X projection */
        double dist = sqrt(sprite_x * sprite_x + sprite_y * sprite_y);
        double camera_x = tan(angle_diff) / tan(cam->fov / 2.0);
        int screen_x = (int)((grid->width / 2.0) * (1.0 + camera_x));

        if (screen_x < 0 || screen_x >= grid->width) continue;

        /* Z-buffer check */
        double perp_dist = dist * cos(angle_diff);
        if (perp_dist >= z_buffer[screen_x]) continue; /* Hidden behind a wall */

        /* Render at eye-height (centre of screen + pitch) */
        int draw_y = grid->height / 2 + (int)cam->pitch;

        if (draw_y >= 0 && draw_y < grid->height) {
            Cell existing;
            if (grid_get(grid, screen_x, draw_y, &existing)) {
                /* Draw '*' in the light's colour on top of whatever's already there */
                grid_set(grid, screen_x, draw_y, '*', l->color, existing.bg);
            }
        }
    }
}