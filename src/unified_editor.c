/**
 * unified_editor.c — Unified in-world editor controller
 *
 * Owns SceneDocument + CommandHistory. Material apply/undo/redo/save
 * route exclusively through command_history_* / scene_document_save.
 */

#include "unified_editor.h"
#include "editor_highlight.h"

#include "camera.h"

#include <stdio.h>
#include <string.h>

#define EDITOR_PICKER_VISIBLE 6


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
    editor->material_picker_index = 0;
    editor->highlighted_material = 0;
    editor->last_command_result = CMD_RESULT_OK;
    editor->last_save_result = SCENE_SAVE_OK;
    editor->request_exit_to_main_menu = false;
    editor->exit_choice = EDITOR_EXIT_RESUME;
    editor_clear_selection(editor);
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
    }

    editor_sync_highlighted_from_picker(editor);
}

static bool editor_document_has_unsaveable_material(
    const SceneDocument *document
) {
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

static void editor_revalidate_selection(UnifiedEditorState *editor) {
    const Map *map;

    if (!editor) {
        return;
    }
    if (editor->selection.type == SELECTION_NONE) {
        return;
    }

    map = scene_document_get_map(&editor->document);
    if (!editor_selection_is_valid_for_map(editor->selection, map)) {
        editor_clear_selection(editor);
        editor->inspector_open = false;
        editor->status = EDITOR_STATUS_INVALID_SELECTION;
    }
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
    command_history_init(&editor->history, 1);
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
    editor->assets = NULL;
    editor->active = false;
    editor_reset_session_ui(editor);
}

SceneLoadResult unified_editor_load_scene(
    UnifiedEditorState *editor,
    const char *path
) {
    if (!editor || !path) {
        return SCENE_LOAD_FILE_NOT_FOUND;
    }

    /* Snapshot UI that must survive a failed load. */
    SelectionTarget prev_selection = editor->selection;
    EditorHit prev_hover = editor->hover;
    EditorMode prev_mode = editor->mode;
    bool prev_inspector = editor->inspector_open;
    EditorModal prev_modal = editor->modal;

    SceneLoadResult result = scene_document_load(&editor->document, path);
    editor->last_load_result = result;

    if (result != SCENE_LOAD_OK) {
        editor->status = EDITOR_STATUS_LOAD_FAILED;
        editor->selection = prev_selection;
        editor->hover = prev_hover;
        editor->mode = prev_mode;
        editor->inspector_open = prev_inspector;
        editor->modal = prev_modal;
        return result;
    }

    command_history_destroy(&editor->history);
    command_history_init(&editor->history, editor->document.current_state);
    editor_clear_selection(editor);
    editor->inspector_open = false;
    editor->modal = EDITOR_MODAL_NONE;
    editor->status = EDITOR_STATUS_NONE;
    editor->request_exit_to_main_menu = false;
    editor->exit_choice = EDITOR_EXIT_RESUME;
    editor->material_picker_index = 0;
    editor->highlighted_material = 0;
    editor_sync_highlighted_from_picker(editor);
    return SCENE_LOAD_OK;
}


/* ---- Mutation wrappers (Phase 5) ------------------------------------- */

CommandResult unified_editor_set_wall_material(
    UnifiedEditorState *editor,
    MaterialId material
) {
    WallMaterialRef ref;
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

    ref = editor_wall_face_to_material_ref(editor->selection.value.wall_face);
    result = command_history_set_wall_material(
        &editor->history,
        &editor->document,
        ref,
        material
    );

    editor_map_command_result(editor, result);
    editor_revalidate_selection(editor);

    /* Live-allowed IDs above 9 must surface unsaveable immediately. */
    if ((result == CMD_RESULT_OK || result == CMD_RESULT_NO_CHANGE) &&
        material > 9) {
        editor->status = EDITOR_STATUS_UNSAVABLE_MATERIAL_ID;
    }

    return result;
}

CommandResult unified_editor_undo(UnifiedEditorState *editor) {
    CommandResult result;

    if (!editor || !editor->active) {
        return CMD_RESULT_NOTHING_TO_UNDO;
    }

    result = command_history_undo(&editor->history, &editor->document);
    editor_map_command_result(editor, result);
    editor_revalidate_selection(editor);
    return result;
}

CommandResult unified_editor_redo(UnifiedEditorState *editor) {
    CommandResult result;

    if (!editor || !editor->active) {
        return CMD_RESULT_NOTHING_TO_REDO;
    }

    result = command_history_redo(&editor->history, &editor->document);
    editor_map_command_result(editor, result);
    editor_revalidate_selection(editor);
    return result;
}

SceneSaveResult unified_editor_save(UnifiedEditorState *editor) {
    SceneSaveResult result;

    if (!editor || !editor->active) {
        return SCENE_SAVE_NO_PATH;
    }

    result = scene_document_save(&editor->document);
    editor->last_save_result = result;

    if (result == SCENE_SAVE_OK) {
        editor->status = EDITOR_STATUS_SAVED;
    } else if (result == SCENE_SAVE_UNREPRESENTABLE_MATERIAL) {
        editor->status = EDITOR_STATUS_UNSAVABLE_MATERIAL_ID;
    } else {
        editor->status = EDITOR_STATUS_SAVE_FAILED;
    }

    /* Save failure must not discard history or edits. */
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

    if (editor->inspector_open) {
        editor->inspector_open = false;
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
        editor->hover.target.type != SELECTION_WALL_FACE) {
        editor->status = EDITOR_STATUS_INVALID_SELECTION;
        return;
    }

    {
        const Map *map = scene_document_get_map(&editor->document);
        if (!editor_selection_is_valid_for_map(editor->hover.target, map)) {
            editor->status = EDITOR_STATUS_INVALID_SELECTION;
            return;
        }
    }

    editor->selection = editor->hover.target;
    editor->inspector_open = true;
    editor->status = EDITOR_STATUS_NONE;
    editor_rebuild_picker_for_selection(editor);
    editor_refresh_unsaveable_status(editor);
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
    if (!editor->inspector_open) {
        return;
    }
    if (editor->highlighted_material == 0) {
        editor->status = EDITOR_STATUS_INVALID_MATERIAL;
        return;
    }
    (void)unified_editor_set_wall_material(editor, editor->highlighted_material);
}

static void editor_handle_reload_request(UnifiedEditorState *editor) {
    if (scene_document_is_dirty(&editor->document)) {
        editor->modal = EDITOR_MODAL_RELOAD_PROMPT;
        return;
    }
    if (editor->document.path) {
        (void)unified_editor_load_scene(editor, editor->document.path);
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

        case EDITOR_EXIT_SAVE_AND_EXIT: {
            SceneSaveResult sr = unified_editor_save(editor);
            if (sr == SCENE_SAVE_OK) {
                editor->request_exit_to_main_menu = true;
                editor->modal = EDITOR_MODAL_NONE;
            } else {
                /* Failed save must not exit or discard edits. */
                editor->modal = EDITOR_MODAL_NONE;
            }
            break;
        }

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
            (void)unified_editor_load_scene(editor, path_copy);
        }
        return;
    }

    editor->modal = EDITOR_MODAL_NONE;
}


