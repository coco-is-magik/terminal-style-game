/** ui_render_adapter.h — Headless authored UI rendering into UiCanvas. */
#ifndef UI_RENDER_ADAPTER_H
#define UI_RENDER_ADAPTER_H

#include "assets.h"
#include "ui_canvas.h"
#include "ui_layout_resolver.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    UiElementId element_id;
    bool focused;
    bool pressed;
    bool disabled;
    bool visible;
    bool preserve_authored_colors;
} UiRenderElementState;

typedef struct {
    UiDocumentColor focused_foreground;
    UiDocumentColor focused_background;
    UiDocumentColor pressed_foreground;
    UiDocumentColor pressed_background;
    UiDocumentColor disabled_foreground;
    UiDocumentColor disabled_background;
} UiRenderTheme;

typedef enum {
    UI_RENDER_OK = 0,
    UI_RENDER_INVALID_ARGUMENT,
    UI_RENDER_INVALID_DOCUMENT,
    UI_RENDER_INVALID_STATE,
    UI_RENDER_LAYOUT_ERROR,
    UI_RENDER_MISSING_SPRITE,
    UI_RENDER_MISSING_MATERIAL
} UiRenderResult;

UiRenderResult ui_render_document(
    const UiDocument *document,
    const AssetRegistry *assets,
    const UiRenderElementState *states,
    size_t state_count,
    const UiRenderTheme *theme,
    UiCanvas *canvas
);

#endif