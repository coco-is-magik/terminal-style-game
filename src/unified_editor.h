/**
 * unified_editor.h — Unified in-world editor controller
 *
 * Owns SceneDocument + CommandHistory while APP_STATE_EDITOR is active.
 * Borrows AssetRegistry and Camera (via parameters). Material assignment
 * goes through command_history_* only.
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
#include "map_catalog.h"
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
    EDITOR_MODAL_RELOAD_PROMPT,
    EDITOR_MODAL_MAP_CHOOSER,
    EDITOR_MODAL_DIRTY_OPEN_PROMPT
} EditorModal;

/* Exit-prompt choices (plan §9). Resume and Cancel both dismiss without exit. */
typedef enum {
    EDITOR_EXIT_RESUME = 0,
    EDITOR_EXIT_SAVE_AND_EXIT,
    EDITOR_EXIT_DISCARD_AND_EXIT,
    EDITOR_EXIT_CANCEL,
    EDITOR_EXIT_CHOICE_COUNT
} EditorExitChoice;

typedef enum {
    EDITOR_DIRTY_OPEN_CANCEL = 0,
    EDITOR_DIRTY_OPEN_SAVE,
    EDITOR_DIRTY_OPEN_DISCARD,
    EDITOR_DIRTY_OPEN_CHOICE_COUNT
} EditorDirtyOpenChoice;


typedef enum {
    EDITOR_STATUS_NONE = 0,
    EDITOR_STATUS_SAVED,
    EDITOR_STATUS_SAVE_FAILED,
    EDITOR_STATUS_INVALID_SELECTION,
    EDITOR_STATUS_INVALID_MATERIAL,
    EDITOR_STATUS_UNSAVABLE_MATERIAL_ID,
    EDITOR_STATUS_OUT_OF_MEMORY,
    EDITOR_STATUS_STATE_ID_EXHAUSTED,
    EDITOR_STATUS_LOAD_FAILED,
    EDITOR_STATUS_CATALOG_FAILED
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
    MapCatalog map_catalog;
    char *map_root;
    size_t map_chooser_index;
    size_t pending_map_index;

    SelectionTarget selection;
    EditorHit hover;

    AssetRegistry *assets;

    bool inspector_open;
    size_t material_picker_index;
    MaterialId highlighted_material;

    CommandResult last_command_result;
    SceneLoadResult last_load_result;
    SceneSaveResult last_save_result;
    MapCatalogResult last_catalog_result;

    bool request_exit_to_main_menu;
    bool active;

    /* Valid while modal == EDITOR_MODAL_EXIT_PROMPT. */
    EditorExitChoice exit_choice;

    /* Valid while modal == EDITOR_MODAL_DIRTY_OPEN_PROMPT. */
    EditorDirtyOpenChoice dirty_open_choice;
} UnifiedEditorState;


bool unified_editor_init(
    UnifiedEditorState *editor,
    AssetRegistry *assets
);

void unified_editor_destroy(UnifiedEditorState *editor);

MapCatalogResult unified_editor_begin_map_open(
    UnifiedEditorState *editor,
    const char *map_root
);

bool unified_editor_has_document(const UnifiedEditorState *editor);

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

CommandResult unified_editor_set_wall_material(
    UnifiedEditorState *editor,
    MaterialId material
);

CommandResult unified_editor_undo(UnifiedEditorState *editor);
CommandResult unified_editor_redo(UnifiedEditorState *editor);
SceneSaveResult unified_editor_save(UnifiedEditorState *editor);

#endif /* UNIFIED_EDITOR_H */
