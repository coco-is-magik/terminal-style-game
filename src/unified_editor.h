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
#include "editor_domain.h"
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
    EDITOR_MODAL_DIRTY_OPEN_PROMPT,
    EDITOR_MENU_SAVE
} EditorModal;

typedef enum {
    EDITOR_SAVE_MENU_EDIT_NAME = 0,
    EDITOR_SAVE_MENU_CONFIRM_OVERWRITE
} EditorSaveMenuStage;

typedef enum {
    EDITOR_SAVE_OVERWRITE = 0,
    EDITOR_SAVE_EDIT_NAME,
    EDITOR_SAVE_CANCEL,
    EDITOR_SAVE_CHOICE_COUNT
} EditorSaveChoice;

typedef enum {
    EDITOR_CHOOSER_LEGACY_CURRENT = 0,
    EDITOR_CHOOSER_NATIVE_OPEN,
    EDITOR_CHOOSER_LEGACY_IMPORT
} EditorChooserKind;

typedef enum {
    EDITOR_PENDING_NONE = 0,
    EDITOR_PENDING_CHOOSER_LOAD,
    EDITOR_PENDING_NEW,
    EDITOR_PENDING_EXIT_TO_MENU,
    EDITOR_PENDING_WINDOW_CLOSE
} EditorPendingAction;

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
    EDITOR_STATUS_CATALOG_FAILED,
    EDITOR_STATUS_REPAIR_REQUIRED,
    EDITOR_STATUS_DURABILITY_WARNING,
    EDITOR_STATUS_INVALID_SCENE_NAME,
    EDITOR_STATUS_INVALID_NUMERIC_VALUE
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
    WorldState runtime_world;
    CommandHistory history;
    MapCatalog map_catalog;
    char *map_root;
    char *scene_root;
    size_t map_chooser_index;
    size_t pending_map_index;
    EditorChooserKind chooser_kind;
    EditorPendingAction pending_action;
    char save_as_name[65];
    size_t save_as_name_length;
    char save_as_path[1024];
    EditorSaveMenuStage save_menu_stage;
    EditorSaveChoice save_choice;
    EditorModal save_return_menu;
    bool save_force_new_path;

    SelectionTarget selection;
    EditorHit hover;

    AssetRegistry *assets;

    bool inspector_open;
    EditorInspectorKind inspector_kind;
    size_t material_picker_index;
    MaterialId highlighted_material;
    EditorLightField light_field;
    char light_value_text[32];
    size_t light_value_text_length;
    bool light_value_editing;
    int light_repeat_direction;
    double light_repeat_elapsed;

    CommandResult last_command_result;
    SceneLoadResult last_load_result;
    SceneSaveResult last_save_result;
    SceneDiagnostic last_scene_diagnostic;
    MapCatalogResult last_catalog_result;

    bool request_exit_to_main_menu;
    bool request_window_close;
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
MapCatalogResult unified_editor_begin_native_open(UnifiedEditorState *editor,
                                                  const char *scene_root);
MapCatalogResult unified_editor_begin_legacy_import(UnifiedEditorState *editor,
                                                    const char *map_root);
void unified_editor_request_new(UnifiedEditorState *editor);
void unified_editor_request_window_close(UnifiedEditorState *editor);

bool unified_editor_has_document(const UnifiedEditorState *editor);

SceneLoadResult unified_editor_load_scene(
    UnifiedEditorState *editor,
    const char *path
);

SceneLoadResult unified_editor_open_native(UnifiedEditorState *editor,
                                           const char *path);
SceneLoadResult unified_editor_import_legacy(UnifiedEditorState *editor,
                                             const char *path);
SceneLoadResult unified_editor_new_scene(UnifiedEditorState *editor);

EditorInputConsumption unified_editor_update(
    UnifiedEditorState *editor,
    InputState *input,
    Camera *camera,
    double delta_seconds,
    int viewport_rows
);

void unified_editor_render_overlay(
    const UnifiedEditorState *editor,
    Grid *grid
);
void unified_editor_render_text_overlay(
    const UnifiedEditorState *editor,
    Grid *grid
);
bool unified_editor_crosshair_visible(const UnifiedEditorState *editor);

CommandResult unified_editor_set_wall_material(
    UnifiedEditorState *editor,
    MaterialId material
);
CommandResult unified_editor_step_light_field(
    UnifiedEditorState *editor,
    EditorLightField field,
    int direction
);
CommandResult unified_editor_set_light_field_value(
    UnifiedEditorState *editor,
    EditorLightField field,
    double value
);

CommandResult unified_editor_undo(UnifiedEditorState *editor);
CommandResult unified_editor_redo(UnifiedEditorState *editor);
SceneSaveResult unified_editor_save(UnifiedEditorState *editor);
SceneSaveResult unified_editor_save_as(UnifiedEditorState *editor,
                                       const char *path, const char *name);

#endif /* UNIFIED_EDITOR_H */
