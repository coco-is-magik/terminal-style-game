#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define test_mkdir(path) _mkdir(path)
#else
#define test_mkdir(path) mkdir(path, 0700)
#endif
#include <unistd.h>

#include "../src/ui_menu_workspace.h"
#include "../src/ui_menu_workspace_internal.h"
#include "../src/ui_editor_action.h"

static void assert_menu_save_committed(UiMenuWorkspaceResult result) {
    assert_true(result == UI_MENU_WORKSPACE_OK ||
                result == UI_MENU_WORKSPACE_OK_DURABILITY_WARNING);
}

static void make_paths(char root[64], char menus[96]) {
    memcpy(root, "build/tsg_ui_workspace_XXXXXX",
           sizeof("build/tsg_ui_workspace_XXXXXX"));
    assert_non_null(mkdtemp(root));
    assert_true(snprintf(menus, 96, "%s/menus", root) > 0);
    assert_int_equal(test_mkdir(menus), 0);
}

/* The field's own state, which is what "the paste/keystroke changed nothing" has to mean. A whole-struct
   comparison cannot say this any more: the workspace also records *why* it refused, on purpose. */
static void assert_field_unchanged(const UiMenuWorkspace *workspace,
                                  const UiMenuWorkspace *before) {
    assert_string_equal(workspace->edit_text, before->edit_text);
    assert_int_equal(workspace->edit_text_length, before->edit_text_length);
    assert_int_equal(workspace->edit_text_cursor, before->edit_text_cursor);
    assert_int_equal(workspace->edit_text_anchor, before->edit_text_anchor);
    assert_int_equal(workspace->edit_preedit_length, before->edit_preedit_length);
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
    assert_menu_save_committed(ui_menu_workspace_save(&workspace));
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
    assert_menu_save_committed(ui_menu_workspace_save(&workspace));
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
    /* The document is untouched; the only thing that changed is the recorded reason, which is now the
       point (the field comparison would hide it). */
    assert_memory_equal(&workspace.document, &before.document, sizeof(workspace.document));
    assert_int_equal(workspace.has_document, before.has_document);
    assert_int_equal(workspace.mode, before.mode);
    /* A structural error has no position to name, so nothing is recorded as one. */
    assert_null(ui_menu_workspace_last_load_diagnostic(&workspace));
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 1),
                     UI_MENU_WORKSPACE_NO_ACTION);
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 0),
                     UI_MENU_WORKSPACE_INVALID_ARGUMENT);
    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void test_managed_menu_directory_failure_preserves_workspace(void **state) {
    UiMenuWorkspace workspace;
    UiMenuWorkspace before;
    char root[] = "build/tsg_ui_directory_XXXXXX";
    char menus[96];
    FILE *file;
    (void)state;
    assert_non_null(mkdtemp(root));
    assert_true(snprintf(menus, sizeof(menus), "%s/menus", root) > 0);
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    workspace.mode = UI_MENU_WORKSPACE_CREATE_NAME;
    memcpy(workspace.create_name, "menu", sizeof("menu"));
    workspace.create_name_length = strlen(workspace.create_name);
    before = workspace;
    assert_int_equal(ui_menu_workspace_internal_create_document(
                         &workspace, PLATFORM_FS_FAULT_ENSURE_DIRECTORY),
                     UI_MENU_WORKSPACE_SAVE_FAILED);
    assert_memory_equal(&workspace, &before, sizeof(workspace));
    assert_int_equal(access(menus, F_OK), -1);
    assert_int_equal(ui_menu_workspace_internal_create_document(
                         &workspace, PLATFORM_FS_FAULT_NONE),
                     UI_MENU_WORKSPACE_OK);
    assert_true(workspace.has_document);
    assert_string_equal(workspace.document.name, "menu");
    ui_menu_workspace_clear(&workspace);
    assert_int_equal(rmdir(menus), 0);
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    workspace.mode = UI_MENU_WORKSPACE_CREATE_NAME;
    memcpy(workspace.create_name, "menu", sizeof("menu"));
    workspace.create_name_length = strlen(workspace.create_name);
    before = workspace;
    file = fopen(menus, "wb");
    assert_non_null(file);
    assert_true(fputs("not a directory\n", file) >= 0);
    assert_int_equal(fclose(file), 0);
    assert_int_equal(ui_menu_workspace_internal_create_document(
                         &workspace, PLATFORM_FS_FAULT_NONE),
                     UI_MENU_WORKSPACE_SAVE_FAILED);
    assert_memory_equal(&workspace, &before, sizeof(workspace));
    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(menus), 0);
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
    memcpy(bad_root, "build/tsg_ui_bad_root_XXXXXX",
           sizeof("build/tsg_ui_bad_root_XXXXXX"));
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
    assert_menu_save_committed(ui_menu_workspace_save(&workspace));
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
    assert_menu_save_committed(ui_menu_workspace_save(&workspace));
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
    assert_menu_save_committed(ui_menu_workspace_save(&workspace));
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

