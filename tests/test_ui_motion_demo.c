#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>

#include <cmocka.h>

#include <stdlib.h>
#include <string.h>

#include "../src/ui_motion_demo.h"
#include "../src/ui_motion_demo_runtime.h"
#include "../src/ui_theme.h"

static Cell cell(const UiCanvas *canvas, int x, int y) {
    return canvas->cells[y * canvas->width + x];
}

static bool contains_text(const UiCanvas *canvas, const char *text) {
    size_t length = strlen(text);
    int y;
    int x;
    for (y = 0; y < canvas->height; y++) {
        for (x = 0; x + (int)length <= canvas->width; x++) {
            size_t i;
            for (i = 0U; i < length; i++)
                if (cell(canvas, x + (int)i, y).glyph != (uint8_t)text[i]) break;
            if (i == length) return true;
        }
    }
    return false;
}

static void test_controls_replay_pause_step_reduced_and_scale(void **state) {
    UiMotionDemoState demo;
    InputState input = {0};
    bool should_exit = true;
    double elapsed;
    (void)state;

    ui_motion_demo_state_init(&demo, 1000.0);
    assert_true(ui_motion_demo_elapsed_ms(&demo, 1100.0, &elapsed));
    assert_float_equal(elapsed, 100.0, 0.000001);
    input.place = true;
    assert_true(ui_motion_demo_apply_input(&demo, &input, 1100.0, &should_exit));
    assert_true(demo.paused);
    assert_false(should_exit);
    memset(&input, 0, sizeof(input));
    input.arrow_right = true;
    assert_true(ui_motion_demo_apply_input(&demo, &input, 1200.0, &should_exit));
    assert_float_equal(demo.paused_elapsed_ms, 120.0, 0.000001);
    memset(&input, 0, sizeof(input));
    input.arrow_left = true;
    assert_true(ui_motion_demo_apply_input(&demo, &input, 1300.0, &should_exit));
    assert_float_equal(demo.paused_elapsed_ms, 100.0, 0.000001);
    memset(&input, 0, sizeof(input));
    input.confirm = true;
    input.tab = true;
    input.up = true;
    assert_true(ui_motion_demo_apply_input(&demo, &input, 1400.0, &should_exit));
    assert_float_equal(demo.paused_elapsed_ms, 0.0, 0.000001);
    assert_true(demo.reduced_motion);
    assert_int_equal(demo.scale_percent, 125);
    memset(&input, 0, sizeof(input));
    input.esc = true;
    assert_true(ui_motion_demo_apply_input(&demo, &input, 1400.0, &should_exit));
    assert_true(should_exit);
}

static void test_invalid_time_is_transactional(void **state) {
    UiMotionDemoState demo;
    UiMotionDemoState before;
    InputState input = {0};
    bool should_exit = true;
    (void)state;
    ui_motion_demo_state_init(&demo, 1000.0);
    before = demo;
    assert_false(ui_motion_demo_apply_input(&demo, &input, 999.0, &should_exit));
    assert_memory_equal(&demo, &before, sizeof(demo));
    assert_true(should_exit);
}

static void test_specimen_has_required_comparisons_and_stable_control(void **state) {
    UiMotionDemoState demo;
    UiCanvas *first = ui_canvas_create(UI_MOTION_DEMO_WIDTH, UI_MOTION_DEMO_HEIGHT);
    UiCanvas *later = ui_canvas_create(UI_MOTION_DEMO_WIDTH, UI_MOTION_DEMO_HEIGHT);
    const UiThemePalette *p = &ui_theme_provisional_tokens()->palette;
    Cell fixed_first;
    Cell fixed_later;
    (void)state;
    assert_non_null(first);
    assert_non_null(later);
    ui_motion_demo_state_init(&demo, 0.0);
    assert_int_equal(ui_motion_demo_render(&demo, 0.0, first), UI_MOTION_DEMO_RENDER_OK);
    assert_int_equal(ui_motion_demo_render(&demo, 80.0, later), UI_MOTION_DEMO_RENDER_OK);
    assert_true(contains_text(first, "MAJOR ENTER 160 ms / EXIT 120 ms"));
    assert_true(contains_text(first, "FEEDBACK 80 ms / RELATIONSHIP 120 ms"));
    assert_true(contains_text(first, "PALETTE-NATIVE"));
    assert_true(contains_text(first, "ISOLATED RGB"));
    assert_true(contains_text(first, "no framebuffer RGB split"));
    assert_true(contains_text(first, "ACCEPTED D6 VOCABULARY"));
    fixed_first = cell(first, 8, 14);
    fixed_later = cell(later, 8, 14);
    assert_memory_equal(&fixed_first, &fixed_later, sizeof(fixed_first));
    assert_int_equal(fixed_first.fg.r, p->focus.red);
    assert_int_equal(cell(first, 24, 37).fg.r, 255);
    assert_int_equal(cell(first, 26, 38).fg.g, 224);
    ui_canvas_destroy(later);
    ui_canvas_destroy(first);
}

