#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ui_animation.h"
#include "../src/ui_theme.h"

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

static void assert_region_equal(Grid *actual, Grid *expected,
                                int x, int y, int width, int height) {
    for (int row = y; row < y + height; row++) {
        for (int column = x; column < x + width; column++) {
            Cell actual_cell;
            Cell expected_cell;
            assert_true(grid_get(actual, column, row, &actual_cell));
            assert_true(grid_get(expected, column, row, &expected_cell));
            assert_memory_equal(&actual_cell, &expected_cell, sizeof(Cell));
        }
    }
}

static void test_button_effect_presets_are_deterministic_and_bounded(void **state) {
    static const char *const presets[] = {
        "edge_trace", "chromatic_register", "command_flash"
    };
    static const char *const triggers[] = {"focus", "focus", "activate"};
    static const UiAnimationEvent events[] = {
        UI_ANIMATION_EVENT_FOCUS, UI_ANIMATION_EVENT_FOCUS,
        UI_ANIMATION_EVENT_ACTIVATE
    };
    UiElement target = element("target", UI_ELE_BUTTON, 8, 8, 12, 1);
    UiElement unit = element("unit", UI_ELE_ANIMATION, 0, 0, 12, 1);
    UiLayout layout = {0};
    Grid *grid = grid_create(32, 20);
    Grid *repeat = grid_create(32, 20);
    Grid *reference = grid_create(32, 20);
    SDL_Color bg = {5, 8, 10, 255};
    SDL_Color fg = {242, 247, 248, 255};
    Cell value;
    (void)state;
    assert_non_null(grid);
    assert_non_null(repeat);
    assert_non_null(reference);
    target.focused = true;
    (void)snprintf(unit.target, sizeof(unit.target), "target");
    (void)snprintf(unit.orientation, sizeof(unit.orientation), "horizontal");
    layout.elements[0] = &target;
    layout.elements[1] = &unit;
    layout.element_count = 2;
    grid_clear(reference, bg);
    ui_layout_render(&layout, reference, fg, bg);

    for (size_t i = 0U; i < sizeof(presets) / sizeof(presets[0]); i++) {
        (void)snprintf(unit.preset, sizeof(unit.preset), "%s", presets[i]);
        (void)snprintf(unit.trigger, sizeof(unit.trigger), "%s", triggers[i]);
        assert_true(ui_ele_animation_is_valid(&unit));

        grid_clear(grid, bg);
        ui_layout_render(&layout, grid, fg, bg);
        assert_true(ui_animation_render_layout(&layout, grid, 0.0, false, false, events[i]));
        assert_region_equal(grid, reference, 8, 8, 12, 1);

        grid_clear(repeat, bg);
        ui_layout_render(&layout, repeat, fg, bg);
        assert_true(ui_animation_render_layout(&layout, repeat, 0.0, false, false, events[i]));
        assert_memory_equal(grid->cells, repeat->cells, 32U * 20U * sizeof(Cell));

        grid_clear(grid, bg);
        ui_layout_render(&layout, grid, fg, bg);
        assert_true(ui_animation_render_layout(&layout, grid, 40.0, false, false, events[i]));
        assert_region_equal(grid, reference, 8, 8, 12, 1);

        grid_clear(grid, bg);
        ui_layout_render(&layout, grid, fg, bg);
        assert_true(ui_animation_render_layout(&layout, grid, 80.0, false, false, events[i]));
        assert_memory_equal(grid->cells, reference->cells, 32U * 20U * sizeof(Cell));

        grid_clear(grid, bg);
        ui_layout_render(&layout, grid, fg, bg);
        assert_true(ui_animation_render_layout(&layout, grid, 40.0, true, false, events[i]));
        assert_memory_equal(grid->cells, reference->cells, 32U * 20U * sizeof(Cell));
    }

    (void)snprintf(unit.preset, sizeof(unit.preset), "edge_trace");
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "focus");
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                           UI_ANIMATION_EVENT_FOCUS));
    assert_true(grid_get(grid, 8, 7, &value));
    assert_int_equal(value.glyph, '=');
    assert_int_equal(value.fg.g, 245);

    (void)snprintf(unit.preset, sizeof(unit.preset), "chromatic_register");
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                           UI_ANIMATION_EVENT_FOCUS));
    assert_true(grid_get(grid, 10, 7, &value));
    assert_int_equal(value.fg.g, 245);
    assert_true(grid_get(grid, 16, 9, &value));
    assert_int_equal(value.fg.g, 255);

    (void)snprintf(unit.preset, sizeof(unit.preset), "command_flash");
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "activate");
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                           UI_ANIMATION_EVENT_ACTIVATE));
    assert_true(grid_get(grid, 5, 8, &value));
    assert_int_equal(value.glyph, '!');
    assert_true(grid_get(grid, 22, 8, &value));
    assert_int_equal(value.glyph, '!');
    target.focused = false;
    grid_clear(repeat, bg);
    ui_layout_render(&layout, repeat, fg, bg);
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                           UI_ANIMATION_EVENT_ACTIVATE));
    assert_memory_equal(grid->cells, repeat->cells, 32U * 20U * sizeof(Cell));
    target.focused = true;

    unit.loop = 1;
    assert_false(ui_ele_animation_is_valid(&unit));
    unit.loop = 0;
    unit.randomize = 1;
    assert_false(ui_ele_animation_is_valid(&unit));
    unit.randomize = 0;
    (void)snprintf(unit.orientation, sizeof(unit.orientation), "vertical");
    assert_false(ui_ele_animation_is_valid(&unit));
    (void)snprintf(unit.orientation, sizeof(unit.orientation), "horizontal");
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "focus");
    assert_false(ui_ele_animation_is_valid(&unit));

    unit.trigger[0] = '\0';
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "activate");
    target.type = UI_ELE_CONTAINER;
    grid_clear(grid, bg);
    assert_false(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                            UI_ANIMATION_EVENT_ACTIVATE));

    release(&target);
    grid_destroy(reference);
    grid_destroy(repeat);
    grid_destroy(grid);
}

