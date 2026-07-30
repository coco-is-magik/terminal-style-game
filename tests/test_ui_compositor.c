#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdlib.h>
#include <string.h>

#include "../src/ui_canvas.h"
#include "../src/ui_compositor.h"
#include "../src/grid.h"

static UiLayer layer_for(const UiCanvas *canvas, int role, int z, int width, int height) {
    UiLayer layer = {role, canvas, UI_ANCHOR_TOP_LEFT, {0, 0, width, height},
                     UI_SCALE_INHERIT_GLOBAL, 0, z, true, 0};
    return layer;
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
    assert_true(ui_compositor_compose(&list, 200, pixels, 20, 20, NULL, 0, 0));
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
        assert_true(ui_compositor_compose(&list, presets[p], pixels,
                                          2080, 1280, NULL, 0, 0));
        assert_true(pixels[sample_y * 2080 + sample_x] != 0);
    }

    free(pixels);
    ui_canvas_destroy(canvas);
    grid_destroy(staging);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_canvas_transparency_and_intentional_space),
        cmocka_unit_test(test_fractional_edges_are_adjacent),
        cmocka_unit_test(test_layers_clip_policy_and_stable_order),
        cmocka_unit_test(test_capacity_hidden_and_invalid),
        cmocka_unit_test(test_centered_menu_crop_visible_at_all_presets),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}