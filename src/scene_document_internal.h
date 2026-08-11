/**
 * scene_document_internal.h — Command-system-only SceneDocument mutations
 *
 * Not for UI or application code. Command history is the sole caller of
 * these helpers so authored map changes stay behind the undo boundary.
 */

#ifndef SCENE_DOCUMENT_INTERNAL_H
#define SCENE_DOCUMENT_INTERNAL_H

#include "scene_document.h"

bool scene_document_internal_set_wall_material(
    SceneDocument *document,
    WallMaterialRef ref,
    MaterialId material
);

bool scene_document_internal_set_surface_material(
    SceneDocument *document,
    int map_x,
    int map_y,
    SceneSurfaceKind surface,
    MaterialId material,
    const AssetRegistry *assets
);

bool scene_document_internal_set_cell_occupancy(
    SceneDocument *document,
    int map_x,
    int map_y,
    SceneCellOccupancy occupancy
);

bool scene_document_internal_set_ambient_intensity(
    SceneDocument *document,
    double intensity
);

bool scene_document_internal_set_light(
    SceneDocument *document,
    SceneInstanceId instance_id,
    const SceneLight *value
);

bool scene_document_internal_light_value_is_valid(
    const SceneDocument *document,
    SceneInstanceId instance_id,
    const SceneLight *value
);

void scene_document_internal_set_current_state(
    SceneDocument *document,
    DocumentStateId state
);

typedef enum {
    SCENE_ID_ALLOCATE_OK = 0,
    SCENE_ID_ALLOCATE_INVALID_ARGUMENT,
    SCENE_ID_ALLOCATE_EXHAUSTED
} SceneIdAllocateResult;

SceneIdAllocateResult scene_document_internal_allocate_instance_id(
    SceneDocument *document,
    SceneInstanceId *out_id
);

typedef enum {
    SCENE_REPAIR_REPLACE_OK = 0,
    SCENE_REPAIR_REPLACE_NO_CHANGE,
    SCENE_REPAIR_REPLACE_INVALID_ARGUMENT,
    SCENE_REPAIR_REPLACE_INSTANCE_NOT_FOUND,
    SCENE_REPAIR_REPLACE_ASSET_NOT_LOADED
} SceneRepairReplaceResult;

typedef enum {
    SCENE_SAVE_FAULT_NONE = 0,
    SCENE_SAVE_FAULT_TEMP_CREATE,
    SCENE_SAVE_FAULT_WRITE,
    SCENE_SAVE_FAULT_FLUSH,
    SCENE_SAVE_FAULT_FILE_SYNC,
    SCENE_SAVE_FAULT_CLOSE,
    SCENE_SAVE_FAULT_MODE,
    SCENE_SAVE_FAULT_RENAME,
    SCENE_SAVE_FAULT_DIRECTORY_SYNC
} SceneSaveFault;

SceneRepairReplaceResult scene_document_internal_replace_decal_asset(
    SceneDocument *document,
    const AssetRegistry *assets,
    SceneInstanceId instance_id,
    uint16_t replacement_asset_id,
    DocumentStateId resulting_state
);

SceneRepairReplaceResult scene_document_internal_replace_surface_material(
    SceneDocument *document,
    const AssetRegistry *assets,
    int map_x,
    int map_y,
    SceneSurfaceKind surface,
    uint8_t replacement_material_id,
    DocumentStateId resulting_state
);

SceneSaveResult scene_document_internal_save_as_native(
    SceneDocument *document,
    const char *path,
    const char *name,
    SceneDiagnostic *out_diagnostic,
    SceneSaveFault fault
);

#endif /* SCENE_DOCUMENT_INTERNAL_H */