static void test_committed_warning_updates_heap_history_identity(void **state) {
    UiMenuWorkspace workspace;
    char root[64], menus[96], path[128];
    (void)state;
    make_paths(root, menus);
    assert_true(snprintf(path, sizeof(path), "%s/menu.tui", menus) > 0);
    save_menu(path, "menu");
    ui_menu_workspace_init(&workspace);
    assert_true(ui_menu_workspace_result_is_committed(UI_MENU_WORKSPACE_OK));
    assert_true(ui_menu_workspace_result_is_committed(
        UI_MENU_WORKSPACE_OK_DURABILITY_WARNING));
    assert_false(ui_menu_workspace_result_is_committed(
        UI_MENU_WORKSPACE_SAVE_FAILED));
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_next(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 1), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.change_count, 1U);
    assert_true(ui_menu_workspace_is_dirty(&workspace));
    {
        UiMenuWorkspace before = workspace;
        assert_int_equal(ui_menu_workspace_internal_save(
                             &workspace, UI_DOCUMENT_SAVE_FAULT_SYNC),
                         UI_MENU_WORKSPACE_SAVE_FAILED);
        assert_memory_equal(&workspace, &before, sizeof(workspace));
    }
    assert_int_equal(ui_menu_workspace_internal_save(
                         &workspace, UI_DOCUMENT_SAVE_FAULT_DURABILITY),
                     UI_MENU_WORKSPACE_OK_DURABILITY_WARNING);
    assert_false(ui_menu_workspace_is_dirty(&workspace));
    assert_memory_equal(&workspace.saved_document, &workspace.document,
                        sizeof(workspace.document));
    assert_string_equal(workspace.changes[0].before->path, path);
    assert_string_equal(workspace.changes[0].after->path, path);
    assert_int_equal(workspace.changes[0].before->state.saved_state,
                     workspace.document.state.saved_state);
    assert_int_equal(workspace.changes[0].after->state.saved_state,
                     workspace.document.state.saved_state);
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 1), UI_MENU_WORKSPACE_OK);
    assert_true(ui_menu_workspace_is_dirty(&workspace));
    assert_int_equal(ui_menu_workspace_escape(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.mode, UI_MENU_WORKSPACE_HIERARCHY);
    assert_int_equal(ui_menu_workspace_escape(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.mode, UI_MENU_WORKSPACE_CLOSE_PROMPT);
    assert_int_equal(ui_menu_workspace_internal_confirm(
                         &workspace, UI_DOCUMENT_SAVE_FAULT_DURABILITY),
                     UI_MENU_WORKSPACE_OK_DURABILITY_WARNING);
    assert_false(workspace.has_document);
    assert_int_equal(workspace.mode, UI_MENU_WORKSPACE_CHOOSER);
    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}


static void apply_host_parity_trace(UiMenuWorkspace *workspace) {
    assert_int_equal(ui_menu_workspace_next(workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace->mode, UI_MENU_WORKSPACE_PROPERTIES);
    assert_int_equal(ui_menu_workspace_adjust(workspace, 1), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_next(workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_adjust(workspace, 1), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_undo(workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_redo(workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_escape(workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace->mode, UI_MENU_WORKSPACE_HIERARCHY);
}

static void test_same_workspace_trace_is_host_independent(void **state) {
    UiMenuWorkspace standalone;
    UiMenuWorkspace embedded;
    char root[64], menus[96], path[128];
    (void)state;
    make_paths(root, menus);
    assert_true(snprintf(path, sizeof(path), "%s/menu.tui", menus) > 0);
    save_menu(path, "menu");
    ui_menu_workspace_init(&standalone);
    ui_menu_workspace_init(&embedded);
    assert_int_equal(ui_menu_workspace_open(&standalone, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_open(&embedded, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&standalone), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&embedded), UI_MENU_WORKSPACE_OK);
    apply_host_parity_trace(&standalone);
    apply_host_parity_trace(&embedded);
    assert_memory_equal(&standalone.document, &embedded.document,
                        sizeof(standalone.document));
    assert_int_equal(standalone.mode, embedded.mode);
    assert_int_equal(standalone.element_index, embedded.element_index);
    assert_int_equal(standalone.property, embedded.property);
    assert_int_equal(standalone.change_count, embedded.change_count);
    assert_int_equal(standalone.change_cursor, embedded.change_cursor);
    assert_int_equal(standalone.nested_cursor.depth, embedded.nested_cursor.depth);
    assert_memory_equal(standalone.changes[0].before, embedded.changes[0].before,
                        sizeof(UiDocument));
    assert_memory_equal(standalone.changes[0].after, embedded.changes[0].after,
                        sizeof(UiDocument));
    ui_menu_workspace_clear(&standalone);
    ui_menu_workspace_clear(&embedded);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void open_workspace_document(UiMenuWorkspace *workspace,
                                    const char *root) {
    ui_menu_workspace_init(workspace);
    assert_int_equal(ui_menu_workspace_open(workspace, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(workspace), UI_MENU_WORKSPACE_OK);
}

static void test_phase3_animation_action_is_one_undoable_edit(void **state) {
    UiMenuWorkspace workspace;
    const UiDocumentElement *animation;
    char root[64], menus[96], path[128];
    (void)state;
    make_paths(root, menus);
    assert_true(snprintf(path, sizeof(path), "%s/menu.tui", menus) > 0);
    save_menu(path, "menu");
    open_workspace_document(&workspace, root);
    assert_int_equal(ui_menu_workspace_open_actions(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.action_index = UI_MENU_ACTION_ADD_ANIMATION;
    assert_true(ui_menu_workspace_action_available(
        &workspace, UI_MENU_ACTION_ADD_ANIMATION));
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    animation = ui_menu_workspace_selected_element(&workspace);
    assert_non_null(animation);
    assert_int_equal(animation->type, UI_DOCUMENT_ELEMENT_ANIMATION);
    assert_int_equal(animation->parent_id, 0U);
    assert_int_equal(animation->animation.target_id, 1U);
    assert_int_equal(workspace.change_count, 1U);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.property = UI_MENU_PROPERTY_X;
    assert_int_equal(ui_menu_workspace_adjust(&workspace, 1), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(&workspace)->layout.x, 1);
    assert_int_equal(workspace.change_count, 2U);
    assert_int_equal(ui_menu_workspace_escape(&workspace), UI_MENU_WORKSPACE_OK);
    assert_false(ui_menu_workspace_action_available(
        &workspace, UI_MENU_ACTION_REPARENT));
    assert_int_equal(ui_menu_workspace_undo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_selected_element(&workspace)->layout.x, 0);
    assert_int_equal(ui_menu_workspace_undo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.document.element_count, 2U);
    assert_int_equal(ui_menu_workspace_redo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.document.element_count, 3U);
    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void test_phase3_candidate_preview_is_nonhistorical_until_accept(void **state) {
    UiMenuWorkspace workspace;
    const UiDocumentElement *preview;
    DocumentStateId original_state;
    char root[64], menus[96], path[128];
    (void)state;
    make_paths(root, menus);
    assert_true(snprintf(path, sizeof(path), "%s/menu.tui", menus) > 0);
    save_menu(path, "menu");
    open_workspace_document(&workspace, root);
    assert_int_equal(ui_menu_workspace_next(&workspace), UI_MENU_WORKSPACE_OK);
    original_state = workspace.document.state.current_state;
    assert_int_equal(ui_menu_workspace_begin_candidate(
        &workspace, UI_MENU_PROPERTY_FOCUS_EFFECT), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_adjust_candidate(&workspace, 1),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_adjust_candidate(&workspace, 1),
                     UI_MENU_WORKSPACE_OK);
    preview = &ui_menu_workspace_preview_document(&workspace)->elements[1];
    assert_string_not_equal(preview->focus_effect,
                            workspace.document.elements[1].focus_effect);
    assert_int_equal(workspace.document.state.current_state, original_state);
    assert_int_equal(workspace.change_count, 0U);
    assert_false(ui_menu_workspace_is_dirty(&workspace));
    assert_int_equal(ui_menu_workspace_cancel_candidate(&workspace),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.change_count, 0U);
    assert_int_equal(ui_menu_workspace_begin_candidate(
        &workspace, UI_MENU_PROPERTY_FOCUS_EFFECT), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_adjust_candidate(&workspace, 1),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_accept_candidate(&workspace),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.change_count, 1U);
    assert_int_equal(workspace.change_cursor, 1U);
    assert_true(ui_menu_workspace_is_dirty(&workspace));
    assert_int_equal(ui_menu_workspace_undo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_string_equal(workspace.document.elements[1].focus_effect, "none");
    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

static void test_phase3_action_vocabulary_and_playback_are_host_independent(void **state) {
    UiMenuWorkspace direct;
    UiMenuWorkspace adapted;
    UiEditorAction action = {0};
    char root[64], menus[96], path[128];
    (void)state;
    make_paths(root, menus);
    assert_true(snprintf(path, sizeof(path), "%s/menu.tui", menus) > 0);
    save_menu(path, "menu");
    open_workspace_document(&direct, root);
    open_workspace_document(&adapted, root);
    assert_int_equal(ui_menu_workspace_next(&direct), UI_MENU_WORKSPACE_OK);
    action.type = UI_EDITOR_ACTION_NEXT;
    assert_int_equal(ui_editor_action_apply(&adapted, &action), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_begin_candidate(
        &direct, UI_MENU_PROPERTY_ACTIVATE_EFFECT), UI_MENU_WORKSPACE_OK);
    action.type = UI_EDITOR_ACTION_CANDIDATE_BEGIN;
    action.property = UI_MENU_PROPERTY_ACTIVATE_EFFECT;
    assert_int_equal(ui_editor_action_apply(&adapted, &action), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_adjust_candidate(&direct, 1),
                     UI_MENU_WORKSPACE_OK);
    action.type = UI_EDITOR_ACTION_INCREASE;
    assert_int_equal(ui_editor_action_apply(&adapted, &action), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_accept_candidate(&direct), UI_MENU_WORKSPACE_OK);
    action.type = UI_EDITOR_ACTION_CANDIDATE_ACCEPT;
    assert_int_equal(ui_editor_action_apply(&adapted, &action), UI_MENU_WORKSPACE_OK);
    assert_memory_equal(&direct.document, &adapted.document, sizeof(direct.document));
    assert_int_equal(direct.change_count, adapted.change_count);
    assert_int_equal(ui_menu_workspace_playback_start(&direct, 10.0),
                     UI_MENU_WORKSPACE_OK);
    action.type = UI_EDITOR_ACTION_PLAYBACK_START;
    action.now_ms = 10.0;
    assert_int_equal(ui_editor_action_apply(&adapted, &action), UI_MENU_WORKSPACE_OK);
    action.type = UI_EDITOR_ACTION_PLAYBACK_EVENT;
    action.playback_event = UI_ANIMATION_EVENT_CONTEXT_ENTER;
    action.target_id = 0U;
    action.now_ms = 20.0;
    assert_int_equal(ui_menu_workspace_playback_event(
        &direct, action.playback_event, action.target_id, action.now_ms),
        UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_editor_action_apply(&adapted, &action), UI_MENU_WORKSPACE_OK);
    assert_memory_equal(&direct.playback, &adapted.playback, sizeof(direct.playback));
    assert_int_equal(direct.playback_status, UI_MENU_PLAYBACK_PLAYING);
    assert_int_equal(ui_menu_workspace_playback_stop(&direct), UI_MENU_WORKSPACE_OK);
    action.type = UI_EDITOR_ACTION_PLAYBACK_STOP;
    assert_int_equal(ui_editor_action_apply(&adapted, &action), UI_MENU_WORKSPACE_OK);
    assert_int_equal(adapted.playback_status, UI_MENU_PLAYBACK_STOPPED);
    ui_menu_workspace_clear(&direct);
    ui_menu_workspace_clear(&adapted);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}

/* A workspace holding a field and nothing else. The text-field rules do not need a document, so these
   tests drive the buffer directly — which is also the only way to reach a caret mid-string, since a real
   flow always opens the field with the caret at the end. */
static void text_workspace(UiMenuWorkspace *workspace, UiMenuWorkspaceMode mode, const char *text) {
    size_t length = strlen(text);
    ui_menu_workspace_init(workspace);
    workspace->active = true;
    workspace->mode = mode;
    memcpy(workspace->edit_text, text, length + 1U);
    workspace->edit_text_length = length;
    workspace->edit_text_cursor = length;
    workspace->edit_text_anchor = length;
}

/* "a" + U+3042 + "e" + U+0301: one ASCII, one three-byte character, and a base with its combining mark.
   Seven bytes, and cluster boundaries only at 0, 1, 4 and 7 — 5 and 6 are inside the last character. */

/* An IME composition is shown, not stored: the field is untouched, so cancelling leaves no trace and
   committing adds exactly the committed text (V1-6 plan, D-F). */
static void test_preedit_is_shown_but_never_committed(void **state) {
    UiMenuWorkspace workspace;
    UiMenuWorkspace before;
    (void)state;

    text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_CONTENT, "ab");
    before = workspace;
    assert_false(ui_menu_workspace_has_preedit(&workspace));

    /* "にほん" is what the IME is offering; the field still says "ab". */
    assert_int_equal(ui_menu_workspace_set_preedit(&workspace, "\xE3\x81\xAB\xE3\x81\xBB\xE3\x82\x93",
                                                   9U),
                     UI_MENU_WORKSPACE_OK);
    assert_true(ui_menu_workspace_has_preedit(&workspace));
    assert_string_equal(workspace.edit_preedit, "\xE3\x81\xAB\xE3\x81\xBB\xE3\x82\x93");
    assert_string_equal(workspace.edit_text, "ab");
    assert_int_equal(workspace.edit_text_length, 2U);

    /* Cancelling (empty composition) discards it: the buffer is zeroed and the field cannot tell that a
       composition ever happened. (A whole-struct comparison would also cover fields this function has no
       business touching, and cmocka cannot say which byte differed.) */
    assert_int_equal(ui_menu_workspace_set_preedit(&workspace, "", 0U), UI_MENU_WORKSPACE_OK);
    assert_false(ui_menu_workspace_has_preedit(&workspace));
    assert_string_equal(workspace.edit_preedit, "");
    assert_string_equal(workspace.edit_text, before.edit_text);
    assert_int_equal(workspace.edit_text_length, before.edit_text_length);
    assert_int_equal(workspace.edit_text_cursor, before.edit_text_cursor);
    assert_int_equal(workspace.edit_text_anchor, before.edit_text_anchor);

    /* Committing is a normal append, and it clears the composition in the same call. */
    assert_int_equal(ui_menu_workspace_set_preedit(&workspace, "\xE3\x81\xAB", 3U),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "\xE3\x81\xAB"), UI_MENU_WORKSPACE_OK);
    assert_false(ui_menu_workspace_has_preedit(&workspace));
    assert_string_equal(workspace.edit_text, "ab\xE3\x81\xAB");
    assert_int_equal(workspace.edit_text_cursor, 5U);
}

/* A composition that cannot be shown in full is shown as far as a cluster boundary; it is never split. */
static void test_preedit_is_truncated_at_a_cluster_boundary(void **state) {
    UiMenuWorkspace workspace;
    char big[UI_DOCUMENT_CONTENT_CAPACITY + 8U];
    size_t i;
    (void)state;

    text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_CONTENT, "");
    /* Three-byte characters, one byte more than the buffer holds: the last one cannot fit whole. */
    for (i = 0U; i + 3U < sizeof(big); i += 3U) {
        big[i] = (char)0xE3;
        big[i + 1U] = (char)0x81;
        big[i + 2U] = (char)0x82;
    }
    big[i] = '\0';
    assert_int_equal(ui_menu_workspace_set_preedit(&workspace, big, i), UI_MENU_WORKSPACE_OK);
    assert_true(workspace.edit_preedit_length > 0U);
    assert_int_equal(workspace.edit_preedit_length % 3U, 0U);
    assert_int_equal(workspace.edit_preedit_length, (UI_DOCUMENT_CONTENT_CAPACITY - 1U) / 3U * 3U);
    /* And the field is still empty: showing a composition never writes to it. */
    assert_int_equal(workspace.edit_text_length, 0U);
}

/* Leaving the field — by Esc or by Enter — takes the composition with it, which is the "focus change must
   not leave pre-edit behind" clause. */
static void test_leaving_the_field_discards_the_composition(void **state) {
    UiMenuWorkspace workspace;
    (void)state;

    text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_CONTENT, "ab");
    assert_int_equal(ui_menu_workspace_set_preedit(&workspace, "\xE3\x81\xAB", 3U),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_escape(&workspace), UI_MENU_WORKSPACE_OK);
    assert_false(ui_menu_workspace_has_preedit(&workspace));

    /* A caret move ends it too: the composition was anchored where the caret was. Reopen the field,
       since Esc left it. */
    workspace.mode = UI_MENU_WORKSPACE_EDIT_CONTENT;
    assert_int_equal(ui_menu_workspace_set_preedit(&workspace, "\xE3\x81\xAB", 3U),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_move_caret(&workspace, -1, false), UI_MENU_WORKSPACE_OK);
    assert_false(ui_menu_workspace_has_preedit(&workspace));

    /* Outside a text mode there is no field to compose into. */
    workspace.mode = UI_MENU_WORKSPACE_HIERARCHY;
    assert_int_equal(ui_menu_workspace_set_preedit(&workspace, "\xE3\x81\xAB", 3U),
                     UI_MENU_WORKSPACE_NO_ACTION);
}


#define CLUSTER_TEXT "a\xE3\x81\x82" "e\xCC\x81"

static void test_caret_moves_by_cluster_not_by_byte(void **state) {
    UiMenuWorkspace workspace;
    (void)state;

    text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_CONTENT, CLUSTER_TEXT);
    assert_int_equal(workspace.edit_text_length, 7U);

    /* Left from the end skips the combining mark: it belongs to the 'e' before it. */
    assert_int_equal(ui_menu_workspace_move_caret(&workspace, -1, false), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.edit_text_cursor, 4U);
    /* Left again lands before the three-byte character, not inside it. */
    assert_int_equal(ui_menu_workspace_move_caret(&workspace, -1, false), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.edit_text_cursor, 1U);
    assert_int_equal(ui_menu_workspace_move_caret(&workspace, -1, false), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.edit_text_cursor, 0U);
    /* At the start there is nowhere to go, and that is not an error. */
    assert_int_equal(ui_menu_workspace_move_caret(&workspace, -1, false), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.edit_text_cursor, 0U);

    assert_int_equal(ui_menu_workspace_move_caret(&workspace, 1, false), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.edit_text_cursor, 1U);
    assert_int_equal(ui_menu_workspace_move_caret(&workspace, 1, false), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.edit_text_cursor, 4U);
    assert_int_equal(ui_menu_workspace_move_caret(&workspace, 1, false), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.edit_text_cursor, 7U);
    assert_int_equal(ui_menu_workspace_move_caret(&workspace, 1, false), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.edit_text_cursor, 7U);
    assert_false(ui_menu_workspace_has_selection(&workspace));
}

static void test_selection_extends_with_shift_and_collapses_without(void **state) {
    UiMenuWorkspace workspace;
    (void)state;

    text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_CONTENT, CLUSTER_TEXT);

    /* Shift+Left from the end selects the last cluster. */
    assert_int_equal(ui_menu_workspace_move_caret(&workspace, -1, true), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.edit_text_cursor, 4U);
    assert_int_equal(workspace.edit_text_anchor, 7U);
    assert_true(ui_menu_workspace_has_selection(&workspace));

    /* Shift+Left again grows it backwards over the Japanese character. */
    assert_int_equal(ui_menu_workspace_move_caret(&workspace, -1, true), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.edit_text_cursor, 1U);
    assert_int_equal(ui_menu_workspace_move_caret(&workspace, -1, true), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.edit_text_cursor, 0U);
    assert_int_equal(workspace.edit_text_anchor, 7U);

    /* A plain arrow collapses to the edge it heads for instead of stepping from the far end. */
    assert_int_equal(ui_menu_workspace_move_caret(&workspace, 1, false), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.edit_text_cursor, 7U);
    assert_false(ui_menu_workspace_has_selection(&workspace));

    /* Select-all is the one selection a user cannot reach by arrow keys alone. */
    assert_int_equal(ui_menu_workspace_select_all(&workspace, true), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.edit_text_anchor, 0U);
    assert_int_equal(workspace.edit_text_cursor, 7U);
    assert_int_equal(ui_menu_workspace_select_all(&workspace, false), UI_MENU_WORKSPACE_OK);
    assert_false(ui_menu_workspace_has_selection(&workspace));
}
static void test_typing_inserts_at_the_caret_and_replaces_a_selection(void **state) {
    UiMenuWorkspace workspace;
    (void)state;

    text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_CONTENT, "abc");
    assert_int_equal(ui_menu_workspace_move_caret(&workspace, -1, false), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_move_caret(&workspace, -1, false), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "Z"), UI_MENU_WORKSPACE_OK);
    assert_string_equal(workspace.edit_text, "aZbc");
    assert_int_equal(workspace.edit_text_cursor, 2U);
    assert_false(ui_menu_workspace_has_selection(&workspace));

    /* Typing over a selection replaces it, and the caret lands after the replacement. */
    assert_int_equal(ui_menu_workspace_select_all(&workspace, true), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "\xE3\x81\x82"), UI_MENU_WORKSPACE_OK);
    assert_string_equal(workspace.edit_text, "\xE3\x81\x82");
    assert_int_equal(workspace.edit_text_cursor, 3U);
    assert_false(ui_menu_workspace_has_selection(&workspace));
}

static void test_backspace_deletes_a_selection_or_one_cluster(void **state) {
    UiMenuWorkspace workspace;
    (void)state;

    /* One cluster goes, all three bytes of it. */
    text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_CONTENT, "a\xE3\x81\x82");
    assert_int_equal(ui_menu_workspace_backspace(&workspace), UI_MENU_WORKSPACE_OK);
    assert_string_equal(workspace.edit_text, "a");
    assert_int_equal(workspace.edit_text_length, 1U);
    assert_int_equal(ui_menu_workspace_backspace(&workspace), UI_MENU_WORKSPACE_OK);
    assert_string_equal(workspace.edit_text, "");
    /* An empty field has nothing to delete, which is NO_ACTION rather than an error. */
    assert_int_equal(ui_menu_workspace_backspace(&workspace), UI_MENU_WORKSPACE_NO_ACTION);

    /* With a selection, Backspace takes the selection and nothing else. */
    text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_CONTENT, "abcdef");
    assert_int_equal(ui_menu_workspace_move_caret(&workspace, -1, true), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_move_caret(&workspace, -1, true), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_backspace(&workspace), UI_MENU_WORKSPACE_OK);
    assert_string_equal(workspace.edit_text, "abcd");
    assert_int_equal(workspace.edit_text_cursor, 4U);
    assert_false(ui_menu_workspace_has_selection(&workspace));
}

/* The two field kinds refuse differently, and both refusals are visible to the caller rather than
   silent: content is refused whole, an identifier filters to what it can use. */
static void test_content_refuses_what_the_document_would_and_identifiers_filter(void **state) {
    UiMenuWorkspace workspace;
    (void)state;

    text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_CONTENT, "");
    /* A control character, an RTL letter and a backslash are all things `ui_text` refuses (I3). */
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "a\x01"), UI_MENU_WORKSPACE_REJECTED_TEXT);
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "\xD7\x90"), UI_MENU_WORKSPACE_REJECTED_TEXT);
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "a\\b"), UI_MENU_WORKSPACE_REJECTED_TEXT);
    assert_string_equal(workspace.edit_text, "");
    /* Japanese is not refused: this is the point of the whole phase. */
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "HP\xE3\x81\x82"),
                     UI_MENU_WORKSPACE_OK);
    assert_string_equal(workspace.edit_text, "HP\xE3\x81\x82");

    /* A port name is an identifier: the usable characters survive, the rest are dropped. */
    text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_PORT, "");
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "play now!"), UI_MENU_WORKSPACE_OK);
    assert_string_equal(workspace.edit_text, "playnow");
    /* A multi-byte character is dropped whole rather than as three stray bytes. */
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "\xE3\x81\x82"),
                     UI_MENU_WORKSPACE_NO_ACTION);
    assert_string_equal(workspace.edit_text, "playnow");
}

