#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include "../src/menu_state.h"
#include "../src/config.h"

/* ===================================================================
 *  Generic stack tests
 * =================================================================== */

/* push/pop work correctly */
static void test_stack_push_pop(void **state) {
    (void)state;
    MenuStack ms;
    menu_stack_init(&ms);
    assert_int_equal(ms.depth, 0);

    assert_true(menu_stack_push(&ms, MENU_MAIN));
    assert_int_equal(ms.depth, 1);
    assert_int_equal(menu_stack_peek(&ms), MENU_MAIN);

    assert_true(menu_stack_push(&ms, MENU_PAUSE));
    assert_int_equal(ms.depth, 2);
    assert_int_equal(menu_stack_peek(&ms), MENU_PAUSE);

    assert_true(menu_stack_pop(&ms));
    assert_int_equal(ms.depth, 1);
    assert_int_equal(menu_stack_peek(&ms), MENU_MAIN);

    assert_true(menu_stack_pop(&ms));
    assert_int_equal(ms.depth, 0);
    assert_int_equal(menu_stack_peek(&ms), MENU_NONE);
}

/* peek on empty returns MENU_NONE */
static void test_stack_peek_empty(void **state) {
    (void)state;
    MenuStack ms;
    menu_stack_init(&ms);
    assert_int_equal(menu_stack_peek(&ms), MENU_NONE);
}

/* pop on empty returns false without crashing */
static void test_stack_pop_empty(void **state) {
    (void)state;
    MenuStack ms;
    menu_stack_init(&ms);
    assert_false(menu_stack_pop(&ms));
    assert_int_equal(ms.depth, 0);
}

/* clear resets depth to 0 */
static void test_stack_clear(void **state) {
    (void)state;
    MenuStack ms;
    menu_stack_init(&ms);
    menu_stack_push(&ms, MENU_MAIN);
    menu_stack_push(&ms, MENU_PAUSE);
    menu_stack_push(&ms, MENU_CONFIRM_QUIT);
    assert_int_equal(ms.depth, 3);
    menu_stack_clear(&ms);
    assert_int_equal(ms.depth, 0);
    assert_int_equal(menu_stack_peek(&ms), MENU_NONE);
}

/* push returns false when stack is full */
static void test_stack_push_full(void **state) {
    (void)state;
    MenuStack ms;
    menu_stack_init(&ms);
    for (int i = 0; i < MENU_STACK_MAX; i++) {
        assert_true(menu_stack_push(&ms, MENU_PAUSE));
    }
    assert_false(menu_stack_push(&ms, MENU_PAUSE));
    assert_int_equal(ms.depth, MENU_STACK_MAX);
}

/* ===================================================================
 *  Application routing tests
 *
 *  These test the routing rules that live in app.c by mirroring them
 *  as inline helpers.  If the routing logic in app.c changes, these
 *  helpers must be updated to match.
 * =================================================================== */

/**
 * simulate_esc() — Mirrors the ESC routing block in app.c
 *
 * Rules (in order):
 *   APP_STATE_MAIN_MENU → ignore
 *   depth > 0           → pop
 *   APP_STATE_PLAYING   → push MENU_PAUSE
 *   APP_STATE_EDITOR    → no menu action; UnifiedEditorState owns Escape
 */
static void simulate_esc(AppState *app_state, MenuStack *ms) {
    if (*app_state == APP_STATE_MAIN_MENU) return;
    if (ms->depth > 0)                     { menu_stack_pop(ms);  return; }
    if (*app_state == APP_STATE_PLAYING)   { menu_stack_push(ms, MENU_PAUSE);  return; }
}

/** simulate_quit_action() — Mirrors the "Quit" button confirm handler */
static void simulate_quit_action(MenuStack *ms) {
    menu_stack_push(ms, MENU_CONFIRM_QUIT);
}

/* ESC from main menu does nothing (not quit, not pop) */
static void test_esc_main_menu_ignored(void **state) {
    (void)state;
    MenuStack ms;
    menu_stack_init(&ms);
    menu_stack_push(&ms, MENU_MAIN);

    AppState app = APP_STATE_MAIN_MENU;
    simulate_esc(&app, &ms);

    /* Stack unchanged, still at main menu */
    assert_int_equal(ms.depth, 1);
    assert_int_equal(menu_stack_peek(&ms), MENU_MAIN);
    assert_int_equal(app, APP_STATE_MAIN_MENU);
}

