/**
 * editor_selection.c — Typed selection from the center-camera ray
 *
 * Coordinate convention (confirmed against camera/raycast):
 *   angle 0 = +X/east, PI/2 = +Y/south (Y increases south).
 *
 * Face from DDA side + ray direction (plan §10):
 *   side == 0 && ray_dir_x > 0 -> WEST
 *   side == 0 && ray_dir_x < 0 -> EAST
 *   side == 1 && ray_dir_y > 0 -> NORTH
 *   side == 1 && ray_dir_y < 0 -> SOUTH
 */

#include "editor_selection.h"
#include "config.h"
#include "raycast.h"

#include <math.h>
#include <stddef.h>

WallFace editor_calculate_wall_face(
    int side,
    double ray_dir_x,
    double ray_dir_y
) {
    if (side == 0) {
        return (ray_dir_x > 0.0) ? WALL_FACE_WEST : WALL_FACE_EAST;
    }
    return (ray_dir_y > 0.0) ? WALL_FACE_NORTH : WALL_FACE_SOUTH;
}

WallMaterialRef editor_wall_face_to_material_ref(WallFaceRef face) {
    WallMaterialRef ref;
    ref.map_x = face.map_x;
    ref.map_y = face.map_y;
    return ref;
}

bool editor_selection_is_valid_for_map(
    SelectionTarget selection,
    const Map *map
) {
    if (!map) {
        return false;
    }

    if (selection.type == SELECTION_FLOOR ||
        selection.type == SELECTION_CEILING) {
        return map_in_bounds((Map *)map,
            selection.value.horizontal.map_x,
            selection.value.horizontal.map_y);
    }
    if (selection.type != SELECTION_WALL_FACE) return false;

    int x = selection.value.wall_face.map_x;
    int y = selection.value.wall_face.map_y;

    /* map_get / map_in_bounds take non-const Map*; they do not mutate. */
    Map *mutable_map = (Map *)map;
    if (!map_in_bounds(mutable_map, x, y)) {
        return false;
    }

    MapCell *cell = map_get(mutable_map, x, y);
    if (!cell || cell->material_id <= 0) {
        return false;
    }

    return true;
}

EditorHit editor_raycast_selection(
    const Camera *camera,
    const Map *map
) {
    EditorHit hit;
    hit.valid = false;
    hit.distance = 0.0;
    hit.target.type = SELECTION_NONE;

    if (!camera || !map) {
        return hit;
    }

    double max_dist = config_get()->raycast_max_distance;
    if (max_dist <= 0.0) {
        max_dist = 20.0;
    }

    /* raycast_fire does not mutate map or camera; API is non-const historically. */
    RayResult res = raycast_fire((Map *)map, (Camera *)camera,
                                 camera->transform.angle, max_dist);
    if (!res.hit) {
        return hit;
    }

    double ray_dir_x = cos(camera->transform.angle);
    double ray_dir_y = sin(camera->transform.angle);
    WallFace face = editor_calculate_wall_face(res.side, ray_dir_x, ray_dir_y);

    hit.valid = true;
    hit.distance = res.distance;
    hit.target.type = SELECTION_WALL_FACE;
    hit.target.value.wall_face.map_x = res.map_x;
    hit.target.value.wall_face.map_y = res.map_y;
    hit.target.value.wall_face.face = face;
    return hit;
}


EditorHit editor_pick_light_selection(
    const Camera *camera,
    const SceneLight *lights,
    size_t light_count,
    EditorHit wall_hit,
    double max_distance,
    double pick_radius
) {
    EditorHit result = wall_hit;
    double limit;
    double radius_squared;
    double dir_x;
    double dir_y;
    double best_forward = 0.0;
    SceneInstanceId best_id = SCENE_INSTANCE_ID_INVALID;
    size_t i;

    if (!camera || (!lights && light_count > 0U) ||
        !isfinite(max_distance) || max_distance <= 0.0 ||
        !isfinite(pick_radius) || pick_radius <= 0.0) {
        return result;
    }

    limit = wall_hit.valid ? wall_hit.distance : max_distance;
    if (!isfinite(limit) || limit <= 0.0 || limit > max_distance) {
        limit = max_distance;
    }
    radius_squared = pick_radius * pick_radius;
    dir_x = cos(camera->transform.angle);
    dir_y = sin(camera->transform.angle);

    for (i = 0U; i < light_count; i++) {
        const SceneLight *light = &lights[i];
        double offset_x;
        double offset_y;
        double forward;
        double perpendicular;
        double perpendicular_squared;

        if (light->id == SCENE_INSTANCE_ID_INVALID ||
            !isfinite(light->x) || !isfinite(light->y)) {
            continue;
        }
        offset_x = light->x - camera->transform.pos.x;
        offset_y = light->y - camera->transform.pos.y;
        forward = offset_x * dir_x + offset_y * dir_y;
        if (forward <= 0.0 || forward > limit) continue;

        perpendicular = offset_x * dir_y - offset_y * dir_x;
        perpendicular_squared = perpendicular * perpendicular;
        if (perpendicular_squared > radius_squared) continue;

        if (best_id == SCENE_INSTANCE_ID_INVALID ||
            forward < best_forward ||
            (forward == best_forward && light->id < best_id)) {
            best_forward = forward;
            best_id = light->id;
        }
    }

    if (best_id != SCENE_INSTANCE_ID_INVALID) {
        result.valid = true;
        result.distance = best_forward;
        result.target.type = SELECTION_LIGHT;
        result.target.value.light.id = best_id;
    }
    return result;
}

EditorHit editor_pick_horizontal_surface_selection(
    const Camera *camera,
    const Map *map,
    EditorHit existing_hit,
    int viewport_rows,
    double max_distance
) {
    EditorHit result = existing_hit;
    double pitch;
    double denominator;
    double distance;
    double world_x;
    double world_y;
    int map_x;
    int map_y;
    SelectionType type;

    if (!camera || !map || viewport_rows <= 0 ||
        !isfinite(camera->transform.pos.x) ||
        !isfinite(camera->transform.pos.y) ||
        !isfinite(camera->transform.angle) || !isfinite(camera->pitch) ||
        !isfinite(max_distance) || max_distance <= 0.0) return result;
    pitch = camera->pitch;
    if (fabs(pitch) < 0.001) return result;
    denominator = 2.0 * fabs(pitch);
    distance = (double)viewport_rows / denominator;
    if (!isfinite(distance) || distance <= 0.0 || distance > max_distance)
        return result;
    if (existing_hit.valid && (!isfinite(existing_hit.distance) ||
        existing_hit.distance <= distance)) return result;

    world_x = camera->transform.pos.x + distance * cos(camera->transform.angle);
    world_y = camera->transform.pos.y + distance * sin(camera->transform.angle);
    if (!isfinite(world_x) || !isfinite(world_y)) return result;
    map_x = (int)floor(world_x);
    map_y = (int)floor(world_y);
    if (!map_in_bounds((Map *)map, map_x, map_y)) return result;
    type = pitch < 0.0 ? SELECTION_FLOOR : SELECTION_CEILING;
    result.valid = true;
    result.distance = distance;
    result.target.type = type;
    result.target.value.horizontal.map_x = map_x;
    result.target.value.horizontal.map_y = map_y;
    return result;
}
