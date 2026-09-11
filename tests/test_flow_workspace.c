#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/flow_workspace.h"
#include "../src/ui_nested_inspector.h"

static FlowDocument document_fixture(FlowNodeId *scene, FlowNodeId *menu,
                                     FlowEdgeId *start_edge) {
    FlowDocument document;
    FlowEdgeId edge;
    flow_document_init(&document);
    assert_int_equal(flow_document_add_node(&document, FLOW_NODE_SCENE, "scene", scene),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_add_node(&document, FLOW_NODE_MENU, "menu", menu),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&document, 1U, "start", *scene, start_edge),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&document, *scene, "exit", *menu, &edge),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&document, *menu, "return", *scene, &edge),
                     FLOW_DOCUMENT_OK);
    return document;
}

static void test_shared_nested_inspector_navigation_and_rows(void **state) {
    UiNestedInspectorCursor cursor = {0U, {2U, 0U, 0U, 0U}};
    size_t index = 0U;
    char row[64];
    (void)state;
    assert_true(ui_nested_inspector_step(&index, 3U, true));
    assert_int_equal(index, 2U);
    assert_true(ui_nested_inspector_step(&index, 3U, false));
    assert_int_equal(index, 0U);
    assert_false(ui_nested_inspector_step(&index, 0U, false));
    assert_true(ui_nested_inspector_enter(&cursor, 2U));
    assert_int_equal(cursor.depth, 1U);
    assert_int_equal(*ui_nested_inspector_index(&cursor), 0U);
    *ui_nested_inspector_index(&cursor) = 1U;
    assert_true(ui_nested_inspector_escape(&cursor));
    assert_int_equal(cursor.depth, 0U);
    assert_int_equal(*ui_nested_inspector_index_const(&cursor), 2U);
    assert_false(ui_nested_inspector_escape(&cursor));
    assert_true(ui_nested_inspector_format_row(
        row, sizeof(row), true, 2U, 7U, "Button:play"));
    assert_string_equal(row, " >     [7] Button:play");
    assert_false(ui_nested_inspector_format_row(
        row, 4U, false, 0U, 0U, "too long"));
}

static void test_navigation_rewire_and_nested_escape(void **state) {
    FlowNodeId scene;
    FlowNodeId menu;
    FlowEdgeId start_edge;
    FlowDocument document = document_fixture(&scene, &menu, &start_edge);
    FlowWorkspace workspace;
    (void)state;
    flow_workspace_init(&workspace);
    assert_int_equal(flow_workspace_open_document(&workspace, &document),
                     FLOW_WORKSPACE_OK);
    assert_true(workspace.active);
    assert_false(flow_workspace_is_dirty(&workspace));
    assert_int_equal(flow_workspace_selected_node(&workspace)->type, FLOW_NODE_START);
    assert_int_equal(flow_workspace_outgoing_count(&workspace), 1U);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_EDGES);
    assert_int_equal(flow_workspace_selected_edge(&workspace)->id, start_edge);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_TARGETS);
    assert_int_equal(flow_workspace_selected_target(&workspace)->id, scene);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_NEXT),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_selected_target(&workspace)->id, menu);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_EDGES);
    assert_int_equal(flow_document_find_edge(&workspace.document, start_edge)->target_id,
                     menu);
    assert_true(flow_workspace_is_dirty(&workspace));
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_ESCAPE),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_NODES);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_ESCAPE),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_CLOSE_PROMPT);
}

static void test_close_prompt_discard_cancel_and_clean_close(void **state) {
    FlowNodeId scene;
    FlowNodeId menu;
    FlowEdgeId start_edge;
    FlowDocument document = document_fixture(&scene, &menu, &start_edge);
    FlowWorkspace workspace;
    (void)state;
    assert_int_equal(flow_workspace_open_document(&workspace, &document),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_NEXT),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_ESCAPE),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_ESCAPE),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_NEXT),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.close_choice, FLOW_WORKSPACE_CLOSE_DISCARD);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_NEXT),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.close_choice, FLOW_WORKSPACE_CLOSE_CANCEL);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_true(workspace.active);
    assert_true(flow_workspace_is_dirty(&workspace));
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_NODES);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_ESCAPE),
                     FLOW_WORKSPACE_OK);
    workspace.close_choice = FLOW_WORKSPACE_CLOSE_DISCARD;
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_false(workspace.active);
    assert_false(flow_document_is_dirty(&workspace.document));
    assert_int_equal(flow_document_find_edge(&workspace.document, start_edge)->target_id,
                     scene);
    assert_int_equal(flow_workspace_open_document(&workspace, &workspace.document),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_ESCAPE),
                     FLOW_WORKSPACE_OK);
    assert_false(workspace.active);
}

