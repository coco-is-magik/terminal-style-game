#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>

#include <stdio.h>
#include <string.h>

#include "../src/ui_workbench.h"

static void test_open_cycle_select_and_properties(void **state) {
    UiWorkbench workbench;
    UiElement *first;
    (void)state;
    ui_workbench_init(&workbench);
    assert_int_equal(ui_workbench_open(&workbench, MENU_MAIN), UI_WORKBENCH_OK);
    assert_int_equal(workbench.element_count, workbench.layout->element_count + 1);
    assert_int_equal(workbench.elements[0]->type, UI_ELE_CONTAINER);
    first = ui_workbench_current_element(&workbench);
    assert_non_null(first);
    assert_int_equal(ui_workbench_cycle_element(&workbench, 1), UI_WORKBENCH_OK);
    assert_ptr_not_equal(ui_workbench_current_element(&workbench), first);
    assert_int_equal(ui_workbench_toggle_editing(&workbench), UI_WORKBENCH_OK);
    assert_true(workbench.editing);
    assert_int_equal(ui_workbench_cycle_property(&workbench, 1), UI_WORKBENCH_OK);
    assert_string_equal(ui_workbench_property_name(workbench.property), "transition");
    assert_non_null(ui_workbench_current_value(&workbench));
    ui_workbench_destroy(&workbench);
}

static void test_safe_navigation_and_unavailable_actions(void **state) {
    UiWorkbench workbench;
    int i;
    (void)state;
    ui_workbench_init(&workbench);
    assert_int_equal(ui_workbench_open(&workbench, MENU_MAIN), UI_WORKBENCH_OK);
    for (i = 0; i < workbench.element_count; i++) {
        UiElement *element = workbench.elements[i];
        if (element && strcmp(element->action, "open_settings") == 0) {
            workbench.element_index = i;
            break;
        }
    }
    assert_int_equal(ui_workbench_invoke(&workbench), UI_WORKBENCH_OK);
    assert_int_equal(workbench.context, MENU_SETTINGS);
    assert_int_equal(ui_workbench_open(&workbench, MENU_NONE),
                     UI_WORKBENCH_INVALID_ARGUMENT);
    ui_workbench_destroy(&workbench);
}

static void test_invalid_operations_are_nonmutating(void **state) {
    UiWorkbench workbench;
    (void)state;
    ui_workbench_init(&workbench);
    assert_int_equal(ui_workbench_cycle_element(&workbench, 1),
                     UI_WORKBENCH_INVALID_ARGUMENT);
    assert_int_equal(ui_workbench_move(&workbench, 1, 0), UI_WORKBENCH_NO_CHANGE);
    assert_int_equal(ui_workbench_cycle_value(&workbench, 1),
                     UI_WORKBENCH_NO_CHANGE);
    assert_int_equal(ui_workbench_invoke(&workbench),
                     UI_WORKBENCH_UNAVAILABLE_ACTION);
    ui_workbench_destroy(&workbench);
}

static void test_value_cycle_is_not_move_mode_gated(void **state) {
    UiWorkbench workbench;
    UiLayout layout = {0};
    UiElement element = {0};
    (void)state;
    ui_workbench_init(&workbench);
    (void)snprintf(element.name, sizeof(element.name),
                   "test_ui_workbench_missing_destination");
    element.type = UI_ELE_BUTTON;
    element.visible = 1;
    element.layout.width = 10;
    element.layout.height = 1;
    (void)snprintf(element.style, sizeof(element.style), "plain");
    (void)snprintf(element.transition, sizeof(element.transition), "none");
    (void)snprintf(element.focus_effect, sizeof(element.focus_effect), "none");
    layout.element_count = 1;
    layout.elements[0] = &element;
    workbench.layout = &layout;
    workbench.elements[0] = &element;
    workbench.element_count = 1;
    workbench.editing = false;
    workbench.property = UI_WORKBENCH_PROPERTY_STYLE;
    assert_int_equal(ui_workbench_cycle_value(&workbench, 1),
                     UI_WORKBENCH_SAVE_FAILED);
    assert_string_equal(element.style, "plain");
    workbench.layout = NULL;
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_open_cycle_select_and_properties),
        cmocka_unit_test(test_safe_navigation_and_unavailable_actions),
        cmocka_unit_test(test_invalid_operations_are_nonmutating),
        cmocka_unit_test(test_value_cycle_is_not_move_mode_gated)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}