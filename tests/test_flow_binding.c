#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>

#include "../src/flow_binding.h"

typedef struct {
    FlowDocument document;
    FlowReferenceCatalog catalog;
    FlowReferenceEntry entries[2];
    const char *scene_ports[1];
    const char *menu_ports[1];
    FlowNodeId scene_id;
    FlowNodeId menu_id;
} Fixture;

static void fixture_init(Fixture *value) {
    FlowEdgeId edge;
    memset(value, 0, sizeof(*value));
    value->scene_ports[0] = "complete";
    value->menu_ports[0] = "play";
    value->entries[0] = (FlowReferenceEntry){
        FLOW_NODE_SCENE, "mission", value->scene_ports, 1U
    };
    value->entries[1] = (FlowReferenceEntry){
        FLOW_NODE_MENU, "missions", value->menu_ports, 1U
    };
    value->catalog = (FlowReferenceCatalog){value->entries, 2U};
    flow_document_init(&value->document);
    assert_int_equal(flow_document_add_node(&value->document, FLOW_NODE_MENU,
        "missions", &value->menu_id), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_add_node(&value->document, FLOW_NODE_SCENE,
        "mission", &value->scene_id), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&value->document, 1U, "start",
        value->menu_id, &edge), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&value->document, value->menu_id, "play",
        value->scene_id, &edge), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&value->document, value->scene_id,
        "complete", value->menu_id, &edge), FLOW_DOCUMENT_OK);
}

static FlowRuntimeSession session_at_menu(Fixture *value) {
    FlowRuntimeSession session;
    const FlowNode *target;
    assert_int_equal(flow_runtime_session_init(&session, &value->document,
        &value->catalog), FLOW_RUNTIME_OK);
    assert_int_equal(flow_runtime_transition(&session, &value->document, "start",
        &target), FLOW_RUNTIME_OK);
    assert_int_equal(target->type, FLOW_NODE_MENU);
    return session;
}

static void test_button_and_scene_exit_complete_cycle(void **state) {
    Fixture f;
    fixture_init(&f);
    FlowRuntimeSession session = session_at_menu(&f);
    UiInteractionActivation button = {7U, "play"};
    EntityTriggerTickResult tick = {
        .flow_exit_requested = true,
        .flow_exit_trigger_id = 42U,
        .flow_exit_port = "complete"
    };
    FlowBindingTargetRequest request;
    (void)state;
    assert_int_equal(flow_binding_activate_button(&session, &f.document, &button,
        &request), FLOW_BINDING_OK);
    assert_int_equal(request.node_id, f.scene_id);
    assert_int_equal(request.type, FLOW_NODE_SCENE);
    assert_string_equal(request.asset_name, "mission");
    assert_int_equal(session.current_node_id, f.scene_id);
    assert_int_equal(flow_binding_activate_scene_exit(&session, &f.document, &tick,
        &request), FLOW_BINDING_OK);
    assert_int_equal(request.node_id, f.menu_id);
    assert_int_equal(request.type, FLOW_NODE_MENU);
    assert_string_equal(request.asset_name, "missions");
    assert_int_equal(session.current_node_id, f.menu_id);
}

static void assert_preserved(FlowRuntimeSession *session, FlowNodeId expected_id,
                             FlowBindingTargetRequest *request,
                             const FlowBindingTargetRequest *expected_request) {
    assert_int_equal(session->current_node_id, expected_id);
    assert_memory_equal(request, expected_request, sizeof(*request));
}

static void test_rejects_wrong_source_missing_port_and_bad_activations(void **state) {
    Fixture f;
    fixture_init(&f);
    FlowRuntimeSession session = session_at_menu(&f);
    UiInteractionActivation button = {7U, "complete"};
    EntityTriggerTickResult tick = {
        .flow_exit_requested = true,
        .flow_exit_trigger_id = 42U,
        .flow_exit_port = "complete"
    };
    FlowBindingTargetRequest request = {99U, FLOW_NODE_START, "sentinel"};
    FlowBindingTargetRequest before = request;
    FlowNodeId session_before = session.current_node_id;
    char unterminated[FLOW_NAME_CAPACITY];
    (void)state;
    assert_int_equal(flow_binding_activate_scene_exit(&session, &f.document, &tick,
        &request), FLOW_BINDING_WRONG_SOURCE_TYPE);
    assert_preserved(&session, session_before, &request, &before);
    assert_int_equal(flow_binding_activate_button(&session, &f.document, &button,
        &request), FLOW_BINDING_MISSING_PORT);
    assert_preserved(&session, session_before, &request, &before);
    button.element_id = 0U;
    assert_int_equal(flow_binding_activate_button(&session, &f.document, &button,
        &request), FLOW_BINDING_INVALID_ACTIVATION);
    assert_preserved(&session, session_before, &request, &before);
    tick.flow_exit_requested = false;
    assert_int_equal(flow_binding_activate_scene_exit(&session, &f.document, &tick,
        &request), FLOW_BINDING_INVALID_ACTIVATION);
    assert_preserved(&session, session_before, &request, &before);
    memset(unterminated, 'x', sizeof(unterminated));
    button.element_id = 7U;
    button.flow_port = unterminated;
    assert_int_equal(flow_binding_activate_button(&session, &f.document, &button,
        &request), FLOW_BINDING_INVALID_ACTIVATION);
    assert_preserved(&session, session_before, &request, &before);
}

