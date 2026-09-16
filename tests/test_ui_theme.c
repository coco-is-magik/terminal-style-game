#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>

#include <cmocka.h>

#include <float.h>
#include <limits.h>
#include <math.h>

#include "../src/ui_theme.h"

static void assert_color(UiThemeColor color, uint8_t red, uint8_t green,
                         uint8_t blue, uint8_t alpha) {
    assert_int_equal(color.red, red);
    assert_int_equal(color.green, green);
    assert_int_equal(color.blue, blue);
    assert_int_equal(color.alpha, alpha);
}

static void test_provisional_tokens_are_exact_and_immutable(void **state) {
    const UiThemeTokens *tokens = ui_theme_provisional_tokens();
    (void)state;
    assert_non_null(tokens);
    assert_ptr_equal(tokens, ui_theme_provisional_tokens());
    assert_color(tokens->palette.canvas, 0x05U, 0x08U, 0x0aU, 0xffU);
    assert_color(tokens->palette.panel, 0x0dU, 0x14U, 0x18U, 0xffU);
    assert_color(tokens->palette.elevated, 0x16U, 0x21U, 0x26U, 0xffU);
    assert_color(tokens->palette.text_primary, 0xf2U, 0xf7U, 0xf8U, 0xffU);
    assert_color(tokens->palette.text_secondary, 0xa8U, 0xb4U, 0xb8U, 0xffU);
    assert_color(tokens->palette.border, 0x64U, 0x75U, 0x7cU, 0xffU);
    assert_color(tokens->palette.accent, 0x67U, 0xf5U, 0xc2U, 0xffU);
    assert_color(tokens->palette.focus, 0xa8U, 0xffU, 0xe1U, 0xffU);
    assert_color(tokens->palette.selection_background, 0x12U, 0x3dU, 0x32U, 0xffU);
    assert_color(tokens->palette.disabled_text, 0x8dU, 0x99U, 0x9dU, 0xffU);
    assert_color(tokens->palette.disabled_background, 0x16U, 0x1dU, 0x20U, 0xffU);
    assert_color(tokens->palette.warning, 0xffU, 0xd1U, 0x66U, 0xffU);
    assert_color(tokens->palette.error, 0xffU, 0x6bU, 0x7aU, 0xffU);
    assert_color(tokens->palette.success, 0x71U, 0xf7U, 0x9fU, 0xffU);
    assert_color(tokens->palette.destructive, 0xffU, 0x88U, 0x94U, 0xffU);
    assert_int_equal(tokens->geometry.base_space_cells, 1);
    assert_int_equal(tokens->geometry.ordinary_target_cells, 3);
    assert_int_equal(tokens->geometry.horizontal_label_padding_cells, 2);
    assert_int_equal(tokens->geometry.panel_inset_cells, 2);
    assert_int_equal(tokens->geometry.related_item_gap_cells, 1);
    assert_int_equal(tokens->geometry.group_gap_cells, 2);
    assert_int_equal(tokens->geometry.major_section_gap_cells, 3);
    assert_int_equal(tokens->geometry.border_cells, 1);
    assert_int_equal(tokens->geometry.editor_baseline_columns, 260);
    assert_int_equal(tokens->geometry.editor_baseline_rows, 160);
    assert_int_equal(tokens->geometry.authored_minimum_columns, 40);
    assert_int_equal(tokens->geometry.authored_minimum_rows, 15);
    assert_int_equal(tokens->geometry.ordinary_major_context_limit, 2);
}

