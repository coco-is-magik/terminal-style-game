/**
 * menu_state.c — Generic menu stack implementation
 *
 * All functions are simple array operations with bounds checks.
 * No application logic, no knowledge of AppState or input meaning.
 */

#include "menu_state.h"
#include <string.h>  /* memset */

void menu_stack_init(MenuStack *ms) {
    memset(ms->stack, 0, sizeof(ms->stack));
    ms->depth = 0;
}

bool menu_stack_push(MenuStack *ms, MenuId id) {
    if (ms->depth >= MENU_STACK_MAX) return false;
    ms->stack[ms->depth++] = id;
    return true;
}

bool menu_stack_pop(MenuStack *ms) {
    if (ms->depth <= 0) return false;
    ms->depth--;
    return true;
}

MenuId menu_stack_peek(const MenuStack *ms) {
    if (ms->depth <= 0) return MENU_NONE;
    return ms->stack[ms->depth - 1];
}

void menu_stack_clear(MenuStack *ms) {
    ms->depth = 0;
}
