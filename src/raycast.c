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
 *     d. Renders all world decals (wall, floor, and ceiling) projected
 *        onto the grid
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
#include "smc_render_opt.h"  /* SMC-generated hot-path wrappers */
#include "decal_projection.h"
#include "height_projection.h"
#include "heightfield_trace.h"
#include "raycast_internal.h"
#include <float.h>
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

/* Converts legacy decal footprint spacing (width / pattern_cols) into
 * terminal-friendly ASCII glyph-anchor spacing.  Without this factor
 * the step between adjacent anchors would span a full fraction of the
 * 3-D decal footprint and produce large visible gaps in the terminal. */
#define DEFAULT_DECAL_GLYPH_COMPRESSION 8.0
#define DECAL_OCCLUSION_EPSILON 0.01

#define WORLD_HIT_VALID_BIT (UINT64_C(1) << 63)
#define WORLD_HIT_GENERATED_BIT (UINT64_C(1) << 62)
#define WORLD_HIT_KIND_SHIFT 60U
#define WORLD_HIT_X_SHIFT 9U
#define WORLD_HIT_COORD_MASK UINT64_C(0x1FF)

uint64_t raycast_world_hit_key(const HeightfieldHit *hit) {
    if (!hit || !hit->hit || hit->map_x < 0 || hit->map_y < 0) return 0U;
    return WORLD_HIT_VALID_BIT |
        (hit->generated_boundary ? WORLD_HIT_GENERATED_BIT : 0U) |
        ((uint64_t)hit->kind << WORLD_HIT_KIND_SHIFT) |
        (((uint64_t)(unsigned int)hit->map_x & WORLD_HIT_COORD_MASK) <<
            WORLD_HIT_X_SHIFT) |
        ((uint64_t)(unsigned int)hit->map_y & WORLD_HIT_COORD_MASK);
}

static bool world_hit_is_surface(uint64_t key, HeightfieldHitKind kind,
                                 int map_x, int map_y, bool allow_generated) {
    return (key & WORLD_HIT_VALID_BIT) != 0U &&
        (allow_generated || (key & WORLD_HIT_GENERATED_BIT) == 0U) &&
        ((key >> WORLD_HIT_KIND_SHIFT) & UINT64_C(3)) == (uint64_t)kind &&
        (int)((key >> WORLD_HIT_X_SHIFT) & WORLD_HIT_COORD_MASK) == map_x &&
        (int)(key & WORLD_HIT_COORD_MASK) == map_y;
}

/* ===================================================================
 *  Decal projection helpers
 * =================================================================== */

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
    *screen_y = height_project_y(cam, grid->height, world_z, transform_y);
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

static double horizontal_true_distance(const SceneHeightView *heights,
                                       bool heights_valid, const Camera *cam,
                                       int viewport_height, double denominator,
                                       double legacy_distance, double ray_angle,
                                       bool floor_surface, int map_x, int map_y) {
    const SceneAuthoredCell *cell;
    double plane_z;
    double vertical_delta;
    double current_distance;
    if (!heights_valid || map_x < 0 || map_y < 0 ||
        map_x >= heights->width || map_y >= heights->height) {
        return legacy_distance;
    }
    cell = &heights->cells[(size_t)map_y * (size_t)heights->width + (size_t)map_x];
    if (cam->z == 0.5 &&
        ((floor_surface && cell->floor_height_step == SCENE_DEFAULT_FLOOR_HEIGHT_STEP) ||
         (!floor_surface &&
          cell->ceiling_height_step == SCENE_DEFAULT_CEILING_HEIGHT_STEP))) {
        return legacy_distance;
    }
    plane_z = scene_height_world(floor_surface
        ? cell->floor_height_step : cell->ceiling_height_step);
    vertical_delta = floor_surface ? cam->z - plane_z : plane_z - cam->z;
    if (!isfinite(vertical_delta) || vertical_delta <= 0.0) return legacy_distance;
    current_distance = 2.0 * vertical_delta * viewport_height / denominator;
    return smc_true_distance(current_distance, ray_angle, cam->transform.angle);
}

