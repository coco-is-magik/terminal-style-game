#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdlib.h>
#include <string.h>

#include "../src/ui_menu_runtime.h"

static const UiRenderTheme theme = {
    {255, 255, 0, 255}, {0, 0, 80, 255},
    {0, 0, 0, 255}, {255, 255, 0, 255},
    {128, 128, 128, 255}, {20, 20, 20, 255}
};

typedef struct {
    UiDocument menu;
    UiElementId play;
    UiElementId back;
    FlowDocument flow;
    FlowReferenceCatalog catalog;
    FlowReferenceEntry entries[2];
    const char *menu_ports[2];
    const char *scene_ports[1];
    FlowNodeId menu_id;
    FlowNodeId scene_id;
    FlowRuntimeSession flow_session;
    AssetRegistry assets;
} Fixture;

static void fixture_init(Fixture *value) {
    FlowEdgeId edge;
    const FlowNode *target;
    memset(value, 0, sizeof(*value));
    assert_int_equal(ui_document_create_menu(&value->menu, "missions"),
                     UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&value->menu,
        UI_DOCUMENT_ELEMENT_BUTTON, 1U, "play", "PLAY", "play", &value->play),
        UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_layout(&value->menu, value->play,
        (UiDocumentLayout){1, 1, 6, 1, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&value->menu,
        UI_DOCUMENT_ELEMENT_BUTTON, 1U, "back", "BACK", "back", &value->back),
        UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_layout(&value->menu, value->back,
        (UiDocumentLayout){1, 3, 6, 1, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}), UI_DOCUMENT_OK);
    value->menu_ports[0] = "play";
    value->menu_ports[1] = "back";
    value->scene_ports[0] = "complete";
    value->entries[0] = (FlowReferenceEntry){
        FLOW_NODE_MENU, "missions", value->menu_ports, 2U
    };
    value->entries[1] = (FlowReferenceEntry){
        FLOW_NODE_SCENE, "mission", value->scene_ports, 1U
    };
    value->catalog = (FlowReferenceCatalog){value->entries, 2U};
    flow_document_init(&value->flow);
    assert_int_equal(flow_document_add_node(&value->flow, FLOW_NODE_MENU,
        "missions", &value->menu_id), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_add_node(&value->flow, FLOW_NODE_SCENE,
        "mission", &value->scene_id), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&value->flow, 1U, "start",
        value->menu_id, &edge), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&value->flow, value->menu_id, "play",
        value->scene_id, &edge), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&value->flow, value->menu_id, "back",
        value->scene_id, &edge), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&value->flow, value->scene_id,
        "complete", value->menu_id, &edge), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_runtime_session_init(&value->flow_session, &value->flow,
        &value->catalog), FLOW_RUNTIME_OK);
    assert_int_equal(flow_runtime_transition(&value->flow_session, &value->flow,
        "start", &target), FLOW_RUNTIME_OK);
    assert_true(asset_registry_init(&value->assets));
}

static void fixture_destroy(Fixture *value) {
    asset_registry_clear(&value->assets);
}

static UiMenuRuntime active_runtime(Fixture *fixture) {
    UiMenuRuntime runtime;
    ui_menu_runtime_init(&runtime);
    assert_int_equal(ui_menu_runtime_activate(&runtime, &fixture->menu,
        &fixture->assets, &theme, &fixture->flow, &fixture->flow_session, 12, 6),
        UI_MENU_RUNTIME_OK);
    return runtime;
}

