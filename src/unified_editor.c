/**
 * unified_editor.c — Unified in-world editor controller
 *
 * Owns SceneDocument + CommandHistory. Material apply/undo/redo/save
 * route exclusively through command_history_* / scene_document_save.
 */

#include "unified_editor.h"
#include "editor_domain.h"
#include "editor_highlight.h"

#include "camera.h"
#include "config.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define EDITOR_PICKER_VISIBLE 6
#define EDITOR_MAP_CHOOSER_VISIBLE 10
#define EDITOR_LIGHT_PICK_RADIUS 0.50
#define EDITOR_LIGHT_REPEAT_DELAY_SECONDS 0.35
#define EDITOR_LIGHT_REPEAT_INTERVAL_SECONDS 0.08
#define EDITOR_LIGHT_REPEAT_MAX_STEPS_PER_FRAME 8

static bool g_fail_runtime_build_for_test = false;

void unified_editor_set_runtime_build_failure_for_test(bool fail) {
    g_fail_runtime_build_for_test = fail;
}

static void editor_cancel_light_value_edit(UnifiedEditorState *editor);

static void editor_clear_selection(UnifiedEditorState *editor) {
    editor->selection.type = SELECTION_NONE;
    memset(&editor->selection.value, 0, sizeof(editor->selection.value));
    editor->hover.valid = false;
    editor->hover.distance = 0.0;
    editor->hover.target.type = SELECTION_NONE;
    memset(&editor->hover.target.value, 0, sizeof(editor->hover.target.value));
}

static void editor_reset_session_ui(UnifiedEditorState *editor) {
    editor->mode = EDITOR_MODE_WALK;
    editor->modal = EDITOR_MODAL_NONE;
    editor->status = EDITOR_STATUS_NONE;
    editor->inspector_open = false;
    editor->inspector_kind = EDITOR_INSPECTOR_NONE;
    editor->material_picker_index = 0;
    editor->highlighted_material = 0;
    editor->surface_field = EDITOR_SURFACE_FIELD_MATERIAL;
    editor->material_picker_open = false;
    editor->light_field = EDITOR_LIGHT_FIELD_X;
    editor->light_value_text[0] = '\0';
    editor->light_value_text_length = 0U;
    editor->light_value_editing = false;
    editor->light_repeat_direction = 0;
    editor->light_repeat_elapsed = 0.0;
    editor->last_command_result = CMD_RESULT_OK;
    editor->last_save_result = SCENE_SAVE_OK;
    scene_diagnostic_reset(&editor->last_scene_diagnostic);
    editor->last_catalog_result = MAP_CATALOG_OK;
    editor->request_exit_to_main_menu = false;
    editor->request_window_close = false;
    editor->exit_choice = EDITOR_EXIT_RESUME;
    editor->dirty_open_choice = EDITOR_DIRTY_OPEN_CANCEL;
    editor->map_chooser_index = 0;
    editor->pending_map_index = 0;
    editor->chooser_kind = EDITOR_CHOOSER_LEGACY_CURRENT;
    editor->pending_action = EDITOR_PENDING_NONE;
    editor->save_as_name[0] = '\0';
    editor->save_as_name_length = 0;
    editor->save_as_path[0] = '\0';
    editor->save_menu_stage = EDITOR_SAVE_MENU_EDIT_NAME;
    editor->save_choice = EDITOR_SAVE_OVERWRITE;
    editor->save_return_menu = EDITOR_MODAL_NONE;
    editor->save_force_new_path = false;
    editor_clear_selection(editor);
}

static char *editor_duplicate_string(const char *text) {
    size_t len;
    char *copy;

    if (!text) return NULL;
    len = strlen(text);
    copy = malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, text, len + 1);
    return copy;
}

static const char *editor_exit_choice_label(EditorExitChoice choice) {
    switch (choice) {
        case EDITOR_EXIT_RESUME:           return "Resume";
        case EDITOR_EXIT_SAVE_AND_EXIT:    return "Save and Exit";
        case EDITOR_EXIT_DISCARD_AND_EXIT: return "Discard and Exit";
        case EDITOR_EXIT_CANCEL:           return "Cancel";
        default:                           return "?";
    }
}

static const char *editor_dirty_open_choice_label(EditorDirtyOpenChoice choice) {
    switch (choice) {
        case EDITOR_DIRTY_OPEN_CANCEL:  return "Cancel";
        case EDITOR_DIRTY_OPEN_SAVE:    return "Save current map";
        case EDITOR_DIRTY_OPEN_DISCARD: return "Discard changes";
        default:                        return "?";
    }
}

static const char *editor_save_choice_label(EditorSaveChoice choice) {
    switch (choice) {
        case EDITOR_SAVE_OVERWRITE: return "Overwrite";
        case EDITOR_SAVE_EDIT_NAME: return "Edit name";
        case EDITOR_SAVE_CANCEL:    return "Cancel";
        default:                    return "?";
    }
}


static void editor_mark_keyboard(EditorInputConsumption *c) {
    c->keyboard_consumed = true;
}

static const char *editor_mode_label(EditorMode mode) {
    return mode == EDITOR_MODE_EDIT ? "EDIT" : "WALK";
}

static const char *editor_face_label(WallFace face) {
    switch (face) {
        case WALL_FACE_NORTH: return "N";
        case WALL_FACE_SOUTH: return "S";
        case WALL_FACE_EAST:  return "E";
        case WALL_FACE_WEST:  return "W";
        default:              return "?";
    }
}

static const char *editor_status_label(EditorStatus status) {
    switch (status) {
        case EDITOR_STATUS_SAVED:                 return "Saved";
        case EDITOR_STATUS_SAVE_FAILED:           return "Save failed";
        case EDITOR_STATUS_INVALID_SELECTION:     return "Invalid selection";
        case EDITOR_STATUS_INVALID_MATERIAL:      return "Invalid material";
        case EDITOR_STATUS_UNSAVABLE_MATERIAL_ID:  return "Unsaveable material ID";
        case EDITOR_STATUS_OUT_OF_MEMORY:         return "Out of memory";
        case EDITOR_STATUS_STATE_ID_EXHAUSTED:    return "State ID exhausted";
        case EDITOR_STATUS_LOAD_FAILED:           return "Load failed";
        case EDITOR_STATUS_CATALOG_FAILED:        return "Map list failed";
        case EDITOR_STATUS_REPAIR_REQUIRED:       return "Repair required before Save";
        case EDITOR_STATUS_DURABILITY_WARNING:    return "Saved; durability warning";
        case EDITOR_STATUS_INVALID_SCENE_NAME:    return "Invalid scene name";
        case EDITOR_STATUS_INVALID_NUMERIC_VALUE: return "Invalid or out-of-range number";
        case EDITOR_STATUS_WALL_ATTACHMENT_BLOCKED: return "Remove attached wall decal first";
        case EDITOR_STATUS_SPAWN_BLOCKED:          return "Cannot place wall at spawn";
        case EDITOR_STATUS_PLAYER_BLOCKED:         return "Leave cell before placing wall";
        case EDITOR_STATUS_MAP_LIMIT:              return "Map dimension limit reached";
        case EDITOR_STATUS_RESIZE_BLOCKED:         return "Map resize blocked by outer content";
        case EDITOR_STATUS_NONE:
        default:                                  return "";
    }
}

/* ---- Material picker helpers ----------------------------------------- */

static size_t editor_count_loaded_materials(const AssetRegistry *assets) {
    size_t n = 0;
    int id;

    if (!assets) {
        return 0;
    }
    for (id = 1; id <= 255; id++) {
        if (material_id_is_loaded(assets, id)) {
            n++;
        }
    }
    return n;
}

static bool editor_material_at_picker_index(
    const AssetRegistry *assets,
    size_t index,
    MaterialId *out_id
) {
    size_t n = 0;
    int id;

    if (!assets || !out_id) {
        return false;
    }
    for (id = 1; id <= 255; id++) {
        if (!material_id_is_loaded(assets, id)) {
            continue;
        }
        if (n == index) {
            *out_id = (MaterialId)id;
            return true;
        }
        n++;
    }
    return false;
}

static bool editor_find_picker_index_for_material(
    const AssetRegistry *assets,
    MaterialId material,
    size_t *out_index
) {
    size_t n = 0;
    int id;

    if (!assets || !out_index) {
        return false;
    }
    for (id = 1; id <= 255; id++) {
        if (!material_id_is_loaded(assets, id)) {
            continue;
        }
        if ((MaterialId)id == material) {
            *out_index = n;
            return true;
        }
        n++;
    }
    return false;
}

static void editor_sync_highlighted_from_picker(UnifiedEditorState *editor) {
    MaterialId id = 0;
    size_t count;

    if (!editor || !editor->assets) {
        return;
    }

    count = editor_count_loaded_materials(editor->assets);
    if (count == 0) {
        editor->material_picker_index = 0;
        editor->highlighted_material = 0;
        return;
    }

    if (editor->material_picker_index >= count) {
        editor->material_picker_index = count - 1;
    }

    if (editor_material_at_picker_index(
            editor->assets,
            editor->material_picker_index,
            &id)) {
        editor->highlighted_material = id;
    } else {
        editor->highlighted_material = 0;
    }
}

static void editor_rebuild_picker_for_selection(UnifiedEditorState *editor) {
    MaterialId current = 0;
    size_t idx = 0;

    if (!editor) {
        return;
    }

    editor->material_picker_index = 0;
    editor->highlighted_material = 0;

    if (editor->selection.type == SELECTION_WALL_FACE) {
        WallMaterialRef ref =
            editor_wall_face_to_material_ref(editor->selection.value.wall_face);
        if (scene_document_get_wall_material(&editor->document, ref, &current) &&
            editor_find_picker_index_for_material(
                editor->assets, current, &idx)) {
            editor->material_picker_index = idx;
        }
    } else if (editor->selection.type == SELECTION_FLOOR ||
               editor->selection.type == SELECTION_CEILING) {
        SceneSurfaceKind surface = editor->selection.type == SELECTION_FLOOR
            ? SCENE_SURFACE_FLOOR : SCENE_SURFACE_CEILING;
        if (scene_document_get_surface_material(
                &editor->document,
                editor->selection.value.horizontal.map_x,
                editor->selection.value.horizontal.map_y,
                surface, &current) &&
            editor_find_picker_index_for_material(editor->assets, current, &idx)) {
            editor->material_picker_index = idx;
        }
    }

    editor_sync_highlighted_from_picker(editor);
}

static bool editor_document_has_unsaveable_material(
    const SceneDocument *document
) {
    size_t length;
    if (!document || !document->path) return false;
    length = strlen(document->path);
    if (length > 7U &&
        strcmp(document->path + length - 7U, ".tscene") == 0) {
        return false;
    }
    return scene_document_validate_for_save(document) ==
           SCENE_SAVE_UNREPRESENTABLE_MATERIAL;
}

static void editor_refresh_unsaveable_status(UnifiedEditorState *editor) {
    if (!editor) {
        return;
    }
    if (editor_document_has_unsaveable_material(&editor->document)) {
        if (editor->status == EDITOR_STATUS_NONE ||
            editor->status == EDITOR_STATUS_SAVED ||
            editor->status == EDITOR_STATUS_UNSAVABLE_MATERIAL_ID) {
            editor->status = EDITOR_STATUS_UNSAVABLE_MATERIAL_ID;
        }
    } else if (editor->status == EDITOR_STATUS_UNSAVABLE_MATERIAL_ID) {
        editor->status = EDITOR_STATUS_NONE;
    }
}