/* Capacity refuses the keystroke instead of shortening it, and says so. */
static void test_field_at_capacity_refuses_without_splitting(void **state) {
    UiMenuWorkspace workspace;
    char full[UI_DOCUMENT_CONTENT_CAPACITY];
    UiMenuWorkspace before;
    (void)state;

    memset(full, 'x', sizeof(full) - 1U);
    full[sizeof(full) - 1U] = '\0';
    text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_CONTENT, full);
    assert_int_equal(workspace.edit_text_length, UI_DOCUMENT_CONTENT_CAPACITY - 1U);

    before = workspace;
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "y"), UI_MENU_WORKSPACE_FIELD_FULL);
    assert_field_unchanged(&workspace, &before);

    /* Two bytes do not sneak in where one does not fit: the refusal is whole, not by byte. */
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "\xE3\x81\x82"),
                     UI_MENU_WORKSPACE_FIELD_FULL);
    assert_field_unchanged(&workspace, &before);

    /* One byte room left: an ASCII character fits, and the Japanese one still does not. */
    assert_int_equal(ui_menu_workspace_backspace(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "\xE3\x81\x82"),
                     UI_MENU_WORKSPACE_FIELD_FULL);
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "y"), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.edit_text_length, UI_DOCUMENT_CONTENT_CAPACITY - 1U);
}




