/** ui_editor_action.h — Host-independent UI Scene editor input vocabulary. */
#ifndef UI_EDITOR_ACTION_H
#define UI_EDITOR_ACTION_H

#include "ui_menu_workspace.h"

typedef enum {
    UI_EDITOR_ACTION_PREVIOUS = 0,
    UI_EDITOR_ACTION_NEXT,
    UI_EDITOR_ACTION_CONFIRM,
    UI_EDITOR_ACTION_CANCEL,
    UI_EDITOR_ACTION_DECREASE,
    UI_EDITOR_ACTION_INCREASE,
    UI_EDITOR_ACTION_OPEN_ACTIONS,
    UI_EDITOR_ACTION_REMOVE,
    UI_EDITOR_ACTION_SAVE,
    UI_EDITOR_ACTION_UNDO,
    UI_EDITOR_ACTION_REDO,
    UI_EDITOR_ACTION_APPEND_TEXT,
    UI_EDITOR_ACTION_BACKSPACE,
    /* Move the caret inside a text field. `extend` is held down Shift: the selection's anchor stays and
       the caret moves, which is the only difference between "move" and "select" (V1-6 plan, I4). */
    UI_EDITOR_ACTION_CARET_LEFT,
    UI_EDITOR_ACTION_CARET_RIGHT,
    UI_EDITOR_ACTION_SELECT_ALL,
    /* Paste the text the caller supplied in `action->text` — one transaction under D-E. */
    UI_EDITOR_ACTION_PASTE,
    UI_EDITOR_ACTION_CANDIDATE_BEGIN,
    UI_EDITOR_ACTION_CANDIDATE_ACCEPT,
    UI_EDITOR_ACTION_CANDIDATE_CANCEL,
    UI_EDITOR_ACTION_PLAYBACK_START,
    UI_EDITOR_ACTION_PLAYBACK_EVENT,
    UI_EDITOR_ACTION_PLAYBACK_STOP
} UiEditorActionType;

typedef struct {
    UiEditorActionType type;
    const char *text;
    UiMenuProperty property;
    UiAnimationEvent playback_event;
    UiElementId target_id;
    double now_ms;
    /* Shift was held: affects only the caret actions, where it turns a move into a selection change. */
    bool extend;
} UiEditorAction;

UiMenuWorkspaceResult ui_editor_action_apply(
    UiMenuWorkspace *workspace, const UiEditorAction *action);

#endif /* UI_EDITOR_ACTION_H */