#include "ui_menu_runtime.h"

#include <string.h>

static void build_interaction_states(
    const UiMenuRuntime *runtime,
    UiInteractionElementState states[UI_DOCUMENT_MAX_ELEMENTS]
) {
    size_t i;
    for (i = 0U; i < runtime->element_count; i++) {
        states[i].element_id = runtime->document->elements[i].id;
        states[i].disabled = runtime->element_states[i].disabled;
        states[i].visible = runtime->element_states[i].visible;
    }
}

static void build_render_states(
    const UiMenuRuntime *runtime,
    UiRenderElementState states[UI_DOCUMENT_MAX_ELEMENTS]
) {
    size_t i;
    for (i = 0U; i < runtime->element_count; i++) {
        UiElementId id = runtime->document->elements[i].id;
        states[i].element_id = id;
        states[i].focused = runtime->interaction.focused_element_id == id;
        states[i].pressed = runtime->pressed_element_id == id;
        states[i].disabled = runtime->element_states[i].disabled;
        states[i].visible = runtime->element_states[i].visible;
        states[i].preserve_authored_colors = false;
    }
}

static size_t element_index(const UiMenuRuntime *runtime, UiElementId id) {
    size_t i;
    for (i = 0U; i < runtime->element_count; i++)
        if (runtime->document->elements[i].id == id) return i;
    return runtime->element_count;
}

static bool runtime_dependencies_valid(const UiMenuRuntime *runtime,
                                       bool require_render_dependencies) {
    if (!runtime->document || !runtime->flow_document || !runtime->flow_session ||
        runtime->element_count == 0U ||
        runtime->element_count > UI_DOCUMENT_MAX_ELEMENTS ||
        runtime->element_count != runtime->document->element_count ||
        runtime->viewport_width <= 0 || runtime->viewport_height <= 0)
        return false;
    if (require_render_dependencies &&
        (!runtime->assets || !runtime->theme || !runtime->assets->materials ||
         !runtime->assets->palettes || !runtime->assets->material_names))
        return false;
    return true;
}

void ui_menu_runtime_init(UiMenuRuntime *runtime) {
    if (runtime) memset(runtime, 0, sizeof(*runtime));
}

UiMenuRuntimeResult ui_menu_runtime_activate(
    UiMenuRuntime *runtime, const UiDocument *document,
    const AssetRegistry *assets, const UiRenderTheme *theme,
    const FlowDocument *flow_document, FlowRuntimeSession *flow_session,
    int viewport_width, int viewport_height
) {
    UiMenuRuntime candidate;
    UiInteractionElementState states[UI_DOCUMENT_MAX_ELEMENTS];
    const FlowNode *node;
    size_t i;
    if (!runtime || !document || !assets || !theme || !flow_document ||
        !flow_session || viewport_width <= 0 || viewport_height <= 0)
        return UI_MENU_RUNTIME_INVALID_ARGUMENT;
    if (ui_document_validate(document) != UI_DOCUMENT_OK ||
        document->kind != UI_DOCUMENT_KIND_MENU)
        return UI_MENU_RUNTIME_INVALID_DOCUMENT;
    if (!assets->materials || !assets->palettes || !assets->material_names)
        return UI_MENU_RUNTIME_INVALID_DEPENDENCY;
    if (flow_document_validate(flow_document) != FLOW_DOCUMENT_OK)
        return UI_MENU_RUNTIME_INVALID_FLOW_STATE;
    node = flow_runtime_current_node(flow_session, flow_document);
    if (!node || node->type != FLOW_NODE_MENU ||
        strcmp(node->asset_name, document->name) != 0)
        return UI_MENU_RUNTIME_INVALID_FLOW_STATE;
    ui_menu_runtime_init(&candidate);
    candidate.document = document;
    candidate.assets = assets;
    candidate.theme = theme;
    candidate.flow_document = flow_document;
    candidate.flow_session = flow_session;
    candidate.element_count = document->element_count;
    candidate.viewport_width = viewport_width;
    candidate.viewport_height = viewport_height;
    candidate.active = true;
    for (i = 0U; i < candidate.element_count; i++)
        candidate.element_states[i].visible =
            document->elements[i].visual.visible_by_default;
    build_interaction_states(&candidate, states);
    if (ui_interaction_session_init(&candidate.interaction, document, states,
        candidate.element_count, viewport_width, viewport_height) != UI_INTERACTION_OK)
        return UI_MENU_RUNTIME_INVALID_DOCUMENT;
    *runtime = candidate;
    return UI_MENU_RUNTIME_OK;
}