static void test_context_transition_presets_are_deterministic_and_class_bounded(void **state) {
    UiElement button = element("button", UI_ELE_BUTTON, 8, 8, 12, 1);
    UiElement panel = element("panel", UI_ELE_CONTAINER, 8, 6, 12, 8);
    UiElement unit = element("transition", UI_ELE_ANIMATION, 0, 0, 0, 0);
    UiLayout layout = {0};
    Grid *grid = grid_create(32, 24);
    Grid *repeat = grid_create(32, 24);
    Grid *reference = grid_create(32, 24);
    SDL_Color bg = {5, 8, 10, 255};
    SDL_Color fg = {242, 247, 248, 255};
    Cell value;
    (void)state;
    assert_non_null(grid);
    assert_non_null(repeat);
    assert_non_null(reference);
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "context_enter");
    (void)snprintf(unit.orientation, sizeof(unit.orientation), "radial");

    layout.elements[0] = &button;
    layout.elements[1] = &unit;
    layout.element_count = 2;
    (void)snprintf(unit.preset, sizeof(unit.preset), "button_reassemble");
    (void)snprintf(unit.target, sizeof(unit.target), "button");
    assert_true(ui_ele_animation_is_valid(&unit));
    unit.loop = 1;
    assert_false(ui_ele_animation_is_valid(&unit));
    unit.loop = 0;
    (void)snprintf(unit.orientation, sizeof(unit.orientation), "horizontal");
    assert_false(ui_ele_animation_is_valid(&unit));
    (void)snprintf(unit.orientation, sizeof(unit.orientation), "radial");
    grid_clear(reference, bg);
    ui_layout_render(&layout, reference, fg, bg);
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                           UI_ANIMATION_EVENT_CONTEXT_ENTER));
    assert_region_equal(grid, reference, 8, 8, 12, 1);
    assert_true(grid_get(grid, 7, 7, &value));
    assert_int_equal(value.glyph, '[');
    grid_clear(repeat, bg);
    ui_layout_render(&layout, repeat, fg, bg);
    assert_true(ui_animation_render_layout(&layout, repeat, 0.0, false, false,
                                           UI_ANIMATION_EVENT_CONTEXT_ENTER));
    assert_memory_equal(grid->cells, repeat->cells, 32U * 24U * sizeof(Cell));
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 160.0, false, false,
                                           UI_ANIMATION_EVENT_CONTEXT_ENTER));
    assert_memory_equal(grid->cells, reference->cells, 32U * 24U * sizeof(Cell));
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 40.0, true, false,
                                           UI_ANIMATION_EVENT_CONTEXT_ENTER));
    assert_memory_equal(grid->cells, reference->cells, 32U * 24U * sizeof(Cell));
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "context_exit");
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                           UI_ANIMATION_EVENT_CONTEXT_EXIT));
    assert_true(grid_get(grid, 12, 7, &value));
    assert_int_equal(value.glyph, '[');
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 120.0, false, false,
                                           UI_ANIMATION_EVENT_CONTEXT_EXIT));
    assert_memory_equal(grid->cells, reference->cells, 32U * 24U * sizeof(Cell));

    layout.elements[0] = &panel;
    (void)snprintf(unit.preset, sizeof(unit.preset), "panel_register");
    (void)snprintf(unit.target, sizeof(unit.target), "panel");
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "context_enter");
    assert_true(ui_ele_animation_is_valid(&unit));
    unit.randomize = 1;
    assert_false(ui_ele_animation_is_valid(&unit));
    unit.randomize = 0;
    grid_clear(reference, bg);
    ui_layout_render(&layout, reference, fg, bg);
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                           UI_ANIMATION_EVENT_CONTEXT_ENTER));
    assert_true(grid_get(grid, 3, 1, &value));
    assert_int_equal(value.glyph, '+');
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "context_exit");
    assert_true(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                           UI_ANIMATION_EVENT_CONTEXT_EXIT));
    assert_true(grid_get(grid, 7, 5, &value));
    assert_int_equal(value.glyph, '+');

    (void)snprintf(unit.preset, sizeof(unit.preset), "button_reassemble");
    assert_false(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                            UI_ANIMATION_EVENT_CONTEXT_EXIT));
    (void)snprintf(unit.preset, sizeof(unit.preset), "panel_register");
    (void)snprintf(unit.target, sizeof(unit.target), "button");
    layout.elements[0] = &button;
    assert_false(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                            UI_ANIMATION_EVENT_CONTEXT_EXIT));

    release(&button);
    grid_destroy(reference);
    grid_destroy(repeat);
    grid_destroy(grid);
}

static void test_generated_context_transitions_skip_incompatible_elements(void **state) {
    UiElement button = element("button", UI_ELE_BUTTON, 8, 8, 12, 1);
    UiElement panel = element("panel", UI_ELE_CONTAINER, 4, 4, 24, 12);
    UiLayout layout = {0};
    Grid *grid = grid_create(40, 28);
    Grid *repeat = grid_create(40, 28);
    SDL_Color bg = {5, 8, 10, 255};
    SDL_Color fg = {242, 247, 248, 255};
    (void)state;
    assert_non_null(grid);
    assert_non_null(repeat);
    button.parent = &panel;
    layout.elements[0] = &button;
    layout.element_count = 1;

    (void)snprintf(layout.transition, sizeof(layout.transition), "button_reassemble");
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                           UI_ANIMATION_EVENT_CONTEXT_ENTER));
    (void)snprintf(layout.transition, sizeof(layout.transition), "panel_register");
    grid_clear(repeat, bg);
    ui_layout_render(&layout, repeat, fg, bg);
    assert_true(ui_animation_render_layout(&layout, repeat, 0.0, false, false,
                                           UI_ANIMATION_EVENT_CONTEXT_ENTER));
    assert_false(memcmp(grid->cells, repeat->cells, 40U * 28U * sizeof(Cell)) == 0);

    release(&button);
    grid_destroy(repeat);
    grid_destroy(grid);
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