/* D-E rule 1: an empty clipboard is a status. Nothing happens, nothing is refused, nothing is reported. */
static void test_paste_of_nothing_is_silent(void **state) {
    UiMenuWorkspace workspace;
    UiTextDiagnostic diagnostic;
    UiMenuWorkspace before;
    (void)state;

    text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_CONTENT, "abc");
    before = workspace;
    memset(&diagnostic, 0x5a, sizeof(diagnostic));
    assert_int_equal(ui_menu_workspace_paste_text(&workspace, "", 0U, &diagnostic),
                     UI_MENU_WORKSPACE_NO_ACTION);
    assert_field_unchanged(&workspace, &before);
    /* The diagnostic is cleared rather than left holding a previous paste's explanation: a status has
       nothing to say, and a caller must not read a stale offset. */
    assert_int_equal(diagnostic.result, UI_TEXT_OK);
    assert_int_equal(diagnostic.offset, 0U);
    /* NULL is the same status, for a caller whose platform reported EMPTY. */
    assert_int_equal(ui_menu_workspace_paste_text(&workspace, NULL, 0U, NULL),
                     UI_MENU_WORKSPACE_NO_ACTION);
    assert_field_unchanged(&workspace, &before);
}

/* D-E rule 2: malformed UTF-8 refuses the whole paste and names the byte it starts at. */
static void test_paste_of_malformed_text_is_refused_with_its_offset(void **state) {
    UiMenuWorkspace workspace;
    UiTextDiagnostic diagnostic;
    UiMenuWorkspace before;
    (void)state;

    text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_CONTENT, "abc");
    before = workspace;
    assert_int_equal(ui_menu_workspace_paste_text(&workspace, "ok\x80" "ok", 5U, &diagnostic),
                     UI_MENU_WORKSPACE_REJECTED_TEXT);
    assert_int_equal(diagnostic.result, UI_TEXT_MALFORMED);
    assert_int_equal(diagnostic.offset, 2U);
    assert_int_equal(diagnostic.codepoint, 0U);
    assert_field_unchanged(&workspace, &before);

    /* A well-formed paste into the same field still works, so the refusal is about the bytes. */
    assert_int_equal(ui_menu_workspace_paste_text(&workspace, "xy", 2U, &diagnostic),
                     UI_MENU_WORKSPACE_OK);
    assert_string_equal(workspace.edit_text, "abcxy");
}

