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

#include "../src/ui_menu_workspace.h"

static void make_paths(char root[64], char menus[96]) {
    memcpy(root, "/tmp/tsg_ui_workspace_XXXXXX",
           sizeof("/tmp/tsg_ui_workspace_XXXXXX"));
    assert_non_null(mkdtemp(root));
    assert_true(snprintf(menus, 96, "%s/menus", root) > 0);
    assert_int_equal(mkdir(menus, 0700), 0);
}

static void save_menu(const char *path, const char *name) {
    UiDocument document;
    UiElementId button;
    assert_int_equal(ui_document_create_menu(&document, name), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_BUTTON,
        1U, "play", "PLAY", "play", &button), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_layout(&document, button,
        (UiDocumentLayout){2, 3, 8, 1, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_save_as(&document, path), UI_DOCUMENT_OK);
}

static void test_chooser_load_hierarchy_property_history_and_discard(void **state) {
    UiMenuWorkspace workspace;
    UiDocument reopened;
    const UiDocumentElement *element;
    char root[64];
    char menus[96];
    char path[128];
    (void)state;
    make_paths(root, menus);
    assert_true(snprintf(path, sizeof(path), "%s/a_menu.tui", menus) > 0);
    save_menu(path, "a_menu");
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    assert_true(workspace.active);
    assert_int_equal(workspace.catalog.count, 1U);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_true(workspace.has_document);
    assert_int_equal(workspace.mode, UI_MENU_WORKSPACE_HIERARCHY);
    assert_int_equal(ui_menu_workspace_next(&workspace), UI_MENU_WORKSPACE_OK);
    element = ui_menu_workspace_selected_element(&workspace);
    assert_non_null(element);
    assert_string_equal(element->name, "play");
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.mode, UI_MENU_WORKSPACE_PROPERTIES);
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 1), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(&workspace)->layout.x, 3);
    assert_true(ui_menu_workspace_is_dirty(&workspace));
    assert_int_equal(ui_menu_workspace_undo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(&workspace)->layout.x, 2);
    assert_false(ui_menu_workspace_is_dirty(&workspace));
    assert_int_equal(ui_menu_workspace_redo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_save(&workspace), UI_MENU_WORKSPACE_OK);
    assert_false(ui_menu_workspace_is_dirty(&workspace));
    assert_int_equal(ui_menu_workspace_undo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_true(ui_menu_workspace_is_dirty(&workspace));
    assert_int_equal(ui_menu_workspace_escape(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_escape(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.mode, UI_MENU_WORKSPACE_CLOSE_PROMPT);
    workspace.close_choice = UI_MENU_CLOSE_DISCARD;
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_false(workspace.has_document);
    ui_document_init(&reopened);
    assert_int_equal(ui_document_load(&reopened, path), UI_DOCUMENT_OK);
    assert_int_equal(reopened.elements[1].layout.x, 3);
    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void test_create_name_save_and_catalog_refresh(void **state) {
    UiMenuWorkspace workspace;
    char root[64];
    char menus[96];
    char path[128];
    (void)state;
    make_paths(root, menus);
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.mode, UI_MENU_WORKSPACE_CREATE_NAME);
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "new menu!"),
                     UI_MENU_WORKSPACE_OK);
    assert_string_equal(workspace.create_name, "newmenu");
    assert_int_equal(ui_menu_workspace_backspace(&workspace), UI_MENU_WORKSPACE_OK);
    assert_string_equal(workspace.create_name, "newmen");
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "u"),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_true(workspace.has_document);
    assert_true(ui_menu_workspace_is_dirty(&workspace));
    assert_int_equal(ui_menu_workspace_save(&workspace), UI_MENU_WORKSPACE_OK);
    assert_false(ui_menu_workspace_is_dirty(&workspace));
    assert_true(snprintf(path, sizeof(path), "%s/newmenu.tui", menus) > 0);
    assert_int_equal(access(path, F_OK), 0);
    assert_int_equal(ui_menu_workspace_escape(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.mode, UI_MENU_WORKSPACE_CHOOSER);
    assert_int_equal(workspace.catalog.count, 1U);
    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void test_invalid_load_and_boundaries_are_transactional(void **state) {
    UiMenuWorkspace workspace;
    UiMenuWorkspace before;
    FILE *file;
    char root[64];
    char menus[96];
    char path[128];
    (void)state;
    make_paths(root, menus);
    assert_true(snprintf(path, sizeof(path), "%s/bad.tui", menus) > 0);
    file = fopen(path, "w");
    assert_non_null(file);
    assert_true(fputs("ui_version=99\n", file) >= 0);
    assert_int_equal(fclose(file), 0);
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    before = workspace;
    assert_int_equal(ui_menu_workspace_confirm(&workspace),
                     UI_MENU_WORKSPACE_LOAD_FAILED);
    assert_memory_equal(&workspace, &before, sizeof(workspace));
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 1),
                     UI_MENU_WORKSPACE_NO_ACTION);
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 0),
                     UI_MENU_WORKSPACE_INVALID_ARGUMENT);
    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void test_catalog_open_failure_preserves_workspace(void **state) {
    UiMenuWorkspace workspace;
    UiMenuWorkspace before;
    char root[64];
    char menus[96];
    char path[128];
    char bad_root[64];
    char bad_menus[96];
    FILE *file;
    (void)state;
    make_paths(root, menus);
    assert_true(snprintf(path, sizeof(path), "%s/menu.tui", menus) > 0);
    save_menu(path, "menu");
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    before = workspace;
    memcpy(bad_root, "/tmp/tsg_ui_bad_root_XXXXXX",
           sizeof("/tmp/tsg_ui_bad_root_XXXXXX"));
    assert_non_null(mkdtemp(bad_root));
    assert_true(snprintf(bad_menus, sizeof(bad_menus), "%s/menus", bad_root) > 0);
    file = fopen(bad_menus, "w");
    assert_non_null(file);
    assert_int_equal(fclose(file), 0);
    assert_int_equal(ui_menu_workspace_open(&workspace, bad_root),
                     UI_MENU_WORKSPACE_CATALOG_FAILED);
    assert_memory_equal(&workspace, &before, sizeof(workspace));
    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
    assert_int_equal(unlink(bad_menus), 0);
    assert_int_equal(rmdir(bad_root), 0);
}