UiMenuRuntimeResult ui_menu_runtime_set_element_state(
    UiMenuRuntime *runtime, UiElementId element_id, bool disabled, bool visible
) {
    size_t index;
    if (!runtime || element_id == 0U) return UI_MENU_RUNTIME_INVALID_ARGUMENT;
    if (!runtime->active) return UI_MENU_RUNTIME_INACTIVE;
    if (!runtime_dependencies_valid(runtime, false))
        return UI_MENU_RUNTIME_INVALID_DEPENDENCY;
    index = element_index(runtime, element_id);
    if (index >= runtime->element_count) return UI_MENU_RUNTIME_INVALID_ELEMENT;
    runtime->element_states[index].disabled = disabled;
    runtime->element_states[index].visible = visible;
    if ((disabled || !visible) && runtime->pressed_element_id == element_id)
        runtime->pressed_element_id = 0U;
    return UI_MENU_RUNTIME_OK;
}

UiMenuRuntimeResult ui_menu_runtime_render(
    const UiMenuRuntime *runtime, UiCanvas *canvas
) {
    UiRenderElementState states[UI_DOCUMENT_MAX_ELEMENTS];
    if (!runtime || !canvas) return UI_MENU_RUNTIME_INVALID_ARGUMENT;
    if (!runtime->active) return UI_MENU_RUNTIME_INACTIVE;
    if (!runtime_dependencies_valid(runtime, true))
        return UI_MENU_RUNTIME_INVALID_DEPENDENCY;
    if (canvas->width != runtime->viewport_width ||
        canvas->height != runtime->viewport_height)
        return UI_MENU_RUNTIME_INVALID_ARGUMENT;
    build_render_states(runtime, states);
    return ui_render_document(runtime->document, runtime->assets, states,
        runtime->element_count, runtime->theme, canvas) == UI_RENDER_OK
        ? UI_MENU_RUNTIME_OK : UI_MENU_RUNTIME_RENDER_ERROR;
}

static UiMenuRuntimeResult interaction_result(UiInteractionResult result) {
    if (result == UI_INTERACTION_OK) return UI_MENU_RUNTIME_OK;
    if (result == UI_INTERACTION_NO_HIT ||
        result == UI_INTERACTION_NO_DIRECTIONAL_TARGET ||
        result == UI_INTERACTION_NO_ELIGIBLE)
        return UI_MENU_RUNTIME_NO_ACTION;
    return UI_MENU_RUNTIME_INTERACTION_ERROR;
}

static UiMenuRuntimeResult activate_pressed(
    UiMenuRuntime *runtime, UiElementId required_id,
    FlowBindingTargetRequest *out_request
) {
    UiInteractionElementState states[UI_DOCUMENT_MAX_ELEMENTS];
    UiInteractionActivation activation;
    FlowBindingResult result;
    if (runtime->pressed_element_id == 0U ||
        runtime->pressed_element_id != required_id) {
        runtime->pressed_element_id = 0U;
        return UI_MENU_RUNTIME_NO_ACTION;
    }
    build_interaction_states(runtime, states);
    if (ui_interaction_activate(&runtime->interaction, runtime->document, states,
        runtime->element_count, runtime->viewport_width, runtime->viewport_height,
        &activation) != UI_INTERACTION_OK) {
        runtime->pressed_element_id = 0U;
        return UI_MENU_RUNTIME_INTERACTION_ERROR;
    }
    result = flow_binding_activate_button(runtime->flow_session,
        runtime->flow_document, &activation, out_request);
    runtime->pressed_element_id = 0U;
    if (result != FLOW_BINDING_OK) return UI_MENU_RUNTIME_FLOW_ERROR;
    runtime->active = false;
    return UI_MENU_RUNTIME_OK;
}