/* D-E rule 3: a paste that would not fit is refused whole — never shortened to fit. */
static void test_paste_that_does_not_fit_is_refused_whole(void **state) {
    UiMenuWorkspace workspace;
    char big[UI_DOCUMENT_CONTENT_CAPACITY + 16U];
    UiMenuWorkspace before;
    (void)state;

    text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_CONTENT, "abc");
    memset(big, 'z', sizeof(big) - 1U);
    big[sizeof(big) - 1U] = '\0';
    before = workspace;
    assert_int_equal(ui_menu_workspace_paste_text(&workspace, big, sizeof(big) - 1U, NULL),
                     UI_MENU_WORKSPACE_FIELD_FULL);
    assert_field_unchanged(&workspace, &before);

    /* Replacing a selection counts the selection: a paste that fits only because the selection goes is
       accepted, which is why the check subtracts it rather than comparing absolute sizes. */
    assert_int_equal(ui_menu_workspace_select_all(&workspace, true), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_paste_text(&workspace, big, 200U, NULL),
                     UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.edit_text_length, 200U);
}

/* D-E rule 4: a control character is refused, because a single-line label has no use for one. */
static void test_paste_with_a_control_character_is_refused(void **state) {
    UiMenuWorkspace workspace;
    UiTextDiagnostic diagnostic;
    UiMenuWorkspace before;
    (void)state;

    text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_CONTENT, "abc");
    before = workspace;
    assert_int_equal(ui_menu_workspace_paste_text(&workspace, "line\nbreak", 10U, &diagnostic),
                     UI_MENU_WORKSPACE_REJECTED_TEXT);
    assert_int_equal(diagnostic.result, UI_TEXT_CONTROL);
    assert_int_equal(diagnostic.offset, 4U);
    assert_int_equal(diagnostic.codepoint, 0x000AU);
    assert_field_unchanged(&workspace, &before);

    /* Tab too, and the RTL letter that `ui_text` refuses for its own reason (D-C). */
    assert_int_equal(ui_menu_workspace_paste_text(&workspace, "a\tb", 3U, NULL),
                     UI_MENU_WORKSPACE_REJECTED_TEXT);
    assert_int_equal(ui_menu_workspace_paste_text(&workspace, "\xD7\x90", 2U, &diagnostic),
                     UI_MENU_WORKSPACE_REJECTED_TEXT);
    assert_int_equal(diagnostic.result, UI_TEXT_RIGHT_TO_LEFT);
    assert_field_unchanged(&workspace, &before);
}

