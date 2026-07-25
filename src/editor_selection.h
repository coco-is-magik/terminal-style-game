/**
 * editor_selection.h — First-person wall selection helpers
 *
 * Converts center-camera ray hits into EditorHit / WallFaceRef using the
 * existing raycast DDA path. No floor/ceiling/entity selection in MVP.
 */

#ifndef EDITOR_SELECTION_H
#define EDITOR_SELECTION_H

#include "camera.h"
#include "editor_types.h"
#include "map.h"

EditorHit editor_raycast_selection(
    const Camera *camera,
    const Map *map
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