static void test_save_load_and_failure_are_transactional(void **state) {
    FlowNodeId scene;
    FlowNodeId menu;
    FlowEdgeId start_edge;
    FlowDocument document = document_fixture(&scene, &menu, &start_edge);
    FlowWorkspace workspace;
    FlowWorkspace before;
    char path[] = "/tmp/tsg_flow_workspace_XXXXXX";
    int fd;
    (void)state;
    fd = mkstemp(path);
    assert_true(fd >= 0);
    assert_int_equal(close(fd), 0);
    assert_int_equal(flow_workspace_open_document(&workspace, &document),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_save(&workspace), FLOW_WORKSPACE_SAVE_FAILED);
    assert_int_equal(flow_workspace_save_as(&workspace, path), FLOW_WORKSPACE_OK);
    assert_false(flow_workspace_is_dirty(&workspace));
    assert_string_equal(workspace.document.path, path);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_NEXT),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_true(flow_workspace_is_dirty(&workspace));
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_SAVE),
                     FLOW_WORKSPACE_OK);
    assert_false(flow_workspace_is_dirty(&workspace));
    before = workspace;
    assert_int_equal(flow_workspace_load(&workspace, "/missing/flow.document"),
                     FLOW_WORKSPACE_INVALID_DOCUMENT);
    assert_memory_equal(&workspace, &before, sizeof(workspace));
    assert_int_equal(unlink(path), 0);
}

static void test_invalid_and_empty_operations_are_typed(void **state) {
    FlowWorkspace workspace;
    FlowWorkspace before;
    FlowDocument invalid;
    (void)state;
    flow_workspace_init(&workspace);
    before = workspace;
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_NEXT),
                     FLOW_WORKSPACE_INACTIVE);
    assert_int_equal(flow_workspace_open_document(NULL, &workspace.document),
                     FLOW_WORKSPACE_INVALID_ARGUMENT);
    invalid = workspace.document;
    invalid.nodes[0].id = 0U;
    assert_int_equal(flow_workspace_open_document(&workspace, &invalid),
                     FLOW_WORKSPACE_INVALID_DOCUMENT);
    assert_memory_equal(&workspace, &before, sizeof(workspace));
    assert_int_equal(flow_workspace_open_document(&workspace, &before.document),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_NO_ACTION);
    assert_int_equal(flow_workspace_handle_input(&workspace,
        (FlowWorkspaceInput)99), FLOW_WORKSPACE_INVALID_ARGUMENT);
}

static void test_undo_redo_save_identity_and_branch_truncation(void **state) {
    FlowNodeId scene;
    FlowNodeId menu;
    FlowEdgeId start_edge;
    FlowDocument document = document_fixture(&scene, &menu, &start_edge);
    FlowWorkspace workspace;
    char path[] = "/tmp/tsg_flow_workspace_history_XXXXXX";
    int fd;
    (void)state;
    fd = mkstemp(path);
    assert_true(fd >= 0);
    assert_int_equal(close(fd), 0);
    assert_int_equal(flow_workspace_open_document(&workspace, &document),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_save_as(&workspace, path), FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_NEXT),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.change_count, 1U);
    assert_int_equal(workspace.change_cursor, 1U);
    assert_true(flow_workspace_is_dirty(&workspace));
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_UNDO),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_document_find_edge(&workspace.document, start_edge)->target_id,
                     scene);
    assert_false(flow_workspace_is_dirty(&workspace));
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_REDO),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_document_find_edge(&workspace.document, start_edge)->target_id,
                     menu);
    assert_true(flow_workspace_is_dirty(&workspace));
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_SAVE),
                     FLOW_WORKSPACE_OK);
    assert_false(flow_workspace_is_dirty(&workspace));
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_UNDO),
                     FLOW_WORKSPACE_OK);
    assert_true(flow_workspace_is_dirty(&workspace));
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_REDO),
                     FLOW_WORKSPACE_OK);
    assert_false(flow_workspace_is_dirty(&workspace));
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_UNDO),
                     FLOW_WORKSPACE_OK);
    workspace.mode = FLOW_WORKSPACE_TARGETS;
    workspace.target_index = 1U;
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.change_count, 1U);
    assert_int_equal(workspace.change_cursor, 1U);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_REDO),
                     FLOW_WORKSPACE_NO_ACTION);
    assert_int_equal(unlink(path), 0);
}

