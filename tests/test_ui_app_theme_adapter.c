#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>

#include <cmocka.h>

#include "../src/ui_app_theme_adapter.h"
#include "../src/ui_theme.h"

static void assert_color(SDL_Color color, uint8_t red, uint8_t green,
                         uint8_t blue, uint8_t alpha) {
    assert_int_equal(color.r, red);
    assert_int_equal(color.g, green);
    assert_int_equal(color.b, blue);
    assert_int_equal(color.a, alpha);
}

static void test_menu_palette_maps_exact_provisional_roles(void **state) {
    UiAppMenuPalette palette;
    (void)state;
    assert_true(ui_app_theme_menu_palette(&palette));
    /* Default scheme BRIGHT: the hovered label brightens, with no fill. */
    assert_color(palette.selected_foreground, 0xf2U, 0xf7U, 0xf8U, 0xffU);
    assert_color(palette.selected_background, 0x05U, 0x08U, 0x0aU, 0xffU);
    assert_color(palette.unselected_foreground, 0xa8U, 0xb4U, 0xb8U, 0xffU);
    assert_color(palette.unselected_background, 0x05U, 0x08U, 0x0aU, 0xffU);
}

static void test_menu_highlight_schemes_are_neutral_and_distinct(void **state) {
    UiAppMenuPalette palette;
    const UiThemePalette *tokens = &ui_theme_provisional_tokens()->palette;
    int scheme;
    (void)state;
    assert_false(ui_app_theme_menu_palette_for(NULL, UI_MENU_HIGHLIGHT_PLAIN));
    assert_false(ui_app_theme_menu_palette_for(&palette, UI_MENU_HIGHLIGHT_COUNT));
    for (scheme = 0; scheme < UI_MENU_HIGHLIGHT_COUNT; scheme++) {
        assert_true(ui_app_theme_menu_palette_for(&palette, (UiMenuHighlight)scheme));
        /* No scheme re-introduces the old green selection fill. */
        assert_false(palette.selected_background.r == 0x12U &&
                     palette.selected_background.g == 0x3dU &&
                     palette.selected_background.b == 0x32U);
        /* Unselected is identical in every scheme. */
        assert_color(palette.unselected_foreground, 0xa8U, 0xb4U, 0xb8U, 0xffU);
        assert_color(palette.unselected_background, 0x05U, 0x08U, 0x0aU, 0xffU);
        assert_int_equal(palette.selected_foreground.a, 255U);
        assert_int_equal(palette.selected_background.a, 255U);
    }
    /* INVERSE is the only scheme that fills, and it fills with the text colour. */
    assert_true(ui_app_theme_menu_palette_for(&palette, UI_MENU_HIGHLIGHT_INVERSE));
    assert_color(palette.selected_background, tokens->text_primary.red,
                 tokens->text_primary.green, tokens->text_primary.blue,
                 tokens->text_primary.alpha);
}

static void test_menu_palette_is_stable_and_preserves_alpha(void **state) {
    UiAppMenuPalette first;
    UiAppMenuPalette second;
    (void)state;
    assert_true(ui_app_theme_menu_palette(&first));
    assert_true(ui_app_theme_menu_palette(&second));
    assert_memory_equal(&first, &second, sizeof(first));
    assert_int_equal(first.selected_foreground.a, 255U);
    assert_int_equal(first.selected_background.a, 255U);
    assert_int_equal(first.unselected_foreground.a, 255U);
    assert_int_equal(first.unselected_background.a, 255U);
}

static void test_menu_palette_rejects_null_output(void **state) {
    (void)state;
    assert_false(ui_app_theme_menu_palette(NULL));
}

static void test_workbench_palette_maps_all_chrome_roles(void **state) {
    UiAppWorkbenchPalette palette;
    const UiThemePalette *tokens = &ui_theme_provisional_tokens()->palette;
    (void)state;
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_color(palette.primary_text, tokens->text_primary.red,
                 tokens->text_primary.green, tokens->text_primary.blue,
                 tokens->text_primary.alpha);
    assert_color(palette.secondary_text, tokens->text_secondary.red,
                 tokens->text_secondary.green, tokens->text_secondary.blue,
                 tokens->text_secondary.alpha);
    assert_color(palette.canvas, tokens->canvas.red, tokens->canvas.green,
                 tokens->canvas.blue, tokens->canvas.alpha);
    assert_color(palette.border, tokens->border.red, tokens->border.green,
                 tokens->border.blue, tokens->border.alpha);
    assert_color(palette.panel, tokens->panel.red, tokens->panel.green,
                 tokens->panel.blue, tokens->panel.alpha);
    assert_color(palette.elevated, tokens->elevated.red,
                 tokens->elevated.green, tokens->elevated.blue,
                 tokens->elevated.alpha);
    assert_color(palette.accent, tokens->accent.red, tokens->accent.green,
                 tokens->accent.blue, tokens->accent.alpha);
    assert_color(palette.focus, tokens->focus.red, tokens->focus.green,
                 tokens->focus.blue, tokens->focus.alpha);
    assert_color(palette.selection_background,
                 tokens->selection_background.red,
                 tokens->selection_background.green,
                 tokens->selection_background.blue,
                 tokens->selection_background.alpha);
    assert_color(palette.disabled_text, tokens->disabled_text.red,
                 tokens->disabled_text.green, tokens->disabled_text.blue,
                 tokens->disabled_text.alpha);
    assert_color(palette.disabled_background,
                 tokens->disabled_background.red,
                 tokens->disabled_background.green,
                 tokens->disabled_background.blue,
                 tokens->disabled_background.alpha);
    assert_color(palette.warning, tokens->warning.red, tokens->warning.green,
                 tokens->warning.blue, tokens->warning.alpha);
    assert_color(palette.error, tokens->error.red, tokens->error.green,
                 tokens->error.blue, tokens->error.alpha);
    assert_color(palette.success, tokens->success.red, tokens->success.green,
                 tokens->success.blue, tokens->success.alpha);
    assert_color(palette.destructive, tokens->destructive.red,
                 tokens->destructive.green, tokens->destructive.blue,
                 tokens->destructive.alpha);
    assert_false(ui_app_theme_workbench_palette(NULL));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_menu_palette_maps_exact_provisional_roles),
        cmocka_unit_test(test_menu_highlight_schemes_are_neutral_and_distinct),
        cmocka_unit_test(test_menu_palette_is_stable_and_preserves_alpha),
        cmocka_unit_test(test_menu_palette_rejects_null_output),
        cmocka_unit_test(test_workbench_palette_maps_all_chrome_roles)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}