static void editor_map_command_result(
    UnifiedEditorState *editor,
    CommandResult result
) {
    editor->last_command_result = result;
    switch (result) {
        case CMD_RESULT_OK:
        case CMD_RESULT_NO_CHANGE:
        case CMD_RESULT_NOTHING_TO_UNDO:
        case CMD_RESULT_NOTHING_TO_REDO:
            editor->status = EDITOR_STATUS_NONE;
            break;
        case CMD_RESULT_INVALID_TARGET:
            editor->status = EDITOR_STATUS_INVALID_SELECTION;
            break;
        case CMD_RESULT_MATERIAL_NOT_LOADED:
            editor->status = EDITOR_STATUS_INVALID_MATERIAL;
            break;
        case CMD_RESULT_WALL_ATTACHMENT_BLOCKED:
            editor->status = EDITOR_STATUS_WALL_ATTACHMENT_BLOCKED;
            break;
        case CMD_RESULT_SPAWN_BLOCKED:
            editor->status = EDITOR_STATUS_SPAWN_BLOCKED;
            break;
        case CMD_RESULT_PLAYER_BLOCKED:
            editor->status = EDITOR_STATUS_PLAYER_BLOCKED;
            break;
        case CMD_RESULT_MAP_LIMIT:
            editor->status = EDITOR_STATUS_MAP_LIMIT;
            break;
        case CMD_RESULT_RESIZE_BLOCKED:
            editor->status = EDITOR_STATUS_RESIZE_BLOCKED;
            break;
        case CMD_RESULT_OUT_OF_MEMORY:
            editor->status = EDITOR_STATUS_OUT_OF_MEMORY;
            break;
        case CMD_RESULT_STATE_ID_EXHAUSTED:
            editor->status = EDITOR_STATUS_STATE_ID_EXHAUSTED;
            break;
        default:
            editor->status = EDITOR_STATUS_NONE;
            break;
    }
    editor_refresh_unsaveable_status(editor);
}

static bool editor_selection_is_valid(
    const UnifiedEditorState *editor,
    SelectionTarget selection
) {
    if (!editor || selection.type == SELECTION_NONE) return false;
    if (selection.type == SELECTION_WALL_FACE) {
        return editor_selection_is_valid_for_map(
            selection, scene_document_get_map(&editor->document));
    }
    if (selection.type == SELECTION_LIGHT) {
        return scene_document_find_light(
            &editor->document, selection.value.light.id) != NULL;
    }
    if (selection.type == SELECTION_FLOOR ||
        selection.type == SELECTION_CEILING) {
        return editor_selection_is_valid_for_map(
            selection, scene_document_get_map(&editor->document));
    }
    return false;
}

static void editor_revalidate_selection(UnifiedEditorState *editor) {
    if (!editor || editor->selection.type == SELECTION_NONE) return;
    if (!editor_selection_is_valid(editor, editor->selection)) {
        editor_clear_selection(editor);
        editor->inspector_open = false;
        editor->inspector_kind = EDITOR_INSPECTOR_NONE;
        editor->status = EDITOR_STATUS_INVALID_SELECTION;
    }
}

static bool editor_command_requires_runtime_refresh(const EditorCommand *command) {
    size_t i;
    if (!command) return false;
    for (i = 0U; i < command->mutation_count; i++) {
        EditorMutationType type = command->mutations[i].type;
        if (type == EDITOR_MUTATION_SET_LIGHT ||
            type == EDITOR_MUTATION_SET_AMBIENT_INTENSITY ||
            type == EDITOR_MUTATION_PLACE_WALL ||
            type == EDITOR_MUTATION_REMOVE_WALL ||
            type == EDITOR_MUTATION_GROW_EAST ||
            type == EDITOR_MUTATION_GROW_SOUTH ||
            type == EDITOR_MUTATION_SHRINK_EAST ||
            type == EDITOR_MUTATION_SHRINK_SOUTH) return true;
    }
    return false;
}

static CommandExecutionContext editor_command_context(
    const UnifiedEditorState *editor
) {
    CommandExecutionContext context = {0};
    if (!editor) return context;
    context.assets = editor->assets;
    context.has_player_cell = editor->has_player_cell;
    context.player_map_x = editor->player_map_x;
    context.player_map_y = editor->player_map_y;
    return context;
}

static bool editor_refresh_runtime(UnifiedEditorState *editor) {
    WorldState candidate;
    if (!editor) return false;
    world_init(&candidate);
    if (g_fail_runtime_build_for_test || scene_document_build_runtime_world(
            &editor->document, editor->assets, &candidate) !=
        SCENE_RUNTIME_BUILD_OK) {
        world_clear(&candidate);
        editor->status = EDITOR_STATUS_OUT_OF_MEMORY;
        return false;
    }
    world_clear(&editor->runtime_world);
    editor->runtime_world = candidate;
    return true;
}

static bool editor_command_commit_runtime(
    UnifiedEditorState *editor,
    CommandResult result,
    size_t old_count,
    size_t old_cursor,
    DocumentStateId old_next_state
) {
    CommandExecutionContext context;
    if (result != CMD_RESULT_OK) return result == CMD_RESULT_NO_CHANGE;
    if (editor_refresh_runtime(editor)) return true;
    context = editor_command_context(editor);
    context.has_player_cell = false;
    (void)command_history_undo_checked(&editor->history, &editor->document, &context);
    editor->history.count = old_count;
    editor->history.cursor = old_cursor;
    editor->history.next_state_id = old_next_state;
    editor->last_command_result = CMD_RESULT_OUT_OF_MEMORY;
    editor->status = EDITOR_STATUS_OUT_OF_MEMORY;
    return false;
}

/* ---- Public lifecycle ------------------------------------------------ */

bool unified_editor_init(
    UnifiedEditorState *editor,
    AssetRegistry *assets
) {
    if (!editor || !assets) {
        return false;
    }

    memset(editor, 0, sizeof(*editor));
    scene_document_init(&editor->document);
    world_init(&editor->runtime_world);
    command_history_init(&editor->history, 1);
    map_catalog_init(&editor->map_catalog);
    editor->assets = assets;
    editor_reset_session_ui(editor);
    editor->active = true;
    return true;
}

void unified_editor_destroy(UnifiedEditorState *editor) {
    if (!editor) {
        return;
    }

    command_history_destroy(&editor->history);
    scene_document_destroy(&editor->document);
    world_clear(&editor->runtime_world);
    map_catalog_clear(&editor->map_catalog);
    free(editor->map_root);
    editor->map_root = NULL;
    free(editor->scene_root);
    editor->scene_root = NULL;
    editor->assets = NULL;
    editor->active = false;
    editor_reset_session_ui(editor);
}

bool unified_editor_has_document(const UnifiedEditorState *editor) {
    const Map *map;
    if (!editor || !editor->active) return false;
    map = scene_document_get_map(&editor->document);
    return map && map->cells && map->width > 0 && map->height > 0;
}

MapCatalogResult unified_editor_begin_map_open(
    UnifiedEditorState *editor,
    const char *map_root
) {
    const char *root;
    char *new_root = NULL;
    MapCatalogResult result;

    if (!editor || !editor->active) return MAP_CATALOG_INVALID_ARGUMENT;

    if (map_root) {
        if (map_root[0] == '\0') return MAP_CATALOG_INVALID_ARGUMENT;
        new_root = editor_duplicate_string(map_root);
        if (!new_root) {
            editor->last_catalog_result = MAP_CATALOG_OUT_OF_MEMORY;
            editor->status = EDITOR_STATUS_CATALOG_FAILED;
            editor->modal = EDITOR_MODAL_MAP_CHOOSER;
            return MAP_CATALOG_OUT_OF_MEMORY;
        }
        root = new_root;
    } else {
        root = editor->map_root;
    }

    editor->modal = EDITOR_MODAL_MAP_CHOOSER;
    editor->chooser_kind = EDITOR_CHOOSER_LEGACY_CURRENT;
    editor->dirty_open_choice = EDITOR_DIRTY_OPEN_CANCEL;
    if (!root) {
        editor->last_catalog_result = MAP_CATALOG_INVALID_ARGUMENT;
        editor->status = EDITOR_STATUS_CATALOG_FAILED;
        return MAP_CATALOG_INVALID_ARGUMENT;
    }

    result = map_catalog_refresh(&editor->map_catalog, root);
    editor->last_catalog_result = result;
    if (result != MAP_CATALOG_OK) {
        free(new_root);
        editor->status = EDITOR_STATUS_CATALOG_FAILED;
        return result;
    }

    if (new_root) {
        free(editor->map_root);
        editor->map_root = new_root;
        new_root = NULL;
    }

    if (editor->map_catalog.count == 0) {
        editor->map_chooser_index = 0;
    } else if (editor->map_chooser_index >= editor->map_catalog.count) {
        editor->map_chooser_index = editor->map_catalog.count - 1;
    }
    editor->status = EDITOR_STATUS_NONE;
    return MAP_CATALOG_OK;
}

static MapCatalogResult editor_begin_filtered_chooser(
    UnifiedEditorState *editor,
    const char *requested_root,
    char **stored_root,
    EditorChooserKind kind,
    bool native
) {
    const char *root;
    char *new_root = NULL;
    MapCatalogResult result;

    if (!editor || !editor->active || !stored_root) {
        return MAP_CATALOG_INVALID_ARGUMENT;
    }
    if (requested_root) {
        if (requested_root[0] == '\0') return MAP_CATALOG_INVALID_ARGUMENT;
        new_root = editor_duplicate_string(requested_root);
        if (!new_root) {
            editor->status = EDITOR_STATUS_CATALOG_FAILED;
            return MAP_CATALOG_OUT_OF_MEMORY;
        }
        root = new_root;
    } else {
        root = *stored_root;
    }
    if (!root) {
        free(new_root);
        editor->status = EDITOR_STATUS_CATALOG_FAILED;
        return MAP_CATALOG_INVALID_ARGUMENT;
    }
    result = native ? map_catalog_refresh_native(&editor->map_catalog, root)
                    : map_catalog_refresh(&editor->map_catalog, root);
    editor->last_catalog_result = result;
    if (result != MAP_CATALOG_OK) {
        free(new_root);
        editor->status = EDITOR_STATUS_CATALOG_FAILED;
        return result;
    }
    if (new_root) {
        free(*stored_root);
        *stored_root = new_root;
    }
    editor->chooser_kind = kind;
    editor->map_chooser_index = 0;
    editor->modal = EDITOR_MODAL_MAP_CHOOSER;
    editor->status = EDITOR_STATUS_NONE;
    return MAP_CATALOG_OK;
}

MapCatalogResult unified_editor_begin_native_open(UnifiedEditorState *editor,
                                                  const char *scene_root) {
    return editor_begin_filtered_chooser(editor, scene_root, &editor->scene_root,
                                         EDITOR_CHOOSER_NATIVE_OPEN, true);
}

MapCatalogResult unified_editor_begin_legacy_import(UnifiedEditorState *editor,
                                                    const char *map_root) {
    return editor_begin_filtered_chooser(editor, map_root, &editor->map_root,
                                         EDITOR_CHOOSER_LEGACY_IMPORT, false);
}

/* DEPRECATED — retained for the second removal pass after regression tests are
   added. The legacy-current open path now routes through
   unified_editor_import_legacy(), and F5 reload now uses editor_reload_document().
   This function is no longer reachable. */
