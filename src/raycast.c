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
 *     b. Draws the wall slice with correct height, glyph, and lighting
 *     c. Draws the ceiling and floor with distance-based shading
 *     d. Projects decal cells from surface-local space into screen cells
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
 *  Decal projection helpers
 * =================================================================== */

static void decal_basis(const Decal *d,
                        double *nx, double *ny, double *nz,
                        double *tx, double *ty, double *tz,
                        double *bx, double *by, double *bz) {
    double cos_r = cos(d->rotation);
    double sin_r = sin(d->rotation);

    switch (d->surface) {
        case DECAL_SURFACE_WALL:
            *nx = cos_r; *ny = sin_r; *nz = 0.0;
            *tx = sin_r; *ty = -cos_r; *tz = 0.0;
            *bx = 0.0;   *by = 0.0;    *bz = -1.0;
            break;
        case DECAL_SURFACE_FLOOR:
            *nx = 0.0;   *ny = 0.0;   *nz = 1.0;
            *tx = cos_r; *ty = sin_r; *tz = 0.0;
            *bx = -sin_r; *by = cos_r; *bz = 0.0;
            break;
        default:
            *nx = 0.0;   *ny = 0.0;   *nz = -1.0;
            *tx = cos_r; *ty = sin_r; *tz = 0.0;
            *bx = -sin_r; *by = cos_r; *bz = 0.0;
            break;
    }
}

static bool project_world_point(Grid *grid, Camera *cam,
                                double world_x, double world_y, double world_z,
                                double *screen_x, double *screen_y, double *depth) {
    double dx = world_x - cam->transform.pos.x;
    double dy = world_y - cam->transform.pos.y;
    double dir_x = cos(cam->transform.angle);
    double dir_y = sin(cam->transform.angle);
    double plane_x = -sin(cam->transform.angle) * tan(cam->fov / 2.0);
    double plane_y =  cos(cam->transform.angle) * tan(cam->fov / 2.0);
    double det = plane_x * dir_y - dir_x * plane_y;

    if (fabs(det) < 0.000001) return false;

    double inv_det = 1.0 / det;
    double transform_x = inv_det * (dir_y * dx - dir_x * dy);
    double transform_y = inv_det * (-plane_y * dx + plane_x * dy);

    if (transform_y <= 0.001) return false;

    *screen_x = (grid->width / 2.0) * (1.0 + transform_x / transform_y);
    *screen_y = grid->height / 2.0 + cam->pitch +
                (0.5 - world_z) * grid->height / transform_y;
    *depth = transform_y;

    return true;
}

static double decal_light_level(Map *map, const Decal *d,
                                double world_x, double world_y,
                                double normal_y) {
    double light_level = 1.0;

    if (map->light_map) {
        int map_x = (int)world_x;
        int map_y = (int)world_y;
        if (map_in_bounds(map, map_x, map_y)) {
            light_level = map->light_map[map_y * map->width + map_x];
            if (light_level > 1.0) light_level = 1.0;
        }
    }

    if (d->surface == DECAL_SURFACE_WALL && fabs(normal_y) > 0.5) {
        light_level *= config_get()->side_shadow_attenuation;
    }

    return light_level;
}

