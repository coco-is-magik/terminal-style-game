/**
 * menu_state.h — Generic menu stack for context-aware navigation
 *
 * Defines MenuId (which screen is active) and MenuStack (a push/pop
 * stack of active menus).  This module is generic: it has no knowledge
 * of AppState or input meaning.  Application-level routing (ESC behavior,
 * confirm actions) lives in app.c.
 *
 * Usage:
 *   MenuStack ms;
 *   menu_stack_init(&ms);
 *   menu_stack_push(&ms, MENU_MAIN);
 *   MenuId top = menu_stack_peek(&ms);  // MENU_MAIN
 *   menu_stack_pop(&ms);
 *   menu_stack_peek(&ms);               // MENU_NONE (empty)
 *
 * See menu_state.c for the implementation.
 */

#ifndef MENU_STATE_H
#define MENU_STATE_H

#include <stdbool.h>  /* bool */

/**
 * MenuId — Identifies a specific menu screen
 *
 * MENU_NONE is returned by peek() when the stack is empty.
 * MENU_ID_COUNT is a sentinel for array sizing; do not use as a real ID.
 */
typedef enum {
    MENU_NONE = 0,         /* No menu active (returned by peek on empty stack) */
    MENU_MAIN,             /* Main menu: Start Game, Asset Editor, Quit */
    MENU_PAUSE,            /* Pause menu: Resume, Main Menu, Quit */
    MENU_EDITOR,           /* Editor menu: Back to Editor, Main Menu */
    MENU_CONFIRM_QUIT,     /* Quit confirmation: Yes, No */
    MENU_ID_COUNT          /* Sentinel — total number of valid MenuId values */
} MenuId;

/** Maximum menus that may be simultaneously stacked (nested dialogs) */
#define MENU_STACK_MAX 8

/**
 * MenuStack — Fixed-size push/pop stack of active menus
 *
 * depth == 0  → no menu open; gameplay or editor renders
 * depth  > 0  → stack[depth-1] is the currently visible menu
 */
typedef struct {
    MenuId stack[MENU_STACK_MAX];  /* Entries, bottom at index 0 */
    int    depth;                   /* Number of entries currently in the stack */
} MenuStack;

/**
 * menu_stack_init() — Zero-initialise a MenuStack
 * @param ms  Stack to initialise (must not be NULL)
 */
void menu_stack_init(MenuStack *ms);

/**
 * menu_stack_push() — Push a new menu onto the top of the stack
 *
 * @param ms  Stack to modify
 * @param id  MenuId to push
 * @return    true on success; false if the stack is already full
 */
bool menu_stack_push(MenuStack *ms, MenuId id);

/**
 * menu_stack_pop() — Remove the top-most menu from the stack
 *
 * Generic: does not enforce any root-menu protection.
 * Application-level routing is responsible for guarding when pop
 * should be ignored (e.g. MENU_MAIN with APP_STATE_MAIN_MENU).
 *
 * @param ms  Stack to modify
 * @return    true on success; false if the stack was already empty
 */
bool menu_stack_pop(MenuStack *ms);

/**
 * menu_stack_peek() — Return the top-most menu ID without removing it
 *
 * @param ms  Stack to query
 * @return    MenuId of the active menu, or MENU_NONE if the stack is empty
 */
MenuId menu_stack_peek(const MenuStack *ms);

/**
 * menu_stack_clear() — Remove all entries from the stack (depth → 0)
 * @param ms  Stack to clear
 */
void menu_stack_clear(MenuStack *ms);

#endif /* MENU_STATE_H */