UiMenuRuntimeResult ui_menu_runtime_handle_input(
    UiMenuRuntime *runtime, const UiMenuRuntimeInput *input,
    FlowBindingTargetRequest *out_request
) {
    UiInteractionElementState states[UI_DOCUMENT_MAX_ELEMENTS];
    UiInteractionResult result;
    UiInteractionDirection direction;
    UiInteractionSession interaction;
    UiElementId hit;
    if (!runtime || !input || !out_request)
        return UI_MENU_RUNTIME_INVALID_ARGUMENT;
    if (!runtime->active) return UI_MENU_RUNTIME_INACTIVE;
    if (!runtime_dependencies_valid(runtime, false))
        return UI_MENU_RUNTIME_INVALID_DEPENDENCY;
    build_interaction_states(runtime, states);
    if (input->type == UI_MENU_INPUT_FOCUS_NEXT ||
        input->type == UI_MENU_INPUT_FOCUS_PREVIOUS) {
        interaction = runtime->interaction;
        result = ui_interaction_focus_next(&interaction, runtime->document,
            states, runtime->element_count, runtime->viewport_width,
            runtime->viewport_height,
            input->type == UI_MENU_INPUT_FOCUS_PREVIOUS);
        if (result == UI_INTERACTION_OK || result == UI_INTERACTION_NO_ELIGIBLE) {
            runtime->interaction = interaction;
            runtime->pressed_element_id = 0U;
        }
        return interaction_result(result);
    }
    if (input->type >= UI_MENU_INPUT_FOCUS_LEFT &&
        input->type <= UI_MENU_INPUT_FOCUS_DOWN) {
        interaction = runtime->interaction;
        direction = (UiInteractionDirection)(input->type - UI_MENU_INPUT_FOCUS_LEFT);
        result = ui_interaction_focus_direction(&interaction,
            runtime->document, states, runtime->element_count,
            runtime->viewport_width, runtime->viewport_height, direction);
        if (result == UI_INTERACTION_OK ||
            result == UI_INTERACTION_NO_DIRECTIONAL_TARGET) {
            runtime->interaction = interaction;
            runtime->pressed_element_id = 0U;
        }
        return interaction_result(result);
    }
    if (input->type == UI_MENU_INPUT_POINTER_DOWN) {
        result = ui_interaction_focus_pointer(&runtime->interaction,
            runtime->document, states, runtime->element_count,
            runtime->viewport_width, runtime->viewport_height,
            input->pointer_x, input->pointer_y);
        runtime->pressed_element_id = result == UI_INTERACTION_OK
            ? runtime->interaction.focused_element_id : 0U;
        return interaction_result(result);
    }
    if (input->type == UI_MENU_INPUT_POINTER_UP) {
        hit = 0U;
        result = ui_interaction_hit_test(runtime->document, states,
            runtime->element_count, runtime->viewport_width,
            runtime->viewport_height, input->pointer_x, input->pointer_y, &hit);
        if (result != UI_INTERACTION_OK) {
            runtime->pressed_element_id = 0U;
            return interaction_result(result);
        }
        return activate_pressed(runtime, hit, out_request);
    }
    if (input->type == UI_MENU_INPUT_CONFIRM_DOWN) {
        UiInteractionActivation activation;
        result = ui_interaction_activate(&runtime->interaction, runtime->document,
            states, runtime->element_count, runtime->viewport_width,
            runtime->viewport_height, &activation);
        runtime->pressed_element_id = result == UI_INTERACTION_OK
            ? activation.element_id : 0U;
        return interaction_result(result);
    }
    if (input->type == UI_MENU_INPUT_CONFIRM_UP)
        return activate_pressed(runtime, runtime->interaction.focused_element_id,
                                out_request);
    return UI_MENU_RUNTIME_INVALID_ARGUMENT;
}