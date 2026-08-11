/**
 * editor_selection.h — First-person typed selection helpers
 *
 * Preserves center-ray wall DDA selection and composes deterministic point-light
 * picking over borrowed authored values. It owns no document or runtime state.
 */

#ifndef EDITOR_SELECTION_H
#define EDITOR_SELECTION_H

#include "camera.h"
#include "editor_types.h"
#include "map.h"
#include "scene_types.h"

EditorHit editor_raycast_selection(
    const Camera *camera,
    const Map *map
);

EditorHit editor_pick_light_selection(
    const Camera *camera,
    const SceneLight *lights,
    size_t light_count,
    EditorHit wall_hit,
    double max_distance,
    double pick_radius
);

EditorHit editor_pick_horizontal_surface_selection(
    const Camera *camera,
    const Map *map,
    EditorHit existing_hit,
    int viewport_rows,
    double max_distance
);

WallFace editor_calculate_wall_face(
    int side,
    double ray_dir_x,
    double ray_dir_y
);

WallMaterialRef editor_wall_face_to_material_ref(
    WallFaceRef face
);

bool editor_selection_is_valid_for_map(
    SelectionTarget selection,
    const Map *map
);

#endif /* EDITOR_SELECTION_H */
