#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

static void test_animation_properties_and_modes(void **state) {
    UiWorkbench workbench;
    UiLayout layout = {0};
    UiElement target = {0};
    UiElement animation = {0};
    MapCatalogEntry source = {"animation_pause_glitch.txt", NULL};
    (void)state;
    ui_workbench_init(&workbench);
    (void)snprintf(target.name, sizeof(target.name), "target");
    target.type = UI_ELE_CONTAINER;
    target.visible = 1;
    (void)snprintf(target.style, sizeof(target.style), "plain");
    (void)snprintf(target.transition, sizeof(target.transition), "none");
    (void)snprintf(target.focus_effect, sizeof(target.focus_effect), "none");
    (void)snprintf(animation.name, sizeof(animation.name), "animation");
    animation.type = UI_ELE_ANIMATION;
    animation.visible = 1;
    animation.layout.width = 10;
    animation.layout.height = 5;
    (void)snprintf(animation.style, sizeof(animation.style), "plain");
    (void)snprintf(animation.transition, sizeof(animation.transition), "none");
    (void)snprintf(animation.focus_effect, sizeof(animation.focus_effect), "none");
    (void)snprintf(animation.preset, sizeof(animation.preset), "pause_glitch");
    (void)snprintf(animation.target, sizeof(animation.target), "target");
    (void)snprintf(animation.trigger, sizeof(animation.trigger), "context_enter");
    (void)snprintf(animation.orientation, sizeof(animation.orientation), "horizontal");
    layout.element_count = 2;
    layout.elements[0] = &target;
    layout.elements[1] = &animation;
    workbench.layout = &layout;
    workbench.elements[0] = &target;
    workbench.elements[1] = &animation;
    workbench.element_count = 2;
    workbench.element_index = 1;
    workbench.property = UI_WORKBENCH_PROPERTY_STYLE;
    assert_int_equal(ui_workbench_cycle_value(&workbench, 1), UI_WORKBENCH_NO_CHANGE);
    assert_int_equal(animation.randomize, 0);
    workbench.property = UI_WORKBENCH_PROPERTY_FOCUS_EFFECT;
    assert_int_equal(ui_workbench_cycle_property(&workbench, 1), UI_WORKBENCH_OK);
    assert_int_equal(workbench.property, UI_WORKBENCH_PROPERTY_PRESET);
    assert_int_equal(ui_workbench_cycle_property(&workbench, -1), UI_WORKBENCH_OK);
    assert_int_equal(workbench.property, UI_WORKBENCH_PROPERTY_HEIGHT);
    workbench.catalog.entries = &source;
    workbench.catalog.count = 1U;
    assert_int_equal(ui_workbench_begin_add(&workbench), UI_WORKBENCH_OK);
    assert_int_equal(workbench.mode, UI_WORKBENCH_MODE_ADD);
    assert_string_equal(ui_workbench_add_source_name(&workbench),
                        "animation_pause_glitch.txt");
    ui_workbench_cancel_mode(&workbench);
    assert_int_equal(workbench.mode, UI_WORKBENCH_MODE_BROWSE);
    workbench.element_index = 0;
    assert_int_equal(ui_workbench_request_remove(&workbench),
                     UI_WORKBENCH_NO_CHANGE);
    workbench.element_index = 1;
    assert_int_equal(ui_workbench_request_remove(&workbench), UI_WORKBENCH_OK);
    assert_int_equal(workbench.mode, UI_WORKBENCH_MODE_REMOVE_CONFIRM);
    workbench.catalog.entries = NULL;
    workbench.catalog.count = 0U;
    workbench.layout = NULL;
}

static void write_fixture(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert_non_null(file);
    assert_true(fputs(text, file) >= 0);
    assert_int_equal(fclose(file), 0);
}