static void test_living_field_is_deterministic_bounded_and_reduced(void **state) {
    UiElement target = element("target", UI_ELE_CONTAINER, 8, 6, 20, 10);
    UiElement unit = element("unit", UI_ELE_ANIMATION, 0, 0, 20, 10);
    UiLayout layout = {0};
    Grid *grid = grid_create(40, 24);
    Grid *repeat = grid_create(40, 24);
    Grid *reference = grid_create(40, 24);
    SDL_Color bg = {5, 8, 10, 255};
    SDL_Color fg = {242, 247, 248, 255};
    unsigned int period = ui_theme_motion_duration_ms(UI_THEME_MOTION_AMBIENT, false);
    const UiThemeColor *material = ui_theme_material_palette();
    const UiThemeColor neutral =
        ui_theme_provisional_tokens()->palette.text_secondary;
    int lit = 0;
    int lit_neutral = 0;
    int hue_seen = 0;
    bool seen[UI_THEME_MATERIAL_COLOR_COUNT] = {false};
    bool outside = false;
    bool foreign = false;
    (void)state;
    assert_non_null(grid);
    assert_non_null(repeat);
    assert_non_null(reference);
    assert_true(period > 0U);
    (void)snprintf(unit.target, sizeof(unit.target), "target");
    (void)snprintf(unit.preset, sizeof(unit.preset), "living_field");
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "while_visible");
    (void)snprintf(unit.orientation, sizeof(unit.orientation), "radial");
    layout.elements[0] = &target;
    layout.elements[1] = &unit;
    layout.element_count = 2;

    assert_true(ui_ele_animation_is_valid(&unit));
    unit.randomize = 1;
    assert_false(ui_ele_animation_is_valid(&unit));
    unit.randomize = 0;
    unit.loop = 1;
    assert_false(ui_ele_animation_is_valid(&unit));
    unit.loop = 0;
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "context_enter");
    assert_false(ui_ele_animation_is_valid(&unit));
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "while_visible");
    assert_true(ui_ele_animation_is_valid(&unit));

    grid_clear(reference, bg);
    ui_layout_render(&layout, reference, fg, bg);
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                           UI_ANIMATION_EVENT_WHILE_VISIBLE));
    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            Cell cell;
            assert_true(grid_get(grid, x, y, &cell));
            if (cell.glyph == 0U || cell.glyph == ' ') continue;
            if (x < 8 || x >= 28 || y < 6 || y >= 16) {
                outside = true;
            } else {
                lit++;
                {
                    bool known = false;
                    for (int m = 0; m < UI_THEME_MATERIAL_COLOR_COUNT; m++)
                        if (cell.fg.r == material[m].red &&
                            cell.fg.g == material[m].green &&
                            cell.fg.b == material[m].blue) {
                            seen[m] = true;
                            known = true;
                        }
                    if (cell.fg.r == neutral.red &&
                        cell.fg.g == neutral.green &&
                        cell.fg.b == neutral.blue) {
                        lit_neutral++;
                        known = true;
                    }
                    if (!known) foreign = true;
                }
            }
        }
    }
    assert_false(outside);
    assert_false(foreign);
    assert_true(lit > 60);
    /* Selective colour over a neutral structure: the field mixes coloured
       traces and neutral marks instead of ordering a full-spectrum ramp, so
       both classes must be present. */
    assert_true(lit_neutral > 0);
    assert_true(lit_neutral < lit);

    grid_clear(repeat, bg);
    ui_layout_render(&layout, repeat, fg, bg);
    assert_true(ui_animation_render_layout(&layout, repeat, 0.0, false, false,
                                           UI_ANIMATION_EVENT_WHILE_VISIBLE));
    assert_memory_equal(grid->cells, repeat->cells, 40U * 24U * sizeof(Cell));

    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, (double)period, false, false,
                                           UI_ANIMATION_EVENT_WHILE_VISIBLE));
    assert_memory_equal(grid->cells, repeat->cells, 40U * 24U * sizeof(Cell));

    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 0.0, true, false,
                                           UI_ANIMATION_EVENT_WHILE_VISIBLE));
    assert_memory_equal(grid->cells, reference->cells, 40U * 24U * sizeof(Cell));

    /* The chromatic fringe is a directional chromatic aberration and draws only
       the three additive primaries; sample several phases to observe them. */
    for (int step = 0; step < 8; step++) {
        double t = (double)period * (double)step / 8.0;
        grid_clear(grid, bg);
        ui_layout_render(&layout, grid, fg, bg);
        assert_true(ui_animation_render_layout(&layout, grid, t, false, false,
                                               UI_ANIMATION_EVENT_WHILE_VISIBLE));
        for (int y = 6; y < 16; y++) {
            for (int x = 8; x < 28; x++) {
                Cell cell;
                assert_true(grid_get(grid, x, y, &cell));
                if (cell.glyph == 0U || cell.glyph == ' ') continue;
                for (int m = 0; m < UI_THEME_MATERIAL_COLOR_COUNT; m++)
                    if (cell.fg.r == material[m].red &&
                        cell.fg.g == material[m].green &&
                        cell.fg.b == material[m].blue) seen[m] = true;
            }
        }
    }
    for (int m = 0; m < UI_THEME_MATERIAL_COLOR_COUNT; m++)
        if (seen[m]) hue_seen++;
    assert_true(hue_seen >= 3);

    release(&target);
    grid_destroy(reference);
    grid_destroy(repeat);
    grid_destroy(grid);
}

/* Cells the settle has painted, and the ones the tide has covered: everything the
   surface does not read as blank, whichever of the two blank spellings (a zero
   glyph or an authored space) the cleared grid carries. Used to measure how much
   of the surface a material reaches at a given moment. */
static int grid_filled_cells(Grid *grid) {
    int count = 0;
    for (int i = 0; i < grid->width * grid->height; i++)
        if (grid->cells[i].glyph != 0U && grid->cells[i].glyph != ' ') count++;
    return count;
}

/* How many distinct glyphs a surface carries, ignoring blanks: the flood is
   material, so it must be woven rather than one repeated mark. */
static int grid_distinct_glyphs(Grid *grid) {
    bool seen[256] = {false};
    int count = 0;
    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            Cell value;
            if (!grid_get(grid, x, y, &value)) continue;
            if (value.glyph == 0U || value.glyph == ' ') continue;
            if (!seen[value.glyph]) {
                seen[value.glyph] = true;
                count++;
            }
        }
    }
    return count;
}

/* True when at least one cell is drawn in one of the three additive primaries,
   which is how the tide's front edge carries its chromatic aberration. */
static bool grid_uses_fringe_colour(Grid *grid) {
    const UiThemeColor *material = ui_theme_material_palette();
    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            Cell value;
            if (!grid_get(grid, x, y, &value)) continue;
            for (int m = 0; m < UI_THEME_MATERIAL_COLOR_COUNT; m++)
                if ((m == 0 || m == 2 || m == 4) &&
                    value.fg.r == material[m].red &&
                    value.fg.g == material[m].green &&
                    value.fg.b == material[m].blue)
                    return true;
        }
    }
    return false;
}

/* The tide covers the surface between states, and it is the one primitive allowed
   to pass over authored content — for exactly as long as the transition lasts. The
   rule it serves is recorded in the reference of record (2026-10-09 direction). */
