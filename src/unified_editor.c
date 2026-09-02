/**
 * unified_editor.c — Unified in-world editor controller
 *
 * Owns SceneDocument + CommandHistory. Material apply/undo/redo/save
 * route exclusively through command_history_* / scene_document_save.
 */

#include "unified_editor.h"
#include "asset_refresh.h"
#include "editor_domain.h"
#include "editor_highlight.h"

#include "camera.h"
#include "config.h"
#include "raycast.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define EDITOR_PICKER_VISIBLE 4
#define EDITOR_MAP_CHOOSER_VISIBLE 10
#define EDITOR_LIGHT_PICK_RADIUS 0.50
#define EDITOR_SPRITE_PICK_RADIUS 0.5
#define EDITOR_LIGHT_REPEAT_DELAY_SECONDS 0.35
#define EDITOR_LIGHT_REPEAT_INTERVAL_SECONDS 0.08
#define EDITOR_LIGHT_REPEAT_MAX_STEPS_PER_FRAME 8

static bool g_fail_runtime_build_for_test = false;

void unified_editor_set_runtime_build_failure_for_test(bool fail) {
    g_fail_runtime_build_for_test = fail;
}

static void editor_cancel_light_value_edit(UnifiedEditorState *editor);
static bool editor_rebuild_material_shortlist(UnifiedEditorState *editor);
static bool editor_rebuild_decal_shortlist(UnifiedEditorState *editor);
static void editor_rebuild_sprite_shortlist(UnifiedEditorState *editor);

static void editor_close_sprite_menu(UnifiedEditorState *editor) {
    if (!editor) return;
    editor->sprite_menu_open = false;
    editor->sprite_menu_stage = EDITOR_SPRITE_MENU_ACTIONS;
    editor->sprite_menu_index = 0U;
    sprite_document_destroy(&editor->sprite_document);
}

static void editor_clear_selection(UnifiedEditorState *editor) {
    editor->selection.type = SELECTION_NONE;
    memset(&editor->selection.value, 0, sizeof(editor->selection.value));
    editor->hover.valid = false;
    editor->hover.distance = 0.0;
    editor->hover.target.type = SELECTION_NONE;
    memset(&editor->hover.target.value, 0, sizeof(editor->hover.target.value));
    editor_selection_set_clear(&editor->selection_set);
}

static void editor_reset_selection_set(UnifiedEditorState *editor) {
    if (!editor) return;
    if (editor->selection.type == SELECTION_WALL_FACE ||
        editor->selection.type == SELECTION_FLOOR ||
        editor->selection.type == SELECTION_CEILING)
        (void)editor_selection_set_reset(&editor->selection_set, editor->selection);
    else editor_selection_set_clear(&editor->selection_set);
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
    editor->decal_menu_open = false;
    editor->decal_menu_open = false;
    editor->movement_menu_open = false;
    editor->movement_field = EDITOR_MOVEMENT_FIELD_GRAVITY_MAGNITUDE;
    editor->optical_menu_open = false;
    editor->transparency_menu_open = false;
    editor->optical_scope = EDITOR_OPTICAL_SCOPE_CELL;
    editor->optical_field = EDITOR_OPTICAL_FIELD_PLAYER_BLOCKS;
    editor->optical_menu_index = 0U;
    editor->transparency_field = EDITOR_TRANSPARENCY_FIELD_MASTER;
    editor->decal_menu_stage = EDITOR_DECAL_MENU_LIST;
    editor->decal_menu_index = 0U;
    editor->decal_pattern_index = 0U;
    editor->decal_create_cols = 1U;
    editor->decal_create_rows = 1U;
    editor->decal_create_edit_rows = false;
    editor->material_search_text[0] = '\0';
    editor->material_search_text_length = 0U;
    editor->material_collision_id = 0U;
    editor->light_field = EDITOR_LIGHT_FIELD_X;
    editor->decal_field = EDITOR_DECAL_FIELD_POSITION_U;
    editor->sprite_field = EDITOR_SPRITE_FIELD_X;
    editor->trigger_field = EDITOR_TRIGGER_FIELD_MIN_X;
    entity_trigger_session_reset(&editor->trigger_session);
    editor->trigger_field = EDITOR_TRIGGER_FIELD_MIN_X;
    editor->sprite_menu_open = false;
    editor->sprite_menu_stage = EDITOR_SPRITE_MENU_ACTIONS;
    editor->sprite_menu_index = 0U;
    editor->sprite_shortlist_count = 0U;
    editor->sprite_paint_x = 0U;
    editor->sprite_paint_y = 0U;
    editor->sprite_paint_material = 1U;
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
        case EDITOR_STATUS_UNSAVABLE_MATERIAL_ID: return "Unsaveable material ID";
        case EDITOR_STATUS_OUT_OF_MEMORY:         return "Out of memory";
        case EDITOR_STATUS_STATE_ID_EXHAUSTED:    return "State ID exhausted";
        case EDITOR_STATUS_LOAD_FAILED:           return "Load failed";
        case EDITOR_STATUS_CATALOG_FAILED:        return "Map list failed";
        case EDITOR_STATUS_REPAIR_REQUIRED:       return "Repair required before Save";
        case EDITOR_STATUS_DURABILITY_WARNING:    return "Saved; durability warning";
        case EDITOR_STATUS_INVALID_SCENE_NAME:    return "Invalid scene name";
        case EDITOR_STATUS_INVALID_NUMERIC_VALUE: return "Invalid or out-of-range number";
        case EDITOR_STATUS_INVALID_DECAL:         return "Invalid decal operation";
        case EDITOR_STATUS_INVALID_SPRITE:        return "Invalid sprite operation";
        case EDITOR_STATUS_SPRITE_PATTERN_SAVED: return "Sprite pattern saved";
        case EDITOR_STATUS_SPRITE_PATTERN_LOADED: return "Sprite pattern loaded";
        case EDITOR_STATUS_WALL_ATTACHMENT_BLOCKED: return "Remove attached wall decal first";
        case EDITOR_STATUS_SPAWN_BLOCKED:          return "Cannot place wall at spawn";
        case EDITOR_STATUS_PLAYER_BLOCKED:         return "Leave cell before placing wall";
        case EDITOR_STATUS_MAP_LIMIT:              return "Map dimension limit reached";
        case EDITOR_STATUS_RESIZE_BLOCKED:         return "Map resize blocked by outer content";
        case EDITOR_STATUS_SELECTION_LIMIT:        return "Selection limit reached (8 faces)";
        case EDITOR_STATUS_HISTORY_LIMIT:          return "Command history memory limit reached";
        case EDITOR_STATUS_INVALID_TRIGGER:        return "Invalid trigger operation";
        case EDITOR_STATUS_TRIGGER_REFERENCE_BLOCKED: return "Light is referenced by a trigger";
        case EDITOR_STATUS_NONE:
        default:                                   return "";
    }
}

/* ---- Material picker helpers ----------------------------------------- */

static bool editor_reserve_material_ids(MaterialId **ids, size_t *capacity,
                                        size_t needed) {
    MaterialId *grown;
    size_t next;
    if (needed <= *capacity) return true;
    next = *capacity == 0U ? 8U : *capacity;
    while (next < needed) {
        if (next > SIZE_MAX / 2U) return false;
        next *= 2U;
    }
    grown = realloc(*ids, next * sizeof(*grown));
    if (!grown) return false;
    *ids = grown;
    *capacity = next;
    return true;
}

static bool editor_shortlist_add(UnifiedEditorState *editor, MaterialId id) {
    size_t i;
    if (id == 0U || !material_id_is_loaded(editor->assets, id)) return true;
    for (i = 0U; i < editor->material_shortlist_count; i++) {
        if (editor->material_shortlist[i] == id) return true;
    }
    if (!editor_reserve_material_ids(&editor->material_shortlist,
                                     &editor->material_shortlist_capacity,
                                     editor->material_shortlist_count + 1U)) return false;
    editor->material_shortlist[editor->material_shortlist_count++] = id;
    return true;
}

static bool editor_material_name_has_prefix(const char *name,
                                            const char *prefix) {
    while (*prefix) {
        unsigned char a = (unsigned char)*name++;
        unsigned char b = (unsigned char)*prefix++;
        if (a == '\0' || tolower(a) != tolower(b)) return false;
    }
    return true;
}

static void editor_sort_shortlist(UnifiedEditorState *editor) {
    size_t i;
    for (i = 1U; i < editor->material_shortlist_count; i++) {
        MaterialId value = editor->material_shortlist[i];
        size_t j = i;
        while (j > 0U) {
            MaterialId previous = editor->material_shortlist[j - 1U];
            int order = strcmp(material_name_by_id(editor->assets, previous),
                               material_name_by_id(editor->assets, value));
            if (order < 0 || (order == 0 && previous < value)) break;
            editor->material_shortlist[j] = previous;
            j--;
        }
        editor->material_shortlist[j] = value;
    }
}

static bool editor_rebuild_search_results(UnifiedEditorState *editor) {
    size_t i;
    editor->material_search_result_count = 0U;
    if (!editor_reserve_material_ids(&editor->material_search_results,
                                     &editor->material_search_result_capacity,
                                     editor->material_shortlist_count)) return false;
    for (i = 0U; i < editor->material_shortlist_count; i++) {
        MaterialId id = editor->material_shortlist[i];
        if (editor_material_name_has_prefix(material_name_by_id(editor->assets, id),
                                            editor->material_search_text)) {
            editor->material_search_results[editor->material_search_result_count++] = id;
        }
    }
    editor->material_picker_index = 0U;
    editor->highlighted_material = editor->material_search_result_count > 0U
        ? editor->material_search_results[0] : (MaterialId)0;
    return true;
}

static bool editor_rebuild_material_shortlist(UnifiedEditorState *editor) {
    const SceneAuthoredCell *cells;
    const SceneDecalInstance *decals;
    size_t count;
    size_t i;
    if (!editor || !editor->assets) return false;
    editor->material_shortlist_count = 0U;
    cells = scene_document_get_authored_cells(&editor->document, &count);
    for (i = 0U; i < count; i++) {
        if (!editor_shortlist_add(editor, cells[i].wall_material) ||
            !editor_shortlist_add(editor, cells[i].floor_material) ||
            !editor_shortlist_add(editor, cells[i].ceiling_material)) return false;
    }
    decals = scene_document_get_decals(&editor->document, &count);
    for (i = 0U; i < count; i++) {
        const DecalPatternAsset *pattern = asset_registry_get_decal_pattern(
            editor->assets, decals[i].asset.id);
        size_t cell_count;
        size_t j;
        if (!pattern || pattern->cols <= 0 || pattern->rows <= 0) continue;
        cell_count = (size_t)pattern->cols * (size_t)pattern->rows;
        for (j = 0U; j < cell_count; j++) {
            if (!editor_shortlist_add(editor, pattern->pattern[j].material_id)) return false;
        }
    }
    editor_sort_shortlist(editor);
    editor->material_search_text[0] = '\0';
    editor->material_search_text_length = 0U;
    return editor_rebuild_search_results(editor);
}

static bool editor_rebuild_decal_shortlist(UnifiedEditorState *editor) {
    const SceneDecalInstance *decals;
    size_t count;
    size_t i;
    if (!editor) return false;
    editor->decal_shortlist_count = 0U;
    decals = scene_document_get_decals(&editor->document, &count);
    for (i = 0U; i < count; i++) {
        uint16_t id = decals[i].asset.id;
        size_t insert = 0U;
        uint16_t *grown;
        size_t capacity;
        if (id == 0U) continue;
        while (insert < editor->decal_shortlist_count &&
               editor->decal_shortlist[insert] < id) insert++;
        if (insert < editor->decal_shortlist_count &&
            editor->decal_shortlist[insert] == id) continue;
        if (editor->decal_shortlist_count == editor->decal_shortlist_capacity) {
            capacity = editor->decal_shortlist_capacity == 0U ? 8U :
                       editor->decal_shortlist_capacity * 2U;
            grown = realloc(editor->decal_shortlist,
                            capacity * sizeof(*grown));
            if (!grown) return false;
            editor->decal_shortlist = grown;
            editor->decal_shortlist_capacity = capacity;
        }
        memmove(editor->decal_shortlist + insert + 1U,
                editor->decal_shortlist + insert,
                (editor->decal_shortlist_count - insert) *
                    sizeof(*editor->decal_shortlist));
        editor->decal_shortlist[insert] = id;
        editor->decal_shortlist_count++;
    }
    return true;
}

static bool editor_rebuild_asset_shortlists(UnifiedEditorState *editor) {
    return editor_rebuild_material_shortlist(editor) &&
           editor_rebuild_decal_shortlist(editor);
}

static bool editor_find_search_index(const UnifiedEditorState *editor,
                                     MaterialId material, size_t *out_index) {
    size_t i;
    if (!editor || !out_index) return false;
    for (i = 0U; i < editor->material_search_result_count; i++) {
        if (editor->material_search_results[i] == material) {
            *out_index = i;
            return true;
        }
    }
    return false;
}

static void editor_sync_highlighted_from_picker(UnifiedEditorState *editor) {
    if (!editor || editor->material_search_result_count == 0U) {
        if (editor) {
            editor->material_picker_index = 0U;
            editor->highlighted_material = 0U;
        }
        return;
    }
    if (editor->material_picker_index >= editor->material_search_result_count) {
        editor->material_picker_index = editor->material_search_result_count - 1U;
    }
    editor->highlighted_material =
        editor->material_search_results[editor->material_picker_index];
}

