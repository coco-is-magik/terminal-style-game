#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>

#include <math.h>
#include <string.h>

#include "../src/ui_animation_playback.h"

static UiDocumentAnimation animation(UiDocumentAnimationTrigger trigger,
                                     bool loop) {
    UiDocumentAnimation value = {0};
    value.preset = UI_DOCUMENT_ANIMATION_PRESET_CENTER_OUT;
    value.target_id = 7U;
    value.trigger = trigger;
    value.orientation = UI_DOCUMENT_ANIMATION_ORIENTATION_RADIAL;
    value.loop = loop;
    return value;
}

static void test_context_entry_endpoint_and_replay(void **state) {
    UiAnimationPlayback playback;
    UiDocumentAnimation value = animation(
        UI_DOCUMENT_ANIMATION_TRIGGER_CONTEXT_ENTER, false);
    UiAnimationPlaybackSample sample;
    (void)state;
    assert_true(ui_animation_playback_init(&playback, 10.0));
    assert_true(ui_animation_playback_event(&playback,
        UI_ANIMATION_EVENT_CONTEXT_ENTER, 0U, 20.0));
    assert_true(ui_animation_playback_sample(&playback, &value, true,
                                             20.0, false, &sample));
    assert_true(sample.visible);
    assert_float_equal(sample.progress, 0.0, 0.000001);
    assert_true(ui_animation_playback_sample(&playback, &value, true,
                                             180.0, false, &sample));
    assert_false(sample.visible);
    assert_true(sample.complete);
    assert_float_equal(sample.progress, 1.0, 0.000001);
    assert_true(ui_animation_playback_event(&playback,
        UI_ANIMATION_EVENT_CONTEXT_ENTER, 0U, 200.0));
    assert_true(ui_animation_playback_sample(&playback, &value, true,
                                             200.0, false, &sample));
    assert_true(sample.visible);
    assert_float_equal(sample.elapsed_ms, 0.0, 0.000001);
}

static void test_exit_loop_focus_and_visibility_filtering(void **state) {
    UiAnimationPlayback playback;
    UiDocumentAnimation exit_value = animation(
        UI_DOCUMENT_ANIMATION_TRIGGER_CONTEXT_EXIT, false);
    UiDocumentAnimation focus_value = animation(
        UI_DOCUMENT_ANIMATION_TRIGGER_FOCUS, true);
    UiDocumentAnimation visible_value = animation(
        UI_DOCUMENT_ANIMATION_TRIGGER_WHILE_VISIBLE, false);
    UiAnimationPlaybackSample sample;
    (void)state;
    assert_true(ui_animation_playback_init(&playback, 0.0));
    assert_true(ui_animation_playback_event(&playback,
        UI_ANIMATION_EVENT_CONTEXT_EXIT, 0U, 10.0));
    assert_true(ui_animation_playback_sample(&playback, &exit_value, true,
                                             10.0, false, &sample));
    assert_true(sample.visible);
    assert_float_equal(sample.progress, 1.0, 0.000001);
    assert_true(ui_animation_playback_event(&playback,
        UI_ANIMATION_EVENT_FOCUS, 7U, 20.0));
    assert_true(ui_animation_playback_sample(&playback, &focus_value, true,
                                             180.0, false, &sample));
    assert_true(sample.visible);
    assert_float_equal(sample.elapsed_ms, 0.0, 0.000001);
    assert_true(ui_animation_playback_sample(&playback, &visible_value, false,
                                             180.0, false, &sample));
    assert_false(sample.visible);
    assert_true(ui_animation_playback_sample(&playback, &visible_value, true,
                                             180.0, false, &sample));
    assert_true(sample.visible);
}

static void test_reduced_motion_and_invalid_input_are_transactional(void **state) {
    UiAnimationPlayback playback;
    UiAnimationPlayback before;
    UiDocumentAnimation value = animation(
        UI_DOCUMENT_ANIMATION_TRIGGER_ACTIVATE, false);
    UiAnimationPlaybackSample sample = {true, false, 9.0, 0.5};
    UiAnimationPlaybackSample before_sample;
    (void)state;
    assert_true(ui_animation_playback_init(&playback, 5.0));
    assert_true(ui_animation_playback_event(&playback,
        UI_ANIMATION_EVENT_ACTIVATE, 7U, 10.0));
    assert_true(ui_animation_playback_sample(&playback, &value, true,
                                             10.0, true, &sample));
    assert_false(sample.visible);
    assert_true(sample.complete);
    before = playback;
    assert_false(ui_animation_playback_event(&playback,
        UI_ANIMATION_EVENT_ACTIVATE, 7U, 9.0));
    assert_memory_equal(&playback, &before, sizeof(playback));
    assert_false(ui_animation_playback_event(&playback,
        UI_ANIMATION_EVENT_CONTEXT_ENTER, 7U, 11.0));
    assert_memory_equal(&playback, &before, sizeof(playback));
    before_sample = sample;
    assert_false(ui_animation_playback_sample(&playback, &value, true,
                                              NAN, false, &sample));
    assert_memory_equal(&sample, &before_sample, sizeof(sample));
}

static void test_context_exit_interrupts_context_enter(void **state) {
    UiAnimationPlayback playback;
    UiDocumentAnimation entering = animation(
        UI_DOCUMENT_ANIMATION_TRIGGER_CONTEXT_ENTER, false);
    UiDocumentAnimation exiting = animation(
        UI_DOCUMENT_ANIMATION_TRIGGER_CONTEXT_EXIT, false);
    UiAnimationPlaybackSample sample;
    (void)state;
    assert_true(ui_animation_playback_init(&playback, 0.0));
    assert_true(ui_animation_playback_event(&playback,
        UI_ANIMATION_EVENT_CONTEXT_ENTER, 0U, 10.0));
    assert_true(ui_animation_playback_sample(&playback, &entering, true,
                                             50.0, false, &sample));
    assert_true(sample.visible);
    assert_true(ui_animation_playback_event(&playback,
        UI_ANIMATION_EVENT_CONTEXT_EXIT, 0U, 50.0));
    assert_true(ui_animation_playback_sample(&playback, &entering, true,
                                             50.0, false, &sample));
    assert_false(sample.visible);
    assert_true(ui_animation_playback_sample(&playback, &exiting, true,
                                             50.0, false, &sample));
    assert_true(sample.visible);
    assert_float_equal(sample.progress, 1.0, 0.000001);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_context_entry_endpoint_and_replay),
        cmocka_unit_test(test_exit_loop_focus_and_visibility_filtering),
        cmocka_unit_test(test_reduced_motion_and_invalid_input_are_transactional),
        cmocka_unit_test(test_context_exit_interrupts_context_enter)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}