SceneLoadResult unified_editor_load_scene(
    UnifiedEditorState *editor,
    const char *path
) {
    SceneDocument candidate_document;
    WorldState candidate_runtime;
    if (!editor || !path) {
        return SCENE_LOAD_FILE_NOT_FOUND;
    }

    /* Snapshot UI that must survive a failed load. */
    SelectionTarget prev_selection = editor->selection;
    EditorHit prev_hover = editor->hover;
    EditorMode prev_mode = editor->mode;
    bool prev_inspector = editor->inspector_open;
    EditorInspectorKind prev_inspector_kind = editor->inspector_kind;
    EditorLightField prev_light_field = editor->light_field;
    EditorModal prev_modal = editor->modal;

    scene_document_init(&candidate_document);
    world_init(&candidate_runtime);
    SceneLoadResult result = scene_document_load(&candidate_document, path);
    editor->last_load_result = result;

    if (result != SCENE_LOAD_OK) {
        scene_document_destroy(&candidate_document);
        world_clear(&candidate_runtime);
        editor->status = EDITOR_STATUS_LOAD_FAILED;
        editor->selection = prev_selection;
        editor->hover = prev_hover;
        editor->mode = prev_mode;
        editor->inspector_open = prev_inspector;
        editor->inspector_kind = prev_inspector_kind;
        editor->light_field = prev_light_field;
        editor->modal = prev_modal;
        return result;
    }

    if (scene_document_build_runtime_world(&candidate_document, editor->assets,
                                           &candidate_runtime) !=
        SCENE_RUNTIME_BUILD_OK) {
        scene_document_destroy(&candidate_document);
        world_clear(&candidate_runtime);
        editor->status = EDITOR_STATUS_LOAD_FAILED;
        return SCENE_LOAD_VALIDATION_FAILED;
    }

    scene_document_destroy(&editor->document);
    world_clear(&editor->runtime_world);
    editor->document = candidate_document;
    editor->runtime_world = candidate_runtime;

    command_history_destroy(&editor->history);
    command_history_init(&editor->history, editor->document.current_state);
    editor_clear_selection(editor);
    editor->inspector_open = false;
    editor->inspector_kind = EDITOR_INSPECTOR_NONE;
    editor->inspector_kind = EDITOR_INSPECTOR_NONE;
    editor->modal = EDITOR_MODAL_NONE;
    editor->status = EDITOR_STATUS_NONE;
    editor->request_exit_to_main_menu = false;
    editor->exit_choice = EDITOR_EXIT_RESUME;
    editor->material_picker_index = 0;
    editor->highlighted_material = 0;
    editor_sync_highlighted_from_picker(editor);
    return SCENE_LOAD_OK;
}

typedef SceneLoadResult (*EditorDocumentLoadFn)(SceneDocument *, const char *,
                                                SceneDiagnostic *);

static SceneLoadResult editor_replace_document(UnifiedEditorState *editor,
                                               const char *path,
                                               EditorDocumentLoadFn load) {
    SceneDocument candidate_document;
    WorldState candidate_runtime;
    SceneDiagnostic diagnostic;
    SceneLoadResult result;
    if (!editor || !editor->active || !path || !load) {
        return SCENE_LOAD_VALIDATION_FAILED;
    }
    scene_document_init(&candidate_document);
    world_init(&candidate_runtime);
    result = load(&candidate_document, path, &diagnostic);
    editor->last_load_result = result;
    if (result != SCENE_LOAD_OK ||
        scene_document_build_runtime_world(&candidate_document, editor->assets,
                                           &candidate_runtime) !=
            SCENE_RUNTIME_BUILD_OK) {
        scene_document_destroy(&candidate_document);
        world_clear(&candidate_runtime);
        editor->status = EDITOR_STATUS_LOAD_FAILED;
        return result == SCENE_LOAD_OK ? SCENE_LOAD_VALIDATION_FAILED : result;
    }
    scene_document_destroy(&editor->document);
    world_clear(&editor->runtime_world);
    editor->document = candidate_document;
    editor->runtime_world = candidate_runtime;
    command_history_destroy(&editor->history);
    command_history_init(&editor->history, editor->document.current_state);
    editor_clear_selection(editor);
    editor->inspector_open = false;
    editor->inspector_kind = EDITOR_INSPECTOR_NONE;
    editor->modal = EDITOR_MODAL_NONE;
    editor->status = scene_document_is_repair_required(&editor->document)
                         ? EDITOR_STATUS_REPAIR_REQUIRED : EDITOR_STATUS_NONE;
    return SCENE_LOAD_OK;
}

SceneLoadResult unified_editor_open_native(UnifiedEditorState *editor,
                                           const char *path) {
    SceneDocument candidate_document;
    WorldState candidate_runtime;
    SceneDiagnostic diagnostic;
    SceneLoadResult result;
    if (!editor || !editor->active || !path) return SCENE_LOAD_VALIDATION_FAILED;
    scene_document_init(&candidate_document);
    world_init(&candidate_runtime);
    result = scene_document_load_native_with_assets(&candidate_document, path,
                                                    editor->assets, &diagnostic);
    if (result != SCENE_LOAD_OK ||
        scene_document_build_runtime_world(&candidate_document, editor->assets,
                                           &candidate_runtime) !=
            SCENE_RUNTIME_BUILD_OK) {
        scene_document_destroy(&candidate_document);
        world_clear(&candidate_runtime);
        editor->last_load_result = result;
        editor->status = EDITOR_STATUS_LOAD_FAILED;
        return result == SCENE_LOAD_OK ? SCENE_LOAD_VALIDATION_FAILED : result;
    }
    scene_document_destroy(&editor->document);
    world_clear(&editor->runtime_world);
    editor->document = candidate_document;
    editor->runtime_world = candidate_runtime;
    command_history_destroy(&editor->history);
    command_history_init(&editor->history, editor->document.current_state);
    editor_clear_selection(editor);
    editor->inspector_open = false;
    editor->inspector_kind = EDITOR_INSPECTOR_NONE;
    editor->modal = EDITOR_MODAL_NONE;
    editor->last_load_result = SCENE_LOAD_OK;
    editor->status = scene_document_is_repair_required(&editor->document)
                         ? EDITOR_STATUS_REPAIR_REQUIRED : EDITOR_STATUS_NONE;
    return SCENE_LOAD_OK;
}

SceneLoadResult unified_editor_import_legacy(UnifiedEditorState *editor,
                                             const char *path) {
    return editor_replace_document(editor, path, scene_document_import_legacy);
}

SceneLoadResult unified_editor_new_scene(UnifiedEditorState *editor) {
    SceneDocument candidate_document;
    WorldState candidate_runtime;
    SceneLoadResult result;
    if (!editor || !editor->active) return SCENE_LOAD_VALIDATION_FAILED;
    scene_document_init(&candidate_document);
    world_init(&candidate_runtime);
    result = scene_document_create_new(&candidate_document);
    if (result != SCENE_LOAD_OK ||
        scene_document_build_runtime_world(&candidate_document, editor->assets,
                                           &candidate_runtime) !=
            SCENE_RUNTIME_BUILD_OK) {
        scene_document_destroy(&candidate_document);
        world_clear(&candidate_runtime);
        editor->status = EDITOR_STATUS_LOAD_FAILED;
        return result == SCENE_LOAD_OK ? SCENE_LOAD_VALIDATION_FAILED : result;
    }
    scene_document_destroy(&editor->document);
    world_clear(&editor->runtime_world);
    editor->document = candidate_document;
    editor->runtime_world = candidate_runtime;
    command_history_destroy(&editor->history);
    command_history_init(&editor->history, editor->document.current_state);
    editor_clear_selection(editor);
    editor->inspector_open = false;
    editor->modal = EDITOR_MODAL_NONE;
    editor->status = EDITOR_STATUS_NONE;
    return SCENE_LOAD_OK;
}


/* ---- Mutation wrappers (Phase 5) ------------------------------------- */

CommandResult unified_editor_set_wall_material(
    UnifiedEditorState *editor,
    MaterialId material
) {
    EditorMutationRequest request;
    CommandResult result;

    if (!editor || !editor->active || !editor->assets) {
        return CMD_RESULT_INVALID_TARGET;
    }

    if (editor->selection.type != SELECTION_WALL_FACE) {
        editor->status = EDITOR_STATUS_INVALID_SELECTION;
        editor->last_command_result = CMD_RESULT_INVALID_TARGET;
        return CMD_RESULT_INVALID_TARGET;
    }

    if (!material_id_is_loaded(editor->assets, (int)material)) {
        editor->status = EDITOR_STATUS_INVALID_MATERIAL;
        editor->last_command_result = CMD_RESULT_INVALID_TARGET;
        return CMD_RESULT_INVALID_TARGET;
    }

    {
        const Map *map = scene_document_get_map(&editor->document);
        if (!editor_selection_is_valid_for_map(editor->selection, map)) {
            editor->status = EDITOR_STATUS_INVALID_SELECTION;
            editor->last_command_result = CMD_RESULT_INVALID_TARGET;
            return CMD_RESULT_INVALID_TARGET;
        }
    }

    if (!editor_domain_make_wall_material_request(
            editor->selection, material, &request)) {
        editor->status = EDITOR_STATUS_INVALID_SELECTION;
        editor->last_command_result = CMD_RESULT_INVALID_TARGET;
        return CMD_RESULT_INVALID_TARGET;
    }
    {
        CommandExecutionContext context = editor_command_context(editor);
        result = command_history_execute_group_checked(
            &editor->history, &editor->document, &request, 1U, &context);
    }

    editor_map_command_result(editor, result);
    editor_revalidate_selection(editor);

    /* Only the deprecated legacy writer is limited to one decimal digit. */
    if ((result == CMD_RESULT_OK || result == CMD_RESULT_NO_CHANGE) &&
        material > 9 &&
        editor_document_has_unsaveable_material(&editor->document)) {
        editor->status = EDITOR_STATUS_UNSAVABLE_MATERIAL_ID;
    }

    return result;
}

CommandResult unified_editor_set_surface_material(
    UnifiedEditorState *editor,
    int map_x,
    int map_y,
    SceneSurfaceKind surface,
    MaterialId material
) {
    CommandExecutionContext context;
    CommandResult result;
    if (!editor || !editor->active) return CMD_RESULT_INVALID_TARGET;
    context = editor_command_context(editor);
    result = command_history_set_surface_material(
        &editor->history, &editor->document, map_x, map_y,
        surface, material, &context);
    editor_map_command_result(editor, result);
    return result;
}

CommandResult unified_editor_set_ambient_intensity(
    UnifiedEditorState *editor,
    double intensity
) {
    CommandResult result;
    size_t old_count;
    size_t old_cursor;
    DocumentStateId old_next;
    if (!editor || !editor->active) return CMD_RESULT_INVALID_TARGET;
    old_count = editor->history.count;
    old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    result = command_history_set_ambient_intensity(
        &editor->history, &editor->document, intensity);
    editor_map_command_result(editor, result);
    if (result == CMD_RESULT_OK && !editor_command_commit_runtime(
            editor, result, old_count, old_cursor, old_next))
        return CMD_RESULT_OUT_OF_MEMORY;
    return result;
}

static CommandResult editor_set_wall_occupancy(
    UnifiedEditorState *editor,
    int map_x,
    int map_y,
    bool occupied
) {
    CommandExecutionContext context;
    CommandResult result;
    size_t old_count;
    size_t old_cursor;
    DocumentStateId old_next;
    if (!editor || !editor->active) return CMD_RESULT_INVALID_TARGET;
    old_count = editor->history.count;
    old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    context = editor_command_context(editor);
    result = occupied
        ? command_history_place_wall(
            &editor->history, &editor->document, map_x, map_y, &context)
        : command_history_remove_wall(
            &editor->history, &editor->document, map_x, map_y, &context);
    editor_map_command_result(editor, result);
    if (result == CMD_RESULT_OK && !editor_command_commit_runtime(
            editor, result, old_count, old_cursor, old_next))
        return CMD_RESULT_OUT_OF_MEMORY;
    if (result == CMD_RESULT_OK) {
        editor_clear_selection(editor);
        editor->inspector_open = false;
        editor->inspector_kind = EDITOR_INSPECTOR_NONE;
    }
    return result;
}

CommandResult unified_editor_place_wall(
    UnifiedEditorState *editor, int map_x, int map_y
) {
    return editor_set_wall_occupancy(editor, map_x, map_y, true);
}

CommandResult unified_editor_remove_wall(
    UnifiedEditorState *editor, int map_x, int map_y
) {
    return editor_set_wall_occupancy(editor, map_x, map_y, false);
}

CommandResult unified_editor_step_light_field(
    UnifiedEditorState *editor,
    EditorLightField field,
    int direction
) {
    EditorMutationRequest request;
    CommandResult result;
    if (!editor || !editor->active ||
        !editor_domain_make_light_step_request(
            &editor->document, editor->selection, field, direction, &request)) {
        if (editor) {
            editor->status = EDITOR_STATUS_INVALID_SELECTION;
            editor->last_command_result = CMD_RESULT_INVALID_TARGET;
        }
        return CMD_RESULT_INVALID_TARGET;
    }
    result = command_history_execute_group(
        &editor->history, &editor->document, &request, 1U);
    editor_map_command_result(editor, result);
    editor_revalidate_selection(editor);
    if (result == CMD_RESULT_OK) (void)editor_refresh_runtime(editor);
    return result;
}