static void test_activation_render_and_confirm_target(void **state) {
    Fixture f;
    UiMenuRuntime runtime;
    UiCanvas *canvas;
    FlowBindingTargetRequest request;
    (void)state;
    fixture_init(&f);
    runtime = active_runtime(&f);
    canvas = ui_canvas_create(12, 6);
    assert_non_null(canvas);
    assert_true(runtime.active);
    assert_int_equal(runtime.interaction.focused_element_id, f.play);
    assert_int_equal(ui_menu_runtime_render(&runtime, canvas), UI_MENU_RUNTIME_OK);
    assert_int_equal(canvas->cells[1 * canvas->width + 1].glyph, '>');
    assert_int_equal(ui_menu_runtime_handle_input(&runtime,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_CONFIRM_DOWN, 0, 0}, &request),
        UI_MENU_RUNTIME_OK);
    assert_int_equal(runtime.pressed_element_id, f.play);
    assert_int_equal(ui_menu_runtime_render(&runtime, canvas), UI_MENU_RUNTIME_OK);
    assert_int_equal(canvas->cells[1 * canvas->width + 1].glyph, '#');
    assert_int_equal(ui_menu_runtime_handle_input(&runtime,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_CONFIRM_UP, 0, 0}, &request),
        UI_MENU_RUNTIME_OK);
    assert_false(runtime.active);
    assert_int_equal(runtime.pressed_element_id, 0U);
    assert_int_equal(request.node_id, f.scene_id);
    assert_int_equal(request.type, FLOW_NODE_SCENE);
    assert_string_equal(request.asset_name, "mission");
    assert_int_equal(f.flow_session.current_node_id, f.scene_id);
    assert_int_equal(ui_menu_runtime_render(&runtime, canvas), UI_MENU_RUNTIME_INACTIVE);
    ui_canvas_destroy(canvas);
    fixture_destroy(&f);
}

static void test_navigation_pointer_press_release_and_mismatch(void **state) {
    Fixture f;
    UiMenuRuntime runtime;
    FlowBindingTargetRequest request = {99U, FLOW_NODE_START, "sentinel"};
    FlowBindingTargetRequest before = request;
    (void)state;
    fixture_init(&f);
    runtime = active_runtime(&f);
    assert_int_equal(ui_menu_runtime_handle_input(&runtime,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_FOCUS_NEXT, 0, 0}, &request),
        UI_MENU_RUNTIME_OK);
    assert_int_equal(runtime.interaction.focused_element_id, f.back);
    assert_int_equal(ui_menu_runtime_handle_input(&runtime,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_FOCUS_UP, 0, 0}, &request),
        UI_MENU_RUNTIME_OK);
    assert_int_equal(runtime.interaction.focused_element_id, f.play);
    assert_int_equal(ui_menu_runtime_handle_input(&runtime,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_FOCUS_LEFT, 0, 0}, &request),
        UI_MENU_RUNTIME_NO_ACTION);
    assert_int_equal(ui_menu_runtime_handle_input(&runtime,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_FOCUS_RIGHT, 0, 0}, &request),
        UI_MENU_RUNTIME_NO_ACTION);
    assert_int_equal(ui_menu_runtime_handle_input(&runtime,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_FOCUS_NEXT, 0, 0}, &request),
        UI_MENU_RUNTIME_OK);
    assert_int_equal(runtime.interaction.focused_element_id, f.back);
    assert_int_equal(ui_menu_runtime_handle_input(&runtime,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_FOCUS_PREVIOUS, 0, 0}, &request),
        UI_MENU_RUNTIME_OK);
    assert_int_equal(runtime.interaction.focused_element_id, f.play);
    assert_int_equal(ui_menu_runtime_handle_input(&runtime,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_FOCUS_DOWN, 0, 0}, &request),
        UI_MENU_RUNTIME_OK);
    assert_int_equal(runtime.interaction.focused_element_id, f.back);
    assert_int_equal(ui_menu_runtime_handle_input(&runtime,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_POINTER_DOWN, 2, 1}, &request),
        UI_MENU_RUNTIME_OK);
    assert_int_equal(runtime.interaction.focused_element_id, f.play);
    assert_int_equal(runtime.pressed_element_id, f.play);
    assert_int_equal(ui_menu_runtime_handle_input(&runtime,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_POINTER_UP, 2, 3}, &request),
        UI_MENU_RUNTIME_NO_ACTION);
    assert_int_equal(runtime.pressed_element_id, 0U);
    assert_memory_equal(&request, &before, sizeof(request));
    assert_int_equal(f.flow_session.current_node_id, f.menu_id);
    assert_int_equal(ui_menu_runtime_handle_input(&runtime,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_POINTER_DOWN, 2, 3}, &request),
        UI_MENU_RUNTIME_OK);
    assert_int_equal(ui_menu_runtime_handle_input(&runtime,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_POINTER_UP, 2, 3}, &request),
        UI_MENU_RUNTIME_OK);
    assert_int_equal(request.node_id, f.scene_id);
    fixture_destroy(&f);
}

