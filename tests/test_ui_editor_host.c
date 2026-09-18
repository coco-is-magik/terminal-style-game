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

#include "../src/ui_editor_host.h"
#include "../src/ui_editor_presentation.h"

static const UiRenderTheme theme = {
    {120, 220, 160, 255}, {0, 40, 20, 255},
    {20, 20, 20, 255}, {220, 220, 120, 255},
    {120, 120, 120, 255}, {20, 20, 20, 255}
};

static void create_project(char root[64], char menus[96], char path[128]) {
    UiDocument document;
    UiElementId button;
    memcpy(root, "build/tsg_ui_host_XXXXXX", sizeof("build/tsg_ui_host_XXXXXX"));
    assert_non_null(mkdtemp(root));
    assert_true(snprintf(menus, 96, "%s/menus", root) > 0);
    assert_int_equal(mkdir(menus, 0700), 0);
    assert_true(snprintf(path, 128, "%s/menu.tui", menus) > 0);
    assert_int_equal(ui_document_create_menu(&document, "menu"), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_BUTTON,
        1U, "play", "PLAY", "play", &button), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_layout(&document, button,
        (UiDocumentLayout){2, 3, 8, 3, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_save_as(&document, path), UI_DOCUMENT_OK);
}

static void apply(UiMenuWorkspace *workspace, InputState input) {
    UiEditorHostViewport viewport = {120, 40, 40, 3};
    bool handled = false;
    assert_int_equal(ui_editor_host_apply_input(
        workspace, &input, &viewport, &handled), UI_MENU_WORKSPACE_OK);
    assert_true(handled);
}

static void test_keyboard_hosts_produce_identical_state_and_cells(void **state) {
    UiMenuWorkspace standalone;
    UiMenuWorkspace embedded;
    AssetRegistry assets;
    Grid *standalone_grid;
    Grid *embedded_grid;
    InputState input = {0};
    char root[64], menus[96], path[128];
    (void)state;
    create_project(root, menus, path);
    ui_menu_workspace_init(&standalone);
    ui_menu_workspace_init(&embedded);
    assert_int_equal(ui_menu_workspace_open(&standalone, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_open(&embedded, root), UI_MENU_WORKSPACE_OK);
    input.editor_confirm_pressed = true;
    apply(&standalone, input); apply(&embedded, input);
    memset(&input, 0, sizeof(input)); input.editor_next_pressed = true;
    apply(&standalone, input); apply(&embedded, input);
    memset(&input, 0, sizeof(input)); input.editor_confirm_pressed = true;
    apply(&standalone, input); apply(&embedded, input);
    standalone.property = UI_MENU_PROPERTY_FOCUS_EFFECT;
    embedded.property = UI_MENU_PROPERTY_FOCUS_EFFECT;
    apply(&standalone, input); apply(&embedded, input);
    memset(&input, 0, sizeof(input)); input.editor_increase_pressed = true;
    apply(&standalone, input); apply(&embedded, input);
    memset(&input, 0, sizeof(input)); input.editor_confirm_pressed = true;
    apply(&standalone, input); apply(&embedded, input);
    assert_memory_equal(&standalone.document, &embedded.document,
                        sizeof(standalone.document));
    assert_int_equal(standalone.change_cursor, embedded.change_cursor);
    assert_int_equal(standalone.change_count, embedded.change_count);
    assert_null(standalone.candidate_document);
    assert_null(embedded.candidate_document);
    assert_int_equal(ui_menu_workspace_playback_start(&standalone, 10.0),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_playback_start(&embedded, 10.0),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_playback_event(&standalone,
        UI_ANIMATION_EVENT_CONTEXT_ENTER, 0U, 20.0), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_playback_event(&embedded,
        UI_ANIMATION_EVENT_CONTEXT_ENTER, 0U, 20.0), UI_MENU_WORKSPACE_OK);
    assert_memory_equal(&standalone.playback, &embedded.playback,
                        sizeof(standalone.playback));
    assert_int_equal(standalone.playback_status, embedded.playback_status);
    assert_true(asset_registry_init(&assets));
    standalone_grid = grid_create(120, 40);
    embedded_grid = grid_create(120, 40);
    assert_non_null(standalone_grid);
    assert_non_null(embedded_grid);
    assert_true(ui_editor_presentation_render(
        &standalone, UI_MENU_WORKSPACE_OK, &assets, &theme, NULL,
        50.0, standalone_grid));
    assert_true(ui_editor_presentation_render(
        &embedded, UI_MENU_WORKSPACE_OK, &assets, &theme, NULL,
        50.0, embedded_grid));
    assert_memory_equal(standalone_grid->cells, embedded_grid->cells,
        (size_t)standalone_grid->width * (size_t)standalone_grid->height * sizeof(Cell));
    grid_destroy(standalone_grid);
    grid_destroy(embedded_grid);
    asset_registry_clear(&assets);
    ui_menu_workspace_clear(&standalone);
    ui_menu_workspace_clear(&embedded);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void test_pointer_hosts_produce_identical_history(void **state) {
    UiMenuWorkspace standalone;
    UiMenuWorkspace embedded;
    AssetRegistry assets;
    Grid *standalone_grid;
    Grid *embedded_grid;
    InputState input = {0};
    char root[64], menus[96], path[128];
    (void)state;
    create_project(root, menus, path);
    ui_menu_workspace_init(&standalone);
    ui_menu_workspace_init(&embedded);
    assert_int_equal(ui_menu_workspace_open(&standalone, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_open(&embedded, root), UI_MENU_WORKSPACE_OK);
    input.editor_confirm_pressed = true;
    apply(&standalone, input); apply(&embedded, input);
    memset(&input, 0, sizeof(input));
    input.mouse_grid_valid = true;
    input.mouse_grid_x = 42;
    input.mouse_grid_y = 6;
    input.mouse_left_pressed = true;
    apply(&standalone, input); apply(&embedded, input);
    memset(&input, 0, sizeof(input));
    input.mouse_grid_valid = true;
    input.mouse_grid_x = 44;
    input.mouse_grid_y = 8;
    input.mouse_dx = 2.0f;
    input.mouse_dy = 2.0f;
    apply(&standalone, input); apply(&embedded, input);
    input.mouse_left_released = true;
    apply(&standalone, input); apply(&embedded, input);
    assert_memory_equal(&standalone.document, &embedded.document,
                        sizeof(standalone.document));
    assert_int_equal(standalone.change_count, 1U);
    assert_int_equal(standalone.change_count, embedded.change_count);
    assert_int_equal(standalone.change_cursor, embedded.change_cursor);
    assert_true(asset_registry_init(&assets));
    standalone_grid = grid_create(120, 40);
    embedded_grid = grid_create(120, 40);
    assert_non_null(standalone_grid);
    assert_non_null(embedded_grid);
    assert_true(ui_editor_presentation_render(
        &standalone, UI_MENU_WORKSPACE_OK, &assets, &theme, NULL,
        75.0, standalone_grid));
    assert_true(ui_editor_presentation_render(
        &embedded, UI_MENU_WORKSPACE_OK, &assets, &theme, NULL,
        75.0, embedded_grid));
    assert_memory_equal(standalone_grid->cells, embedded_grid->cells,
        (size_t)standalone_grid->width * (size_t)standalone_grid->height * sizeof(Cell));
    grid_destroy(standalone_grid);
    grid_destroy(embedded_grid);
    asset_registry_clear(&assets);
    ui_menu_workspace_clear(&standalone);
    ui_menu_workspace_clear(&embedded);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void test_invalid_host_input_is_rejected(void **state) {
    UiMenuWorkspace workspace;
    InputState input = {0};
    UiEditorHostViewport viewport = {120, 40, 40, 3};
    bool handled = true;
    (void)state;
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_editor_host_apply_input(
        NULL, &input, &viewport, &handled), UI_MENU_WORKSPACE_INVALID_ARGUMENT);
    assert_int_equal(ui_editor_host_apply_input(
        &workspace, &input, NULL, &handled), UI_MENU_WORKSPACE_INVALID_ARGUMENT);
    ui_menu_workspace_clear(&workspace);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_keyboard_hosts_produce_identical_state_and_cells),
        cmocka_unit_test(test_pointer_hosts_produce_identical_history),
        cmocka_unit_test(test_invalid_host_input_is_rejected)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}