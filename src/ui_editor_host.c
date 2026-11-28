#include "ui_editor_host.h"

#include "ui_editor_presentation.h"

UiMenuWorkspaceResult ui_editor_host_apply_input(
    UiMenuWorkspace *workspace, const InputState *input,
    const UiEditorHostViewport *viewport, const char *clipboard_text,
    bool *handled
) {
    UiEditorAction action = {0};
    UiEditorSurfaceRegion region;
    bool have_action = false;
    bool text_mode;
    int pointer_x = 0;
    int pointer_y = 0;
    bool pointer_inside;
    if (!workspace || !input || !viewport || !handled ||
        viewport->viewport_columns <= 0 || viewport->viewport_rows <= 0)
        return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    *handled = true;
    text_mode = workspace->mode == UI_MENU_WORKSPACE_CREATE_NAME ||
                workspace->mode == UI_MENU_WORKSPACE_EDIT_CONTENT ||
                workspace->mode == UI_MENU_WORKSPACE_EDIT_PORT ||
                workspace->mode == UI_MENU_WORKSPACE_EDIT_NAME;
    /* The preview's rectangle comes from the renderer's own geometry, so a press can
       only land on a cell that was painted. */
    ui_editor_presentation_preview_region(workspace, viewport->viewport_columns,
                                          viewport->viewport_rows, &region);
    pointer_inside = input->mouse_grid_valid &&
        ui_editor_surface_region_translate(&region, input->mouse_grid_x,
                                           input->mouse_grid_y, &pointer_x, &pointer_y);
    if (workspace->pointer_before) {
        if (input->editor_cancel_pressed || input->editor_ui_workspace_pressed)
            return ui_menu_workspace_pointer_cancel(workspace);
        if (input->mouse_grid_valid &&
            (input->mouse_dx != 0.0f || input->mouse_dy != 0.0f ||
             input->mouse_left_released)) {
            /* A drag may leave the preview and is clamped by the workspace, so motion
               keeps the unbounded offset rather than the bounded translation. */
            UiMenuWorkspaceResult result = ui_menu_workspace_pointer_motion(
                workspace, input->mouse_grid_x - region.origin_x,
                input->mouse_grid_y - region.origin_y);
            if (result != UI_MENU_WORKSPACE_OK) return result;
        }
        return input->mouse_left_released
            ? ui_menu_workspace_pointer_release(workspace) : UI_MENU_WORKSPACE_OK;
    }
    if (text_mode) {
        bool extend = input->editor_caret_extend_pressed;
        if (input->editor_confirm_pressed) action.type = UI_EDITOR_ACTION_CONFIRM;
        else if (input->editor_cancel_pressed || input->editor_ui_workspace_pressed)
            action.type = UI_EDITOR_ACTION_CANCEL;
        else if (input->editor_text_backspace_pressed)
            action.type = UI_EDITOR_ACTION_BACKSPACE;
        else if (input->editor_decrease_pressed)
            action.type = UI_EDITOR_ACTION_CARET_LEFT, action.extend = extend;
        else if (input->editor_increase_pressed)
            action.type = UI_EDITOR_ACTION_CARET_RIGHT, action.extend = extend;
        else if (input->editor_paste_pressed && clipboard_text) {
            action.type = UI_EDITOR_ACTION_PASTE;
            action.text = clipboard_text;
        } else if (input->editor_paste_pressed) {
            /* Rule 1: the key was pressed with nothing readable on the clipboard. Silent, and not an
               error — the editor simply does nothing. */
            return UI_MENU_WORKSPACE_NO_ACTION;
        } else if (input->text_input_len > 0) {
            /* Committed text is authoritative, so it is handled before a composition is shown: a frame can
               carry both the editing event and the commit that ended it, and the commit is what the user
               gets. Appending ends the composition as part of the same edit (D-F). */
            action.type = UI_EDITOR_ACTION_APPEND_TEXT;
            action.text = input->text_input;
        } else if (input->text_editing_active) {
            /* A composition in progress: shown after the caret and never committed. Nothing here touches
               the field, which is why cancelling one leaves no trace. */
            return ui_menu_workspace_set_preedit(workspace, input->text_editing,
                                                 (size_t)input->text_editing_len);
        } else {
            *handled = false;
            return UI_MENU_WORKSPACE_NO_ACTION;
        }
        return ui_editor_action_apply(workspace, &action);
    }
    if (input->mouse_left_pressed && input->mouse_grid_valid) {
        UiMenuWorkspaceResult result;
        if (!pointer_inside) return UI_MENU_WORKSPACE_NO_ACTION;
        result = ui_menu_workspace_pointer_press(workspace, pointer_x, pointer_y,
                                                 region.columns, region.rows);
        if (result == UI_MENU_WORKSPACE_OK && input->mouse_left_released)
            return ui_menu_workspace_pointer_release(workspace);
        return result;
    }
    if (input->editor_previous_pressed)
        action.type = UI_EDITOR_ACTION_PREVIOUS, have_action = true;
    else if (input->editor_next_pressed)
        action.type = UI_EDITOR_ACTION_NEXT, have_action = true;
    else if (input->editor_confirm_pressed)
        action.type = UI_EDITOR_ACTION_CONFIRM, have_action = true;
    else if (input->editor_cancel_pressed || input->editor_ui_workspace_pressed)
        action.type = UI_EDITOR_ACTION_CANCEL, have_action = true;
    else if (input->editor_save_pressed)
        action.type = UI_EDITOR_ACTION_SAVE, have_action = true;
    else if (input->editor_undo_pressed)
        action.type = UI_EDITOR_ACTION_UNDO, have_action = true;
    else if (input->editor_redo_pressed)
        action.type = UI_EDITOR_ACTION_REDO, have_action = true;
    else if (input->editor_decrease_pressed)
        action.type = UI_EDITOR_ACTION_DECREASE, have_action = true;
    else if (input->editor_increase_pressed)
        action.type = UI_EDITOR_ACTION_INCREASE, have_action = true;
    else if (input->editor_select_pressed)
        action.type = UI_EDITOR_ACTION_OPEN_ACTIONS, have_action = true;
    else if (input->editor_text_backspace_pressed)
        action.type = UI_EDITOR_ACTION_REMOVE, have_action = true;
    if (have_action) return ui_editor_action_apply(workspace, &action);
    *handled = false;
    return UI_MENU_WORKSPACE_NO_ACTION;
}