#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <cmocka.h>

#include "../src/input.h"

static void test_frame_reset_preserves_held_and_quit(void **state) {
    (void)state;
    InputState input;
    memset(&input, 0, sizeof(input));
    input.quit = true;
    input.forward = true;
    input.mouse_left = true;
    input.up = true;
    input.mouse_dx = 9.0f;
    strcpy(input.text_input, "old");
    input.text_input_len = 3;
    input_begin_frame(&input);
    assert_true(input.quit);
    assert_true(input.forward);
    assert_true(input.mouse_left);
    assert_false(input.up);
    assert_float_equal(input.mouse_dx, 0.0f, 0.001f);
    assert_int_equal(input.text_input_len, 0);
}

static void test_quit_and_headless_filter(void **state) {
    (void)state;
    InputState input = {0};
    InputEvent key = {INPUT_EVENT_KEY_DOWN, INPUT_KEY_TAB, false, false, 0, 0, 0, NULL};
    InputEvent quit = {INPUT_EVENT_QUIT, INPUT_KEY_NONE, false, false, 0, 0, 0, NULL};
    input_apply_event(&input, &key, true);
    assert_false(input.tab);
    input_apply_event(&input, &quit, true);
    assert_true(input.quit);
}

static void test_key_and_editor_shortcuts(void **state) {
    (void)state;
    InputState input = {0};
    InputEvent event = {INPUT_EVENT_KEY_DOWN, INPUT_KEY_UP, false, false, 0, 0, 0, NULL};
    input_apply_event(&input, &event, false);
    assert_true(input.up);
    assert_true(input.editor_previous_pressed);
    event.key = INPUT_KEY_Z;
    event.ctrl = true;
    input_apply_event(&input, &event, false);
    assert_true(input.editor_undo_pressed);
    event.key = INPUT_KEY_TAB;
    event.ctrl = false;
    input_apply_event(&input, &event, false);
    assert_true(input.tab);
    assert_true(input.editor_toggle_mode_pressed);
    event.key = INPUT_KEY_RETURN;
    input_apply_event(&input, &event, false);
    assert_true(input.confirm);
    assert_true(input.editor_confirm_pressed);
    event.key = INPUT_KEY_O;
    event.ctrl = true;
    input_apply_event(&input, &event, false);
    assert_true(input.editor_open_pressed);
    input_begin_frame(&input);
    assert_false(input.editor_open_pressed);
    event.key = INPUT_KEY_UP;
    event.repeat = true;
    input_apply_event(&input, &event, false);
    assert_false(input.up);
}

static void test_editor_open_requires_ctrl_and_nonrepeat(void **state) {
    InputState input = {0};
    InputEvent event = {
        INPUT_EVENT_KEY_DOWN, INPUT_KEY_O, false, false, 0, 0, 0, NULL
    };
    (void)state;

    input_apply_event(&input, &event, false);
    assert_false(input.editor_open_pressed);
    event.ctrl = true;
    event.repeat = true;
    input_apply_event(&input, &event, false);
    assert_false(input.editor_open_pressed);
    event.repeat = false;
    input_apply_event(&input, &event, false);
    assert_true(input.editor_open_pressed);
}

static void test_text_mouse_buttons_and_wheel(void **state) {
    (void)state;
    InputState input = {0};
    InputEvent event = {INPUT_EVENT_TEXT, INPUT_KEY_NONE, false, false, 0, 0, 0, "abc"};
    input_apply_event(&input, &event, false);
    assert_string_equal(input.text_input, "abc");
    event.type = INPUT_EVENT_MOUSE_MOTION; event.x = 2.5f; event.y = -1.0f;
    input_apply_event(&input, &event, false);
    input_apply_event(&input, &event, false);
    assert_float_equal(input.mouse_dx, 5.0f, 0.001f);
    event.type = INPUT_EVENT_MOUSE_BUTTON_DOWN; event.button = 1;
    input_apply_event(&input, &event, false);
    assert_true(input.mouse_left);
    event.type = INPUT_EVENT_MOUSE_BUTTON_UP;
    input_apply_event(&input, &event, false);
    assert_false(input.mouse_left);
    event.type = INPUT_EVENT_MOUSE_WHEEL; event.x = 1.0f; event.y = -2.0f;
    input_apply_event(&input, &event, false);
    assert_float_equal(input.mouse_wheel_y, -2.0f, 0.001f);
}

static void test_ui_scale_shortcuts_are_global_nonrepeat_edges(void **state) {
    InputState input = {0};
    InputEvent event = {
        INPUT_EVENT_KEY_DOWN, INPUT_KEY_EQUALS, false, true, 0, 0, 0, NULL
    };
    (void)state;
    input_apply_event(&input, &event, false);
    assert_true(input.ui_scale_increase_pressed);
    assert_false(input.confirm);

    event.key = INPUT_KEY_MINUS;
    input_apply_event(&input, &event, false);
    assert_true(input.ui_scale_decrease_pressed);
    event.key = INPUT_KEY_ZERO;
    input_apply_event(&input, &event, false);
    assert_true(input.ui_scale_reset_pressed);

    input_begin_frame(&input);
    assert_false(input.ui_scale_increase_pressed);
    assert_false(input.ui_scale_decrease_pressed);
    assert_false(input.ui_scale_reset_pressed);
    event.key = INPUT_KEY_EQUALS;
    event.repeat = true;
    input_apply_event(&input, &event, false);
    assert_false(input.ui_scale_increase_pressed);
    event.repeat = false;
    input_apply_event(&input, &event, true);
    assert_false(input.ui_scale_increase_pressed);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_frame_reset_preserves_held_and_quit),
        cmocka_unit_test(test_quit_and_headless_filter),
        cmocka_unit_test(test_key_and_editor_shortcuts),
        cmocka_unit_test(test_editor_open_requires_ctrl_and_nonrepeat),
        cmocka_unit_test(test_text_mouse_buttons_and_wheel),
        cmocka_unit_test(test_ui_scale_shortcuts_are_global_nonrepeat_edges),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}