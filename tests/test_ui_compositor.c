#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdlib.h>
#include <string.h>

#include "../src/ui_canvas.h"
#include "../src/ui_compositor.h"
#include "../src/glyph_missing.h"
#include "../src/grid.h"

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define TEST_COLOR_TO_UINT32(c) (((uint32_t)(c).r << 24) | \
                                 ((uint32_t)(c).g << 16) | \
                                 ((uint32_t)(c).b << 8) | (uint32_t)(c).a)
#else
#define TEST_COLOR_TO_UINT32(c) (((uint32_t)(c).a << 24) | \
                                 ((uint32_t)(c).b << 16) | \
                                 ((uint32_t)(c).g << 8) | (uint32_t)(c).r)
#endif

extern const unsigned char font8x8_basic[128][8];

static UiLayer layer_for(const UiCanvas *canvas, int role, int z, int width, int height) {
    UiLayer layer = {role, canvas, UI_ANCHOR_TOP_LEFT, {0, 0, width, height},
                     UI_SCALE_INHERIT_GLOBAL, 0, z, true, 0};
    return layer;
}

static int reference_layer_scale(const UiLayer *layer, int global_scale_percent) {
    if (layer->scale_policy == UI_SCALE_FIXED_100) return 100;
    if (layer->scale_policy == UI_SCALE_EXPLICIT_PRESET) return layer->explicit_scale_percent;
    return global_scale_percent;
}

