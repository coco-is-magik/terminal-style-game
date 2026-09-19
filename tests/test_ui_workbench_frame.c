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
#include "../src/ui_animation.h"
#include "../src/ui_app_theme_adapter.h"
#include "../src/ui_canvas.h"
#include "../src/ui_ele.h"
#include "../src/ui_preferences.h"
#include "../src/ui_workbench_chrome.h"
#include "../src/ui_workbench.h"
#include "../src/ui_workbench_frame.h"
#include "../src/ui_workbench_store.h"

#include "ui_workbench_frame_fixtures.h"

#define UI_WORKBENCH_FRAME_TEST_COLUMNS 260
#define UI_WORKBENCH_FRAME_TEST_ROWS 160
#define UI_WORKBENCH_FRAME_TEST_CELLS \
    ((size_t)UI_WORKBENCH_FRAME_TEST_COLUMNS * \
     (size_t)UI_WORKBENCH_FRAME_TEST_ROWS)

static bool open_fixture(UiWorkbench *workbench, MenuId context) {
    return workbench && ui_workbench_open(workbench, context) == UI_WORKBENCH_OK;
}

static bool render_fixture(UiWorkbench *workbench, Grid *grid,
                            UiAppWorkbenchPalette *palette,
                            int scale_percent, double elapsed_ms, bool *ok) {
    UiWorkbenchFrameInput input;
    bool rendered;
    if (!ok) return false;
    input.grid = grid;
    input.workbench = workbench;
    input.palette = palette;
    input.tooltip_text = NULL;
    input.scale_percent = scale_percent;
    input.elapsed_ms = elapsed_ms;
    input.reduced_motion = false;
    input.pointer_row = 0;
    input.pointer_column = 0;
    input.pointer_active = false;
    rendered = ui_workbench_frame_render(input);
    *ok = rendered;
    return rendered;
}

static bool distinct_footer(UiWorkbench *workbench, Grid *grid,
                             UiAppWorkbenchPalette *palette, bool *distinct) {
    bool rendered = false;
    if (!render_fixture(workbench, grid, palette, 100, 0.0, &rendered))
        return false;
    if (!rendered) return false;
    return ui_workbench_frame_footer_distinct(grid, distinct);
}

static void test_rejects_invalid_arguments(void **state) {
    UiWorkbench workbench;
    Grid *grid;
    UiAppWorkbenchPalette palette;
    Cell small[4];
    Cell fine[(size_t)UI_WORKBENCH_FRAME_CONTRACT_COLUMNS *
              (size_t)UI_WORKBENCH_FRAME_CONTRACT_ROWS];
    UiWorkbenchFrameInput bad;
    (void)state;
    ui_workbench_init(&workbench);
    grid = grid_create(UI_WORKBENCH_FRAME_CONTRACT_COLUMNS,
                       UI_WORKBENCH_FRAME_CONTRACT_ROWS);
    assert_non_null(grid);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_true(open_fixture(&workbench, MENU_MAIN));
    assert_true(ui_workbench_frame_contract_dimensions(
        UI_WORKBENCH_FRAME_CONTRACT_COLUMNS, UI_WORKBENCH_FRAME_CONTRACT_ROWS));
    assert_false(ui_workbench_frame_contract_dimensions(120, 40));
    bad.grid = NULL;
    bad.workbench = &workbench;
    bad.palette = &palette;
    bad.tooltip_text = NULL;
    bad.scale_percent = 100;
    bad.elapsed_ms = 0.0;
    bad.reduced_motion = false;
    bad.pointer_row = 0;
    bad.pointer_column = 0;
    bad.pointer_active = false;
    assert_false(ui_workbench_frame_render(bad));
    bad.grid = grid;
    bad.workbench = NULL;
    assert_false(ui_workbench_frame_render(bad));
    bad.workbench = &workbench;
    bad.palette = NULL;
    assert_false(ui_workbench_frame_render(bad));
    bad.palette = &palette;
    bad.scale_percent = 99;
    assert_false(ui_workbench_frame_render(bad));
    bad.scale_percent = 100;
    bad.elapsed_ms = -1.0;
    assert_false(ui_workbench_frame_render(bad));
    bad.elapsed_ms = 0.0;
    bad.pointer_row = -1;
    bad.pointer_active = true;
    assert_false(ui_workbench_frame_render(bad));
    assert_false(ui_workbench_frame_copy_cells(NULL, fine, sizeof(fine)));
    assert_false(ui_workbench_frame_copy_cells(grid, NULL, sizeof(fine)));
    assert_false(ui_workbench_frame_copy_cells(grid, small, sizeof(small)));
    assert_int_equal(ui_workbench_frame_checksum_fixture(NULL, 10U), 0U);
    assert_int_equal(ui_workbench_frame_checksum_fixture(fine, 0U), 0U);
    assert_false(ui_workbench_frame_contract_scale_policy(99));
    assert_false(ui_workbench_frame_preview_matches(NULL, NULL, NULL, 100, 0.0,
                                                    false));
    assert_false(ui_workbench_frame_footer_distinct(NULL, NULL));
    grid_destroy(grid);
    ui_workbench_destroy(&workbench);
}