static void test_state_eligibility_and_no_action(void **state) {
    Fixture f;
    UiMenuRuntime runtime;
    FlowBindingTargetRequest request = {99U, FLOW_NODE_START, "sentinel"};
    FlowBindingTargetRequest before = request;
    (void)state;
    fixture_init(&f);
    runtime = active_runtime(&f);
    assert_int_equal(ui_menu_runtime_set_element_state(&runtime, f.play, true, true),
                     UI_MENU_RUNTIME_OK);
    assert_int_equal(ui_menu_runtime_handle_input(&runtime,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_FOCUS_NEXT, 0, 0}, &request),
        UI_MENU_RUNTIME_OK);
    assert_int_equal(runtime.interaction.focused_element_id, f.back);
    assert_int_equal(ui_menu_runtime_set_element_state(&runtime, f.back, false, false),
                     UI_MENU_RUNTIME_OK);
    assert_int_equal(ui_menu_runtime_handle_input(&runtime,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_CONFIRM_DOWN, 0, 0}, &request),
        UI_MENU_RUNTIME_INTERACTION_ERROR);
    assert_int_equal(runtime.pressed_element_id, 0U);
    assert_memory_equal(&request, &before, sizeof(request));
    assert_int_equal(ui_menu_runtime_set_element_state(&runtime, 999U, false, true),
                     UI_MENU_RUNTIME_INVALID_ELEMENT);
    fixture_destroy(&f);
}

static void test_activation_reset_is_transactional(void **state) {
    Fixture f;
    UiMenuRuntime runtime;
    UiMenuRuntime before;
    UiDocument wrong;
    FlowRuntimeSession wrong_session;
    UiDocument other;
    FlowNodeId other_id;
    FlowEdgeId edge;
    UiElementId continue_id;
    AssetRegistry invalid_assets = {0};
    (void)state;
    fixture_init(&f);
    runtime = active_runtime(&f);
    runtime.pressed_element_id = f.play;
    before = runtime;
    assert_int_equal(ui_document_create_menu(&wrong, "other"), UI_DOCUMENT_OK);
    assert_int_equal(ui_menu_runtime_activate(&runtime, &wrong, &f.assets, &theme,
        &f.flow, &f.flow_session, 12, 6), UI_MENU_RUNTIME_INVALID_FLOW_STATE);
    assert_memory_equal(&runtime, &before, sizeof(runtime));
    assert_int_equal(ui_menu_runtime_activate(&runtime, &f.menu, &invalid_assets,
        &theme, &f.flow, &f.flow_session, 12, 6),
        UI_MENU_RUNTIME_INVALID_DEPENDENCY);
    assert_memory_equal(&runtime, &before, sizeof(runtime));
    f.flow.edges[0].target_id = 999U;
    assert_int_equal(ui_menu_runtime_activate(&runtime, &f.menu, &f.assets, &theme,
        &f.flow, &f.flow_session, 12, 6), UI_MENU_RUNTIME_INVALID_FLOW_STATE);
    assert_memory_equal(&runtime, &before, sizeof(runtime));
    f.flow.edges[0].target_id = f.menu_id;
    wrong_session = f.flow_session;
    wrong_session.current_node_id = 999U;
    assert_int_equal(ui_menu_runtime_activate(&runtime, &f.menu, &f.assets, &theme,
        &f.flow, &wrong_session, 12, 6), UI_MENU_RUNTIME_INVALID_FLOW_STATE);
    assert_memory_equal(&runtime, &before, sizeof(runtime));
    assert_int_equal(ui_menu_runtime_activate(&runtime, &f.menu, &f.assets, &theme,
        &f.flow, &f.flow_session, 12, 6), UI_MENU_RUNTIME_OK);
    assert_int_equal(runtime.pressed_element_id, 0U);
    assert_int_equal(runtime.interaction.focused_element_id, f.play);
    assert_int_equal(ui_document_create_menu(&other, "other"), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&other, UI_DOCUMENT_ELEMENT_BUTTON,
        1U, "continue", "CONTINUE", "continue", &continue_id), UI_DOCUMENT_OK);
    assert_int_equal(flow_document_add_node(&f.flow, FLOW_NODE_MENU, "other",
        &other_id), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&f.flow, f.scene_id, "other", other_id,
        &edge), FLOW_DOCUMENT_OK);
    f.flow_session.current_node_id = other_id;
    assert_int_equal(ui_menu_runtime_activate(&runtime, &other, &f.assets, &theme,
        &f.flow, &f.flow_session, 12, 6), UI_MENU_RUNTIME_OK);
    assert_ptr_equal(runtime.document, &other);
    assert_int_equal(runtime.interaction.focused_element_id, continue_id);
    assert_int_equal(runtime.pressed_element_id, 0U);
    fixture_destroy(&f);
}

