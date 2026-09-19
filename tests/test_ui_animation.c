#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ui_animation.h"

static UiElement element(const char *name, UiElementType type, int x, int y,
                         int width, int height) {
    UiElement value = {0};
    (void)snprintf(value.name, sizeof(value.name), "%s", name);
    value.type = type;
    value.layout = (UiElementLayout){x, y, UI_COORD_ABSOLUTE, width, height};
    value.visible = 1;
    value.align = UI_ALIGN_LEFT;
    (void)snprintf(value.style, sizeof(value.style), "plain");
    (void)snprintf(value.transition, sizeof(value.transition), "none");
    (void)snprintf(value.focus_effect, sizeof(value.focus_effect), "none");
    if (type == UI_ELE_TEXT || type == UI_ELE_BUTTON) {
        value.content = malloc(2U);
        assert_non_null(value.content);
        memcpy(value.content, "X", 2U);
        value.content_capacity = 2U;
    }
    return value;
}

static void release(UiElement *value) {
    free(value->content);
    value->content = NULL;
}

static bool grid_contains_glyph_outside(Grid *grid, uint8_t glyph,
                                        int x0, int y0, int x1, int y1) {
    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            Cell value;
            if (!grid_get(grid, x, y, &value)) continue;
            if (value.glyph == glyph && (x < x0 || x >= x1 || y < y0 || y >= y1))
                return true;
        }
    }
    return false;
}

static bool grid_contains_any_glyph_in(Grid *grid, int x0, int y0, int width, int height) {
    for (int y = y0; y < y0 + height; y++) {
        for (int x = x0; x < x0 + width; x++) {
            Cell value;
            if (grid_get(grid, x, y, &value) && value.glyph != 0U && value.glyph != ' ')
                return true;
        }
    }
    return false;
}

static void test_pause_glitch_canvas_matches_layout_unit(void **state) {
    UiElement target = element("target", UI_ELE_CONTAINER, 30, 9, 24, 18);
    UiElement unit = element("pause_glitch", UI_ELE_ANIMATION, 0, 0, 24, 18);
    UiLayout layout = {0};
    UiCanvas *canvas = ui_canvas_create(80, 40);
    Grid *grid = grid_create(80, 40);
    size_t count = 80U * 40U;
    size_t i;
    (void)state;
    assert_non_null(canvas);
    assert_non_null(grid);
    (void)snprintf(unit.preset, sizeof(unit.preset), "pause_glitch");
    (void)snprintf(unit.target, sizeof(unit.target), "target");
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "context_enter");
    (void)snprintf(unit.orientation, sizeof(unit.orientation), "horizontal");
    layout.elements[0] = &target;
    layout.elements[1] = &unit;
    layout.element_count = 2;
    assert_true(ui_animation_render_pause_glitch_canvas(
        canvas, &target.layout, 0.5, false));
    assert_true(ui_animation_render_layout(&layout, grid, 32.991, false, false,
                                           UI_ANIMATION_EVENT_CONTEXT_ENTER));
    for (i = 0U; i < count; i++) {
        if (!canvas->touched[i]) continue;
        assert_memory_equal(&canvas->cells[i], &grid->cells[i], sizeof(Cell));
    }
    grid_destroy(grid);
    ui_canvas_destroy(canvas);
}

static void test_center_out_and_reduced_motion(void **state) {
    UiElement target = element("target", UI_ELE_TEXT, 6, 6, 10, 1);
    UiElement unit = element("center", UI_ELE_ANIMATION, 0, 0, 10, 6);
    UiLayout layout = {0};
    Grid *grid = grid_create(30, 20);
    Cell value;
    SDL_Color bg = {0, 0, 0, 255};
    (void)state;
    assert_non_null(grid);
    (void)snprintf(unit.preset, sizeof(unit.preset), "center_out");
    (void)snprintf(unit.target, sizeof(unit.target), "target");
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "while_visible");
    (void)snprintf(unit.orientation, sizeof(unit.orientation), "radial");
    unit.loop = 1;
    layout.elements[0] = &target;
    layout.elements[1] = &unit;
    layout.element_count = 2;
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, (SDL_Color){255, 255, 255, 255}, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                           UI_ANIMATION_EVENT_WHILE_VISIBLE));
    assert_true(grid_contains_glyph_outside(grid, 'X', 6, 6, 16, 7));
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, (SDL_Color){255, 255, 255, 255}, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 80.0, true, false,
                                           UI_ANIMATION_EVENT_WHILE_VISIBLE));
    assert_true(grid_get(grid, 1, 6, &value));
    assert_int_equal(value.glyph, ' ');
    grid_destroy(grid);
    release(&target);
}