/* A pasted identifier is refused as a whole rather than filtered: dropping half of what someone pasted is
   the half-application D-E forbids, even though typing filters character by character. */
/* A refusal says *why* and *where*, and a later success does not leave the old reason behind: the surface
   shows whatever this returns, so a stale one would be a lie about different text. */
static void test_text_refusals_record_their_reason(void **state) {
    UiMenuWorkspace workspace;
    const UiTextDiagnostic *diagnostic;
    (void)state;

    text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_CONTENT, "abc");
    assert_null(ui_menu_workspace_last_text_diagnostic(&workspace));

    assert_int_equal(ui_menu_workspace_append_text(&workspace, "x\ny"),
                     UI_MENU_WORKSPACE_REJECTED_TEXT);
    diagnostic = ui_menu_workspace_last_text_diagnostic(&workspace);
    assert_non_null(diagnostic);
    assert_int_equal(diagnostic->result, UI_TEXT_CONTROL);
    assert_int_equal(diagnostic->offset, 1U);
    assert_int_equal(diagnostic->codepoint, 0x000AU);

    /* A paste records the same way, because the surface shows one line for both. */
    assert_int_equal(ui_menu_workspace_paste_text(&workspace, "ok\x80", 3U, NULL),
                     UI_MENU_WORKSPACE_REJECTED_TEXT);
    diagnostic = ui_menu_workspace_last_text_diagnostic(&workspace);
    assert_non_null(diagnostic);
    assert_int_equal(diagnostic->result, UI_TEXT_MALFORMED);
    assert_int_equal(diagnostic->offset, 2U);

    /* Success clears it. */
    assert_int_equal(ui_menu_workspace_append_text(&workspace, "d"), UI_MENU_WORKSPACE_OK);
    assert_null(ui_menu_workspace_last_text_diagnostic(&workspace));

    /* A full field is a refusal with no character to blame, so there is no text diagnostic - the surface
       has its own sentence for it. */
    {
        char full[UI_DOCUMENT_CONTENT_CAPACITY];
        memset(full, 'z', sizeof(full) - 1U);
        full[sizeof(full) - 1U] = '\0';
        text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_CONTENT, full);
        assert_int_equal(ui_menu_workspace_append_text(&workspace, "y"),
                         UI_MENU_WORKSPACE_FIELD_FULL);
        assert_null(ui_menu_workspace_last_text_diagnostic(&workspace));
    }
}