CommandResult unified_editor_set_light_field_value(
    UnifiedEditorState *editor,
    EditorLightField field,
    double value
) {
    EditorMutationRequest request;
    CommandResult result;
    if (!editor || !editor->active ||
        !editor_domain_make_light_value_request(
            &editor->document, editor->selection, field, value, &request)) {
        if (editor) {
            editor->status = EDITOR_STATUS_INVALID_NUMERIC_VALUE;
            editor->last_command_result = CMD_RESULT_INVALID_TARGET;
        }
        return CMD_RESULT_INVALID_TARGET;
    }
    result = command_history_execute_group(
        &editor->history, &editor->document, &request, 1U);
    editor_map_command_result(editor, result);
    editor_revalidate_selection(editor);
    if (result == CMD_RESULT_OK) (void)editor_refresh_runtime(editor);
    return result;
}

CommandResult unified_editor_undo(UnifiedEditorState *editor) {
    CommandResult result;
    bool refresh_runtime = false;

    if (!editor || !editor->active) {
        return CMD_RESULT_NOTHING_TO_UNDO;
    }

    if (editor->history.cursor > 0U) {
        refresh_runtime = editor_command_requires_runtime_refresh(
            &editor->history.commands[editor->history.cursor - 1U]);
    }
    {
        CommandExecutionContext context = editor_command_context(editor);
        result = command_history_undo_checked(
            &editor->history, &editor->document, &context);
    }
    editor_map_command_result(editor, result);
    editor_revalidate_selection(editor);
    if (result == CMD_RESULT_OK && refresh_runtime)
        (void)editor_refresh_runtime(editor);
    return result;
}

CommandResult unified_editor_redo(UnifiedEditorState *editor) {
    CommandResult result;
    bool refresh_runtime = false;

    if (!editor || !editor->active) {
        return CMD_RESULT_NOTHING_TO_REDO;
    }

    if (editor->history.cursor < editor->history.count) {
        refresh_runtime = editor_command_requires_runtime_refresh(
            &editor->history.commands[editor->history.cursor]);
    }
    {
        CommandExecutionContext context = editor_command_context(editor);
        result = command_history_redo_checked(
            &editor->history, &editor->document, &context);
    }
    editor_map_command_result(editor, result);
    editor_revalidate_selection(editor);
    if (result == CMD_RESULT_OK && refresh_runtime)
        (void)editor_refresh_runtime(editor);
    return result;
}

SceneSaveResult unified_editor_save(UnifiedEditorState *editor) {
    SceneSaveResult result;

    if (!editor || !editor->active) {
        return SCENE_SAVE_NO_PATH;
    }

    if (editor->document.path) {
        size_t length = strlen(editor->document.path);
        result = length > 7U &&
                         strcmp(editor->document.path + length - 7U,
                                ".tscene") == 0
                     ? scene_document_save_native(&editor->document, NULL)
                     : scene_document_save(&editor->document);
    } else {
        result = SCENE_SAVE_NO_PATH;
    }
    editor->last_save_result = result;

    if (result == SCENE_SAVE_OK) {
        editor->status = EDITOR_STATUS_SAVED;
    } else if (result == SCENE_SAVE_OK_DURABILITY_WARNING) {
        editor->status = EDITOR_STATUS_DURABILITY_WARNING;
    } else if (result == SCENE_SAVE_REPAIR_BLOCKED) {
        editor->status = EDITOR_STATUS_REPAIR_REQUIRED;
    } else if (result == SCENE_SAVE_UNREPRESENTABLE_MATERIAL) {
        editor->status = EDITOR_STATUS_UNSAVABLE_MATERIAL_ID;
    } else {
        editor->status = EDITOR_STATUS_SAVE_FAILED;
    }

    /* Save failure must not discard history or edits. */
    return result;
}

SceneSaveResult unified_editor_save_as(UnifiedEditorState *editor,
                                       const char *path, const char *name) {
    SceneDiagnostic diagnostic;
    SceneSaveResult result;
    if (!editor || !editor->active) return SCENE_SAVE_INVALID_DOCUMENT;
    result = scene_document_save_as_native(&editor->document, path, name,
                                           &diagnostic);
    editor->last_scene_diagnostic = diagnostic;
    editor->last_save_result = result;
    if (result == SCENE_SAVE_OK) editor->status = EDITOR_STATUS_SAVED;
    else if (result == SCENE_SAVE_OK_DURABILITY_WARNING)
        editor->status = EDITOR_STATUS_DURABILITY_WARNING;
    else if (result == SCENE_SAVE_REPAIR_BLOCKED)
        editor->status = EDITOR_STATUS_REPAIR_REQUIRED;
    else editor->status = EDITOR_STATUS_SAVE_FAILED;
    return result;
}

/* ---- Input handlers -------------------------------------------------- */

static void editor_handle_escape(
    UnifiedEditorState *editor,
    EditorInputConsumption *consumed
) {
    if (editor->modal != EDITOR_MODAL_NONE) {
        editor->modal = EDITOR_MODAL_NONE;
        editor_mark_keyboard(consumed);
        return;
    }

    if (editor->material_picker_open) {
        editor->material_picker_open = false;
        editor_mark_keyboard(consumed);
        return;
    }

    if (editor->inspector_open) {
        editor->inspector_open = false;
        editor->inspector_kind = EDITOR_INSPECTOR_NONE;
        editor_clear_selection(editor);
        editor_mark_keyboard(consumed);
        return;
    }

    if (editor->selection.type != SELECTION_NONE) {
        editor_clear_selection(editor);
        editor_mark_keyboard(consumed);
        return;
    }

    /* Default to Resume so Enter never silently discards dirty work. */
    editor->exit_choice = EDITOR_EXIT_RESUME;
    editor->modal = EDITOR_MODAL_EXIT_PROMPT;
    editor_mark_keyboard(consumed);
}


static void editor_handle_select(
    UnifiedEditorState *editor,
    EditorInputConsumption *consumed
) {
    editor_mark_keyboard(consumed);

    if (!editor->hover.valid ||
        !editor_selection_is_valid(editor, editor->hover.target)) {
        editor->status = EDITOR_STATUS_INVALID_SELECTION;
        return;
    }

    editor->selection = editor->hover.target;
    editor->inspector_kind = editor_domain_inspector_kind(editor->selection);
    editor->inspector_open = editor->inspector_kind != EDITOR_INSPECTOR_NONE;
    editor->light_field = EDITOR_LIGHT_FIELD_X;
    editor->surface_field = EDITOR_SURFACE_FIELD_MATERIAL;
    editor->material_picker_open = false;
    editor_cancel_light_value_edit(editor);
    editor->light_repeat_direction = 0;
    editor->light_repeat_elapsed = 0.0;
    editor->status = EDITOR_STATUS_NONE;
    if (editor->inspector_kind == EDITOR_INSPECTOR_WALL_MATERIAL ||
        editor->inspector_kind == EDITOR_INSPECTOR_FLOOR_SURFACE ||
        editor->inspector_kind == EDITOR_INSPECTOR_CEILING_SURFACE) {
        editor_rebuild_picker_for_selection(editor);
        editor_refresh_unsaveable_status(editor);
    }
}

static void editor_handle_picker_prev(UnifiedEditorState *editor) {
    size_t count = editor_count_loaded_materials(editor->assets);
    if (count == 0) {
        return;
    }
    if (editor->material_picker_index == 0) {
        editor->material_picker_index = count - 1;
    } else {
        editor->material_picker_index--;
    }
    editor_sync_highlighted_from_picker(editor);
}

static void editor_handle_picker_next(UnifiedEditorState *editor) {
    size_t count = editor_count_loaded_materials(editor->assets);
    if (count == 0) {
        return;
    }
    editor->material_picker_index++;
    if (editor->material_picker_index >= count) {
        editor->material_picker_index = 0;
    }
    editor_sync_highlighted_from_picker(editor);
}

static void editor_handle_confirm_apply(UnifiedEditorState *editor) {
    if (!editor->inspector_open) return;
    if (editor->surface_field == EDITOR_SURFACE_FIELD_CONSTRUCTION) {
        if (editor->selection.type == SELECTION_WALL_FACE)
            (void)unified_editor_remove_wall(
                editor, editor->selection.value.wall_face.map_x,
                editor->selection.value.wall_face.map_y);
        else if (editor->selection.type == SELECTION_FLOOR ||
                 editor->selection.type == SELECTION_CEILING)
            (void)unified_editor_place_wall(
                editor, editor->selection.value.horizontal.map_x,
                editor->selection.value.horizontal.map_y);
        return;
    }
    if (editor->surface_field != EDITOR_SURFACE_FIELD_MATERIAL) return;
    if (editor->highlighted_material == 0) {
        editor->status = EDITOR_STATUS_INVALID_MATERIAL;
        return;
    }
    if (editor->selection.type == SELECTION_WALL_FACE)
        (void)unified_editor_set_wall_material(editor, editor->highlighted_material);
    else
        (void)unified_editor_set_surface_material(
            editor, editor->selection.value.horizontal.map_x,
            editor->selection.value.horizontal.map_y,
            editor->selection.type == SELECTION_FLOOR
                ? SCENE_SURFACE_FLOOR : SCENE_SURFACE_CEILING,
            editor->highlighted_material);
}

static bool editor_construction_is_disabled(const UnifiedEditorState *editor) {
    return editor && editor->selection.type == SELECTION_WALL_FACE &&
        (editor->selection.value.wall_face.map_x == 0 ||
         editor->selection.value.wall_face.map_y == 0);
}

static void editor_step_surface_field(UnifiedEditorState *editor, int direction) {
    do {
        if (direction < 0) {
            editor->surface_field = editor->surface_field == EDITOR_SURFACE_FIELD_MATERIAL
                ? (EditorSurfaceField)(EDITOR_SURFACE_FIELD_COUNT - 1)
                : (EditorSurfaceField)(editor->surface_field - 1);
        } else {
            editor->surface_field = (EditorSurfaceField)(
                (editor->surface_field + 1) % EDITOR_SURFACE_FIELD_COUNT);
        }
    } while (editor->surface_field == EDITOR_SURFACE_FIELD_CONSTRUCTION &&
             editor_construction_is_disabled(editor));
}

static void editor_handle_surface_confirm(UnifiedEditorState *editor) {
    if (editor->surface_field == EDITOR_SURFACE_FIELD_MATERIAL) {
        if (editor->material_picker_open) {
            editor_handle_confirm_apply(editor);
            editor->material_picker_open = false;
        } else {
            editor->material_picker_open = true;
        }
    } else if (editor->surface_field == EDITOR_SURFACE_FIELD_AMBIENT) {
        editor->light_value_editing = true;
        editor->light_value_text_length = 0U;
        editor->light_value_text[0] = '\0';
    } else if (!editor_construction_is_disabled(editor)) {
        editor_handle_confirm_apply(editor);
    }
}

static void editor_handle_light_field_prev(UnifiedEditorState *editor) {
    editor->light_value_editing = false;
    editor->light_value_text_length = 0U;
    editor->light_value_text[0] = '\0';
    if (editor->light_field == EDITOR_LIGHT_FIELD_X)
        editor->light_field = EDITOR_LIGHT_FIELD_RADIUS;
    else editor->light_field = (EditorLightField)(editor->light_field - 1);
}

static void editor_handle_light_field_next(UnifiedEditorState *editor) {
    editor->light_value_editing = false;
    editor->light_value_text_length = 0U;
    editor->light_value_text[0] = '\0';
    editor->light_field = (EditorLightField)(editor->light_field + 1);
    if (editor->light_field >= EDITOR_LIGHT_FIELD_COUNT)
        editor->light_field = EDITOR_LIGHT_FIELD_X;
}

static void editor_cancel_light_value_edit(UnifiedEditorState *editor) {
    if (!editor) return;
    editor->light_value_editing = false;
    editor->light_value_text_length = 0U;
    editor->light_value_text[0] = '\0';
}