static void test_same_state_produces_same_checksum(void **state) {
    UiWorkbench first;
    UiWorkbench second;
    Grid *grid;
    Grid *repeat;
    UiAppWorkbenchPalette palette;
    Cell first_cells[UI_WORKBENCH_FRAME_TEST_CELLS];
    Cell second_cells[UI_WORKBENCH_FRAME_TEST_CELLS];
    bool first_ok = false;
    bool second_ok = false;
    (void)state;
    ui_workbench_init(&first);
    ui_workbench_init(&second);
    grid = grid_create(UI_WORKBENCH_FRAME_TEST_COLUMNS,
                       UI_WORKBENCH_FRAME_TEST_ROWS);
    repeat = grid_create(UI_WORKBENCH_FRAME_TEST_COLUMNS,
                         UI_WORKBENCH_FRAME_TEST_ROWS);
    assert_non_null(grid);
    assert_non_null(repeat);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_true(open_fixture(&first, MENU_MAIN));
    assert_true(open_fixture(&second, MENU_MAIN));
    assert_true(render_fixture(&first, grid, &palette, 150, 0.0, &first_ok));
    assert_true(first_ok);
    assert_true(render_fixture(&second, repeat, &palette, 150, 0.0, &second_ok));
    assert_true(second_ok);
    assert_true(ui_workbench_frame_copy_cells(grid, first_cells,
                                              sizeof(first_cells)));
    assert_true(ui_workbench_frame_copy_cells(repeat, second_cells,
                                              sizeof(second_cells)));
    assert_int_equal(
        ui_workbench_frame_checksum_fixture(first_cells,
                                            UI_WORKBENCH_FRAME_TEST_CELLS),
        ui_workbench_frame_checksum_fixture(second_cells,
                                            UI_WORKBENCH_FRAME_TEST_CELLS));
    grid_destroy(grid);
    grid_destroy(repeat);
    ui_workbench_destroy(&first);
    ui_workbench_destroy(&second);
}

static void test_footer_rows_stay_distinct_across_contexts(void **state) {
    UiWorkbench workbench;
    Grid *grid;
    UiAppWorkbenchPalette palette;
    static const MenuId contexts[] = {MENU_MAIN, MENU_PAUSE, MENU_SETTINGS,
                                      MENU_CONFIRM_QUIT};
    size_t index;
    (void)state;
    ui_workbench_init(&workbench);
    grid = grid_create(UI_WORKBENCH_FRAME_TEST_COLUMNS,
                       UI_WORKBENCH_FRAME_TEST_ROWS);
    assert_non_null(grid);
    assert_true(ui_app_theme_workbench_palette(&palette));
    for (index = 0U; index < sizeof(contexts) / sizeof(contexts[0U]); index++) {
        bool distinct = false;
        assert_true(open_fixture(&workbench, contexts[index]));
        assert_non_null(workbench.layout);
        assert_true(distinct_footer(&workbench, grid, &palette, &distinct));
        assert_true(distinct);
    }
    grid_destroy(grid);
    ui_workbench_destroy(&workbench);
}