static void test_render_and_flow_failures_preserve_external_outputs(void **state) {
    Fixture f;
    UiMenuRuntime runtime;
    UiCanvas *canvas;
    UiCanvas *wrong_canvas;
    Cell saved_cell;
    uint8_t saved_touched;
    FlowBindingTargetRequest request = {99U, FLOW_NODE_START, "sentinel"};
    FlowBindingTargetRequest before = request;
    FlowNodeId flow_before;
    (void)state;
    fixture_init(&f);
    runtime = active_runtime(&f);
    canvas = ui_canvas_create(12, 6);
    assert_non_null(canvas);
    wrong_canvas = ui_canvas_create(11, 6);
    assert_non_null(wrong_canvas);
    wrong_canvas->cells[0].glyph = 'Y';
    wrong_canvas->touched[0] = 1U;
    assert_int_equal(ui_menu_runtime_render(&runtime, wrong_canvas),
                     UI_MENU_RUNTIME_INVALID_ARGUMENT);
    assert_int_equal(wrong_canvas->cells[0].glyph, 'Y');
    assert_int_equal(wrong_canvas->touched[0], 1U);
    canvas->cells[0].glyph = 'X';
    canvas->touched[0] = 1U;
    saved_cell = canvas->cells[0];
    saved_touched = canvas->touched[0];
    runtime.assets = NULL;
    assert_int_equal(ui_menu_runtime_render(&runtime, canvas),
                     UI_MENU_RUNTIME_INVALID_DEPENDENCY);
    assert_memory_equal(&canvas->cells[0], &saved_cell, sizeof(saved_cell));
    assert_int_equal(canvas->touched[0], saved_touched);
    runtime.assets = &f.assets;
    {
        UiDocumentVisual visual = f.menu.elements[1].visual;
        visual.mode = UI_DOCUMENT_VISUAL_SPRITE;
        visual.sprite_id = 99U;
        assert_int_equal(ui_document_set_visual(&f.menu, f.play, visual),
                         UI_DOCUMENT_OK);
        assert_int_equal(ui_menu_runtime_render(&runtime, canvas),
                         UI_MENU_RUNTIME_RENDER_ERROR);
        assert_memory_equal(&canvas->cells[0], &saved_cell, sizeof(saved_cell));
        assert_int_equal(canvas->touched[0], saved_touched);
        visual.mode = UI_DOCUMENT_VISUAL_NATIVE;
        visual.sprite_id = 0U;
        assert_int_equal(ui_document_set_visual(&f.menu, f.play, visual),
                         UI_DOCUMENT_OK);
    }
    assert_int_equal(ui_menu_runtime_handle_input(&runtime,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_CONFIRM_DOWN, 0, 0}, &request),
        UI_MENU_RUNTIME_OK);
    f.flow.edges[1].source_port[0] = 'x';
    flow_before = f.flow_session.current_node_id;
    assert_int_equal(ui_menu_runtime_handle_input(&runtime,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_CONFIRM_UP, 0, 0}, &request),
        UI_MENU_RUNTIME_FLOW_ERROR);
    assert_true(runtime.active);
    assert_int_equal(f.flow_session.current_node_id, flow_before);
    assert_memory_equal(&request, &before, sizeof(request));
    ui_canvas_destroy(canvas);
    ui_canvas_destroy(wrong_canvas);
    fixture_destroy(&f);
}