static void test_contrast_ratios_and_thresholds(void **state) {
    const UiThemePalette *palette = &ui_theme_provisional_tokens()->palette;
    double ratio = 0.0;
    (void)state;
    assert_true(ui_theme_contrast_ratio(
        palette->text_primary, palette->canvas, &ratio));
    assert_float_equal(ratio, 18.588, 0.001);
    assert_true(ui_theme_pair_meets(
        palette->text_secondary, palette->panel, UI_THEME_TEXT_MIN_CONTRAST));
    assert_true(ui_theme_pair_meets(
        palette->border, palette->panel, UI_THEME_NON_TEXT_MIN_CONTRAST));
    assert_true(ui_theme_pair_meets(
        palette->focus, palette->panel, UI_THEME_NON_TEXT_MIN_CONTRAST));
    assert_true(ui_theme_pair_meets(
        palette->accent, palette->canvas, UI_THEME_NON_TEXT_MIN_CONTRAST));
    assert_true(ui_theme_pair_meets(
        palette->text_primary, palette->selection_background,
        UI_THEME_TEXT_MIN_CONTRAST));
    assert_true(ui_theme_pair_meets(
        palette->disabled_text, palette->disabled_background,
        UI_THEME_TEXT_MIN_CONTRAST));
    assert_true(ui_theme_pair_meets(
        palette->warning, palette->canvas, UI_THEME_TEXT_MIN_CONTRAST));
    assert_true(ui_theme_pair_meets(
        palette->error, palette->canvas, UI_THEME_TEXT_MIN_CONTRAST));
    assert_true(ui_theme_pair_meets(
        palette->success, palette->canvas, UI_THEME_TEXT_MIN_CONTRAST));
    assert_true(ui_theme_pair_meets(
        palette->destructive, palette->canvas, UI_THEME_TEXT_MIN_CONTRAST));
    assert_false(ui_theme_pair_meets(
        (UiThemeColor){119U, 119U, 119U, 255U},
        (UiThemeColor){255U, 255U, 255U, 255U}, 4.5));
    assert_true(ui_theme_pair_meets(
        (UiThemeColor){118U, 118U, 118U, 255U},
        (UiThemeColor){255U, 255U, 255U, 255U}, 4.5));
}

static void test_demo_display_pairs_meet_required_contrast(void **state) {
    const UiThemePalette *p = &ui_theme_provisional_tokens()->palette;
    struct Pair {
        UiThemeColor foreground;
        UiThemeColor background;
        double minimum;
    } pairs[] = {
        {p->text_primary, p->canvas, UI_THEME_TEXT_MIN_CONTRAST},
        {p->text_primary, p->panel, UI_THEME_TEXT_MIN_CONTRAST},
        {p->text_primary, p->elevated, UI_THEME_TEXT_MIN_CONTRAST},
        {p->text_primary, p->selection_background, UI_THEME_TEXT_MIN_CONTRAST},
        {p->text_secondary, p->canvas, UI_THEME_TEXT_MIN_CONTRAST},
        {p->text_secondary, p->panel, UI_THEME_TEXT_MIN_CONTRAST},
        {p->text_secondary, p->elevated, UI_THEME_TEXT_MIN_CONTRAST},
        {p->disabled_text, p->disabled_background, UI_THEME_TEXT_MIN_CONTRAST},
        {p->border, p->canvas, UI_THEME_NON_TEXT_MIN_CONTRAST},
        {p->border, p->panel, UI_THEME_NON_TEXT_MIN_CONTRAST},
        {p->focus, p->canvas, UI_THEME_NON_TEXT_MIN_CONTRAST},
        {p->focus, p->panel, UI_THEME_NON_TEXT_MIN_CONTRAST},
        {p->focus, p->elevated, UI_THEME_NON_TEXT_MIN_CONTRAST},
        {p->focus, p->selection_background, UI_THEME_NON_TEXT_MIN_CONTRAST},
        {p->accent, p->canvas, UI_THEME_NON_TEXT_MIN_CONTRAST},
        {p->accent, p->panel, UI_THEME_NON_TEXT_MIN_CONTRAST},
        {p->canvas, p->accent, UI_THEME_TEXT_MIN_CONTRAST},
        {p->warning, p->canvas, UI_THEME_TEXT_MIN_CONTRAST},
        {p->warning, p->panel, UI_THEME_TEXT_MIN_CONTRAST},
        {p->error, p->canvas, UI_THEME_TEXT_MIN_CONTRAST},
        {p->error, p->panel, UI_THEME_TEXT_MIN_CONTRAST},
        {p->success, p->canvas, UI_THEME_TEXT_MIN_CONTRAST},
        {p->success, p->panel, UI_THEME_TEXT_MIN_CONTRAST},
        {p->destructive, p->canvas, UI_THEME_TEXT_MIN_CONTRAST},
        {p->destructive, p->panel, UI_THEME_TEXT_MIN_CONTRAST}
    };
    size_t i;
    (void)state;
    for (i = 0U; i < sizeof(pairs) / sizeof(pairs[0]); i++)
        assert_true(ui_theme_pair_meets(
            pairs[i].foreground, pairs[i].background, pairs[i].minimum));
}

