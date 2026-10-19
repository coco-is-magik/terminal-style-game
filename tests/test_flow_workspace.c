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
#include "../src/flow_workspace_internal.h"
#include "../src/flow_graph.h"
#include "../src/ui_nested_inspector.h"

static void assert_workspace_save_committed(FlowWorkspaceResult result) {
    assert_true(result == FLOW_WORKSPACE_OK ||
                result == FLOW_WORKSPACE_OK_DURABILITY_WARNING);
}

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
    FlowWorkspace *before;
    char path[] = "build/tsg_flow_workspace_XXXXXX";
    int fd;
    (void)state;
    fd = mkstemp(path);
    assert_true(fd >= 0);
    assert_int_equal(close(fd), 0);
    assert_int_equal(flow_workspace_open_document(&workspace, &document),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_save(&workspace), FLOW_WORKSPACE_SAVE_FAILED);
    assert_workspace_save_committed(flow_workspace_save_as(&workspace, path));
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
    assert_workspace_save_committed(flow_workspace_handle_input(
        &workspace, FLOW_WORKSPACE_INPUT_SAVE));
    assert_false(flow_workspace_is_dirty(&workspace));
    before = malloc(sizeof(*before));
    assert_non_null(before);
    *before = workspace;
    assert_int_equal(flow_workspace_load(&workspace, "/missing/flow.document"),
                     FLOW_WORKSPACE_INVALID_DOCUMENT);
    assert_memory_equal(&workspace, before, sizeof(workspace));
    free(before);
    assert_int_equal(unlink(path), 0);
}

static void test_invalid_and_empty_operations_are_typed(void **state) {
    FlowWorkspace workspace;
    FlowWorkspace *before;
    FlowDocument invalid;
    (void)state;
    flow_workspace_init(&workspace);
    before = malloc(sizeof(*before));
    assert_non_null(before);
    *before = workspace;
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_NEXT),
                     FLOW_WORKSPACE_INACTIVE);
    assert_int_equal(flow_workspace_open_document(NULL, &workspace.document),
                     FLOW_WORKSPACE_INVALID_ARGUMENT);
    invalid = workspace.document;
    invalid.nodes[0].id = 0U;
    assert_int_equal(flow_workspace_open_document(&workspace, &invalid),
                     FLOW_WORKSPACE_INVALID_DOCUMENT);
    assert_memory_equal(&workspace, before, sizeof(workspace));
    assert_int_equal(flow_workspace_open_document(&workspace, &before->document),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_NO_ACTION);
    assert_int_equal(flow_workspace_handle_input(&workspace,
        (FlowWorkspaceInput)99), FLOW_WORKSPACE_INVALID_ARGUMENT);
    free(before);
}

