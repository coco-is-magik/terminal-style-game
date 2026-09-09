#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <string.h>

#include "../src/scene_flow_adapter.h"

static SceneTrigger exit_trigger(SceneInstanceId id, const char *port) {
    SceneTrigger trigger = {
        .id = id,
        .condition = SCENE_TRIGGER_CONDITION_ENTER_REGION,
        .action = SCENE_TRIGGER_ACTION_EXIT_FLOW
    };
    memcpy(trigger.flow_port, port, strlen(port) + 1U);
    return trigger;
}

static void test_builds_scene_entry_and_validates_graph(void **state) {
    SceneDocument scene = {0};
    SceneTrigger triggers[] = {exit_trigger(3U, "complete"), exit_trigger(4U, "back")};
    SceneFlowReferenceView view;
    FlowDocument flow;
    FlowNodeId scene_node;
    FlowNodeId menu_node;
    FlowEdgeId edge;
    FlowReferenceEntry entries[2];
    const char *menu_ports[] = {"play"};
    FlowReferenceCatalog catalog = {entries, 2U};
    (void)state;
    memcpy(scene.name, "mission", sizeof("mission"));
    scene.triggers = triggers;
    scene.trigger_count = 2U;
    assert_int_equal(scene_flow_reference_view_build(&scene, &view),
                     SCENE_FLOW_ADAPTER_OK);
    assert_int_equal(view.entry.type, FLOW_NODE_SCENE);
    assert_string_equal(view.entry.name, "mission");
    assert_int_equal(view.entry.port_count, 2U);
    assert_string_equal(view.entry.ports[0], "complete");
    assert_string_equal(view.entry.ports[1], "back");
    entries[0] = view.entry;
    entries[1] = (FlowReferenceEntry){FLOW_NODE_MENU, "missions", menu_ports, 1U};
    flow_document_init(&flow);
    assert_int_equal(flow_document_add_node(&flow, FLOW_NODE_MENU, "missions", &menu_node),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_add_node(&flow, FLOW_NODE_SCENE, "mission", &scene_node),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&flow, 1U, "start", menu_node, &edge),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&flow, menu_node, "play", scene_node, &edge),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&flow, scene_node, "complete", menu_node, &edge),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_reference_validate_document(&flow, &catalog), FLOW_REFERENCE_OK);
}

static void test_rejects_duplicate_invalid_and_excess_ports_atomically(void **state) {
    SceneDocument scene = {0};
    SceneTrigger triggers[SCENE_MAX_FLOW_EXITS + 1U];
    SceneFlowReferenceView view;
    SceneFlowReferenceView before;
    size_t i;
    (void)state;
    memset(&view, 0x5a, sizeof(view));
    before = view;
    memcpy(scene.name, "mission", sizeof("mission"));
    scene.triggers = triggers;
    scene.trigger_count = 2U;
    triggers[0] = exit_trigger(1U, "exit");
    triggers[1] = exit_trigger(2U, "exit");
    assert_int_equal(scene_flow_reference_view_build(&scene, &view),
                     SCENE_FLOW_ADAPTER_DUPLICATE_PORT);
    assert_memory_equal(&view, &before, sizeof(view));
    memset(scene.name, 'x', sizeof(scene.name));
    assert_int_equal(scene_flow_reference_view_build(&scene, &view),
                     SCENE_FLOW_ADAPTER_INVALID_SCENE);
    assert_memory_equal(&view, &before, sizeof(view));
    scene.triggers = triggers;
    scene.trigger_count = 1U;
    memcpy(scene.name, "mission", sizeof("mission"));
    triggers[0] = exit_trigger(1U, "exit");
    memset(triggers[0].flow_port, 'x', sizeof(triggers[0].flow_port));
    assert_int_equal(scene_flow_reference_view_build(&scene, &view),
                     SCENE_FLOW_ADAPTER_INVALID_SCENE);
    assert_memory_equal(&view, &before, sizeof(view));
    triggers[1].flow_port[0] = '\0';
    assert_int_equal(scene_flow_reference_view_build(&scene, &view),
                     SCENE_FLOW_ADAPTER_INVALID_SCENE);
    for (i = 0U; i < SCENE_MAX_FLOW_EXITS + 1U; i++) {
        char port[SCENE_TRIGGER_FLOW_PORT_CAPACITY];
        snprintf(port, sizeof(port), "exit-%zu", i);
        triggers[i] = exit_trigger((SceneInstanceId)i + 1U, port);
    }
    scene.trigger_count = SCENE_MAX_FLOW_EXITS + 1U;
    assert_int_equal(scene_flow_reference_view_build(&scene, &view),
                     SCENE_FLOW_ADAPTER_TOO_MANY_PORTS);
    assert_memory_equal(&view, &before, sizeof(view));
    assert_int_equal(scene_flow_reference_view_build(NULL, &view),
                     SCENE_FLOW_ADAPTER_INVALID_ARGUMENT);
    scene.trigger_count = 1U;
    scene.triggers = NULL;
    assert_int_equal(scene_flow_reference_view_build(&scene, &view),
                     SCENE_FLOW_ADAPTER_INVALID_SCENE);
    assert_memory_equal(&view, &before, sizeof(view));
    scene.trigger_count = 0U;
    memcpy(scene.name, "bad/name", sizeof("bad/name"));
    assert_int_equal(scene_flow_reference_view_build(&scene, &view),
                     SCENE_FLOW_ADAPTER_INVALID_SCENE);
    assert_memory_equal(&view, &before, sizeof(view));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_builds_scene_entry_and_validates_graph),
        cmocka_unit_test(test_rejects_duplicate_invalid_and_excess_ports_atomically)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}