Cell raycast_sample_heightfield_hit(const Map *map,
                                    const AssetRegistry *assets,
                                    const HeightfieldHit *hit) {
    const SDL_Color darkness = {0, 0, 0, 255};
    Cell sampled = {' ', {0, 0, 0, 255}, {0, 0, 0, 255}};
    if (!map || !assets || !hit || !hit->hit) return sampled;
    if (material_id_is_loaded(assets, hit->material)) {
        const Material *material = &assets->materials[hit->material];
        double light_level = 1.0;
        int glyph_index = hit->distance > 10.0 ? 3 :
            hit->distance > 7.0 ? 2 : hit->distance > 4.0 ? 1 : 0;
        if (map->light_map && map_in_bounds(map, hit->map_x, hit->map_y)) {
            light_level = map->light_map[
                (size_t)hit->map_y * (size_t)map->width + (size_t)hit->map_x];
            if (light_level > 1.0) light_level = 1.0;
            if (light_level < 0.0) light_level = 0.0;
        }
        if ((hit->kind == HEIGHTFIELD_HIT_WALL || hit->generated_boundary) &&
            hit->side == 1) {
            light_level *= config_get()->side_shadow_attenuation;
        }
        sampled.glyph = material->glyphs[glyph_index];
        sampled.fg = palette_sample(
            &assets->palettes[material->palette_id], hit->distance, light_level);
        sampled.bg = darkness;
    } else {
        sampled.glyph = '.';
        sampled.fg = darkness;
        sampled.bg = (SDL_Color){128, 0, 255, 255};
    }
    return sampled;
}

double raycast_heightfield_column_depth(const HeightfieldTraceColumn *column) {
    double depth;
    size_t i;
    if (!column || !column->valid) return DBL_MAX;
    depth = column->max_distance;
    for (i = 0U; i < column->interval_count; i++) {
        int next_x = column->interval_next_x[i];
        int next_y = column->interval_next_y[i];
        if (map_in_bounds(column->map, next_x, next_y)) {
            size_t index = (size_t)next_y * (size_t)column->map->width +
                           (size_t)next_x;
            if (column->map->cells[index].material_id != 0 ||
                column->heights->cells[index].occupancy ==
                    SCENE_CELL_OCCUPANCY_WALL) {
                depth = column->interval_exit[i] * column->correction;
                break;
            }
        }
    }
    return depth;
}

void raycast_render_heightfield_opaque_impl(
    Grid *grid, Map *map, Camera *cam, AssetRegistry *assets,
    const SceneHeightView *heights, double *z_buffer
) {
    int x;
    for (x = 0; x < grid->width; x++) {
        HeightfieldTraceColumn column;
        int y;
        if (!heightfield_trace_prepare_column(
                &column, cam, map, heights, grid->width, grid->height, x,
                config_get()->raycast_max_distance)) continue;
        z_buffer[x] = raycast_heightfield_column_depth(&column);
        for (y = 0; y < grid->height; y++) {
            HeightfieldHit hit = heightfield_trace_prepared_sample(&column, y);
            size_t output_index = (size_t)y * (size_t)grid->width + (size_t)x;
            Cell sampled = raycast_sample_heightfield_hit(map, assets, &hit);
            {
                Cell *output = &grid->cells[output_index];
                *output = sampled;
                grid->world_depths[output_index] = hit.hit
                    ? hit.perpendicular_distance : DBL_MAX;
                grid->world_hit_keys[output_index] = raycast_world_hit_key(&hit);
            }
        }
    }
}

