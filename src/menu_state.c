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

bool menu_stack_contains(const MenuStack *ms, MenuId id) {
    int i;
    if (!ms || id <= MENU_NONE || id >= MENU_ID_COUNT) return false;
    for (i = 0; i < ms->depth; i++)
        if (ms->stack[i] == id) return true;
    return false;
}

void menu_stack_clear(MenuStack *ms) {
    ms->depth = 0;
}
