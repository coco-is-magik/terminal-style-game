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

void scene_document_internal_set_current_state(
    SceneDocument *document,
    DocumentStateId state
);

#endif /* SCENE_DOCUMENT_INTERNAL_H */
