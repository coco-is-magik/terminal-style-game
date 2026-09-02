/**
 * scene_document.h — Authoritative authored scene document
 *
 * SceneDocument owns authored map, metadata, spawn, ambient, placed-instance,
 * identity, path, dirty, provenance, and repair state. Native serialization is
 * introduced incrementally; the temporary legacy load/save adapter still writes
 * digit grids only. Map.light_map remains derived and is never authored.
 */

#ifndef SCENE_DOCUMENT_H
#define SCENE_DOCUMENT_H

#include "editor_types.h"
#include "assets.h"
#include "map.h"
#include "scene_diagnostic.h"
#include "scene_types.h"
#include "surface_view.h"
#include "height_view.h"
#include "optical_runtime_view.h"
#include "world.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    Map map;
    SceneAuthoredCell *authored_cells;
    size_t authored_cell_count;
    char name[SCENE_NAME_MAX + 1U];
    double ambient_intensity;
    double spawn_x;
    double spawn_y;
    double spawn_angle;
    SceneMovementParameters movement;
    SceneLight *lights;
    size_t light_count;
    size_t light_capacity;
    SceneDecalInstance *decals;
    size_t decal_count;
    size_t decal_capacity;
    SceneSpriteInstance *sprites;
    size_t sprite_count;
    size_t sprite_capacity;
    OpticalExtension *optical_material_defaults;
    size_t optical_material_capacity;
    size_t optical_material_storage_capacity;
    OpticalCellOverride *optical_cell_overrides;
    size_t optical_cell_override_count;
    size_t optical_cell_override_capacity;
    uint32_t optical_generation;
    int east_growth[SCENE_MAX_WIDTH];
    size_t east_growth_count;
    int south_growth[SCENE_MAX_HEIGHT];
    size_t south_growth_count;
    SceneInstanceId next_instance_id;
    char *legacy_source_path;
    SceneDiagnostic *repair_diagnostics;
    size_t repair_diagnostic_count;
    size_t repair_diagnostic_capacity;
    bool repair_required;
    bool imported_unsaved;
    bool migration_pending;
    DocumentStateId current_state;
    DocumentStateId saved_state;
    char *path;
} SceneDocument;

typedef enum {
    SCENE_LOAD_OK = 0,
    SCENE_LOAD_FILE_NOT_FOUND,
    SCENE_LOAD_PARSE_ERROR,
    SCENE_LOAD_VALIDATION_FAILED,
    SCENE_LOAD_OUT_OF_MEMORY
} SceneLoadResult;

typedef enum {
    SCENE_SAVE_OK = 0,
    SCENE_SAVE_NO_PATH,
    SCENE_SAVE_INVALID_DOCUMENT,
    SCENE_SAVE_REPAIR_BLOCKED,
    SCENE_SAVE_UNREPRESENTABLE_MATERIAL,
    SCENE_SAVE_TEMP_CREATE_FAILED,
    SCENE_SAVE_WRITE_FAILED,
    SCENE_SAVE_FLUSH_FAILED,
    SCENE_SAVE_FILE_SYNC_FAILED,
    SCENE_SAVE_CLOSE_FAILED,
    SCENE_SAVE_MODE_FAILED,
    SCENE_SAVE_REPLACE_FAILED,
    SCENE_SAVE_OK_DURABILITY_WARNING
} SceneSaveResult;

typedef enum {
    SCENE_RUNTIME_BUILD_OK = 0,
    SCENE_RUNTIME_BUILD_INVALID_ARGUMENT,
    SCENE_RUNTIME_BUILD_OUT_OF_MEMORY,
    SCENE_RUNTIME_BUILD_INVALID_DOCUMENT
} SceneRuntimeBuildResult;

typedef enum {
    MATERIAL_REFERENCE_NONE = 0,
    MATERIAL_REFERENCE_WALL,
    MATERIAL_REFERENCE_FLOOR,
    MATERIAL_REFERENCE_CEILING,
    MATERIAL_REFERENCE_DECAL_PATTERN
} MaterialReferenceKind;

typedef struct {
    MaterialReferenceKind kind;
    int map_x;
    int map_y;
    SceneInstanceId decal_instance_id;
    uint16_t decal_pattern_id;
} MaterialReference;

void scene_document_init(SceneDocument *document);
void scene_document_destroy(SceneDocument *document);

/** Find the first scene/decal-pattern reference blocking material deletion. */
bool scene_document_find_material_reference(
    const SceneDocument *document,
    const AssetRegistry *assets,
    uint16_t material_id,
    MaterialReference *out_reference
);

SceneLoadResult scene_document_create_new(SceneDocument *document);

SceneLoadResult scene_document_load(
    SceneDocument *document,
    const char *path
);

SceneLoadResult scene_document_load_native(
    SceneDocument *document,
    const char *path,
    SceneDiagnostic *out_diagnostic
);

