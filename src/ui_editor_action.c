#include "ui_editor_action.h"

UiMenuWorkspaceResult ui_editor_action_apply(
    UiMenuWorkspace *workspace, const UiEditorAction *action
) {
    if (!workspace || !action) return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    switch (action->type) {
        case UI_EDITOR_ACTION_PREVIOUS: return ui_menu_workspace_previous(workspace);
        case UI_EDITOR_ACTION_NEXT: return ui_menu_workspace_next(workspace);
        case UI_EDITOR_ACTION_CONFIRM:
            if (workspace->candidate_document)
                return ui_menu_workspace_accept_candidate(workspace);
            if (workspace->mode == UI_MENU_WORKSPACE_PROPERTIES &&
                workspace->property >= UI_MENU_PROPERTY_ENTRY_EFFECT)
                return ui_menu_workspace_begin_candidate(workspace,
                                                         workspace->property);
            return ui_menu_workspace_confirm(workspace);
        case UI_EDITOR_ACTION_CANCEL:
            if (workspace->candidate_document)
                return ui_menu_workspace_cancel_candidate(workspace);
            return ui_menu_workspace_escape(workspace);
        case UI_EDITOR_ACTION_DECREASE:
            if (workspace->candidate_document)
                return ui_menu_workspace_adjust_candidate(workspace, -1);
            return ui_menu_workspace_adjust(workspace, -1);
        case UI_EDITOR_ACTION_INCREASE:
            if (workspace->candidate_document)
                return ui_menu_workspace_adjust_candidate(workspace, 1);
            return ui_menu_workspace_adjust(workspace, 1);
        case UI_EDITOR_ACTION_OPEN_ACTIONS:
            return ui_menu_workspace_open_actions(workspace);
        case UI_EDITOR_ACTION_REMOVE:
            return ui_menu_workspace_request_remove(workspace);
        case UI_EDITOR_ACTION_SAVE: return ui_menu_workspace_save(workspace);
        case UI_EDITOR_ACTION_UNDO: return ui_menu_workspace_undo(workspace);
        case UI_EDITOR_ACTION_REDO: return ui_menu_workspace_redo(workspace);
        case UI_EDITOR_ACTION_APPEND_TEXT:
            return action->text ? ui_menu_workspace_append_text(workspace, action->text)
                                : UI_MENU_WORKSPACE_INVALID_ARGUMENT;
        case UI_EDITOR_ACTION_BACKSPACE: return ui_menu_workspace_backspace(workspace);
        case UI_EDITOR_ACTION_CANDIDATE_BEGIN:
            return ui_menu_workspace_begin_candidate(workspace, action->property);
        case UI_EDITOR_ACTION_CANDIDATE_ACCEPT:
            return ui_menu_workspace_accept_candidate(workspace);
        case UI_EDITOR_ACTION_CANDIDATE_CANCEL:
            return ui_menu_workspace_cancel_candidate(workspace);
        case UI_EDITOR_ACTION_PLAYBACK_START:
            return ui_menu_workspace_playback_start(workspace, action->now_ms);
        case UI_EDITOR_ACTION_PLAYBACK_EVENT:
            return ui_menu_workspace_playback_event(workspace, action->playback_event,
                                                    action->target_id, action->now_ms);
        case UI_EDITOR_ACTION_PLAYBACK_STOP:
            return ui_menu_workspace_playback_stop(workspace);
        default: return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    }
}