static void test_deterministic_replay_and_invalid_input(void **state) {
    Fixture f;
    UiMenuRuntime first;
    UiMenuRuntime second;
    FlowRuntimeSession second_flow;
    FlowBindingTargetRequest request = {99U, FLOW_NODE_START, "sentinel"};
    FlowBindingTargetRequest before = request;
    (void)state;
    fixture_init(&f);
    first = active_runtime(&f);
    second_flow = f.flow_session;
    ui_menu_runtime_init(&second);
    assert_int_equal(ui_menu_runtime_activate(&second, &f.menu, &f.assets, &theme,
        &f.flow, &second_flow, 12, 6), UI_MENU_RUNTIME_OK);
    assert_int_equal(ui_menu_runtime_handle_input(&first,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_FOCUS_NEXT, 0, 0}, &request),
        UI_MENU_RUNTIME_OK);
    assert_int_equal(ui_menu_runtime_handle_input(&second,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_FOCUS_NEXT, 0, 0}, &request),
        UI_MENU_RUNTIME_OK);
    assert_int_equal(first.interaction.focused_element_id,
                     second.interaction.focused_element_id);
    assert_int_equal(ui_menu_runtime_handle_input(&first,
        &(UiMenuRuntimeInput){(UiMenuRuntimeInputType)99, 0, 0}, &request),
        UI_MENU_RUNTIME_INVALID_ARGUMENT);
    assert_memory_equal(&request, &before, sizeof(request));
    assert_int_equal(ui_menu_runtime_handle_input(&first,
        &(UiMenuRuntimeInput){UI_MENU_INPUT_FOCUS_NEXT, 0, 0}, NULL),
        UI_MENU_RUNTIME_INVALID_ARGUMENT);
    {
        UiMenuRuntime corrupted = first;
        UiMenuRuntime before_runtime;
        corrupted.document = NULL;
        before_runtime = corrupted;
        assert_int_equal(ui_menu_runtime_handle_input(&corrupted,
            &(UiMenuRuntimeInput){UI_MENU_INPUT_FOCUS_NEXT, 0, 0}, &request),
            UI_MENU_RUNTIME_INVALID_DEPENDENCY);
        assert_memory_equal(&corrupted, &before_runtime, sizeof(corrupted));
        assert_int_equal(ui_menu_runtime_set_element_state(&corrupted, f.play,
            false, true), UI_MENU_RUNTIME_INVALID_DEPENDENCY);
    }
    fixture_destroy(&f);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_activation_render_and_confirm_target),
        cmocka_unit_test(test_navigation_pointer_press_release_and_mismatch),
        cmocka_unit_test(test_state_eligibility_and_no_action),
        cmocka_unit_test(test_activation_reset_is_transactional),
        cmocka_unit_test(test_render_and_flow_failures_preserve_external_outputs),
        cmocka_unit_test(test_deterministic_replay_and_invalid_input)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}