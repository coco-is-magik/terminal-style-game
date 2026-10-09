#ifndef MENU_CONTROLLER_H
#define MENU_CONTROLLER_H

#include <stdbool.h>

#include "config.h"

typedef enum {
    MENU_ACTION_UNKNOWN = 0,
    MENU_ACTION_START_GAME,
    MENU_ACTION_OPEN_EDITOR,
    MENU_ACTION_QUIT,
    MENU_ACTION_RESUME,
    MENU_ACTION_MAIN_MENU,
    MENU_ACTION_CONFIRM_QUIT,
    MENU_ACTION_CANCEL,
    MENU_ACTION_DISCARD_CHANGES,
    MENU_ACTION_OPEN_SETTINGS,
    MENU_ACTION_UI_SCALE_DECREASE,
    MENU_ACTION_UI_SCALE_INCREASE,
    MENU_ACTION_UI_SCALE_RESET,
    MENU_ACTION_TOGGLE_REDUCED_MOTION,
    MENU_ACTION_BACK
} MenuAction;

MenuAction menu_controller_parse_action(const char *action);
void menu_controller_consume_confirm(bool *confirm_pressed,
                                     bool *editor_confirm_pressed,
                                     bool action_handled);

/** Toggles one session-only option; null is rejected without side effects. */
bool menu_controller_toggle_session_option(bool *value);

/** Resolve menu actions whose application-state transition is unconditional. */
bool menu_controller_state_transition(MenuAction action, AppState current,
                                      AppState *out_next);

/** Updates a tracked menu/focus identity and reports whether it changed. */
bool menu_controller_focus_identity_changed(int active_menu, int focus_index,
                                            int *tracked_menu,
                                            int *tracked_focus_index);

/**
 * True while a context transition owns the surface, so the menu's focus movement
 * and activation are ignored until the new state's controls are live.
 *
 * The tide covers the surface between states (change of direction recorded
 * 2026-10-09), and the recorded rule for that cover is that the controls it passes
 * over are not interactive while it is over them. `exit_until_ms` and
 * `enter_until_ms` are absolute deadlines in milliseconds; 0 (or any deadline
 * already passed) is not a transition. Pure; no I/O.
 */
bool menu_controller_transition_owns_input(double now_ms, double exit_until_ms,
                                          double enter_until_ms);

#endif