static void test_workspace_committed_warning_updates_owned_snapshots(void **state) {
    FlowNodeId scene;
    FlowNodeId menu;
    FlowEdgeId start_edge;
    FlowDocument document = document_fixture(&scene, &menu, &start_edge);
    FlowWorkspace workspace;
    char path[] = "build/tsg_flow_workspace_warning_XXXXXX";
    int fd = mkstemp(path);
    (void)state;
    assert_true(fd >= 0);
    assert_int_equal(close(fd), 0);
    assert_true(flow_workspace_result_is_committed(FLOW_WORKSPACE_OK));
    assert_true(flow_workspace_result_is_committed(
        FLOW_WORKSPACE_OK_DURABILITY_WARNING));
    assert_false(flow_workspace_result_is_committed(FLOW_WORKSPACE_SAVE_FAILED));
    assert_int_equal(flow_workspace_open_document(&workspace, &document),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(
                         &workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(
                         &workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(
                         &workspace, FLOW_WORKSPACE_INPUT_NEXT),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(
                         &workspace, FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.change_count, 1U);
    assert_true(flow_workspace_is_dirty(&workspace));
    assert_int_equal(flow_workspace_internal_save_as(
                         &workspace, path,
                         FLOW_DOCUMENT_SAVE_FAULT_DURABILITY),
                     FLOW_WORKSPACE_OK_DURABILITY_WARNING);
    assert_int_equal(workspace.last_document_result,
                     FLOW_DOCUMENT_OK_DURABILITY_WARNING);
    assert_false(flow_workspace_is_dirty(&workspace));
    assert_string_equal(workspace.document.path, path);
    assert_memory_equal(&workspace.saved_document, &workspace.document,
                        sizeof(workspace.document));
    assert_string_equal(workspace.changes[0].before.path, path);
    assert_string_equal(workspace.changes[0].after.path, path);
    assert_int_equal(workspace.changes[0].before.state.saved_state,
                     workspace.document.state.saved_state);
    assert_int_equal(workspace.changes[0].after.state.saved_state,
                     workspace.document.state.saved_state);
    assert_int_equal(unlink(path), 0);
}

static void test_undo_redo_save_identity_and_branch_truncation(void **state) {
    FlowNodeId scene;
    FlowNodeId menu;
    FlowEdgeId start_edge;
    FlowDocument document = document_fixture(&scene, &menu, &start_edge);
    FlowWorkspace workspace;
    char path[] = "build/tsg_flow_workspace_history_XXXXXX";
    int fd;
    (void)state;
    fd = mkstemp(path);
    assert_true(fd >= 0);
    assert_int_equal(close(fd), 0);
    assert_int_equal(flow_workspace_open_document(&workspace, &document),
                     FLOW_WORKSPACE_OK);
    assert_workspace_save_committed(flow_workspace_save_as(&workspace, path));
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
    assert_workspace_save_committed(flow_workspace_handle_input(
        &workspace, FLOW_WORKSPACE_INPUT_SAVE));
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
    char path[] = "build/tsg_flow_workspace_close_save_XXXXXX";
    int fd;
    (void)state;
    fd = mkstemp(path);
    assert_true(fd >= 0);
    assert_int_equal(close(fd), 0);
    assert_int_equal(flow_workspace_open_document(&workspace, &document),
                     FLOW_WORKSPACE_OK);
    assert_workspace_save_committed(flow_workspace_save_as(&workspace, path));
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
    assert_workspace_save_committed(flow_workspace_handle_input(
        &workspace, FLOW_WORKSPACE_INPUT_CONFIRM));
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
    char path[] = "build/tsg_flow_workspace_i11_XXXXXX";
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
    assert_int_equal(workspace.last_document_result, FLOW_DOCUMENT_UNREACHABLE_NODE);
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
    assert_workspace_save_committed(flow_workspace_save_as(&workspace, path));
    assert_false(flow_workspace_is_dirty(&workspace));
    assert_int_equal(flow_workspace_handle_input(&workspace, FLOW_WORKSPACE_INPUT_UNDO),
                     FLOW_WORKSPACE_OK);
    assert_true(flow_workspace_is_dirty(&workspace));
    assert_int_equal(unlink(path), 0);
}

static void test_connect_shortcut_opens_free_port_target_list(void **state) {
    const char *scene_ports[] = {"exit", "secret"};
    const char *menu_ports[] = {"play"};
    FlowReferenceEntry entries[] = {
        {FLOW_NODE_SCENE, "scene", scene_ports, 2U},
        {FLOW_NODE_MENU, "menu", menu_ports, 1U}
    };
    FlowReferenceCatalog catalog = {entries, 2U};
    FlowDocument document;
    FlowWorkspace workspace;
    FlowNodeId scene;
    FlowNodeId menu;
    FlowEdgeId edge;
    (void)state;

    flow_document_init(&document);
    assert_int_equal(flow_document_add_node(&document, FLOW_NODE_SCENE, "scene",
                                            &scene), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_add_node(&document, FLOW_NODE_MENU, "menu",
                                            &menu), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&document, 1U, "start", scene, &edge),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&document, scene, "exit", menu, &edge),
                     FLOW_DOCUMENT_OK);
    flow_workspace_init(&workspace);
    assert_int_equal(flow_workspace_open_document(&workspace, &document),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_set_catalog(&workspace, &catalog),
                     FLOW_WORKSPACE_OK);

    /* Start has one port and it is connected: the shortcut opens its target
       list at the current target rather than inventing a free port. */
    workspace.node_index = 0U;
    assert_int_equal(flow_workspace_handle_input(&workspace,
                                                 FLOW_WORKSPACE_INPUT_CONNECT),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_TARGETS);
    assert_string_equal(flow_workspace_selected_port(&workspace), "start");
    assert_int_equal(workspace.target_index, 0U);
    assert_non_null(flow_workspace_selected_target(&workspace));
    assert_int_equal(flow_workspace_selected_target(&workspace)->id, scene);
    assert_int_equal(flow_workspace_handle_input(&workspace,
                                                 FLOW_WORKSPACE_INPUT_CONNECT),
                     FLOW_WORKSPACE_NO_ACTION);
    assert_int_equal(flow_workspace_handle_input(&workspace,
                                                 FLOW_WORKSPACE_INPUT_ESCAPE),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_EDGES);
    assert_int_equal(workspace.edge_index, 0U);
    assert_int_equal(flow_workspace_handle_input(&workspace,
                                                 FLOW_WORKSPACE_INPUT_ESCAPE),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_NODES);

    /* Scene's first port is connected and its second is free: the shortcut
       skips the connected one and lands on the free port's target list. */
    workspace.node_index = 1U;
    assert_int_equal(flow_workspace_handle_input(&workspace,
                                                 FLOW_WORKSPACE_INPUT_CONNECT),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_TARGETS);
    assert_int_equal(workspace.edge_index, 1U);
    assert_string_equal(flow_workspace_selected_port(&workspace), "secret");
    assert_null(flow_workspace_selected_edge(&workspace));
    assert_int_equal(workspace.document.node_count, 3U);
    assert_int_equal(workspace.document.edge_count, 2U);
    assert_int_equal(workspace.change_count, 0U);

    /* Escape walks back one level at a time and restores the port cursor. */
    assert_int_equal(flow_workspace_handle_input(&workspace,
                                                 FLOW_WORKSPACE_INPUT_ESCAPE),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_EDGES);
    assert_int_equal(workspace.edge_index, 1U);
    assert_int_equal(flow_workspace_handle_input(&workspace,
                                                 FLOW_WORKSPACE_INPUT_ESCAPE),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_NODES);
    assert_int_equal(workspace.node_index, 1U);

    workspace.node_index = 2U;
    assert_int_equal(flow_workspace_handle_input(&workspace,
                                                 FLOW_WORKSPACE_INPUT_CONNECT),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_TARGETS);
    assert_string_equal(flow_workspace_selected_port(&workspace), "play");
}

static void test_inspector_begin_connection_and_add_node(void **state) {
    const char *scene_ports[] = {"exit", "secret"};
    const char *menu_ports[] = {"play"};
    const char *bonus_ports[] = {"enter"};
    FlowReferenceEntry entries[] = {
        {FLOW_NODE_SCENE, "scene", scene_ports, 2U},
        {FLOW_NODE_MENU, "menu", menu_ports, 1U},
        {FLOW_NODE_SCENE, "bonus", bonus_ports, 1U}
    };
    FlowReferenceCatalog catalog = {entries, 3U};
    FlowDocument document;
    FlowWorkspace workspace;
    FlowNodeId scene;
    FlowNodeId menu;
    FlowEdgeId edge;
    (void)state;

    flow_document_init(&document);
    assert_int_equal(flow_document_add_node(&document, FLOW_NODE_SCENE, "scene",
                                            &scene), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_add_node(&document, FLOW_NODE_MENU, "menu",
                                            &menu), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&document, 1U, "start", scene, &edge),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&document, scene, "exit", menu, &edge),
                     FLOW_DOCUMENT_OK);
    flow_workspace_init(&workspace);
    assert_int_equal(flow_workspace_open_document(&workspace, &document),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_set_catalog(&workspace, &catalog),
                     FLOW_WORKSPACE_OK);

    /* The Scene has one connected and one free port. */
    workspace.node_index = 1U;
    assert_int_equal(flow_workspace_free_port_count(&workspace), 1U);

    /* A port row opens the picker at that port's current target. */
    assert_int_equal(flow_workspace_begin_connection(&workspace, 1U, 0U),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_TARGETS);
    assert_string_equal(flow_workspace_selected_port(&workspace), "exit");
    assert_int_equal(flow_workspace_selected_target(&workspace)->id, menu);
    assert_int_equal(flow_workspace_handle_input(&workspace,
                                                 FLOW_WORKSPACE_INPUT_ESCAPE),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(flow_workspace_handle_input(&workspace,
                                                 FLOW_WORKSPACE_INPUT_ESCAPE),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_NODES);

    /* Add node takes the first free port and selects the first catalog entry
       that is not in the document, rather than silently rewiring a used port. */
    workspace.node_index = 1U;
    assert_int_equal(flow_workspace_begin_add_node(&workspace), FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.mode, FLOW_WORKSPACE_TARGETS);
    assert_string_equal(flow_workspace_selected_port(&workspace), "secret");
    assert_non_null(flow_workspace_selected_asset(&workspace));
    assert_string_equal(flow_workspace_selected_asset(&workspace)->name, "bonus");
    assert_int_equal(flow_workspace_handle_input(&workspace,
                                                 FLOW_WORKSPACE_INPUT_CONFIRM),
                     FLOW_WORKSPACE_OK);
    assert_int_equal(workspace.document.node_count, 4U);
    assert_int_equal(workspace.document.edge_count, 3U);

    /* Start has no free port, so Add node refuses instead of rewiring it. */
    workspace.node_index = 0U;
    assert_int_equal(flow_workspace_free_port_count(&workspace), 0U);
    assert_int_equal(flow_workspace_begin_add_node(&workspace),
                     FLOW_WORKSPACE_NO_ACTION);
}