/* A refused load that is refused for its *content* says which line and column; the workspace keeps that so
   the chooser can show it. */
static void test_refused_load_records_its_position(void **state) {
    UiMenuWorkspace workspace;
    UiDocument document;
    UiElementId button;
    const UiDocumentLoadDiagnostic *load;
    FILE *file;
    char root[64], menus[96], path[160];
    char bytes[4096];
    size_t length;
    char *match;
    (void)state;

    make_paths(root, menus);
    assert_true(snprintf(path, sizeof(path), "%s/menu.tui", menus) > 0);
    assert_int_equal(ui_document_create_menu(&document, "menu"), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_BUTTON,
        1U, "play", "PLAY", "play", &button), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_save_as(&document, path), UI_DOCUMENT_OK);

    /* Put one bad byte into the button's content, leaving everything else valid. */
    file = fopen(path, "rb");
    assert_non_null(file);
    length = fread(bytes, 1U, sizeof(bytes) - 1U, file);
    assert_int_equal(fclose(file), 0);
    bytes[length] = '\0';
    match = strstr(bytes, "content=PLAY");
    assert_non_null(match);
    match[10] = (char)0x80; /* the 'A' of PLAY becomes an invalid lead byte */
    file = fopen(path, "wb");
    assert_non_null(file);
    assert_true(fwrite(bytes, 1U, length, file) == length);
    assert_int_equal(fclose(file), 0);

    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_LOAD_FAILED);
    load = ui_menu_workspace_last_load_diagnostic(&workspace);
    assert_non_null(load);
    assert_true(load->line > 0U);
    assert_int_equal(load->column, 10U); /* "content=" is 8 bytes, then P at 8, L at 9, the broken A at 10 */
    assert_int_equal(load->text_result, UI_TEXT_MALFORMED);

    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}