static void editor_rebuild_picker_for_selection(UnifiedEditorState *editor) {
    MaterialId current = 0U;
    size_t index = 0U;
    if (!editor) return;
    editor->material_picker_index = 0U;
    editor->highlighted_material = 0U;
    if (editor->selection.type == SELECTION_WALL_FACE) {
        WallMaterialRef ref = editor_wall_face_to_material_ref(
            editor->selection.value.wall_face);
        if (scene_document_get_wall_material(&editor->document, ref, &current) &&
            editor_find_search_index(editor, current, &index)) {
            editor->material_picker_index = index;
        }
    } else if (editor->selection.type == SELECTION_FLOOR ||
               editor->selection.type == SELECTION_CEILING) {
        SceneSurfaceKind surface = editor->selection.type == SELECTION_FLOOR
            ? SCENE_SURFACE_FLOOR : SCENE_SURFACE_CEILING;
        if (scene_document_get_surface_material(
                &editor->document, editor->selection.value.horizontal.map_x,
                editor->selection.value.horizontal.map_y, surface, &current) &&
            editor_find_search_index(editor, current, &index)) {
            editor->material_picker_index = index;
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
        case CMD_RESULT_HISTORY_LIMIT:
            editor->status = EDITOR_STATUS_HISTORY_LIMIT;
            break;
        case CMD_RESULT_TRIGGER_REFERENCE_BLOCKED:
            editor->status = EDITOR_STATUS_TRIGGER_REFERENCE_BLOCKED;
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
    if (selection.type == SELECTION_DECAL) {
        return scene_document_find_decal(
            &editor->document, selection.value.decal.id) != NULL;
    }
    if (selection.type == SELECTION_SPRITE)
        return scene_document_find_sprite(
            &editor->document, selection.value.sprite.id) != NULL;
    if (selection.type == SELECTION_TRIGGER)
        return scene_document_find_trigger(
            &editor->document, selection.value.trigger.id) != NULL;
    if (selection.type == SELECTION_FLOOR ||
        selection.type == SELECTION_CEILING) {
        return editor_selection_is_valid_for_map(
            selection, scene_document_get_map(&editor->document));
    }
    return false;
}

static void editor_revalidate_selection(UnifiedEditorState *editor) {
    const SelectionTarget *primary;
    if (!editor || editor->selection.type == SELECTION_NONE) return;
    if (editor->selection.type == SELECTION_WALL_FACE ||
        editor->selection.type == SELECTION_FLOOR ||
        editor->selection.type == SELECTION_CEILING)
        editor_selection_set_revalidate(
            &editor->selection_set, scene_document_get_map(&editor->document));
    primary = editor_selection_set_primary(&editor->selection_set);
    if (primary) editor->selection = *primary;
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
            type == EDITOR_MUTATION_INSERT_LIGHT ||
            type == EDITOR_MUTATION_REMOVE_LIGHT ||
            type == EDITOR_MUTATION_SET_DECAL ||
            type == EDITOR_MUTATION_INSERT_DECAL ||
            type == EDITOR_MUTATION_REMOVE_DECAL ||
            type == EDITOR_MUTATION_SET_SPRITE ||
            type == EDITOR_MUTATION_INSERT_SPRITE ||
            type == EDITOR_MUTATION_REMOVE_SPRITE ||
            type == EDITOR_MUTATION_SET_TRIGGER ||
            type == EDITOR_MUTATION_INSERT_TRIGGER ||
            type == EDITOR_MUTATION_REMOVE_TRIGGER ||
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
    entity_trigger_session_reset(&editor->trigger_session);
    return true;
}

static void editor_tick_triggers(UnifiedEditorState *editor, Camera *camera,
                                 double delta_seconds) {
    EntityTriggerTickResult result;
    size_t i;
    if (!editor || !camera || editor->mode != EDITOR_MODE_WALK ||
        entity_trigger_session_tick(&editor->trigger_session,
            editor->document.triggers, editor->document.trigger_count,
            editor->document.lights, editor->document.light_count,
            editor->document.spawn_x, editor->document.spawn_y,
            editor->document.spawn_angle, camera->transform.pos.x,
            camera->transform.pos.y, delta_seconds, &result) != ENTITY_TRIGGER_OK)
        return;
    if (result.teleported) {
        SceneHeightView heights;
        camera->transform.pos.x = result.player_x;
        camera->transform.pos.y = result.player_y;
        camera->transform.angle = result.player_angle;
        if (scene_document_get_height_view(&editor->document, &heights))
            (void)vertical_physics_reset(&editor->vertical_physics, camera,
                                         &editor->document.map, &heights);
    }
    for (i = 0U; i < editor->document.light_count &&
         i < (size_t)editor->runtime_world.num_lights; i++)
        editor->runtime_world.lights[i].intensity =
            entity_trigger_session_light_enabled(
                &editor->trigger_session, editor->document.lights[i].id)
            ? editor->document.lights[i].intensity : 0.0;
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
    vertical_physics_init(&editor->vertical_physics);
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
    free(editor->material_root);
    editor->material_root = NULL;
    free(editor->asset_root);
    editor->asset_root = NULL;
    free(editor->material_shortlist);
    editor->material_shortlist = NULL;
    editor->material_shortlist_count = 0U;
    editor->material_shortlist_capacity = 0U;
    free(editor->material_search_results);
    editor->material_search_results = NULL;
    editor->material_search_result_count = 0U;
    editor->material_search_result_capacity = 0U;
    free(editor->decal_shortlist);
    editor->decal_shortlist = NULL;
    editor->decal_shortlist_count = 0U;
    editor->decal_shortlist_capacity = 0U;
    sprite_document_destroy(&editor->sprite_document);
    editor->assets = NULL;
    editor->active = false;
    editor_reset_session_ui(editor);
}

bool unified_editor_set_material_root(UnifiedEditorState *editor,
                                      const char *material_root) {
    char *copy;
    if (!editor || !editor->active || !material_root || material_root[0] == '\0') {
        return false;
    }
    copy = editor_duplicate_string(material_root);
    if (!copy) return false;
    free(editor->material_root);
    editor->material_root = copy;
    return true;
}

bool unified_editor_set_asset_root(UnifiedEditorState *editor,
                                   const char *asset_root) {
    char *copy;
    if (!editor || !editor->active || !asset_root || asset_root[0] == '\0') {
        return false;
    }
    copy = editor_duplicate_string(asset_root);
    if (!copy) return false;
    free(editor->asset_root);
    editor->asset_root = copy;
    return true;
}

size_t unified_editor_material_shortlist_count(const UnifiedEditorState *editor) {
    return editor ? editor->material_shortlist_count : 0U;
}

MaterialId unified_editor_material_shortlist_at(const UnifiedEditorState *editor,
                                                size_t index) {
    if (!editor || index >= editor->material_shortlist_count) return 0;
    return editor->material_shortlist[index];
}

size_t unified_editor_decal_shortlist_count(const UnifiedEditorState *editor) {
    return editor ? editor->decal_shortlist_count : 0U;
}

uint16_t unified_editor_decal_shortlist_at(const UnifiedEditorState *editor,
                                           size_t index) {
    if (!editor || index >= editor->decal_shortlist_count) return 0U;
    return editor->decal_shortlist[index];
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
    EditorSelectionSet prev_selection_set = editor->selection_set;
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
        editor->selection_set = prev_selection_set;
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
    vertical_physics_init(&editor->vertical_physics);

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
    if (!editor_rebuild_asset_shortlists(editor)) {
        editor->status = EDITOR_STATUS_OUT_OF_MEMORY;
    }
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
    vertical_physics_init(&editor->vertical_physics);
    command_history_destroy(&editor->history);
    command_history_init(&editor->history, editor->document.current_state);
    editor_clear_selection(editor);
    editor->inspector_open = false;
    editor->inspector_kind = EDITOR_INSPECTOR_NONE;
    editor->modal = EDITOR_MODAL_NONE;
    editor->status = scene_document_is_repair_required(&editor->document)
                         ? EDITOR_STATUS_REPAIR_REQUIRED : EDITOR_STATUS_NONE;
    if (!editor_rebuild_asset_shortlists(editor)) {
        editor->status = EDITOR_STATUS_OUT_OF_MEMORY;
    }
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
    vertical_physics_init(&editor->vertical_physics);
    command_history_destroy(&editor->history);
    command_history_init(&editor->history, editor->document.current_state);
    editor_clear_selection(editor);
    editor->inspector_open = false;
    editor->inspector_kind = EDITOR_INSPECTOR_NONE;
    editor->modal = EDITOR_MODAL_NONE;
    editor->last_load_result = SCENE_LOAD_OK;
    editor->status = scene_document_is_repair_required(&editor->document)
                         ? EDITOR_STATUS_REPAIR_REQUIRED : EDITOR_STATUS_NONE;
    if (!editor_rebuild_asset_shortlists(editor)) {
        editor->status = EDITOR_STATUS_OUT_OF_MEMORY;
    }
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
    vertical_physics_init(&editor->vertical_physics);
    command_history_destroy(&editor->history);
    command_history_init(&editor->history, editor->document.current_state);
    editor_clear_selection(editor);
    editor->inspector_open = false;
    editor->modal = EDITOR_MODAL_NONE;
    editor->status = EDITOR_STATUS_NONE;
    if (!editor_rebuild_asset_shortlists(editor)) {
        editor->status = EDITOR_STATUS_OUT_OF_MEMORY;
    }
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
    if ((result == CMD_RESULT_OK || result == CMD_RESULT_NO_CHANGE) &&
        (!editor_shortlist_add(editor, material))) {
        editor->status = EDITOR_STATUS_OUT_OF_MEMORY;
    } else if (result == CMD_RESULT_OK || result == CMD_RESULT_NO_CHANGE) {
        editor_sort_shortlist(editor);
        if (!editor_rebuild_search_results(editor)) {
            editor->status = EDITOR_STATUS_OUT_OF_MEMORY;
        }
    }

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
    if ((result == CMD_RESULT_OK || result == CMD_RESULT_NO_CHANGE) &&
        !editor_shortlist_add(editor, material)) {
        editor->status = EDITOR_STATUS_OUT_OF_MEMORY;
    } else if (result == CMD_RESULT_OK || result == CMD_RESULT_NO_CHANGE) {
        editor_sort_shortlist(editor);
        if (!editor_rebuild_search_results(editor)) {
            editor->status = EDITOR_STATUS_OUT_OF_MEMORY;
        }
    }
    return result;
}

static CommandResult editor_execute_vertical_request(
    UnifiedEditorState *editor, const EditorMutationRequest *request
) {
    CommandExecutionContext context;
    CommandResult result;
    if (!editor || !request) return CMD_RESULT_INVALID_TARGET;
    context = editor_command_context(editor);
    result = command_history_execute_group_checked(
        &editor->history, &editor->document, request, 1U, &context);
    editor_map_command_result(editor, result);
    if (result == CMD_RESULT_OK) vertical_physics_init(&editor->vertical_physics);
    return result;
}

static CommandResult editor_execute_selection_group(
    UnifiedEditorState *editor, const EditorMutationRequest *requests,
    size_t request_count, bool refresh_runtime, bool clear_after
);

CommandResult unified_editor_step_selected_height(
    UnifiedEditorState *editor, int direction
) {
    EditorMutationRequest requests[EDITOR_SELECTION_SET_CAPACITY];
    size_t count;
    size_t i;
    CommandResult result;
    if (!editor) return CMD_RESULT_INVALID_TARGET;
    count = editor->selection_set.count > 0U ? editor->selection_set.count : 1U;
    if (count > EDITOR_SELECTION_SET_CAPACITY) return CMD_RESULT_INVALID_TARGET;
    for (i = 0U; i < count; i++) {
        SelectionTarget target = editor->selection_set.count > 0U
            ? editor->selection_set.members[i] : editor->selection;
        if (!editor_domain_make_height_step_request(
                &editor->document, target, direction, &requests[i]))
            return CMD_RESULT_INVALID_TARGET;
    }
    result = editor_execute_selection_group(editor, requests, count, false, false);
    if (result == CMD_RESULT_OK) vertical_physics_init(&editor->vertical_physics);
    return result;
}

CommandResult unified_editor_toggle_selected_surface_presence(UnifiedEditorState *editor) {
    EditorMutationRequest requests[EDITOR_SELECTION_SET_CAPACITY];
    size_t count;
    size_t i;
    CommandResult result;
    if (!editor) return CMD_RESULT_INVALID_TARGET;
    count = editor->selection_set.count > 0U ? editor->selection_set.count : 1U;
    if (count > EDITOR_SELECTION_SET_CAPACITY) return CMD_RESULT_INVALID_TARGET;
    for (i = 0U; i < count; i++) {
        SelectionTarget target = editor->selection_set.count > 0U
            ? editor->selection_set.members[i] : editor->selection;
        if (!editor_domain_make_surface_presence_request(
                &editor->document, target, &requests[i]))
            return CMD_RESULT_INVALID_TARGET;
    }
    result = editor_execute_selection_group(editor, requests, count, false, false);
    if (result == CMD_RESULT_OK) vertical_physics_init(&editor->vertical_physics);
    return result;
}

CommandResult unified_editor_step_selected_gravity_direction(
    UnifiedEditorState *editor, int direction
) {
    EditorMutationRequest request;
    if (!editor || !editor_domain_make_gravity_direction_step_request(
            &editor->document, editor->selection, direction, &request))
        return CMD_RESULT_INVALID_TARGET;
    return editor_execute_vertical_request(editor, &request);
}

CommandResult unified_editor_step_selected_gravity_scale(
    UnifiedEditorState *editor, int direction
) {
    EditorMutationRequest request;
    if (!editor || !editor_domain_make_gravity_scale_step_request(
            &editor->document, editor->selection, direction, &request))
        return CMD_RESULT_INVALID_TARGET;
    return editor_execute_vertical_request(editor, &request);
}

CommandResult unified_editor_step_movement_parameter(
    UnifiedEditorState *editor, EditorMovementField field, int direction
) {
    EditorMutationRequest request;
    if (!editor || !editor_domain_make_movement_step_request(
            &editor->document, field, direction, &request))
        return CMD_RESULT_INVALID_TARGET;
    return editor_execute_vertical_request(editor, &request);
}

static CommandResult editor_execute_optical_request(
    UnifiedEditorState *editor, const EditorMutationRequest *request
) {
    CommandResult result;
    if (!editor || !editor->active || !request) return CMD_RESULT_INVALID_TARGET;
    result = command_history_execute_group(
        &editor->history, &editor->document, request, 1U);
    editor_map_command_result(editor, result);
    return result;
}

CommandResult unified_editor_step_selected_optical(
    UnifiedEditorState *editor, int direction
) {
    EditorMutationRequest request;
    if (!editor || !editor_domain_make_optical_step_request(
            &editor->document, editor->selection, editor->optical_scope,
            editor->optical_field, direction, &request))
        return CMD_RESULT_INVALID_TARGET;
    return editor_execute_optical_request(editor, &request);
}

CommandResult unified_editor_toggle_selected_optical_inherit(
    UnifiedEditorState *editor
) {
    EditorMutationRequest request;
    if (!editor || !editor_domain_make_optical_inherit_toggle_request(
            &editor->document, editor->selection, editor->optical_scope,
            editor->optical_field, &request)) return CMD_RESULT_INVALID_TARGET;
    return editor_execute_optical_request(editor, &request);
}

CommandResult unified_editor_step_selected_transparency(
    UnifiedEditorState *editor, int direction
) {
    EditorMutationRequest request;
    if (!editor || !editor_domain_make_transparency_step_request(
            &editor->document, editor->selection, editor->optical_scope,
            direction, &request)) return CMD_RESULT_INVALID_TARGET;
    return editor_execute_optical_request(editor, &request);
}

CommandResult unified_editor_clear_selected_transparency_override(
    UnifiedEditorState *editor
) {
    EditorMutationRequest request;
    if (!editor || !editor_domain_make_transparency_inherit_request(
            &editor->document, editor->selection, editor->optical_scope,
            &request)) return CMD_RESULT_INVALID_TARGET;
    return editor_execute_optical_request(editor, &request);
}

void unified_editor_toggle_optical_scope(UnifiedEditorState *editor) {
    if (!editor) return;
    editor->optical_scope = editor->optical_scope == EDITOR_OPTICAL_SCOPE_CELL
        ? EDITOR_OPTICAL_SCOPE_MATERIAL : EDITOR_OPTICAL_SCOPE_CELL;
}

static CommandResult editor_execute_selection_group(
    UnifiedEditorState *editor, const EditorMutationRequest *requests,
    size_t request_count, bool refresh_runtime, bool clear_after
) {
    CommandExecutionContext context;
    CommandResult result;
    size_t old_count;
    size_t old_cursor;
    DocumentStateId old_next;
    if (!editor || request_count == 0U) return CMD_RESULT_INVALID_TARGET;
    old_count = editor->history.count;
    old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    context = editor_command_context(editor);
    result = command_history_execute_group_checked(
        &editor->history, &editor->document, requests, request_count, &context);
    editor_map_command_result(editor, result);
    if (refresh_runtime && result == CMD_RESULT_OK &&
        !editor_command_commit_runtime(editor, result, old_count, old_cursor, old_next))
        return CMD_RESULT_OUT_OF_MEMORY;
    if (result == CMD_RESULT_OK && clear_after) {
        editor_clear_selection(editor);
        editor->inspector_open = false;
        editor->inspector_kind = EDITOR_INSPECTOR_NONE;
    }
    return result;
}

CommandResult unified_editor_apply_material_to_selection(
    UnifiedEditorState *editor, MaterialId material
) {
    EditorMutationRequest requests[EDITOR_SELECTION_SET_CAPACITY];
    size_t i;
    if (!editor || !editor->active || editor->selection_set.count == 0U ||
        !material_id_is_loaded(editor->assets, material))
        return CMD_RESULT_INVALID_TARGET;
    for (i = 0U; i < editor->selection_set.count; i++)
        if (!editor_domain_make_surface_material_request(
                editor->selection_set.members[i], material, &requests[i]))
            return CMD_RESULT_INVALID_TARGET;
    return editor_execute_selection_group(
        editor, requests, editor->selection_set.count, false, false);
}

CommandResult unified_editor_apply_construction_to_selection(
    UnifiedEditorState *editor
) {
    EditorMutationRequest requests[EDITOR_SELECTION_SET_CAPACITY];
    size_t i;
    if (!editor || !editor->active || editor->selection_set.count == 0U)
        return CMD_RESULT_INVALID_TARGET;
    for (i = 0U; i < editor->selection_set.count; i++) {
        SelectionTarget target = editor->selection_set.members[i];
        if (!editor_domain_make_construction_request(target, &requests[i]))
            return CMD_RESULT_INVALID_TARGET;
        if (target.type == SELECTION_WALL_FACE &&
            (target.value.wall_face.map_x == editor->document.map.width - 1 ||
             target.value.wall_face.map_y == editor->document.map.height - 1)) {
            editor_map_command_result(editor, CMD_RESULT_RESIZE_BLOCKED);
            return CMD_RESULT_RESIZE_BLOCKED;
        }
    }
    return editor_execute_selection_group(
        editor, requests, editor->selection_set.count, true, true);
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
            editor, result, old_count, old_cursor, old_next)) {
        return CMD_RESULT_OUT_OF_MEMORY;
    }
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
            editor, result, old_count, old_cursor, old_next)) {
        return CMD_RESULT_OUT_OF_MEMORY;
    }
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

static bool editor_compute_light_placement_cell(
    const UnifiedEditorState *editor,
    int *out_map_x,
    int *out_map_y
) {
    const EditorHit *hover;
    const Map *map;
    if (!editor || !out_map_x || !out_map_y) return false;
    hover = &editor->hover;
    if (!hover->valid) return false;
    map = scene_document_get_map(&editor->document);
    if (!map) return false;
    if (hover->target.type == SELECTION_FLOOR ||
        hover->target.type == SELECTION_CEILING) {
        *out_map_x = hover->target.value.horizontal.map_x;
        *out_map_y = hover->target.value.horizontal.map_y;
        return map_in_bounds(map, *out_map_x, *out_map_y);
    }
    if (hover->target.type == SELECTION_WALL_FACE) {
        const WallFaceRef *wf = &hover->target.value.wall_face;
        int x = wf->map_x;
        int y = wf->map_y;
        switch (wf->face) {
            case WALL_FACE_NORTH: y--; break;
            case WALL_FACE_SOUTH: y++; break;
            case WALL_FACE_EAST:  x++; break;
            case WALL_FACE_WEST:  x--; break;
        }
        if (map_in_bounds(map, x, y)) {
            *out_map_x = x;
            *out_map_y = y;
            return true;
        }
        return false;
    }
    return false;
}

CommandResult unified_editor_place_light(UnifiedEditorState *editor) {
    int map_x;
    int map_y;
    SceneLight prototype;
    SceneInstanceId new_id = SCENE_INSTANCE_ID_INVALID;
    CommandResult result;
    size_t old_count;
    size_t old_cursor;
    DocumentStateId old_next;
    SceneInstanceId old_next_instance;
    if (!editor || !editor->active) return CMD_RESULT_INVALID_TARGET;
    if (!editor_compute_light_placement_cell(editor, &map_x, &map_y)) {
        editor->status = EDITOR_STATUS_INVALID_SELECTION;
        editor->last_command_result = CMD_RESULT_INVALID_TARGET;
        return CMD_RESULT_INVALID_TARGET;
    }
    memset(&prototype, 0, sizeof(prototype));
    prototype.x = (double)map_x + 0.5;
    prototype.y = (double)map_y + 0.5;
    prototype.red = 255;
    prototype.green = 255;
    prototype.blue = 255;
    prototype.alpha = 255;
    prototype.intensity = 1.0;
    prototype.radius = 5.0;
    scene_light_set_point_defaults(&prototype);
    prototype.falloff = config_get()->light_falloff_default;
    old_count = editor->history.count;
    old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    old_next_instance = editor->document.next_instance_id;
    result = command_history_insert_light(
        &editor->history, &editor->document, &prototype, &new_id);
    editor_map_command_result(editor, result);
    if (result == CMD_RESULT_OK && !editor_command_commit_runtime(
            editor, result, old_count, old_cursor, old_next)) {
        editor->document.next_instance_id = old_next_instance;
        return CMD_RESULT_OUT_OF_MEMORY;
    }
    if (result == CMD_RESULT_OK) {
        editor->selection.type = SELECTION_LIGHT;
        editor->selection.value.light.id = new_id;
        editor_reset_selection_set(editor);
        editor->inspector_open = true;
        editor->inspector_kind = EDITOR_INSPECTOR_LIGHT;
        editor->light_field = EDITOR_LIGHT_FIELD_X;
        editor_cancel_light_value_edit(editor);
    }
    return result;
}

