#include "heightfield_trace.h"

#include "height_projection.h"
#include "heightfield_trace_internal.h"

#include <stddef.h>

typedef struct {
    size_t interval_index;
    unsigned int phase;
    double row_delta;
    bool ended;
    bool opening;
} HeightfieldOpticalCursor;

static bool optical_cursor_next(const HeightfieldTraceColumn *column,
                                HeightfieldOpticalCursor *cursor,
                                HeightfieldHit *out_hit) {
    const Camera *camera = column->camera;
    int viewport_height = column->viewport_height;
    while (!cursor->ended && cursor->interval_index < column->interval_count) {
        size_t i = cursor->interval_index;
        int map_x = column->interval_map_x[i];
        int map_y = column->interval_map_y[i];
        int next_x = column->interval_next_x[i];
        int next_y = column->interval_next_y[i];
        int side = column->interval_side[i];
        double enter = column->interval_enter[i];
        double exit = column->interval_exit[i];
        const SceneAuthoredCell *cell = column->interval_cell[i];
        if (!cell) {
            cursor->ended = true;
            break;
        }
        if (cursor->phase == 0U) {
            HeightfieldHit horizontal = heightfield_horizontal_hit(
                camera, cell, column->interval_floor_z[i],
                column->interval_ceiling_z[i], map_x, map_y,
                viewport_height, cursor->row_delta, column->correction,
                column->direction_x, column->direction_y, enter, exit);
            cursor->phase = 1U;
            if (horizontal.hit) {
                *out_hit = horizontal;
                return true;
            }
        }
        if ((cursor->row_delta > 0.0 && !cell->floor_present) ||
            (cursor->row_delta < 0.0 && !cell->ceiling_present)) {
            cursor->ended = true;
            cursor->opening = true;
            break;
        }
        {
            double perpendicular = exit * column->correction;
            double z = camera->z -
                cursor->row_delta * perpendicular / viewport_height;
            HeightfieldHitKind kind;
            uint16_t material;
            bool owner_is_to;
            cursor->phase = 0U;
            cursor->interval_index++;
            if (heightfield_boundary_span(cell, column->interval_next_cell[i], z,
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
                *out_hit = boundary;
                return true;
            }
        }
    }
    cursor->ended = true;
    return false;
}

bool heightfield_trace_selective(
    const HeightfieldTraceColumn *column,
    const OpticalRuntimeView *optical_view,
    uint32_t source_generation,
    int screen_y,
    HeightfieldOpticalResult *out_result
) {
    HeightfieldOpticalResult result = {0};
    HeightfieldOpticalCursor cursor = {0};
    if (!column || !column->valid || !optical_view || !out_result ||
        !optical_runtime_view_is_current(optical_view, source_generation) ||
        optical_view->cell_count != column->heights->cell_count ||
        screen_y < 0 || screen_y >= column->viewport_height) return false;
    cursor.row_delta = screen_y + 0.5 -
        camera_horizon_row(column->camera, column->viewport_height);
    while (result.count < HEIGHTFIELD_OPTICAL_MAX_LAYERS) {
        HeightfieldHit hit;
        OpticalResolved resolved;
        size_t cell_index;
        if (!optical_cursor_next(column, &cursor, &hit)) {
            result.reached_opening = cursor.opening;
            break;
        }
        if (hit.map_x < 0 || hit.map_y < 0 ||
            hit.map_x >= column->heights->width ||
            hit.map_y >= column->heights->height) return false;
        cell_index = (size_t)hit.map_y * (size_t)column->heights->width +
                     (size_t)hit.map_x;
        if (!optical_runtime_view_resolve(
                optical_view, cell_index, hit.material, true, &resolved))
            return false;
        result.layers[result.count].hit = hit;
        result.layers[result.count].optical = resolved;
        result.count++;
        if (resolved.ray_blocks || resolved.transmission == 0U) {
            result.terminated_by_surface = true;
            break;
        }
        if (result.count == HEIGHTFIELD_OPTICAL_MAX_LAYERS) {
            result.layer_cap_exhausted = true;
            break;
        }
    }
    *out_result = result;
    return true;
}

bool heightfield_trace_continue_after_nearest(
    const HeightfieldTraceColumn *column,
    const OpticalRuntimeView *optical_view,
    uint32_t source_generation,
    int screen_y,
    const HeightfieldHit *nearest_hit,
    const OpticalResolved *nearest_optical,
    HeightfieldOpticalResult *out_result
) {
    HeightfieldOpticalResult result = {0};
    HeightfieldOpticalCursor cursor = {0};
    HeightfieldHit yielded;
    if (!column || !column->valid || !optical_view || !nearest_hit ||
        !nearest_hit->hit || !nearest_optical || !out_result ||
        nearest_optical->ray_blocks || nearest_optical->transmission == 0U ||
        !optical_runtime_view_is_current(optical_view, source_generation) ||
        optical_view->cell_count != column->heights->cell_count ||
        screen_y < 0 || screen_y >= column->viewport_height) return false;
    cursor.row_delta = screen_y + 0.5 -
        camera_horizon_row(column->camera, column->viewport_height);
    if (!optical_cursor_next(column, &cursor, &yielded) ||
        yielded.kind != nearest_hit->kind ||
        yielded.map_x != nearest_hit->map_x ||
        yielded.map_y != nearest_hit->map_y ||
        yielded.material != nearest_hit->material ||
        yielded.generated_boundary != nearest_hit->generated_boundary ||
        yielded.distance != nearest_hit->distance) return false;
    result.layers[0].hit = *nearest_hit;
    result.layers[0].optical = *nearest_optical;
    result.count = 1U;
    while (result.count < HEIGHTFIELD_OPTICAL_MAX_LAYERS) {
        OpticalResolved resolved;
        size_t cell_index;
        if (!optical_cursor_next(column, &cursor, &yielded)) {
            result.reached_opening = cursor.opening;
            break;
        }
        if (yielded.map_x < 0 || yielded.map_y < 0 ||
            yielded.map_x >= column->heights->width ||
            yielded.map_y >= column->heights->height) return false;
        cell_index = (size_t)yielded.map_y * (size_t)column->heights->width +
                     (size_t)yielded.map_x;
        if (!optical_runtime_view_resolve(
                optical_view, cell_index, yielded.material, true, &resolved))
            return false;
        result.layers[result.count].hit = yielded;
        result.layers[result.count].optical = resolved;
        result.count++;
        if (resolved.ray_blocks || resolved.transmission == 0U) {
            result.terminated_by_surface = true;
            break;
        }
        if (result.count == HEIGHTFIELD_OPTICAL_MAX_LAYERS) {
            result.layer_cap_exhausted = true;
            break;
        }
    }
    *out_result = result;
    return true;
}
