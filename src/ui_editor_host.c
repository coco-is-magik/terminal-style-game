#include "ui_editor_host.h"

UiMenuWorkspaceResult ui_editor_host_apply_input(
    UiMenuWorkspace *workspace, const InputState *input,
    const UiEditorHostViewport *viewport, bool *handled
) {
    UiEditorAction action = {0};
    bool have_action = false;
    bool text_mode;
    int preview_width;
    int preview_height;
    if (!workspace || !input || !viewport || !handled ||
        viewport->viewport_columns <= 0 || viewport->viewport_rows <= 0)
        return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    *handled = true;
    text_mode = workspace->mode == UI_MENU_WORKSPACE_CREATE_NAME ||
                workspace->mode == UI_MENU_WORKSPACE_EDIT_CONTENT ||
                workspace->mode == UI_MENU_WORKSPACE_EDIT_PORT ||
                workspace->mode == UI_MENU_WORKSPACE_EDIT_NAME;
    ui_menu_workspace_preview_dimensions(workspace, &preview_width, &preview_height);
    if (preview_width > viewport->viewport_columns - viewport->preview_x - 1)
        preview_width = viewport->viewport_columns - viewport->preview_x - 1;
    if (preview_height > viewport->viewport_rows - viewport->preview_y - 2)
        preview_height = viewport->viewport_rows - viewport->preview_y - 2;
    if (workspace->pointer_before) {
        if (input->editor_cancel_pressed || input->editor_ui_workspace_pressed)
            return ui_menu_workspace_pointer_cancel(workspace);
        if (input->mouse_grid_valid &&
            (input->mouse_dx != 0.0f || input->mouse_dy != 0.0f ||
             input->mouse_left_released)) {
            UiMenuWorkspaceResult result = ui_menu_workspace_pointer_motion(
                workspace, input->mouse_grid_x - viewport->preview_x,
                input->mouse_grid_y - viewport->preview_y);
            if (result != UI_MENU_WORKSPACE_OK) return result;
        }
        return input->mouse_left_released
            ? ui_menu_workspace_pointer_release(workspace) : UI_MENU_WORKSPACE_OK;
    }
    if (text_mode) {
        if (input->editor_confirm_pressed) action.type = UI_EDITOR_ACTION_CONFIRM;
        else if (input->editor_cancel_pressed || input->editor_ui_workspace_pressed)
            action.type = UI_EDITOR_ACTION_CANCEL;
        else if (input->editor_text_backspace_pressed)
            action.type = UI_EDITOR_ACTION_BACKSPACE;
        else if (input->text_input_len > 0) {
            action.type = UI_EDITOR_ACTION_APPEND_TEXT;
            action.text = input->text_input;
        } else {
            *handled = false;
            return UI_MENU_WORKSPACE_NO_ACTION;
        }
        return ui_editor_action_apply(workspace, &action);
    }
    if (input->mouse_left_pressed && input->mouse_grid_valid &&
        preview_width > 0 && preview_height > 0) {
        UiMenuWorkspaceResult result = ui_menu_workspace_pointer_press(
            workspace, input->mouse_grid_x - viewport->preview_x,
            input->mouse_grid_y - viewport->preview_y, preview_width, preview_height);
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