static void test_tide_covers_the_surface_then_restores_it(void **state) {
    UiElement target = element("target", UI_ELE_CONTAINER, 0, 0, 40, 24);
    UiElement label = element("label", UI_ELE_BUTTON, 8, 8, 12, 1);
    UiElement cover = element("cover", UI_ELE_ANIMATION, 0, 0, 40, 24);
    UiElement reveal = element("reveal", UI_ELE_ANIMATION, 0, 0, 40, 24);
    UiLayout layout = {0};
    Grid *grid = grid_create(40, 24);
    Grid *repeat = grid_create(40, 24);
    SDL_Color bg = {5, 8, 10, 255};
    SDL_Color fg = {242, 247, 248, 255};
    unsigned int exit_ms = ui_theme_motion_duration_ms(UI_THEME_MOTION_MAJOR_EXIT, false);
    unsigned int enter_ms = ui_theme_motion_duration_ms(UI_THEME_MOTION_MAJOR_ENTER, false);
    Cell cell;
    (void)state;
    assert_non_null(grid);
    assert_non_null(repeat);
    (void)snprintf(cover.target, sizeof(cover.target), "target");
    (void)snprintf(cover.preset, sizeof(cover.preset), "tide_cover");
    (void)snprintf(cover.trigger, sizeof(cover.trigger), "context_exit");
    (void)snprintf(cover.orientation, sizeof(cover.orientation), "horizontal");
    cover.extent = UI_EXTENT_SURFACE;
    (void)snprintf(reveal.target, sizeof(reveal.target), "target");
    (void)snprintf(reveal.preset, sizeof(reveal.preset), "tide_reveal");
    (void)snprintf(reveal.trigger, sizeof(reveal.trigger), "context_enter");
    (void)snprintf(reveal.orientation, sizeof(reveal.orientation), "horizontal");
    reveal.extent = UI_EXTENT_SURFACE;
    layout.elements[0] = &target;
    layout.elements[1] = &label;
    layout.elements[2] = &cover;
    layout.element_count = 3;

    /* A cover is an exit and a reveal is an enter: neither accepts the other's
       trigger, so nothing covers on the way in by accident. */
    assert_true(ui_ele_animation_is_valid(&cover));
    assert_true(ui_ele_animation_is_valid(&reveal));
    (void)snprintf(cover.trigger, sizeof(cover.trigger), "context_enter");
    assert_false(ui_ele_animation_is_valid(&cover));
    (void)snprintf(cover.trigger, sizeof(cover.trigger), "context_exit");

    /* The exit starts uncovered: the surface still carries the authored glyph. */
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                           UI_ANIMATION_EVENT_CONTEXT_EXIT));
    assert_true(grid_get(grid, 8, 8, &cell));
    assert_int_equal(cell.glyph, 'X');

    /* ... and ends covered, with the control passed over: the one sanctioned
       exception. The flood is material, so it is woven and carries chromatic
       edges rather than being one repeated mark. The sample is one millisecond
       inside the window because a finite transition ends by going invisible at
       its own duration, exactly like every other unit. */
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, (double)exit_ms - 1.0,
                                           false, false,
                                           UI_ANIMATION_EVENT_CONTEXT_EXIT));
    assert_true(grid_get(grid, 8, 8, &cell));
    assert_true(cell.glyph != 'X');
    assert_true(grid_distinct_glyphs(grid) >= 3);
    assert_true(grid_uses_fringe_colour(grid));

    /* The flood crosses at a steady pace instead of arriving in the first frame:
       half way through the window its front has crossed about half the diagonal,
       which is roughly a fifth of the area in one corner. A fast-start curve
       would already have the whole surface covered here, which is exactly the
       wipe the tide's own curve exists to avoid. */
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, (double)exit_ms / 2.0, false,
                                           false, UI_ANIMATION_EVENT_CONTEXT_EXIT));
    {
        int filled = grid_filled_cells(grid);
        int total = 40 * 24;
        assert_true(filled > total * 10 / 100);
        assert_true(filled < total * 50 / 100);
    }

    /* The enter begins exactly where the exit ended — covered — and resolves with
       the control back, untouched. */
    layout.elements[2] = &reveal;
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                           UI_ANIMATION_EVENT_CONTEXT_ENTER));
    assert_true(grid_get(grid, 8, 8, &cell));
    assert_true(cell.glyph != 'X');
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, (double)enter_ms, false,
                                           false, UI_ANIMATION_EVENT_CONTEXT_ENTER));
    assert_true(grid_get(grid, 8, 8, &cell));
    assert_int_equal(cell.glyph, 'X');

    /* Deterministic: the same explicit time composes the same cells. */
    layout.elements[2] = &cover;
    for (int step = 0; step < 4; step++) {
        double t = (double)exit_ms * (double)step / 4.0;
        grid_clear(grid, bg);
        ui_layout_render(&layout, grid, fg, bg);
        assert_true(ui_animation_render_layout(&layout, grid, t, false, false,
                                               UI_ANIMATION_EVENT_CONTEXT_EXIT));
        grid_clear(repeat, bg);
        ui_layout_render(&layout, repeat, fg, bg);
        assert_true(ui_animation_render_layout(&layout, repeat, t, false, false,
                                               UI_ANIMATION_EVENT_CONTEXT_EXIT));
        assert_memory_equal(grid->cells, repeat->cells, 40U * 24U * sizeof(Cell));
    }

    /* Reduced motion never covers a control, and a static preview never plays a
       transition: it shows the state the surface settles into. */
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, (double)exit_ms, true,
                                           false, UI_ANIMATION_EVENT_CONTEXT_EXIT));
    assert_true(grid_get(grid, 8, 8, &cell));
    assert_int_equal(cell.glyph, 'X');
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, (double)exit_ms, false,
                                           false, UI_ANIMATION_EVENT_PREVIEW));
    assert_true(grid_get(grid, 8, 8, &cell));
    assert_int_equal(cell.glyph, 'X');

    release(&cover);
    release(&reveal);
    release(&label);
    release(&target);
    grid_destroy(repeat);
    grid_destroy(grid);
}

/* An element's `extent=` chooses whether a backdrop is a bounded region or the
   whole surface. The bounded case follows its target; the surface case fills the
   frame it renders into, so a menu backdrop covers the display without an asset
   carrying a grid size. */

/* Cells the settle's lattice occupies on a surface of this size: the far corner of
   each block of four, which is where a character is still standing at the halfway
   point of the window (UI_SETTLE_LATTICE). */
static int settle_lattice_cells(int width, int height) {
    int count = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int anchor_x = ((x / 4) + 1) * 4 - 1;
            int anchor_y = ((y / 4) + 1) * 4 - 1;
            if (anchor_x > width - 1) anchor_x = width - 1;
            if (anchor_y > height - 1) anchor_y = height - 1;
            if (anchor_x == x && anchor_y == y) count++;
        }
    }
    return count;
}

/* The outgoing menu as it stands when the action is taken: its authored
   characters over the fabric its backdrop fills the surface with. That is the
   surface a settle has to start from, unchanged. */
static void render_outgoing_surface(UiLayout *layout, Grid *grid, SDL_Color fg,
                                    SDL_Color bg) {
    grid_clear(grid, bg);
    ui_layout_render(layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(layout, grid, 0.0, false, false,
                                           UI_ANIMATION_EVENT_WHILE_VISIBLE));
}

/* True when a glyph belongs to the fabric's alphabet — the ramp the backdrop and
   the tide are drawn from. */
static bool settle_ramp_glyph(uint8_t glyph) {
    static const char ramp[] = ". :-=+*#";
    for (size_t i = 0; i < sizeof(ramp) - 1U; i++)
        if (glyph == (uint8_t)ramp[i]) return true;
    return false;
}

/* The settle: a surface handing over to the world frame condenses onto its
   lattice, cycles through the fabric's glyphs where the backdrop wave crosses it,
   and then thins away, so the live frame underneath is left uncovered. The rule it
   serves is recorded in the reference of record (2026-10-09 direction, item 4,
   Stage C). */
/* Cells the tide has painted: the material's own neutral, or one of the three
   additive primaries its edge and wake are drawn in. */
static bool tide_painted(SDL_Color c) {
    const UiThemeColor *material = ui_theme_material_palette();
    UiThemeColor neutral = ui_theme_provisional_tokens()->palette.text_secondary;
    if (c.r == neutral.red && c.g == neutral.green && c.b == neutral.blue) return true;
    for (int m = 0; m < UI_THEME_MATERIAL_COLOR_COUNT; m++)
        if ((m == 0 || m == 2 || m == 4) && c.r == material[m].red &&
            c.g == material[m].green && c.b == material[m].blue)
            return true;
    return false;
}