static void test_paste_into_an_identifier_is_all_or_nothing(void **state) {
    UiMenuWorkspace workspace;
    UiMenuWorkspace before;
    (void)state;

    text_workspace(&workspace, UI_MENU_WORKSPACE_EDIT_PORT, "");
    before = workspace;
    assert_int_equal(ui_menu_workspace_paste_text(&workspace, "play now!", 9U, NULL),
                     UI_MENU_WORKSPACE_REJECTED_TEXT);
    assert_field_unchanged(&workspace, &before);
    assert_int_equal(ui_menu_workspace_paste_text(&workspace, "play_now", 8U, NULL),
                     UI_MENU_WORKSPACE_OK);
    assert_string_equal(workspace.edit_text, "play_now");
}
static void test_a_paste_is_one_undo_step(void **state) {
    UiMenuWorkspace workspace;
    UiDocument document;
    UiElementId button;
    char root[64], menus[96], path[160];
    size_t changes;
    (void)state;

    make_paths(root, menus);
    assert_true(snprintf(path, sizeof(path), "%s/menu.tui", menus) > 0);
    assert_int_equal(ui_document_create_menu(&document, "menu"), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_BUTTON,
        1U, "play", "PLAY", "play", &button), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_save_as(&document, path), UI_DOCUMENT_OK);
    ui_menu_workspace_init(&workspace);
    assert_int_equal(ui_menu_workspace_open(&workspace, root), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.element_index = 1U;
    assert_int_equal(ui_menu_workspace_open_actions(&workspace), UI_MENU_WORKSPACE_OK);
    workspace.action_index = UI_MENU_ACTION_EDIT_CONTENT;
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_int_equal(workspace.mode, UI_MENU_WORKSPACE_EDIT_CONTENT);
    assert_string_equal(workspace.edit_text, "PLAY");
    changes = workspace.change_count;

    assert_int_equal(ui_menu_workspace_paste_text(&workspace, " NOW", 4U, NULL),
                     UI_MENU_WORKSPACE_OK);
    assert_string_equal(workspace.edit_text, "PLAY NOW");
    /* The field edit is not a document change yet: nothing has been committed. */
    assert_int_equal(workspace.change_count, changes);
    assert_int_equal(ui_menu_workspace_confirm(&workspace), UI_MENU_WORKSPACE_OK);
    assert_string_equal(ui_document_find_element(&workspace.document, button)->content,
                        "PLAY NOW");
    assert_int_equal(workspace.change_count, changes + 1U);

    /* One undo restores the pre-paste text; a second would be needed only if paste had been many edits. */
    assert_int_equal(ui_menu_workspace_undo(&workspace), UI_MENU_WORKSPACE_OK);
    assert_string_equal(ui_document_find_element(&workspace.document, button)->content, "PLAY");
    assert_int_equal(workspace.change_count, changes + 1U);

    ui_menu_workspace_clear(&workspace);
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
}









int main(void) {
    const struct CMUnitTest tests[] = {
/* The property that makes paste usable: it is one edit. One paste, one commit, one undo — not one change
   per character, which is what pasting through the typing path would produce. */
        cmocka_unit_test(test_chooser_load_hierarchy_property_history_and_discard),
        cmocka_unit_test(test_create_name_save_and_catalog_refresh),
        cmocka_unit_test(test_caret_moves_by_cluster_not_by_byte),
        cmocka_unit_test(test_selection_extends_with_shift_and_collapses_without),
        cmocka_unit_test(test_typing_inserts_at_the_caret_and_replaces_a_selection),
        cmocka_unit_test(test_backspace_deletes_a_selection_or_one_cluster),
        cmocka_unit_test(test_content_refuses_what_the_document_would_and_identifiers_filter),
        cmocka_unit_test(test_field_at_capacity_refuses_without_splitting),
        cmocka_unit_test(test_paste_of_nothing_is_silent),
        cmocka_unit_test(test_paste_of_malformed_text_is_refused_with_its_offset),
        cmocka_unit_test(test_paste_that_does_not_fit_is_refused_whole),
        cmocka_unit_test(test_paste_with_a_control_character_is_refused),
        cmocka_unit_test(test_paste_into_an_identifier_is_all_or_nothing),
        cmocka_unit_test(test_text_refusals_record_their_reason),
        cmocka_unit_test(test_refused_load_records_its_position),
        cmocka_unit_test(test_a_paste_is_one_undo_step),
        cmocka_unit_test(test_preedit_is_shown_but_never_committed),
        cmocka_unit_test(test_preedit_is_truncated_at_a_cluster_boundary),
        cmocka_unit_test(test_leaving_the_field_discards_the_composition),
        cmocka_unit_test(test_invalid_load_and_boundaries_are_transactional)
        ,cmocka_unit_test(test_managed_menu_directory_failure_preserves_workspace)
        ,cmocka_unit_test(test_catalog_open_failure_preserves_workspace)
        ,cmocka_unit_test(test_i13_construct_edit_remove_and_history)
        ,cmocka_unit_test(test_i13_container_subtree_remove_is_one_restorable_command)
        ,cmocka_unit_test(test_i14_rename_reparent_and_adjacent_subtree_history)
        ,cmocka_unit_test(test_i15_visual_properties_history_and_persistence)
        ,cmocka_unit_test(test_i16_pointer_select_move_resize_cancel_and_history)
        ,cmocka_unit_test(test_i17_preview_settings_are_session_only)
        ,cmocka_unit_test(test_history_capacity_evicts_oldest_without_disabling_edits)
        ,cmocka_unit_test(test_created_hierarchy_exposes_valid_reparent_destination)
        ,cmocka_unit_test(test_committed_warning_updates_heap_history_identity)
        ,cmocka_unit_test(test_same_workspace_trace_is_host_independent)
        ,cmocka_unit_test(test_phase3_animation_action_is_one_undoable_edit)
        ,cmocka_unit_test(test_phase3_candidate_preview_is_nonhistorical_until_accept)
        ,cmocka_unit_test(test_phase3_action_vocabulary_and_playback_are_host_independent)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