static void test_scale_change_does_not_rewrite_help_footer(void **state) {
    UiWorkbench workbench;
    Grid *grid;
    UiAppWorkbenchPalette palette;
    static const int scales[] = {100, 125, 150, 200};
    char expected[UI_WORKBENCH_FRAME_TEST_COLUMNS + 1U];
    char controls[UI_WORKBENCH_FRAME_TEST_COLUMNS + 1U];
    size_t index;
    int x;
    bool rendered = false;
    (void)state;
    ui_workbench_init(&workbench);
    grid = grid_create(UI_WORKBENCH_FRAME_TEST_COLUMNS,
                       UI_WORKBENCH_FRAME_TEST_ROWS);
    assert_non_null(grid);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_true(open_fixture(&workbench, MENU_MAIN));
    assert_true(render_fixture(&workbench, grid, &palette, scales[0], 0.0,
                               &rendered));
    assert_true(rendered);
    for (x = 0; x < UI_WORKBENCH_FRAME_TEST_COLUMNS; x++)
        expected[x] = (char)grid->cells[(size_t)(UI_WORKBENCH_FRAME_TEST_ROWS - 2) *
                                        (size_t)grid->width + (size_t)x]
                          .glyph;
    expected[UI_WORKBENCH_FRAME_TEST_COLUMNS] = '\0';
    for (index = 1U; index < sizeof(scales) / sizeof(scales[0U]); index++) {
        assert_true(render_fixture(&workbench, grid, &palette, scales[index],
                                   0.0, &rendered));
        assert_true(rendered);
        for (x = 0; x < UI_WORKBENCH_FRAME_TEST_COLUMNS; x++)
            controls[x] =
                (char)grid->cells[(size_t)(UI_WORKBENCH_FRAME_TEST_ROWS - 2) *
                                  (size_t)grid->width + (size_t)x]
                    .glyph;
        controls[UI_WORKBENCH_FRAME_TEST_COLUMNS] = '\0';
        assert_string_equal(controls, expected);
    }
    grid_destroy(grid);
    ui_workbench_destroy(&workbench);
}

static void test_preview_matches_normal_run_rendering(void **state) {
    UiWorkbench workbench;
    Grid *grid;
    UiAppWorkbenchPalette palette;
    Grid *staging;
    UiCanvas *authored;
    UiCanvas *scaled;
    static const int scales[] = {100, 125, 150, 200};
    static const MenuId contexts[] = {MENU_MAIN, MENU_PAUSE, MENU_SETTINGS,
                                      MENU_CONFIRM_QUIT};
    size_t context_index;
    size_t scale_index;
    bool rendered = false;
    (void)state;
    ui_workbench_init(&workbench);
    grid = grid_create(UI_WORKBENCH_FRAME_TEST_COLUMNS,
                       UI_WORKBENCH_FRAME_TEST_ROWS);
    assert_non_null(grid);
    staging = grid_create(UI_WORKBENCH_FRAME_TEST_COLUMNS, 156);
    assert_non_null(staging);
    authored = ui_canvas_create(UI_WORKBENCH_FRAME_TEST_COLUMNS, 156);
    assert_non_null(authored);
    scaled = ui_canvas_create(UI_WORKBENCH_FRAME_TEST_COLUMNS, 156);
    assert_non_null(scaled);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_true(ui_workbench_frame_contract_scale_policy(100));
    assert_true(ui_workbench_frame_contract_scale_policy(125));
    assert_true(ui_workbench_frame_contract_scale_policy(150));
    assert_true(ui_workbench_frame_contract_scale_policy(200));
    assert_false(ui_workbench_frame_contract_scale_policy(101));
    for (context_index = 0U;
         context_index < sizeof(contexts) / sizeof(contexts[0U]);
         context_index++) {
        assert_true(open_fixture(&workbench, contexts[context_index]));
        for (scale_index = 0U;
             scale_index < sizeof(scales) / sizeof(scales[0U]); scale_index++) {
            int rows;
            int row;
            int column;
            assert_true(render_fixture(&workbench, grid, &palette,
                                       scales[scale_index], 0.0, &rendered));
            assert_true(rendered);
            (void)grid_clear_region_zero(staging, 0, 0, staging->width,
                                         staging->height);
            ui_layout_set_focus(workbench.layout, 0);
            ui_layout_render(workbench.layout, staging,
                             palette.secondary_text, palette.canvas);
            ui_canvas_copy_grid_region(authored, staging, 0, 0);
            rows = ui_workbench_chrome_preview_rows(160, scales[scale_index]);
            assert_true(rows > 0 && rows <= 156);
            assert_true(ui_workbench_chrome_scale_preview_rows(
                scaled, authored, rows, scales[scale_index]));
            for (row = 0; row < rows; row++) {
                for (column = 0; column < 260; column++) {
                    const Cell *wanted = &scaled->cells[(size_t)row * 260U +
                                                        (size_t)column];
                    const Cell *composed =
                        &grid->cells[(size_t)(row + 1) * 260U +
                                     (size_t)column];
                    if (!ui_canvas_is_touched(scaled, column, row)) continue;
                    if (!ui_workbench_chrome_cell_in_preview(column, row + 1,
                                                             260, 160))
                        continue;
                    if (wanted->glyph == '>' || wanted->glyph == '<')
                        continue;
                    assert_int_equal(composed->glyph, wanted->glyph);
                }
            }
        }
    }
    ui_canvas_destroy(scaled);
    ui_canvas_destroy(authored);
    grid_destroy(staging);
    grid_destroy(grid);
    ui_workbench_destroy(&workbench);
}

