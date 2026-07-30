#include "ui_compositor.h"
#include "ui_preferences.h"

#include <SDL3/SDL.h>

extern const unsigned char font8x8_basic[128][8];

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define UI_COLOR_TO_UINT32(c) (((uint32_t)(c).r << 24) | \
                               ((uint32_t)(c).g << 16) | \
                               ((uint32_t)(c).b << 8) | (uint32_t)(c).a)
#else
#define UI_COLOR_TO_UINT32(c) (((uint32_t)(c).a << 24) | \
                               ((uint32_t)(c).b << 16) | \
                               ((uint32_t)(c).g << 8) | (uint32_t)(c).r)
#endif

void ui_layer_list_clear(UiLayerList *list) {
    if (list) list->count = 0;
}

static bool valid_clip(UiClipRect clip) {
    return clip.width >= 0 && clip.height >= 0;
}

bool ui_layer_list_add(UiLayerList *list, const UiLayer *layer) {
    UiLayer copy;
    if (!list || !layer || !layer->canvas || list->count >= UI_COMPOSITOR_MAX_LAYERS ||
        layer->role_id < 0 || !valid_clip(layer->clip) ||
        layer->anchor < UI_ANCHOR_TOP_LEFT || layer->anchor > UI_ANCHOR_BOTTOM_LEFT ||
        layer->scale_policy < UI_SCALE_INHERIT_GLOBAL ||
        layer->scale_policy > UI_SCALE_EXPLICIT_PRESET ||
        (layer->scale_policy == UI_SCALE_EXPLICIT_PRESET &&
         !ui_preferences_is_valid_scale(layer->explicit_scale_percent))) {
        return false;
    }
    copy = *layer;
    copy.insertion_order = list->count;
    list->layers[list->count++] = copy;
    return true;
}

int ui_compositor_scaled_edge(int source_edge, int scale_percent) {
    if (source_edge <= 0) return 0;
    return (source_edge * scale_percent) / 100;
}

static int layer_scale(const UiLayer *layer, int global_scale_percent) {
    if (layer->scale_policy == UI_SCALE_FIXED_100) return 100;
    if (layer->scale_policy == UI_SCALE_EXPLICIT_PRESET) return layer->explicit_scale_percent;
    return global_scale_percent;
}

static void layer_origin(const UiLayer *layer, int scale, int width, int height,
                         int *origin_x, int *origin_y) {
    int scaled_width = ui_compositor_scaled_edge(layer->canvas->width * 8, scale);
    int scaled_height = ui_compositor_scaled_edge(layer->canvas->height * 8, scale);
    *origin_x = 0;
    *origin_y = 0;
    if (layer->anchor == UI_ANCHOR_CENTER) {
        *origin_x = (width - scaled_width) / 2;
        *origin_y = (height - scaled_height) / 2;
    } else if (layer->anchor == UI_ANCHOR_BOTTOM_LEFT) {
        *origin_y = height - scaled_height;
    }
}

static bool in_clip(const UiLayer *layer, int x, int y, int width, int height) {
    int clip_right = layer->clip.x + layer->clip.width;
    int clip_bottom = layer->clip.y + layer->clip.height;
    return x >= 0 && y >= 0 && x < width && y < height &&
           x >= layer->clip.x && y >= layer->clip.y &&
           x < clip_right && y < clip_bottom;
}

static void compose_layer(const UiLayer *layer, int global_scale_percent,
                          uint32_t *pixels, int width, int height,
                          uint8_t *touched_cells, int cell_columns, int cell_rows) {
    int scale = layer_scale(layer, global_scale_percent);
    int origin_x;
    int origin_y;
    int cy;
    int cx;
    if (!layer->visible || layer->clip.width == 0 || layer->clip.height == 0) return;
    layer_origin(layer, scale, width, height, &origin_x, &origin_y);
    for (cy = 0; cy < layer->canvas->height; cy++) {
        for (cx = 0; cx < layer->canvas->width; cx++) {
            const Cell *cell;
            const unsigned char *glyph;
            uint32_t fg;
            uint32_t bg;
            int sy;
            int sx;
            if (!ui_canvas_is_touched(layer->canvas, cx, cy)) continue;
            cell = &layer->canvas->cells[cy * layer->canvas->width + cx];
            glyph = font8x8_basic[cell->glyph < 128 ? cell->glyph : 32];
            fg = UI_COLOR_TO_UINT32(cell->fg);
            bg = UI_COLOR_TO_UINT32(cell->bg);
            for (sy = 0; sy < 8; sy++) {
                int y0 = origin_y + ui_compositor_scaled_edge(cy * 8 + sy, scale);
                int y1 = origin_y + ui_compositor_scaled_edge(cy * 8 + sy + 1, scale);
                for (sx = 0; sx < 8; sx++) {
                    int x0 = origin_x + ui_compositor_scaled_edge(cx * 8 + sx, scale);
                    int x1 = origin_x + ui_compositor_scaled_edge(cx * 8 + sx + 1, scale);
                    uint32_t color = (glyph[sy] & (1u << sx)) ? fg : bg;
                    int y;
                    int x;
                    for (y = y0; y < y1; y++) {
                        for (x = x0; x < x1; x++) {
                            if (in_clip(layer, x, y, width, height)) {
                                pixels[y * width + x] = color;
                                if (touched_cells && x / 8 < cell_columns && y / 8 < cell_rows) {
                                    touched_cells[(y / 8) * cell_columns + (x / 8)] = 1;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

bool ui_compositor_compose(const UiLayerList *list, int global_scale_percent,
                           uint32_t *pixels, int pixel_width, int pixel_height,
                           uint8_t *touched_cells, int cell_columns, int cell_rows) {
    size_t pass;
    if (!list || !pixels || pixel_width <= 0 || pixel_height <= 0 ||
        !ui_preferences_is_valid_scale(global_scale_percent)) return false;
    for (pass = 0; pass < list->count; pass++) {
        size_t i;
        const UiLayer *selected = NULL;
        size_t lower_count;
        for (i = 0; i < list->count; i++) {
            size_t j;
            lower_count = 0;
            for (j = 0; j < list->count; j++) {
                if (list->layers[j].z_order < list->layers[i].z_order ||
                    (list->layers[j].z_order == list->layers[i].z_order &&
                     list->layers[j].insertion_order < list->layers[i].insertion_order)) {
                    lower_count++;
                }
            }
            if (lower_count == pass) {
                selected = &list->layers[i];
                break;
            }
        }
        if (selected) compose_layer(selected, global_scale_percent, pixels,
                                    pixel_width, pixel_height, touched_cells,
                                    cell_columns, cell_rows);
    }
    return true;
}