static void test_history_capacity_evicts_oldest_without_disabling_edits(void **state) {
    FlowNodeId scene;
    FlowNodeId menu;
    FlowEdgeId start_edge;
    FlowDocument document = document_fixture(&scene, &menu, &start_edge);
    FlowWorkspace workspace;
    size_t i;
    (void)state;
    flow_workspace_init(&workspace);
    assert_int_equal(flow_workspace_open_document(&workspace, &document),
                     FLOW_WORKSPACE_OK);
    for (i = 0U; i < 39U; i++) {
        FlowNodeId target = i % 2U == 0U ? menu : scene;
        workspace.mode = FLOW_WORKSPACE_EDGES;
        workspace.node_index = 0U;
        workspace.edge_index = 0U;
        assert_int_equal(flow_workspace_handle_input(
            &workspace, FLOW_WORKSPACE_INPUT_CONFIRM), FLOW_WORKSPACE_OK);
        workspace.target_index = target == scene ? 0U : 1U;
        assert_int_equal(flow_workspace_handle_input(
            &workspace, FLOW_WORKSPACE_INPUT_CONFIRM), FLOW_WORKSPACE_OK);
    }
    assert_int_equal(workspace.change_count, FLOW_WORKSPACE_HISTORY_CAPACITY);
    assert_int_equal(workspace.change_cursor, FLOW_WORKSPACE_HISTORY_CAPACITY);
    assert_int_equal(flow_document_find_edge(
        &workspace.document, start_edge)->target_id, menu);
    for (i = 0U; i < FLOW_WORKSPACE_HISTORY_CAPACITY; i++)
        assert_int_equal(flow_workspace_undo(&workspace), FLOW_WORKSPACE_OK);
    assert_int_equal(flow_document_find_edge(
        &workspace.document, start_edge)->target_id, menu);
    assert_int_equal(flow_workspace_undo(&workspace), FLOW_WORKSPACE_NO_ACTION);
}

