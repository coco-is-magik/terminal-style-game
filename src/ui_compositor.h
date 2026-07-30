#ifndef UI_COMPOSITOR_H
#define UI_COMPOSITOR_H

#include "ui_canvas.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UI_COMPOSITOR_MAX_LAYERS 16

typedef enum {
    UI_ANCHOR_TOP_LEFT = 0,
    UI_ANCHOR_CENTER,
    UI_ANCHOR_BOTTOM_LEFT
} UiAnchor;

typedef enum {
    UI_SCALE_INHERIT_GLOBAL = 0,
    UI_SCALE_FIXED_100,
    UI_SCALE_EXPLICIT_PRESET
} UiScalePolicy;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} UiClipRect;

typedef struct {
    int role_id;
    const UiCanvas *canvas;
    UiAnchor anchor;
    UiClipRect clip;
    UiScalePolicy scale_policy;
    int explicit_scale_percent;
    int z_order;
    bool visible;
    size_t insertion_order;
} UiLayer;

typedef struct {
    UiLayer layers[UI_COMPOSITOR_MAX_LAYERS];
    size_t count;
} UiLayerList;

void ui_layer_list_clear(UiLayerList *list);
bool ui_layer_list_add(UiLayerList *list, const UiLayer *layer);
int ui_compositor_scaled_edge(int source_edge, int scale_percent);
bool ui_compositor_compose(const UiLayerList *list, int global_scale_percent,
                           uint32_t *pixels, int pixel_width, int pixel_height,
                           uint8_t *touched_cells, int cell_columns, int cell_rows);

#endif