static int tide_painted_cells(Grid *grid) {
    int count = 0;
    for (int i = 0; i < grid->width * grid->height; i++)
        if (tide_painted(grid->cells[i].fg)) count++;
    return count;
}

static void tide_render(UiLayout *layout, Grid *grid, double elapsed_ms,
                        UiAnimationEvent event) {
    SDL_Color bg = {5, 8, 10, 255};
    SDL_Color fg = {242, 247, 248, 255};
    grid_clear(grid, bg);
    ui_layout_render(layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(layout, grid, elapsed_ms, false, false, event));
}

/* The time at which a direction has painted `target` cells, by bisection: the cover
   is monotone through each window, so the two halves can be compared at the same
   cover rather than at the same elapsed time. */
static double tide_time_for_cover(UiLayout *layout, Grid *grid, UiAnimationEvent event,
                                  unsigned int window_ms, int target) {
    double low = 0.0;
    double high = (double)window_ms;
    for (int step = 0; step < 24; step++) {
        double mid = (low + high) / 2.0;
        bool above;
        tide_render(layout, grid, mid, event);
        above = tide_painted_cells(grid) >= target;
        if (event == UI_ANIMATION_EVENT_CONTEXT_ENTER) {
            /* The reveal opens covered and drains, so it is monotone downwards. */
            if (above) low = mid; else high = mid;
        } else if (above) {
            high = mid;
        } else {
            low = mid;
        }
    }
    return (low + high) / 2.0;
}

/* The tide has to read as water arriving, not as a wipe: an edge that swells and
   travels, a woven sheet rather than one flat stripe, one continuous motion across
   the handover, and a reveal that unwinds exactly what the cover wound. The first
   attempt failed all four — the owner's report was "the wave straightens and freezes
   for a moment ... more like a stutter between things" — because its edge was a
   straight anti-diagonal, a constant band lift clamped every cell to one glyph, and
   its phase came from the ambient clock, which over a 120 ms window barely moves and
   does not line up across the handover. */
static void test_tide_reads_as_a_tide_not_a_wipe(void **state) {
    UiElement target = element("target", UI_ELE_CONTAINER, 0, 0, 120, 80);
    UiElement label = element("label", UI_ELE_BUTTON, 40, 20, 24, 1);
    UiElement cover = element("cover", UI_ELE_ANIMATION, 0, 0, 120, 80);
    UiElement reveal = element("reveal", UI_ELE_ANIMATION, 0, 0, 120, 80);
    UiLayout layout = {0};
    Grid *grid = grid_create(120, 80);
    Grid *exit_frame = grid_create(120, 80);
    Grid *enter_frame = grid_create(120, 80);
    unsigned int exit_ms = ui_theme_motion_duration_ms(UI_THEME_MOTION_MAJOR_EXIT, false);
    unsigned int enter_ms = ui_theme_motion_duration_ms(UI_THEME_MOTION_MAJOR_ENTER, false);
    unsigned int ambient = ui_theme_ambient_period_ms();
    const size_t cells = 120U * 80U;
    int changed;
    (void)state;
    assert_non_null(grid);
    assert_non_null(exit_frame);
    assert_non_null(enter_frame);
    (void)snprintf(cover.target, sizeof(cover.target), "target");
    (void)snprintf(cover.preset, sizeof(cover.preset), "tide_cover");
    (void)snprintf(cover.trigger, sizeof(cover.trigger), "context_exit");
    (void)snprintf(cover.orientation, sizeof(cover.orientation), "horizontal");
    cover.extent = UI_EXTENT_SURFACE;
    (void)snprintf(reveal.target, sizeof(reveal.target), "target");
    (void)snprintf(reveal.preset, sizeof(reveal.preset), "tide_reveal");
    (void)snprintf(reveal.trigger, sizeof(reveal.trigger), "context_enter");
    (void)snprintf(reveal.orientation, sizeof(reveal.orientation), "horizontal");
    reveal.extent = UI_EXTENT_SURFACE;
    layout.elements[0] = &target;
    layout.elements[1] = &label;
    layout.elements[2] = &cover;
    layout.elements[3] = &reveal;
    layout.element_count = 4;

    /* The handover: the runner drops the exit overlay on the frame the unit ends and
       the incoming surface's reveal starts on that frame. One material, so those two
       frames are nearly the same surface — 0.12% of the surface apart when measured,
       against 82% for the ambient-clock phase, which is the stutter. */
    tide_render(&layout, exit_frame, (double)exit_ms - 1.0,
                UI_ANIMATION_EVENT_CONTEXT_EXIT);
    tide_render(&layout, enter_frame, 0.0, UI_ANIMATION_EVENT_CONTEXT_ENTER);
    changed = 0;
    for (size_t i = 0; i < cells; i++)
        if (memcmp(&exit_frame->cells[i], &enter_frame->cells[i], sizeof(Cell)) != 0)
            changed++;
    if (changed > (int)(cells / 100U))
        fprintf(stderr, "FAIL-PRODUCT: handover differs in %d of %zu cells\n", changed,
                cells);
    assert_true(changed <= (int)(cells / 100U));

    /* The reveal retraces the cover: at the same cover the two directions compose the
       same surface, because the phase is the cover and not a clock. */
    {
        double exit_t = tide_time_for_cover(&layout, grid, UI_ANIMATION_EVENT_CONTEXT_EXIT,
                                            exit_ms, (int)(cells / 2U));
        double enter_t = tide_time_for_cover(&layout, grid,
                                             UI_ANIMATION_EVENT_CONTEXT_ENTER, enter_ms,
                                             (int)(cells / 2U));
        tide_render(&layout, exit_frame, exit_t, UI_ANIMATION_EVENT_CONTEXT_EXIT);
        tide_render(&layout, enter_frame, enter_t, UI_ANIMATION_EVENT_CONTEXT_ENTER);
        changed = 0;
        for (size_t i = 0; i < cells; i++)
            if (memcmp(&exit_frame->cells[i], &enter_frame->cells[i], sizeof(Cell)) != 0)
                changed++;
        if (changed > (int)(cells / 100U))
            fprintf(stderr,
                    "FAIL-PRODUCT: at half cover the two directions differ in %d of %zu "
                    "cells (exit t=%.1f, enter t=%.1f)\n",
                    changed, cells, exit_t, enter_t);
        assert_true(changed <= (int)(cells / 100U));
    }

    /* The edge swells, and the sheet under it is woven: a straight edge gives one
       value of (edge + y) per row (the wipe the owner rejected), and a clamped band
       gives one glyph (the flat stripe). */
    {
        double mid = tide_time_for_cover(&layout, grid, UI_ANIMATION_EVENT_CONTEXT_EXIT,
                                         exit_ms, (int)(cells / 2U));
        int low = 1 << 30;
        int high = -(1 << 30);
        int rows = 0;
        int distinct = 0;
        bool seen[256] = {false};
        tide_render(&layout, grid, mid, UI_ANIMATION_EVENT_CONTEXT_EXIT);
        for (int y = 0; y < 80; y++) {
            int edge = -1;
            for (int x = 0; x < 120; x++) {
                Cell cell;
                if (!grid_get(grid, x, y, &cell)) continue;
                if (tide_painted(cell.fg) && cell.glyph != 0U && cell.glyph != ' ') {
                    edge = x;
                    if (!seen[cell.glyph]) {
                        seen[cell.glyph] = true;
                        distinct++;
                    }
                }
            }
            if (edge <= 0) continue;
            if (edge + y < low) low = edge + y;
            if (edge + y > high) high = edge + y;
            rows++;
        }
        assert_true(rows > 20);
        if (high - low < 3)
            fprintf(stderr, "FAIL-PRODUCT: the flood's edge is straight (spread %d)\n",
                    high - low);
        assert_true(high - low >= 3);
        if (distinct < 3)
            fprintf(stderr, "FAIL-PRODUCT: the flood is one flat stripe (%d glyphs)\n",
                    distinct);
        assert_true(distinct >= 3);
    }

    /* A transition is not the backdrop: the speed of the ambient loop does not touch
       it, or slowing the backdrop would silently slow every state change with it. */
    {
        double t = (double)exit_ms / 2.0;
        tide_render(&layout, exit_frame, t, UI_ANIMATION_EVENT_CONTEXT_EXIT);
        ui_theme_set_ambient_period_ms(ambient * 2U);
        tide_render(&layout, enter_frame, t, UI_ANIMATION_EVENT_CONTEXT_EXIT);
        assert_memory_equal(exit_frame->cells, enter_frame->cells,
                            cells * sizeof(Cell));
    }

    release(&reveal);
    release(&cover);
    release(&label);
    release(&target);
    grid_destroy(enter_frame);
    grid_destroy(exit_frame);
    grid_destroy(grid);
    ui_theme_set_ambient_period_ms(ambient);
}