static void render_decals(Grid *grid, Map *map, Camera *cam,
                          AssetRegistry *assets, WorldState *world,
                          const double *z_buffer, int z_count,
                          const SceneHeightView *heights,
                          bool bounded_occlusion) {
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
        Decal projected;
        if (!d->pattern || d->pattern_cols <= 0 || d->pattern_rows <= 0) continue;
        if (d->width <= 0.0 || d->height <= 0.0) continue;

        projected = *d;
        if (d->surface != DECAL_SURFACE_WALL &&
            scene_height_view_is_valid(heights, map->width, map->height)) {
            int anchor_x = (int)floor(d->x);
            int anchor_y = (int)floor(d->y);
            const SceneAuthoredCell *cell;
            if (!map_in_bounds(map, anchor_x, anchor_y)) continue;
            cell = &heights->cells[
                (size_t)anchor_y * (size_t)map->width + (size_t)anchor_x];
            if ((d->surface == DECAL_SURFACE_FLOOR && !cell->floor_present) ||
                (d->surface == DECAL_SURFACE_CEILING && !cell->ceiling_present))
                continue;
            projected.z = scene_height_world(d->surface == DECAL_SURFACE_FLOOR
                ? cell->floor_height_step : cell->ceiling_height_step);
        }

        DecalBasis basis = decal_projection_basis(&projected);

        double view_x = cam->transform.pos.x - projected.x;
        double view_y = cam->transform.pos.y - projected.y;
        double view_z = cam->z - projected.z;
        if (view_x * basis.normal[0] + view_y * basis.normal[1] +
            view_z * basis.normal[2] <= 0.0) continue;

        for (int py = 0; py < d->pattern_rows; py++) {
            for (int px = 0; px < d->pattern_cols; px++) {
                PatternCell pc = d->pattern[py * d->pattern_cols + px];
                if (pc.glyph == ' ' || pc.glyph == '\0') continue;

                double world_x, world_y, world_z;
                decal_projection_glyph_world(&projected, &basis, px, py,
                                             DEFAULT_DECAL_GLYPH_COMPRESSION,
                                             &world_x, &world_y, &world_z);
                if (projected.surface != DECAL_SURFACE_WALL &&
                    scene_height_view_is_valid(heights, map->width, map->height)) {
                    int glyph_map_x = (int)floor(world_x);
                    int glyph_map_y = (int)floor(world_y);
                    const SceneAuthoredCell *cell;
                    if (!map_in_bounds(map, glyph_map_x, glyph_map_y)) continue;
                    cell = &heights->cells[
                        (size_t)glyph_map_y * (size_t)map->width +
                        (size_t)glyph_map_x];
                    if ((projected.surface == DECAL_SURFACE_FLOOR &&
                         !cell->floor_present) ||
                        (projected.surface == DECAL_SURFACE_CEILING &&
                         !cell->ceiling_present)) continue;
                    world_z = scene_height_world(
                        projected.surface == DECAL_SURFACE_FLOOR
                            ? cell->floor_height_step : cell->ceiling_height_step);
                }
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

                int cell_index = screen_y * grid->width + screen_x;
                if (bounded_occlusion) {
                    HeightfieldHitKind surface_kind = projected.surface ==
                        DECAL_SURFACE_FLOOR ? HEIGHTFIELD_HIT_FLOOR :
                        projected.surface == DECAL_SURFACE_CEILING
                            ? HEIGHTFIELD_HIT_CEILING : HEIGHTFIELD_HIT_WALL;
                    int surface_x = projected.surface == DECAL_SURFACE_WALL
                        ? projected.map_x : (int)floor(world_x);
                    int surface_y = projected.surface == DECAL_SURFACE_WALL
                        ? projected.map_y : (int)floor(world_y);
                    bool own_surface = world_hit_is_surface(
                        grid->world_hit_keys[cell_index], surface_kind,
                        surface_x, surface_y,
                        projected.surface == DECAL_SURFACE_WALL);
                    if (!own_surface &&
                        grid->world_depths[cell_index] + DECAL_OCCLUSION_EPSILON <
                            depth) continue;
                } else if (depth > z_buffer[screen_x] + 0.001) {
                    continue;
                }
                int source_order = i * 1000000 + py * d->pattern_cols + px;
                if (depth > decal_depth[cell_index] + 0.000001) continue;
                if (fabs(depth - decal_depth[cell_index]) <= 0.000001 &&
                    source_order >= decal_order[cell_index]) {
                    continue;
                }

                Cell existing;
                if (!grid_get(grid, screen_x, screen_y, &existing)) continue;

                Material *d_mat = &assets->materials[pc.material_id];
                double light_level = decal_light_level(map, &projected, world_x, world_y,
                                                       basis.normal[1]);
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

void raycast_render_world_overlays(Grid *grid, Map *map, Camera *cam,
                                   AssetRegistry *assets, WorldState *world,
                                   const SceneHeightView *heights,
                                   bool bounded_occlusion) {
    double *z_buffer;
    if (!grid || !map || !cam || !assets || !world || !grid->column_depths) return;
    z_buffer = grid->column_depths;
    render_decals(grid, map, cam, assets, world, z_buffer, grid->width,
                  heights, bounded_occlusion);
    for (int i = 0; i < world->num_lights; i++) {
        Light *light = &world->lights[i];
        double sprite_x = light->pos.x - cam->transform.pos.x;
        double sprite_y = light->pos.y - cam->transform.pos.y;
        double angle_to_light = atan2(sprite_y, sprite_x);
        double angle_diff = angle_to_light - cam->transform.angle;
        double distance;
        double camera_x;
        double perpendicular_distance;
        int screen_x;
        int draw_y;
        while (angle_diff > PI) angle_diff -= 2.0 * PI;
        while (angle_diff < -PI) angle_diff += 2.0 * PI;
        if (cos(angle_diff) < 0.1) continue;
        distance = sqrt(sprite_x * sprite_x + sprite_y * sprite_y);
        camera_x = smc_light_screen_x(angle_diff, cam->fov);
        screen_x = (int)((grid->width / 2.0) * (1.0 + camera_x));
        if (screen_x < 0 || screen_x >= grid->width) continue;
        perpendicular_distance = distance * cos(angle_diff);
        if (perpendicular_distance >= z_buffer[screen_x]) continue;
        draw_y = grid->height / 2 + (int)cam->pitch;
        if (draw_y >= 0 && draw_y < grid->height) {
            Cell existing;
            if (grid_get(grid, screen_x, draw_y, &existing)) {
                grid_set(grid, screen_x, draw_y, '*', light->color, existing.bg);
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
    int side = 0;          /* 0 = X-axis step, Y-aligned NS wall; 1 = Y-axis step, X-aligned EW wall */
    double perp_wall_dist = 0;

    while (!hit && perp_wall_dist < max_dist) {
        /* Jump to the next tile boundary — whichever axis has the smaller
         * side_dist value gets stepped first (this is the DDA core). */
        if (side_dist_x < side_dist_y) {
            side_dist_x += delta_dist_x;
            map_x += step_x;
            side = 0;  /* X-axis step: hit a Y-aligned North-South wall */
            perp_wall_dist = side_dist_x - delta_dist_x;
        } else {
            side_dist_y += delta_dist_y;
            map_y += step_y;
            side = 1;  /* Y-axis step: hit an X-aligned East-West wall */
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
 *   4. Draws ceiling and floor with distance-based shading from the light map
 *
 * After all columns are processed, a second pass renders:
 *   5. All world decals (wall, floor, and ceiling) projected onto the grid
 *   6. Light source '*' markers
 *
 * @param grid    The character grid to render into
 * @param map     The tile map (for wall data + light_map)
 * @param cam     The camera (position, angle, FOV, pitch)
 * @param assets  Asset registry (palettes, materials)
 * @param world   World state (lights, decals)
 * @param surfaces Optional borrowed authored floor/ceiling material view. NULL
 *                 preserves constant legacy backgrounds.
 */
void raycast_render_height_legacy_impl(
    Grid *grid, Map *map, Camera *cam, AssetRegistry *assets, WorldState *world,
    const SceneSurfaceView *surfaces, const SceneHeightView *heights
) {
    bool surfaces_valid;
    bool heights_valid;
    bool bounded_occlusion = false;
    if (!grid || !map || !cam || !assets || !world) return;

    surfaces_valid = surfaces && surfaces->cells &&
        surfaces->width == map->width && surfaces->height == map->height &&
        surfaces->width > 0 && surfaces->height > 0 &&
        surfaces->cell_count ==
            (size_t)surfaces->width * (size_t)surfaces->height;
    heights_valid = scene_height_view_is_valid(heights, map->width, map->height);

    /* Grid-owned workspace avoids both a hidden width cap and per-frame allocation. */
    double *z_buffer = grid->column_depths;
    int max_x_idx = grid->width;

    if (!z_buffer) return;

    if (heights_valid &&
        (!heightfield_view_is_flat_default(heights, map->width, map->height) ||
         fabs(cam->z - 0.5) > 0.000001)) {
        bounded_occlusion = true;
        raycast_render_heightfield_opaque_impl(
            grid, map, cam, assets, heights, z_buffer);
        goto render_overlays;
    }

    /* ================================================================
     *  MAIN RAYCASTING LOOP — one iteration per screen column
     * ================================================================ */

    for (int x = 0; x < max_x_idx; x++) {
        size_t ceiling_cached_index = SIZE_MAX;
        size_t floor_cached_index = SIZE_MAX;
        const Material *ceiling_cached_material = NULL;
        const Material *floor_cached_material = NULL;
        bool ceiling_cached_missing = false;
        bool floor_cached_missing = false;
        /* ---- a. Calculate ray angle for this column ---- */
        /* Map the grid column index to a normalised camera-space X:
         *   -1 = left edge of screen, 0 = centre, +1 = right edge */
        double camera_x = 2 * (x + 0.5) / (double)grid->width - 1;

        /* Compute the actual world angle for this column based on the
         * camera's forward direction and FOV.  The atan(tan(fov/2) * cx)
         * gives the angle offset from centre.
         *
         * SMC hot path: this expression is evaluated once per screen column
         * per frame.  The generated dispatch table replaces the inline
         * atan/tan call sequence. */
        double ray_angle = cam->transform.angle + smc_ray_angle_offset(camera_x, cam->fov);

        /* ---- Direction vector for this ray column ---- */
        double dir_x = cos(ray_angle);
        double dir_y = sin(ray_angle);

        /* ---- b. Fire the ray ---- */
        RayResult ray = raycast_fire(map, cam, ray_angle, config_get()->raycast_max_distance);

        int line_height = 0;
        int material_id = 0;
        double perp_dist = ray.distance;
        double wall_floor = 0.0;
        double wall_ceiling = 1.0;
        bool default_wall_span = true;

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
             * perpendicular distance so walls remain visually straight.
             *
             * SMC hot path: one call per column per frame. */
            perp_dist = smc_fisheye_correct(ray.distance, ray_angle, cam->transform.angle);
            if (perp_dist < 0.001) perp_dist = 0.001;  /* Prevent division by zero */

            /* Wall slice height in cells: taller = closer */
            line_height = (int)(grid->height / perp_dist);
            if (heights_valid) {
                size_t index = (size_t)ray.map_y * (size_t)map->width +
                               (size_t)ray.map_x;
                wall_floor = scene_height_world(heights->cells[index].floor_height_step);
                wall_ceiling = scene_height_world(
                    heights->cells[index].ceiling_height_step);
                default_wall_span = cam->z == 0.5 &&
                    heights->cells[index].floor_height_step ==
                        SCENE_DEFAULT_FLOOR_HEIGHT_STEP &&
                    heights->cells[index].ceiling_height_step ==
                        SCENE_DEFAULT_CEILING_HEIGHT_STEP;
            }
        }

        /* Store in z-buffer for decal/light occlusion checks */
        z_buffer[x] = perp_dist;

        /* ---- Vertical bounds of the wall slice ---- */
        /* Centre the wall vertically and apply camera pitch (looking up/down) */
        int draw_start = heights_valid && ray.hit && !default_wall_span
            ? (int)floor(height_project_y(cam, grid->height, wall_ceiling, perp_dist))
            : -line_height / 2 + grid->height / 2 + (int)cam->pitch;
        if (draw_start < 0) draw_start = 0;
        int draw_end = heights_valid && ray.hit && !default_wall_span
            ? (int)floor(height_project_y(cam, grid->height, wall_floor, perp_dist))
            : line_height / 2 + grid->height / 2 + (int)cam->pitch;
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
            /* SMC hot path: ceiling true-distance expression, once per
             * ceiling pixel per column. */
            double trueDist = smc_true_distance(currentDist, ray_angle, cam->transform.angle);

            /* World coordinates of this ceiling point */
            double curX = cam->transform.pos.x + trueDist * dir_x;
            double curY = cam->transform.pos.y + trueDist * dir_y;

            int map_x = surfaces_valid ? (int)floor(curX) : (int)curX;
            int map_y = surfaces_valid ? (int)floor(curY) : (int)curY;
            trueDist = horizontal_true_distance(
                heights, heights_valid, cam, grid->height, denom, trueDist,
                ray_angle, false, map_x, map_y);
            curX = cam->transform.pos.x + trueDist * dir_x;
            curY = cam->transform.pos.y + trueDist * dir_y;
            map_x = surfaces_valid ? (int)floor(curX) : (int)curX;
            map_y = surfaces_valid ? (int)floor(curY) : (int)curY;
            bool in_bounds = map_in_bounds(map, map_x, map_y);
            uint8_t glyph = ' ';
            SDL_Color fg = {255, 255, 255, 255};
            SDL_Color bg = {50, 50, 50, 255};   /* Grey ceiling */

            /* Apply light map */
            double light_level = 1.0;
            if (map->light_map) {
                if (in_bounds) {
                    light_level = map->light_map[map_y * map->width + map_x];
                    if (light_level > 1.0) light_level = 1.0;
                }
            }

            if (surfaces_valid && in_bounds) {
                size_t index = (size_t)map_y * (size_t)map->width + (size_t)map_x;
                if (index != ceiling_cached_index) {
                    int id = surfaces->cells[index].ceiling_material;
                    ceiling_cached_index = index;
                    ceiling_cached_missing = !material_id_is_loaded(assets, id);
                    ceiling_cached_material = ceiling_cached_missing
                        ? NULL : &assets->materials[id];
                }
                if (ceiling_cached_missing) {
                    glyph = '.';
                    fg = (SDL_Color){0, 0, 0, 255};
                    bg = (SDL_Color){128, 0, 255, 255};
                } else {
                    int glyph_index = trueDist > 10.0 ? 3 :
                        trueDist > 7.0 ? 2 : trueDist > 4.0 ? 1 : 0;
                    glyph = ceiling_cached_material->glyphs[glyph_index];
                    fg = palette_sample(
                        &assets->palettes[ceiling_cached_material->palette_id],
                        trueDist, light_level);
                    bg = (SDL_Color){0, 0, 0, 255};
                }
            } else if (in_bounds) {
                fg.r = (uint8_t)(fg.r * light_level);
                fg.g = (uint8_t)(fg.g * light_level);
                fg.b = (uint8_t)(fg.b * light_level);
                bg.r = (uint8_t)(bg.r * light_level);
                bg.g = (uint8_t)(bg.g * light_level);
                bg.b = (uint8_t)(bg.b * light_level);
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
            /* SMC hot path: floor true-distance expression, once per
             * floor pixel per column. */
            double trueDist = smc_true_distance(currentDist, ray_angle, cam->transform.angle);

            double curX = cam->transform.pos.x + trueDist * dir_x;
            double curY = cam->transform.pos.y + trueDist * dir_y;

            int map_x = surfaces_valid ? (int)floor(curX) : (int)curX;
            int map_y = surfaces_valid ? (int)floor(curY) : (int)curY;
            trueDist = horizontal_true_distance(
                heights, heights_valid, cam, grid->height, denom, trueDist,
                ray_angle, true, map_x, map_y);
            curX = cam->transform.pos.x + trueDist * dir_x;
            curY = cam->transform.pos.y + trueDist * dir_y;
            map_x = surfaces_valid ? (int)floor(curX) : (int)curX;
            map_y = surfaces_valid ? (int)floor(curY) : (int)curY;
            bool in_bounds = map_in_bounds(map, map_x, map_y);
            uint8_t glyph = ' ';
            SDL_Color fg = {255, 255, 255, 255};
            SDL_Color bg = {30, 30, 30, 255};   /* Darker floor */

            double light_level = 1.0;
            if (map->light_map) {
                if (in_bounds) {
                    light_level = map->light_map[map_y * map->width + map_x];
                    if (light_level > 1.0) light_level = 1.0;
                }
            }

            if (surfaces_valid && in_bounds) {
                size_t index = (size_t)map_y * (size_t)map->width + (size_t)map_x;
                if (index != floor_cached_index) {
                    int id = surfaces->cells[index].floor_material;
                    floor_cached_index = index;
                    floor_cached_missing = !material_id_is_loaded(assets, id);
                    floor_cached_material = floor_cached_missing
                        ? NULL : &assets->materials[id];
                }
                if (floor_cached_missing) {
                    glyph = '.';
                    fg = (SDL_Color){0, 0, 0, 255};
                    bg = (SDL_Color){128, 0, 255, 255};
                } else {
                    int glyph_index = trueDist > 10.0 ? 3 :
                        trueDist > 7.0 ? 2 : trueDist > 4.0 ? 1 : 0;
                    glyph = floor_cached_material->glyphs[glyph_index];
                    fg = palette_sample(
                        &assets->palettes[floor_cached_material->palette_id],
                        trueDist, light_level);
                    bg = (SDL_Color){0, 0, 0, 255};
                }
            } else if (in_bounds) {
                fg.r = (uint8_t)(fg.r * light_level);
                fg.g = (uint8_t)(fg.g * light_level);
                fg.b = (uint8_t)(fg.b * light_level);
                bg.r = (uint8_t)(bg.r * light_level);
                bg.g = (uint8_t)(bg.g * light_level);
                bg.b = (uint8_t)(bg.b * light_level);
            }

            grid_set(grid, x, y, glyph, fg, bg);
        }
    }

render_overlays:
    raycast_render_world_overlays(
        grid, map, cam, assets, world,
        heights_valid ? heights : NULL, bounded_occlusion);
}

void raycast_render_height(Grid *grid, Map *map, Camera *cam,
                           AssetRegistry *assets, WorldState *world,
                           const SceneSurfaceView *surfaces,
                           const SceneHeightView *heights) {
    raycast_render_height_legacy_impl(
        grid, map, cam, assets, world, surfaces, heights);
}

void raycast_render(Grid *grid, Map *map, Camera *cam, AssetRegistry *assets,
                    WorldState *world, const SceneSurfaceView *surfaces) {
    raycast_render_height_legacy_impl(
        grid, map, cam, assets, world, surfaces, NULL);
}