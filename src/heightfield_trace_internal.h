#ifndef HEIGHTFIELD_TRACE_INTERNAL_H
#define HEIGHTFIELD_TRACE_INTERNAL_H

#include "heightfield_trace.h"

#include <float.h>
#include <math.h>

#define HEIGHTFIELD_TRACE_EPSILON 0.000001

static double heightfield_lower_bound(const SceneAuthoredCell *cell) {
    return cell && cell->floor_present
        ? scene_height_world(cell->floor_height_step) : -DBL_MAX;
}

static double heightfield_upper_bound(const SceneAuthoredCell *cell) {
    return cell && cell->ceiling_present
        ? scene_height_world(cell->ceiling_height_step) : DBL_MAX;
}

static HeightfieldHit heightfield_horizontal_hit(
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
    if (!present || fabs(row_delta) <= HEIGHTFIELD_TRACE_EPSILON) return best;
    z = floor_surface ? floor_z : ceiling_z;
    perpendicular = (camera->z - z) * viewport_height / row_delta;
    if (perpendicular <= HEIGHTFIELD_TRACE_EPSILON) return best;
    distance = perpendicular / correction;
    if (distance + HEIGHTFIELD_TRACE_EPSILON < enter ||
        distance > exit + HEIGHTFIELD_TRACE_EPSILON) return best;
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

static bool heightfield_boundary_span(
    const SceneAuthoredCell *from, const SceneAuthoredCell *to, double z,
    HeightfieldHitKind *kind, uint16_t *material, bool *owner_is_to
) {
    double from_floor = heightfield_lower_bound(from);
    double to_floor = heightfield_lower_bound(to);
    double from_ceiling = heightfield_upper_bound(from);
    double to_ceiling = heightfield_upper_bound(to);
    if (!to || to->occupancy == SCENE_CELL_OCCUPANCY_WALL) {
        double wall_bottom = to ? fmin(from_floor, to_floor) : from_floor;
        double wall_top = to ? fmax(from_ceiling, to_ceiling) : from_ceiling;
        if (z >= wall_bottom - HEIGHTFIELD_TRACE_EPSILON &&
            z <= wall_top + HEIGHTFIELD_TRACE_EPSILON) {
            *kind = HEIGHTFIELD_HIT_WALL;
            *material = to ? to->wall_material : from->wall_material;
            *owner_is_to = to != NULL;
            return true;
        }
        return false;
    }
    if (z >= fmin(from_floor, to_floor) - HEIGHTFIELD_TRACE_EPSILON &&
        z <= fmax(from_floor, to_floor) + HEIGHTFIELD_TRACE_EPSILON &&
        fabs(from_floor - to_floor) > HEIGHTFIELD_TRACE_EPSILON) {
        const SceneAuthoredCell *owner = from_floor > to_floor ? from : to;
        *kind = HEIGHTFIELD_HIT_FLOOR;
        *material = owner->floor_material;
        *owner_is_to = owner == to;
        return true;
    }
    if (z >= fmin(from_ceiling, to_ceiling) - HEIGHTFIELD_TRACE_EPSILON &&
        z <= fmax(from_ceiling, to_ceiling) + HEIGHTFIELD_TRACE_EPSILON &&
        fabs(from_ceiling - to_ceiling) > HEIGHTFIELD_TRACE_EPSILON) {
        const SceneAuthoredCell *owner = from_ceiling < to_ceiling ? from : to;
        *kind = HEIGHTFIELD_HIT_CEILING;
        *material = owner->ceiling_material;
        *owner_is_to = owner == to;
        return true;
    }
    return false;
}

#endif /* HEIGHTFIELD_TRACE_INTERNAL_H */