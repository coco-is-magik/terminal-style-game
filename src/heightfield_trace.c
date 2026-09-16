#include "heightfield_trace.h"

#include "height_projection.h"

#include "heightfield_trace_internal.h"

#include <math.h>
#include <stddef.h>

static const SceneAuthoredCell *cell_at(const SceneHeightView *view, int x, int y) {
    if (!view || x < 0 || y < 0 || x >= view->width || y >= view->height)
        return NULL;
    return &view->cells[(size_t)y * (size_t)view->width + (size_t)x];
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
    column->run_count = 0U;
    camera_x = 2.0 * (screen_x + 0.5) / viewport_width - 1.0;
    ray_angle = camera->transform.angle + atan(camera_x * tan(camera->fov / 2.0));
    column->correction = cos(ray_angle - camera->transform.angle);
    if (!isfinite(column->correction) || column->correction <= HEIGHTFIELD_TRACE_EPSILON)
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
    column->projection_distance_offset = 0.0;
    column->projection_correction = column->correction;
    column->delta_x = fabs(1.0 / (fabs(dir_x) <= HEIGHTFIELD_TRACE_EPSILON ? HEIGHTFIELD_TRACE_EPSILON : dir_x));
    column->delta_y = fabs(1.0 / (fabs(dir_y) <= HEIGHTFIELD_TRACE_EPSILON ? HEIGHTFIELD_TRACE_EPSILON : dir_y));
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
    for (size_t i = 0U; i < column->interval_count; i++) {
        const SceneAuthoredCell *cell = column->interval_cell[i];
        bool begins_run = i == 0U;
        if (!begins_run) {
            const SceneAuthoredCell *previous = column->interval_cell[i - 1U];
            begins_run = !cell || !previous ||
                cell->occupancy != previous->occupancy ||
                cell->floor_present != previous->floor_present ||
                cell->ceiling_present != previous->ceiling_present ||
                cell->floor_height_step != previous->floor_height_step ||
                cell->ceiling_height_step != previous->ceiling_height_step;
        }
        if (begins_run) {
            column->run_start[column->run_count] = i;
            column->run_count++;
        }
    }
    column->valid = true;
    return true;
}

bool heightfield_trace_set_projection_path(
    HeightfieldTraceColumn *column, double distance_offset, double correction
) {
    if (!column || !column->valid || !isfinite(distance_offset) ||
        distance_offset < 0.0 || !isfinite(correction) ||
        correction <= HEIGHTFIELD_TRACE_EPSILON) return false;
    column->projection_distance_offset = distance_offset;
    column->projection_correction = correction;
    return true;
}

