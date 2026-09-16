#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>

#include <cmocka.h>

#include "../src/ui_app_theme_adapter.h"

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
    assert_color(palette.selected_foreground, 0xa8U, 0xffU, 0xe1U, 0xffU);
    assert_color(palette.selected_background, 0x12U, 0x3dU, 0x32U, 0xffU);
    assert_color(palette.unselected_foreground, 0xa8U, 0xb4U, 0xb8U, 0xffU);
    assert_color(palette.unselected_background, 0x05U, 0x08U, 0x0aU, 0xffU);
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

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_menu_palette_maps_exact_provisional_roles),
        cmocka_unit_test(test_menu_palette_is_stable_and_preserves_alpha),
        cmocka_unit_test(test_menu_palette_rejects_null_output)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}