/*PART_REDUCED*/
static void test_overlays_stay_outside_authored_cells(void **state) {
    UiWorkbench workbench;
    Grid *grid;
    UiAppWorkbenchPalette palette;
    bool rendered = false;
    UiElement *focused = NULL;
    SDL_Color selection;
    int focus_x;
    int focus_y;
    int focus_width;
    int focus_height;
    Cell left_marker;
    Cell right_marker;
    (void)state;
    ui_workbench_init(&workbench);
    grid = grid_create(UI_WORKBENCH_FRAME_TEST_COLUMNS,
                       UI_WORKBENCH_FRAME_TEST_ROWS);
    assert_non_null(grid);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_true(open_fixture(&workbench, MENU_MAIN));
    assert_true(ui_workbench_chrome_role_color(
        &palette, UI_WORKBENCH_CHROME_ROLE_ACCENT, &selection));
    assert_true(render_fixture(&workbench, grid, &palette, 100, 0.0, &rendered));
    assert_true(rendered);
    focused = ui_workbench_current_element(&workbench);
    assert_non_null(focused);
    assert_true(ui_ele_absolute_bounds(focused, &focus_x, &focus_y,
                                       &focus_width, &focus_height));
    assert_true(focus_width > 0 && focus_height > 0);
    assert_true(focus_x - 1 >= 0);
    assert_true(focus_x + focus_width < UI_WORKBENCH_FRAME_TEST_COLUMNS);
    assert_true(focus_y >= UI_WORKBENCH_CHROME_PREVIEW_FIRST_ROW);
    assert_true(ui_workbench_chrome_cell_in_preview(
        focus_x - 1, focus_y, UI_WORKBENCH_FRAME_TEST_COLUMNS,
        UI_WORKBENCH_FRAME_TEST_ROWS));
    assert_true(ui_workbench_chrome_cell_in_preview(
        focus_x + focus_width, focus_y, UI_WORKBENCH_FRAME_TEST_COLUMNS,
        UI_WORKBENCH_FRAME_TEST_ROWS));
    assert_true(grid_get(grid, focus_x - 1, focus_y + 1, &left_marker));
    assert_true(grid_get(grid, focus_x + focus_width, focus_y + 1,
                         &right_marker));
    assert_int_equal(left_marker.glyph, '>');
    assert_int_equal(right_marker.glyph, '<');
    assert_int_equal(left_marker.fg.r, selection.r);
    assert_int_equal(left_marker.fg.g, selection.g);
    assert_int_equal(left_marker.fg.b, selection.b);
    assert_int_equal(left_marker.fg.a, selection.a);
    assert_int_equal(right_marker.fg.r, selection.r);
    assert_int_equal(right_marker.fg.g, selection.g);
    assert_int_equal(right_marker.fg.b, selection.b);
    assert_int_equal(right_marker.fg.a, selection.a);
    grid_destroy(grid);
    ui_workbench_destroy(&workbench);
}