static void test_settle_condenses_to_the_lattice_then_resolves(void **state) {
    UiElement target = element("target", UI_ELE_CONTAINER, 0, 0, 40, 24);
    UiElement label = element("label", UI_ELE_BUTTON, 8, 8, 12, 1);
    UiElement field = element("field", UI_ELE_ANIMATION, 0, 0, 40, 24);
    UiLayout layout = {0};
    Grid *grid = grid_create(40, 24);
    Grid *start = grid_create(40, 24);
    Grid *repeat = grid_create(40, 24);
    SDL_Color bg = {5, 8, 10, 255};
    SDL_Color fg = {242, 247, 248, 255};
    unsigned int exit_ms = ui_theme_motion_duration_ms(UI_THEME_MOTION_MAJOR_EXIT, false);
    int at_start;
    int half;
    int quarter;
    int late;
    (void)state;
    assert_non_null(grid);
    assert_non_null(start);
    assert_non_null(repeat);
    layout.elements[0] = &target;
    layout.elements[1] = &label;
    layout.element_count = 2;
    (void)snprintf(field.target, sizeof(field.target), "target");
    (void)snprintf(field.preset, sizeof(field.preset), "living_field");
    (void)snprintf(field.trigger, sizeof(field.trigger), "while_visible");
    field.extent = UI_EXTENT_SURFACE;
    layout.elements[2] = &field;
    layout.element_count = 3;

    /* The surface as the outgoing menu leaves it: authored characters over the
       fabric its backdrop fills the surface with. */
    render_outgoing_surface(&layout, start, fg, bg);
    at_start = grid_filled_cells(start);
    assert_true(at_start > 0);

    /* The window opens on the surface as it stands: nothing has left yet, because
       the diamond each character retreats inside still covers its whole block, and
       the wave may not take a character over until the material is moving. The
       first frame of a settle is the surface exactly as it stood. */
    render_outgoing_surface(&layout, grid, fg, bg);
    assert_true(ui_animation_render_world_settle(&layout, grid, 0.0, false));
    assert_memory_equal(grid->cells, start->cells, 40U * 24U * sizeof(Cell));

    /* Half way through the window the surface is exactly its lattice: every
       lattice position still carries a character and nothing else does. */
    render_outgoing_surface(&layout, grid, fg, bg);
    assert_true(ui_animation_render_world_settle(&layout, grid, (double)exit_ms / 2.0,
                                                 false));
    half = grid_filled_cells(grid);
    assert_int_equal(half, settle_lattice_cells(40, 24));
    assert_true(grid_uses_fringe_colour(grid));
    /* The settle never invents a character: what a cell shows is either the
       surface's own character, still standing where it was, or one from the
       fabric's alphabet where the wave has taken it over. */
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 40; x++) {
            Cell drawn;
            if (!grid_get(grid, x, y, &drawn)) continue;
            if (drawn.glyph == 0U || drawn.glyph == ' ') continue;
            assert_true(drawn.glyph == start->cells[(size_t)y * 40U + (size_t)x].glyph ||
                        settle_ramp_glyph(drawn.glyph));
        }
    }

    /* The material thins in a stable order from there, so the surface carries
       strictly less of itself at each later sample of the window. */
    render_outgoing_surface(&layout, grid, fg, bg);
    assert_true(ui_animation_render_world_settle(&layout, grid, (double)exit_ms / 4.0,
                                                 false));
    quarter = grid_filled_cells(grid);
    render_outgoing_surface(&layout, grid, fg, bg);
    assert_true(ui_animation_render_world_settle(&layout, grid,
                                                 (double)exit_ms * 3.0 / 4.0, false));
    late = grid_filled_cells(grid);
    assert_true(quarter > half);
    assert_true(half > late);
    assert_true(late > 0);

    /* At the end of the window nothing is drawn at all, so the live frame
       underneath is simply uncovered: the surface is its own self again. */
    render_outgoing_surface(&layout, grid, fg, bg);
    assert_true(ui_animation_render_world_settle(&layout, grid, (double)exit_ms, false));
    assert_memory_equal(grid->cells, start->cells, 40U * 24U * sizeof(Cell));
    render_outgoing_surface(&layout, grid, fg, bg);
    assert_true(ui_animation_render_world_settle(&layout, grid, (double)exit_ms + 50.0,
                                                 false));
    assert_memory_equal(grid->cells, start->cells, 40U * 24U * sizeof(Cell));

    /* Deterministic: the same explicit time composes the same cells. */
    for (int step = 1; step < 4; step++) {
        double t = (double)exit_ms * (double)step / 4.0;
        render_outgoing_surface(&layout, grid, fg, bg);
        assert_true(ui_animation_render_world_settle(&layout, grid, t, false));
        render_outgoing_surface(&layout, repeat, fg, bg);
        assert_true(ui_animation_render_world_settle(&layout, repeat, t, false));
        assert_memory_equal(grid->cells, repeat->cells, 40U * 24U * sizeof(Cell));
    }

    /* Reduced motion draws no material at all, which is the recorded no-motion
       response: the world frame is simply there. */
    render_outgoing_surface(&layout, grid, fg, bg);
    assert_true(ui_animation_render_world_settle(&layout, grid,
                                                 (double)exit_ms / 2.0, true));
    assert_memory_equal(grid->cells, start->cells, 40U * 24U * sizeof(Cell));

    /* A negative time, no layout, or a layout with nothing to address is refused
       rather than composed. */
    assert_false(ui_animation_render_world_settle(NULL, grid, 0.0, false));
    assert_false(ui_animation_render_world_settle(&layout, grid, -1.0, false));
    {
        UiLayout empty = {0};
        assert_false(ui_animation_render_world_settle(&empty, grid, 0.0, false));
    }

    release(&label);
    release(&target);
    grid_destroy(repeat);
    grid_destroy(start);
    grid_destroy(grid);
}