static void test_add_and_remove_existing_unit_in_isolated_root(void **state) {
    char root[] = "build/tsg_ui_workbench_root_XXXXXX";
    char original[1024];
    UiWorkbench workbench;
    int source_index = -1;
    int clone_index = -1;
    FILE *file;
    char bytes[1024];
    size_t read_count;
    (void)state;
    assert_non_null(getcwd(original, sizeof(original)));
    assert_non_null(mkdtemp(root));
    assert_int_equal(chdir(root), 0);
    assert_int_equal(mkdir("assets", 0700), 0);
    assert_int_equal(mkdir("assets/ui_elements", 0700), 0);
    assert_int_equal(mkdir("assets/ui_layouts", 0700), 0);
    write_fixture("assets/ui_elements/main_menu_container.txt",
        "name=main_menu_container\ntype=container\nx=10\ny=10\nwidth=20\nheight=10\n");
    write_fixture("assets/ui_elements/source_button.txt",
        "name=source_button\ntype=button\nparent=main_menu_container\n"
        "coords=relative\nx=1\ny=1\nwidth=8\nheight=1\naction=quit\ncontent=QUIT\n");
    write_fixture("assets/ui_layouts/main_menu.txt",
        "name=main_menu\ntype=layout\nelements=source_button\n");
    write_fixture("assets/ui_layouts/pause_menu.txt",
        "name=pause_menu\ntype=layout\nelements=source_button\n");
    write_fixture("assets/ui_layouts/settings.txt",
        "name=settings\ntype=layout\nelements=source_button\n");
    write_fixture("assets/ui_layouts/confirm_quit.txt",
        "name=confirm_quit\ntype=layout\nelements=source_button\n");
    write_fixture("assets/ui_layouts/master_map.txt",
        "layout=main_menu\ncache_next=main_menu_container,source_button\ndrop_after=\n"
        "layout=pause_menu\ncache_next=main_menu_container,source_button\ndrop_after=\n"
        "layout=settings\ncache_next=main_menu_container,source_button\ndrop_after=\n"
        "layout=confirm_quit\ncache_next=main_menu_container,source_button\ndrop_after=\n");
    ui_workbench_init(&workbench);
    assert_int_equal(ui_workbench_open(&workbench, MENU_MAIN), UI_WORKBENCH_OK);
    assert_int_equal(ui_workbench_begin_add(&workbench), UI_WORKBENCH_OK);
    for (size_t i = 0U; i < workbench.catalog.count; i++)
        if (strcmp(workbench.catalog.entries[i].name, "source_button.txt") == 0)
            source_index = (int)i;
    assert_true(source_index >= 0);
    workbench.add_index = (size_t)source_index;
    assert_int_equal(ui_workbench_confirm_add(&workbench), UI_WORKBENCH_OK);
    assert_non_null(access("assets/ui_elements/main_menu_source_button_1.txt", F_OK) == 0
        ? (void *)1 : NULL);
    for (int i = 0; i < workbench.element_count; i++)
        if (strcmp(workbench.elements[i]->name, "main_menu_source_button_1") == 0)
            clone_index = i;
    assert_true(clone_index >= 0);
    workbench.element_index = clone_index;
    assert_int_equal(ui_workbench_request_remove(&workbench), UI_WORKBENCH_OK);
    assert_int_equal(ui_workbench_confirm_remove(&workbench), UI_WORKBENCH_OK);
    file = fopen("assets/ui_layouts/main_menu.txt", "rb");
    assert_non_null(file);
    read_count = fread(bytes, 1U, sizeof(bytes) - 1U, file);
    assert_true(read_count > 0U);
    bytes[read_count] = '\0';
    assert_int_equal(fclose(file), 0);
    assert_null(strstr(bytes, "main_menu_source_button_1"));
    assert_int_equal(access("assets/ui_elements/source_button.txt", F_OK), 0);
    assert_int_equal(access("assets/ui_elements/main_menu_source_button_1.txt", F_OK), 0);
    ui_workbench_destroy(&workbench);
    assert_int_equal(unlink("assets/ui_elements/main_menu_source_button_1.txt"), 0);
    assert_int_equal(unlink("assets/ui_elements/source_button.txt"), 0);
    assert_int_equal(unlink("assets/ui_elements/main_menu_container.txt"), 0);
    assert_int_equal(unlink("assets/ui_layouts/main_menu.txt"), 0);
    assert_int_equal(unlink("assets/ui_layouts/pause_menu.txt"), 0);
    assert_int_equal(unlink("assets/ui_layouts/settings.txt"), 0);
    assert_int_equal(unlink("assets/ui_layouts/confirm_quit.txt"), 0);
    assert_int_equal(unlink("assets/ui_layouts/master_map.txt"), 0);
    assert_int_equal(rmdir("assets/ui_elements"), 0);
    assert_int_equal(rmdir("assets/ui_layouts"), 0);
    assert_int_equal(rmdir("assets"), 0);
    assert_int_equal(chdir(original), 0);
    assert_int_equal(rmdir(root), 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_open_cycle_select_and_properties),
        cmocka_unit_test(test_safe_navigation_and_unavailable_actions),
        cmocka_unit_test(test_invalid_operations_are_nonmutating),
        cmocka_unit_test(test_value_cycle_is_not_move_mode_gated),
        cmocka_unit_test(test_animation_properties_and_modes),
        cmocka_unit_test(test_add_and_remove_existing_unit_in_isolated_root)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}