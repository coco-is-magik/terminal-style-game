#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/flow_document.h"

static void test_init_add_connect_and_cycle(void **state) {
    FlowDocument document;
    FlowNodeId scene;
    FlowNodeId menu;
    FlowEdgeId edge;
    (void)state;
    flow_document_init(&document);
    assert_int_equal(document.node_count, 1U);
    assert_int_equal(document.nodes[0].type, FLOW_NODE_START);
    assert_false(flow_document_is_dirty(&document));
    assert_int_equal(flow_document_add_node(&document, FLOW_NODE_SCENE, "hub", &scene), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_add_node(&document, FLOW_NODE_MENU, "missions", &menu), FLOW_DOCUMENT_OK);
    assert_true(flow_document_is_dirty(&document));
    assert_int_equal(flow_document_connect(&document, 1U, "start", scene, &edge), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&document, scene, "exit", menu, &edge), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&document, menu, "return", scene, &edge), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_validate(&document), FLOW_DOCUMENT_OK);
}

static void test_rejects_invalid_ports_references_and_unreachable_nodes(void **state) {
    FlowDocument document;
    FlowNodeId scene;
    FlowEdgeId edge;
    (void)state;
    flow_document_init(&document);
    assert_int_equal(flow_document_add_node(&document, FLOW_NODE_START, "bad", &scene), FLOW_DOCUMENT_INVALID_ARGUMENT);
    assert_int_equal(flow_document_add_node(&document, FLOW_NODE_SCENE, "bad/name", &scene), FLOW_DOCUMENT_INVALID_NAME);
    assert_int_equal(flow_document_add_node(&document, FLOW_NODE_SCENE, "level_1", &scene), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_validate(&document), FLOW_DOCUMENT_UNREACHABLE_NODE);
    assert_int_equal(flow_document_connect(&document, 1U, "wrong", scene, &edge), FLOW_DOCUMENT_INVALID_PORT);
    assert_int_equal(flow_document_connect(&document, 99U, "exit", scene, &edge), FLOW_DOCUMENT_MISSING_NODE);
    assert_int_equal(flow_document_connect(&document, 1U, "start", scene, &edge), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&document, 1U, "start", scene, &edge), FLOW_DOCUMENT_DUPLICATE_SOURCE_PORT);
    assert_int_equal(flow_document_validate(&document), FLOW_DOCUMENT_OK);
}

static void test_save_load_round_trip_and_transactional_failure(void **state) {
    FlowDocument document;
    FlowDocument loaded;
    FlowDocument before;
    FlowNodeId scene;
    FlowNodeId menu;
    FlowEdgeId edge;
    char path[] = "/tmp/tsg_flow_document_XXXXXX";
    int fd;
    FILE *file;
    (void)state;
    fd = mkstemp(path);
    assert_true(fd >= 0);
    assert_int_equal(close(fd), 0);
    flow_document_init(&document);
    assert_int_equal(flow_document_add_node(&document, FLOW_NODE_SCENE, "hub", &scene), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_add_node(&document, FLOW_NODE_MENU, "missions", &menu), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&document, 1U, "start", menu, &edge), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&document, menu, "play", scene, &edge), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_save_as(&document, path), FLOW_DOCUMENT_OK);
    assert_false(flow_document_is_dirty(&document));
    memset(&loaded, 0x5a, sizeof(loaded));
    assert_int_equal(flow_document_load(&loaded, path), FLOW_DOCUMENT_OK);
    assert_int_equal(loaded.node_count, 3U);
    assert_int_equal(loaded.edge_count, 2U);
    assert_string_equal(flow_document_find_node(&loaded, scene)->asset_name, "hub");
    assert_false(flow_document_is_dirty(&loaded));
    assert_int_equal(flow_document_save(&loaded), FLOW_DOCUMENT_OK);
    before = loaded;
    file = fopen(path, "w");
    assert_non_null(file);
    assert_true(fputs("flow_version=99\n", file) >= 0);
    assert_int_equal(fclose(file), 0);
    assert_int_equal(flow_document_load(&loaded, path), FLOW_DOCUMENT_PARSE_ERROR);
    assert_memory_equal(&loaded, &before, sizeof(loaded));
    assert_int_equal(unlink(path), 0);
}