static void test_i13_construct_edit_remove_and_history(void **state) {
    UiMenuWorkspace workspace;
    UiDocument document;
    UiDocument reopened;
    UiFlowReferenceView view;
    UiElementId panel;
    UiElementId existing_button;
    UiElementId added_button;
    char root[64], menus[96], path[128];
    (void)state;
    make_paths(root, menus);
    assert_true(snprintf(path, sizeof(path), "%s/menu.tui", menus) > 0);
    assert_int_equal(ui_document_create_menu(&document, "menu"), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_CONTAINER,
        1U, "panel", "", "", &panel), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_BUTTON,
        1U, "existing", "OLD", "existing", &existing_button), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_save_as(&document, path), UI_DOCUMENT_OK);
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.element_index = 1U;
    assert_int_equal(ui_menu_workspace_open_actions(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.action_index = UI_MENU_ACTION_ADD_BUTTON;
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    added_button = ui_menu_workspace_selected_element(&workspace)->id;
    assert_string_equal(ui_menu_workspace_selected_element(&workspace)->name, "button_1");
    assert_string_equal(ui_menu_workspace_selected_element(&workspace)->flow_port, "button_1");
    assert_int_equal(workspace.document.element_count, 4U);
    assert_int_equal(ui_menu_workspace_open_actions(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.action_index = UI_MENU_ACTION_EDIT_CONTENT;
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    while (workspace.edit_text_length > 0U)
        assert_int_equal(ui_menu_workspace_backspace(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "PLAY NOW"), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_string_equal(ui_document_find_element(&workspace.document, added_button)->content,
                        "PLAY NOW");
    workspace.action_index = UI_MENU_ACTION_EDIT_PORT;
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    while (workspace.edit_text_length > 0U)
        assert_int_equal(ui_menu_workspace_backspace(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "existing"), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_MUTATION_FAILED);
    assert_string_equal(ui_document_find_element(&workspace.document, added_button)->flow_port,
                        "button_1");
    assert_int_equal(ui_menu_workspace_escape(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.action_index = UI_MENU_ACTION_EDIT_PORT;
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    while (workspace.edit_text_length > 0U)
        assert_int_equal(ui_menu_workspace_backspace(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "play"), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_save(&workspace), UI_MENU_WORKSPACE_OK);
    ui_document_init(&reopened);
    assert_int_equal(ui_document_load(&reopened, path), UI_DOCUMENT_OK);
    assert_string_equal(ui_document_find_element(&reopened, added_button)->content,
                        "PLAY NOW");
    assert_string_equal(ui_document_find_element(&reopened, added_button)->flow_port,
                        "play");
    assert_int_equal(ui_document_build_flow_reference(&reopened, &view), UI_DOCUMENT_OK);
    assert_string_equal(view.entry.ports[1], "play");
    assert_int_equal(ui_menu_workspace_request_remove(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_null(ui_document_find_element(&workspace.document, added_button));
    assert_int_equal(ui_menu_workspace_undo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_non_null(ui_document_find_element(&workspace.document, added_button));
    assert_int_equal(ui_menu_workspace_redo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_null(ui_document_find_element(&workspace.document, added_button));
    assert_true(ui_menu_workspace_is_dirty(&workspace));
    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void test_i13_container_subtree_remove_is_one_restorable_command(void **state) {
    UiMenuWorkspace workspace;
    UiDocument document;
    UiElementId container;
    UiElementId child;
    size_t before_commands;
    char root[64], menus[96], path[128];
    (void)state;
    make_paths(root, menus);
    assert_true(snprintf(path, sizeof(path), "%s/menu.tui", menus) > 0);
    assert_int_equal(ui_document_create_menu(&document, "menu"), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_save_as(&document, path), UI_DOCUMENT_OK);
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_open_actions(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.action_index = UI_MENU_ACTION_ADD_CONTAINER;
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    container = ui_menu_workspace_selected_element(&workspace)->id;
    assert_int_equal(ui_menu_workspace_open_actions(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.action_index = UI_MENU_ACTION_ADD_TEXT;
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    child = ui_menu_workspace_selected_element(&workspace)->id;
    workspace.element_index = 1U;
    before_commands = workspace.change_count;
    assert_int_equal(ui_menu_workspace_request_remove(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.change_count, before_commands + 1U);
    assert_null(ui_document_find_element(&workspace.document, container));
    assert_null(ui_document_find_element(&workspace.document, child));
    assert_int_equal(ui_menu_workspace_selected_element(&workspace)->id, 1U);
    assert_int_equal(ui_menu_workspace_undo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_non_null(ui_document_find_element(&workspace.document, container));
    assert_non_null(ui_document_find_element(&workspace.document, child));
    assert_int_equal(ui_menu_workspace_selected_element(&workspace)->id, container);
    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void test_i14_rename_reparent_and_adjacent_subtree_history(void **state) {
    UiMenuWorkspace workspace;
    UiDocument document;
    UiElementId first;
    UiElementId first_child;
    UiElementId second;
    UiElementId second_child;
    size_t commands;
    char root[64], menus[96], path[128];
    (void)state;
    make_paths(root, menus);
    assert_true(snprintf(path, sizeof(path), "%s/menu.tui", menus) > 0);
    assert_int_equal(ui_document_create_menu(&document, "menu"), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_CONTAINER,
        1U, "first", "", "", &first), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_CONTAINER,
        1U, "second", "", "", &second), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_TEXT,
        first, "first_child", "A", "", &first_child), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_TEXT,
        second, "second_child", "B", "", &second_child), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_save_as(&document, path), UI_DOCUMENT_OK);
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);

    workspace.element_index = 3U;
    assert_int_equal(ui_menu_workspace_open_actions(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.action_index = UI_MENU_ACTION_RENAME;
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.mode, UI_MENU_WORKSPACE_EDIT_NAME);
    while (workspace.edit_text_length > 0U)
        assert_int_equal(ui_menu_workspace_backspace(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "second"),
                     UI_MENU_WORKSPACE_OK);
    commands = workspace.change_count;
    assert_int_equal(ui_menu_workspace_confirm(&workspace),
                     UI_MENU_WORKSPACE_MUTATION_FAILED);
    assert_int_equal(workspace.mode, UI_MENU_WORKSPACE_EDIT_NAME);
    assert_int_equal(workspace.change_count, commands);
    while (workspace.edit_text_length > 0U)
        assert_int_equal(ui_menu_workspace_backspace(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "renamed"),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_string_equal(ui_menu_workspace_selected_element(&workspace)->name, "renamed");

    assert_int_equal(ui_menu_workspace_escape(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.mode, UI_MENU_WORKSPACE_HIERARCHY);
    assert_int_equal(ui_menu_workspace_open_actions(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.action_index = UI_MENU_ACTION_REPARENT;
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.mode, UI_MENU_WORKSPACE_REPARENT);
    assert_true(ui_menu_workspace_reparent_target_available(&workspace, 2U));
    assert_false(ui_menu_workspace_reparent_target_available(&workspace, 1U));
    assert_false(ui_menu_workspace_reparent_target_available(&workspace, 4U));
    assert_int_equal(workspace.reparent_index, 0U);
    assert_int_equal(ui_menu_workspace_next(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.reparent_index, 2U);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_document_find_element(
        &workspace.document, first_child)->parent_id, second);
    {
        size_t hierarchy[UI_DOCUMENT_MAX_ELEMENTS];
        size_t hierarchy_count = 0U;
        assert_true(ui_menu_workspace_build_hierarchy(
            &workspace, hierarchy, &hierarchy_count));
        assert_int_equal(hierarchy_count, 5U);
        assert_int_equal(workspace.document.elements[hierarchy[0]].id, 1U);
        assert_int_equal(workspace.document.elements[hierarchy[1]].id, first);
        assert_int_equal(workspace.document.elements[hierarchy[2]].id, second);
        assert_int_equal(workspace.document.elements[hierarchy[3]].id, first_child);
        assert_int_equal(workspace.document.elements[hierarchy[4]].id, second_child);
        assert_int_equal(workspace.document.elements[3].id, first_child);
        workspace.mode = UI_MENU_WORKSPACE_HIERARCHY;
        workspace.element_index = hierarchy[2];
        assert_int_equal(ui_menu_workspace_next(&workspace), UI_MENU_WORKSPACE_OK);
        assert_int_equal(ui_menu_workspace_selected_element(&workspace)->id, first_child);
    }
    assert_int_equal(ui_menu_workspace_undo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_document_find_element(
        &workspace.document, first_child)->parent_id, first);
    assert_int_equal(ui_menu_workspace_redo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_document_find_element(
        &workspace.document, first_child)->parent_id, second);

    workspace.element_index = 1U;
    assert_int_equal(ui_menu_workspace_open_actions(&workspace), UI_MENU_WORKSPACE_OK);
    assert_false(ui_menu_workspace_action_available(
        &workspace, UI_MENU_ACTION_MOVE_EARLIER));
    assert_true(ui_menu_workspace_action_available(
        &workspace, UI_MENU_ACTION_MOVE_LATER));
    workspace.action_index = UI_MENU_ACTION_MOVE_LATER;
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.document.elements[1].id, second);
    assert_int_equal(ui_menu_workspace_selected_element(&workspace)->id, first);
    assert_int_equal(ui_menu_workspace_undo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.document.elements[1].id, first);
    assert_int_equal(ui_menu_workspace_selected_element(&workspace)->id, first);
    assert_int_equal(ui_menu_workspace_redo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.document.elements[1].id, second);
    assert_non_null(ui_document_find_element(&workspace.document, second_child));
    assert_int_equal(ui_menu_workspace_save(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_document_load(&document, path), UI_DOCUMENT_OK);
    assert_string_equal(ui_document_find_element(&document, first_child)->name, "renamed");
    assert_int_equal(ui_document_find_element(&document, first_child)->parent_id, second);
    assert_int_equal(document.elements[1].id, second);
    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void test_i15_visual_properties_history_and_persistence(void **state) {
    UiMenuWorkspace workspace;
    UiDocument reopened;
    const UiDocumentElement *element;
    size_t commands;
    char root[64], menus[96], path[128];
    (void)state;
    make_paths(root, menus);
    assert_true(snprintf(path, sizeof(path), "%s/menu.tui", menus) > 0);
    save_menu(path, "menu");
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);

    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.mode, UI_MENU_WORKSPACE_PROPERTIES);
    assert_int_equal(workspace.property, UI_MENU_PROPERTY_VISUAL_MODE);
    assert_false(ui_menu_workspace_property_available(&workspace, UI_MENU_PROPERTY_X));
    assert_true(ui_menu_workspace_property_available(
        &workspace, UI_MENU_PROPERTY_FILL_ENABLED));
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 1), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.document.elements[0].visual.mode,
                     UI_DOCUMENT_VISUAL_SPRITE);
    assert_int_equal(workspace.document.elements[0].visual.sprite_id, 1U);
    assert_true(ui_menu_workspace_property_available(
        &workspace, UI_MENU_PROPERTY_SPRITE_ID));
    assert_false(ui_menu_workspace_property_available(
        &workspace, UI_MENU_PROPERTY_FILL_ENABLED));
    assert_int_equal(ui_menu_workspace_undo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.document.elements[0].visual.mode,
                     UI_DOCUMENT_VISUAL_NATIVE);
    assert_int_equal(ui_menu_workspace_escape(&workspace), UI_MENU_WORKSPACE_OK);

    assert_int_equal(ui_menu_workspace_next(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.property = UI_MENU_PROPERTY_HORIZONTAL_ANCHOR;
    assert_int_equal(ui_menu_workspace_adjust(&workspace, -1), UI_MENU_WORKSPACE_OK);
    element = ui_menu_workspace_selected_element(&workspace);
    assert_int_equal(element->layout.horizontal_anchor, UI_DOCUMENT_ANCHOR_STRETCH);
    workspace.property = UI_MENU_PROPERTY_VERTICAL_ANCHOR;
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 1), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(
        &workspace)->layout.vertical_anchor, UI_DOCUMENT_ANCHOR_CENTER);
    workspace.property = UI_MENU_PROPERTY_ALIGNMENT;
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 1), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(
        &workspace)->visual.align, UI_DOCUMENT_ALIGN_CENTER);
    workspace.property = UI_MENU_PROPERTY_FOREGROUND_RED;
    assert_int_equal(ui_menu_workspace_adjust(&workspace, -1), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(
        &workspace)->visual.foreground.red, 254U);
    workspace.property = UI_MENU_PROPERTY_BACKGROUND_BLUE;
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 1), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(
        &workspace)->visual.background.blue, 1U);
    workspace.property = UI_MENU_PROPERTY_FILL_ENABLED;
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 1), UI_MENU_WORKSPACE_OK);
    assert_true(ui_menu_workspace_selected_element(&workspace)->visual.fill_enabled);
    workspace.property = UI_MENU_PROPERTY_FILL_GLYPH;
    assert_int_equal(ui_menu_workspace_adjust(&workspace, -1), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(
        &workspace)->visual.fill_glyph, 126U);
    workspace.property = UI_MENU_PROPERTY_BORDER_ENABLED;
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 1), UI_MENU_WORKSPACE_OK);
    assert_true(ui_menu_workspace_selected_element(&workspace)->visual.border_enabled);
    workspace.property = UI_MENU_PROPERTY_BORDER_GLYPH;
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 1), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(
        &workspace)->visual.border_glyph, (uint8_t)'$');
    workspace.property = UI_MENU_PROPERTY_VISIBLE;
    assert_int_equal(ui_menu_workspace_adjust(&workspace, -1), UI_MENU_WORKSPACE_OK);
    assert_false(ui_menu_workspace_selected_element(&workspace)->visual.visible_by_default);

    workspace.property = UI_MENU_PROPERTY_VISUAL_MODE;
    commands = workspace.change_count;
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 1), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.change_count, commands + 1U);
    element = ui_menu_workspace_selected_element(&workspace);
    assert_int_equal(element->visual.mode, UI_DOCUMENT_VISUAL_SPRITE);
    assert_int_equal(element->visual.sprite_id, 1U);
    workspace.property = UI_MENU_PROPERTY_SPRITE_ID;
    assert_int_equal(ui_menu_workspace_adjust(&workspace, -1), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(
        &workspace)->visual.sprite_id, UINT8_MAX);
    assert_false(ui_menu_workspace_property_available(
        &workspace, UI_MENU_PROPERTY_ALIGNMENT));
    assert_int_equal(ui_menu_workspace_undo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(
        &workspace)->visual.sprite_id, 1U);
    assert_int_equal(ui_menu_workspace_redo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(
        &workspace)->visual.sprite_id, UINT8_MAX);
    assert_int_equal(ui_menu_workspace_save(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_document_load(&reopened, path), UI_DOCUMENT_OK);
    assert_int_equal(reopened.elements[1].visual.mode, UI_DOCUMENT_VISUAL_SPRITE);
    assert_int_equal(reopened.elements[1].visual.sprite_id, UINT8_MAX);
    assert_false(reopened.elements[1].visual.visible_by_default);
    assert_int_equal(reopened.elements[1].layout.horizontal_anchor,
                     UI_DOCUMENT_ANCHOR_STRETCH);
    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void test_i16_pointer_select_move_resize_cancel_and_history(void **state) {
    UiMenuWorkspace workspace;
    UiDocument document;
    UiDocument before_cancel;
    UiElementId back;
    UiElementId front;
    size_t commands;
    char root[64], menus[96], path[128];
    (void)state;
    make_paths(root, menus);
    assert_true(snprintf(path, sizeof(path), "%s/menu.tui", menus) > 0);
    assert_int_equal(ui_document_create_menu(&document, "menu"), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_TEXT,
        1U, "back", "BACK", "", &back), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_layout(&document, back,
        (UiDocumentLayout){2, 3, 8, 3, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_BUTTON,
        1U, "front", "FRONT", "front", &front), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_layout(&document, front,
        (UiDocumentLayout){2, 3, 8, 3, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_save_as(&document, path), UI_DOCUMENT_OK);
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);

    commands = workspace.change_count;
    assert_int_equal(ui_menu_workspace_pointer_press(&workspace, 3, 4, 80, 25),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(&workspace)->id, front);
    assert_int_equal(workspace.pointer_mode, UI_MENU_POINTER_MOVE);
    assert_int_equal(ui_menu_workspace_pointer_motion(&workspace, 6, 6),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_document_find_element(&workspace.document, front)->layout.x, 5);
    assert_int_equal(ui_document_find_element(&workspace.document, front)->layout.y, 5);
    assert_int_equal(workspace.change_count, commands);
    assert_int_equal(ui_menu_workspace_pointer_release(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.change_count, commands + 1U);
    assert_int_equal(ui_menu_workspace_undo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_document_find_element(&workspace.document, front)->layout.x, 2);
    assert_int_equal(ui_menu_workspace_redo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_document_find_element(&workspace.document, front)->layout.x, 5);

    assert_int_equal(ui_menu_workspace_pointer_press(&workspace, 12, 7, 80, 25),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.pointer_mode, UI_MENU_POINTER_RESIZE);
    assert_int_equal(ui_menu_workspace_pointer_motion(&workspace, 14, 9),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_document_find_element(&workspace.document, front)->layout.width, 10);
    assert_int_equal(ui_document_find_element(&workspace.document, front)->layout.height, 5);
    assert_int_equal(ui_menu_workspace_pointer_release(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.change_count, commands + 2U);

    before_cancel = workspace.document;
    assert_int_equal(ui_menu_workspace_pointer_press(&workspace, 6, 6, 80, 25),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_pointer_motion(&workspace, 8, 8),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_pointer_cancel(&workspace), UI_MENU_WORKSPACE_OK);
    assert_memory_equal(&workspace.document, &before_cancel, sizeof(before_cancel));
    assert_int_equal(workspace.change_count, commands + 2U);
    assert_null(workspace.pointer_before);

    assert_int_equal(ui_menu_workspace_pointer_press(&workspace, 14, 9, 80, 25),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_pointer_motion(&workspace, 0, 0),
                     UI_MENU_WORKSPACE_MUTATION_FAILED);
    assert_memory_equal(&workspace.document, &before_cancel, sizeof(before_cancel));
    ui_menu_workspace_clear(&workspace);
    assert_null(workspace.pointer_before);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void test_i17_preview_settings_are_session_only(void **state) {
    UiMenuWorkspace workspace;
    UiDocument document_before;
    size_t history_before;
    int width;
    int height;
    char root[64], menus[96], path[128];
    (void)state;
    make_paths(root, menus);
    assert_true(snprintf(path, sizeof(path), "%s/menu.tui", menus) > 0);
    save_menu(path, "menu");
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    document_before = workspace.document;
    history_before = workspace.change_count;
    assert_int_equal(ui_menu_workspace_open_actions(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.action_index = UI_MENU_ACTION_PREVIEW_SETTINGS;
    assert_true(ui_menu_workspace_action_available(
        &workspace, UI_MENU_ACTION_PREVIEW_SETTINGS));
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.mode, UI_MENU_WORKSPACE_PREVIEW_SETTINGS);
    ui_menu_workspace_preview_dimensions(&workspace, &width, &height);
    assert_int_equal(width, 80);
    assert_int_equal(height, 25);
    assert_int_equal(ui_menu_workspace_adjust(&workspace, -1), UI_MENU_WORKSPACE_OK);
    ui_menu_workspace_preview_dimensions(&workspace, &width, &height);
    assert_int_equal(width, 60);
    assert_int_equal(height, 20);
    assert_int_equal(ui_menu_workspace_next(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.preview_field, UI_MENU_PREVIEW_FIELD_SCALE);
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 1), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_preview_scale_percent(&workspace), 125);
    ui_menu_workspace_preview_dimensions(&workspace, &width, &height);
    assert_int_equal(width, 48);
    assert_int_equal(height, 16);
    assert_memory_equal(&workspace.document, &document_before, sizeof(document_before));
    assert_int_equal(workspace.change_count, history_before);
    assert_false(ui_menu_workspace_is_dirty(&workspace));
    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void test_history_capacity_evicts_oldest_without_disabling_edits(void **state) {
    UiMenuWorkspace workspace;
    char root[64], menus[96], path[128];
    size_t i;
    (void)state;
    make_paths(root, menus);
    assert_true(snprintf(path, sizeof(path), "%s/menu.tui", menus) > 0);
    save_menu(path, "menu");
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.property = UI_MENU_PROPERTY_FOREGROUND_BLUE;
    for (i = 0U; i < 255U; i++)
        assert_int_equal(ui_menu_workspace_adjust(&workspace, -1), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(
        &workspace)->visual.foreground.blue, 0U);
    assert_int_equal(workspace.change_count, UI_MENU_WORKSPACE_HISTORY_CAPACITY);
    assert_int_equal(workspace.change_cursor, UI_MENU_WORKSPACE_HISTORY_CAPACITY);
    for (i = 0U; i < UI_MENU_WORKSPACE_HISTORY_CAPACITY; i++)
        assert_int_equal(ui_menu_workspace_undo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(
        &workspace)->visual.foreground.blue, UI_MENU_WORKSPACE_HISTORY_CAPACITY);
    for (i = 0U; i < UI_MENU_WORKSPACE_HISTORY_CAPACITY; i++)
        assert_int_equal(ui_menu_workspace_redo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(
        &workspace)->visual.foreground.blue, 0U);
    assert_int_equal(ui_menu_workspace_undo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 1), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.change_count, UI_MENU_WORKSPACE_HISTORY_CAPACITY);
    assert_int_equal(ui_menu_workspace_redo(&workspace), UI_MENU_WORKSPACE_NO_ACTION);
    workspace.mode = UI_MENU_WORKSPACE_HIERARCHY;
    workspace.element_index = 1U;
    assert_int_equal(ui_menu_workspace_pointer_press(&workspace, 2, 3, 80, 25),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_pointer_motion(&workspace, 4, 4),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_pointer_release(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(&workspace)->layout.x, 4);
    assert_int_equal(ui_menu_workspace_selected_element(&workspace)->layout.y, 4);
    assert_int_equal(workspace.change_count, UI_MENU_WORKSPACE_HISTORY_CAPACITY);
    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void test_created_hierarchy_exposes_valid_reparent_destination(void **state) {
    UiMenuWorkspace workspace;
    UiElementId first_container;
    UiElementId second_container;
    UiElementId child;
    char root[64], menus[96];
    (void)state;
    make_paths(root, menus);
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "menu"),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_open_actions(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.action_index = UI_MENU_ACTION_ADD_CONTAINER;
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    first_container = ui_menu_workspace_selected_element(&workspace)->id;
    workspace.element_index = 0U;
    assert_int_equal(ui_menu_workspace_open_actions(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.action_index = UI_MENU_ACTION_ADD_CONTAINER;
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    second_container = ui_menu_workspace_selected_element(&workspace)->id;
    workspace.element_index = 1U;
    assert_int_equal(ui_menu_workspace_selected_element(&workspace)->id, first_container);
    assert_int_equal(ui_menu_workspace_open_actions(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.action_index = UI_MENU_ACTION_ADD_TEXT;
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    child = ui_menu_workspace_selected_element(&workspace)->id;
    assert_true(ui_menu_workspace_action_available(
        &workspace, UI_MENU_ACTION_REPARENT));
    assert_true(ui_menu_workspace_reparent_target_available(&workspace, 0U));
    assert_true(ui_menu_workspace_reparent_target_available(&workspace, 2U));
    assert_int_equal(workspace.mode, UI_MENU_WORKSPACE_HIERARCHY);
    assert_int_equal(ui_menu_workspace_open_actions(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.action_index = UI_MENU_ACTION_REPARENT;
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.reparent_index = 2U;
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_document_find_element(
        &workspace.document, child)->parent_id, second_container);
    ui_menu_workspace_clear(&workspace);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_chooser_load_hierarchy_property_history_and_discard),
        cmocka_unit_test(test_create_name_save_and_catalog_refresh),
        cmocka_unit_test(test_invalid_load_and_boundaries_are_transactional)
        ,cmocka_unit_test(test_catalog_open_failure_preserves_workspace)
        ,cmocka_unit_test(test_i13_construct_edit_remove_and_history)
        ,cmocka_unit_test(test_i13_container_subtree_remove_is_one_restorable_command)
        ,cmocka_unit_test(test_i14_rename_reparent_and_adjacent_subtree_history)
        ,cmocka_unit_test(test_i15_visual_properties_history_and_persistence)
        ,cmocka_unit_test(test_i16_pointer_select_move_resize_cancel_and_history)
        ,cmocka_unit_test(test_i17_preview_settings_are_session_only)
        ,cmocka_unit_test(test_history_capacity_evicts_oldest_without_disabling_edits)
        ,cmocka_unit_test(test_created_hierarchy_exposes_valid_reparent_destination)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}