static void test_reduced_motion_freezes_animated_decoration(void **state) {
    UiWorkbench workbench;
    Grid *animated;
    Grid *still;
    UiAppWorkbenchPalette palette;
    UiWorkbenchFrameInput input;
    bool animated_ok = false;
    bool still_ok = false;
    size_t index;
    (void)state;
    ui_workbench_init(&workbench);
    animated = grid_create(UI_WORKBENCH_FRAME_TEST_COLUMNS,
                           UI_WORKBENCH_FRAME_TEST_ROWS);
    still = grid_create(UI_WORKBENCH_FRAME_TEST_COLUMNS,
                        UI_WORKBENCH_FRAME_TEST_ROWS);
    assert_non_null(animated);
    assert_non_null(still);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_true(open_fixture(&workbench, MENU_MAIN));
    input.grid = animated;
    input.workbench = &workbench;
    input.palette = &palette;
    input.tooltip_text = NULL;
    input.scale_percent = 100;
    input.elapsed_ms = 400.0;
    input.reduced_motion = false;
    input.pointer_row = 0;
    input.pointer_column = 0;
    input.pointer_active = false;
    assert_true(ui_workbench_frame_render(input));
    animated_ok = true;
    input.grid = still;
    input.reduced_motion = true;
    assert_true(ui_workbench_frame_render(input));
    still_ok = true;
    assert_true(animated_ok && still_ok);
    assert_true(ui_workbench_frame_preview_matches(still, &workbench, &palette,
                                                   100, 400.0, true));
    for (index = 0U; index < UI_WORKBENCH_FRAME_TEST_CELLS; index++) {
        if (animated->cells[index].glyph != still->cells[index].glyph) break;
    }
    grid_destroy(animated);
    grid_destroy(still);
    ui_workbench_destroy(&workbench);
}

static bool frame_has_text_at(const Grid *grid, int x, int y,
                              const char *text) {
    size_t i;
    size_t length;
    if (!grid || !grid->cells || !text) return false;
    length = strlen(text);
    if (x < 0 || y < 0 || y >= grid->height || x + (int)length > grid->width)
        return false;
    for (i = 0U; i < length; i++) {
        if (grid->cells[(size_t)y * (size_t)grid->width + (size_t)(x + (int)i)]
                .glyph != (uint8_t)text[i]) return false;
    }
    return true;
}

static void test_tooltip_text_reaches_footer_row(void **state) {
    UiWorkbench workbench;
    Grid *grid;
    UiAppWorkbenchPalette palette;
    UiWorkbenchFrameInput input;
    bool rendered = false;
    (void)state;
    ui_workbench_init(&workbench);
    grid = grid_create(UI_WORKBENCH_FRAME_TEST_COLUMNS,
                       UI_WORKBENCH_FRAME_TEST_ROWS);
    assert_non_null(grid);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_true(open_fixture(&workbench, MENU_MAIN));
    input.grid = grid;
    input.workbench = &workbench;
    input.palette = &palette;
    input.tooltip_text = "Browse mode. Up/Down selects an element.";
    input.scale_percent = 100;
    input.elapsed_ms = 0.0;
    input.reduced_motion = false;
    input.pointer_row = 0;
    input.pointer_column = 0;
    input.pointer_active = false;
    assert_true(ui_workbench_frame_render(input));
    rendered = true;
    assert_true(rendered);
    assert_true(frame_has_text_at(grid, 1, 157, "UI WORKBENCH"));
    assert_true(frame_has_text_at(grid, 1, 159, "Loaded | Browse mode."));
    grid_destroy(grid);
    ui_workbench_destroy(&workbench);
}

/*PART_POINTER*/
static void test_recorded_contract_fixtures_match_oracle(void **state) {
    UiWorkbench workbench;
    Grid *grid;
    UiAppWorkbenchPalette palette;
    Cell cells[UI_WORKBENCH_FRAME_TEST_CELLS];
    size_t index;
    bool rendered = false;
    (void)state;
    ui_workbench_init(&workbench);
    grid = grid_create(UI_WORKBENCH_FRAME_TEST_COLUMNS,
                       UI_WORKBENCH_FRAME_TEST_ROWS);
    assert_non_null(grid);
    assert_true(ui_app_theme_workbench_palette(&palette));
    for (index = 0U;
         index < ui_workbench_frame_contract_fixture_count; index++) {
        uint64_t checksum;
        assert_true(open_fixture(
            &workbench,
            ui_workbench_frame_contract_fixtures[index].context));
        assert_true(render_fixture(
            &workbench, grid, &palette,
            ui_workbench_frame_contract_fixtures[index].scale_percent,
            UI_WORKBENCH_FRAME_FIXTURE_ELAPSED_MS, &rendered));
        assert_true(rendered);
        assert_true(ui_workbench_frame_copy_cells(grid, cells, sizeof(cells)));
        checksum = ui_workbench_frame_checksum_fixture(
            cells, UI_WORKBENCH_FRAME_TEST_CELLS);
        assert_int_equal(checksum,
                         ui_workbench_frame_contract_fixtures[index].checksum);
    }
    grid_destroy(grid);
    ui_workbench_destroy(&workbench);
}

