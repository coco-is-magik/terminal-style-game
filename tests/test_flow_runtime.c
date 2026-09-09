#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "../src/flow_runtime.h"

static void build_fixture(FlowDocument *document, FlowReferenceCatalog *catalog,
                          FlowReferenceEntry entries[2],
                          const char *scene_ports[1],
                          const char *menu_ports[1]) {
    FlowNodeId scene, menu;
    FlowEdgeId edge;
    scene_ports[0] = "exit";
    menu_ports[0] = "play";
    entries[0] = (FlowReferenceEntry){FLOW_NODE_SCENE, "hub", scene_ports, 1U};
    entries[1] = (FlowReferenceEntry){FLOW_NODE_MENU, "missions", menu_ports, 1U};
    *catalog = (FlowReferenceCatalog){entries, 2U};
    flow_document_init(document);
    assert_int_equal(flow_document_add_node(document, FLOW_NODE_MENU,
                                            "missions", &menu), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_add_node(document, FLOW_NODE_SCENE,
                                            "hub", &scene), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(document, 1U, "start", menu, &edge),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(document, menu, "play", scene, &edge),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(document, scene, "exit", menu, &edge),
                     FLOW_DOCUMENT_OK);
}

static void test_navigates_start_scene_menu_cycle(void **state) {
    FlowDocument document;
    FlowReferenceCatalog catalog;
    FlowReferenceEntry entries[2];
    const char *scene_ports[1];
    const char *menu_ports[1];
    FlowRuntimeSession session = {99U};
    const FlowNode *target = NULL;
    (void)state;
    build_fixture(&document, &catalog, entries, scene_ports, menu_ports);
    assert_int_equal(flow_runtime_session_init(&session, &document, &catalog),
                     FLOW_RUNTIME_OK);
    assert_int_equal(flow_runtime_current_node(&session, &document)->type,
                     FLOW_NODE_START);
    assert_int_equal(flow_runtime_transition(&session, &document, "start", &target),
                     FLOW_RUNTIME_OK);
    assert_int_equal(target->type, FLOW_NODE_MENU);
    assert_int_equal(flow_runtime_transition(&session, &document, "play", &target),
                     FLOW_RUNTIME_OK);
    assert_int_equal(target->type, FLOW_NODE_SCENE);
    assert_int_equal(flow_runtime_transition(&session, &document, "exit", &target),
                     FLOW_RUNTIME_OK);
    assert_int_equal(target->type, FLOW_NODE_MENU);
}

static void test_failure_does_not_mutate_session_or_output(void **state) {
    FlowDocument document;
    FlowReferenceCatalog catalog;
    FlowReferenceEntry entries[2];
    const char *scene_ports[1];
    const char *menu_ports[1];
    FlowRuntimeSession session;
    FlowNode sentinel = {77U, FLOW_NODE_SCENE, "sentinel"};
    const FlowNode *target = &sentinel;
    FlowNodeId before;
    (void)state;
    build_fixture(&document, &catalog, entries, scene_ports, menu_ports);
    assert_int_equal(flow_runtime_session_init(&session, &document, &catalog),
                     FLOW_RUNTIME_OK);
    before = session.current_node_id;
    assert_int_equal(flow_runtime_transition(&session, &document, "missing", &target),
                     FLOW_RUNTIME_MISSING_PORT);
    assert_int_equal(session.current_node_id, before);
    assert_ptr_equal(target, &sentinel);
    session.current_node_id = 999U;
    assert_int_equal(flow_runtime_transition(&session, &document, "start", &target),
                     FLOW_RUNTIME_INVALID_SESSION);
    assert_int_equal(session.current_node_id, 999U);
    assert_ptr_equal(target, &sentinel);
    session.current_node_id = document.nodes[0].id;
    document.edges[0].target_id = 999U;
    assert_int_equal(flow_runtime_transition(&session, &document, "start", &target),
                     FLOW_RUNTIME_INVALID_DATA);
    assert_int_equal(session.current_node_id, document.nodes[0].id);
    assert_ptr_equal(target, &sentinel);
}

static void test_deterministic_replay_and_invalid_init(void **state) {
    FlowDocument document;
    FlowReferenceCatalog catalog;
    FlowReferenceEntry entries[2];
    const char *scene_ports[1];
    const char *menu_ports[1];
    FlowRuntimeSession first = {0U};
    FlowRuntimeSession second = {0U};
    const FlowNode *target;
    (void)state;
    build_fixture(&document, &catalog, entries, scene_ports, menu_ports);
    assert_int_equal(flow_runtime_session_init(&first, &document, &catalog),
                     FLOW_RUNTIME_OK);
    assert_int_equal(flow_runtime_session_init(&second, &document, &catalog),
                     FLOW_RUNTIME_OK);
    assert_int_equal(flow_runtime_transition(&first, &document, "start", &target),
                     FLOW_RUNTIME_OK);
    assert_int_equal(flow_runtime_transition(&second, &document, "start", &target),
                     FLOW_RUNTIME_OK);
    assert_int_equal(first.current_node_id, second.current_node_id);
    entries[1].name = "missing";
    first.current_node_id = 77U;
    assert_int_equal(flow_runtime_session_init(&first, &document, &catalog),
                     FLOW_RUNTIME_INVALID_DATA);
    assert_int_equal(first.current_node_id, 77U);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_navigates_start_scene_menu_cycle),
        cmocka_unit_test(test_failure_does_not_mutate_session_or_output),
        cmocka_unit_test(test_deterministic_replay_and_invalid_init)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}