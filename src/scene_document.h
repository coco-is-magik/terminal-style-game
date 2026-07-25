/**
 * scene_document.h — Authoritative authored map document for the unified editor
 *
 * SceneDocument owns the only editable Map used by APP_STATE_EDITOR.
 * Serialization writes material IDs only (existing digit-grid format).
 * light_map is derived runtime data and is never written to disk.
 */

#ifndef SCENE_DOCUMENT_H
#define SCENE_DOCUMENT_H

#include "editor_types.h"
#include "map.h"

#include <stdbool.h>

typedef struct {
    Map map;
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
    SCENE_SAVE_UNREPRESENTABLE_MATERIAL,
    SCENE_SAVE_TEMP_CREATE_FAILED,
    SCENE_SAVE_WRITE_FAILED,
    SCENE_SAVE_FLUSH_FAILED,
    SCENE_SAVE_CLOSE_FAILED,
    SCENE_SAVE_REPLACE_FAILED
} SceneSaveResult;

void scene_document_init(SceneDocument *document);
void scene_document_destroy(SceneDocument *document);

SceneLoadResult scene_document_load(
    SceneDocument *document,
    const char *path
);

SceneSaveResult scene_document_save(SceneDocument *document);

const Map *scene_document_get_map(const SceneDocument *document);
Map *scene_document_get_map_for_runtime(SceneDocument *document);

bool scene_document_get_wall_material(
    const SceneDocument *document,
    WallMaterialRef ref,
    MaterialId *out_material
);

bool scene_document_is_dirty(const SceneDocument *document);
SceneSaveResult scene_document_validate_for_save(
    const SceneDocument *document
);

#endif /* SCENE_DOCUMENT_H */