SceneLoadResult scene_document_load_native_with_assets(
    SceneDocument *document,
    const char *path,
    const AssetRegistry *assets,
    SceneDiagnostic *out_diagnostic
);

SceneLoadResult scene_document_import_legacy(
    SceneDocument *document,
    const char *path,
    SceneDiagnostic *out_diagnostic
);

SceneSaveResult scene_document_save(SceneDocument *document);
SceneSaveResult scene_document_save_native(
    SceneDocument *document,
    SceneDiagnostic *out_diagnostic
);
SceneSaveResult scene_document_save_as_native(
    SceneDocument *document,
    const char *path,
    const char *name,
    SceneDiagnostic *out_diagnostic
);

const Map *scene_document_get_map(const SceneDocument *document);
Map *scene_document_get_map_for_runtime(SceneDocument *document);
const SceneAuthoredCell *scene_document_get_authored_cells(
    const SceneDocument *document,
    size_t *out_count
);
bool scene_document_get_surface_view(
    const SceneDocument *document,
    SceneSurfaceView *out_view
);
bool scene_document_get_height_view(
    const SceneDocument *document,
    SceneHeightView *out_view
);
bool scene_document_get_optical_view(
    const SceneDocument *document,
    OpticalRuntimeView *out_view,
    uint32_t *out_generation
);
bool scene_document_get_optical_material_extension(
    const SceneDocument *document, uint16_t material_id,
    OpticalExtension *out_extension
);
bool scene_document_get_optical_cell_extension(
    const SceneDocument *document, size_t cell_index,
    OpticalExtension *out_extension
);

bool scene_document_get_wall_material(
    const SceneDocument *document,
    WallMaterialRef ref,
    MaterialId *out_material
);
bool scene_document_get_surface_material(
    const SceneDocument *document,
    int map_x,
    int map_y,
    SceneSurfaceKind surface,
    MaterialId *out_material
);
bool scene_document_get_cell_occupancy(
    const SceneDocument *document,
    int map_x,
    int map_y,
    SceneCellOccupancy *out_occupancy
);
bool scene_document_get_cell_vertical(
    const SceneDocument *document, int map_x, int map_y,
    SceneCellVertical *out_value
);
bool scene_document_cell_has_wall_decal(
    const SceneDocument *document,
    int map_x,
    int map_y
);

bool scene_document_is_dirty(const SceneDocument *document);
bool scene_document_is_repair_required(const SceneDocument *document);
bool scene_document_is_imported_unsaved(const SceneDocument *document);
const char *scene_document_get_name(const SceneDocument *document);
const char *scene_document_get_path(const SceneDocument *document);
const char *scene_document_get_legacy_source_path(
    const SceneDocument *document
);
double scene_document_get_ambient_intensity(const SceneDocument *document);
void scene_document_get_spawn(
    const SceneDocument *document,
    double *out_x,
    double *out_y,
    double *out_angle
);
const SceneLight *scene_document_get_lights(
    const SceneDocument *document,
    size_t *out_count
);
const SceneLight *scene_document_find_light(
    const SceneDocument *document,
    SceneInstanceId instance_id
);
const SceneDecalInstance *scene_document_get_decals(
    const SceneDocument *document,
    size_t *out_count
);
const SceneDecalInstance *scene_document_find_decal(
    const SceneDocument *document,
    SceneInstanceId instance_id
);
const SceneSpriteInstance *scene_document_get_sprites(
    const SceneDocument *document,
    size_t *out_count
);
const SceneSpriteInstance *scene_document_find_sprite(
    const SceneDocument *document,
    SceneInstanceId instance_id
);
const SceneDiagnostic *scene_document_get_repair_diagnostics(
    const SceneDocument *document,
    size_t *out_count
);
/** Transactionally rebuild missing-asset diagnostics without changing history. */
SceneLoadResult scene_document_refresh_repair_diagnostics(
    SceneDocument *document,
    const AssetRegistry *assets,
    SceneDiagnostic *out_diagnostic
);
SceneInstanceId scene_document_get_next_instance_id(
    const SceneDocument *document
);
SceneSaveResult scene_document_validate_for_save(
    const SceneDocument *document
);
SceneSaveResult scene_document_validate_for_save_diagnostic(
    const SceneDocument *document,
    SceneDiagnostic *out_diagnostic
);

const DecalPatternAsset *scene_document_resolve_decal_pattern(
    const SceneDocument *document,
    const AssetRegistry *assets,
    SceneInstanceId instance_id,
    bool *out_uses_fallback
);

/* Rebuilds a disposable runtime view transactionally from authored scene data. */
SceneRuntimeBuildResult scene_document_build_runtime_world(
    const SceneDocument *document,
    const AssetRegistry *assets,
    WorldState *runtime
);

#endif /* SCENE_DOCUMENT_H */