static void test_alpha_composition_and_invalid_outputs_are_transactional(void **state) {
    UiThemeColor output = {9U, 8U, 7U, 6U};
    UiThemeColor sentinel = output;
    double ratio = 7.0;
    (void)state;
    assert_true(ui_theme_composite_over(
        (UiThemeColor){255U, 255U, 255U, 128U},
        (UiThemeColor){0U, 0U, 0U, 255U}, &output));
    assert_color(output, 128U, 128U, 128U, 255U);
    output = sentinel;
    assert_false(ui_theme_composite_over(
        (UiThemeColor){255U, 255U, 255U, 128U},
        (UiThemeColor){0U, 0U, 0U, 128U}, &output));
    assert_memory_equal(&output, &sentinel, sizeof(output));
    assert_false(ui_theme_composite_over(
        (UiThemeColor){0U}, (UiThemeColor){0U, 0U, 0U, 255U}, NULL));
    assert_false(ui_theme_contrast_ratio(
        (UiThemeColor){255U, 255U, 255U, 128U},
        (UiThemeColor){0U, 0U, 0U, 255U}, &ratio));
    assert_float_equal(ratio, 7.0, 0.000001);
    assert_false(ui_theme_contrast_ratio(
        (UiThemeColor){255U, 255U, 255U, 255U},
        (UiThemeColor){0U, 0U, 0U, 255U}, NULL));
    assert_false(ui_theme_pair_meets(
        (UiThemeColor){255U, 255U, 255U, 255U},
        (UiThemeColor){0U, 0U, 0U, 255U}, NAN));
}

static void test_state_precedence(void **state) {
    uint32_t all = UI_THEME_STATE_FLAG_HOVER | UI_THEME_STATE_FLAG_SELECTED |
        UI_THEME_STATE_FLAG_FOCUSED | UI_THEME_STATE_FLAG_PRESSED |
        UI_THEME_STATE_FLAG_URGENT_ERROR | UI_THEME_STATE_FLAG_DISABLED;
    (void)state;
    assert_int_equal(ui_theme_resolve_state(0U), UI_THEME_STATE_NORMAL);
    assert_int_equal(ui_theme_resolve_state(UI_THEME_STATE_FLAG_HOVER),
                     UI_THEME_STATE_HOVER);
    assert_int_equal(ui_theme_resolve_state(
        UI_THEME_STATE_FLAG_HOVER | UI_THEME_STATE_FLAG_SELECTED),
        UI_THEME_STATE_SELECTED);
    assert_int_equal(ui_theme_resolve_state(
        UI_THEME_STATE_FLAG_SELECTED | UI_THEME_STATE_FLAG_FOCUSED),
        UI_THEME_STATE_FOCUSED);
    assert_int_equal(ui_theme_resolve_state(
        UI_THEME_STATE_FLAG_FOCUSED | UI_THEME_STATE_FLAG_PRESSED),
        UI_THEME_STATE_PRESSED);
    assert_int_equal(ui_theme_resolve_state(
        UI_THEME_STATE_FLAG_PRESSED | UI_THEME_STATE_FLAG_URGENT_ERROR),
        UI_THEME_STATE_URGENT_ERROR);
    assert_int_equal(ui_theme_resolve_state(all), UI_THEME_STATE_DISABLED);
}

static void test_scaled_edges_are_deterministic_and_transactional(void **state) {
    int output = 77;
    (void)state;
    assert_true(ui_theme_scaled_edge(8, 100, &output));
    assert_int_equal(output, 8);
    assert_true(ui_theme_scaled_edge(8, 125, &output));
    assert_int_equal(output, 10);
    assert_true(ui_theme_scaled_edge(9, 150, &output));
    assert_int_equal(output, 13);
    assert_true(ui_theme_scaled_edge(8, 200, &output));
    assert_int_equal(output, 16);
    output = 77;
    assert_false(ui_theme_scaled_edge(-1, 100, &output));
    assert_int_equal(output, 77);
    assert_false(ui_theme_scaled_edge(8, 0, &output));
    assert_int_equal(output, 77);
    assert_false(ui_theme_scaled_edge(INT_MAX, INT_MAX, &output));
    assert_int_equal(output, 77);
    assert_false(ui_theme_scaled_edge(8, 100, NULL));
}

