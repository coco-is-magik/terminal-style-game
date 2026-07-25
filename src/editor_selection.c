/**
 * editor_selection.c — Wall face selection from center-camera ray
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
    if (!map || selection.type != SELECTION_WALL_FACE) {
        return false;
    }

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
