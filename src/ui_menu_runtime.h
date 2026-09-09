/** ui_menu_runtime.h — Headless runtime host for one authored Menu. */
#ifndef UI_MENU_RUNTIME_H
#define UI_MENU_RUNTIME_H

#include "flow_binding.h"
#include "ui_render_adapter.h"

typedef struct {
    bool disabled;
    bool visible;
} UiMenuRuntimeElementState;

typedef enum {
    UI_MENU_INPUT_FOCUS_NEXT = 0,
    UI_MENU_INPUT_FOCUS_PREVIOUS,
    UI_MENU_INPUT_FOCUS_LEFT,
    UI_MENU_INPUT_FOCUS_RIGHT,
    UI_MENU_INPUT_FOCUS_UP,
    UI_MENU_INPUT_FOCUS_DOWN,
    UI_MENU_INPUT_POINTER_DOWN,
    UI_MENU_INPUT_POINTER_UP,
    UI_MENU_INPUT_CONFIRM_DOWN,
    UI_MENU_INPUT_CONFIRM_UP
} UiMenuRuntimeInputType;

typedef struct {
    UiMenuRuntimeInputType type;
    int pointer_x;
    int pointer_y;
} UiMenuRuntimeInput;

typedef struct {
    const UiDocument *document;
    const AssetRegistry *assets;
    const UiRenderTheme *theme;
    const FlowDocument *flow_document;
    FlowRuntimeSession *flow_session;
    UiInteractionSession interaction;
    UiMenuRuntimeElementState element_states[UI_DOCUMENT_MAX_ELEMENTS];
    size_t element_count;
    UiElementId pressed_element_id;
    int viewport_width;
    int viewport_height;
    bool active;
} UiMenuRuntime;

typedef enum {
    UI_MENU_RUNTIME_OK = 0,
    UI_MENU_RUNTIME_INVALID_ARGUMENT,
    UI_MENU_RUNTIME_INVALID_DOCUMENT,
    UI_MENU_RUNTIME_INVALID_DEPENDENCY,
    UI_MENU_RUNTIME_INVALID_FLOW_STATE,
    UI_MENU_RUNTIME_INVALID_ELEMENT,
    UI_MENU_RUNTIME_INACTIVE,
    UI_MENU_RUNTIME_NO_ACTION,
    UI_MENU_RUNTIME_INTERACTION_ERROR,
    UI_MENU_RUNTIME_RENDER_ERROR,
    UI_MENU_RUNTIME_FLOW_ERROR
} UiMenuRuntimeResult;

void ui_menu_runtime_init(UiMenuRuntime *runtime);
UiMenuRuntimeResult ui_menu_runtime_activate(
    UiMenuRuntime *runtime,
    const UiDocument *document,
    const AssetRegistry *assets,
    const UiRenderTheme *theme,
    const FlowDocument *flow_document,
    FlowRuntimeSession *flow_session,
    int viewport_width,
    int viewport_height
);
UiMenuRuntimeResult ui_menu_runtime_set_element_state(
    UiMenuRuntime *runtime,
    UiElementId element_id,
    bool disabled,
    bool visible
);
UiMenuRuntimeResult ui_menu_runtime_render(
    const UiMenuRuntime *runtime,
    UiCanvas *canvas
);
UiMenuRuntimeResult ui_menu_runtime_handle_input(
    UiMenuRuntime *runtime,
    const UiMenuRuntimeInput *input,
    FlowBindingTargetRequest *out_request
);

#endif /* UI_MENU_RUNTIME_H */