static void test_motion_roles_easing_and_reduced_motion(void **state) {
    static const unsigned int expected_durations[] = {0U, 80U, 160U, 120U, 120U};
    const UiThemeTokens *tokens = ui_theme_provisional_tokens();
    double output;
    (void)state;
    assert_int_equal(ui_theme_motion_duration_ms(UI_THEME_MOTION_IMMEDIATE, false), 0U);
    assert_int_equal(ui_theme_motion_duration_ms(UI_THEME_MOTION_FEEDBACK, false), 80U);
    assert_int_equal(ui_theme_motion_duration_ms(UI_THEME_MOTION_MAJOR_ENTER, false), 160U);
    assert_int_equal(ui_theme_motion_duration_ms(UI_THEME_MOTION_MAJOR_EXIT, false), 120U);
    assert_int_equal(ui_theme_motion_duration_ms(UI_THEME_MOTION_RELATIONSHIP, false), 120U);
    assert_memory_equal(tokens->motion_duration_ms, expected_durations,
                        sizeof(tokens->motion_duration_ms));
    assert_true(ui_theme_motion_progress(
        UI_THEME_MOTION_MAJOR_ENTER, 0.0, false, &output));
    assert_float_equal(output, 0.0, 0.000001);
    assert_true(ui_theme_motion_progress(
        UI_THEME_MOTION_MAJOR_ENTER, 80.0, false, &output));
    assert_float_equal(output, 0.875, 0.000001);
    assert_true(ui_theme_motion_progress(
        UI_THEME_MOTION_MAJOR_ENTER, 160.0, false, &output));
    assert_float_equal(output, 1.0, 0.000001);
    assert_true(ui_theme_motion_progress(
        UI_THEME_MOTION_MAJOR_ENTER, 1000.0, false, &output));
    assert_float_equal(output, 1.0, 0.000001);
    assert_true(ui_theme_motion_progress(
        UI_THEME_MOTION_MAJOR_ENTER, 1.0, true, &output));
    assert_float_equal(output, 1.0, 0.000001);
    assert_true(ui_theme_motion_progress(
        UI_THEME_MOTION_IMMEDIATE, 0.0, false, &output));
    assert_float_equal(output, 1.0, 0.000001);
    output = 7.0;
    assert_false(ui_theme_motion_progress(
        UI_THEME_MOTION_MAJOR_ENTER, -1.0, false, &output));
    assert_float_equal(output, 7.0, 0.000001);
    assert_false(ui_theme_motion_progress(
        UI_THEME_MOTION_MAJOR_ENTER, INFINITY, false, &output));
    assert_float_equal(output, 7.0, 0.000001);
    assert_false(ui_theme_motion_progress(
        (UiThemeMotionRole)UI_THEME_MOTION_ROLE_COUNT, 1.0, false, &output));
    assert_false(ui_theme_motion_progress(
        UI_THEME_MOTION_MAJOR_ENTER, 1.0, false, NULL));
    assert_int_equal(ui_theme_motion_duration_ms(
        (UiThemeMotionRole)UI_THEME_MOTION_ROLE_COUNT, false), 0U);
}

static void test_motion_interruption_starts_from_current_value(void **state) {
    double output = -1.0;
    (void)state;
    assert_true(ui_theme_motion_value(
        UI_THEME_MOTION_MAJOR_EXIT, 0.0, false, 0.625, 0.0, &output));
    assert_float_equal(output, 0.625, 0.000001);
    assert_true(ui_theme_motion_value(
        UI_THEME_MOTION_MAJOR_EXIT, 60.0, false, 0.625, 0.0, &output));
    assert_float_equal(output, 0.078125, 0.000001);
    assert_true(ui_theme_motion_value(
        UI_THEME_MOTION_MAJOR_ENTER, 1.0, true, 0.625, 1.0, &output));
    assert_float_equal(output, 1.0, 0.000001);
    output = -1.0;
    assert_false(ui_theme_motion_value(
        UI_THEME_MOTION_MAJOR_ENTER, 1.0, false, NAN, 1.0, &output));
    assert_float_equal(output, -1.0, 0.000001);
    assert_true(ui_theme_motion_value(
        UI_THEME_MOTION_MAJOR_ENTER, 80.0, false, -DBL_MAX, DBL_MAX, &output));
    assert_true(isfinite(output));
    assert_true(fabs(output / DBL_MAX - 0.75) < 0.000001);
    assert_false(ui_theme_motion_value(
        UI_THEME_MOTION_MAJOR_ENTER, 1.0, false, 0.0, 1.0, NULL));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_provisional_tokens_are_exact_and_immutable),
        cmocka_unit_test(test_contrast_ratios_and_thresholds),
        cmocka_unit_test(test_demo_display_pairs_meet_required_contrast),
        cmocka_unit_test(test_alpha_composition_and_invalid_outputs_are_transactional),
        cmocka_unit_test(test_state_precedence),
        cmocka_unit_test(test_scaled_edges_are_deterministic_and_transactional),
        cmocka_unit_test(test_motion_roles_easing_and_reduced_motion),
        cmocka_unit_test(test_motion_interruption_starts_from_current_value)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}