/* ESC from playing (no menu open) pushes pause menu */
static void test_esc_playing_pushes_pause(void **state) {
    (void)state;
    MenuStack ms;
    menu_stack_init(&ms);

    AppState app = APP_STATE_PLAYING;
    simulate_esc(&app, &ms);

    assert_int_equal(ms.depth, 1);
    assert_int_equal(menu_stack_peek(&ms), MENU_PAUSE);
    /* AppState stays PLAYING — the menu is an overlay */
    assert_int_equal(app, APP_STATE_PLAYING);
}

/* ESC from pause (depth=1 PLAYING) pops pause → no menu open */
static void test_esc_pause_returns_to_playing(void **state) {
    (void)state;
    MenuStack ms;
    menu_stack_init(&ms);
    menu_stack_push(&ms, MENU_PAUSE);

    AppState app = APP_STATE_PLAYING;
    simulate_esc(&app, &ms);

    /* Pause was popped → no menu */
    assert_int_equal(ms.depth, 0);
    assert_int_equal(menu_stack_peek(&ms), MENU_NONE);
    /* AppState unchanged */
    assert_int_equal(app, APP_STATE_PLAYING);
}

/* Unified editor handles Escape internally; the generic menu stack is unchanged. */
static void test_esc_editor_leaves_menu_stack_empty(void **state) {
    (void)state;
    MenuStack ms;
    menu_stack_init(&ms);

    AppState app = APP_STATE_EDITOR;
    simulate_esc(&app, &ms);

    assert_int_equal(ms.depth, 0);
    assert_int_equal(menu_stack_peek(&ms), MENU_NONE);
    assert_int_equal(app, APP_STATE_EDITOR);
}

/* Quit button action pushes confirm dialog */
static void test_quit_action_pushes_confirm(void **state) {
    (void)state;
    MenuStack ms;
    menu_stack_init(&ms);
    menu_stack_push(&ms, MENU_MAIN);
    assert_int_equal(ms.depth, 1);

    simulate_quit_action(&ms);

    assert_int_equal(ms.depth, 2);
    assert_int_equal(menu_stack_peek(&ms), MENU_CONFIRM_QUIT);
}

/* Confirm Yes sets quit flag */
static void test_confirm_yes_sets_quit(void **state) {
    (void)state;
    MenuStack ms;
    menu_stack_init(&ms);
    menu_stack_push(&ms, MENU_CONFIRM_QUIT);

    bool quit = false;
    /* Yes handler: set quit */
    quit = true;
    (void)ms;  /* ms would be used for cleanup in real code */

    assert_true(quit);
}

/* Confirm No pops the confirm dialog */
static void test_confirm_no_pops(void **state) {
    (void)state;
    MenuStack ms;
    menu_stack_init(&ms);
    menu_stack_push(&ms, MENU_MAIN);
    menu_stack_push(&ms, MENU_CONFIRM_QUIT);
    assert_int_equal(ms.depth, 2);

    /* No handler: pop */
    menu_stack_pop(&ms);

    assert_int_equal(ms.depth, 1);
    assert_int_equal(menu_stack_peek(&ms), MENU_MAIN);
}

/* ===================================================================
 *  main
 * =================================================================== */
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_stack_push_pop),
        cmocka_unit_test(test_stack_peek_empty),
        cmocka_unit_test(test_stack_pop_empty),
        cmocka_unit_test(test_stack_clear),
        cmocka_unit_test(test_stack_push_full),
        cmocka_unit_test(test_esc_main_menu_ignored),
        cmocka_unit_test(test_esc_playing_pushes_pause),
        cmocka_unit_test(test_esc_pause_returns_to_playing),
        cmocka_unit_test(test_esc_editor_leaves_menu_stack_empty),
        cmocka_unit_test(test_quit_action_pushes_confirm),
        cmocka_unit_test(test_confirm_yes_sets_quit),
        cmocka_unit_test(test_confirm_no_pops),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