static bool heightfield_trace_sample_interval(
    const HeightfieldTraceColumn *column, double row_delta,
    size_t interval_index, HeightfieldHit *out_hit
) {
    HeightfieldHit none = {0};
    const Camera *camera;
    const Map *map;
    int viewport_height;
    double dir_x;
    double dir_y;
    double correction;
    double distance_offset;
    camera = column->camera;
    map = column->map;
    viewport_height = column->viewport_height;
    dir_x = column->direction_x;
    dir_y = column->direction_y;
    correction = column->projection_correction;
    distance_offset = column->projection_distance_offset;
    {
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
        if (!cell) {
            *out_hit = none;
            return true;
        }
        horizontal = heightfield_horizontal_hit(
            camera, cell, column->interval_floor_z[interval_index],
            column->interval_ceiling_z[interval_index], map_x, map_y,
            viewport_height, row_delta, distance_offset, correction,
            dir_x, dir_y, enter, exit);
        if (horizontal.hit) {
            *out_hit = horizontal;
            return true;
        }
        if ((row_delta > 0.0 && !cell->floor_present) ||
            (row_delta < 0.0 && !cell->ceiling_present)) {
            *out_hit = none;
            return true;
        }
        perpendicular = (distance_offset + exit) * correction;
        z = camera->z - row_delta * perpendicular / viewport_height;
        if (heightfield_boundary_span(cell, column->interval_next_cell[interval_index], z,
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
            *out_hit = boundary;
            return true;
        }
    }
    return false;
}

HeightfieldHit heightfield_trace_prepared_sample(
    const HeightfieldTraceColumn *column, int screen_y
) {
    HeightfieldHit none = {0};
    double row_delta;
    size_t interval_index;
    if (!column || !column->valid || screen_y < 0 ||
        screen_y >= column->viewport_height) return none;
    row_delta = screen_y + 0.5 -
        camera_horizon_row(column->camera, column->viewport_height);
    for (interval_index = 0U; interval_index < column->interval_count;
         interval_index++) {
        HeightfieldHit hit;
        if (heightfield_trace_sample_interval(
                column, row_delta, interval_index, &hit)) return hit;
    }
    return none;
}

HeightfieldHit heightfield_trace_prepared_opaque_sample(
    const HeightfieldTraceColumn *column, int screen_y
) {
    HeightfieldHit none = {0};
    const Camera *camera;
    double row_delta;
    size_t run_index;
    if (!column || !column->valid || screen_y < 0 ||
        screen_y >= column->viewport_height || column->run_count == 0U)
        return none;
    camera = column->camera;
    row_delta = screen_y + 0.5 -
        camera_horizon_row(camera, column->viewport_height);
    for (run_index = 0U; run_index < column->run_count; run_index++) {
        size_t start = column->run_start[run_index];
        size_t end = run_index + 1U < column->run_count
            ? column->run_start[run_index + 1U] - 1U
            : column->interval_count - 1U;
        const SceneAuthoredCell *cell = column->interval_cell[start];
        if (fabs(row_delta) > HEIGHTFIELD_TRACE_EPSILON &&
            (row_delta > 0.0 ? cell->floor_present : cell->ceiling_present)) {
            double z = scene_height_world(row_delta > 0.0
                ? cell->floor_height_step : cell->ceiling_height_step);
            double perpendicular =
                (camera->z - z) * column->viewport_height / row_delta;
            if (perpendicular > HEIGHTFIELD_TRACE_EPSILON) {
                double distance = perpendicular / column->projection_correction -
                    column->projection_distance_offset;
                size_t low = start;
                size_t high = end + 1U;
                while (low < high) {
                    size_t middle = low + (high - low) / 2U;
                    if (column->interval_exit[middle] +
                            HEIGHTFIELD_TRACE_EPSILON < distance)
                        low = middle + 1U;
                    else high = middle;
                }
                if (low <= end && distance + HEIGHTFIELD_TRACE_EPSILON >=
                        column->interval_enter[low]) {
                    const SceneAuthoredCell *owner = column->interval_cell[low];
                    HeightfieldHit hit = {
                        row_delta > 0.0 ? HEIGHTFIELD_HIT_FLOOR
                                        : HEIGHTFIELD_HIT_CEILING,
                        distance, perpendicular,
                        camera->transform.pos.x + distance * column->direction_x,
                        camera->transform.pos.y + distance * column->direction_y, z,
                        column->interval_map_x[low], column->interval_map_y[low], -1,
                        row_delta > 0.0 ? owner->floor_material
                                        : owner->ceiling_material,
                        false, true
                    };
                    return hit;
                }
            }
        }
        {
            HeightfieldHit boundary;
            if (heightfield_trace_sample_interval(
                    column, row_delta, end, &boundary)) return boundary;
        }
    }
    return none;
}

#ifdef R9_OPTICAL_RESEARCH

static void collect_hit(R9HeightfieldHitList *list, size_t capacity,
                        const HeightfieldHit *hit) {
    if (list->count < capacity) {
        list->hits[list->count++] = *hit;
    } else {
        list->truncated = true;
    }
}

bool heightfield_trace_collect(const HeightfieldTraceColumn *column, int screen_y,
                               size_t capacity, R9HeightfieldHitList *out_hits) {
    R9HeightfieldHitList result = {0};
    const Camera *camera;
    int viewport_height;
    double row_delta;
    size_t interval_index;
    if (!column || !column->valid || !out_hits || screen_y < 0 ||
        screen_y >= column->viewport_height || capacity == 0U ||
        capacity > R9_HEIGHTFIELD_MAX_HITS) return false;
    camera = column->camera;
    viewport_height = column->viewport_height;
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
        if (!cell) break;
        horizontal = heightfield_horizontal_hit(
            camera, cell, column->interval_floor_z[interval_index],
            column->interval_ceiling_z[interval_index], map_x, map_y,
            viewport_height, row_delta, column->projection_distance_offset,
            column->projection_correction,
            column->direction_x, column->direction_y, enter, exit);
        if (horizontal.hit) {
            collect_hit(&result, capacity, &horizontal);
            if (result.truncated) break;
        }
        if ((row_delta > 0.0 && !cell->floor_present) ||
            (row_delta < 0.0 && !cell->ceiling_present)) {
            result.terminal_opening = true;
            break;
        }
        perpendicular = (column->projection_distance_offset + exit) *
            column->projection_correction;
        z = camera->z - row_delta * perpendicular / viewport_height;
        if (heightfield_boundary_span(cell, column->interval_next_cell[interval_index], z,
                          &kind, &material, &owner_is_to)) {
            HeightfieldHit boundary = {
                kind, exit, perpendicular,
                camera->transform.pos.x + exit * column->direction_x,
                camera->transform.pos.y + exit * column->direction_y, z,
                next_x, next_y, side, material, true, true
            };
            if (!owner_is_to || !map_in_bounds(column->map, next_x, next_y)) {
                boundary.map_x = map_x;
                boundary.map_y = map_y;
            }
            collect_hit(&result, capacity, &boundary);
            if (result.truncated) break;
        }
    }
    *out_hits = result;
    return true;
}

#endif /* R9_OPTICAL_RESEARCH */

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
