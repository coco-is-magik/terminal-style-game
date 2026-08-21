#include "heightfield_trace.h"

#include "height_projection.h"

#include <float.h>
#include <math.h>
#include <stddef.h>

#define TRACE_EPSILON 0.000001

static const SceneAuthoredCell *cell_at(const SceneHeightView *view, int x, int y) {
    if (!view || x < 0 || y < 0 || x >= view->width || y >= view->height)
        return NULL;
    return &view->cells[(size_t)y * (size_t)view->width + (size_t)x];
}

static double lower_bound(const SceneAuthoredCell *cell) {
    return cell && cell->floor_present
        ? scene_height_world(cell->floor_height_step) : -DBL_MAX;
}

static double upper_bound(const SceneAuthoredCell *cell) {
    return cell && cell->ceiling_present
        ? scene_height_world(cell->ceiling_height_step) : DBL_MAX;
}

bool heightfield_view_is_flat_default(const SceneHeightView *view,
                                      int width, int height) {
    size_t i;
    if (!scene_height_view_is_valid(view, width, height)) return false;
    for (i = 0U; i < view->cell_count; i++) {
        const SceneAuthoredCell *cell = &view->cells[i];
        if (!cell->floor_present || !cell->ceiling_present ||
            cell->floor_height_step != SCENE_DEFAULT_FLOOR_HEIGHT_STEP ||
            cell->ceiling_height_step != SCENE_DEFAULT_CEILING_HEIGHT_STEP)
            return false;
    }
    return true;
}

static HeightfieldHit horizontal_hit(
    const Camera *camera, const SceneAuthoredCell *cell,
    double floor_z, double ceiling_z, int map_x, int map_y,
    int viewport_height, double row_delta, double correction,
    double dir_x, double dir_y, double enter, double exit
) {
    HeightfieldHit best = {0};
    bool floor_surface = row_delta > 0.0;
    bool present = floor_surface ? cell->floor_present : cell->ceiling_present;
    double perpendicular;
    double distance;
    double z;
    if (!present || fabs(row_delta) <= TRACE_EPSILON) return best;
    z = floor_surface ? floor_z : ceiling_z;
    perpendicular = (camera->z - z) * viewport_height / row_delta;
    if (perpendicular <= TRACE_EPSILON) return best;
    distance = perpendicular / correction;
    if (distance + TRACE_EPSILON < enter || distance > exit + TRACE_EPSILON)
        return best;
    best.hit = true;
    best.kind = floor_surface ? HEIGHTFIELD_HIT_FLOOR : HEIGHTFIELD_HIT_CEILING;
    best.distance = distance;
    best.perpendicular_distance = perpendicular;
    best.world_x = camera->transform.pos.x + distance * dir_x;
    best.world_y = camera->transform.pos.y + distance * dir_y;
    best.world_z = z;
    best.map_x = map_x;
    best.map_y = map_y;
    best.side = -1;
    best.material = floor_surface ? cell->floor_material : cell->ceiling_material;
    return best;
}

static bool boundary_span(
    const SceneAuthoredCell *from, const SceneAuthoredCell *to, double z,
    HeightfieldHitKind *kind, uint16_t *material, bool *owner_is_to
) {
    double from_floor = lower_bound(from);
    double to_floor = lower_bound(to);
    double from_ceiling = upper_bound(from);
    double to_ceiling = upper_bound(to);
    if (!to || to->occupancy == SCENE_CELL_OCCUPANCY_WALL) {
        double wall_bottom = to ? fmin(from_floor, to_floor) : from_floor;
        double wall_top = to ? fmax(from_ceiling, to_ceiling) : from_ceiling;
        if (z >= wall_bottom - TRACE_EPSILON &&
            z <= wall_top + TRACE_EPSILON) {
            *kind = HEIGHTFIELD_HIT_WALL;
            *material = to ? to->wall_material : from->wall_material;
            *owner_is_to = to != NULL;
            return true;
        }
        return false;
    }
    if (z >= fmin(from_floor, to_floor) - TRACE_EPSILON &&
        z <= fmax(from_floor, to_floor) + TRACE_EPSILON &&
        fabs(from_floor - to_floor) > TRACE_EPSILON) {
        const SceneAuthoredCell *owner = from_floor > to_floor ? from : to;
        *kind = HEIGHTFIELD_HIT_FLOOR;
        *material = owner->floor_material;
        *owner_is_to = owner == to;
        return true;
    }
    if (z >= fmin(from_ceiling, to_ceiling) - TRACE_EPSILON &&
        z <= fmax(from_ceiling, to_ceiling) + TRACE_EPSILON &&
        fabs(from_ceiling - to_ceiling) > TRACE_EPSILON) {
        const SceneAuthoredCell *owner = from_ceiling < to_ceiling ? from : to;
        *kind = HEIGHTFIELD_HIT_CEILING;
        *material = owner->ceiling_material;
        *owner_is_to = owner == to;
        return true;
    }
    return false;
}