static void render_decals(Grid *grid, Map *map, Camera *cam,
                          AssetRegistry *assets, WorldState *world,
                          const double *z_buffer, int z_count) {
    const double camera_z = 0.5;
    static double decal_depth[1024 * 1024];
    static int decal_order[1024 * 1024];
    int cell_count = grid->width * grid->height;

    if (cell_count > (int)(sizeof(decal_depth) / sizeof(decal_depth[0]))) return;

    for (int i = 0; i < cell_count; i++) {
        decal_depth[i] = 1.0e30;
        decal_order[i] = 2147483647;
    }

    for (int i = 0; i < world->num_decals; i++) {
        Decal *d = &world->decals[i];
        if (!d->pattern || d->pattern_cols <= 0 || d->pattern_rows <= 0) continue;
        if (d->width <= 0.0 || d->height <= 0.0) continue;

        double nx, ny, nz, tx, ty, tz, bx, by, bz;
        decal_basis(d, &nx, &ny, &nz, &tx, &ty, &tz, &bx, &by, &bz);

        double view_x = cam->transform.pos.x - d->x;
        double view_y = cam->transform.pos.y - d->y;
        double view_z = camera_z - d->z;
        if (view_x * nx + view_y * ny + view_z * nz <= 0.0) continue;

        for (int py = 0; py < d->pattern_rows; py++) {
            for (int px = 0; px < d->pattern_cols; px++) {
                PatternCell pc = d->pattern[py * d->pattern_cols + px];
                if (pc.glyph == ' ' || pc.glyph == '\0') continue;

                double glyph_step_u = d->glyph_step_u > 0.0 ?
                                      d->glyph_step_u : d->width / (double)d->pattern_cols;
                double glyph_step_v = d->glyph_step_v > 0.0 ?
                                      d->glyph_step_v : d->height / (double)d->pattern_rows;
                double local_u = ((double)px - ((double)d->pattern_cols - 1.0) * 0.5) * glyph_step_u;
                double local_v = ((double)py - ((double)d->pattern_rows - 1.0) * 0.5) * glyph_step_v;
                double world_x = d->x + local_u * tx + local_v * bx;
                double world_y = d->y + local_u * ty + local_v * by;
                double world_z = d->z + local_u * tz + local_v * bz;
                double screen_x_f, screen_y_f, depth;

                if (!project_world_point(grid, cam, world_x, world_y, world_z,
                                         &screen_x_f, &screen_y_f, &depth)) {
                    continue;
                }

                int screen_x = (int)floor(screen_x_f);
                int screen_y = (int)floor(screen_y_f);

                if (screen_x < 0 || screen_x >= grid->width ||
                    screen_y < 0 || screen_y >= grid->height) {
                    continue;
                }
                if (screen_x >= z_count) continue;
                if (depth > z_buffer[screen_x] + 0.001) continue;

                int cell_index = screen_y * grid->width + screen_x;
                int source_order = i * 1000000 + py * d->pattern_cols + px;
                if (depth > decal_depth[cell_index] + 0.000001) continue;
                if (fabs(depth - decal_depth[cell_index]) <= 0.000001 &&
                    source_order >= decal_order[cell_index]) {
                    continue;
                }

                Cell existing;
                if (!grid_get(grid, screen_x, screen_y, &existing)) continue;

                Material *d_mat = &assets->materials[pc.material_id];
                double light_level = decal_light_level(map, d, world_x, world_y, ny);
                SDL_Color fg = palette_sample(&assets->palettes[d_mat->palette_id],
                                               depth, light_level);
                grid_set(grid, screen_x, screen_y, pc.glyph, fg, existing.bg);
                decal_depth[cell_index] = depth;
                decal_order[cell_index] = source_order;

                if (DECAL_DEBUG_MODE) {
                    printf("Decal Anchor: surface=%d px=%d py=%d world=(%.2f, %.2f, %.2f) screen=(%d, %d)\n",
                           d->surface, px, py, world_x, world_y, world_z, screen_x, screen_y);
                }
            }
        }
    }
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
 *   6. Light source '*' markers
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

            /* ---- Draw each pixel of the wall slice ---- */
            for (int y = draw_start; y <= draw_end; y++) {
                uint8_t glyph = wall_glyph;
                SDL_Color fg = wall_color;
                SDL_Color bg = {0, 0, 0, 255};

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
            double light_level = 1.0;
            if (map->light_map) {
                int map_x = (int)curX;
                int map_y = (int)curY;
                if (map_in_bounds(map, map_x, map_y)) {
                    light_level = map->light_map[map_y * map->width + map_x];
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

            double light_level = 1.0;
            if (map->light_map) {
                int map_x = (int)curX;
                int map_y = (int)curY;
                if (map_in_bounds(map, map_x, map_y)) {
                    light_level = map->light_map[map_y * map->width + map_x];
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

    render_decals(grid, map, cam, assets, world, z_buffer, max_x_idx);

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