/*
 * The backdrop's speed is a setting, not a constant. Retuning the ambient loop
 * changes how fast the material advances and nothing else: the same phase composes
 * the same surface at any period, and one whole loop lands back on it, because the
 * wave advances a whole number of wavelengths per loop. That is what makes the
 * speed safe to try at different values from configuration.
 */
static void test_backdrop_period_retunes_the_speed_of_the_field(void **state) {
    UiElement target = element("target", UI_ELE_CONTAINER, 0, 0, 24, 12);
    UiElement field = element("field", UI_ELE_ANIMATION, 0, 0, 24, 12);
    UiLayout layout = {0};
    Grid *grid = grid_create(24, 12);
    Grid *reference = grid_create(24, 12);
    SDL_Color bg = {5, 8, 10, 255};
    SDL_Color fg = {242, 247, 248, 255};
    const size_t cells = 24U * 12U;
    unsigned int base;
    (void)state;
    assert_non_null(grid);
    assert_non_null(reference);
    (void)snprintf(field.target, sizeof(field.target), "target");
    (void)snprintf(field.preset, sizeof(field.preset), "living_field");
    (void)snprintf(field.trigger, sizeof(field.trigger), "while_visible");
    field.extent = UI_EXTENT_SURFACE;
    layout.elements[0] = &target;
    layout.elements[1] = &field;
    layout.element_count = 2;
    ui_theme_set_ambient_period_ms(0U);
    base = ui_theme_ambient_period_ms();
    assert_int_equal(base, ui_theme_motion_duration_ms(UI_THEME_MOTION_AMBIENT, false));

    /* Phase zero at the theme's own period: the surface to compare against. */
    grid_clear(reference, bg);
    ui_layout_render(&layout, reference, fg, bg);
    assert_true(ui_animation_render_layout(&layout, reference, 0.0, false, false,
                                           UI_ANIMATION_EVENT_WHILE_VISIBLE));

    /* The same phase at twice the period is the same surface: only the rate
       differs, which is the whole of what the setting changes. */
    ui_theme_set_ambient_period_ms(base * 2U);
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, 0.0, false, false,
                                           UI_ANIMATION_EVENT_WHILE_VISIBLE));
    assert_memory_equal(grid->cells, reference->cells, cells * sizeof(Cell));

    /* Under the slower setting the same elapsed time is only part of the way
       through — the material is identical, it simply takes longer to cross. */
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, (double)base, false, false,
                                           UI_ANIMATION_EVENT_WHILE_VISIBLE));
    assert_true(memcmp(grid->cells, reference->cells, cells * sizeof(Cell)) != 0);
    /* A whole loop is a whole loop at any period: two of the slower ones land back
       on the surface the window started from. */
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, (double)base * 2.0, false, false,
                                           UI_ANIMATION_EVENT_WHILE_VISIBLE));
    assert_memory_equal(grid->cells, reference->cells, cells * sizeof(Cell));

    /* The same two properties at the theme's own period. */
    ui_theme_set_ambient_period_ms(0U);
    assert_int_equal(ui_theme_ambient_period_ms(), base);
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, (double)base / 2.0, false, false,
                                           UI_ANIMATION_EVENT_WHILE_VISIBLE));
    assert_true(memcmp(grid->cells, reference->cells, cells * sizeof(Cell)) != 0);
    grid_clear(grid, bg);
    ui_layout_render(&layout, grid, fg, bg);
    assert_true(ui_animation_render_layout(&layout, grid, (double)base, false, false,
                                           UI_ANIMATION_EVENT_WHILE_VISIBLE));
    assert_memory_equal(grid->cells, reference->cells, cells * sizeof(Cell));

    /* Restoring the theme's own value is what every other test relies on. */
    ui_theme_set_ambient_period_ms(0U);
    assert_int_equal(ui_theme_ambient_period_ms(), base);
    release(&field);
    release(&target);
    grid_destroy(reference);
    grid_destroy(grid);
}


/*
 * The application does not load the element catalogue the way the workbench does.
 * It preloads a fixed list per layout from `assets/ui_layouts/master_map.txt`
 * (`cache_next=`) and then resolves each layout's `elements=` line against that
 * cache; a name the cache does not hold becomes a NULL slot without a word
 * (`ui_layout_load`, `src/ui_ele.c`). An authored element can therefore silently
 * stop existing — which is exactly what happened to the menu transitions: the
 * tide cover and reveal were authored into the four menus but never added to the
 * preload lists, so no application run ever played a transition, while every test
 * passed because the workbench loads the whole catalogue.
 *
 * This is the guard for that: resolve each menu the way `src/app.c` does and
 * require that the resolved surface can cover itself on the way out and reveal
 * itself on the way in.
 */