CommandResult unified_editor_remove_light(
    UnifiedEditorState *editor,
    SceneInstanceId id
) {
    CommandResult result;
    size_t old_count;
    size_t old_cursor;
    DocumentStateId old_next;
    if (!editor || !editor->active) return CMD_RESULT_INVALID_TARGET;
    old_count = editor->history.count;
    old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    result = command_history_remove_light(
        &editor->history, &editor->document, id);
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

static bool editor_target_decal_surface(
    const UnifiedEditorState *editor, SelectionTarget selection,
    SceneDecalSurface *out_surface,
    int *out_map_x,
    int *out_map_y,
    int *out_side,
    double *out_rotation
) {
    if (!editor || !out_surface || !out_map_x || !out_map_y || !out_side ||
        !out_rotation) return false;
    if (selection.type == SELECTION_FLOOR ||
        selection.type == SELECTION_CEILING) {
        *out_surface = selection.type == SELECTION_FLOOR
            ? SCENE_DECAL_SURFACE_FLOOR : SCENE_DECAL_SURFACE_CEILING;
        *out_map_x = selection.value.horizontal.map_x;
        *out_map_y = selection.value.horizontal.map_y;
        *out_side = 0;
        *out_rotation = 0.0;
        return map_in_bounds(&editor->document.map, *out_map_x, *out_map_y);
    }
    if (selection.type == SELECTION_WALL_FACE) {
        const WallFaceRef *face = &selection.value.wall_face;
        *out_surface = SCENE_DECAL_SURFACE_WALL;
        *out_map_x = face->map_x;
        *out_map_y = face->map_y;
        if (face->face == WALL_FACE_EAST) {
            *out_side = 0; *out_rotation = 0.0;
        } else if (face->face == WALL_FACE_WEST) {
            *out_map_x -= 1; *out_side = 0; *out_rotation = PI;
        } else if (face->face == WALL_FACE_SOUTH) {
            *out_side = 1; *out_rotation = PI * 0.5;
        } else {
            *out_map_y -= 1; *out_side = 1; *out_rotation = -PI * 0.5;
        }
        return map_in_bounds(&editor->document.map, *out_map_x, *out_map_y);
    }
    return false;
}

static bool editor_selected_decal_surface(
    const UnifiedEditorState *editor, SceneDecalSurface *out_surface,
    int *out_map_x, int *out_map_y, int *out_side, double *out_rotation
) {
    if (!editor) return false;
    return editor_target_decal_surface(
        editor, editor->selection, out_surface, out_map_x, out_map_y,
        out_side, out_rotation);
}

static bool editor_decal_matches_surface_selection(
    const UnifiedEditorState *editor, const SceneDecalInstance *decal
) {
    SceneDecalSurface surface;
    int map_x;
    int map_y;
    int side;
    double rotation;
    if (!decal || !editor_selected_decal_surface(
            editor, &surface, &map_x, &map_y, &side, &rotation)) return false;
    (void)rotation;
    if (decal->surface != surface) return false;
    if (surface == SCENE_DECAL_SURFACE_WALL)
        return decal->map_x == map_x && decal->map_y == map_y &&
            decal->side == side && fabs(decal->rotation - rotation) < 0.000001;
    return (int)floor(decal->x) == map_x && (int)floor(decal->y) == map_y;
}

static size_t editor_surface_decal_count(const UnifiedEditorState *editor) {
    size_t count = 0U;
    if (!editor) return 0U;
    for (size_t i = 0U; i < editor->document.decal_count; i++)
        if (editor_decal_matches_surface_selection(editor, &editor->document.decals[i]))
            count++;
    return count;
}

static const SceneDecalInstance *editor_surface_decal_at(
    const UnifiedEditorState *editor, size_t match_index
) {
    if (!editor) return NULL;
    for (size_t i = 0U; i < editor->document.decal_count; i++) {
        if (!editor_decal_matches_surface_selection(editor, &editor->document.decals[i]))
            continue;
        if (match_index == 0U) return &editor->document.decals[i];
        match_index--;
    }
    return NULL;
}

static bool editor_create_empty_decal_pattern(UnifiedEditorState *editor) {
    DecalDocument document;
    AssetRefreshResult refresh;
    char directory[1024];
    uint16_t id;
    if (!editor || !editor->asset_root ||
        snprintf(directory, sizeof(directory), "%s/decals", editor->asset_root) >=
            (int)sizeof(directory)) return false;
    decal_document_init(&document);
    if (decal_document_create(
            &document, editor->assets, editor->decal_create_cols,
            editor->decal_create_rows) != DECAL_DOCUMENT_OK) {
        decal_document_destroy(&document);
        editor->status = EDITOR_STATUS_INVALID_DECAL;
        return false;
    }
    id = document.value.id;
    refresh = asset_refresh_save_decal_as(
        &document, editor->assets, &editor->document, directory,
        editor->asset_root, &editor->last_scene_diagnostic);
    decal_document_destroy(&document);
    if (refresh != ASSET_REFRESH_OK) {
        editor->status = refresh == ASSET_REFRESH_OUT_OF_MEMORY
            ? EDITOR_STATUS_OUT_OF_MEMORY : EDITOR_STATUS_SAVE_FAILED;
        return false;
    }
    if (!editor_rebuild_asset_shortlists(editor)) {
        editor->status = EDITOR_STATUS_OUT_OF_MEMORY;
        return false;
    }
    return (editor->selection_set.count > 1U
        ? unified_editor_place_decal_on_selection(editor, id, 1.0, 1.0)
        : unified_editor_place_decal(editor, id, 1.0, 1.0)) == CMD_RESULT_OK;
}

CommandResult unified_editor_place_decal(
    UnifiedEditorState *editor, uint16_t asset_id, double width, double height
) {
    SceneDecalInstance prototype;
    SceneDecalSurface surface;
    SceneInstanceId new_id = SCENE_INSTANCE_ID_INVALID;
    SceneInstanceId old_next_instance;
    int map_x;
    int map_y;
    int side;
    double rotation;
    CommandResult result;
    size_t old_count;
    size_t old_cursor;
    DocumentStateId old_next;
    if (!editor || !editor->active ||
        !asset_registry_get_decal_pattern(editor->assets, asset_id) ||
        !isfinite(width) || !isfinite(height) || width <= 0.0 || height <= 0.0 ||
        !editor_selected_decal_surface(
            editor, &surface, &map_x, &map_y, &side, &rotation)) {
        if (editor) editor_map_command_result(editor, CMD_RESULT_INVALID_TARGET);
        return CMD_RESULT_INVALID_TARGET;
    }
    memset(&prototype, 0, sizeof(prototype));
    prototype.asset.kind = SCENE_ASSET_KIND_DECAL_PATTERN;
    prototype.asset.id = asset_id;
    prototype.surface = surface;
    prototype.width = width;
    prototype.height = height;
    prototype.depth = 0.1;
    prototype.rotation = rotation;
    if (surface == SCENE_DECAL_SURFACE_WALL) {
        prototype.map_x = map_x;
        prototype.map_y = map_y;
        prototype.side = side;
        prototype.u = width < 1.0 ? (1.0 - width) * 0.5 : 0.0;
        prototype.v = height < 1.0 ? (1.0 - height) * 0.5 : 0.0;
    } else {
        prototype.x = (double)map_x + 0.5;
        prototype.y = (double)map_y + 0.5;
        prototype.z = surface == SCENE_DECAL_SURFACE_CEILING ? 1.0 : 0.0;
    }
    old_count = editor->history.count;
    old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    old_next_instance = editor->document.next_instance_id;
    result = command_history_insert_decal(
        &editor->history, &editor->document, &prototype, &new_id);
    editor_map_command_result(editor, result);
    if (result == CMD_RESULT_OK && !editor_command_commit_runtime(
            editor, result, old_count, old_cursor, old_next)) {
        editor->document.next_instance_id = old_next_instance;
        return CMD_RESULT_OUT_OF_MEMORY;
    }
    if (result == CMD_RESULT_OK) {
        editor->selection.type = SELECTION_DECAL;
        editor->selection.value.decal.id = new_id;
        editor_reset_selection_set(editor);
        editor->inspector_open = true;
        editor->inspector_kind = EDITOR_INSPECTOR_DECAL;
        editor->decal_field = EDITOR_DECAL_FIELD_POSITION_U;
        editor->decal_menu_open = false;
        editor_cancel_light_value_edit(editor);
        (void)editor_rebuild_decal_shortlist(editor);
    }
    return result;
}

CommandResult unified_editor_place_decal_on_selection(
    UnifiedEditorState *editor, uint16_t asset_id, double width, double height
) {
    SceneDecalInstance prototypes[EDITOR_SELECTION_SET_CAPACITY];
    SceneInstanceId old_next_instance;
    size_t old_count;
    size_t old_cursor;
    DocumentStateId old_next;
    CommandResult result;
    size_t i;
    if (!editor || !editor->active || editor->selection_set.count == 0U ||
        !asset_registry_get_decal_pattern(editor->assets, asset_id) ||
        !isfinite(width) || !isfinite(height) || width <= 0.0 || height <= 0.0)
        return CMD_RESULT_INVALID_TARGET;
    for (i = 0U; i < editor->selection_set.count; i++) {
        SceneDecalSurface surface;
        int map_x, map_y, side;
        double rotation;
        SceneDecalInstance *prototype = &prototypes[i];
        if (!editor_target_decal_surface(
                editor, editor->selection_set.members[i], &surface,
                &map_x, &map_y, &side, &rotation)) return CMD_RESULT_INVALID_TARGET;
        memset(prototype, 0, sizeof(*prototype));
        prototype->asset.kind = SCENE_ASSET_KIND_DECAL_PATTERN;
        prototype->asset.id = asset_id;
        prototype->surface = surface;
        prototype->width = width;
        prototype->height = height;
        prototype->depth = 0.1;
        prototype->rotation = rotation;
        if (surface == SCENE_DECAL_SURFACE_WALL) {
            prototype->map_x = map_x;
            prototype->map_y = map_y;
            prototype->side = side;
            prototype->u = width < 1.0 ? (1.0 - width) * 0.5 : 0.0;
            prototype->v = height < 1.0 ? (1.0 - height) * 0.5 : 0.0;
        } else {
            prototype->x = (double)map_x + 0.5;
            prototype->y = (double)map_y + 0.5;
            prototype->z = surface == SCENE_DECAL_SURFACE_CEILING ? 1.0 : 0.0;
        }
    }
    old_count = editor->history.count;
    old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    old_next_instance = editor->document.next_instance_id;
    result = command_history_insert_decals(
        &editor->history, &editor->document, prototypes, editor->selection_set.count);
    editor_map_command_result(editor, result);
    if (result == CMD_RESULT_OK && !editor_command_commit_runtime(
            editor, result, old_count, old_cursor, old_next)) {
        editor->document.next_instance_id = old_next_instance;
        return CMD_RESULT_OUT_OF_MEMORY;
    }
    if (result == CMD_RESULT_OK) {
        editor->decal_menu_open = false;
        (void)editor_rebuild_decal_shortlist(editor);
    }
    return result;
}

CommandResult unified_editor_remove_decal(
    UnifiedEditorState *editor, SceneInstanceId id
) {
    CommandResult result;
    size_t old_count;
    size_t old_cursor;
    DocumentStateId old_next;
    if (!editor || !editor->active) return CMD_RESULT_INVALID_TARGET;
    old_count = editor->history.count;
    old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    result = command_history_remove_decal(&editor->history, &editor->document, id);
    editor_map_command_result(editor, result);
    if (result == CMD_RESULT_OK && !editor_command_commit_runtime(
            editor, result, old_count, old_cursor, old_next))
        return CMD_RESULT_OUT_OF_MEMORY;
    if (result == CMD_RESULT_OK) {
        editor_clear_selection(editor);
        editor->inspector_open = false;
        editor->inspector_kind = EDITOR_INSPECTOR_NONE;
        (void)editor_rebuild_decal_shortlist(editor);
    }
    return result;
}

CommandResult unified_editor_step_decal_field(
    UnifiedEditorState *editor, EditorDecalField field, int direction
) {
    EditorMutationRequest request;
    CommandResult result;
    size_t old_count;
    size_t old_cursor;
    DocumentStateId old_next;
    if (!editor || !editor->active ||
        !editor_domain_make_decal_step_request(
            &editor->document, editor->selection, field, direction, &request)) {
        if (editor) editor_map_command_result(editor, CMD_RESULT_INVALID_TARGET);
        return CMD_RESULT_INVALID_TARGET;
    }
    old_count = editor->history.count;
    old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    result = command_history_execute_group(
        &editor->history, &editor->document, &request, 1U);
    editor_map_command_result(editor, result);
    editor_revalidate_selection(editor);
    if (result == CMD_RESULT_OK && !editor_command_commit_runtime(
            editor, result, old_count, old_cursor, old_next))
        return CMD_RESULT_OUT_OF_MEMORY;
    return result;
}

CommandResult unified_editor_set_decal_field_value(
    UnifiedEditorState *editor, EditorDecalField field, double value
) {
    EditorMutationRequest request;
    CommandResult result;
    size_t old_count;
    size_t old_cursor;
    DocumentStateId old_next;
    if (!editor || !editor->active ||
        !editor_domain_make_decal_value_request(
            &editor->document, editor->selection, field, value, &request)) {
        if (editor) {
            editor->status = EDITOR_STATUS_INVALID_NUMERIC_VALUE;
            editor->last_command_result = CMD_RESULT_INVALID_TARGET;
        }
        return CMD_RESULT_INVALID_TARGET;
    }
    old_count = editor->history.count;
    old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    result = command_history_execute_group(
        &editor->history, &editor->document, &request, 1U);
    editor_map_command_result(editor, result);
    editor_revalidate_selection(editor);
    if (result == CMD_RESULT_OK && !editor_command_commit_runtime(
            editor, result, old_count, old_cursor, old_next))
        return CMD_RESULT_OUT_OF_MEMORY;
    return result;
}

static void editor_rebuild_sprite_shortlist(UnifiedEditorState *editor) {
    uint16_t id;
    if (!editor || !editor->assets) return;
    editor->sprite_shortlist_count = 0U;
    for (id = 1U; id < SPRITE_ID_CAPACITY; id++) {
        if (sprite_id_is_loaded(editor->assets, (int)id)) {
            editor->sprite_shortlist[editor->sprite_shortlist_count++] = id;
        }
    }
}

static bool editor_sprite_directory(const UnifiedEditorState *editor,
                                    char *out, size_t out_size) {
    int written;
    if (!editor || !editor->asset_root || !out || out_size == 0U) return false;
    written = snprintf(out, out_size, "%s/sprites", editor->asset_root);
    return written >= 0 && (size_t)written < out_size;
}

static bool editor_ensure_sprite_directory(const UnifiedEditorState *editor,
                                           char *out, size_t out_size) {
    struct stat info;
    if (!editor_sprite_directory(editor, out, out_size)) return false;
    if (stat(out, &info) == 0) return S_ISDIR(info.st_mode);
    return errno == ENOENT && mkdir(out, 0755) == 0;
}

static bool editor_open_sprite_document(UnifiedEditorState *editor,
                                        uint16_t asset_id) {
    char directory[1024];
    SpriteDocumentResult result;
    if (!editor_sprite_directory(editor, directory, sizeof(directory))) return false;
    result = sprite_document_open_loaded(
        &editor->sprite_document, editor->assets, asset_id, directory);
    if (result != SPRITE_DOCUMENT_OK) {
        editor->status = result == SPRITE_DOCUMENT_OUT_OF_MEMORY
            ? EDITOR_STATUS_OUT_OF_MEMORY : EDITOR_STATUS_INVALID_SPRITE;
        return false;
    }
    editor->sprite_paint_x = 0U;
    editor->sprite_paint_y = 0U;
    editor->sprite_paint_material = 1U;
    while (editor->sprite_paint_material <= ASSET_ID_MAX &&
           !material_id_is_loaded(editor->assets,
                                  (int)editor->sprite_paint_material)) {
        editor->sprite_paint_material++;
    }
    if (editor->sprite_paint_material > ASSET_ID_MAX)
        editor->sprite_paint_material = 0U;
    return true;
}

static bool editor_save_sprite_document(UnifiedEditorState *editor) {
    char directory[1024];
    SpriteDocumentResult result;
    if (!editor_ensure_sprite_directory(editor, directory, sizeof(directory))) {
        editor->status = EDITOR_STATUS_SAVE_FAILED;
        return false;
    }
    result = sprite_document_save(
        &editor->sprite_document, editor->assets, directory);
    if (result == SPRITE_DOCUMENT_OK)
        result = sprite_document_commit_to_registry(
            &editor->sprite_document, editor->assets);
    if (result != SPRITE_DOCUMENT_OK || !editor_refresh_runtime(editor)) {
        editor->status = result == SPRITE_DOCUMENT_OUT_OF_MEMORY
            ? EDITOR_STATUS_OUT_OF_MEMORY : EDITOR_STATUS_SAVE_FAILED;
        return false;
    }
    editor_rebuild_sprite_shortlist(editor);
    editor->status = EDITOR_STATUS_SPRITE_PATTERN_SAVED;
    return true;
}

static bool editor_set_selected_sprite_asset(UnifiedEditorState *editor,
                                             uint16_t asset_id) {
    const SceneSpriteInstance *current;
    SceneSpriteInstance changed;
    size_t old_count;
    size_t old_cursor;
    DocumentStateId old_next;
    CommandResult result;
    if (!editor || editor->selection.type != SELECTION_SPRITE ||
        !sprite_id_is_loaded(editor->assets, (int)asset_id)) return false;
    current = scene_document_find_sprite(
        &editor->document, editor->selection.value.sprite.id);
    if (!current) return false;
    changed = *current;
    changed.asset.id = asset_id;
    old_count = editor->history.count;
    old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    result = command_history_set_sprite(
        &editor->history, &editor->document, changed.id, &changed);
    editor_map_command_result(editor, result);
    if (result == CMD_RESULT_NO_CHANGE) return true;
    if (result != CMD_RESULT_OK) return false;
    return editor_command_commit_runtime(
        editor, result, old_count, old_cursor, old_next);
}

static bool editor_create_and_place_sprite_canvas(UnifiedEditorState *editor) {
    char directory[1024];
    char path[1060];
    int map_x;
    int map_y;
    uint16_t asset_id;
    SpriteDocumentResult result;
    if (!editor || !editor_compute_light_placement_cell(editor, &map_x, &map_y)) {
        if (editor) editor->status = EDITOR_STATUS_INVALID_SELECTION;
        return false;
    }
    (void)map_x;
    (void)map_y;
    if (!editor_ensure_sprite_directory(
            editor, directory, sizeof(directory))) {
        editor->status = EDITOR_STATUS_SAVE_FAILED;
        return false;
    }
    result = sprite_document_create(
        &editor->sprite_document, editor->assets, 8U, 8U);
    if (result == SPRITE_DOCUMENT_OK)
        result = sprite_document_save(
            &editor->sprite_document, editor->assets, directory);
    if (result == SPRITE_DOCUMENT_OK)
        result = sprite_document_commit_to_registry(
            &editor->sprite_document, editor->assets);
    if (result != SPRITE_DOCUMENT_OK) {
        if (editor->sprite_document.path) {
            (void)unlink(editor->sprite_document.path);
        }
        sprite_document_destroy(&editor->sprite_document);
        editor->status = result == SPRITE_DOCUMENT_OUT_OF_MEMORY
            ? EDITOR_STATUS_OUT_OF_MEMORY : EDITOR_STATUS_SAVE_FAILED;
        return false;
    }
    asset_id = editor->sprite_document.id;
    editor_rebuild_sprite_shortlist(editor);
    if (unified_editor_place_sprite(editor, asset_id) != CMD_RESULT_OK) {
        free(editor->assets->sprites[asset_id].pattern);
        memset(&editor->assets->sprites[asset_id], 0,
               sizeof(editor->assets->sprites[asset_id]));
        if (snprintf(path, sizeof(path), "%s/%u.txt", directory,
                     (unsigned)asset_id) >= 0 && strlen(path) < sizeof(path)) {
            (void)unlink(path);
        }
        sprite_document_destroy(&editor->sprite_document);
        editor_rebuild_sprite_shortlist(editor);
        return false;
    }
    editor->sprite_field = EDITOR_SPRITE_FIELD_PATTERN;
    editor->sprite_menu_open = true;
    editor->sprite_menu_stage = EDITOR_SPRITE_MENU_ACTIONS;
    editor->sprite_menu_index = 2U;
    editor->status = EDITOR_STATUS_NONE;
    return true;
}

static void editor_cycle_sprite_paint_material(UnifiedEditorState *editor,
                                               int direction) {
    int candidate;
    int attempts;
    if (!editor || (direction != -1 && direction != 1)) return;
    candidate = editor->sprite_paint_material > 0
        ? editor->sprite_paint_material : 1;
    for (attempts = 0; attempts < ASSET_ID_MAX; attempts++) {
        candidate += direction;
        if (candidate < 1) candidate = ASSET_ID_MAX;
        if (candidate > ASSET_ID_MAX) candidate = 1;
        if (material_id_is_loaded(editor->assets, candidate)) {
            editor->sprite_paint_material = candidate;
            return;
        }
    }
}

static bool editor_begin_selected_sprite_pattern(UnifiedEditorState *editor) {
    const SceneSpriteInstance *sprite;
    if (!editor || editor->selection.type != SELECTION_SPRITE) return false;
    sprite = scene_document_find_sprite(
        &editor->document, editor->selection.value.sprite.id);
    if (!sprite || !editor_open_sprite_document(editor, sprite->asset.id)) return false;
    editor_rebuild_sprite_shortlist(editor);
    editor->sprite_menu_open = true;
    editor->sprite_menu_stage = EDITOR_SPRITE_MENU_ACTIONS;
    editor->sprite_menu_index = 0U;
    return true;
}

static bool editor_handle_sprite_menu_input(UnifiedEditorState *editor,
                                            const InputState *input) {
    if (!editor || !input || !editor->sprite_menu_open) return false;
    if (editor->sprite_menu_stage == EDITOR_SPRITE_MENU_PAINT) {
        if (input->editor_save_pressed) {
            (void)editor_save_sprite_document(editor);
        } else if (input->editor_previous_pressed && editor->sprite_paint_y > 0U) {
            editor->sprite_paint_y--;
        } else if (input->editor_next_pressed &&
                   editor->sprite_paint_y + 1U < editor->sprite_document.rows) {
            editor->sprite_paint_y++;
        } else if (input->editor_decrease_pressed && editor->sprite_paint_x > 0U) {
            editor->sprite_paint_x--;
        } else if (input->editor_increase_pressed &&
                   editor->sprite_paint_x + 1U < editor->sprite_document.cols) {
            editor->sprite_paint_x++;
        } else if (input->prev_glyph) {
            editor_cycle_sprite_paint_material(editor, -1);
        } else if (input->next_glyph) {
            editor_cycle_sprite_paint_material(editor, 1);
        } else if (input->editor_text_backspace_pressed) {
            (void)sprite_document_erase_cell(
                &editor->sprite_document, editor->sprite_paint_x,
                editor->sprite_paint_y);
        } else if (input->text_input_len > 0 &&
                   editor->sprite_paint_material > 0) {
            unsigned char glyph = (unsigned char)input->text_input[0];
            if (glyph >= 32U && glyph <= 126U) {
                PatternCell cell = {
                    (uint8_t)glyph, (uint16_t)editor->sprite_paint_material};
                (void)sprite_document_paint_cell(
                    &editor->sprite_document, editor->assets,
                    editor->sprite_paint_x, editor->sprite_paint_y, cell);
            }
        }
        return true;
    }
    if (input->editor_previous_pressed) {
        size_t count = editor->sprite_menu_stage == EDITOR_SPRITE_MENU_ACTIONS
            ? 3U : editor->sprite_shortlist_count;
        if (count > 0U) editor->sprite_menu_index = editor->sprite_menu_index == 0U
            ? count - 1U : editor->sprite_menu_index - 1U;
        return true;
    }
    if (input->editor_next_pressed) {
        size_t count = editor->sprite_menu_stage == EDITOR_SPRITE_MENU_ACTIONS
            ? 3U : editor->sprite_shortlist_count;
        if (count > 0U) editor->sprite_menu_index =
            (editor->sprite_menu_index + 1U) % count;
        return true;
    }
    if (!input->editor_confirm_pressed) return false;
    if (editor->sprite_menu_stage == EDITOR_SPRITE_MENU_LOAD) {
        if (editor->sprite_menu_index < editor->sprite_shortlist_count) {
            uint16_t id = editor->sprite_shortlist[editor->sprite_menu_index];
            if (editor_set_selected_sprite_asset(editor, id) &&
                editor_open_sprite_document(editor, id)) {
                editor->sprite_menu_stage = EDITOR_SPRITE_MENU_ACTIONS;
                editor->sprite_menu_index = 0U;
                editor->status = EDITOR_STATUS_SPRITE_PATTERN_LOADED;
            }
        }
        return true;
    }
    if (editor->sprite_menu_index == 0U) {
        editor_rebuild_sprite_shortlist(editor);
        editor->sprite_menu_stage = EDITOR_SPRITE_MENU_LOAD;
        editor->sprite_menu_index = 0U;
    } else if (editor->sprite_menu_index == 1U) {
        (void)editor_save_sprite_document(editor);
    } else {
        editor->sprite_menu_stage = EDITOR_SPRITE_MENU_PAINT;
    }
    return true;
}

CommandResult unified_editor_place_sprite(
    UnifiedEditorState *editor, uint16_t asset_id
) {
    int map_x;
    int map_y;
    SceneSpriteInstance prototype;
    SceneInstanceId new_id = SCENE_INSTANCE_ID_INVALID;
    CommandResult result;
    size_t old_count;
    size_t old_cursor;
    DocumentStateId old_next;
    SceneInstanceId old_next_instance;
    if (!editor || !editor->active) return CMD_RESULT_INVALID_TARGET;
    if (asset_id == 0U || !sprite_id_is_loaded(editor->assets, (int)asset_id)) {
        editor->status = EDITOR_STATUS_INVALID_SELECTION;
        editor->last_command_result = CMD_RESULT_INVALID_TARGET;
        return CMD_RESULT_INVALID_TARGET;
    }
    if (!editor_compute_light_placement_cell(editor, &map_x, &map_y)) {
        editor->status = EDITOR_STATUS_INVALID_SELECTION;
        editor->last_command_result = CMD_RESULT_INVALID_TARGET;
        return CMD_RESULT_INVALID_TARGET;
    }
    memset(&prototype, 0, sizeof(prototype));
    prototype.asset.kind = SCENE_ASSET_KIND_SPRITE_PATTERN;
    prototype.asset.id = asset_id;
    prototype.x = (double)map_x + 0.5;
    prototype.y = (double)map_y + 0.5;
    old_count = editor->history.count;
    old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    old_next_instance = editor->document.next_instance_id;
    result = command_history_insert_sprite(
        &editor->history, &editor->document, &prototype, &new_id);
    editor_map_command_result(editor, result);
    if (result == CMD_RESULT_OK && !editor_command_commit_runtime(
            editor, result, old_count, old_cursor, old_next)) {
        editor->document.next_instance_id = old_next_instance;
        return CMD_RESULT_OUT_OF_MEMORY;
    }
    if (result == CMD_RESULT_OK) {
        editor->selection.type = SELECTION_SPRITE;
        editor->selection.value.sprite.id = new_id;
        editor_reset_selection_set(editor);
        editor->inspector_open = true;
        editor->inspector_kind = EDITOR_INSPECTOR_SPRITE;
        editor->sprite_field = EDITOR_SPRITE_FIELD_X;
        editor_cancel_light_value_edit(editor);
    }
    return result;
}

CommandResult unified_editor_remove_sprite(
    UnifiedEditorState *editor, SceneInstanceId id
) {
    CommandResult result;
    size_t old_count;
    size_t old_cursor;
    DocumentStateId old_next;
    if (!editor || !editor->active) return CMD_RESULT_INVALID_TARGET;
    old_count = editor->history.count;
    old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    result = command_history_remove_sprite(&editor->history, &editor->document, id);
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

CommandResult unified_editor_step_sprite_field(
    UnifiedEditorState *editor, EditorSpriteField field, int direction
) {
    EditorMutationRequest request;
    CommandResult result;
    size_t old_count;
    size_t old_cursor;
    DocumentStateId old_next;
    if (!editor || !editor->active ||
        !editor_domain_make_sprite_step_request(
            &editor->document, editor->selection, field, direction, &request)) {
        if (editor) {
            editor->status = EDITOR_STATUS_INVALID_SELECTION;
            editor->last_command_result = CMD_RESULT_INVALID_TARGET;
        }
        return CMD_RESULT_INVALID_TARGET;
    }
    old_count = editor->history.count;
    old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    result = command_history_execute_group(
        &editor->history, &editor->document, &request, 1U);
    editor_map_command_result(editor, result);
    editor_revalidate_selection(editor);
    if (result == CMD_RESULT_OK && !editor_command_commit_runtime(
            editor, result, old_count, old_cursor, old_next))
        return CMD_RESULT_OUT_OF_MEMORY;
    return result;
}

CommandResult unified_editor_set_sprite_field_value(
    UnifiedEditorState *editor, EditorSpriteField field, double value
) {
    EditorMutationRequest request;
    CommandResult result;
    size_t old_count;
    size_t old_cursor;
    DocumentStateId old_next;
    if (!editor || !editor->active ||
        !editor_domain_make_sprite_value_request(
            &editor->document, editor->selection, field, value, &request)) {
        if (editor) {
            editor->status = EDITOR_STATUS_INVALID_NUMERIC_VALUE;
            editor->last_command_result = CMD_RESULT_INVALID_TARGET;
        }
        return CMD_RESULT_INVALID_TARGET;
    }
    old_count = editor->history.count;
    old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    result = command_history_execute_group(
        &editor->history, &editor->document, &request, 1U);
    editor_map_command_result(editor, result);
    editor_revalidate_selection(editor);
    if (result == CMD_RESULT_OK && !editor_command_commit_runtime(
            editor, result, old_count, old_cursor, old_next))
        return CMD_RESULT_OUT_OF_MEMORY;
    return result;
}

CommandResult unified_editor_place_trigger(UnifiedEditorState *editor) {
    int map_x, map_y;
    SceneTrigger prototype = {0};
    SceneInstanceId id = 0U, old_next_instance;
    size_t old_count, old_cursor;
    DocumentStateId old_next;
    CommandResult result;
    if (!editor || !editor->active ||
        !editor_compute_light_placement_cell(editor, &map_x, &map_y)) {
        if (editor) editor->status = EDITOR_STATUS_INVALID_SELECTION;
        return CMD_RESULT_INVALID_TARGET;
    }
    prototype.min_x = map_x; prototype.min_y = map_y;
    prototype.max_x = map_x + 1.0; prototype.max_y = map_y + 1.0;
    prototype.condition = SCENE_TRIGGER_CONDITION_ENTER_REGION;
    prototype.action = SCENE_TRIGGER_ACTION_SET_FLAG;
    prototype.flag_id = 1U; prototype.flag_value = true;
    old_count = editor->history.count; old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    old_next_instance = editor->document.next_instance_id;
    result = command_history_insert_trigger(
        &editor->history, &editor->document, &prototype, &id);
    editor_map_command_result(editor, result);
    if (result == CMD_RESULT_OK && !editor_command_commit_runtime(
            editor, result, old_count, old_cursor, old_next)) {
        editor->document.next_instance_id = old_next_instance;
        return CMD_RESULT_OUT_OF_MEMORY;
    }
    if (result == CMD_RESULT_OK) {
        editor->selection.type = SELECTION_TRIGGER;
        editor->selection.value.trigger.id = id;
        editor_reset_selection_set(editor);
        editor->inspector_open = true;
        editor->inspector_kind = EDITOR_INSPECTOR_TRIGGER;
        editor->trigger_field = EDITOR_TRIGGER_FIELD_MIN_X;
    }
    return result;
}

CommandResult unified_editor_remove_trigger(
    UnifiedEditorState *editor, SceneInstanceId id
) {
    CommandResult result;
    size_t old_count, old_cursor;
    DocumentStateId old_next;
    if (!editor || !editor->active) return CMD_RESULT_INVALID_TARGET;
    old_count = editor->history.count; old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    result = command_history_remove_trigger(&editor->history, &editor->document, id);
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

CommandResult unified_editor_step_trigger_field(
    UnifiedEditorState *editor, EditorTriggerField field, int direction
) {
    EditorMutationRequest request;
    CommandResult result;
    size_t old_count, old_cursor;
    DocumentStateId old_next;
    if (!editor || !editor->active || !editor_domain_make_trigger_step_request(
            &editor->document, editor->selection, field, direction, &request)) {
        if (editor) editor->status = EDITOR_STATUS_INVALID_TRIGGER;
        return CMD_RESULT_INVALID_TARGET;
    }
    old_count = editor->history.count; old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    result = command_history_execute_group(
        &editor->history, &editor->document, &request, 1U);
    editor_map_command_result(editor, result);
    if (result == CMD_RESULT_OK && !editor_command_commit_runtime(
            editor, result, old_count, old_cursor, old_next))
        return CMD_RESULT_OUT_OF_MEMORY;
    return result;
}

static CommandResult editor_confirm_trigger_payload(UnifiedEditorState *editor) {
    EditorMutationRequest request;
    CommandResult result;
    size_t old_count, old_cursor;
    DocumentStateId old_next;
    if (!editor || !editor_domain_make_trigger_confirm_request(
            &editor->document, editor->selection, editor->trigger_field, &request))
        return CMD_RESULT_INVALID_TARGET;
    old_count = editor->history.count; old_cursor = editor->history.cursor;
    old_next = editor->history.next_state_id;
    result = command_history_execute_group(
        &editor->history, &editor->document, &request, 1U);
    editor_map_command_result(editor, result);
    if (result == CMD_RESULT_OK && !editor_command_commit_runtime(
            editor, result, old_count, old_cursor, old_next))
        return CMD_RESULT_OUT_OF_MEMORY;
    return result;
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
    if (result == CMD_RESULT_OK) vertical_physics_init(&editor->vertical_physics);
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
    if (result == CMD_RESULT_OK) vertical_physics_init(&editor->vertical_physics);
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
        editor->material_collision_id = 0U;
        editor_mark_keyboard(consumed);
        return;
    }

    if (editor->movement_menu_open) {
        editor->movement_menu_open = false;
        editor_mark_keyboard(consumed);
        return;
    }

    if (editor->optical_menu_open) {
        if (editor->transparency_menu_open) {
            editor->transparency_menu_open = false;
            editor_mark_keyboard(consumed);
            return;
        }
        editor->optical_menu_open = false;
        editor_mark_keyboard(consumed);
        return;
    }

    if (editor->decal_menu_open) {
        if (editor->decal_menu_stage != EDITOR_DECAL_MENU_LIST) {
            editor->decal_menu_stage = EDITOR_DECAL_MENU_LIST;
            editor->decal_menu_index = 0U;
        } else {
            editor->decal_menu_open = false;
        }
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
    editor_close_sprite_menu(editor);

    if (!editor->hover.valid ||
        !editor_selection_is_valid(editor, editor->hover.target)) {
        editor->status = EDITOR_STATUS_INVALID_SELECTION;
        return;
    }

    editor->selection = editor->hover.target;
    editor_reset_selection_set(editor);
    editor->inspector_kind = editor_domain_inspector_kind(editor->selection);
    editor->inspector_open = editor->inspector_kind != EDITOR_INSPECTOR_NONE;
    editor->light_field = EDITOR_LIGHT_FIELD_X;
    editor->surface_field = EDITOR_SURFACE_FIELD_MATERIAL;
    editor->sprite_field = EDITOR_SPRITE_FIELD_X;
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

static bool editor_extension_target_visible(
    const UnifiedEditorState *editor, const Camera *camera, SelectionTarget target
) {
    double target_x;
    double target_y;
    double dx;
    double dy;
    double distance;
    double angle;
    RayResult wall;
    if (!editor || !camera) return false;
    if (target.type == SELECTION_WALL_FACE) {
        target_x = target.value.wall_face.map_x + 0.5;
        target_y = target.value.wall_face.map_y + 0.5;
    } else if (target.type == SELECTION_FLOOR || target.type == SELECTION_CEILING) {
        target_x = target.value.horizontal.map_x + 0.5;
        target_y = target.value.horizontal.map_y + 0.5;
    } else return false;
    dx = target_x - camera->transform.pos.x;
    dy = target_y - camera->transform.pos.y;
    distance = sqrt(dx * dx + dy * dy);
    if (!isfinite(distance) || distance <= 0.001) return true;
    angle = atan2(dy, dx);
    wall = raycast_fire((Map *)&editor->document.map, (Camera *)camera,
                        angle, distance + 1.0);
    if (target.type == SELECTION_WALL_FACE) {
        double ray_x = cos(angle);
        double ray_y = sin(angle);
        return wall.hit && wall.map_x == target.value.wall_face.map_x &&
            wall.map_y == target.value.wall_face.map_y &&
            editor_calculate_wall_face(wall.side, ray_x, ray_y) ==
                target.value.wall_face.face;
    }
    return !wall.hit || wall.distance >= distance - 0.001;
}

bool unified_editor_extend_selection(
    UnifiedEditorState *editor, const Camera *camera, int delta_x, int delta_y
) {
    const SelectionTarget *primary;
    SelectionTarget target;
    if (!editor || !camera || (delta_x == 0 && delta_y == 0) ||
        (delta_x != 0 && delta_y != 0)) return false;
    primary = editor_selection_set_primary(&editor->selection_set);
    if (!primary) {
        editor_reset_selection_set(editor);
        primary = editor_selection_set_primary(&editor->selection_set);
    }
    if (!primary) return false;
    target = *primary;
    if (target.type == SELECTION_WALL_FACE) {
        WallFace face = target.value.wall_face.face;
        if ((face == WALL_FACE_NORTH || face == WALL_FACE_SOUTH) && delta_y != 0)
            return false;
        if ((face == WALL_FACE_EAST || face == WALL_FACE_WEST) && delta_x != 0)
            return false;
        target.value.wall_face.map_x += delta_x;
        target.value.wall_face.map_y += delta_y;
    } else if (target.type == SELECTION_FLOOR || target.type == SELECTION_CEILING) {
        target.value.horizontal.map_x += delta_x;
        target.value.horizontal.map_y += delta_y;
    } else return false;
    if (!editor_selection_is_valid_for_map(target, &editor->document.map) ||
        !editor_extension_target_visible(editor, camera, target)) {
        editor->status = EDITOR_STATUS_INVALID_SELECTION;
        return false;
    }
    if (!editor_selection_set_add(&editor->selection_set, target)) {
        editor->status = EDITOR_STATUS_SELECTION_LIMIT;
        return false;
    }
    editor->selection = target;
    editor->inspector_kind = editor_domain_inspector_kind(target);
    editor->inspector_open = true;
    if (editor->surface_field == EDITOR_SURFACE_FIELD_AMBIENT)
        editor->surface_field = EDITOR_SURFACE_FIELD_MATERIAL;
    editor->material_picker_open = false;
    editor->decal_menu_open = false;
    editor->movement_menu_open = false;
    editor->status = EDITOR_STATUS_NONE;
    editor_rebuild_picker_for_selection(editor);
    return true;
}

static void editor_handle_picker_prev(UnifiedEditorState *editor) {
    size_t count = editor->material_search_result_count;
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
    size_t count = editor->material_search_result_count;
    if (count == 0) {
        return;
    }
    editor->material_picker_index++;
    if (editor->material_picker_index >= count) {
        editor->material_picker_index = 0;
    }
    editor_sync_highlighted_from_picker(editor);
}

static void editor_append_material_search(UnifiedEditorState *editor,
                                          const char *text) {
    const char *cursor;
    if (!editor || !text) return;
    for (cursor = text; *cursor; cursor++) {
        unsigned char ch = (unsigned char)*cursor;
        if (!(isalnum(ch) || ch == '_' || ch == '-') ||
            editor->material_search_text_length + 1U >=
                sizeof(editor->material_search_text)) continue;
        editor->material_search_text[editor->material_search_text_length++] =
            (char)ch;
        editor->material_search_text[editor->material_search_text_length] = '\0';
    }
    if (!editor_rebuild_search_results(editor)) {
        editor->status = EDITOR_STATUS_OUT_OF_MEMORY;
    }
}

static void editor_backspace_material_search(UnifiedEditorState *editor) {
    if (!editor || editor->material_search_text_length == 0U) return;
    editor->material_search_text[--editor->material_search_text_length] = '\0';
    if (!editor_rebuild_search_results(editor)) {
        editor->status = EDITOR_STATUS_OUT_OF_MEMORY;
    }
}

static bool editor_create_searched_material(UnifiedEditorState *editor) {
    MaterialDocument document;
    MaterialDocumentResult result;
    const char glyphs[4] = {'#', '#', '#', '#'};
    MaterialId id;
    if (!editor || !editor->material_root || !editor->asset_root ||
        editor->material_search_text_length == 0U) {
        editor->status = EDITOR_STATUS_INVALID_MATERIAL;
        return false;
    }
    material_document_init(&document);
    result = material_document_create(
        &document, editor->assets, editor->material_search_text,
        (uint16_t)config_get()->default_palette_id, glyphs);
    if (result == MATERIAL_DOCUMENT_DUPLICATE_NAME) {
        int existing = material_find_by_name(
            editor->assets, editor->material_search_text);
        material_document_destroy(&document);
        if (existing <= 0) {
            editor->status = EDITOR_STATUS_INVALID_MATERIAL;
            return false;
        }
        editor->material_collision_id = (MaterialId)existing;
        editor->modal = EDITOR_MODAL_MATERIAL_COLLISION;
        editor->status = EDITOR_STATUS_NONE;
        return false;
    }
    if (result == MATERIAL_DOCUMENT_OK &&
        asset_refresh_save_material_as(
            &document, editor->assets, &editor->document,
            editor->material_root, editor->asset_root,
            &editor->last_scene_diagnostic) != ASSET_REFRESH_OK) {
        result = MATERIAL_DOCUMENT_IO_ERROR;
    }
    id = document.value.id;
    material_document_destroy(&document);
    if (result != MATERIAL_DOCUMENT_OK || !editor_shortlist_add(editor, id)) {
        editor->status = result == MATERIAL_DOCUMENT_OUT_OF_MEMORY
            ? EDITOR_STATUS_OUT_OF_MEMORY : EDITOR_STATUS_SAVE_FAILED;
        return false;
    }
    editor_sort_shortlist(editor);
    if (!editor_rebuild_asset_shortlists(editor)) {
        editor->status = EDITOR_STATUS_OUT_OF_MEMORY;
        return false;
    }
    editor->highlighted_material = id;
    return true;
}

static bool editor_apply_material_selection(UnifiedEditorState *editor,
                                            MaterialId id) {
    CommandResult result;
    if (!editor || id == 0U || !material_id_is_loaded(editor->assets, id)) {
        return false;
    }
    if (editor->selection_set.count > 1U) {
        result = unified_editor_apply_material_to_selection(editor, id);
    } else if (editor->selection.type == SELECTION_WALL_FACE) {
        result = unified_editor_set_wall_material(editor, id);
    } else {
        result = unified_editor_set_surface_material(
            editor, editor->selection.value.horizontal.map_x,
            editor->selection.value.horizontal.map_y,
            editor->selection.type == SELECTION_FLOOR
                ? SCENE_SURFACE_FLOOR : SCENE_SURFACE_CEILING,
            id);
    }
    if (result != CMD_RESULT_OK && result != CMD_RESULT_NO_CHANGE) return false;
    editor->highlighted_material = id;
    editor->material_picker_open = false;
    editor->material_collision_id = 0U;
    editor->modal = EDITOR_MODAL_NONE;
    return true;
}

static bool editor_overwrite_colliding_material(UnifiedEditorState *editor) {
    MaterialDocument document;
    MaterialDocumentResult result;
    AssetRefreshResult refresh_result = ASSET_REFRESH_INVALID_ARGUMENT;
    const char glyphs[4] = {'#', '#', '#', '#'};
    MaterialId id;
    if (!editor || !editor->material_root || !editor->asset_root ||
        editor->material_collision_id == 0U) return false;
    id = editor->material_collision_id;
    material_document_init(&document);
    result = material_document_create_replacement(
        &document, editor->assets, id, editor->material_search_text,
        (uint16_t)config_get()->default_palette_id, glyphs);
    if (result == MATERIAL_DOCUMENT_OK) {
        refresh_result = asset_refresh_save_material_as(
            &document, editor->assets, &editor->document,
            editor->material_root, editor->asset_root,
            &editor->last_scene_diagnostic);
    }
    material_document_destroy(&document);
    if (result != MATERIAL_DOCUMENT_OK || refresh_result != ASSET_REFRESH_OK) {
        editor->status = result == MATERIAL_DOCUMENT_OUT_OF_MEMORY ||
                         refresh_result == ASSET_REFRESH_OUT_OF_MEMORY
            ? EDITOR_STATUS_OUT_OF_MEMORY : EDITOR_STATUS_SAVE_FAILED;
        return false;
    }
    if (!editor_rebuild_asset_shortlists(editor)) {
        editor->status = EDITOR_STATUS_OUT_OF_MEMORY;
        return false;
    }
    return editor_apply_material_selection(editor, id);
}

static void editor_handle_confirm_apply(UnifiedEditorState *editor) {
    if (!editor->inspector_open) return;
    if (editor->surface_field == EDITOR_SURFACE_FIELD_CONSTRUCTION) {
        if (editor->selection_set.count > 1U)
            (void)unified_editor_apply_construction_to_selection(editor);
        else if (editor->selection.type == SELECTION_WALL_FACE)
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
    if (editor->highlighted_material == 0 &&
        !editor_create_searched_material(editor)) {
        if (editor->modal != EDITOR_MODAL_MATERIAL_COLLISION) {
            editor->status = EDITOR_STATUS_INVALID_MATERIAL;
        }
        return;
    }
    if (editor->selection.type == SELECTION_WALL_FACE)
        (void)(editor->selection_set.count > 1U
            ? unified_editor_apply_material_to_selection(
                editor, editor->highlighted_material)
            : unified_editor_set_wall_material(editor, editor->highlighted_material));
    else if (editor->selection_set.count > 1U)
        (void)unified_editor_apply_material_to_selection(
            editor, editor->highlighted_material);
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
    } while ((editor->surface_field == EDITOR_SURFACE_FIELD_CONSTRUCTION &&
              editor_construction_is_disabled(editor)) ||
             (editor->selection_set.count > 1U &&
               (editor->surface_field == EDITOR_SURFACE_FIELD_AMBIENT ||
                editor->surface_field > EDITOR_SURFACE_FIELD_HEIGHT)) ||
             (editor->selection_set.count > 1U &&
              editor->surface_field == EDITOR_SURFACE_FIELD_OPTICS) ||
             (editor->selection.type == SELECTION_WALL_FACE &&
              editor->surface_field >= EDITOR_SURFACE_FIELD_HEIGHT &&
              editor->surface_field != EDITOR_SURFACE_FIELD_OPTICS));
}

static void editor_handle_surface_confirm(UnifiedEditorState *editor) {
    if (editor->surface_field == EDITOR_SURFACE_FIELD_MATERIAL) {
        if (editor->material_picker_open) {
            editor_handle_confirm_apply(editor);
            if (editor->modal != EDITOR_MODAL_MATERIAL_COLLISION) {
                editor->material_picker_open = false;
            }
        } else {
            editor->material_search_text[0] = '\0';
            editor->material_search_text_length = 0U;
            if (!editor_rebuild_search_results(editor)) {
                editor->status = EDITOR_STATUS_OUT_OF_MEMORY;
            }
            editor->material_picker_open = true;
        }
    } else if (editor->surface_field == EDITOR_SURFACE_FIELD_DECALS) {
        editor->decal_menu_open = true;
        editor->decal_menu_stage = EDITOR_DECAL_MENU_LIST;
        editor->decal_menu_index = 0U;
    } else if (editor->surface_field == EDITOR_SURFACE_FIELD_AMBIENT) {
        editor->light_value_editing = true;
        editor->light_value_text_length = 0U;
        editor->light_value_text[0] = '\0';
    } else if (editor->surface_field == EDITOR_SURFACE_FIELD_REMOVE) {
        (void)unified_editor_toggle_selected_surface_presence(editor);
    } else if (editor->surface_field == EDITOR_SURFACE_FIELD_MOVEMENT) {
        editor->movement_menu_open = true;
        editor->movement_field = EDITOR_MOVEMENT_FIELD_GRAVITY_MAGNITUDE;
    } else if (editor->surface_field == EDITOR_SURFACE_FIELD_OPTICS) {
        editor->optical_menu_open = true;
        editor->transparency_menu_open = false;
        editor->optical_scope = EDITOR_OPTICAL_SCOPE_CELL;
        editor->optical_field = EDITOR_OPTICAL_FIELD_PLAYER_BLOCKS;
        editor->optical_menu_index = 0U;
        editor->transparency_field = EDITOR_TRANSPARENCY_FIELD_MASTER;
    } else if (!editor_construction_is_disabled(editor)) {
        editor_handle_confirm_apply(editor);
    }
}

static void editor_step_movement_field(UnifiedEditorState *editor, int direction) {
    if (direction < 0) {
        editor->movement_field = editor->movement_field ==
            EDITOR_MOVEMENT_FIELD_GRAVITY_MAGNITUDE
            ? (EditorMovementField)(EDITOR_MOVEMENT_FIELD_COUNT - 1)
            : (EditorMovementField)(editor->movement_field - 1);
    } else {
        editor->movement_field = (EditorMovementField)(
            (editor->movement_field + 1) % EDITOR_MOVEMENT_FIELD_COUNT);
    }
}

static EditorOpticalField transparency_optical_field(
    EditorTransparencyField field
) {
    switch (field) {
        case EDITOR_TRANSPARENCY_FIELD_OPACITY:
            return EDITOR_OPTICAL_FIELD_OPACITY;
        case EDITOR_TRANSPARENCY_FIELD_RAY_BLOCKS:
            return EDITOR_OPTICAL_FIELD_RAY_BLOCKS;
        case EDITOR_TRANSPARENCY_FIELD_TRANSMISSION:
            return EDITOR_OPTICAL_FIELD_TRANSMISSION;
        case EDITOR_TRANSPARENCY_FIELD_LIGHT_BLOCKS:
            return EDITOR_OPTICAL_FIELD_LIGHT_BLOCKS;
        default:
            return EDITOR_OPTICAL_FIELD_OPACITY;
    }
}

static void editor_step_transparency_field(
    UnifiedEditorState *editor, int direction
) {
    int next;
    if (!editor || (direction != -1 && direction != 1)) return;
    next = (int)editor->transparency_field + direction;
    if (next < 0) next = EDITOR_TRANSPARENCY_FIELD_COUNT - 1;
    if (next >= EDITOR_TRANSPARENCY_FIELD_COUNT) next = 0;
    editor->transparency_field = (EditorTransparencyField)next;
}

static void editor_step_decal_menu(UnifiedEditorState *editor, int direction) {
    size_t count;
    if (editor->decal_menu_stage == EDITOR_DECAL_MENU_CREATE_DIMENSIONS) {
        editor->decal_create_edit_rows = !editor->decal_create_edit_rows;
        return;
    }
    count = editor->decal_menu_stage == EDITOR_DECAL_MENU_LIST
        ? editor_surface_decal_count(editor) + 1U
        : editor->decal_shortlist_count + 1U;
    if (count == 0U) return;
    if (direction < 0)
        editor->decal_menu_index = editor->decal_menu_index == 0U
            ? count - 1U : editor->decal_menu_index - 1U;
    else editor->decal_menu_index = (editor->decal_menu_index + 1U) % count;
}

static void editor_confirm_decal_menu(UnifiedEditorState *editor) {
    if (editor->decal_menu_stage == EDITOR_DECAL_MENU_LIST) {
        size_t count = editor_surface_decal_count(editor);
        if (editor->selection_set.count > 1U) count = 0U;
        if (editor->decal_menu_index < count) {
            const SceneDecalInstance *decal = editor_surface_decal_at(
                editor, editor->decal_menu_index);
            if (!decal) return;
            editor->selection.type = SELECTION_DECAL;
            editor->selection.value.decal.id = decal->id;
            editor_reset_selection_set(editor);
            editor->inspector_kind = EDITOR_INSPECTOR_DECAL;
            editor->decal_field = EDITOR_DECAL_FIELD_POSITION_U;
            editor->decal_menu_open = false;
        } else {
            editor->decal_menu_stage = EDITOR_DECAL_MENU_PATTERNS;
            editor->decal_menu_index = 0U;
        }
        return;
    }
    if (editor->decal_menu_stage == EDITOR_DECAL_MENU_PATTERNS) {
        if (editor->decal_menu_index < editor->decal_shortlist_count) {
            if (editor->selection_set.count > 1U)
                (void)unified_editor_place_decal_on_selection(
                    editor, editor->decal_shortlist[editor->decal_menu_index], 1.0, 1.0);
            else
                (void)unified_editor_place_decal(
                    editor, editor->decal_shortlist[editor->decal_menu_index], 1.0, 1.0);
        } else {
            editor->decal_menu_stage = EDITOR_DECAL_MENU_CREATE_DIMENSIONS;
            editor->decal_create_cols = 1U;
            editor->decal_create_rows = 1U;
            editor->decal_create_edit_rows = false;
        }
        return;
    }
    (void)editor_create_empty_decal_pattern(editor);
}

static void editor_adjust_decal_create_dimension(
    UnifiedEditorState *editor, int direction
) {
    size_t *value = editor->decal_create_edit_rows
        ? &editor->decal_create_rows : &editor->decal_create_cols;
    size_t maximum = editor->decal_create_edit_rows
        ? DECAL_PATTERN_ASSET_MAX_ROWS : DECAL_PATTERN_ASSET_MAX_COLS;
    if (direction < 0) {
        if (*value > 1U) (*value)--;
    } else if (*value < maximum) (*value)++;
}

static void editor_handle_light_field_prev(UnifiedEditorState *editor) {
    editor->light_value_editing = false;
    editor->light_value_text_length = 0U;
    editor->light_value_text[0] = '\0';
    if (editor->light_field == EDITOR_LIGHT_FIELD_X)
        editor->light_field = EDITOR_LIGHT_FIELD_FALLOFF;
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

static void editor_handle_sprite_field_prev(UnifiedEditorState *editor) {
    editor_cancel_light_value_edit(editor);
    if (editor->sprite_field == EDITOR_SPRITE_FIELD_X)
        editor->sprite_field = EDITOR_SPRITE_FIELD_REMOVE;
    else
        editor->sprite_field = (EditorSpriteField)(editor->sprite_field - 1);
}

static void editor_handle_sprite_field_next(UnifiedEditorState *editor) {
    editor_cancel_light_value_edit(editor);
    editor->sprite_field = (EditorSpriteField)(editor->sprite_field + 1);
    if (editor->sprite_field >= EDITOR_SPRITE_FIELD_COUNT)
        editor->sprite_field = EDITOR_SPRITE_FIELD_X;
}

static bool editor_commit_sprite_value(UnifiedEditorState *editor) {
    char *end = NULL;
    double value;
    if (!editor || !editor->light_value_editing ||
        editor->light_value_text_length == 0U) return false;
    value = strtod(editor->light_value_text, &end);
    if (!end || *end != '\0' ||
        unified_editor_set_sprite_field_value(
            editor, editor->sprite_field, value) == CMD_RESULT_INVALID_TARGET) {
        editor->status = EDITOR_STATUS_INVALID_NUMERIC_VALUE;
        return true;
    }
    editor_cancel_light_value_edit(editor);
    return true;
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

static bool editor_commit_decal_value(UnifiedEditorState *editor) {
    char *end = NULL;
    double value;
    if (!editor || !editor->light_value_editing ||
        editor->light_value_text_length == 0U) return false;
    value = strtod(editor->light_value_text, &end);
    if (!end || *end != '\0' ||
        unified_editor_set_decal_field_value(editor, editor->decal_field, value) ==
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
    if (editor->modal == EDITOR_MODAL_MATERIAL_COLLISION) {
        if (!editor_apply_material_selection(
                editor, editor->material_collision_id)) {
            editor->status = EDITOR_STATUS_INVALID_MATERIAL;
        }
        return;
    }

    if (editor->modal == EDITOR_MODAL_MATERIAL_OVERWRITE_PROMPT) {
        if (!editor_overwrite_colliding_material(editor)) {
            editor->modal = EDITOR_MODAL_MATERIAL_COLLISION;
        }
        return;
    }

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

    if (editor->modal == EDITOR_MODAL_LIGHT_REMOVE_PROMPT) {
        editor->modal = EDITOR_MODAL_NONE;
        if (editor->selection.type == SELECTION_LIGHT) {
            (void)unified_editor_remove_light(
                editor, editor->selection.value.light.id);
        }
        return;
    }

    if (editor->modal == EDITOR_MODAL_DECAL_REMOVE_PROMPT) {
        editor->modal = EDITOR_MODAL_NONE;
        if (editor->selection.type == SELECTION_DECAL)
            (void)unified_editor_remove_decal(
                editor, editor->selection.value.decal.id);
        return;
    }

    if (editor->modal == EDITOR_MODAL_SPRITE_REMOVE_PROMPT) {
        editor->modal = EDITOR_MODAL_NONE;
        if (editor->selection.type == SELECTION_SPRITE)
            (void)unified_editor_remove_sprite(
                editor, editor->selection.value.sprite.id);
        return;
    }
    if (editor->modal == EDITOR_MODAL_TRIGGER_REMOVE_PROMPT) {
        editor->modal = EDITOR_MODAL_NONE;
        if (editor->selection.type == SELECTION_TRIGGER)
            (void)unified_editor_remove_trigger(
                editor, editor->selection.value.trigger.id);
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
                if (editor->modal == EDITOR_MODAL_MATERIAL_OVERWRITE_PROMPT) {
                    editor->modal = EDITOR_MODAL_MATERIAL_COLLISION;
                } else if (editor->modal == EDITOR_MODAL_MATERIAL_COLLISION) {
                    editor->material_collision_id = 0U;
                    editor->material_picker_open = false;
                    editor->modal = EDITOR_MODAL_NONE;
                } else if (editor->modal == EDITOR_MENU_SAVE) {
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
            } else if (editor->modal == EDITOR_MODAL_MATERIAL_COLLISION &&
                       input->editor_overwrite_pressed) {
                editor->modal = EDITOR_MODAL_MATERIAL_OVERWRITE_PROMPT;
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
                       input->editor_increase_pressed ||
                       input->editor_overwrite_pressed) {
                editor_mark_keyboard(&consumed);
            }
            return consumed;
        }


        /* 3. Inline numeric entry precedes the normal escape hierarchy. */
        if (input->editor_cancel_pressed) {
            if (editor->sprite_menu_open) {
                if (editor->sprite_menu_stage == EDITOR_SPRITE_MENU_PAINT ||
                    editor->sprite_menu_stage == EDITOR_SPRITE_MENU_LOAD) {
                    editor->sprite_menu_stage = EDITOR_SPRITE_MENU_ACTIONS;
                    editor->sprite_menu_index = 0U;
                } else {
                    editor_close_sprite_menu(editor);
                }
                editor_mark_keyboard(&consumed);
                return consumed;
            } else if (editor->light_value_editing) {
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
        if (unified_editor_has_document(editor) && rmap && camera &&
            !editor->vertical_physics.initialized) {
            SceneHeightView initial_height_view;
            OpticalRuntimeView optical_view;
            uint32_t optical_generation = 0U;
            bool has_optical = scene_document_get_optical_view(
                &editor->document, &optical_view, &optical_generation);
            if (scene_document_get_height_view(
                    &editor->document, &initial_height_view)) {
                (void)vertical_physics_reset_optical(
                    &editor->vertical_physics, camera, rmap, &initial_height_view,
                    has_optical ? &optical_view : NULL, optical_generation);
            }
        }
        if (editor->mode == EDITOR_MODE_WALK &&
            unified_editor_has_document(editor) && rmap) {
            SceneHeightView height_view;
            OpticalRuntimeView optical_view;
            uint32_t optical_generation = 0U;
            bool has_optical = scene_document_get_optical_view(
                &editor->document, &optical_view, &optical_generation);
            double previous_x = camera->transform.pos.x;
            double previous_y = camera->transform.pos.y;
            bool forward = input->forward;
            bool backward = input->backward;
            bool left = input->left;
            bool right = input->right;
            bool jump = input->editor_jump_pressed;
            bool painting = editor->sprite_menu_open &&
                editor->sprite_menu_stage == EDITOR_SPRITE_MENU_PAINT;
            if (painting) {
                input->forward = false;
                input->backward = false;
                input->left = false;
                input->right = false;
                input->editor_jump_pressed = false;
            }
            if (scene_document_get_height_view(&editor->document, &height_view)) {
                VerticalPhysicsResult physics_result;
                if (input->editor_jump_pressed)
                    (void)vertical_physics_jump_optical(
                        &editor->vertical_physics, camera, rmap, &height_view,
                        has_optical ? &optical_view : NULL, optical_generation);
                camera_update_optical(
                    camera, rmap, input, delta_seconds, viewport_rows,
                    has_optical ? &optical_view : NULL, optical_generation);
                if (!editor->vertical_physics.grounded) {
                    double scale = height_view.movement.air_control_scale;
                    camera->transform.pos.x = previous_x +
                        (camera->transform.pos.x - previous_x) * scale;
                    camera->transform.pos.y = previous_y +
                        (camera->transform.pos.y - previous_y) * scale;
                }
                physics_result = vertical_physics_step_optical(
                    &editor->vertical_physics, camera, rmap, &height_view,
                    previous_x, previous_y, delta_seconds,
                    has_optical ? &optical_view : NULL, optical_generation);
                if (physics_result == VERTICAL_PHYSICS_BLOCKED_STEP ||
                    physics_result == VERTICAL_PHYSICS_BLOCKED_CLEARANCE) {
                    editor->status = EDITOR_STATUS_PLAYER_BLOCKED;
                } else if (physics_result == VERTICAL_PHYSICS_OK &&
                           editor->status == EDITOR_STATUS_PLAYER_BLOCKED) {
                    editor->status = EDITOR_STATUS_NONE;
                }
            }
            input->forward = forward; input->backward = backward;
            input->left = left; input->right = right;
            input->editor_jump_pressed = jump;
            editor_tick_triggers(editor, camera, delta_seconds);
        } else {
            consumed.pointer_consumed = true;
        }

        /* 6. Hover ray every frame (both modes; aim freezes in edit). */
        if (unified_editor_has_document(editor) && cmap) {
            EditorHit hit = editor_raycast_selection(camera, cmap);
            SceneHeightView height_view;
            const SceneHeightView *heights = scene_document_get_height_view(
                &editor->document, &height_view) ? &height_view : NULL;
            size_t light_count = 0U;
            const SceneLight *lights = scene_document_get_lights(
                &editor->document, &light_count);
            double max_distance = config_get()->raycast_max_distance;
            if (max_distance <= 0.0) max_distance = 20.0;
            editor->hover = editor_pick_light_selection(
                camera, lights, light_count, hit, max_distance,
                EDITOR_LIGHT_PICK_RADIUS);
            {
                size_t sprite_count = 0U;
                const SceneSpriteInstance *sprites = scene_document_get_sprites(
                    &editor->document, &sprite_count);
                editor->hover = editor_pick_sprite_selection(
                    camera, sprites, sprite_count, editor->hover, max_distance,
                    EDITOR_SPRITE_PICK_RADIUS);
            }
            {
                size_t trigger_count = 0U;
                const SceneTrigger *triggers = scene_document_get_triggers(
                    &editor->document, &trigger_count);
                editor->hover = editor_pick_trigger_selection(
                    camera, triggers, trigger_count, editor->hover, max_distance);
            }
            editor->hover = editor_pick_horizontal_surface_selection_height(
                camera, cmap, heights, editor->hover, viewport_rows, max_distance);
        }
    }

    /* 7. Select hovered wall. */
    if (input->editor_select_pressed && !consumed.keyboard_consumed) {
        editor_handle_select(editor, &consumed);
    }

    if (!consumed.keyboard_consumed && editor->inspector_open &&
        (input->ctrl_left || input->ctrl_right || input->ctrl_up || input->ctrl_down)) {
        int dx = input->ctrl_left ? -1 : (input->ctrl_right ? 1 : 0);
        int dy = input->ctrl_up ? -1 : (input->ctrl_down ? 1 : 0);
        (void)unified_editor_extend_selection(editor, camera, dx, dy);
        editor_mark_keyboard(&consumed);
    }

    /* 8. Inspector navigation (only while open). */
    if (!consumed.keyboard_consumed && editor->inspector_open) {
        if (editor_handle_sprite_menu_input(editor, input)) {
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && editor->optical_menu_open &&
            editor->transparency_menu_open && input->editor_previous_pressed) {
            editor_step_transparency_field(editor, -1);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && editor->optical_menu_open &&
                   editor->transparency_menu_open && input->editor_next_pressed) {
            editor_step_transparency_field(editor, 1);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && editor->optical_menu_open &&
                   editor->transparency_menu_open &&
                   (input->editor_decrease_pressed || input->editor_increase_pressed)) {
            int direction = input->editor_decrease_pressed ? -1 : 1;
            if (editor->transparency_field == EDITOR_TRANSPARENCY_FIELD_MASTER) {
                (void)unified_editor_step_selected_transparency(editor, direction);
            } else {
                editor->optical_field = transparency_optical_field(
                    editor->transparency_field);
                (void)unified_editor_step_selected_optical(editor, direction);
            }
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && editor->optical_menu_open &&
                   editor->transparency_menu_open && input->editor_confirm_pressed) {
            if (editor->transparency_field == EDITOR_TRANSPARENCY_FIELD_MASTER) {
                (void)unified_editor_clear_selected_transparency_override(editor);
            } else {
                editor->optical_field = transparency_optical_field(
                    editor->transparency_field);
                (void)unified_editor_toggle_selected_optical_inherit(editor);
            }
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && editor->optical_menu_open &&
            input->editor_previous_pressed) {
            editor->optical_menu_index = editor->optical_menu_index == 0U
                ? 3U : editor->optical_menu_index - 1U;
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && editor->optical_menu_open &&
                   input->editor_next_pressed) {
            editor->optical_menu_index = (editor->optical_menu_index + 1U) %
                4U;
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && editor->optical_menu_open &&
                   (input->editor_decrease_pressed || input->editor_increase_pressed)) {
            int direction = input->editor_decrease_pressed ? -1 : 1;
            if (editor->optical_menu_index == 0U) {
                unified_editor_toggle_optical_scope(editor);
            } else if (editor->optical_menu_index == 1U) {
                editor->optical_field = EDITOR_OPTICAL_FIELD_PLAYER_BLOCKS;
                (void)unified_editor_step_selected_optical(editor, direction);
            } else if (editor->optical_menu_index == 3U) {
                editor->optical_field = EDITOR_OPTICAL_FIELD_REFLECTIVITY;
                (void)unified_editor_step_selected_optical(editor, direction);
            }
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && editor->optical_menu_open &&
                   input->editor_confirm_pressed) {
            if (editor->optical_menu_index == 0U) {
                unified_editor_toggle_optical_scope(editor);
            } else if (editor->optical_menu_index == 2U) {
                editor->transparency_menu_open = true;
                editor->transparency_field = EDITOR_TRANSPARENCY_FIELD_MASTER;
            } else {
                editor->optical_field = editor->optical_menu_index == 1U
                    ? EDITOR_OPTICAL_FIELD_PLAYER_BLOCKS
                    : EDITOR_OPTICAL_FIELD_REFLECTIVITY;
                (void)unified_editor_toggle_selected_optical_inherit(editor);
            }
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && editor->movement_menu_open &&
            input->editor_previous_pressed) {
            editor_step_movement_field(editor, -1);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && editor->movement_menu_open &&
                   input->editor_next_pressed) {
            editor_step_movement_field(editor, 1);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && editor->movement_menu_open &&
                   input->editor_decrease_pressed) {
            (void)unified_editor_step_movement_parameter(
                editor, editor->movement_field, -1);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && editor->movement_menu_open &&
                   input->editor_increase_pressed) {
            (void)unified_editor_step_movement_parameter(
                editor, editor->movement_field, 1);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && editor->decal_menu_open &&
            input->editor_previous_pressed) {
            editor_step_decal_menu(editor, -1);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && editor->decal_menu_open &&
                   input->editor_next_pressed) {
            editor_step_decal_menu(editor, 1);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && editor->decal_menu_open &&
                   input->editor_confirm_pressed) {
            editor_confirm_decal_menu(editor);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && editor->decal_menu_open &&
                   input->editor_decrease_pressed) {
            if (editor->decal_menu_stage == EDITOR_DECAL_MENU_CREATE_DIMENSIONS)
                editor_adjust_decal_create_dimension(editor, -1);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) && editor->decal_menu_open &&
                   input->editor_increase_pressed) {
            if (editor->decal_menu_stage == EDITOR_DECAL_MENU_CREATE_DIMENSIONS)
                editor_adjust_decal_create_dimension(editor, 1);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) &&
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
                   editor->material_picker_open &&
                   input->editor_text_backspace_pressed) {
            editor_backspace_material_search(editor);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) &&
                   editor->material_picker_open && input->text_input_len > 0) {
            editor_append_material_search(editor, input->text_input);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) &&
                   editor->material_picker_open && input->editor_previous_pressed) {
            editor_handle_picker_prev(editor);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) &&
                   editor->material_picker_open && input->editor_next_pressed) {
            editor_handle_picker_next(editor);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) &&
                   input->editor_decrease_pressed &&
                   editor->surface_field == EDITOR_SURFACE_FIELD_HEIGHT) {
            (void)unified_editor_step_selected_height(editor, -1);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) &&
                   input->editor_increase_pressed &&
                   editor->surface_field == EDITOR_SURFACE_FIELD_HEIGHT) {
            (void)unified_editor_step_selected_height(editor, 1);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) &&
                   input->editor_decrease_pressed &&
                   editor->surface_field == EDITOR_SURFACE_FIELD_GRAVITY_DIRECTION) {
            (void)unified_editor_step_selected_gravity_direction(editor, -1);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) &&
                   input->editor_increase_pressed &&
                   editor->surface_field == EDITOR_SURFACE_FIELD_GRAVITY_DIRECTION) {
            (void)unified_editor_step_selected_gravity_direction(editor, 1);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) &&
                   input->editor_decrease_pressed &&
                   editor->surface_field == EDITOR_SURFACE_FIELD_GRAVITY_SCALE) {
            (void)unified_editor_step_selected_gravity_scale(editor, -1);
            editor_mark_keyboard(&consumed);
        } else if (editor_is_surface_inspector(editor) &&
                   input->editor_increase_pressed &&
                   editor->surface_field == EDITOR_SURFACE_FIELD_GRAVITY_SCALE) {
            (void)unified_editor_step_selected_gravity_scale(editor, 1);
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
                   !editor->light_value_editing &&
                   input->editor_confirm_pressed &&
                   editor->light_field == EDITOR_LIGHT_FIELD_REMOVE) {
            editor->modal = EDITOR_MODAL_LIGHT_REMOVE_PROMPT;
            editor_mark_keyboard(&consumed);
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
                   editor->light_field != EDITOR_LIGHT_FIELD_REMOVE &&
                   input->text_input_len > 0) {
            editor_append_light_value_text(editor, input->text_input);
            editor_mark_keyboard(&consumed);
        } else if (editor->inspector_kind == EDITOR_INSPECTOR_DECAL &&
                   !editor->light_value_editing && input->editor_confirm_pressed &&
                   editor->decal_field == EDITOR_DECAL_FIELD_REMOVE) {
            editor->modal = EDITOR_MODAL_DECAL_REMOVE_PROMPT;
            editor_mark_keyboard(&consumed);
        } else if (editor->inspector_kind == EDITOR_INSPECTOR_DECAL &&
                   editor->light_value_editing && input->editor_confirm_pressed) {
            (void)editor_commit_decal_value(editor);
            editor_mark_keyboard(&consumed);
        } else if (editor->inspector_kind == EDITOR_INSPECTOR_DECAL &&
                   editor->light_value_editing &&
                   input->editor_text_backspace_pressed) {
            editor_backspace_light_value(editor);
            editor_mark_keyboard(&consumed);
        } else if (editor->inspector_kind == EDITOR_INSPECTOR_DECAL &&
                   editor->decal_field != EDITOR_DECAL_FIELD_REMOVE &&
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
        } else if (input->editor_previous_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_DECAL) {
            editor_cancel_light_value_edit(editor);
            editor->decal_field = editor->decal_field == EDITOR_DECAL_FIELD_POSITION_U
                ? (EditorDecalField)(EDITOR_DECAL_FIELD_COUNT - 1)
                : (EditorDecalField)(editor->decal_field - 1);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_previous_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_TRIGGER) {
            editor->trigger_field = editor->trigger_field == EDITOR_TRIGGER_FIELD_MIN_X
                ? EDITOR_TRIGGER_FIELD_REMOVE
                : (EditorTriggerField)(editor->trigger_field - 1);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_next_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_TRIGGER) {
            editor->trigger_field = (EditorTriggerField)(
                (editor->trigger_field + 1) % EDITOR_TRIGGER_FIELD_COUNT);
            editor_mark_keyboard(&consumed);
        } else if ((input->editor_decrease_pressed || input->editor_increase_pressed) &&
                   editor->inspector_kind == EDITOR_INSPECTOR_TRIGGER &&
                   editor->trigger_field != EDITOR_TRIGGER_FIELD_REMOVE &&
                   editor->trigger_field != EDITOR_TRIGGER_FIELD_CONDITION) {
            (void)unified_editor_step_trigger_field(
                editor, editor->trigger_field,
                input->editor_decrease_pressed ? -1 : 1);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_confirm_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_TRIGGER) {
            if (editor->trigger_field == EDITOR_TRIGGER_FIELD_REMOVE)
                editor->modal = EDITOR_MODAL_TRIGGER_REMOVE_PROMPT;
            else if (editor->trigger_field == EDITOR_TRIGGER_FIELD_ACTION)
                (void)unified_editor_step_trigger_field(
                    editor, editor->trigger_field, 1);
            else if (editor->trigger_field == EDITOR_TRIGGER_FIELD_PAYLOAD) {
                (void)editor_confirm_trigger_payload(editor);
            }
            editor_mark_keyboard(&consumed);
        } else if (input->editor_next_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_DECAL) {
            editor_cancel_light_value_edit(editor);
            editor->decal_field = (EditorDecalField)(
                (editor->decal_field + 1) % EDITOR_DECAL_FIELD_COUNT);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_decrease_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_DECAL) {
            if (editor->decal_field != EDITOR_DECAL_FIELD_REMOVE)
                (void)unified_editor_step_decal_field(
                    editor, editor->decal_field, -1);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_increase_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_DECAL) {
            if (editor->decal_field != EDITOR_DECAL_FIELD_REMOVE)
                (void)unified_editor_step_decal_field(
                    editor, editor->decal_field, 1);
            editor_mark_keyboard(&consumed);
        } else if (editor->inspector_kind == EDITOR_INSPECTOR_SPRITE &&
                   !editor->light_value_editing &&
                   input->editor_confirm_pressed &&
                   editor->sprite_field == EDITOR_SPRITE_FIELD_PATTERN) {
            (void)editor_begin_selected_sprite_pattern(editor);
            editor_mark_keyboard(&consumed);
        } else if (editor->inspector_kind == EDITOR_INSPECTOR_SPRITE &&
                   !editor->light_value_editing &&
                   input->editor_confirm_pressed &&
                   editor->sprite_field == EDITOR_SPRITE_FIELD_REMOVE) {
            editor->modal = EDITOR_MODAL_SPRITE_REMOVE_PROMPT;
            editor_mark_keyboard(&consumed);
        } else if (editor->inspector_kind == EDITOR_INSPECTOR_SPRITE &&
                   editor->light_value_editing &&
                   input->editor_confirm_pressed) {
            (void)editor_commit_sprite_value(editor);
            editor_mark_keyboard(&consumed);
        } else if (editor->inspector_kind == EDITOR_INSPECTOR_SPRITE &&
                   editor->light_value_editing &&
                   input->editor_text_backspace_pressed) {
            editor_backspace_light_value(editor);
            editor_mark_keyboard(&consumed);
        } else if (editor->inspector_kind == EDITOR_INSPECTOR_SPRITE &&
                   editor->sprite_field != EDITOR_SPRITE_FIELD_REMOVE &&
                   input->text_input_len > 0) {
            editor_append_light_value_text(editor, input->text_input);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_previous_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_SPRITE) {
            editor_handle_sprite_field_prev(editor);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_next_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_SPRITE) {
            editor_handle_sprite_field_next(editor);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_decrease_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_SPRITE) {
            if (editor->sprite_field == EDITOR_SPRITE_FIELD_X ||
                editor->sprite_field == EDITOR_SPRITE_FIELD_Y)
                (void)unified_editor_step_sprite_field(
                    editor, editor->sprite_field, -1);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_increase_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_SPRITE) {
            if (editor->sprite_field == EDITOR_SPRITE_FIELD_X ||
                editor->sprite_field == EDITOR_SPRITE_FIELD_Y)
                (void)unified_editor_step_sprite_field(
                    editor, editor->sprite_field, 1);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_decrease_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_LIGHT) {
            if (editor->light_field != EDITOR_LIGHT_FIELD_REMOVE) {
                (void)unified_editor_step_light_field(
                    editor, editor->light_field, -1);
                editor->light_repeat_direction = input->held_arrow_left ? -1 : 0;
                editor->light_repeat_elapsed = input->held_arrow_left
                    ? EDITOR_LIGHT_REPEAT_DELAY_SECONDS : 0.0;
            } else {
                editor->light_repeat_direction = 0;
                editor->light_repeat_elapsed = 0.0;
            }
            editor_mark_keyboard(&consumed);
        } else if (input->editor_increase_pressed &&
                   editor->inspector_kind == EDITOR_INSPECTOR_LIGHT) {
            if (editor->light_field != EDITOR_LIGHT_FIELD_REMOVE) {
                (void)unified_editor_step_light_field(
                    editor, editor->light_field, 1);
                editor->light_repeat_direction = input->held_arrow_right ? 1 : 0;
                editor->light_repeat_elapsed = input->held_arrow_right
                    ? EDITOR_LIGHT_REPEAT_DELAY_SECONDS : 0.0;
            } else {
                editor->light_repeat_direction = 0;
                editor->light_repeat_elapsed = 0.0;
            }
            editor_mark_keyboard(&consumed);
        } else if (editor->inspector_kind == EDITOR_INSPECTOR_LIGHT &&
                   !editor->light_value_editing &&
                   editor->light_field != EDITOR_LIGHT_FIELD_REMOVE) {
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
        } else if (input->editor_place_light_pressed) {
            (void)unified_editor_place_light(editor);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_place_sprite_pressed) {
            (void)editor_create_and_place_sprite_canvas(editor);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_place_trigger_pressed) {
            (void)unified_editor_place_trigger(editor);
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

static void editor_render_sprite_pattern_menu(
    const UnifiedEditorState *editor, Grid *grid, int *row,
    SDL_Color fg, SDL_Color bg, SDL_Color dim, SDL_Color warn, SDL_Color hi
) {
    static const char *actions[3] = {
        "Load existing...", "Save pattern", "Edit/Paint"
    };
    char line[160];
    if (!editor || !grid || !row || !editor->sprite_menu_open) return;
    if (editor->sprite_menu_stage == EDITOR_SPRITE_MENU_ACTIONS) {
        size_t action;
        for (action = 0U; action < 3U; action++) {
            snprintf(line, sizeof(line), "   %s %s",
                     editor->sprite_menu_index == action ? ">" : " ",
                     actions[action]);
            grid_print(grid, 1, (*row)++, line,
                       editor->sprite_menu_index == action ? hi : dim, bg);
        }
    } else if (editor->sprite_menu_stage == EDITOR_SPRITE_MENU_LOAD) {
        size_t index;
        grid_print(grid, 1, (*row)++,
                   "     LOAD SPRITE  Up/Down  Enter=load  Esc=back", fg, bg);
        if (editor->sprite_shortlist_count == 0U)
            grid_print(grid, 1, (*row)++, "     (no saved sprite files)", dim, bg);
        for (index = 0U; index < editor->sprite_shortlist_count; index++) {
            snprintf(line, sizeof(line), "     %s sprite pattern:%u",
                     editor->sprite_menu_index == index ? ">" : " ",
                     (unsigned)editor->sprite_shortlist[index]);
            grid_print(grid, 1, (*row)++, line,
                       editor->sprite_menu_index == index ? hi : dim, bg);
        }
    } else {
        size_t y;
        grid_print(grid, 1, (*row)++,
                   "     SPRITE PAINT  Arrows=cursor  Type=paint  Backspace=erase",
                   fg, bg);
        snprintf(line, sizeof(line),
                 "     [ / ]=material  Ctrl+S=save  Esc=back  material:%d%s",
                 editor->sprite_paint_material,
                 editor->sprite_document.dirty ? "  UNSAVED" : "");
        grid_print(grid, 1, (*row)++, line, warn, bg);
        for (y = 0U; y < editor->sprite_document.rows &&
             *row < grid->height - 3; y++) {
            int col = 6;
            size_t x;
            for (x = 0U; x < editor->sprite_document.cols && col < grid->width;
                 x++, col++) {
                PatternCell cell = editor->sprite_document.cells[
                    y * editor->sprite_document.cols + x];
                bool cursor = x == editor->sprite_paint_x &&
                              y == editor->sprite_paint_y;
                (void)grid_set(grid, col, *row,
                    cursor && (cell.glyph == 0U || cell.glyph == (uint8_t)' ')
                        ? (uint8_t)'_' : cell.glyph,
                    cursor ? hi : fg, bg);
            }
            (*row)++;
        }
    }
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
        } else if (editor->hover.valid &&
                   editor->hover.target.type == SELECTION_SPRITE) {
            const SceneSpriteInstance *sprite = scene_document_find_sprite(
                &editor->document, editor->hover.target.value.sprite.id);
            snprintf(line, sizeof(line),
                     "Hover  sprite:%" PRIu64 " asset:%u pos:(%.2f,%.2f) dist:%.2f",
                     editor->hover.target.value.sprite.id,
                     sprite ? (unsigned)sprite->asset.id : 0U,
                     sprite ? sprite->x : 0.0, sprite ? sprite->y : 0.0,
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
                     "Select (%d,%d) face:%s mat:%d%s  selected:%zu primary",
                     wf->map_x, wf->map_y, editor_face_label(wf->face), mat,
                     material_id_is_loaded(editor->assets, (int)mat)
                         ? "" : " (missing)", editor->selection_set.count);
        } else if (editor->selection.type == SELECTION_LIGHT) {
            const SceneLight *light = scene_document_find_light(
                &editor->document, editor->selection.value.light.id);
            snprintf(line, sizeof(line),
                     "Select light:%" PRIu64 " pos:(%.2f,%.2f)",
                     editor->selection.value.light.id,
                     light ? light->x : 0.0, light ? light->y : 0.0);
        } else if (editor->selection.type == SELECTION_DECAL) {
            const SceneDecalInstance *decal = scene_document_find_decal(
                &editor->document, editor->selection.value.decal.id);
            snprintf(line, sizeof(line),
                     "Select decal:%" PRIu64 " asset:%u",
                     editor->selection.value.decal.id,
                     decal ? (unsigned)decal->asset.id : 0U);
        } else if (editor->selection.type == SELECTION_SPRITE) {
            const SceneSpriteInstance *sprite = scene_document_find_sprite(
                &editor->document, editor->selection.value.sprite.id);
            snprintf(line, sizeof(line),
                     "Select sprite:%" PRIu64 " asset:%u pos:(%.2f,%.2f)",
                     editor->selection.value.sprite.id,
                     sprite ? (unsigned)sprite->asset.id : 0U,
                     sprite ? sprite->x : 0.0, sprite ? sprite->y : 0.0);
        } else if (editor->selection.type == SELECTION_TRIGGER) {
            snprintf(line, sizeof(line), "Select trigger:%" PRIu64,
                     editor->selection.value.trigger.id);
        } else if (editor->selection.type == SELECTION_FLOOR ||
                   editor->selection.type == SELECTION_CEILING) {
            snprintf(line, sizeof(line), "Select %s (%d,%d) selected:%zu primary",
                     editor->selection.type == SELECTION_FLOOR ? "floor" : "ceiling",
                     editor->selection.value.horizontal.map_x,
                     editor->selection.value.horizontal.map_y,
                     editor->selection_set.count);
        } else {
            snprintf(line, sizeof(line), "Select (none)");
        }
        grid_print(grid, 1, 3, line, fg, bg);

        row = 5;
        if (editor->inspector_open && editor_is_surface_inspector(editor)) {
            EditorInspectorPresentation presentation;
            size_t count = editor->material_search_result_count;
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
            if (editor->surface_field == EDITOR_SURFACE_FIELD_MATERIAL &&
                       editor->material_picker_open) {
                snprintf(line, sizeof(line), "   Search: %s_",
                         editor->material_search_text);
                grid_print(grid, 1, row++, line, fg, bg);
                if (count == 0U) {
                    grid_print(grid, 1, row++, " > Create new material...", hi, bg);
                }
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

                    id = editor->material_search_results[idx];
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
            if (editor->selection_set.count <= 1U &&
                editor->surface_field == EDITOR_SURFACE_FIELD_AMBIENT &&
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
            if (editor->selection_set.count <= 1U)
                grid_print(grid, 1, row++, line,
                           editor->surface_field == EDITOR_SURFACE_FIELD_AMBIENT ? hi : dim,
                           bg);
            snprintf(line, sizeof(line), " %s Decals      %s  Enter=open",
                     editor->surface_field == EDITOR_SURFACE_FIELD_DECALS &&
                         !editor->decal_menu_open ? ">" : " ",
                      editor->selection_set.count > 1U ? "Add to selection" : "surface list");
            grid_print(grid, 1, row++, line,
                       editor->surface_field == EDITOR_SURFACE_FIELD_DECALS ? hi : dim,
                       bg);
            if (editor->selection_set.count <= 1U) {
                snprintf(line, sizeof(line), " %s Optics... Enter=open",
                         editor->surface_field == EDITOR_SURFACE_FIELD_OPTICS &&
                             !editor->optical_menu_open ? ">" : " ");
                grid_print(grid, 1, row++, line,
                           editor->surface_field == EDITOR_SURFACE_FIELD_OPTICS
                               ? hi : dim, bg);
            }
            if (editor->optical_menu_open) {
                OpticalExtension extension;
                grid_print(grid, 1, row++, editor->transparency_menu_open
                           ? "   TRANSPARENCY  Up/Down  Left/Right=edit  Esc=back"
                           : "   OPTICS  Up/Down  Left/Right=edit  Enter=open/inherit",
                           fg, bg);
                snprintf(line, sizeof(line), " %s Scope: %s",
                         !editor->transparency_menu_open &&
                             editor->optical_menu_index == 0U ? ">" : " ",
                         editor->optical_scope == EDITOR_OPTICAL_SCOPE_CELL
                         ? "cell override" : "material default");
                grid_print(grid, 1, row++, line,
                           !editor->transparency_menu_open &&
                               editor->optical_menu_index == 0U ? hi : dim, bg);
                if (editor_domain_get_optical_extension(
                        &editor->document, editor->selection,
                        editor->optical_scope, &extension, NULL, NULL)) {
                    if (editor->transparency_menu_open) {
                        static const EditorOpticalField fields[] = {
                            EDITOR_OPTICAL_FIELD_OPACITY,
                            EDITOR_OPTICAL_FIELD_RAY_BLOCKS,
                            EDITOR_OPTICAL_FIELD_TRANSMISSION,
                            EDITOR_OPTICAL_FIELD_LIGHT_BLOCKS
                        };
                        static const char *const labels[] = {
                            "Opacity", "Ray blocks", "Transmission", "Light blocks"
                        };
                        char value[32];
                        (void)editor_domain_format_transparency(
                            &editor->document, editor->selection,
                            editor->optical_scope, value, sizeof(value));
                        snprintf(line, sizeof(line), " %s %-14s %s",
                            editor->transparency_field ==
                                EDITOR_TRANSPARENCY_FIELD_MASTER ? ">" : " ",
                            "Transparency", value);
                        grid_print(grid, 1, row++, line,
                            editor->transparency_field ==
                                EDITOR_TRANSPARENCY_FIELD_MASTER ? hi : dim, bg);
                        for (i = 0U; i < 4U; i++) {
                            EditorTransparencyField selected =
                                (EditorTransparencyField)(i + 1U);
                            (void)editor_domain_format_optical_field(
                                &extension, fields[i], value, sizeof(value));
                            snprintf(line, sizeof(line), " %s %-14s %s",
                                editor->transparency_field == selected ? ">" : " ",
                                labels[i], value);
                            grid_print(grid, 1, row++, line,
                                editor->transparency_field == selected ? hi : dim, bg);
                        }
                    } else {
                        char value[32];
                        (void)editor_domain_format_optical_field(
                            &extension, EDITOR_OPTICAL_FIELD_PLAYER_BLOCKS,
                            value, sizeof(value));
                        snprintf(line, sizeof(line), " %s %-14s %s",
                            editor->optical_menu_index == 1U ? ">" : " ",
                            "Player blocks", value);
                        grid_print(grid, 1, row++, line,
                            editor->optical_menu_index == 1U ? hi : dim, bg);
                        (void)editor_domain_format_transparency(
                            &editor->document, editor->selection,
                            editor->optical_scope, value, sizeof(value));
                        snprintf(line, sizeof(line), " %s Transparency... %s",
                            editor->optical_menu_index == 2U ? ">" : " ", value);
                        grid_print(grid, 1, row++, line,
                            editor->optical_menu_index == 2U ? hi : dim, bg);
                        (void)editor_domain_format_optical_field(
                            &extension, EDITOR_OPTICAL_FIELD_REFLECTIVITY,
                            value, sizeof(value));
                        snprintf(line, sizeof(line), " %s %-14s %s",
                            editor->optical_menu_index == 3U ? ">" : " ",
                            "Reflectivity", value);
                        grid_print(grid, 1, row++, line,
                            editor->optical_menu_index == 3U ? hi : dim, bg);
                    }
                }
            }
            if (editor->selection.type == SELECTION_FLOOR ||
                editor->selection.type == SELECTION_CEILING) {
                int map_x = editor->selection.value.horizontal.map_x;
                int map_y = editor->selection.value.horizontal.map_y;
                size_t cell_index = (size_t)map_y * (size_t)editor->document.map.width +
                                    (size_t)map_x;
                const SceneAuthoredCell *cell = &editor->document.authored_cells[cell_index];
                double height = (editor->selection.type == SELECTION_FLOOR
                    ? cell->floor_height_step : cell->ceiling_height_step) /
                    (double)SCENE_HEIGHT_STEPS_PER_UNIT;
                static const char *const gravity_names[] = {
                    "inherit", "down", "up", "north", "south", "east", "west"
                };
                double gravity_scale = cell->gravity_scale_step == 0U ? 0.0 :
                    cell->gravity_scale_step / (double)SCENE_HEIGHT_STEPS_PER_UNIT;
                snprintf(line, sizeof(line), " %s Height      %.2f  Left/Right",
                         editor->surface_field == EDITOR_SURFACE_FIELD_HEIGHT ? ">" : " ",
                         height);
                grid_print(grid, 1, row++, line,
                           editor->surface_field == EDITOR_SURFACE_FIELD_HEIGHT ? hi : dim,
                           bg);
                snprintf(line, sizeof(line), " %s Surface     %s  Enter=toggle",
                          editor->surface_field == EDITOR_SURFACE_FIELD_REMOVE ? ">" : " ",
                          (editor->selection.type == SELECTION_FLOOR
                               ? cell->floor_present : cell->ceiling_present)
                              ? "present" : "removed");
                grid_print(grid, 1, row++, line,
                            editor->surface_field == EDITOR_SURFACE_FIELD_REMOVE ? hi : dim,
                           bg);
                snprintf(line, sizeof(line), " %s Cell grav dir %s  Left/Right",
                         editor->surface_field == EDITOR_SURFACE_FIELD_GRAVITY_DIRECTION ? ">" : " ",
                         gravity_names[cell->gravity_orientation]);
                grid_print(grid, 1, row++, line,
                           editor->surface_field == EDITOR_SURFACE_FIELD_GRAVITY_DIRECTION ? hi : dim,
                           bg);
                snprintf(line, sizeof(line), " %s Cell grav x %s%.2f  Left/Right",
                         editor->surface_field == EDITOR_SURFACE_FIELD_GRAVITY_SCALE ? ">" : " ",
                         cell->gravity_scale_step == 0U ? "inherit " : "", gravity_scale);
                grid_print(grid, 1, row++, line,
                           editor->surface_field == EDITOR_SURFACE_FIELD_GRAVITY_SCALE ? hi : dim,
                           bg);
                snprintf(line, sizeof(line), " %s Map movement... Enter=open",
                         editor->surface_field == EDITOR_SURFACE_FIELD_MOVEMENT &&
                             !editor->movement_menu_open ? ">" : " ");
                grid_print(grid, 1, row++, line,
                           editor->surface_field == EDITOR_SURFACE_FIELD_MOVEMENT ? hi : dim,
                           bg);
                if (editor->has_player_cell &&
                    map_in_bounds(&editor->document.map, editor->player_map_x,
                                  editor->player_map_y)) {
                    size_t player_index = (size_t)editor->player_map_y *
                        (size_t)editor->document.map.width +
                        (size_t)editor->player_map_x;
                    const SceneAuthoredCell *player_cell =
                        &editor->document.authored_cells[player_index];
                    double delta = ((double)cell->floor_height_step -
                        (double)player_cell->floor_height_step) /
                        SCENE_HEIGHT_STEPS_PER_UNIT;
                    double clearance = (cell->ceiling_height_step -
                        cell->floor_height_step) /
                        (double)SCENE_HEIGHT_STEPS_PER_UNIT;
                    const char *traversal;
                    if (clearance < editor->document.movement.head_clearance)
                        traversal = "blocked (clearance)";
                    else if (delta <= 0.0) traversal = "flat/down";
                    else if (delta <= editor->document.movement.step_height)
                        traversal = "ramp/step";
                    else {
                        double apex = editor->document.movement.jump_impulse *
                            editor->document.movement.jump_impulse /
                            (2.0 * editor->document.movement.gravity_magnitude);
                        traversal = delta <= apex ? "jump" : "blocked (height)";
                    }
                    snprintf(line, sizeof(line), "   Traversal: %s", traversal);
                    grid_print(grid, 1, row++, line,
                               strstr(traversal, "blocked") ? warn : fg, bg);
                }
                if (editor->movement_menu_open) {
                    const SceneMovementParameters *movement = &editor->document.movement;
                    double values[EDITOR_MOVEMENT_FIELD_COUNT] = {
                        movement->gravity_magnitude,
                        (double)movement->gravity_orientation,
                        movement->step_height, movement->jump_impulse,
                        movement->air_control_scale, movement->eye_height,
                        movement->head_clearance
                    };
                    grid_print(grid, 1, row++,
                               "   MOVEMENT  Up/Down  Left/Right=edit  Esc=back",
                               fg, bg);
                    for (i = 0U; i < EDITOR_MOVEMENT_FIELD_COUNT; i++) {
                        EditorInspectorFieldPresentation metadata;
                        (void)editor_domain_movement_field_presentation(
                            (EditorMovementField)i, &metadata);
                        if (i == EDITOR_MOVEMENT_FIELD_GRAVITY_ORIENTATION) {
                            snprintf(line, sizeof(line), " %s %s: %s",
                                     editor->movement_field == (EditorMovementField)i ? ">" : " ",
                                     metadata.label,
                                     gravity_names[movement->gravity_orientation]);
                        } else {
                            snprintf(line, sizeof(line), " %s %s: %.4g",
                                     editor->movement_field == (EditorMovementField)i ? ">" : " ",
                                     metadata.label, values[i]);
                        }
                        grid_print(grid, 1, row++, line,
                                   editor->movement_field == (EditorMovementField)i ? hi : dim,
                                   bg);
                    }
                }
            }
            if (editor->decal_menu_open) {
                if (editor->decal_menu_stage == EDITOR_DECAL_MENU_LIST) {
                    size_t decals = editor_surface_decal_count(editor);
                    if (editor->selection_set.count > 1U) decals = 0U;
                    grid_print(grid, 1, row++,
                               "   DECALS  Up/Down  Enter=select  Esc=back", fg, bg);
                    for (i = 0U; i < decals; i++) {
                        const SceneDecalInstance *decal = editor_surface_decal_at(editor, i);
                        snprintf(line, sizeof(line), " %s decal:%" PRIu64 " asset:%u",
                                 editor->decal_menu_index == i ? ">" : " ",
                                 decal ? decal->id : 0U,
                                 decal ? (unsigned)decal->asset.id : 0U);
                        grid_print(grid, 1, row++, line,
                                   editor->decal_menu_index == i ? hi : dim, bg);
                    }
                    grid_print(grid, 1, row++,
                               editor->decal_menu_index == decals
                                   ? " > Add decal..." : "   Add decal...",
                               editor->decal_menu_index == decals ? hi : dim, bg);
                } else if (editor->decal_menu_stage == EDITOR_DECAL_MENU_PATTERNS) {
                    grid_print(grid, 1, row++,
                               "   PATTERN  Up/Down  Enter=place  Esc=back", fg, bg);
                    for (i = 0U; i < editor->decal_shortlist_count; i++) {
                        snprintf(line, sizeof(line), " %s pattern:%u",
                                 editor->decal_menu_index == i ? ">" : " ",
                                 (unsigned)editor->decal_shortlist[i]);
                        grid_print(grid, 1, row++, line,
                                   editor->decal_menu_index == i ? hi : dim, bg);
                    }
                    grid_print(grid, 1, row++,
                               editor->decal_menu_index == editor->decal_shortlist_count
                                   ? " > Create empty..." : "   Create empty...",
                               editor->decal_menu_index == editor->decal_shortlist_count
                                   ? hi : dim, bg);
                } else {
                    grid_print(grid, 1, row++,
                               "   CREATE EMPTY  Up/Down=field Left/Right=size",
                               fg, bg);
                    snprintf(line, sizeof(line), " %s Columns: %zu",
                             !editor->decal_create_edit_rows ? ">" : " ",
                             editor->decal_create_cols);
                    grid_print(grid, 1, row++, line,
                               !editor->decal_create_edit_rows ? hi : dim, bg);
                    snprintf(line, sizeof(line), " %s Rows:    %zu",
                             editor->decal_create_edit_rows ? ">" : " ",
                             editor->decal_create_rows);
                    grid_print(grid, 1, row++, line,
                               editor->decal_create_edit_rows ? hi : dim, bg);
                    grid_print(grid, 1, row++, "   Enter=create and place  Esc=back", dim, bg);
                }
            }
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
        } else if (editor->inspector_open &&
                   editor->inspector_kind == EDITOR_INSPECTOR_DECAL) {
            EditorInspectorPresentation presentation;
            const SceneDecalInstance *decal = scene_document_find_decal(
                &editor->document, editor->selection.value.decal.id);
            if (editor_domain_inspector_presentation(
                    EDITOR_INSPECTOR_DECAL, &presentation)) {
                snprintf(line, sizeof(line), "Inspector: %s  %s",
                         presentation.title, presentation.controls);
                grid_print(grid, 1, row++, line, fg, bg);
                for (size_t field = 0U; field < presentation.field_count; field++) {
                    EditorInspectorFieldPresentation fp;
                    char value[32];
                    bool selected = field == (size_t)editor->decal_field;
                    if (!decal || !editor_domain_decal_field_presentation(
                            (EditorDecalField)field, &fp) ||
                        !editor_domain_format_decal_field(
                            decal, (EditorDecalField)field, value, sizeof(value))) continue;
                    if (selected && editor->light_value_editing)
                        snprintf(line, sizeof(line), " > %-10s [%s_]",
                                 fp.label, editor->light_value_text);
                    else snprintf(line, sizeof(line), " %s %-10s %s",
                                  selected ? ">" : " ", fp.label, value);
                    grid_print(grid, 1, row++, line, selected ? hi : dim, bg);
                }
                if (presentation.note) grid_print(grid, 1, row++, presentation.note, warn, bg);
            }
        } else if (editor->inspector_open &&
                   editor->inspector_kind == EDITOR_INSPECTOR_SPRITE) {
            EditorInspectorPresentation presentation;
            const SceneSpriteInstance *sprite = scene_document_find_sprite(
                &editor->document, editor->selection.value.sprite.id);
            if (editor_domain_inspector_presentation(
                    EDITOR_INSPECTOR_SPRITE, &presentation)) {
                snprintf(line, sizeof(line), "Inspector: %s  %s",
                         presentation.title, presentation.controls);
                grid_print(grid, 1, row++, line, fg, bg);
                for (size_t field = 0U; field < presentation.field_count; field++) {
                    EditorInspectorFieldPresentation fp;
                    char value[32];
                    bool selected = field == (size_t)editor->sprite_field;
                    if (!sprite || !editor_domain_inspector_field_presentation(
                            EDITOR_INSPECTOR_SPRITE, field,
                            scene_document_get_map(&editor->document), &fp) ||
                        !editor_domain_format_sprite_field(
                            sprite, (EditorSpriteField)field, value, sizeof(value))) continue;
                    if (selected && editor->light_value_editing)
                        snprintf(line, sizeof(line), " > %-10s [%s_]",
                                 fp.label, editor->light_value_text);
                    else snprintf(line, sizeof(line), " %s %-10s %s",
                                   selected && !(editor->sprite_menu_open &&
                                       field == EDITOR_SPRITE_FIELD_PATTERN)
                                       ? ">" : " ", fp.label, value);
                    grid_print(grid, 1, row++, line, selected ? hi : dim, bg);
                    if (field == EDITOR_SPRITE_FIELD_PATTERN &&
                        editor->sprite_menu_open) {
                        editor_render_sprite_pattern_menu(
                            editor, grid, &row, fg, bg, dim, warn, hi);
                    }
                }
                if (presentation.note) grid_print(grid, 1, row++, presentation.note, warn, bg);
            }
        } else if (editor->inspector_open &&
                   editor->inspector_kind == EDITOR_INSPECTOR_TRIGGER) {
            EditorInspectorPresentation presentation;
            const SceneTrigger *trigger = scene_document_find_trigger(
                &editor->document, editor->selection.value.trigger.id);
            if (editor_domain_inspector_presentation(
                    EDITOR_INSPECTOR_TRIGGER, &presentation)) {
                snprintf(line, sizeof(line), "Inspector: %s  %s",
                         presentation.title, presentation.controls);
                grid_print(grid, 1, row++, line, fg, bg);
                for (size_t field = 0U; field < presentation.field_count; field++) {
                    EditorInspectorFieldPresentation fp;
                    char value[64];
                    bool selected = field == (size_t)editor->trigger_field;
                    if (!trigger || !editor_domain_trigger_field_presentation(
                            (EditorTriggerField)field, &editor->document.map, &fp) ||
                        !editor_domain_format_trigger_field(
                            trigger, (EditorTriggerField)field, value, sizeof(value))) continue;
                    snprintf(line, sizeof(line), " %s %-10s %s",
                             selected ? ">" : " ", fp.label, value);
                    grid_print(grid, 1, row++, line, selected ? hi : dim, bg);
                }
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
        } else if (editor->modal == EDITOR_MODAL_TRIGGER_REMOVE_PROMPT) {
            grid_print(grid, 1, row++, "Remove trigger? Enter=yes Esc=cancel", warn, bg);
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
        } else if (editor->modal == EDITOR_MODAL_MATERIAL_COLLISION) {
            snprintf(line, sizeof(line),
                     "Material '%s' already exists in assets. Load material?",
                     editor->material_search_text);
            grid_print(grid, 1, row++, line, warn, bg);
            grid_print(grid, 1, row++,
                       "Enter=Yes  Esc=No  O=Overwrite", dim, bg);
        } else if (editor->modal == EDITOR_MODAL_MATERIAL_OVERWRITE_PROMPT) {
            snprintf(line, sizeof(line),
                     "Overwrite '%s'? Existing asset will be lost.",
                     editor->material_search_text);
            grid_print(grid, 1, row++, line, warn, bg);
            grid_print(grid, 1, row++, "Enter=Yes  Esc=No", dim, bg);
        } else if (editor->modal == EDITOR_MODAL_LIGHT_REMOVE_PROMPT) {
            grid_print(grid, 1, row++,
                       "Remove selected light? Enter=yes  Esc=no", warn, bg);
        } else if (editor->modal == EDITOR_MODAL_DECAL_REMOVE_PROMPT) {
            grid_print(grid, 1, row++,
                       "Remove selected decal? Enter=yes  Esc=no", warn, bg);
        } else if (editor->modal == EDITOR_MODAL_SPRITE_REMOVE_PROMPT) {
            grid_print(grid, 1, row++,
                       "Remove selected sprite? Enter=yes  Esc=no", warn, bg);
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
                   "Tab=walk/edit  E=select  L=place light  P=new sprite canvas  Enter=apply",
                   dim, bg);
        grid_print(grid, 1, grid->height - 1,
                   "Ctrl+Z/Y=undo/redo  Ctrl+N=new  Ctrl+S=save  Ctrl+O=open",
                   dim, bg);
    }

}

bool unified_editor_crosshair_visible(const UnifiedEditorState *editor) {
    return editor && editor->active && unified_editor_has_document(editor) &&
           editor->modal != EDITOR_MODAL_MAP_CHOOSER &&
           editor->modal != EDITOR_MODAL_DIRTY_OPEN_PROMPT &&
           editor->modal != EDITOR_MODAL_MATERIAL_COLLISION &&
           editor->modal != EDITOR_MODAL_MATERIAL_OVERWRITE_PROMPT &&
           editor->modal != EDITOR_MENU_SAVE;
}

void unified_editor_render_overlay(const UnifiedEditorState *editor, Grid *grid) {
    unified_editor_render_text_overlay(editor, grid);
    if (unified_editor_crosshair_visible(editor)) {
        editor_crosshair_render(grid);
    }
}