bool heightfield_trace_prepare_column(
    HeightfieldTraceColumn *column, const Camera *camera, const Map *map,
    const SceneHeightView *heights, int viewport_width, int viewport_height,
    int screen_x, double max_distance
) {
    double camera_x;
    double ray_angle;
    double dir_x;
    double dir_y;
    int map_x;
    int map_y;
    double side_x;
    double side_y;
    double enter = 0.0;
    if (!column || !camera || !map || viewport_width <= 0 || viewport_height <= 0 ||
        screen_x < 0 || screen_x >= viewport_width || !isfinite(max_distance) ||
        max_distance <= 0.0 ||
        !scene_height_view_is_valid(heights, map->width, map->height)) return false;
    column->valid = false;
    column->interval_count = 0U;
    camera_x = 2.0 * (screen_x + 0.5) / viewport_width - 1.0;
    ray_angle = camera->transform.angle + atan(camera_x * tan(camera->fov / 2.0));
    column->correction = cos(ray_angle - camera->transform.angle);
    if (!isfinite(column->correction) || column->correction <= TRACE_EPSILON)
        return false;
    dir_x = cos(ray_angle);
    dir_y = sin(ray_angle);
    map_x = (int)floor(camera->transform.pos.x);
    map_y = (int)floor(camera->transform.pos.y);
    if (!map_in_bounds(map, map_x, map_y)) return false;
    column->camera = camera;
    column->map = map;
    column->heights = heights;
    column->viewport_height = viewport_height;
    column->start_map_x = map_x;
    column->start_map_y = map_y;
    column->step_x = dir_x < 0.0 ? -1 : 1;
    column->step_y = dir_y < 0.0 ? -1 : 1;
    column->direction_x = dir_x;
    column->direction_y = dir_y;
    column->delta_x = fabs(1.0 / (fabs(dir_x) <= TRACE_EPSILON ? TRACE_EPSILON : dir_x));
    column->delta_y = fabs(1.0 / (fabs(dir_y) <= TRACE_EPSILON ? TRACE_EPSILON : dir_y));
    column->initial_side_x = (dir_x < 0.0 ? camera->transform.pos.x - map_x
                                         : map_x + 1.0 - camera->transform.pos.x) *
        column->delta_x;
    column->initial_side_y = (dir_y < 0.0 ? camera->transform.pos.y - map_y
                                         : map_y + 1.0 - camera->transform.pos.y) *
        column->delta_y;
    column->max_distance = max_distance;
    side_x = column->initial_side_x;
    side_y = column->initial_side_y;
    while (enter <= max_distance && map_in_bounds(map, map_x, map_y) &&
           column->interval_count < HEIGHTFIELD_TRACE_MAX_INTERVALS) {
        size_t i = column->interval_count++;
        int next_x = map_x;
        int next_y = map_y;
        double exit = side_x < side_y ? side_x : side_y;
        int side;
        if (side_x < side_y) {
            next_x += column->step_x;
            side_x += column->delta_x;
            side = 0;
        } else {
            next_y += column->step_y;
            side_y += column->delta_y;
            side = 1;
        }
        column->interval_map_x[i] = map_x;
        column->interval_map_y[i] = map_y;
        column->interval_next_x[i] = next_x;
        column->interval_next_y[i] = next_y;
        column->interval_side[i] = side;
        column->interval_enter[i] = enter;
        column->interval_exit[i] = exit;
        column->interval_cell[i] = cell_at(heights, map_x, map_y);
        column->interval_next_cell[i] = cell_at(heights, next_x, next_y);
        column->interval_floor_z[i] = scene_height_world(
            column->interval_cell[i]->floor_height_step);
        column->interval_ceiling_z[i] = scene_height_world(
            column->interval_cell[i]->ceiling_height_step);
        map_x = next_x;
        map_y = next_y;
        enter = exit;
    }
    column->valid = true;
    return true;
}