static void test_focus_trigger_and_ordinary_transition(void **state) {
    UiElement button = element("button", UI_ELE_BUTTON, 5, 5, 8, 1);
    UiElement unit = element("focus_anim", UI_ELE_ANIMATION, 0, 0, 8, 3);
    UiLayout layout = {0};
    Grid *grid = grid_create(24, 16);
    Cell value;
    SDL_Color bg = {0, 0, 0, 255};
    (void)state;
    assert_non_null(grid);
    (void)snprintf(unit.preset, sizeof(unit.preset), "local_glitch");
    (void)snprintf(unit.target, sizeof(unit.target), "button");
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "focus");
    (void)snprintf(unit.orientation, sizeof(unit.orientation), "horizontal");
    unit.loop = 1;
    layout.elements[0] = &button;
    layout.elements[1] = &unit;
    layout.element_count = 2;
    grid_clear(grid, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 80.0, false, false,
                                           UI_ANIMATION_EVENT_FOCUS));
    assert_true(grid_get(grid, 9, 4, &value));
    assert_int_equal(value.glyph, ' ');
    button.focused = true;
    assert_true(ui_animation_render_layout(&layout, grid, 80.0, false, false,
                                           UI_ANIMATION_EVENT_FOCUS));
    assert_true(grid_get(grid, 9, 4, &value));
    assert_int_equal(value.glyph, ':');
    layout.element_count = 1;
    button.focused = false;
    (void)snprintf(button.transition, sizeof(button.transition), "center_out");
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, (SDL_Color){255, 255, 255, 255}, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                           UI_ANIMATION_EVENT_CONTEXT_ENTER));
    assert_true(grid_contains_glyph_outside(grid, 'X', 5, 5, 13, 6));
    grid_destroy(grid);
    release(&button);
}

static void test_trigger_filter_and_random_bounds(void **state) {
    UiElement target = element("target", UI_ELE_CONTAINER, 8, 6, 12, 8);
    UiElement unit = element("random", UI_ELE_ANIMATION, 1, 1, 6, 4);
    UiLayout layout = {0};
    Grid *grid = grid_create(32, 24);
    SDL_Color bg = {0, 0, 0, 255};
    (void)state;
    assert_non_null(grid);
    (void)snprintf(unit.preset, sizeof(unit.preset), "pause_glitch");
    (void)snprintf(unit.target, sizeof(unit.target), "target");
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "activate");
    (void)snprintf(unit.orientation, sizeof(unit.orientation), "horizontal");
    unit.randomize = 1;
    layout.elements[0] = &target;
    layout.elements[1] = &unit;
    layout.element_count = 2;
    grid_clear(grid, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 20.0, false, false,
                                           UI_ANIMATION_EVENT_CONTEXT_ENTER));
    assert_false(grid_contains_any_glyph_in(grid, 9, 7, 6, 4));
    assert_true(ui_animation_render_layout(&layout, grid, 20.0, false, false,
                                           UI_ANIMATION_EVENT_ACTIVATE));
    assert_true(grid_contains_any_glyph_in(grid, 9, 7, 6, 4));
    grid_clear(grid, bg);
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "context_exit");
    assert_true(ui_animation_render_layout(&layout, grid, 20.0, false, false,
                                           UI_ANIMATION_EVENT_ACTIVATE));
    assert_false(grid_contains_any_glyph_in(grid, 9, 7, 6, 4));
    assert_true(ui_animation_render_layout(&layout, grid, 20.0, false, false,
                                           UI_ANIMATION_EVENT_CONTEXT_EXIT));
    assert_true(grid_contains_any_glyph_in(grid, 9, 7, 6, 4));
    grid_destroy(grid);
}

