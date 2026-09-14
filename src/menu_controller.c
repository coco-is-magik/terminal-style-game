#include "menu_controller.h"
#include <string.h>

MenuAction menu_controller_parse_action(const char *action) {
    if (!action) return MENU_ACTION_UNKNOWN;
    if (strcmp(action, "start_game") == 0) return MENU_ACTION_START_GAME;
    if (strcmp(action, "open_level_editor") == 0) return MENU_ACTION_OPEN_EDITOR;
    if (strcmp(action, "quit") == 0) return MENU_ACTION_QUIT;
    if (strcmp(action, "resume") == 0) return MENU_ACTION_RESUME;
    if (strcmp(action, "return_to_main_menu") == 0) return MENU_ACTION_MAIN_MENU;
    if (strcmp(action, "confirm_quit") == 0) return MENU_ACTION_CONFIRM_QUIT;
    if (strcmp(action, "cancel") == 0) return MENU_ACTION_CANCEL;
    if (strcmp(action, "discard_changes") == 0) return MENU_ACTION_DISCARD_CHANGES;
    if (strcmp(action, "open_settings") == 0) return MENU_ACTION_OPEN_SETTINGS;
    if (strcmp(action, "ui_scale_decrease") == 0) return MENU_ACTION_UI_SCALE_DECREASE;
    if (strcmp(action, "ui_scale_increase") == 0) return MENU_ACTION_UI_SCALE_INCREASE;
    if (strcmp(action, "ui_scale_reset") == 0) return MENU_ACTION_UI_SCALE_RESET;
    if (strcmp(action, "back") == 0) return MENU_ACTION_BACK;
    return MENU_ACTION_UNKNOWN;
}

void menu_controller_consume_confirm(bool *confirm_pressed,
                                     bool *editor_confirm_pressed,
                                     bool action_handled) {
    if (!action_handled) return;
    if (confirm_pressed) *confirm_pressed = false;
    if (editor_confirm_pressed) *editor_confirm_pressed = false;
}

bool menu_controller_state_transition(MenuAction action, AppState current,
                                      AppState *out_next) {
    AppState next = current;
    if (!out_next) return false;
    switch (action) {
        case MENU_ACTION_START_GAME:
            next = APP_STATE_PLAYING;
            break;
        case MENU_ACTION_MAIN_MENU:
        case MENU_ACTION_DISCARD_CHANGES:
            next = APP_STATE_MAIN_MENU;
            break;
        default:
            return false;
    }
    *out_next = next;
    return true;
}
