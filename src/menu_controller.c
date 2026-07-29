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
    return MENU_ACTION_UNKNOWN;
}

void menu_controller_consume_confirm(bool *confirm_pressed,
                                     bool *editor_confirm_pressed,
                                     bool action_handled) {
    if (!action_handled) return;
    if (confirm_pressed) *confirm_pressed = false;
    if (editor_confirm_pressed) *editor_confirm_pressed = false;
}