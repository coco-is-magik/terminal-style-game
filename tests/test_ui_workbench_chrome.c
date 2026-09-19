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
#include "../src/ui_workbench_frame.h"
#include "../src/ui_workbench_store.h"

static void test_rejects_invalid_arguments(void **state) {
    UiAppWorkbenchPalette palette;
    Grid *grid;
    UiCanvas *canvas;
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
    canvas = ui_canvas_create(260, 156);
    assert_non_null(canvas);
    assert_false(ui_workbench_chrome_scale_preview_rows(NULL, canvas, 156,
                                                        100));
    assert_false(ui_workbench_chrome_scale_preview_rows(canvas, NULL, 156,
                                                        100));
    assert_false(ui_workbench_chrome_scale_preview_rows(canvas, canvas, 0,
                                                        100));
    assert_true(ui_workbench_chrome_scale_preview_rows(canvas, canvas, 156,
                                                       100));
    assert_false(ui_workbench_chrome_placement_cover_row(NULL, canvas, 156,
                                                         100, 157));
    assert_false(ui_workbench_chrome_placement_cover_row(canvas, NULL, 156,
                                                         100, 157));
    assert_true(ui_workbench_chrome_placement_cover_row(canvas, canvas, 156,
                                                        100, 157));
    assert_false(ui_workbench_chrome_blend_preview(NULL, canvas, 156, 100));
    assert_false(ui_workbench_chrome_blend_matches(NULL, canvas, 1, 156));
    ui_canvas_destroy(canvas);
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

static bool open_fixture(UiWorkbench *workbench, MenuId context) {
    return workbench && ui_workbench_open(workbench, context) == UI_WORKBENCH_OK;
}

static bool render_fixture(UiWorkbench *workbench, Grid *grid,
                           UiAppWorkbenchPalette *palette, int scale_percent,
                           double elapsed_ms, bool *ok) {
    UiWorkbenchFrameInput input = {grid,      workbench, palette,
                                    scale_percent, elapsed_ms, false,
                                    0,         0,         false};
    bool rendered;
    if (!ok) return false;
    rendered = ui_workbench_frame_render(input);
    *ok = rendered;
    return rendered;
}

static void test_scaled_preview_keeps_authored_cells(void **state) {
    UiWorkbench workbench;
    Grid *grid;
    UiAppWorkbenchPalette palette;
    UiCanvas *authored;
    UiCanvas *scaled;
    Cell expect;
    Cell actual;
    bool rendered = false;
    (void)state;
    ui_workbench_init(&workbench);
    grid = grid_create(UI_WORKBENCH_FRAME_CONTRACT_COLUMNS,
                       UI_WORKBENCH_FRAME_CONTRACT_ROWS);
    assert_non_null(grid);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_true(open_fixture(&workbench, MENU_MAIN));
    assert_true(render_fixture(&workbench, grid, &palette, 100, 0.0, &rendered));
    assert_true(rendered);
    authored = ui_canvas_create(UI_WORKBENCH_FRAME_CONTRACT_COLUMNS, 156);
    assert_non_null(authored);
    scaled = ui_canvas_create(UI_WORKBENCH_FRAME_CONTRACT_COLUMNS, 156);
    assert_non_null(scaled);
    ui_canvas_copy_grid_region(authored, grid, 0, 1);
    assert_true(ui_workbench_chrome_scale_preview_rows(scaled, authored, 156,
                                                       100));
    assert_true(ui_canvas_is_touched(scaled, 2, 4));
    assert_true(grid_get(grid, 2, 5, &actual));
    expect = scaled->cells[4U * 260U + 2U];
    assert_int_equal(actual.glyph, expect.glyph);
    assert_int_equal(actual.fg.r, expect.fg.r);
    ui_canvas_destroy(scaled);
    ui_canvas_destroy(authored);
    grid_destroy(grid);
    ui_workbench_destroy(&workbench);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_rejects_invalid_arguments),
        cmocka_unit_test(test_all_role_colors_come_from_tokens),
        cmocka_unit_test(test_panels_use_token_borders_and_focus),
        cmocka_unit_test(test_scaled_preview_keeps_authored_cells),
        cmocka_unit_test(test_footer_rows_render_identity_controls_diagnostics)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}