static void test_rejects_invalid_session_graph_and_start_target_atomically(void **state) {
    Fixture f;
    fixture_init(&f);
    FlowRuntimeSession session = session_at_menu(&f);
    UiInteractionActivation button = {7U, "play"};
    FlowBindingTargetRequest request = {99U, FLOW_NODE_START, "sentinel"};
    FlowBindingTargetRequest before = request;
    FlowNodeId session_before;
    (void)state;
    session.current_node_id = 999U;
    assert_int_equal(flow_binding_activate_button(&session, &f.document, &button,
        &request), FLOW_BINDING_INVALID_SESSION);
    assert_preserved(&session, 999U, &request, &before);
    session = session_at_menu(&f);
    session_before = session.current_node_id;
    f.document.edges[1].target_id = 999U;
    assert_int_equal(flow_binding_activate_button(&session, &f.document, &button,
        &request), FLOW_BINDING_INVALID_DATA);
    assert_preserved(&session, session_before, &request, &before);
    fixture_init(&f);
    session = session_at_menu(&f);
    session_before = session.current_node_id;
    {
        FlowEdgeId edge;
        assert_int_equal(flow_document_connect(&f.document, f.menu_id, "keep",
            f.scene_id, &edge), FLOW_DOCUMENT_OK);
    }
    f.document.edges[1].target_id = 1U;
    assert_int_equal(flow_binding_activate_button(&session, &f.document, &button,
        &request), FLOW_BINDING_INVALID_TARGET);
    assert_preserved(&session, session_before, &request, &before);
}

static void test_deterministic_equivalent_sessions(void **state) {
    Fixture f;
    fixture_init(&f);
    FlowRuntimeSession first = session_at_menu(&f);
    FlowRuntimeSession second = session_at_menu(&f);
    UiInteractionActivation activation = {9U, "play"};
    FlowBindingTargetRequest first_request;
    FlowBindingTargetRequest second_request;
    (void)state;
    assert_int_equal(flow_binding_activate_button(&first, &f.document, &activation,
        &first_request), FLOW_BINDING_OK);
    assert_int_equal(flow_binding_activate_button(&second, &f.document, &activation,
        &second_request), FLOW_BINDING_OK);
    assert_int_equal(first.current_node_id, second.current_node_id);
    assert_int_equal(first_request.node_id, second_request.node_id);
    assert_int_equal(first_request.type, second_request.type);
    assert_string_equal(first_request.asset_name, second_request.asset_name);
}

static void test_null_and_incomplete_inputs_are_typed_and_atomic(void **state) {
    Fixture f;
    FlowRuntimeSession session;
    UiInteractionActivation button = {7U, "play"};
    EntityTriggerTickResult tick = {
        .flow_exit_requested = true,
        .flow_exit_trigger_id = 42U,
        .flow_exit_port = "complete"
    };
    FlowBindingTargetRequest request = {99U, FLOW_NODE_START, "sentinel"};
    FlowBindingTargetRequest before = request;
    FlowNodeId session_before;
    (void)state;
    fixture_init(&f);
    session = session_at_menu(&f);
    session_before = session.current_node_id;
    assert_int_equal(flow_binding_activate_button(NULL, &f.document, &button,
        &request), FLOW_BINDING_INVALID_ARGUMENT);
    assert_int_equal(flow_binding_activate_button(&session, NULL, &button,
        &request), FLOW_BINDING_INVALID_ARGUMENT);
    assert_int_equal(flow_binding_activate_button(&session, &f.document, NULL,
        &request), FLOW_BINDING_INVALID_ARGUMENT);
    assert_int_equal(flow_binding_activate_button(&session, &f.document, &button,
        NULL), FLOW_BINDING_INVALID_ARGUMENT);
    assert_preserved(&session, session_before, &request, &before);
    tick.flow_exit_trigger_id = 0U;
    assert_int_equal(flow_binding_activate_scene_exit(&session, &f.document, &tick,
        &request), FLOW_BINDING_INVALID_ACTIVATION);
    assert_preserved(&session, session_before, &request, &before);
    tick.flow_exit_trigger_id = 42U;
    tick.flow_exit_port = NULL;
    assert_int_equal(flow_binding_activate_scene_exit(&session, &f.document, &tick,
        &request), FLOW_BINDING_INVALID_ACTIVATION);
    assert_preserved(&session, session_before, &request, &before);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_button_and_scene_exit_complete_cycle),
        cmocka_unit_test(test_rejects_wrong_source_missing_port_and_bad_activations),
        cmocka_unit_test(test_rejects_invalid_session_graph_and_start_target_atomically),
        cmocka_unit_test(test_deterministic_equivalent_sessions),
        cmocka_unit_test(test_null_and_incomplete_inputs_are_typed_and_atomic)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}