static void test_application_menu_layouts_play_their_authored_transition(void **state) {
    static const char *const menus[] = {"main_menu", "pause_menu", "settings",
                                        "confirm_quit"};
    unsigned int exit_ms = ui_theme_motion_duration_ms(UI_THEME_MOTION_MAJOR_EXIT, false);
    unsigned int enter_ms = ui_theme_motion_duration_ms(UI_THEME_MOTION_MAJOR_ENTER, false);
    Grid *grid = grid_create(260, 160);
    Grid *authored = grid_create(260, 160);
    SDL_Color bg = {5, 8, 10, 255};
    SDL_Color fg = {242, 247, 248, 255};
    const size_t cells = 260U * 160U;
    (void)state;
    assert_non_null(grid);
    assert_non_null(authored);
    assert_true(exit_ms > 0U);
    assert_true(enter_ms > 0U);
    for (size_t menu = 0; menu < sizeof(menus) / sizeof(menus[0]); menu++) {
        char path[64];
        UiCache cache;
        UiLayout *layout;
        int changed;
        ui_cache_init(&cache, "assets/ui_layouts/master_map.txt");
        ui_cache_tick(&cache, menus[menu], "assets/ui_elements");
        assert_true(snprintf(path, sizeof(path), "assets/ui_layouts/%s.txt",
                             menus[menu]) < (int)sizeof(path));
        layout = ui_layout_load(path, &cache);
        assert_non_null(layout);
        assert_true(layout->element_count > 0);
        /* Every authored element resolved: no silent hole in the layout. */
        for (int e = 0; e < layout->element_count; e++) {
            if (!layout->elements[e])
                fprintf(stderr, "FAIL-PRODUCT: %s: element %d is unresolved\n",
                        menus[menu], e);
            assert_non_null(layout->elements[e]);
        }
        /* Children resolve through the same cache, so walk them as well — up the
           parent chain, because a layout's containers are reached as parents
           rather than listed as elements. A missing child is reported by the
           loader but fails nothing, so only a test keeps it honest. */
        for (int e = 0; e < layout->element_count; e++) {
            for (UiElement *level = layout->elements[e]; level; level = level->parent) {
                for (int c = 0; c < level->child_count; c++) {
                    if (!level->children[c])
                        fprintf(stderr,
                                "FAIL-PRODUCT: %s: child %d of '%s' is unresolved\n",
                                menus[menu], c, level->name);
                    assert_non_null(level->children[c]);
                }
            }
        }
        /* The surface as authored, to compare the transition against. */
        grid_clear(grid, bg);
        ui_layout_render(layout, grid, fg, bg);
        memcpy(authored->cells, grid->cells, cells * sizeof(Cell));
        /* On the way out the material covers the surface... */
        assert_true(ui_animation_render_layout(layout, grid, (double)exit_ms - 1.0,
                                               false, false,
                                               UI_ANIMATION_EVENT_CONTEXT_EXIT));
        changed = 0;
        for (size_t i = 0; i < cells; i++)
            if (grid->cells[i].glyph != authored->cells[i].glyph) changed++;
        if (changed <= (int)(cells / 4U))
            fprintf(stderr, "FAIL-PRODUCT: %s: exit covered %d of %zu cells\n",
                    menus[menu], changed, cells);
        assert_true(changed > (int)(cells / 4U));
        /* ... and on the way in it is fully covered before the reveal drains. */
        grid_clear(grid, bg);
        ui_layout_render(layout, grid, fg, bg);
        assert_true(ui_animation_render_layout(layout, grid, 0.0, false, false,
                                               UI_ANIMATION_EVENT_CONTEXT_ENTER));
        changed = 0;
        for (size_t i = 0; i < cells; i++)
            if (grid->cells[i].glyph != authored->cells[i].glyph) changed++;
        if (changed <= (int)(cells / 4U))
            fprintf(stderr, "FAIL-PRODUCT: %s: reveal covered %d of %zu cells\n",
                    menus[menu], changed, cells);
        assert_true(changed > (int)(cells / 4U));
        ui_layout_destroy(layout);
        ui_cache_destroy(&cache);
    }
    grid_destroy(authored);
    grid_destroy(grid);
}


static bool painted_outside(Grid *grid, int x0, int y0, int x1, int y1) {
    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            Cell value;
            if (!grid_get(grid, x, y, &value)) continue;
            if (value.glyph == 0U || value.glyph == ' ') continue;
            if (x < x0 || x >= x1 || y < y0 || y >= y1) return true;
        }
    }
    return false;
}

static void test_surface_extent_fills_the_frame_unless_bounded(void **state) {
    static const double elapsed_ms[] = {0.0, 300.0, 600.0, 900.0, 1200.0, 1500.0};
    UiElement target = element("target", UI_ELE_CONTAINER, 120, 71, 24, 16);
    UiElement unit = element("unit", UI_ELE_ANIMATION, 0, 0, 0, 0);
    UiLayout layout = {0};
    Grid *grid = grid_create(260, 160);
    bool escaped = false;
    (void)state;
    assert_non_null(grid);
    (void)snprintf(unit.preset, sizeof(unit.preset), "%s", "living_field");
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "%s", "while_visible");
    (void)snprintf(unit.orientation, sizeof(unit.orientation), "%s", "horizontal");
    (void)snprintf(unit.target, sizeof(unit.target), "%s", "target");
    layout.elements[0] = &target;
    layout.elements[1] = &unit;
    layout.element_count = 2;

    /* Bounded (the default): the region follows the target, so nothing escapes. */
    assert_int_equal(unit.extent, UI_EXTENT_BOX);
    for (size_t i = 0U; i < sizeof(elapsed_ms) / sizeof(elapsed_ms[0]); i++) {
        assert_true(grid_clear_region_zero(grid, 0, 0, 260, 160));
        assert_true(ui_animation_render_layout(&layout, grid, elapsed_ms[i], false, false,
                                              UI_ANIMATION_EVENT_WHILE_VISIBLE));
        assert_false(painted_outside(grid, 120, 71, 144, 87));
    }

    /* Surface: the same unit fills the frame it renders into. */
    unit.extent = UI_EXTENT_SURFACE;
    for (size_t i = 0U; i < sizeof(elapsed_ms) / sizeof(elapsed_ms[0]); i++) {
        assert_true(grid_clear_region_zero(grid, 0, 0, 260, 160));
        assert_true(ui_animation_render_layout(&layout, grid, elapsed_ms[i], false, false,
                                              UI_ANIMATION_EVENT_WHILE_VISIBLE));
        if (painted_outside(grid, 120, 71, 144, 87)) escaped = true;
    }
    assert_true(escaped);

    /* Reduced motion draws no field at any extent. */
    assert_true(grid_clear_region_zero(grid, 0, 0, 260, 160));
    assert_true(ui_animation_render_layout(&layout, grid, 600.0, true, false,
                                          UI_ANIMATION_EVENT_WHILE_VISIBLE));
    assert_false(painted_outside(grid, 0, 0, 260, 160));

    release(&unit);
    release(&target);
    grid_destroy(grid);
}


int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_pause_glitch_canvas_matches_layout_unit),
        cmocka_unit_test(test_button_effect_presets_are_deterministic_and_bounded),
        cmocka_unit_test(test_context_transition_presets_are_deterministic_and_class_bounded),
        cmocka_unit_test(test_generated_context_transitions_skip_incompatible_elements),
        cmocka_unit_test(test_directed_trigger_endpoints),
        cmocka_unit_test(test_lifecycle_protects_authored_spaces_not_backdrop),
        cmocka_unit_test(test_center_out_and_reduced_motion),
        cmocka_unit_test(test_focus_trigger_and_ordinary_transition),
        cmocka_unit_test(test_trigger_filter_and_random_bounds),
        cmocka_unit_test(test_living_field_is_deterministic_bounded_and_reduced),
        cmocka_unit_test(test_tide_covers_the_surface_then_restores_it),
        cmocka_unit_test(test_settle_condenses_to_the_lattice_then_resolves),
        cmocka_unit_test(test_backdrop_period_retunes_the_speed_of_the_field),
        cmocka_unit_test(test_tide_reads_as_a_tide_not_a_wipe),
        cmocka_unit_test(test_application_menu_layouts_play_their_authored_transition),
        cmocka_unit_test(test_surface_extent_fills_the_frame_unless_bounded)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