static void test_reduced_motion_is_non_spatial_and_repeatable(void **state) {
    UiMotionDemoState demo;
    UiCanvas *first = ui_canvas_create(UI_MOTION_DEMO_WIDTH, UI_MOTION_DEMO_HEIGHT);
    UiCanvas *later = ui_canvas_create(UI_MOTION_DEMO_WIDTH, UI_MOTION_DEMO_HEIGHT);
    size_t count = (size_t)UI_MOTION_DEMO_WIDTH * (size_t)UI_MOTION_DEMO_HEIGHT;
    (void)state;
    assert_non_null(first);
    assert_non_null(later);
    ui_motion_demo_state_init(&demo, 0.0);
    demo.reduced_motion = true;
    demo.paused = true;
    assert_int_equal(ui_motion_demo_render(&demo, 0.0, first), UI_MOTION_DEMO_RENDER_OK);
    assert_int_equal(ui_motion_demo_render(&demo, 500.0, later), UI_MOTION_DEMO_RENDER_OK);
    assert_memory_equal(first->cells, later->cells, count * sizeof(*first->cells));
    assert_memory_equal(first->touched, later->touched, count * sizeof(*first->touched));
    assert_true(contains_text(first, "REDUCED MOTION: immediate, non-spatial"));
    ui_canvas_destroy(later);
    ui_canvas_destroy(first);
}

static void test_all_scales_fit_and_runtime_validates(void **state) {
    static const int scales[] = {100, 125, 150, 200};
    Renderer renderer = {0};
    Grid grid = {0};
    Cell value = {0};
    size_t i;
    (void)state;
    for (i = 0U; i < sizeof(scales) / sizeof(scales[0]); i++) {
        int width;
        int height;
        assert_true(ui_theme_scaled_edge(UI_MOTION_DEMO_WIDTH * 8, scales[i], &width));
        assert_true(ui_theme_scaled_edge(UI_MOTION_DEMO_HEIGHT * 8, scales[i], &height));
        assert_true(width <= 2080);
        assert_true(height <= 1280);
    }
    assert_false(ui_motion_demo_runtime_arguments_valid(NULL, &grid, 120));
    grid.cells = &value;
    renderer.window = (SDL_Window *)(uintptr_t)1U;
    renderer.sdl_ren = (SDL_Renderer *)(uintptr_t)1U;
    renderer.screen_texture = (SDL_Texture *)(uintptr_t)1U;
    renderer.pixel_buffer = (uint32_t *)(uintptr_t)1U;
    renderer.logical_w = 2080;
    renderer.logical_h = 1280;
    assert_true(ui_motion_demo_runtime_arguments_valid(&renderer, &grid, 120));
    assert_false(ui_motion_demo_runtime_arguments_valid(&renderer, &grid, 0));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_controls_replay_pause_step_reduced_and_scale),
        cmocka_unit_test(test_invalid_time_is_transactional),
        cmocka_unit_test(test_specimen_has_required_comparisons_and_stable_control),
        cmocka_unit_test(test_reduced_motion_is_non_spatial_and_repeatable),
        cmocka_unit_test(test_all_scales_fit_and_runtime_validates)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}