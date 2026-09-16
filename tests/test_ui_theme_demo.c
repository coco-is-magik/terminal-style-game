#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>

#include <cmocka.h>

#include <stdlib.h>
#include <string.h>

#include "../src/ui_theme.h"
#include "../src/ui_theme_demo.h"
#include "../src/ui_theme_demo_runtime.h"

static Cell cell(const UiCanvas *canvas, int x, int y) {
    return canvas->cells[y * canvas->width + x];
}

static void assert_color(SDL_Color actual, UiThemeColor expected) {
    assert_int_equal(actual.r, expected.red);
    assert_int_equal(actual.g, expected.green);
    assert_int_equal(actual.b, expected.blue);
    assert_int_equal(actual.a, expected.alpha);
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

static bool contains_color(const UiCanvas *canvas, UiThemeColor expected) {
    size_t count = (size_t)canvas->width * (size_t)canvas->height;
    size_t i;
    for (i = 0U; i < count; i++) {
        Cell value = canvas->cells[i];
        if ((value.fg.r == expected.red && value.fg.g == expected.green &&
             value.fg.b == expected.blue && value.fg.a == expected.alpha) ||
            (value.bg.r == expected.red && value.bg.g == expected.green &&
             value.bg.b == expected.blue && value.bg.a == expected.alpha)) return true;
    }
    return false;
}

static void test_state_controls_are_session_only_and_wrap(void **state) {
    UiThemeDemoState demo;
    InputState input = {0};
    bool should_exit = true;
    (void)state;
    ui_theme_demo_state_init(&demo);
    assert_int_equal(demo.scale_percent, 100);
    input.arrow_right = true;
    assert_true(ui_theme_demo_apply_input(&demo, &input, &should_exit));
    assert_int_equal(demo.scale_percent, 125);
    assert_false(should_exit);
    assert_true(ui_theme_demo_apply_input(&demo, &input, &should_exit));
    assert_int_equal(demo.scale_percent, 150);
    assert_true(ui_theme_demo_apply_input(&demo, &input, &should_exit));
    assert_int_equal(demo.scale_percent, 200);
    assert_true(ui_theme_demo_apply_input(&demo, &input, &should_exit));
    assert_int_equal(demo.scale_percent, 100);
    memset(&input, 0, sizeof(input));
    input.arrow_left = true;
    assert_true(ui_theme_demo_apply_input(&demo, &input, &should_exit));
    assert_int_equal(demo.scale_percent, 200);
    memset(&input, 0, sizeof(input));
    input.ui_scale_decrease_pressed = true;
    assert_true(ui_theme_demo_apply_input(&demo, &input, &should_exit));
    assert_int_equal(demo.scale_percent, 150);
    memset(&input, 0, sizeof(input));
    input.ui_scale_increase_pressed = true;
    assert_true(ui_theme_demo_apply_input(&demo, &input, &should_exit));
    assert_int_equal(demo.scale_percent, 200);
    memset(&input, 0, sizeof(input));
    input.ui_scale_reset_pressed = true;
    assert_true(ui_theme_demo_apply_input(&demo, &input, &should_exit));
    assert_int_equal(demo.scale_percent, 100);
    memset(&input, 0, sizeof(input));
    input.esc = true;
    assert_true(ui_theme_demo_apply_input(&demo, &input, &should_exit));
    assert_true(should_exit);
}

static void test_invalid_input_is_transactional(void **state) {
    UiThemeDemoState demo = {175};
    UiThemeDemoState before = demo;
    InputState input = {0};
    bool should_exit = true;
    (void)state;
    assert_false(ui_theme_demo_apply_input(&demo, &input, &should_exit));
    assert_memory_equal(&demo, &before, sizeof(demo));
    assert_true(should_exit);
    assert_false(ui_theme_demo_apply_input(NULL, &input, &should_exit));
    assert_false(ui_theme_demo_apply_input(&demo, NULL, &should_exit));
    assert_false(ui_theme_demo_apply_input(&demo, &input, NULL));
    ui_theme_demo_state_init(NULL);
}

static void test_specimen_contains_all_roles_and_state_cues(void **state) {
    const UiThemePalette *p = &ui_theme_provisional_tokens()->palette;
    const UiThemeColor roles[] = {
        p->canvas, p->panel, p->elevated, p->text_primary, p->text_secondary,
        p->border, p->accent, p->focus, p->selection_background,
        p->disabled_text, p->disabled_background, p->warning, p->error,
        p->success, p->destructive
    };
    UiThemeDemoState demo;
    UiCanvas *canvas = ui_canvas_create(UI_THEME_DEMO_WIDTH, UI_THEME_DEMO_HEIGHT);
    size_t i;
    (void)state;
    assert_non_null(canvas);
    ui_theme_demo_state_init(&demo);
    assert_int_equal(ui_theme_demo_render(&demo, canvas), UI_THEME_DEMO_RENDER_OK);
    assert_true(contains_text(canvas, "ACCEPTED UI THEME SPECIMEN"));
    assert_true(contains_text(canvas, "DIAGNOSTIC ONLY - NOT PRODUCT UI"));
    assert_true(contains_text(canvas, "[ > OPEN PROJECT < ]"));
    assert_true(contains_text(canvas, "[ * OPEN PROJECT * ]"));
    assert_true(contains_text(canvas, "[ # OPEN PROJECT # ]"));
    assert_true(contains_text(canvas, "[ ! OPEN PROJECT ! ]"));
    assert_true(contains_text(canvas, "[+] SUCCESS  Project saved"));
    assert_true(contains_text(canvas, "[!] WARNING  Unsaved changes"));
    assert_true(contains_text(canvas, "[X] ERROR    Save failed"));
    assert_true(contains_text(canvas, "Palette accepted; diagnostic only"));
    for (i = 0U; i < sizeof(roles) / sizeof(roles[0]); i++)
        assert_true(contains_color(canvas, roles[i]));
    ui_canvas_destroy(canvas);
}

static void test_representative_pairs_match_tokens(void **state) {
    const UiThemePalette *p = &ui_theme_provisional_tokens()->palette;
    UiThemeDemoState demo;
    UiCanvas *canvas = ui_canvas_create(UI_THEME_DEMO_WIDTH, UI_THEME_DEMO_HEIGHT);
    (void)state;
    assert_non_null(canvas);
    ui_theme_demo_state_init(&demo);
    assert_int_equal(ui_theme_demo_render(&demo, canvas), UI_THEME_DEMO_RENDER_OK);
    assert_color(cell(canvas, 4, 9).fg, p->text_primary);
    assert_color(cell(canvas, 4, 9).bg, p->canvas);
    assert_color(cell(canvas, 36, 10).fg, p->text_secondary);
    assert_color(cell(canvas, 36, 10).bg, p->panel);
    assert_color(cell(canvas, 68, 11).fg, p->focus);
    assert_color(cell(canvas, 68, 11).bg, p->elevated);
    assert_color(cell(canvas, 21, 36).bg, p->selection_background);
    assert_color(cell(canvas, 21, 42).fg, p->disabled_text);
    assert_color(cell(canvas, 21, 42).bg, p->disabled_background);
    assert_color(cell(canvas, 58, 34).fg, p->success);
    assert_color(cell(canvas, 58, 36).fg, p->warning);
    assert_color(cell(canvas, 58, 38).fg, p->error);
    assert_color(cell(canvas, 58, 40).fg, p->destructive);
    ui_canvas_destroy(canvas);
}

static void test_render_is_repeatable_and_failure_preserves_canvas(void **state) {
    UiThemeDemoState demo;
    UiCanvas *canvas = ui_canvas_create(UI_THEME_DEMO_WIDTH, UI_THEME_DEMO_HEIGHT);
    UiCanvas *before = ui_canvas_create(UI_THEME_DEMO_WIDTH, UI_THEME_DEMO_HEIGHT);
    size_t count = (size_t)UI_THEME_DEMO_WIDTH * (size_t)UI_THEME_DEMO_HEIGHT;
    (void)state;
    assert_non_null(canvas);
    assert_non_null(before);
    ui_theme_demo_state_init(&demo);
    assert_int_equal(ui_theme_demo_render(&demo, canvas), UI_THEME_DEMO_RENDER_OK);
    memcpy(before->cells, canvas->cells, count * sizeof(*canvas->cells));
    memcpy(before->touched, canvas->touched, count * sizeof(*canvas->touched));
    assert_int_equal(ui_theme_demo_render(&demo, canvas), UI_THEME_DEMO_RENDER_OK);
    assert_memory_equal(canvas->cells, before->cells, count * sizeof(*canvas->cells));
    assert_memory_equal(canvas->touched, before->touched, count * sizeof(*canvas->touched));
    demo.scale_percent = 175;
    assert_int_equal(ui_theme_demo_render(&demo, canvas),
                     UI_THEME_DEMO_RENDER_INVALID_ARGUMENT);
    assert_memory_equal(canvas->cells, before->cells, count * sizeof(*canvas->cells));
    assert_memory_equal(canvas->touched, before->touched, count * sizeof(*canvas->touched));
    assert_int_equal(ui_theme_demo_render(NULL, canvas),
                     UI_THEME_DEMO_RENDER_INVALID_ARGUMENT);
    assert_int_equal(ui_theme_demo_render(&demo, NULL),
                     UI_THEME_DEMO_RENDER_INVALID_ARGUMENT);
    ui_canvas_destroy(before);
    ui_canvas_destroy(canvas);
}

static void test_specimen_fits_all_supported_scales(void **state) {
    static const int scales[] = {100, 125, 150, 200};
    size_t i;
    (void)state;
    for (i = 0U; i < sizeof(scales) / sizeof(scales[0]); i++) {
        int width;
        int height;
        assert_true(ui_theme_scaled_edge(UI_THEME_DEMO_WIDTH * 8, scales[i], &width));
        assert_true(ui_theme_scaled_edge(UI_THEME_DEMO_HEIGHT * 8, scales[i], &height));
        assert_true(width <= 2080);
        assert_true(height <= 1280);
        assert_true((2080 - width) / 2 >= 0);
        assert_true((1280 - height) / 2 >= 0);
    }
}

static void test_runtime_argument_validation(void **state) {
    Renderer renderer = {0};
    Grid grid = {0};
    Cell value = {0};
    (void)state;
    assert_false(ui_theme_demo_runtime_arguments_valid(NULL, &grid, 120));
    assert_false(ui_theme_demo_runtime_arguments_valid(&renderer, NULL, 120));
    grid.cells = &value;
    assert_false(ui_theme_demo_runtime_arguments_valid(&renderer, &grid, 120));
    renderer.window = (SDL_Window *)(uintptr_t)1U;
    renderer.sdl_ren = (SDL_Renderer *)(uintptr_t)1U;
    renderer.screen_texture = (SDL_Texture *)(uintptr_t)1U;
    renderer.pixel_buffer = (uint32_t *)(uintptr_t)1U;
    renderer.logical_w = 2080;
    renderer.logical_h = 1280;
    assert_true(ui_theme_demo_runtime_arguments_valid(&renderer, &grid, 120));
    assert_false(ui_theme_demo_runtime_arguments_valid(&renderer, &grid, 0));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_state_controls_are_session_only_and_wrap),
        cmocka_unit_test(test_invalid_input_is_transactional),
        cmocka_unit_test(test_specimen_contains_all_roles_and_state_cues),
        cmocka_unit_test(test_representative_pairs_match_tokens),
        cmocka_unit_test(test_render_is_repeatable_and_failure_preserves_canvas),
        cmocka_unit_test(test_specimen_fits_all_supported_scales),
        cmocka_unit_test(test_runtime_argument_validation)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}