static void test_graph_layout_is_deterministic_and_keeps_selection_visible(void **state) {
    FlowNodeId scene;
    FlowNodeId menu;
    FlowEdgeId start_edge;
    FlowDocument document = document_fixture(&scene, &menu, &start_edge);
    FlowGraphLayout first;
    FlowGraphLayout second;
    const FlowNode *selected;
    const FlowGraphNodeBox *hit;
    size_t i;
    (void)state;

    assert_true(flow_graph_layout_build(&document, NULL, 48, 12, 200, 120, &first));
    assert_true(flow_graph_layout_build(&document, NULL, 48, 12, 200, 120, &second));
    assert_int_equal(first.node_count, 3U);
    assert_int_equal(first.edge_count, 3U);
    assert_int_equal(first.node_width, second.node_width);
    for (i = 0U; i < first.node_count; i++) {
        assert_int_equal(first.nodes[i].x, second.nodes[i].x);
        assert_int_equal(first.nodes[i].y, second.nodes[i].y);
        assert_true(first.nodes[i].width >= 20);
        assert_true(first.nodes[i].height >= 6);
    }
    assert_int_equal(first.nodes[1].layer, 1);
    assert_int_equal(first.nodes[2].layer, 2);

    selected = flow_document_find_node(&document, menu);
    assert_non_null(selected);
    assert_true(flow_graph_layout_build(&document, selected, 48, 12, 200, 120, &first));
    assert_true(first.nodes[2].selected);
    assert_true(first.nodes[2].x >= 48);
    assert_true(first.nodes[2].y >= 12);
    assert_true(first.nodes[2].x + first.nodes[2].width <= 248);
    assert_true(first.nodes[2].y + first.nodes[2].height <= 132);
    hit = flow_graph_node_at(&first, first.nodes[2].x + 1, first.nodes[2].y + 1);
    assert_non_null(hit);
    assert_int_equal(hit->node_index, 2U);
    assert_null(flow_graph_node_at(&first, 1, 1));
    for (i = 0U; i < first.edge_count; i++) {
        assert_true(first.edges[i].source_node < first.node_count);
        assert_true(first.edges[i].target_node < first.node_count);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_shared_nested_inspector_navigation_and_rows),
        cmocka_unit_test(test_navigation_rewire_and_nested_escape),
        cmocka_unit_test(test_close_prompt_discard_cancel_and_clean_close),
        cmocka_unit_test(test_save_load_and_failure_are_transactional),
        cmocka_unit_test(test_invalid_and_empty_operations_are_typed),
        cmocka_unit_test(test_workspace_committed_warning_updates_owned_snapshots),
        cmocka_unit_test(test_undo_redo_save_identity_and_branch_truncation),
        cmocka_unit_test(test_dirty_close_default_save_commits_and_closes),
        cmocka_unit_test(test_catalog_connect_add_remove_and_history)
        ,cmocka_unit_test(test_connect_shortcut_opens_free_port_target_list),
        cmocka_unit_test(test_inspector_begin_connection_and_add_node),
        cmocka_unit_test(test_history_capacity_evicts_oldest_without_disabling_edits),
        cmocka_unit_test(test_graph_layout_is_deterministic_and_keeps_selection_visible)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
