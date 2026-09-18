#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/checked_size.h"
#include "../src/grid.h"
#include "../src/map_catalog.h"
#include "../src/menu_state.h"
#include "../src/ui_app_theme_adapter.h"
#include "../src/ui_canvas.h"
#include "../src/ui_ele.h"
#include "../src/ui_theme.h"
#include "../src/ui_workbench.h"
#include "../src/ui_workbench_chrome.h"
#include "../src/ui_workbench_store.h"

static void test_rejects_invalid_arguments(void **state) {
    UiAppWorkbenchPalette palette;
    Grid *grid;
    SDL_Color color;
    (void)state;
    assert_non_null(ui_app_theme_workbench_palette(&palette) ? &palette : NULL);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_string_equal(ui_workbench_chrome_result_string(UI_WORKBENCH_CHROME_OK),
                        "ok");
    assert_string_equal(
        ui_workbench_chrome_result_string(UI_WORKBENCH_CHROME_INVALID_ARGUMENT),
        "invalid argument");
    assert_true(ui_workbench_chrome_contract_dimensions(260, 160));
    assert_false(ui_workbench_chrome_contract_dimensions(120, 40));
    assert_false(ui_workbench_chrome_role_color(NULL,
                                                UI_WORKBENCH_CHROME_ROLE_PANEL,
                                                &color));
    assert_false(ui_workbench_chrome_role_color(
        &palette, UI_WORKBENCH_CHROME_ROLE_COUNT, &color));
    assert_false(ui_workbench_chrome_role_color(
        &palette, UI_WORKBENCH_CHROME_ROLE_PANEL, NULL));
    assert_int_equal(ui_workbench_chrome_preview_first_row(), 1);
    assert_int_equal(ui_workbench_chrome_footer_first_row(160), 157);
    assert_int_equal(ui_workbench_chrome_footer_first_row(120), -1);
    assert_int_equal(ui_workbench_chrome_left_pane_width(), 48);
    assert_int_equal(ui_workbench_chrome_right_pane_width(), 72);
    assert_false(ui_workbench_chrome_paint_panel(NULL, &palette, 0, 0, 10, 10,
                                                 false));
    grid = grid_create(260, 160);
    assert_non_null(grid);
    assert_false(ui_workbench_chrome_paint_panel(
        grid, NULL, 0, 0, 10, 10, false));
    assert_false(ui_workbench_chrome_paint_panel(
        grid, &palette, -1, 0, 10, 10, false));
    assert_false(ui_workbench_chrome_footer_rows(NULL, &palette, NULL, 100,
                                                 false));
    grid_destroy(grid);
}

static void assert_sdl_color(SDL_Color actual, uint8_t red, uint8_t green,
                             uint8_t blue, uint8_t alpha) {
    assert_int_equal(actual.r, red);
    assert_int_equal(actual.g, green);
    assert_int_equal(actual.b, blue);
    assert_int_equal(actual.a, alpha);
}

static void test_all_role_colors_come_from_tokens(void **state) {
    UiAppWorkbenchPalette palette;
    const UiThemePalette *tokens = &ui_theme_provisional_tokens()->palette;
    SDL_Color color;
    (void)state;
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_true(ui_workbench_chrome_role_color(
        &palette, UI_WORKBENCH_CHROME_ROLE_PANEL, &color));
    assert_sdl_color(color, tokens->panel.red, tokens->panel.green,
                     tokens->panel.blue, tokens->panel.alpha);
    assert_true(ui_workbench_chrome_role_color(
        &palette, UI_WORKBENCH_CHROME_ROLE_ACCENT, &color));
    assert_sdl_color(color, tokens->accent.red, tokens->accent.green,
                     tokens->accent.blue, tokens->accent.alpha);
    assert_true(ui_workbench_chrome_role_color(
        &palette, UI_WORKBENCH_CHROME_ROLE_FOCUS, &color));
    assert_sdl_color(color, tokens->focus.red, tokens->focus.green,
                     tokens->focus.blue, tokens->focus.alpha);
    assert_true(ui_workbench_chrome_role_color(
        &palette, UI_WORKBENCH_CHROME_ROLE_WARNING, &color));
    assert_sdl_color(color, tokens->warning.red, tokens->warning.green,
                     tokens->warning.blue, tokens->warning.alpha);
    assert_true(ui_workbench_chrome_role_color(
        &palette, UI_WORKBENCH_CHROME_ROLE_ERROR, &color));
    assert_sdl_color(color, tokens->error.red, tokens->error.green,
                     tokens->error.blue, tokens->error.alpha);
}

static void test_panels_use_token_borders_and_focus(void **state) {
    UiAppWorkbenchPalette palette;
    Grid *grid;
    Cell corner;
    Cell fill;
    (void)state;
    assert_true(ui_app_theme_workbench_palette(&palette));
    grid = grid_create(260, 160);
    assert_non_null(grid);
    assert_true(ui_workbench_chrome_paint_panel(grid, &palette, 2, 2, 10, 6,
                                                false));
    assert_true(grid_get(grid, 2, 2, &corner));
    assert_int_equal(corner.glyph, '+');
    assert_int_equal(corner.fg.r, palette.secondary_text.r);
    assert_true(grid_get(grid, 4, 4, &fill));
    assert_int_equal(fill.glyph, ' ');
    assert_int_equal(fill.bg.r, palette.panel.r);
    assert_true(ui_workbench_chrome_paint_panel(grid, &palette, 20, 2, 10, 6,
                                                true));
    assert_true(grid_get(grid, 20, 2, &corner));
    assert_int_equal(corner.glyph, '+');
    assert_int_equal(corner.fg.r, palette.accent.r);
    assert_true(grid_get(grid, 22, 4, &fill));
    assert_int_equal(fill.glyph, ' ');
    assert_int_equal(fill.bg.r, palette.elevated.r);
    assert_int_equal(fill.fg.r, palette.secondary_text.r);
    grid_destroy(grid);
}

static void test_footer_rows_render_identity_controls_diagnostics(void **state) {
    UiWorkbench workbench;
    Grid *grid;
    UiAppWorkbenchPalette palette;
    Cell status;
    Cell controls;
    Cell diagnostic;
    (void)state;
    ui_workbench_init(&workbench);
    grid = grid_create(260, 160);
    assert_non_null(grid);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_int_equal(ui_workbench_open(&workbench, MENU_MAIN),
                     UI_WORKBENCH_OK);
    assert_true(ui_workbench_chrome_footer_rows(grid, &palette, &workbench,
                                                100, false));
    assert_true(grid_get(grid, 1, 157, &status));
    assert_true(grid_get(grid, 1, 158, &controls));
    assert_true(grid_get(grid, 1, 159, &diagnostic));
    assert_int_equal(status.fg.r, palette.primary_text.r);
    assert_int_equal(controls.fg.r, palette.secondary_text.r);
    assert_int_equal(diagnostic.fg.r, palette.secondary_text.r);
    assert_true(status.glyph == 'U' || status.glyph == 'A');
    grid_destroy(grid);
    ui_workbench_destroy(&workbench);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_rejects_invalid_arguments),
        cmocka_unit_test(test_all_role_colors_come_from_tokens),
        cmocka_unit_test(test_panels_use_token_borders_and_focus),
        cmocka_unit_test(test_footer_rows_render_identity_controls_diagnostics)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}