static void editor_append_light_value_text(UnifiedEditorState *editor,
                                           const char *text) {
    const char *cursor;
    if (!editor || !text) return;
    for (cursor = text; *cursor; cursor++) {
        char ch = *cursor;
        bool allowed = ch >= '0' && ch <= '9';
        if (ch == '.' && strchr(editor->light_value_text, '.') == NULL) allowed = true;
        if (ch == '-' && editor->light_value_text_length == 0U) allowed = true;
        if (!allowed ||
            editor->light_value_text_length + 1U >= sizeof(editor->light_value_text)) {
            continue;
        }
        editor->light_value_text[editor->light_value_text_length++] = ch;
        editor->light_value_text[editor->light_value_text_length] = '\0';
        editor->light_value_editing = true;
    }
}

static void editor_backspace_light_value(UnifiedEditorState *editor) {
    if (!editor || !editor->light_value_editing ||
        editor->light_value_text_length == 0U) return;
    editor->light_value_text[--editor->light_value_text_length] = '\0';
}

static bool editor_commit_light_value(UnifiedEditorState *editor) {
    char *end = NULL;
    double value;
    if (!editor || !editor->light_value_editing ||
        editor->light_value_text_length == 0U) return false;
    value = strtod(editor->light_value_text, &end);
    if (!end || *end != '\0' ||
        unified_editor_set_light_field_value(editor, editor->light_field, value) ==
            CMD_RESULT_INVALID_TARGET) {
        editor->status = EDITOR_STATUS_INVALID_NUMERIC_VALUE;
        return true;
    }
    editor_cancel_light_value_edit(editor);
    return true;
}

static bool editor_commit_ambient_value(UnifiedEditorState *editor) {
    char *end = NULL;
    double value;
    if (!editor || !editor->light_value_editing ||
        editor->light_value_text_length == 0U) return false;
    value = strtod(editor->light_value_text, &end);
    if (!end || *end != '\0' || !isfinite(value) || value < 0.0 || value > 1.0 ||
        unified_editor_set_ambient_intensity(editor, value) != CMD_RESULT_OK) {
        editor->status = EDITOR_STATUS_INVALID_NUMERIC_VALUE;
        return true;
    }
    editor_cancel_light_value_edit(editor);
    return true;
}

static bool editor_is_surface_inspector(const UnifiedEditorState *editor) {
    return editor && (editor->inspector_kind == EDITOR_INSPECTOR_WALL_MATERIAL ||
        editor->inspector_kind == EDITOR_INSPECTOR_FLOOR_SURFACE ||
        editor->inspector_kind == EDITOR_INSPECTOR_CEILING_SURFACE);
}

static int editor_light_repeat_steps(UnifiedEditorState *editor,
                                     const InputState *input,
                                     double delta_seconds) {
    int direction = input->held_arrow_left ? -1 :
                    input->held_arrow_right ? 1 : 0;
    int steps = 0;
    if (direction == 0 || input->editor_decrease_pressed ||
        input->editor_increase_pressed) {
        editor->light_repeat_direction = direction;
        editor->light_repeat_elapsed = direction == 0 ? 0.0 :
            EDITOR_LIGHT_REPEAT_DELAY_SECONDS;
        return 0;
    }
    if (direction != editor->light_repeat_direction) {
        editor->light_repeat_direction = direction;
        editor->light_repeat_elapsed = EDITOR_LIGHT_REPEAT_DELAY_SECONDS;
        return 0;
    }
    if (!isfinite(delta_seconds) || delta_seconds <= 0.0) return 0;
    editor->light_repeat_elapsed -= delta_seconds;
    while (editor->light_repeat_elapsed <= 0.0 &&
           steps < EDITOR_LIGHT_REPEAT_MAX_STEPS_PER_FRAME) {
        editor->light_repeat_elapsed += EDITOR_LIGHT_REPEAT_INTERVAL_SECONDS;
        steps++;
    }
    if (editor->light_repeat_elapsed <= 0.0) {
        editor->light_repeat_elapsed = EDITOR_LIGHT_REPEAT_INTERVAL_SECONDS;
    }
    return steps * direction;
}

/* Reload the current document through the correct boundary: native scenes reload
   natively; anything else (legacy source) re-imports. */
static void editor_reload_document(UnifiedEditorState *editor, const char *path) {
    size_t length;
    if (!editor || !path || path[0] == '\0') return;
    length = strlen(path);
    if (length > 7U && strcmp(path + length - 7U, ".tscene") == 0) {
        (void)unified_editor_open_native(editor, path);
    } else {
        (void)unified_editor_import_legacy(editor, path);
    }
}

static void editor_handle_reload_request(UnifiedEditorState *editor) {
    if (scene_document_is_dirty(&editor->document)) {
        editor->modal = EDITOR_MODAL_RELOAD_PROMPT;
        return;
    }
    editor_reload_document(editor, editor->document.path);
}

static void editor_handle_map_chooser_prev(UnifiedEditorState *editor) {
    if (editor->map_catalog.count == 0) return;
    if (editor->map_chooser_index == 0) {
        editor->map_chooser_index = editor->map_catalog.count - 1;
    } else {
        editor->map_chooser_index--;
    }
}

static void editor_handle_map_chooser_next(UnifiedEditorState *editor) {
    if (editor->map_catalog.count == 0) return;
    editor->map_chooser_index =
        (editor->map_chooser_index + 1) % editor->map_catalog.count;
}

static void editor_return_to_map_chooser(UnifiedEditorState *editor) {
    editor->modal = EDITOR_MODAL_MAP_CHOOSER;
    editor->dirty_open_choice = EDITOR_DIRTY_OPEN_CANCEL;
}

static bool editor_save_succeeded(SceneSaveResult result) {
    return result == SCENE_SAVE_OK ||
           result == SCENE_SAVE_OK_DURABILITY_WARNING;
}

static void editor_attempt_pending_map_load(UnifiedEditorState *editor) {
    const MapCatalogEntry *entry =
        map_catalog_get(&editor->map_catalog, editor->pending_map_index);
    SceneLoadResult result;

    if (!entry) {
        editor->status = EDITOR_STATUS_LOAD_FAILED;
        editor_return_to_map_chooser(editor);
        return;
    }
    editor->modal = EDITOR_MODAL_MAP_CHOOSER;
    if (editor->chooser_kind == EDITOR_CHOOSER_NATIVE_OPEN) {
        result = unified_editor_open_native(editor, entry->path);
    } else {
        /* EDITOR_CHOOSER_LEGACY_CURRENT and EDITOR_CHOOSER_LEGACY_IMPORT both
           import through the SceneDocument boundary. Legacy-current open no
           longer keeps a separate mutable digit-grid document model. */
        result = unified_editor_import_legacy(editor, entry->path);
    }
    if (result != SCENE_LOAD_OK) {
        editor->modal = EDITOR_MODAL_MAP_CHOOSER;
    }
}

static void editor_execute_pending_action(UnifiedEditorState *editor) {
    EditorPendingAction action = editor->pending_action;
    editor->pending_action = EDITOR_PENDING_NONE;
    switch (action) {
        case EDITOR_PENDING_CHOOSER_LOAD:
            editor_attempt_pending_map_load(editor);
            break;
        case EDITOR_PENDING_NEW:
            (void)unified_editor_new_scene(editor);
            break;
        case EDITOR_PENDING_EXIT_TO_MENU:
            editor->request_exit_to_main_menu = true;
            editor->modal = EDITOR_MODAL_NONE;
            break;
        case EDITOR_PENDING_WINDOW_CLOSE:
            editor->request_window_close = true;
            editor->modal = EDITOR_MODAL_NONE;
            break;
        case EDITOR_PENDING_NONE:
        default:
            editor->modal = EDITOR_MODAL_NONE;
            break;
    }
}

static void editor_request_destructive_action(UnifiedEditorState *editor,
                                              EditorPendingAction action) {
    if (!editor || !editor->active) return;
    editor->pending_action = action;
    if (unified_editor_has_document(editor) &&
        scene_document_is_dirty(&editor->document)) {
        editor->dirty_open_choice = EDITOR_DIRTY_OPEN_CANCEL;
        editor->modal = EDITOR_MODAL_DIRTY_OPEN_PROMPT;
        return;
    }
    editor_execute_pending_action(editor);
}

void unified_editor_request_new(UnifiedEditorState *editor) {
    editor_request_destructive_action(editor, EDITOR_PENDING_NEW);
}

void unified_editor_request_window_close(UnifiedEditorState *editor) {
    editor_request_destructive_action(editor, EDITOR_PENDING_WINDOW_CLOSE);
}

static void editor_begin_save_menu(UnifiedEditorState *editor,
                                   bool force_new_path,
                                   EditorModal return_menu) {
    const char *name = scene_document_get_name(&editor->document);
    size_t path_length = editor->document.path
        ? strlen(editor->document.path) : 0U;
    bool has_native_path = path_length > 7U &&
        strcmp(editor->document.path + path_length - 7U, ".tscene") == 0;
    size_t length = has_native_path && name ? strlen(name) : 0U;

    if (length > 64U) length = 64U;
    if (length > 0U) memcpy(editor->save_as_name, name, length);
    editor->save_as_name[length] = '\0';
    editor->save_as_name_length = length;
    editor->save_as_path[0] = '\0';
    editor->status = EDITOR_STATUS_NONE;
    editor->save_menu_stage = EDITOR_SAVE_MENU_EDIT_NAME;
    editor->save_choice = EDITOR_SAVE_OVERWRITE;
    editor->save_return_menu = return_menu;
    editor->save_force_new_path = force_new_path || !has_native_path;
    editor->modal = EDITOR_MENU_SAVE;
}

static bool editor_scene_name_character_valid(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-';
}

static void editor_append_save_as_text(UnifiedEditorState *editor,
                                       const char *text) {
    bool invalid = false;
    if (!editor || !text) return;
    while (*text != '\0') {
        if (!editor_scene_name_character_valid(*text)) {
            invalid = true;
        } else if (editor->save_as_name_length < 64U) {
            editor->save_as_name[editor->save_as_name_length++] = *text;
            editor->save_as_name[editor->save_as_name_length] = '\0';
            editor->save_force_new_path = true;
        } else {
            invalid = true;
        }
        text++;
    }
    editor->status = invalid ? EDITOR_STATUS_INVALID_SCENE_NAME
                             : EDITOR_STATUS_NONE;
}

static void editor_backspace_save_name(UnifiedEditorState *editor) {
    if (!editor || editor->save_as_name_length == 0U) return;
    editor->save_as_name_length--;
    editor->save_as_name[editor->save_as_name_length] = '\0';
    editor->save_force_new_path = true;
    editor->status = EDITOR_STATUS_NONE;
}

static bool editor_prepare_save_as_path(UnifiedEditorState *editor) {
    const char *root = editor->scene_root ? editor->scene_root : "assets/scenes";
    int written;
    if (editor->save_as_name_length == 0U) {
        editor->status = EDITOR_STATUS_INVALID_SCENE_NAME;
        return false;
    }
    if (!editor->save_force_new_path && editor->document.path) {
        written = snprintf(editor->save_as_path, sizeof(editor->save_as_path),
                           "%s", editor->document.path);
    } else {
        written = snprintf(editor->save_as_path, sizeof(editor->save_as_path),
                           "%s/%s.tscene", root, editor->save_as_name);
    }
    if (written < 0 || (size_t)written >= sizeof(editor->save_as_path)) {
        editor->save_as_path[0] = '\0';
        editor->status = EDITOR_STATUS_SAVE_FAILED;
        return false;
    }
    return true;
}

static void editor_finish_save_menu(UnifiedEditorState *editor) {
    SceneSaveResult result = unified_editor_save_as(
        editor, editor->save_as_path, editor->save_as_name);
    if (editor_save_succeeded(result)) {
        if (editor->pending_action != EDITOR_PENDING_NONE) {
            editor_execute_pending_action(editor);
        } else {
            editor->modal = EDITOR_MODAL_NONE;
        }
    } else {
        editor->save_menu_stage = EDITOR_SAVE_MENU_EDIT_NAME;
        editor->modal = EDITOR_MENU_SAVE;
    }
}