static void test_dirty_close_default_save_commits_and_closes(void **state) {
    FlowNodeId scene;
    FlowNodeId menu;
    FlowEdgeId start_edge;
    FlowDocument document = document_fixture(&scene, &menu, &start_edge);
    FlowWorkspace workspace;
    FlowDocument reopened;
    char path[] = "/tmp/tsg_flow_workspace_close_save_XXXXXX";
    int fd;
    (void)state;
    fd = mkstemp(path);
    assert_true(fd >= 0);
    assert_int_equal(close(fd), 0);
    assert_int_equal(flow_workspace_open_document(&workspace, &document),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_save_as(&workspace, path), FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_NEXT),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_ESCAPE),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_ESCAPE),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_CLOSE_PROMPT);
    assert_int_equal(workspace.close_choice, FLOW_WORKSPACE_CLOSE_SAVE);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_false(workspace.active);
    flow_document_init(&reopened);
    assert_int_equal(flow_document_load(&reopened, path), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_find_edge(&reopened, start_edge)->target_id, menu);
    assert_int_equal(unlink(path), 0);
}

static void test_catalog_connect_add_remove_and_history(void **state) {
    const char *scene_ports[] = {"exit", "secret"};
    const char *menu_ports[] = {"play"};
    FlowReferenceEntry entries[] = {
        {FLOW_NODE_SCENE, "scene", scene_ports, 2U},
        {FLOW_NODE_MENU, "menu", menu_ports, 1U}
    };
    FlowReferenceCatalog catalog = {entries, 2U};
    FlowDocument document;
    FlowWorkspace workspace;
    const FlowNode *menu;
    FlowNodeId scene;
    FlowEdgeId edge;
    char path[] = "/tmp/tsg_flow_workspace_i11_XXXXXX";
    int fd;
    (void)state;
    flow_document_init(&document);
    assert_int_equal(flow_document_add_node(&document, FLOW_NODE_SCENE, "scene", &scene),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&document, 1U, "start", scene, &edge),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_workspace_open_document(&workspace, &document),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_set_catalog(&workspace, &catalog), FLOW_WORKSPACE_OK);
    workspace.node_index = 1U;
    assert_int_equal(flow_workspace_connection_count(&workspace), 2U);
    workspace.edge_index = 1U;
    assert_string_equal(flow_workspace_selected_port(&workspace), "secret");
    assert_null(flow_workspace_selected_edge(&workspace));
    workspace.mode = FLOW_WORKSPACE_EDGES;
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_TARGETS);
    assert_int_equal(flow_workspace_target_count(&workspace), 2U);
    workspace.target_index = 1U;
    assert_string_equal(flow_workspace_selected_asset(&workspace)->name, "menu");
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.document.node_count, 3U);
    assert_int_equal(workspace.document.edge_count, 2U);
    assert_int_equal(workspace.change_count, 1U);
    menu = &workspace.document.nodes[2];
    assert_string_equal(menu->asset_name, "menu");
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_UNDO),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.document.node_count, 2U);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_REDO),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.document.node_count, 3U);
    workspace.node_index = 1U;
    workspace.edge_index = 1U;
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_REMOVE),
                     FLOW_WORKSPACE_MUTATION_FAILED);
    assert_int_equal(workspace.document.edge_count, 2U);
    workspace.mode = FLOW_WORKSPACE_NODES;
    workspace.node_index = 2U;
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_REMOVE),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.document.node_count, 2U);
    assert_int_equal(workspace.change_count, 2U);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_UNDO),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.document.node_count, 3U);
    fd = mkstemp(path);
    assert_true(fd >= 0);
    assert_int_equal(close(fd), 0);
    assert_int_equal(flow_workspace_save_as(&workspace, path), FLOW_WORKSPACE_OK);
    assert_false(flow_workspace_is_dirty(&workspace));
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_UNDO),
                     FLOW_WORKSPACE_OK);
    assert_true(flow_workspace_is_dirty(&workspace));
    assert_int_equal(unlink(path), 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_shared_nested_inspector_navigation_and_rows),
        cmocka_unit_test(test_navigation_rewire_and_nested_escape),
        cmocka_unit_test(test_close_prompt_discard_cancel_and_clean_close),
        cmocka_unit_test(test_save_load_and_failure_are_transactional),
        cmocka_unit_test(test_invalid_and_empty_operations_are_typed),
        cmocka_unit_test(test_undo_redo_save_identity_and_branch_truncation),
        cmocka_unit_test(test_dirty_close_default_save_commits_and_closes),
        cmocka_unit_test(test_catalog_connect_add_remove_and_history)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}