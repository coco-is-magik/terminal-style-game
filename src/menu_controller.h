#ifndef MENU_CONTROLLER_H
#define MENU_CONTROLLER_H

#include <stdbool.h>

typedef enum {
    MENU_ACTION_UNKNOWN = 0,
    MENU_ACTION_START_GAME,
    MENU_ACTION_OPEN_EDITOR,
    MENU_ACTION_QUIT,
    MENU_ACTION_RESUME,
    MENU_ACTION_MAIN_MENU,
    MENU_ACTION_CONFIRM_QUIT,
    MENU_ACTION_CANCEL,
    MENU_ACTION_DISCARD_CHANGES
} MenuAction;

MenuAction menu_controller_parse_action(const char *action);
void menu_controller_consume_confirm(bool *confirm_pressed,
                                     bool *editor_confirm_pressed,
                                     bool action_handled);

#endif