static void test_validation_rejects_duplicate_ids_and_missing_nodes(void **state) {
    FlowDocument document;
    FlowNodeId scene;
    FlowEdgeId edge;
    (void)state;
    flow_document_init(&document);
    assert_int_equal(flow_document_add_node(&document, FLOW_NODE_SCENE, "level", &scene), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&document, 1U, "start", scene, &edge), FLOW_DOCUMENT_OK);
    document.nodes[1].id = 1U;
    assert_int_equal(flow_document_validate(&document), FLOW_DOCUMENT_DUPLICATE_ID);
    document.nodes[1].id = scene;
    document.edges[0].target_id = 99U;
    assert_int_equal(flow_document_validate(&document), FLOW_DOCUMENT_MISSING_NODE);
    document.edges[0].target_id = scene;
    document.next_node_id = scene;
    assert_int_equal(flow_document_validate(&document), FLOW_DOCUMENT_DUPLICATE_ID);
}

static void test_set_edge_target_is_validated_and_transactional(void **state) {
    FlowDocument document;
    FlowDocument before;
    FlowNodeId scene;
    FlowNodeId menu;
    FlowEdgeId start_edge;
    FlowEdgeId edge;
    (void)state;
    flow_document_init(&document);
    assert_int_equal(flow_document_add_node(&document, FLOW_NODE_SCENE, "scene", &scene),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_add_node(&document, FLOW_NODE_MENU, "menu", &menu),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&document, 1U, "start", scene, &start_edge),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&document, scene, "exit", menu, &edge),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&document, menu, "return", scene, &edge),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_set_edge_target(&document, start_edge, menu),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_find_edge(&document, start_edge)->target_id, menu);
    before = document;
    assert_int_equal(flow_document_set_edge_target(&document, start_edge, menu),
                     FLOW_DOCUMENT_OK);
    assert_memory_equal(&document, &before, sizeof(document));
    assert_int_equal(flow_document_set_edge_target(&document, start_edge, 999U),
                     FLOW_DOCUMENT_MISSING_NODE);
    assert_memory_equal(&document, &before, sizeof(document));
    document.edges[0].target_id = scene;
    document.edges[1].target_id = scene;
    document.edges[2].target_id = menu;
    before = document;
    assert_int_equal(flow_document_set_edge_target(&document, start_edge, menu),
                     FLOW_DOCUMENT_UNREACHABLE_NODE);
    assert_memory_equal(&document, &before, sizeof(document));
}

static void test_checked_in_project_flow_is_valid(void **state) {
    FlowDocument document;
    const FlowNode *scene;
    (void)state;
    flow_document_init(&document);
    assert_int_equal(flow_document_load(&document, "assets/game.flow"),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_validate(&document), FLOW_DOCUMENT_OK);
    assert_int_equal(document.node_count, 2U);
    assert_int_equal(document.edge_count, 1U);
    scene = flow_document_find_node(&document, 2U);
    assert_non_null(scene);
    assert_int_equal(scene->type, FLOW_NODE_SCENE);
    assert_string_equal(scene->asset_name, "testscene");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_add_connect_and_cycle),
        cmocka_unit_test(test_rejects_invalid_ports_references_and_unreachable_nodes),
        cmocka_unit_test(test_save_load_round_trip_and_transactional_failure),
        cmocka_unit_test(test_validation_rejects_duplicate_ids_and_missing_nodes),
        cmocka_unit_test(test_set_edge_target_is_validated_and_transactional),
        cmocka_unit_test(test_checked_in_project_flow_is_valid)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}