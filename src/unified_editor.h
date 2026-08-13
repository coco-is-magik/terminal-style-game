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
#include "material_document.h"
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
    EDITOR_MODAL_MATERIAL_COLLISION,
    EDITOR_MODAL_MATERIAL_OVERWRITE_PROMPT,
    EDITOR_MODAL_LIGHT_REMOVE_PROMPT,
    EDITOR_MODAL_DECAL_REMOVE_PROMPT,
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
    EDITOR_DECAL_MENU_LIST = 0,
    EDITOR_DECAL_MENU_PATTERNS,
    EDITOR_DECAL_MENU_CREATE_DIMENSIONS
} EditorDecalMenuStage;


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
    EDITOR_STATUS_INVALID_NUMERIC_VALUE,
    EDITOR_STATUS_INVALID_DECAL,
    EDITOR_STATUS_WALL_ATTACHMENT_BLOCKED,
    EDITOR_STATUS_SPAWN_BLOCKED,
    EDITOR_STATUS_PLAYER_BLOCKED
    ,EDITOR_STATUS_MAP_LIMIT
    ,EDITOR_STATUS_RESIZE_BLOCKED
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
    bool has_player_cell;
    int player_map_x;
    int player_map_y;

    bool inspector_open;
    EditorInspectorKind inspector_kind;
    size_t material_picker_index;
    MaterialId highlighted_material;
    EditorSurfaceField surface_field;
    bool material_picker_open;
    bool decal_menu_open;
    EditorDecalMenuStage decal_menu_stage;
    size_t decal_menu_index;
    size_t decal_pattern_index;
    size_t decal_create_cols;
    size_t decal_create_rows;
    bool decal_create_edit_rows;
    MaterialId *material_shortlist;
    size_t material_shortlist_count;
    size_t material_shortlist_capacity;
    MaterialId *material_search_results;
    size_t material_search_result_count;
    size_t material_search_result_capacity;
    char material_search_text[MATERIAL_NAME_CAPACITY];
    size_t material_search_text_length;
    MaterialId material_collision_id;
    char *material_root;
    char *asset_root;
    uint16_t *decal_shortlist;
    size_t decal_shortlist_count;
    size_t decal_shortlist_capacity;
    EditorLightField light_field;
    EditorDecalField decal_field;
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

void unified_editor_set_runtime_build_failure_for_test(bool fail);


bool unified_editor_init(
    UnifiedEditorState *editor,
    AssetRegistry *assets
);

void unified_editor_destroy(UnifiedEditorState *editor);
bool unified_editor_set_material_root(UnifiedEditorState *editor,
                                      const char *material_root);
bool unified_editor_set_asset_root(UnifiedEditorState *editor,
                                   const char *asset_root);
size_t unified_editor_material_shortlist_count(const UnifiedEditorState *editor);
MaterialId unified_editor_material_shortlist_at(const UnifiedEditorState *editor,
                                                size_t index);
size_t unified_editor_decal_shortlist_count(const UnifiedEditorState *editor);
uint16_t unified_editor_decal_shortlist_at(const UnifiedEditorState *editor,
                                           size_t index);

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
CommandResult unified_editor_set_surface_material(
    UnifiedEditorState *editor,
    int map_x,
    int map_y,
    SceneSurfaceKind surface,
    MaterialId material
);
CommandResult unified_editor_set_ambient_intensity(
    UnifiedEditorState *editor,
    double intensity
);
CommandResult unified_editor_place_wall(
    UnifiedEditorState *editor,
    int map_x,
    int map_y
);
CommandResult unified_editor_remove_wall(
    UnifiedEditorState *editor,
    int map_x,
    int map_y
);
CommandResult unified_editor_place_light(UnifiedEditorState *editor);
CommandResult unified_editor_remove_light(
    UnifiedEditorState *editor,
    SceneInstanceId id
);
CommandResult unified_editor_place_decal(
    UnifiedEditorState *editor,
    uint16_t asset_id,
    double width,
    double height
);
CommandResult unified_editor_remove_decal(
    UnifiedEditorState *editor,
    SceneInstanceId id
);
CommandResult unified_editor_step_decal_field(
    UnifiedEditorState *editor,
    EditorDecalField field,
    int direction
);
CommandResult unified_editor_set_decal_field_value(
    UnifiedEditorState *editor,
    EditorDecalField field,
    double value
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