EditorInputConsumption unified_editor_update(
    UnifiedEditorState *editor,
    InputState *input,
    Camera *camera,
    double delta_seconds
) {
    EditorInputConsumption consumed = {false, false};

    if (!editor || !editor->active || !input || !camera) {
        return consumed;
    }

    (void)delta_seconds;

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
                editor->modal = EDITOR_MODAL_NONE;
                editor_mark_keyboard(&consumed);
            } else if (input->editor_confirm_pressed) {
                editor_handle_modal_confirm(editor);
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
                       input->editor_reload_pressed ||
                       input->editor_previous_pressed ||
                       input->editor_next_pressed) {
                editor_mark_keyboard(&consumed);
            }
            return consumed;
        }


        /* 3. Escape hierarchy: inspector close before exit prompt. */
        if (input->editor_cancel_pressed) {
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
        if (editor->mode == EDITOR_MODE_WALK && rmap) {
            camera_update(camera, rmap, input, delta_seconds);
        } else {
            consumed.pointer_consumed = true;
        }

        /* 6. Hover ray every frame (both modes; aim freezes in edit). */
        if (cmap) {
            EditorHit hit = editor_raycast_selection(camera, cmap);
            editor->hover = hit;
        }
    }

    /* 7. Select hovered wall. */
    if (input->editor_select_pressed && !consumed.keyboard_consumed) {
        editor_handle_select(editor, &consumed);
    }

    /* 8. Inspector picker navigation (only while open). */
    if (!consumed.keyboard_consumed && editor->inspector_open) {
        if (input->editor_previous_pressed) {
            editor_handle_picker_prev(editor);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_next_pressed) {
            editor_handle_picker_next(editor);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_confirm_pressed) {
            editor_handle_confirm_apply(editor);
            editor_mark_keyboard(&consumed);
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
            (void)unified_editor_save(editor);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_reload_pressed) {
            editor_handle_reload_request(editor);
            editor_mark_keyboard(&consumed);
        } else if (input->editor_previous_pressed ||
                   input->editor_next_pressed ||
                   input->editor_confirm_pressed) {
            /* Recognized but no-op outside inspector; still consume. */
            editor_mark_keyboard(&consumed);
        }
    }

    return consumed;
}

void unified_editor_render_overlay(
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
        } else {
            snprintf(line, sizeof(line), "Select (none)");
        }
        grid_print(grid, 1, 3, line, fg, bg);

        row = 5;
        if (editor->inspector_open) {
            size_t count = editor_count_loaded_materials(editor->assets);
            size_t start;
            size_t i;

            grid_print(grid, 1, row++,
                       "Inspector: materials  Up/Down  Enter=apply", fg, bg);
            grid_print(grid, 1, row++,
                       "NOTE: material applies to entire wall cell", warn, bg);

            if (count == 0) {
                grid_print(grid, 1, row++,
                           "  (no loaded materials)", dim, bg);
            } else {
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
                    snprintf(line, sizeof(line), " %s %3d  %s%s",
                             mark, (int)id, name,
                             id > 9 ? "  [unsavable]" : "");
                    grid_print(grid, 1, row++, line,
                               idx == editor->material_picker_index ? hi : dim,
                               bg);
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
        } else if (editor->modal == EDITOR_MODAL_RELOAD_PROMPT) {
            grid_print(grid, 1, row + 1,
                       "Reload scene? Enter=yes  Esc=cancel", warn, bg);
        }


        grid_print(grid, 1, grid->height - 2,
                   "Tab=walk/edit  E=select  Enter=apply  "
                   "Ctrl+Z/Y=undo/redo  Ctrl+S=save",
                   dim, bg);
    }

    /* UI-layer center ray indicator always wins over world highlights. */
    editor_crosshair_render(grid);
}