static void editor_confirm_save_menu_name(UnifiedEditorState *editor) {
    struct stat metadata;
    if (!editor_prepare_save_as_path(editor)) return;
    if (stat(editor->save_as_path, &metadata) == 0 &&
        (!editor->document.path ||
         strcmp(editor->save_as_path, editor->document.path) != 0)) {
        editor->save_menu_stage = EDITOR_SAVE_MENU_CONFIRM_OVERWRITE;
        editor->save_choice = EDITOR_SAVE_OVERWRITE;
        return;
    }
    editor_finish_save_menu(editor);
}

static void editor_cancel_save_menu(UnifiedEditorState *editor) {
    if (editor->save_menu_stage == EDITOR_SAVE_MENU_CONFIRM_OVERWRITE) {
        editor->save_menu_stage = EDITOR_SAVE_MENU_EDIT_NAME;
        editor->status = EDITOR_STATUS_NONE;
        return;
    }
    if (editor->save_return_menu != EDITOR_MODAL_NONE) {
        editor->modal = editor->save_return_menu;
        if (editor->save_return_menu == EDITOR_MODAL_EXIT_PROMPT) {
            editor->pending_action = EDITOR_PENDING_NONE;
        }
    } else {
        editor->modal = EDITOR_MODAL_NONE;
        editor->pending_action = EDITOR_PENDING_NONE;
    }
}

static void editor_handle_save_choice_prev(UnifiedEditorState *editor) {
    if (editor->save_choice == EDITOR_SAVE_OVERWRITE) {
        editor->save_choice = (EditorSaveChoice)(EDITOR_SAVE_CHOICE_COUNT - 1);
    } else {
        editor->save_choice = (EditorSaveChoice)(editor->save_choice - 1);
    }
}

static void editor_handle_save_choice_next(UnifiedEditorState *editor) {
    editor->save_choice = (EditorSaveChoice)(
        (editor->save_choice + 1) % EDITOR_SAVE_CHOICE_COUNT);
}

static void editor_confirm_save_overwrite(UnifiedEditorState *editor) {
    switch (editor->save_choice) {
        case EDITOR_SAVE_OVERWRITE:
            editor_finish_save_menu(editor);
            break;
        case EDITOR_SAVE_EDIT_NAME:
            editor->save_menu_stage = EDITOR_SAVE_MENU_EDIT_NAME;
            editor->status = EDITOR_STATUS_NONE;
            break;
        case EDITOR_SAVE_CANCEL:
        default:
            editor_cancel_save_menu(editor);
            break;
    }
}

static void editor_handle_map_chooser_confirm(UnifiedEditorState *editor) {
    if (editor->map_catalog.count == 0 ||
        editor->map_chooser_index >= editor->map_catalog.count) {
        return;
    }
    editor->pending_map_index = editor->map_chooser_index;
    editor_request_destructive_action(editor, EDITOR_PENDING_CHOOSER_LOAD);
}

static void editor_handle_dirty_open_prev(UnifiedEditorState *editor) {
    if (editor->dirty_open_choice == EDITOR_DIRTY_OPEN_CANCEL) {
        editor->dirty_open_choice =
            (EditorDirtyOpenChoice)(EDITOR_DIRTY_OPEN_CHOICE_COUNT - 1);
    } else {
        editor->dirty_open_choice =
            (EditorDirtyOpenChoice)(editor->dirty_open_choice - 1);
    }
}

static void editor_handle_dirty_open_next(UnifiedEditorState *editor) {
    editor->dirty_open_choice = (EditorDirtyOpenChoice)(
        (editor->dirty_open_choice + 1) % EDITOR_DIRTY_OPEN_CHOICE_COUNT);
}

static void editor_handle_dirty_open_confirm(UnifiedEditorState *editor) {
    switch (editor->dirty_open_choice) {
        case EDITOR_DIRTY_OPEN_CANCEL:
            if (editor->pending_action == EDITOR_PENDING_CHOOSER_LOAD) {
                editor_return_to_map_chooser(editor);
            } else {
                editor->modal = EDITOR_MODAL_NONE;
            }
            editor->pending_action = EDITOR_PENDING_NONE;
            break;
        case EDITOR_DIRTY_OPEN_SAVE:
            editor_begin_save_menu(editor, false,
                                   EDITOR_MODAL_DIRTY_OPEN_PROMPT);
            break;
        case EDITOR_DIRTY_OPEN_DISCARD:
            editor_execute_pending_action(editor);
            break;
        default:
            editor_return_to_map_chooser(editor);
            break;
    }
}

static void editor_handle_exit_choice_prev(UnifiedEditorState *editor) {
    if (editor->exit_choice == EDITOR_EXIT_RESUME) {
        editor->exit_choice = (EditorExitChoice)(EDITOR_EXIT_CHOICE_COUNT - 1);
    } else {
        editor->exit_choice = (EditorExitChoice)(editor->exit_choice - 1);
    }
}

static void editor_handle_exit_choice_next(UnifiedEditorState *editor) {
    editor->exit_choice =
        (EditorExitChoice)((editor->exit_choice + 1) % EDITOR_EXIT_CHOICE_COUNT);
}

static void editor_handle_exit_confirm(UnifiedEditorState *editor) {
    switch (editor->exit_choice) {
        case EDITOR_EXIT_RESUME:
        case EDITOR_EXIT_CANCEL:
            editor->modal = EDITOR_MODAL_NONE;
            break;

        case EDITOR_EXIT_SAVE_AND_EXIT:
            editor->pending_action = EDITOR_PENDING_EXIT_TO_MENU;
            editor_begin_save_menu(editor, false, EDITOR_MODAL_EXIT_PROMPT);
            break;

        case EDITOR_EXIT_DISCARD_AND_EXIT:
            editor->request_exit_to_main_menu = true;
            editor->modal = EDITOR_MODAL_NONE;
            break;

        default:
            editor->modal = EDITOR_MODAL_NONE;
            break;
    }
}

static void editor_handle_modal_confirm(UnifiedEditorState *editor) {
    if (editor->modal == EDITOR_MODAL_MAP_CHOOSER) {
        editor_handle_map_chooser_confirm(editor);
        return;
    }

    if (editor->modal == EDITOR_MODAL_DIRTY_OPEN_PROMPT) {
        editor_handle_dirty_open_confirm(editor);
        return;
    }

    if (editor->modal == EDITOR_MODAL_EXIT_PROMPT) {
        editor_handle_exit_confirm(editor);
        return;
    }

    if (editor->modal == EDITOR_MODAL_RELOAD_PROMPT) {
        char path_copy[1024];
        path_copy[0] = '\0';
        if (editor->document.path) {
            snprintf(path_copy, sizeof(path_copy), "%s", editor->document.path);
        }
        editor->modal = EDITOR_MODAL_NONE;
        if (path_copy[0] != '\0') {
            editor_reload_document(editor, path_copy);
        }
        return;
    }

    if (editor->modal == EDITOR_MENU_SAVE) {
        if (editor->save_menu_stage == EDITOR_SAVE_MENU_CONFIRM_OVERWRITE) {
            editor_confirm_save_overwrite(editor);
        } else {
            editor_confirm_save_menu_name(editor);
        }
        return;
    }

    editor->modal = EDITOR_MODAL_NONE;
}