static void test_directed_trigger_endpoints(void **state) {
    static const char *const triggers[] = {"context_enter", "context_exit", "focus", "activate"};
    static const double durations[] = {160.0, 120.0, 80.0, 80.0};
    static const UiAnimationEvent events[] = {
        UI_ANIMATION_EVENT_CONTEXT_ENTER, UI_ANIMATION_EVENT_CONTEXT_EXIT,
        UI_ANIMATION_EVENT_FOCUS, UI_ANIMATION_EVENT_ACTIVATE
    };
    UiElement target = element("target", UI_ELE_BUTTON, 8, 8, 12, 1);
    UiElement unit = element("unit", UI_ELE_ANIMATION, 0, 0, 12, 1);
    UiLayout layout = {0};
    Grid *grid = grid_create(40, 24);
    Grid *reference = grid_create(40, 24);
    SDL_Color bg = {0, 0, 0, 255};
    SDL_Color fg = {255, 255, 255, 255};
    size_t i;
    (void)state;
    assert_non_null(grid);
    assert_non_null(reference);
    target.focused = true;
    (void)snprintf(unit.target, sizeof(unit.target), "target");
    (void)snprintf(unit.preset, sizeof(unit.preset), "local_glitch");
    layout.elements[0] = &target;
    layout.elements[1] = &unit;
    layout.element_count = 2;
    grid_clear(reference, bg);
    ui_layout_render(&layout, reference, fg, bg);
    for (i = 0; i < sizeof(triggers) / sizeof(triggers[0]); i++) {
        (void)snprintf(unit.trigger, sizeof(unit.trigger), "%s", triggers[i]);
        grid_clear(grid, bg);
        ui_layout_render(&layout, grid, fg, bg);
        assert_true(ui_animation_render_layout(&layout, grid, durations[i], false, false, events[i]));
        assert_memory_equal(grid->cells, reference->cells, 40U * 24U * sizeof(Cell));
        assert_true(ui_animation_render_layout(&layout, grid, durations[i] / 2, true, false, events[i]));
        assert_memory_equal(grid->cells, reference->cells, 40U * 24U * sizeof(Cell));
    }
    release(&target);
    grid_destroy(reference);
    grid_destroy(grid);
}

static void test_lifecycle_protects_authored_spaces_not_backdrop(void **state) {
    UiElement target = element("target", UI_ELE_BUTTON, 8, 8, 12, 1);
    UiElement unit = element("unit", UI_ELE_ANIMATION, 0, 0, 12, 1);
    UiLayout layout = {0};
    Grid *grid = grid_create(40, 24);
    Grid *authored = grid_create(40, 24);
    SDL_Color bg = {3, 7, 11, 255};
    SDL_Color fg = {220, 230, 240, 255};
    bool decoration = false;
    (void)state;
    assert_non_null(grid);
    assert_non_null(authored);
    free(target.content);
    target.content = malloc(4);
    assert_non_null(target.content);
    memcpy(target.content, "A B", 4);
    target.content_capacity = 4;
    target.focused = true;
    (void)snprintf(unit.target, sizeof(unit.target), "target");
    (void)snprintf(unit.preset, sizeof(unit.preset), "center_out");
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "context_enter");
    (void)snprintf(unit.orientation, sizeof(unit.orientation), "radial");
    layout.elements[0] = &target;
    layout.elements[1] = &unit;
    layout.element_count = 2;
    ui_layout_render(&layout, authored, fg, bg);
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 0, false, false,
                                           UI_ANIMATION_EVENT_CONTEXT_ENTER));
    for (size_t i = 0; i < 40U * 24U; i++) {
        if (authored->cells[i].glyph)
            assert_memory_equal(&grid->cells[i], &authored->cells[i], sizeof(Cell));
        else if (grid->cells[i].glyph != ' ' && grid->cells[i].glyph != 0)
            decoration = true;
    }
    assert_true(decoration);
    release(&target);
    grid_destroy(authored);
    grid_destroy(grid);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_pause_glitch_canvas_matches_layout_unit),
        cmocka_unit_test(test_directed_trigger_endpoints),
        cmocka_unit_test(test_lifecycle_protects_authored_spaces_not_backdrop),
        cmocka_unit_test(test_center_out_and_reduced_motion),
        cmocka_unit_test(test_focus_trigger_and_ordinary_transition),
        cmocka_unit_test(test_trigger_filter_and_random_bounds)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}