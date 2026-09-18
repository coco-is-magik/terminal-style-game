#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>

#include "../src/ui_system_action_policy.h"

static void test_frozen_context_matrix(void **state) {
    (void)state;
    assert_int_equal(ui_system_action_policy_check(
        UI_SYSTEM_ACTION_CONTEXT_PROTECTED_BOOTSTRAP, "start_project"),
        UI_SYSTEM_ACTION_POLICY_ALLOWED);
    assert_int_equal(ui_system_action_policy_check(
        UI_SYSTEM_ACTION_CONTEXT_PROTECTED_BOOTSTRAP, "resume"),
        UI_SYSTEM_ACTION_POLICY_DISALLOWED);
    assert_int_equal(ui_system_action_policy_check(
        UI_SYSTEM_ACTION_CONTEXT_PROTECTED_SETTINGS, "toggle_reduced_motion"),
        UI_SYSTEM_ACTION_POLICY_ALLOWED);
    assert_int_equal(ui_system_action_policy_check(
        UI_SYSTEM_ACTION_CONTEXT_PROTECTED_CONFIRMATION, "confirm_quit"),
        UI_SYSTEM_ACTION_POLICY_ALLOWED);
    assert_int_equal(ui_system_action_policy_check(
        UI_SYSTEM_ACTION_CONTEXT_EDITABLE_PAUSE_OVERLAY, "return_to_bootstrap"),
        UI_SYSTEM_ACTION_POLICY_ALLOWED);
    assert_int_equal(ui_system_action_policy_check(
        UI_SYSTEM_ACTION_CONTEXT_ORDINARY_PROJECT_SCREEN, "quit"),
        UI_SYSTEM_ACTION_POLICY_DISALLOWED);
    assert_int_equal(ui_system_action_policy_check(
        UI_SYSTEM_ACTION_CONTEXT_EMERGENCY_STARTUP, "start_project"),
        UI_SYSTEM_ACTION_POLICY_DISALLOWED);
    assert_int_equal(ui_system_action_policy_check(
        UI_SYSTEM_ACTION_CONTEXT_EMERGENCY_PAUSE, "resume"),
        UI_SYSTEM_ACTION_POLICY_DISALLOWED);
}

static void test_invalid_input(void **state) {
    (void)state;
    assert_int_equal(ui_system_action_policy_check(
        (UiSystemActionContext)99, "quit"),
        UI_SYSTEM_ACTION_POLICY_INVALID_ARGUMENT);
    assert_int_equal(ui_system_action_policy_check(
        UI_SYSTEM_ACTION_CONTEXT_PROTECTED_BOOTSTRAP, NULL),
        UI_SYSTEM_ACTION_POLICY_INVALID_ARGUMENT);
    assert_int_equal(ui_system_action_policy_check(
        UI_SYSTEM_ACTION_CONTEXT_PROTECTED_BOOTSTRAP, ""),
        UI_SYSTEM_ACTION_POLICY_INVALID_ARGUMENT);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_frozen_context_matrix),
        cmocka_unit_test(test_invalid_input)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}