static void reference_layer_origin(const UiLayer *layer, int scale, int width, int height,
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

static bool reference_in_clip(const UiLayer *layer, int x, int y, int width, int height) {
    return x >= 0 && y >= 0 && x < width && y < height &&
           x >= layer->clip.x && y >= layer->clip.y &&
           x < layer->clip.x + layer->clip.width &&
           y < layer->clip.y + layer->clip.height;
}

static void reference_compose_layer(const UiLayer *layer, int global_scale_percent,
                                    uint32_t *pixels, int width, int height,
                                    uint8_t *touched, int columns, int rows) {
    int scale = reference_layer_scale(layer, global_scale_percent);
    int origin_x;
    int origin_y;
    int cy;
    int cx;
    if (!layer->visible || layer->clip.width == 0 || layer->clip.height == 0) return;
    reference_layer_origin(layer, scale, width, height, &origin_x, &origin_y);
    for (cy = 0; cy < layer->canvas->height; cy++) {
        for (cx = 0; cx < layer->canvas->width; cx++) {
            const Cell *cell;
            const unsigned char *glyph;
            int sy;
            int sx;
            if (!ui_canvas_is_touched(layer->canvas, cx, cy)) continue;
            cell = &layer->canvas->cells[cy * layer->canvas->width + cx];
            glyph = font8x8_basic[cell->glyph < 128 ? cell->glyph : 32];
            for (sy = 0; sy < 8; sy++) {
                int y0 = origin_y + ui_compositor_scaled_edge(cy * 8 + sy, scale);
                int y1 = origin_y + ui_compositor_scaled_edge(cy * 8 + sy + 1, scale);
                for (sx = 0; sx < 8; sx++) {
                    int x0 = origin_x + ui_compositor_scaled_edge(cx * 8 + sx, scale);
                    int x1 = origin_x + ui_compositor_scaled_edge(cx * 8 + sx + 1, scale);
                    uint32_t color = (glyph[sy] & (1u << sx)) ?
                                     TEST_COLOR_TO_UINT32(cell->fg) : TEST_COLOR_TO_UINT32(cell->bg);
                    int y;
                    int x;
                    for (y = y0; y < y1; y++) {
                        for (x = x0; x < x1; x++) {
                            if (!reference_in_clip(layer, x, y, width, height)) continue;
                            pixels[y * width + x] = color;
                            if (touched && x / 8 < columns && y / 8 < rows) {
                                touched[(y / 8) * columns + (x / 8)] = 1;
                            }
                        }
                    }
                }
            }
        }
    }
}

static void test_optimized_compositor_matches_reference(void **state) {
    static const int scales[] = {100, 125, 150, 200};
    UiCanvas *back = ui_canvas_create(3, 2);
    UiCanvas *front = ui_canvas_create(2, 2);
    SDL_Color red = {255, 20, 30, 255};
    SDL_Color blue = {10, 40, 255, 255};
    SDL_Color black = {0, 0, 0, 255};
    size_t scale_index;
    (void)state;
    assert_non_null(back);
    assert_non_null(front);
    assert_true(ui_canvas_set(back, 0, 0, 'A', red, black));
    assert_true(ui_canvas_set(back, 2, 1, ' ', blue, red));
    assert_true(ui_canvas_set(front, 0, 0, 'Z', blue, black));
    assert_true(ui_canvas_set(front, 1, 1, 'Q', red, blue));
    for (scale_index = 0; scale_index < sizeof(scales) / sizeof(scales[0]); scale_index++) {
        UiLayerList list = {0};
        UiLayer first = layer_for(back, 1, 4, 29, 23);
        UiLayer second = layer_for(front, 2, 4, 31, 25);
        uint32_t actual[31 * 25];
        uint32_t expected[31 * 25];
        uint8_t actual_touched[4 * 4];
        uint8_t expected_touched[4 * 4];
        first.anchor = UI_ANCHOR_CENTER;
        first.clip = (UiClipRect){2, 1, 25, 20};
        second.anchor = UI_ANCHOR_BOTTOM_LEFT;
        second.clip = (UiClipRect){-2, 3, 31, 19};
        second.scale_policy = UI_SCALE_FIXED_100;
        memset(actual, 0x5a, sizeof(actual));
        memcpy(expected, actual, sizeof(actual));
        memset(actual_touched, 0, sizeof(actual_touched));
        memset(expected_touched, 0, sizeof(expected_touched));
        assert_true(ui_layer_list_add(&list, &first));
        assert_true(ui_layer_list_add(&list, &second));
        reference_compose_layer(&first, scales[scale_index], expected, 31, 25,
                                expected_touched, 4, 4);
        reference_compose_layer(&second, scales[scale_index], expected, 31, 25,
                                expected_touched, 4, 4);
        assert_true(ui_compositor_compose(&list, NULL, NULL, scales[scale_index], actual, 31, 25,
                                          actual_touched, 4, 4));
        assert_memory_equal(actual, expected, sizeof(actual));
        assert_memory_equal(actual_touched, expected_touched, sizeof(actual_touched));
    }
    ui_canvas_destroy(front);
    ui_canvas_destroy(back);
}

static void test_canvas_transparency_and_intentional_space(void **state) {
    UiCanvas *canvas = ui_canvas_create(2, 1);
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color red = {255, 0, 0, 255};
    (void)state;
    assert_non_null(canvas);
    assert_false(ui_canvas_is_touched(canvas, 0, 0));
    assert_true(ui_canvas_set(canvas, 0, 0, ' ', white, red));
    assert_true(ui_canvas_is_touched(canvas, 0, 0));
    assert_false(ui_canvas_is_touched(canvas, 1, 0));
    ui_canvas_clear(canvas);
    assert_false(ui_canvas_is_touched(canvas, 0, 0));
    ui_canvas_destroy(canvas);
}

static void test_region_copy_does_not_resurrect_stale_glyphs(void **state) {
    Grid *staging = grid_create(4, 2);
    UiCanvas *canvas = ui_canvas_create(2, 1);
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color black = {0, 0, 0, 255};
    (void)state;
    assert_non_null(staging);
    assert_non_null(canvas);
    assert_true(grid_set(staging, 1, 0, 'X', white, black));
    ui_canvas_copy_grid_region(canvas, staging, 0, 0);
    assert_true(ui_canvas_is_touched(canvas, 1, 0));
    assert_true(grid_clear_region_zero(staging, 0, 0, 2, 1));
    assert_true(grid_set(staging, 0, 0, 'Y', white, black));
    ui_canvas_copy_grid_region(canvas, staging, 0, 0);
    assert_true(ui_canvas_is_touched(canvas, 0, 0));
    assert_false(ui_canvas_is_touched(canvas, 1, 0));
    assert_int_equal(canvas->cells[0].glyph, 'Y');
    ui_canvas_destroy(canvas);
    grid_destroy(staging);
}

static void test_fractional_edges_are_adjacent(void **state) {
    int i;
    (void)state;
    for (i = 0; i < 32; i++) {
        assert_int_equal(ui_compositor_scaled_edge(i + 1, 125),
                         ui_compositor_scaled_edge(i + 1, 125));
        assert_true(ui_compositor_scaled_edge(i + 1, 125) >=
                    ui_compositor_scaled_edge(i, 125) + 1);
    }
    assert_int_equal(ui_compositor_scaled_edge(8, 125), 10);
    assert_int_equal(ui_compositor_scaled_edge(8, 150), 12);
}

static void test_layers_clip_policy_and_stable_order(void **state) {
    UiCanvas *first = ui_canvas_create(1, 1);
    UiCanvas *second = ui_canvas_create(1, 1);
    UiLayerList list = {0};
    uint32_t pixels[400] = {0};
    SDL_Color black = {0, 0, 0, 255};
    SDL_Color red = {255, 0, 0, 255};
    SDL_Color green = {0, 255, 0, 255};
    UiLayer a;
    UiLayer b;
    (void)state;
    assert_non_null(first);
    assert_non_null(second);
    assert_true(ui_canvas_set(first, 0, 0, ' ', red, red));
    assert_true(ui_canvas_set(second, 0, 0, ' ', green, green));
    a = layer_for(first, 1, 2, 20, 20);
    b = layer_for(second, 2, 2, 20, 20);
    b.scale_policy = UI_SCALE_FIXED_100;
    assert_true(ui_layer_list_add(&list, &a));
    assert_true(ui_layer_list_add(&list, &b));
    assert_true(ui_compositor_compose(&list, NULL, NULL, 200, pixels, 20, 20, NULL, 0, 0));
    assert_true(pixels[0] != 0);
    assert_true(pixels[0] != ((uint32_t)black.a << 24));
    assert_int_equal(pixels[7 * 20 + 7], pixels[0]);
    assert_true(pixels[12 * 20 + 12] != pixels[0]);
    ui_canvas_destroy(first);
    ui_canvas_destroy(second);
}

static void test_capacity_hidden_and_invalid(void **state) {
    UiCanvas *canvas = ui_canvas_create(1, 1);
    UiLayerList list = {0};
    UiLayer layer = layer_for(canvas, 1, 0, 8, 8);
    int i;
    (void)state;
    assert_non_null(canvas);
    for (i = 0; i < UI_COMPOSITOR_MAX_LAYERS; i++) {
        layer.role_id = i;
        assert_true(ui_layer_list_add(&list, &layer));
    }
    assert_false(ui_layer_list_add(&list, &layer));
    ui_layer_list_clear(&list);
    layer.scale_policy = UI_SCALE_EXPLICIT_PRESET;
    layer.explicit_scale_percent = 175;
    assert_false(ui_layer_list_add(&list, &layer));
    ui_canvas_destroy(canvas);
}

static void test_centered_menu_crop_visible_at_all_presets(void **state) {
    const int presets[] = {100, 125, 150, 200};
    Grid *staging = grid_create(260, 160);
    UiCanvas *canvas = ui_canvas_create(80, 40);
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color black = {0, 0, 0, 255};
    size_t pixel_count = (size_t)2080 * 1280;
    uint32_t *pixels = calloc(pixel_count, sizeof(*pixels));
    size_t p;
    (void)state;

    assert_non_null(staging);
    assert_non_null(canvas);
    assert_non_null(pixels);
    grid_clear(staging, black);
    assert_true(grid_set(staging, 129, 79, 'X', white, black));
    ui_canvas_copy_grid_region(canvas, staging, 90, 60);
    assert_true(ui_canvas_is_touched(canvas, 39, 19));

    for (p = 0; p < sizeof(presets) / sizeof(presets[0]); p++) {
        UiLayerList list = {0};
        UiLayer layer = layer_for(canvas, 1, 0, 2080, 1280);
        int scaled_width = ui_compositor_scaled_edge(80 * 8, presets[p]);
        int scaled_height = ui_compositor_scaled_edge(40 * 8, presets[p]);
        int origin_x = (2080 - scaled_width) / 2;
        int origin_y = (1280 - scaled_height) / 2;
        int sample_x = origin_x + ui_compositor_scaled_edge(39 * 8, presets[p]);
        int sample_y = origin_y + ui_compositor_scaled_edge(19 * 8, presets[p]);

        memset(pixels, 0, pixel_count * sizeof(*pixels));
        layer.anchor = UI_ANCHOR_CENTER;
        assert_true(origin_x >= 0);
        assert_true(origin_y >= 0);
        assert_true(origin_x + scaled_width <= 2080);
        assert_true(origin_y + scaled_height <= 1280);
        assert_true(ui_layer_list_add(&list, &layer));
        assert_true(ui_compositor_compose(&list, NULL, NULL, presets[p], pixels,
                                          2080, 1280, NULL, 0, 0));
        assert_true(pixels[sample_y * 2080 + sample_x] != 0);
    }

    free(pixels);
    ui_canvas_destroy(canvas);
    grid_destroy(staging);
}

/* The accessibility presets, pinned to values computed outside this file, because the preset
   composition test derives its probe coordinates from ui_compositor_scaled_edge itself and so
   cannot prove the scaling rule — only that it agrees with itself. These literals are the
   independent reference: cell edges in pixels, and the truncating rule the function documents.
   Covers V1-5 outcome 6: UI accessibility scale is independent of the cell's byte size. */
static void test_scaled_edges_have_independent_expectations(void **state) {
    /* One 8x8 cell, and a full 80x40 canvas, at every accepted preset (100 / 125 / 150 / 200). */
    static const struct {
        int source_edge;
        int scale_percent;
        int expected;
    } cases[] = {
        {8, 100, 8},      {8, 125, 10},     {8, 150, 12},     {8, 200, 16},
        {640, 100, 640},  {640, 125, 800},  {640, 150, 960},  {640, 200, 1280},
        {320, 100, 320},  {320, 125, 400},  {320, 150, 480},  {320, 200, 640},
        /* Truncation, not rounding: 7*125/100 = 8.75 -> 8, 41*150/100 = 61.5 -> 61. */
        {7, 125, 8},      {41, 150, 61},    {41, 200, 82},    {1, 150, 1},
        /* Nothing to scale stays nothing; a nonsensical edge is not a negative rectangle. */
        {0, 150, 0},      {-5, 100, 0},
    };
    size_t i;
    (void)state;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        assert_int_equal(ui_compositor_scaled_edge(cases[i].source_edge, cases[i].scale_percent),
                         cases[i].expected);
    }
}


