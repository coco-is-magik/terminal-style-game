/**
 * unified_editor.c — Unified in-world editor controller (Phase 4 shell)
 *
 * Owns SceneDocument + CommandHistory. Material apply/undo/redo/save
 * wrappers are deferred to Phase 5 (no stub apply APIs here).
 */

#include "unified_editor.h"

#include "camera.h"

#include <stdio.h>
#include <string.h>

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
    editor_clear_selection(editor);
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

    /* Snapshot state that must survive a failed load. */
    DocumentStateId prev_current = editor->document.current_state;
    DocumentStateId prev_saved = editor->document.saved_state;
    SelectionTarget prev_selection = editor->selection;
    EditorHit prev_hover = editor->hover;
    EditorMode prev_mode = editor->mode;
    bool prev_inspector = editor->inspector_open;
    EditorModal prev_modal = editor->modal;
    size_t prev_hist_count = editor->history.count;
    size_t prev_hist_cursor = editor->history.cursor;
    DocumentStateId prev_next_id = editor->history.next_state_id;
    bool prev_dirty = scene_document_is_dirty(&editor->document);

    SceneLoadResult result = scene_document_load(&editor->document, path);
    editor->last_load_result = result;

    if (result != SCENE_LOAD_OK) {
        editor->status = EDITOR_STATUS_LOAD_FAILED;
        /* scene_document_load must not mutate on failure; assert invariants
         * by restoring UI-side fields that we never want load-fail to touch. */
        editor->selection = prev_selection;
        editor->hover = prev_hover;
        editor->mode = prev_mode;
        editor->inspector_open = prev_inspector;
        editor->modal = prev_modal;
        (void)prev_current;
        (void)prev_saved;
        (void)prev_hist_count;
        (void)prev_hist_cursor;
        (void)prev_next_id;
        (void)prev_dirty;
        return result;
    }

    command_history_destroy(&editor->history);
    command_history_init(&editor->history, editor->document.current_state);
    editor_clear_selection(editor);
    editor->inspector_open = false;
    editor->modal = EDITOR_MODAL_NONE;
    editor->status = EDITOR_STATUS_NONE;
    editor->request_exit_to_main_menu = false;
    return SCENE_LOAD_OK;
}

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
        editor_mark_keyboard(consumed);
        return;
    }

    /* Open exit prompt; confirm path is Phase 5+/menu integration. */
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

    const Map *map = scene_document_get_map(&editor->document);
    if (!editor_selection_is_valid_for_map(editor->hover.target, map)) {
        editor->status = EDITOR_STATUS_INVALID_SELECTION;
        return;
    }

    editor->selection = editor->hover.target;
    editor->inspector_open = true;
    editor->status = EDITOR_STATUS_NONE;
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

    const Map *cmap = scene_document_get_map(&editor->document);
    Map *rmap = scene_document_get_map_for_runtime(&editor->document);

    /* 2. Modal input first. */
    if (editor->modal != EDITOR_MODAL_NONE) {
        if (input->editor_cancel_pressed) {
            editor->modal = EDITOR_MODAL_NONE;
            editor_mark_keyboard(&consumed);
        } else if (input->editor_confirm_pressed) {
            if (editor->modal == EDITOR_MODAL_EXIT_PROMPT) {
                editor->request_exit_to_main_menu = true;
            }
            editor->modal = EDITOR_MODAL_NONE;
            editor_mark_keyboard(&consumed);
        } else if (input->editor_toggle_mode_pressed ||
                   input->editor_select_pressed ||
                   input->editor_undo_pressed ||
                   input->editor_redo_pressed ||
                   input->editor_save_pressed ||
                   input->editor_reload_pressed ||
                   input->editor_previous_pressed ||
                   input->editor_next_pressed) {
            /* Swallow editor shortcuts while modal is up. */
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
        /* Consume look/move intent so app layers do not double-apply. */
        consumed.pointer_consumed = true;
    }

    /* 6. Hover ray every frame (both modes; aim freezes in edit). */
    if (cmap) {
        EditorHit hit = editor_raycast_selection(camera, cmap);
        editor->hover = hit;
    }

    /* 7. Select hovered wall. */
    if (input->editor_select_pressed && !consumed.keyboard_consumed) {
        editor_handle_select(editor, &consumed);
    }

    /* Phase 4: undo/redo/save/reload/picker are recognized for consumption
     * so they do not leak to other layers, but apply is Phase 5. */
    if (!consumed.keyboard_consumed) {
        if (input->editor_undo_pressed ||
            input->editor_redo_pressed ||
            input->editor_save_pressed ||
            input->editor_reload_pressed ||
            input->editor_previous_pressed ||
            input->editor_next_pressed ||
            input->editor_confirm_pressed) {
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

    SDL_Color fg = {220, 220, 220, 255};
    SDL_Color bg = {0, 0, 0, 255};
    SDL_Color dim = {160, 160, 160, 255};
    SDL_Color warn = {220, 180, 80, 255};
    char line[128];

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
                 "Select (%d,%d) face:%s mat:%d",
                 wf->map_x, wf->map_y, editor_face_label(wf->face), mat);
    } else {
        snprintf(line, sizeof(line), "Select (none)");
    }
    grid_print(grid, 1, 3, line, fg, bg);

    if (editor->inspector_open) {
        grid_print(grid, 1, 5,
                   "Inspector: material assign is Phase 5", dim, bg);
        grid_print(grid, 1, 6,
                   "NOTE: material applies to entire wall cell", warn, bg);
    }

    const char *status = editor_status_label(editor->status);
    if (status[0] != '\0') {
        snprintf(line, sizeof(line), "Status: %s", status);
        grid_print(grid, 1, 8, line, warn, bg);
    }

    if (editor->modal == EDITOR_MODAL_EXIT_PROMPT) {
        grid_print(grid, 1, 10,
                   "Exit editor? Enter=yes  Esc=cancel", warn, bg);
    } else if (editor->modal == EDITOR_MODAL_RELOAD_PROMPT) {
        grid_print(grid, 1, 10,
                   "Reload scene? Enter=yes  Esc=cancel", warn, bg);
    }

    grid_print(grid, 1, grid->height - 2,
               "Tab=walk/edit  E=select  Esc=back", dim, bg);
}
