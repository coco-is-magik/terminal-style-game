/**
 * unified_editor.h — Unified in-world editor controller (Phase 4 shell)
 *
 * Owns SceneDocument + CommandHistory while APP_STATE_EDITOR is active.
 * Borrows AssetRegistry and Camera (via parameters). Material assignment
 * wrappers are intentionally deferred to Phase 5 — no stub apply APIs.
 */

#ifndef UNIFIED_EDITOR_H
#define UNIFIED_EDITOR_H

#include "assets.h"
#include "camera.h"
#include "command_system.h"
#include "editor_selection.h"
#include "editor_types.h"
#include "grid.h"
#include "input.h"
#include "scene_document.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    EDITOR_MODE_WALK = 0,
    EDITOR_MODE_EDIT
} EditorMode;

typedef enum {
    EDITOR_MODAL_NONE = 0,
    EDITOR_MODAL_EXIT_PROMPT,
    EDITOR_MODAL_RELOAD_PROMPT
} EditorModal;

typedef enum {
    EDITOR_STATUS_NONE = 0,
    EDITOR_STATUS_SAVED,
    EDITOR_STATUS_SAVE_FAILED,
    EDITOR_STATUS_INVALID_SELECTION,
    EDITOR_STATUS_INVALID_MATERIAL,
    EDITOR_STATUS_UNSAVABLE_MATERIAL_ID,
    EDITOR_STATUS_OUT_OF_MEMORY,
    EDITOR_STATUS_STATE_ID_EXHAUSTED,
    EDITOR_STATUS_LOAD_FAILED
} EditorStatus;

typedef struct {
    bool keyboard_consumed;
    bool pointer_consumed;
} EditorInputConsumption;

typedef struct {
    EditorMode mode;
    EditorModal modal;
    EditorStatus status;

    SceneDocument document;
    CommandHistory history;

    SelectionTarget selection;
    EditorHit hover;

    AssetRegistry *assets;

    bool inspector_open;
    size_t material_picker_index;
    MaterialId highlighted_material;

    CommandResult last_command_result;
    SceneLoadResult last_load_result;
    SceneSaveResult last_save_result;

    bool request_exit_to_main_menu;
    bool active;
} UnifiedEditorState;

bool unified_editor_init(
    UnifiedEditorState *editor,
    AssetRegistry *assets
);

void unified_editor_destroy(UnifiedEditorState *editor);

SceneLoadResult unified_editor_load_scene(
    UnifiedEditorState *editor,
    const char *path
);

EditorInputConsumption unified_editor_update(
    UnifiedEditorState *editor,
    InputState *input,
    Camera *camera,
    double delta_seconds
);

void unified_editor_render_overlay(
    const UnifiedEditorState *editor,
    Grid *grid
);

#endif /* UNIFIED_EDITOR_H */