EditorInputConsumption unified_editor_update(
    UnifiedEditorState *editor,
    InputState *input,
    Camera *camera,
    double delta_seconds,
    int viewport_rows
) {
    EditorInputConsumption consumed = {false, false};

    if (!editor || !editor->active || !input) return consumed;
    if (camera && isfinite(camera->transform.pos.x) &&
        isfinite(camera->transform.pos.y)) {
        editor->has_player_cell = true;
        editor->player_map_x = (int)floor(camera->transform.pos.x);
        editor->player_map_y = (int)floor(camera->transform.pos.y);
    } else {
        editor->has_player_cell = false;
    }

    /* 1. Invalidate hover every frame before raycast. */
    editor->hover.valid = false;
    editor->hover.distance = 0.0;
    editor->hover.target.type = SELECTION_NONE;
    memset(&editor->hover.target.value, 0, sizeof(editor->hover.target.value));

    {
        const Map *cmap = scene_document_get_map(&editor->document);
        Map *rmap = scene_document_get_map_for_runtime(&editor->document);

        /* 2. Modal input first. */
        if (editor->modal != EDITOR_MODAL_NONE) {
            if (input->editor_cancel_pressed) {
                if (editor->modal == EDITOR_MENU_SAVE) {
                    editor_cancel_save_menu(editor);
                } else if (editor->modal == EDITOR_MODAL_DIRTY_OPEN_PROMPT) {
                    if (editor->pending_action == EDITOR_PENDING_CHOOSER_LOAD) {
                        editor_return_to_map_chooser(editor);
                    } else {
                        editor->pending_action = EDITOR_PENDING_NONE;
                        editor->modal = EDITOR_MODAL_NONE;
                    }
                } else if (editor->modal == EDITOR_MODAL_MAP_CHOOSER &&
                           !unified_editor_has_document(editor)) {
                    editor->modal = EDITOR_MODAL_NONE;
                    editor->request_exit_to_main_menu = true;
                } else {
                    editor->modal = EDITOR_MODAL_NONE;
                }
                editor_mark_keyboard(&consumed);
            } else if (input->editor_confirm_pressed) {
                editor_handle_modal_confirm(editor);
                editor_mark_keyboard(&consumed);
            } else if (editor->modal == EDITOR_MENU_SAVE &&
                       editor->save_menu_stage == EDITOR_SAVE_MENU_EDIT_NAME &&
                       input->editor_text_backspace_pressed) {
                editor_backspace_save_name(editor);
                editor_mark_keyboard(&consumed);
            } else if (editor->modal == EDITOR_MENU_SAVE &&
                       editor->save_menu_stage == EDITOR_SAVE_MENU_EDIT_NAME &&
                       input->text_input_len > 0) {
                editor_append_save_as_text(editor, input->text_input);
                editor_mark_keyboard(&consumed);
            } else if (editor->modal == EDITOR_MENU_SAVE &&
                       editor->save_menu_stage == EDITOR_SAVE_MENU_CONFIRM_OVERWRITE &&
                       input->editor_previous_pressed) {
                editor_handle_save_choice_prev(editor);
                editor_mark_keyboard(&consumed);
            } else if (editor->modal == EDITOR_MENU_SAVE &&
                       editor->save_menu_stage == EDITOR_SAVE_MENU_CONFIRM_OVERWRITE &&
                       input->editor_next_pressed) {
                editor_handle_save_choice_next(editor);
                editor_mark_keyboard(&consumed);
            } else if (editor->modal == EDITOR_MODAL_MAP_CHOOSER &&
                       input->editor_open_pressed) {
                (void)unified_editor_begin_native_open(editor, NULL);
                editor_mark_keyboard(&consumed);
            } else if (editor->modal == EDITOR_MODAL_MAP_CHOOSER &&
                       input->editor_import_pressed) {
                (void)unified_editor_begin_legacy_import(editor, NULL);
                editor_mark_keyboard(&consumed);
            } else if (editor->modal == EDITOR_MODAL_MAP_CHOOSER &&
                       input->editor_previous_pressed) {
                editor_handle_map_chooser_prev(editor);
                editor_mark_keyboard(&consumed);
            } else if (editor->modal == EDITOR_MODAL_MAP_CHOOSER &&
                       input->editor_next_pressed) {
                editor_handle_map_chooser_next(editor);
                editor_mark_keyboard(&consumed);
            } else if (editor->modal == EDITOR_MODAL_DIRTY_OPEN_PROMPT &&
                       input->editor_previous_pressed) {
                editor_handle_dirty_open_prev(editor);
                editor_mark_keyboard(&consumed);
            } else if (editor->modal == EDITOR_MODAL_DIRTY_OPEN_PROMPT &&
                       input->editor_next_pressed) {
                editor_handle_dirty_open_next(editor);
                editor_mark_keyboard(&consumed);
            } else if (editor->modal == EDITOR_MODAL_EXIT_PROMPT &&
                       input->editor_previous_pressed) {
                editor_handle_exit_choice_prev(editor);
                editor_mark_keyboard(&consumed);
            } else if (editor->modal == EDITOR_MODAL_EXIT_PROMPT &&
                       input->editor_next_pressed) {
                editor_handle_exit_choice_next(editor);
                editor_mark_keyboard(&consumed);
            } else if (input->editor_toggle_mode_pressed ||
                       input->editor_select_pressed ||
                       input->editor_undo_pressed ||
                       input->editor_redo_pressed ||
                       input->editor_save_pressed ||
                       input->editor_save_as_pressed ||
                       input->editor_open_pressed ||
                       input->editor_import_pressed ||
                       input->editor_new_pressed ||
                       input->editor_reload_pressed ||
                       input->editor_text_backspace_pressed ||
                       input->editor_previous_pressed ||
                       input->editor_next_pressed ||
                       input->editor_decrease_pressed ||
                       input->editor_increase_pressed) {
                editor_mark_keyboard(&consumed);
            }
            return consumed;
        }


        /* 3. Inline numeric entry precedes the normal escape hierarchy. */
        if (input->editor_cancel_pressed) {
            if (editor->light_value_editing) {
                editor_cancel_light_value_edit(editor);
                editor_mark_keyboard(&consumed);
                return consumed;
            }
            editor_handle_escape(editor, &consumed);
            return consumed;
        }

        /* 4. Mode toggle (no camera mutation). */
        if (input->editor_toggle_mode_pressed) {
            editor->mode = (editor->mode == EDITOR_MODE_WALK)
                ? EDITOR_MODE_EDIT
                : EDITOR_MODE_WALK;
            editor_mark_keyboard(&consumed);
        }

        /* 5. Walk mode: camera moves; edit mode: freeze movement/look. */
        if (editor->mode == EDITOR_MODE_WALK &&
            unified_editor_has_document(editor) && rmap) {
            camera_update(camera, rmap, input, delta_seconds, viewport_rows);
        } else {
            consumed.pointer_consumed = true;
        }

        /* 6. Hover ray every frame (both modes; aim freezes in edit). */
        if (unified_editor_has_document(editor) && cmap) {
            EditorHit hit = editor_raycast_selection(camera, cmap);
            size_t light_count = 0U;
            const SceneLight *lights = scene_document_get_lights(
                &editor->document, &light_count);
            double max_distance = config_get()->raycast_max_distance;
            if (max_distance <= 0.0) max_distance = 20.0;
            editor->hover = editor_pick_light_selection(
                camera, lights, light_count, hit, max_distance,
                EDITOR_LIGHT_PICK_RADIUS);
            editor->hover = editor_pick_horizontal_surface_selection(
                camera, cmap, editor->hover, viewport_rows, max_distance);
        }
    }

    /* 7. Select hovered wall. */
    if (input->editor_select_pressed && !consumed.keyboard_consumed) {
        editor_handle_select(editor, &consumed);
    }

    /* 8. Inspector navigation (only while open). */
    if (!consumed.keyboard_consumed && editor->inspector_open) {
        if (editor_is_surface_inspector(editor) &&
            editor->surface_field == EDITOR_SURFACE_FIELD_AMBIENT &&
            editor->light_value_editing && input->editor_confirm_pressed) {
            (void)editor_commit_ambient_value(editor);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) &&
            editor->surface_field == EDITOR_SURFACE_FIELD_AMBIENT &&
            input->text_input_len > 0) {
            editor_append_light_value_text(editor, input->text_input);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) &&
                   editor->material_picker_open && input->editor_previous_pressed) {
            editor_handle_picker_prev(editor);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) &&
                   editor->material_picker_open && input->editor_next_pressed) {
            editor_handle_picker_next(editor);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && input->editor_previous_pressed) {
            editor_step_surface_field(editor, -1);
            editor_cancel_light_value_edit(editor);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && input->editor_next_pressed) {
            editor_step_surface_field(editor, 1);
            editor_cancel_light_value_edit(editor);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && input->editor_confirm_pressed) {
            editor_handle_surface_confirm(editor); editor_mark_keyboard(&consumed);
        } else if (editor->inspector_kind == EDITOR_INSPECTOR_LIGHT &&
            editor->light_value_editing && input->editor_confirm_pressed) {
            (void)editor_commit_light_value(editor);
            editor_mark_keyboard(&consumed);
        } else if (editor->inspector_kind == EDITOR_INSPECTOR_LIGHT &&
                   editor->light_value_editing &&
                   input->editor_text_backspace_pressed) {
            editor_backspace_light_value(editor);
            editor_mark_keyboard(&consumed);
        } else if (editor->inspector_kind == EDITOR_INSPECTOR_LIGHT &&
                   input->text_input_len > 0) {
            editor_append_light_value_text(editor, input->text_input);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_previous_pressed &&
            editor->inspector_kind == EDITOR_INSPECTOR_WALL_MATERIAL) {
            editor_handle_picker_prev(editor);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_next_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_WALL_MATERIAL) {
            editor_handle_picker_next(editor);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_confirm_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_WALL_MATERIAL) {
            editor_handle_confirm_apply(editor);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_previous_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_LIGHT) {
            editor_handle_light_field_prev(editor);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_next_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_LIGHT) {
            editor_handle_light_field_next(editor);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_decrease_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_LIGHT) {
            (void)unified_editor_step_light_field(
                editor, editor->light_field, -1);
            editor->light_repeat_direction = input->held_arrow_left ? -1 : 0;
            editor->light_repeat_elapsed = input->held_arrow_left
                ? EDITOR_LIGHT_REPEAT_DELAY_SECONDS : 0.0;
            editor_mark_keyboard(&consumed);
        } else if (input->editor_increase_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_LIGHT) {
            (void)unified_editor_step_light_field(
                editor, editor->light_field, 1);
            editor->light_repeat_direction = input->held_arrow_right ? 1 : 0;
            editor->light_repeat_elapsed = input->held_arrow_right
                ? EDITOR_LIGHT_REPEAT_DELAY_SECONDS : 0.0;
            editor_mark_keyboard(&consumed);
        } else if (editor->inspector_kind == EDITOR_INSPECTOR_LIGHT &&
                   !editor->light_value_editing) {
            int repeat = editor_light_repeat_steps(editor, input, delta_seconds);
            int direction = repeat < 0 ? -1 : 1;
            int count = repeat < 0 ? -repeat : repeat;
            while (count-- > 0) {
                (void)unified_editor_step_light_field(
                    editor, editor->light_field, direction);
            }
            if (repeat != 0) editor_mark_keyboard(&consumed);
        }
    }

    /* 9. Global edit actions. */
    if (!consumed.keyboard_consumed) {
        if (input->editor_undo_pressed) {
            (void)unified_editor_undo(editor);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_redo_pressed) {
            (void)unified_editor_redo(editor);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_save_pressed) {
            editor_begin_save_menu(editor, false, EDITOR_MODAL_NONE);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_save_as_pressed) {
            editor_begin_save_menu(editor, true, EDITOR_MODAL_NONE);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_open_pressed) {
            (void)unified_editor_begin_native_open(editor, NULL);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_import_pressed) {
            (void)unified_editor_begin_legacy_import(editor, NULL);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_new_pressed) {
            unified_editor_request_new(editor);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_reload_pressed) {
            editor_handle_reload_request(editor);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_previous_pressed ||
                   input->editor_next_pressed ||
                   input->editor_confirm_pressed ||
                   input->editor_decrease_pressed ||
                   input->editor_increase_pressed) {
            /* Recognized but no-op outside inspector; still consume. */
            editor_mark_keyboard(&consumed);
        }
    }

    return consumed;
}