static void test_pointer_pane_focus_is_visible_without_color(void **state) {
    UiWorkbench workbench;
    Grid *grid;
    UiAppWorkbenchPalette palette;
    UiWorkbenchFrameInput input;
    int left_x;
    int left_y;
    int left_width;
    int left_height;
    int right_x;
    int right_y;
    int right_width;
    int right_height;
    Cell left_corner;
    Cell right_corner;
    (void)state;
    ui_workbench_init(&workbench);
    grid = grid_create(UI_WORKBENCH_FRAME_TEST_COLUMNS,
                       UI_WORKBENCH_FRAME_TEST_ROWS);
    assert_non_null(grid);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_true(open_fixture(&workbench, MENU_MAIN));
    assert_true(ui_workbench_chrome_pane_content_bounds(
        0, UI_WORKBENCH_FRAME_TEST_COLUMNS, UI_WORKBENCH_FRAME_TEST_ROWS,
        &left_x, &left_y, &left_width, &left_height));
    assert_true(ui_workbench_chrome_pane_content_bounds(
        1, UI_WORKBENCH_FRAME_TEST_COLUMNS, UI_WORKBENCH_FRAME_TEST_ROWS,
        &right_x, &right_y, &right_width, &right_height));
    input.grid = grid;
    input.workbench = &workbench;
    input.palette = &palette;
    input.tooltip_text = NULL;
    input.scale_percent = 100;
    input.elapsed_ms = 0.0;
    input.reduced_motion = false;
    input.pointer_row = left_y + 1;
    input.pointer_column = left_x + 1;
    input.pointer_active = true;
    assert_true(ui_workbench_chrome_cell_in_preview(100, 100,
                                                    UI_WORKBENCH_FRAME_TEST_COLUMNS,
                                                    UI_WORKBENCH_FRAME_TEST_ROWS));
    assert_false(ui_workbench_chrome_cell_in_preview(
        left_x + 1, left_y + 1, UI_WORKBENCH_FRAME_TEST_COLUMNS,
        UI_WORKBENCH_FRAME_TEST_ROWS));
    assert_true(ui_workbench_chrome_paint_panes(grid, &palette, true, false));
    assert_true(grid_get(grid, left_x, left_y, &left_corner));
    assert_int_equal(left_corner.glyph, '+');
    assert_int_equal(left_corner.fg.r, palette.accent.r);
    assert_true(ui_workbench_chrome_paint_panes(grid, &palette, false, true));
    assert_true(grid_get(grid, left_x, left_y, &left_corner));
    assert_true(grid_get(grid, right_x, right_y, &right_corner));
    assert_int_equal(right_corner.glyph, '+');
    assert_int_equal(right_corner.fg.r, palette.accent.r);
    assert_true(ui_workbench_frame_render(input));
    grid_destroy(grid);
    ui_workbench_destroy(&workbench);
}

/*PART_MAIN*/
int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_rejects_invalid_arguments),
        cmocka_unit_test(test_same_state_produces_same_checksum),
        cmocka_unit_test(test_footer_rows_stay_distinct_across_contexts),
        cmocka_unit_test(test_scale_change_does_not_rewrite_help_footer),
        cmocka_unit_test(test_overlays_stay_outside_authored_cells),
        cmocka_unit_test(test_preview_matches_normal_run_rendering),
        cmocka_unit_test(test_reduced_motion_freezes_animated_decoration),
        cmocka_unit_test(test_pointer_pane_focus_is_visible_without_color),
        cmocka_unit_test(test_tooltip_text_reaches_footer_row),
        cmocka_unit_test(test_recorded_contract_fixtures_match_oracle)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}