/* A codepoint the chrome's face cannot draw is recorded against the chrome, with the cell it was seen
   at — the UI's own gaps, not the world's (V1-6 plan, I2). */
static void test_missing_codepoint_is_noted_for_the_chrome(void **state) {
    UiCanvas *canvas = ui_canvas_create(4, 2);
    GlyphMissingState missing;
    UiLayerList list = {0};
    UiLayer layer;
    uint32_t pixels[32 * 16];
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color black = {0, 0, 0, 255};
    (void)state;

    assert_non_null(canvas);
    /* U+3042 with no set loaded: the built-in face covers ASCII only, so this is a genuine gap. */
    assert_true(ui_canvas_set(canvas, 1, 0, 0x3042U, white, black));
    layer = layer_for(canvas, 1, 0, 32, 16);
    assert_true(ui_layer_list_add(&list, &layer));

    glyph_missing_reset(&missing);
    glyph_missing_begin_frame(&missing);
    assert_true(ui_compositor_compose(&list, NULL, &missing, 100, pixels, 32, 16, NULL, 4, 2));
    assert_int_equal(glyph_missing_report_count(&missing), 1U);
    assert_int_equal(glyph_missing_affected_this_frame(&missing), 1U);

    /* A second frame over the same content counts the cell again but must not report it again. */
    glyph_missing_begin_frame(&missing);
    assert_true(ui_compositor_compose(&list, NULL, &missing, 100, pixels, 32, 16, NULL, 4, 2));
    assert_int_equal(glyph_missing_report_count(&missing), 0U);
    assert_int_equal(glyph_missing_affected_this_frame(&missing), 1U);

    ui_canvas_destroy(canvas);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_canvas_transparency_and_intentional_space),
        cmocka_unit_test(test_missing_codepoint_is_noted_for_the_chrome),
        cmocka_unit_test(test_region_copy_does_not_resurrect_stale_glyphs),
        cmocka_unit_test(test_fractional_edges_are_adjacent),
        cmocka_unit_test(test_layers_clip_policy_and_stable_order),
        cmocka_unit_test(test_capacity_hidden_and_invalid),
        cmocka_unit_test(test_centered_menu_crop_visible_at_all_presets),
        cmocka_unit_test(test_scaled_edges_have_independent_expectations),
        cmocka_unit_test(test_optimized_compositor_matches_reference),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