void unified_editor_render_text_overlay(
    const UnifiedEditorState *editor,
    Grid *grid
) {
    if (!editor || !editor->active || !grid) {
        return;
    }

    {
        SDL_Color fg = {220, 220, 220, 255};
        SDL_Color bg = {0, 0, 0, 255};
        SDL_Color dim = {160, 160, 160, 255};
        SDL_Color warn = {220, 180, 80, 255};
        SDL_Color hi = {120, 220, 160, 255};
        char line[160];
        int row;

        snprintf(line, sizeof(line), "EDITOR  mode:%s  dirty:%s",
                 editor_mode_label(editor->mode),
                 scene_document_is_dirty(&editor->document) ? "yes" : "no");
        grid_print(grid, 1, 1, line, fg, bg);

        if (editor->hover.valid &&
            editor->hover.target.type == SELECTION_WALL_FACE) {
            const WallFaceRef *wf = &editor->hover.target.value.wall_face;
            MaterialId mat = 0;
            WallMaterialRef ref = editor_wall_face_to_material_ref(*wf);
            scene_document_get_wall_material(&editor->document, ref, &mat);
            snprintf(line, sizeof(line),
                     "Hover  (%d,%d) face:%s dist:%.2f mat:%d",
                     wf->map_x, wf->map_y, editor_face_label(wf->face),
                     editor->hover.distance, mat);
        } else if (editor->hover.valid &&
                   editor->hover.target.type == SELECTION_LIGHT) {
            const SceneLight *light = scene_document_find_light(
                &editor->document, editor->hover.target.value.light.id);
            snprintf(line, sizeof(line),
                     "Hover  light:%" PRIu64 " pos:(%.2f,%.2f) dist:%.2f",
                     editor->hover.target.value.light.id,
                     light ? light->x : 0.0, light ? light->y : 0.0,
                     editor->hover.distance);
        } else {
            snprintf(line, sizeof(line), "Hover  (none)");
        }
        grid_print(grid, 1, 2, line, dim, bg);

        if (editor->selection.type == SELECTION_WALL_FACE) {
            const WallFaceRef *wf = &editor->selection.value.wall_face;
            MaterialId mat = 0;
            WallMaterialRef ref = editor_wall_face_to_material_ref(*wf);
            scene_document_get_wall_material(&editor->document, ref, &mat);
            snprintf(line, sizeof(line),
                     "Select (%d,%d) face:%s mat:%d%s",
                     wf->map_x, wf->map_y, editor_face_label(wf->face), mat,
                     material_id_is_loaded(editor->assets, (int)mat)
                         ? "" : " (missing)");
        } else if (editor->selection.type == SELECTION_LIGHT) {
            const SceneLight *light = scene_document_find_light(
                &editor->document, editor->selection.value.light.id);
            snprintf(line, sizeof(line),
                     "Select light:%" PRIu64 " pos:(%.2f,%.2f)",
                     editor->selection.value.light.id,
                     light ? light->x : 0.0, light ? light->y : 0.0);
        } else {
            snprintf(line, sizeof(line), "Select (none)");
        }
        grid_print(grid, 1, 3, line, fg, bg);

        row = 5;
        if (editor->inspector_open && editor_is_surface_inspector(editor)) {
            EditorInspectorPresentation presentation;
            size_t count = editor_count_loaded_materials(editor->assets);
            size_t start;
            size_t i;
            MaterialId current = 0;
            bool current_loaded;

            if (editor_domain_inspector_presentation(
                    editor->inspector_kind, &presentation)) {
                snprintf(line, sizeof(line), "Inspector: %s  %s",
                         presentation.title, presentation.controls);
                grid_print(grid, 1, row++, line, fg, bg);
            }
            if (editor->selection.type == SELECTION_WALL_FACE) {
                WallMaterialRef ref = editor_wall_face_to_material_ref(
                    editor->selection.value.wall_face);
                (void)scene_document_get_wall_material(
                    &editor->document, ref, &current);
            } else {
                (void)scene_document_get_surface_material(
                    &editor->document,
                    editor->selection.value.horizontal.map_x,
                    editor->selection.value.horizontal.map_y,
                    editor->selection.type == SELECTION_FLOOR
                        ? SCENE_SURFACE_FLOOR : SCENE_SURFACE_CEILING,
                    &current);
            }
            current_loaded = material_id_is_loaded(editor->assets, current);
            snprintf(line, sizeof(line), " %s Material   %d%s",
                     editor->surface_field == EDITOR_SURFACE_FIELD_MATERIAL &&
                         !editor->material_picker_open ? ">" : " ",
                     current, current_loaded ? "" : " (missing)");
            grid_print(grid, 1, row++, line,
                       editor->surface_field == EDITOR_SURFACE_FIELD_MATERIAL ? hi : dim,
                       bg);
            if (editor->surface_field == EDITOR_SURFACE_FIELD_MATERIAL && count == 0) {
                grid_print(grid, 1, row++, "  (no loaded materials)", dim, bg);
            } else if (editor->surface_field == EDITOR_SURFACE_FIELD_MATERIAL &&
                       editor->material_picker_open) {
                if (editor->material_picker_index >= EDITOR_PICKER_VISIBLE) {
                    start = editor->material_picker_index -
                            (EDITOR_PICKER_VISIBLE - 1);
                } else {
                    start = 0;
                }
                if (start + EDITOR_PICKER_VISIBLE > count &&
                    count >= EDITOR_PICKER_VISIBLE) {
                    start = count - EDITOR_PICKER_VISIBLE;
                }

                for (i = 0; i < EDITOR_PICKER_VISIBLE && start + i < count; i++) {
                    MaterialId id = 0;
                    size_t idx = start + i;
                    const char *name;
                    const char *mark;

                    if (!editor_material_at_picker_index(
                            editor->assets, idx, &id)) {
                        break;
                    }
                    name = material_name_by_id(editor->assets, (int)id);
                    mark = (idx == editor->material_picker_index) ? ">" : " ";
                    snprintf(line, sizeof(line), " %s %3d  %s",
                             mark, (int)id, name);
                    grid_print(grid, 1, row++, line,
                               idx == editor->material_picker_index ? hi : dim,
                               bg);
                }
            }
            snprintf(line, sizeof(line), " %s %s  %s",
                     editor->surface_field == EDITOR_SURFACE_FIELD_CONSTRUCTION &&
                         !editor_construction_is_disabled(editor) ? ">" : " ",
                     editor->inspector_kind == EDITOR_INSPECTOR_WALL_MATERIAL
                         ? "Remove Wall" : "Place Wall",
                     editor_construction_is_disabled(editor) ? "(unavailable)" : "Enter=apply");
            grid_print(grid, 1, row++, line,
                       editor->surface_field == EDITOR_SURFACE_FIELD_CONSTRUCTION &&
                           !editor_construction_is_disabled(editor) ? hi : dim,
                       bg);
            if (editor->surface_field == EDITOR_SURFACE_FIELD_AMBIENT &&
                editor->light_value_editing) {
                snprintf(line, sizeof(line), " > Ambient    [%s_]",
                         editor->light_value_text);
            } else {
                char ambient[16] = "?";
                (void)editor_domain_format_ambient(
                    scene_document_get_ambient_intensity(&editor->document),
                    ambient, sizeof(ambient));
                snprintf(line, sizeof(line), " %s Ambient    %s",
                         editor->surface_field == EDITOR_SURFACE_FIELD_AMBIENT ? ">" : " ",
                         ambient);
            }
            grid_print(grid, 1, row++, line,
                       editor->surface_field == EDITOR_SURFACE_FIELD_AMBIENT ? hi : dim,
                       bg);
            if (presentation.note) grid_print(grid, 1, row++, presentation.note, warn, bg);
        } else if (editor->inspector_open &&
                   editor->inspector_kind == EDITOR_INSPECTOR_LIGHT) {
            EditorInspectorPresentation presentation;
            const SceneLight *light = scene_document_find_light(
                &editor->document, editor->selection.value.light.id);
            size_t field;
            if (!editor_domain_inspector_presentation(
                    editor->inspector_kind, &presentation)) {
                presentation = (EditorInspectorPresentation){0};
            } else {
                snprintf(line, sizeof(line), "Inspector: %s  %s",
                         presentation.title, presentation.controls);
                grid_print(grid, 1, row++, line, fg, bg);
            }
            for (field = 0; field < presentation.field_count; field++) {
                EditorInspectorFieldPresentation field_presentation;
                char value[32];
                bool selected = field == (size_t)editor->light_field;
                if (!light || !editor_domain_inspector_field_presentation(
                        editor->inspector_kind, field,
                        scene_document_get_map(&editor->document),
                        &field_presentation) ||
                    !editor_domain_format_light_field(
                        light, (EditorLightField)field, value, sizeof(value))) {
                    continue;
                }
                if (selected && editor->light_value_editing) {
                    snprintf(line, sizeof(line), " > %-10s [%s_]",
                             field_presentation.label, editor->light_value_text);
                } else {
                    snprintf(line, sizeof(line), " %s %-10s %s",
                             selected ? ">" : " ", field_presentation.label, value);
                }
                grid_print(grid, 1, row++, line, selected ? hi : dim, bg);
            }
            if (presentation.note) {
                grid_print(grid, 1, row++, presentation.note, warn, bg);
            }
        }

        if (editor_document_has_unsaveable_material(&editor->document) ||
            editor->status == EDITOR_STATUS_UNSAVABLE_MATERIAL_ID) {
            grid_print(grid, 1, row++,
                       "WARNING: document has material ID > 9 (unsavable)",
                       warn, bg);
        }

        {
            const char *status = editor_status_label(editor->status);
            if (status[0] != '\0') {
                snprintf(line, sizeof(line), "Status: %s", status);
                grid_print(grid, 1, row++, line, warn, bg);
            }
        }

        if (editor->modal == EDITOR_MODAL_EXIT_PROMPT) {
            int c;
            grid_print(grid, 1, row++,
                       "Exit editor  Up/Down  Enter=choose  Esc=cancel",
                       warn, bg);
            if (scene_document_is_dirty(&editor->document)) {
                grid_print(grid, 1, row++,
                           "  Document is DIRTY", warn, bg);
            }
            for (c = 0; c < EDITOR_EXIT_CHOICE_COUNT; c++) {
                snprintf(line, sizeof(line), " %s %s",
                         c == (int)editor->exit_choice ? ">" : " ",
                         editor_exit_choice_label((EditorExitChoice)c));
                grid_print(grid, 1, row++, line,
                           c == (int)editor->exit_choice ? hi : dim, bg);
            }
        } else if (editor->modal == EDITOR_MODAL_RELOAD_PROMPT) {
            grid_print(grid, 1, row + 1,
                       "Reload scene? Enter=yes  Esc=cancel", warn, bg);
        } else if (editor->modal == EDITOR_MODAL_MAP_CHOOSER) {
            size_t start = 0;
            size_t i;
            const bool native =
                editor->chooser_kind == EDITOR_CHOOSER_NATIVE_OPEN;

            grid_print(grid, 1, row++,
                       native
                           ? "OPEN SCENE  Up/Down  Enter=open  Ctrl+I=import  Esc=cancel"
                           : "IMPORT LEGACY MAP  Up/Down  Enter=import  Ctrl+O=open  Esc=cancel",
                       warn, bg);
            if (editor->map_catalog.count == 0) {
                grid_print(grid, 1, row++,
                           native
                               ? "  (no .tscene files found)"
                               : "  (no legacy .txt files found)",
                           dim, bg);
            } else {
                if (editor->map_chooser_index >= EDITOR_MAP_CHOOSER_VISIBLE) {
                    start = editor->map_chooser_index -
                            (EDITOR_MAP_CHOOSER_VISIBLE - 1);
                }
                if (start + EDITOR_MAP_CHOOSER_VISIBLE > editor->map_catalog.count &&
                    editor->map_catalog.count >= EDITOR_MAP_CHOOSER_VISIBLE) {
                    start = editor->map_catalog.count - EDITOR_MAP_CHOOSER_VISIBLE;
                }
                for (i = 0; i < EDITOR_MAP_CHOOSER_VISIBLE &&
                            start + i < editor->map_catalog.count; i++) {
                    size_t index = start + i;
                    const MapCatalogEntry *entry =
                        map_catalog_get(&editor->map_catalog, index);
                    snprintf(line, sizeof(line), " %s %s",
                             index == editor->map_chooser_index ? ">" : " ",
                             entry ? entry->name : "?");
                    grid_print(grid, 1, row++, line,
                               index == editor->map_chooser_index ? hi : dim, bg);
                }
            }
        } else if (editor->modal == EDITOR_MODAL_DIRTY_OPEN_PROMPT) {
            int c;
            grid_print(grid, 1, row++,
                       "Unsaved changes  Up/Down  Enter=choose  Esc=back",
                       warn, bg);
            for (c = 0; c < EDITOR_DIRTY_OPEN_CHOICE_COUNT; c++) {
                snprintf(line, sizeof(line), " %s %s",
                         c == (int)editor->dirty_open_choice ? ">" : " ",
                         editor_dirty_open_choice_label((EditorDirtyOpenChoice)c));
                grid_print(grid, 1, row++, line,
                           c == (int)editor->dirty_open_choice ? hi : dim, bg);
            }
        } else if (editor->modal == EDITOR_MENU_SAVE) {
            const char *root = editor->scene_root
                ? editor->scene_root : "assets/scenes";

            grid_print(grid, 1, row++, "SAVE SCENE", warn, bg);
            if (editor->save_menu_stage == EDITOR_SAVE_MENU_EDIT_NAME) {
                snprintf(line, sizeof(line), "Name: %s_", editor->save_as_name);
                grid_print(grid, 1, row++, line, hi, bg);
                if (!editor->save_force_new_path && editor->document.path) {
                    snprintf(line, sizeof(line), "Path: %.145s",
                             editor->document.path);
                } else {
                    snprintf(line, sizeof(line), "Path: %.90s/%.55s.tscene",
                             root, editor->save_as_name);
                }
                grid_print(grid, 1, row++, line, dim, bg);
                grid_print(grid, 1, row++,
                           "Type=name  Backspace=delete  Enter=save  Esc=cancel",
                           dim, bg);
                if (editor->status == EDITOR_STATUS_SAVE_FAILED &&
                    editor->last_scene_diagnostic.code != SCENE_DIAGNOSTIC_NONE) {
                    snprintf(line, sizeof(line), "%s: %.110s",
                             scene_diagnostic_id(
                                 editor->last_scene_diagnostic.code),
                             editor->last_scene_diagnostic.detail);
                    grid_print(grid, 1, row++, line, warn, bg);
                }
            } else {
                int c;
                snprintf(line, sizeof(line), "File exists: %.145s",
                         editor->save_as_path);
                grid_print(grid, 1, row++, line, warn, bg);
                for (c = 0; c < EDITOR_SAVE_CHOICE_COUNT; c++) {
                    snprintf(line, sizeof(line), " %s %s",
                             c == (int)editor->save_choice ? ">" : " ",
                             editor_save_choice_label((EditorSaveChoice)c));
                    grid_print(grid, 1, row++, line,
                               c == (int)editor->save_choice ? hi : dim, bg);
                }
                grid_print(grid, 1, row++,
                           "Up/Down=choose  Enter=select  Esc=edit name",
                           dim, bg);
            }
        }


        grid_print(grid, 1, grid->height - 2,
                   "Tab=walk/edit  E=select  Enter=apply  "
                   "Ctrl+Z/Y=undo/redo  Ctrl+N=new  Ctrl+S=save  Ctrl+O=open",
                   dim, bg);
    }

}

bool unified_editor_crosshair_visible(const UnifiedEditorState *editor) {
    return editor && editor->active && unified_editor_has_document(editor) &&
           editor->modal != EDITOR_MODAL_MAP_CHOOSER &&
           editor->modal != EDITOR_MODAL_DIRTY_OPEN_PROMPT &&
           editor->modal != EDITOR_MENU_SAVE;
}

void unified_editor_render_overlay(const UnifiedEditorState *editor, Grid *grid) {
    unified_editor_render_text_overlay(editor, grid);
    if (unified_editor_crosshair_visible(editor)) {
        editor_crosshair_render(grid);
    }
}
