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
#include "height_view.h"

#define EDITOR_SELECTION_SET_CAPACITY 8U

typedef struct {
    SelectionTarget members[EDITOR_SELECTION_SET_CAPACITY];
    size_t count;
    size_t primary_index;
} EditorSelectionSet;

void editor_selection_set_clear(EditorSelectionSet *set);
bool editor_selection_set_reset(EditorSelectionSet *set, SelectionTarget primary);
bool editor_selection_set_contains(const EditorSelectionSet *set,
                                   SelectionTarget target);
bool editor_selection_set_add(EditorSelectionSet *set, SelectionTarget target);
void editor_selection_set_revalidate(EditorSelectionSet *set, const Map *map);
const SelectionTarget *editor_selection_set_primary(const EditorSelectionSet *set);

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
EditorHit editor_pick_sprite_selection(
    const Camera *camera, const SceneSpriteInstance *sprites,
    size_t sprite_count, EditorHit existing_hit, double max_distance,
    double pick_radius
);
EditorHit editor_pick_trigger_selection(
    const Camera *camera, const SceneTrigger *triggers, size_t trigger_count,
    EditorHit existing_hit, double max_distance
);
EditorHit editor_pick_object_selection(
    const Camera *camera, const SceneObjectInstance *objects,
    size_t object_count, EditorHit existing_hit, double max_distance,
    double pick_radius
);

EditorHit editor_pick_horizontal_surface_selection(
    const Camera *camera,
    const Map *map,
    EditorHit existing_hit,
    int viewport_rows,
    double max_distance
);
EditorHit editor_pick_horizontal_surface_selection_height(
    const Camera *camera,
    const Map *map,
    const SceneHeightView *heights,
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

bool editor_project_horizontal_cell(
    const Camera *camera,
    int viewport_width,
    int viewport_height,
    int screen_x,
    int screen_y,
    SelectionType type,
    double *out_distance,
    int *out_map_x,
    int *out_map_y
);
bool editor_project_horizontal_cell_height(
    const Camera *camera,
    const SceneHeightView *heights,
    int viewport_width,
    int viewport_height,
    int screen_x,
    int screen_y,
    SelectionType type,
    double *out_distance,
    int *out_map_x,
    int *out_map_y
);

#endif /* EDITOR_SELECTION_H */