HeightfieldHit heightfield_trace_prepared_sample(
    const HeightfieldTraceColumn *column, int screen_y
) {
    HeightfieldHit none = {0};
    const Camera *camera;
    const Map *map;
    int viewport_height;
    double dir_x;
    double dir_y;
    double correction;
    double row_delta;
    size_t interval_index;
    if (!column || !column->valid || screen_y < 0 ||
        screen_y >= column->viewport_height) return none;
    camera = column->camera;
    map = column->map;
    viewport_height = column->viewport_height;
    dir_x = column->direction_x;
    dir_y = column->direction_y;
    correction = column->correction;
    row_delta = screen_y + 0.5 - camera_horizon_row(camera, viewport_height);
    for (interval_index = 0U; interval_index < column->interval_count;
         interval_index++) {
        int map_x = column->interval_map_x[interval_index];
        int map_y = column->interval_map_y[interval_index];
        int next_x = column->interval_next_x[interval_index];
        int next_y = column->interval_next_y[interval_index];
        int side = column->interval_side[interval_index];
        double enter = column->interval_enter[interval_index];
        double exit = column->interval_exit[interval_index];
        const SceneAuthoredCell *cell = column->interval_cell[interval_index];
        HeightfieldHit horizontal;
        double perpendicular;
        double z;
        HeightfieldHitKind kind;
        uint16_t material;
        bool owner_is_to;
        if (!cell) return none;
        horizontal = horizontal_hit(
            camera, cell, column->interval_floor_z[interval_index],
            column->interval_ceiling_z[interval_index], map_x, map_y,
            viewport_height, row_delta, correction, dir_x, dir_y, enter, exit);
        if (horizontal.hit) return horizontal;
        if ((row_delta > 0.0 && !cell->floor_present) ||
            (row_delta < 0.0 && !cell->ceiling_present)) return none;
        perpendicular = exit * correction;
        z = camera->z - row_delta * perpendicular / viewport_height;
        if (boundary_span(cell, column->interval_next_cell[interval_index], z,
                          &kind, &material, &owner_is_to)) {
            HeightfieldHit boundary = {
                kind, exit, perpendicular,
                camera->transform.pos.x + exit * dir_x,
                camera->transform.pos.y + exit * dir_y, z,
                next_x, next_y, side, material, true, true
            };
            if (!owner_is_to) {
                boundary.map_x = map_x;
                boundary.map_y = map_y;
            }
            if (!map_in_bounds(map, next_x, next_y)) {
                boundary.map_x = map_x;
                boundary.map_y = map_y;
            }
            return boundary;
        }
    }
    return none;
}

HeightfieldHit heightfield_trace_screen_sample(
    const Camera *camera, const Map *map, const SceneHeightView *heights,
    int viewport_width, int viewport_height, int screen_x, int screen_y,
    double max_distance
) {
    HeightfieldTraceColumn column;
    HeightfieldHit none = {0};
    if (!heightfield_trace_prepare_column(&column, camera, map, heights,
            viewport_width, viewport_height, screen_x, max_distance)) return none;
    return heightfield_trace_prepared_sample(&column, screen_y);
}