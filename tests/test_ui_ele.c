#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <cmocka.h>

#include "../src/ui_ele.h"
#include "../src/grid.h"

static int grid_has_text_at(Grid *grid, int x, int y, const char *text) {
    int len = (int)strlen(text);

    for (int i = 0; i < len; i++) {
        Cell c;
        assert_true(grid_get(grid, x + i, y, &c));
        if (c.glyph != (uint8_t)text[i]) return 0;
    }

    return 1;
}

static UiElement make_text_element(const char *name, const char *content,
                                   int x, int y, int width, int z_index) {
    UiElement e;
    memset(&e, 0, sizeof(e));
    strncpy(e.name, name, sizeof(e.name) - 1);
    e.type = UI_ELE_TEXT;
    e.layout.x = x;
    e.layout.y = y;
    e.layout.width = width;
    e.visible = 1;
    e.z_index = z_index;
    ui_ele_set_content(&e, content);
    return e;
}

static void release_stack_element(UiElement *e) {
    free(e->content);
    e->content = NULL;
}

static void test_ui_ele_load_text(void **state) {
    (void)state;

    UiCache cache;
    ui_cache_init(&cache, NULL);
    UiElement *element = ui_ele_load("assets/ui_elements/tooltip_slot.txt", &cache);

    assert_non_null(element);
    assert_string_equal(element->name, "tooltip_slot");
    assert_int_equal(element->type, UI_ELE_TEXT);
    assert_int_equal(element->layout.coords_mode, UI_COORD_RELATIVE);
    assert_int_equal(element->layout.width, 42);
    assert_int_equal(element->layout.height, 4);
    assert_string_equal(element->parent_name, "right_side_container");

    ui_ele_destroy(element);
    ui_cache_destroy(&cache);
}

static void test_ui_ele_parent_resolution_from_cache(void **state) {
    (void)state;

    UiCache cache;
    ui_cache_init(&cache, NULL);
    assert_non_null(ui_cache_load(&cache, "right_side_container", "assets/ui_elements"));
    UiElement *child = ui_cache_load(&cache, "tooltip_slot", "assets/ui_elements");

    assert_non_null(child);
    assert_non_null(child->parent);
    assert_string_equal(child->parent->name, "right_side_container");

    ui_cache_destroy(&cache);
}

static void test_ui_ele_render_wrap(void **state) {
    (void)state;

    UiCache cache;
    ui_cache_init(&cache, NULL);
    UiElement *element = ui_ele_load("assets/ui_elements/tooltip_slot.txt", &cache);
    assert_non_null(element);
    element->layout.x = 0;
    element->layout.y = 0;
    element->layout.coords_mode = UI_COORD_ABSOLUTE;
    element->layout.width = 5;
    element->layout.height = 4;
    ui_ele_set_content(element, "hello world wrap");

    Grid *grid = grid_create(20, 8);
    assert_non_null(grid);
    SDL_Color fg = {255, 255, 255, 255};
    SDL_Color bg = {0, 0, 0, 255};
    grid_clear(grid, bg);
    ui_ele_render(element, grid, 0, 0, fg, bg);

    assert_true(grid_has_text_at(grid, 0, 0, "hello"));
    assert_true(grid_has_text_at(grid, 0, 1, "world"));
    assert_true(grid_has_text_at(grid, 0, 2, "wrap"));

    grid_destroy(grid);
    ui_ele_destroy(element);
    ui_cache_destroy(&cache);
}

static void test_ui_layout_load_and_substitute(void **state) {
    (void)state;

    UiCache cache;
    ui_cache_init(&cache, "assets/ui_layouts/master_map.txt");
    ui_cache_tick(&cache, "live_edit_side", "assets/ui_elements");
    UiElement *tooltip = ui_cache_get(&cache, "slot_material_palette");
    assert_non_null(tooltip);

    UiLayout *layout = ui_layout_load("assets/ui_layouts/live_edit_side.txt", &cache);
    assert_non_null(layout);
    assert_string_equal(layout->name, "live_edit_side");
    assert_int_equal(layout->element_count, 11);
    assert_int_equal(layout->slot_count, 15);
    assert_non_null(layout->slot_elements[0]);

    ui_layout_substitute(layout, "material_palette", tooltip);
    assert_ptr_equal(layout->slot_elements[0], tooltip);

    ui_layout_destroy(layout);
    ui_cache_destroy(&cache);
}

static void test_ui_cache_master_map(void **state) {
    (void)state;

    UiCache cache;
    ui_cache_init(&cache, "assets/ui_layouts/master_map.txt");
    assert_int_equal(cache.master_count, 8);
    assert_string_equal(cache.master_entries[0].layout, "live_edit_side");
    assert_int_equal(cache.master_entries[0].cache_next_count, 30);
    assert_string_equal(cache.master_entries[1].layout, "main_menu");
    assert_int_equal(cache.master_entries[1].cache_next_count, 5);
    assert_string_equal(cache.master_entries[2].layout, "pause_menu");
    assert_string_equal(cache.master_entries[3].layout, "editor_menu");
    assert_string_equal(cache.master_entries[4].layout, "confirm_quit");
    assert_string_equal(cache.master_entries[5].layout, "designer_exit_confirm");
    assert_string_equal(cache.master_entries[6].layout, "asset_select");
    assert_string_equal(cache.master_entries[7].layout, "hud_overlay");

    ui_cache_tick(&cache, "live_edit_side", "assets/ui_elements");
    assert_non_null(ui_cache_get(&cache, "le_material_pane"));
    assert_non_null(ui_cache_get(&cache, "le_dec_meta_pane"));
    assert_non_null(ui_cache_get(&cache, "slot_dec_tooltip"));

    ui_cache_destroy(&cache);
}

static void test_ui_layout_hud_overlay_slots(void **state) {
    (void)state;

    UiCache cache;
    ui_cache_init(&cache, "assets/ui_layouts/master_map.txt");
    ui_cache_tick(&cache, "hud_overlay", "assets/ui_elements");

    UiLayout *layout = ui_layout_load("assets/ui_layouts/hud_overlay.txt", &cache);
    assert_non_null(layout);
    assert_int_equal(layout->element_count, 12);
    assert_int_equal(ui_layout_focusable_count(layout), 0);

    UiElement *grid_slot = ui_cache_get(&cache, "hud_grid");
    assert_non_null(grid_slot);
    ui_ele_set_content(grid_slot, "Grid: 260x160");

    Grid *grid = grid_create(80, 20);
    assert_non_null(grid);
    SDL_Color fg = {255, 255, 255, 255};
    SDL_Color bg = {0, 0, 0, 255};
    grid_clear(grid, bg);
    ui_layout_render(layout, grid, fg, bg);

    assert_true(grid_has_text_at(grid, 2, 2, "Grid: 260x160"));
    assert_true(grid_has_text_at(grid, 2, 5, "Press ESC for menu"));
    assert_true(grid_has_text_at(grid, 2, 7, "[1-Second Rolling Stats]"));

    grid_destroy(grid);
    ui_layout_destroy(layout);
    ui_cache_destroy(&cache);
}

static void test_ui_layout_main_menu_focus_actions(void **state) {
    (void)state;

    UiCache cache;
    ui_cache_init(&cache, "assets/ui_layouts/master_map.txt");
    ui_cache_tick(&cache, "main_menu", "assets/ui_elements");

    UiLayout *layout = ui_layout_load("assets/ui_layouts/main_menu.txt", &cache);
    assert_non_null(layout);
    assert_int_equal(ui_layout_focusable_count(layout), 3);

    UiElement *focused = ui_layout_get_focused(layout, 0);
    assert_non_null(focused);
    assert_string_equal(focused->name, "main_menu_start");
    assert_string_equal(focused->action, "start_game");

    focused = ui_layout_get_focused(layout, 1);
    assert_non_null(focused);
    assert_string_equal(focused->name, "main_menu_asset_editor");
    assert_string_equal(focused->action, "open_asset_editor");

    focused = ui_layout_get_focused(layout, 2);
    assert_non_null(focused);
    assert_string_equal(focused->name, "main_menu_quit");
    assert_string_equal(focused->action, "quit");

    assert_null(ui_layout_get_focused(layout, 3));

    ui_layout_destroy(layout);
    ui_cache_destroy(&cache);
}

static void assert_layout_actions(UiCache *cache,
                                  const char *layout_path,
                                  int expected_count,
                                  const char *a0,
                                  const char *a1,
                                  const char *a2,
                                  const char *a3) {
    UiLayout *layout = ui_layout_load(layout_path, cache);
    assert_non_null(layout);
    assert_int_equal(ui_layout_focusable_count(layout), expected_count);

    const char *actions[] = {a0, a1, a2, a3};
    for (int i = 0; i < expected_count; i++) {
        UiElement *focused = ui_layout_get_focused(layout, i);
        assert_non_null(focused);
        assert_string_equal(focused->action, actions[i]);
    }
    assert_null(ui_layout_get_focused(layout, expected_count));
    ui_layout_destroy(layout);
}

static void test_ui_layout_remaining_menu_focus_actions(void **state) {
    (void)state;

    UiCache cache;
    ui_cache_init(&cache, "assets/ui_layouts/master_map.txt");
    ui_cache_tick(&cache, "pause_menu", "assets/ui_elements");
    ui_cache_tick(&cache, "editor_menu", "assets/ui_elements");
    ui_cache_tick(&cache, "confirm_quit", "assets/ui_elements");
    ui_cache_tick(&cache, "designer_exit_confirm", "assets/ui_elements");
    ui_cache_tick(&cache, "asset_select", "assets/ui_elements");

    assert_layout_actions(&cache, "assets/ui_layouts/pause_menu.txt", 3,
                          "resume", "return_to_main_menu", "quit", NULL);
    assert_layout_actions(&cache, "assets/ui_layouts/editor_menu.txt", 2,
                          "back_to_editor", "return_to_main_menu", NULL, NULL);
    assert_layout_actions(&cache, "assets/ui_layouts/confirm_quit.txt", 2,
                          "confirm_quit", "cancel", NULL, NULL);
    assert_layout_actions(&cache, "assets/ui_layouts/designer_exit_confirm.txt", 2,
                          "discard_changes", "cancel", NULL, NULL);
    assert_layout_actions(&cache, "assets/ui_layouts/asset_select.txt", 4,
                          "open_live_edit", "open_decals", "open_materials", "coming_soon");

    ui_cache_destroy(&cache);
}

static void test_ui_ele_hidden_not_rendered(void **state) {
    (void)state;

    UiElement e = make_text_element("hidden", "hide", 0, 0, 10, 0);
    e.visible = 0;
    Grid *grid = grid_create(10, 4);
    assert_non_null(grid);
    SDL_Color fg = {255, 255, 255, 255};
    SDL_Color bg = {0, 0, 0, 255};
    grid_clear(grid, bg);

    ui_ele_render(&e, grid, 0, 0, fg, bg);
    Cell c;
    assert_true(grid_get(grid, 0, 0, &c));
    assert_int_equal(c.glyph, ' ');

    grid_destroy(grid);
    release_stack_element(&e);
}

static void test_ui_ele_center_align(void **state) {
    (void)state;

    UiElement e = make_text_element("center", "hi", 0, 0, 6, 0);
    e.align = UI_ALIGN_CENTER;
    Grid *grid = grid_create(10, 4);
    assert_non_null(grid);
    SDL_Color fg = {255, 255, 255, 255};
    SDL_Color bg = {0, 0, 0, 255};
    grid_clear(grid, bg);

    ui_ele_render(&e, grid, 0, 0, fg, bg);
    assert_true(grid_has_text_at(grid, 2, 0, "hi"));

    grid_destroy(grid);
    release_stack_element(&e);
}

static void test_ui_ele_color_override(void **state) {
    (void)state;

    UiElement e = make_text_element("color", "x", 0, 0, 4, 0);
    e.has_fg = true;
    e.has_bg = true;
    e.fg = (SDL_Color){1, 2, 3, 255};
    e.bg = (SDL_Color){4, 5, 6, 255};
    Grid *grid = grid_create(10, 4);
    assert_non_null(grid);
    SDL_Color fg = {255, 255, 255, 255};
    SDL_Color bg = {0, 0, 0, 255};
    grid_clear(grid, bg);

    ui_ele_render(&e, grid, 0, 0, fg, bg);
    Cell c;
    assert_true(grid_get(grid, 0, 0, &c));
    assert_int_equal(c.fg.r, 1);
    assert_int_equal(c.fg.g, 2);
    assert_int_equal(c.bg.r, 4);
    assert_int_equal(c.bg.g, 5);

    grid_destroy(grid);
    release_stack_element(&e);
}

static void test_ui_ele_button_action(void **state) {
    (void)state;

    UiElement e = make_text_element("button", "Go", 0, 0, 8, 0);
    e.type = UI_ELE_BUTTON;
    strncpy(e.action, "start_game", sizeof(e.action) - 1);
    assert_int_equal(e.type, UI_ELE_BUTTON);
    assert_string_equal(e.action, "start_game");
    release_stack_element(&e);
}

static void test_ui_ele_z_index_child_order(void **state) {
    (void)state;

    UiElement low = make_text_element("low", "A", 0, 0, 4, 0);
    UiElement high = make_text_element("high", "B", 0, 0, 4, 10);
    UiElement parent;
    memset(&parent, 0, sizeof(parent));
    parent.type = UI_ELE_CONTAINER;
    parent.visible = 1;
    parent.children[0] = &high;
    parent.children[1] = &low;
    parent.child_count = 2;

    Grid *grid = grid_create(10, 4);
    assert_non_null(grid);
    SDL_Color fg = {255, 255, 255, 255};
    SDL_Color bg = {0, 0, 0, 255};
    grid_clear(grid, bg);

    ui_ele_render(&parent, grid, 0, 0, fg, bg);
    Cell c;
    assert_true(grid_get(grid, 0, 0, &c));
    assert_int_equal(c.glyph, 'B');

    grid_destroy(grid);
    release_stack_element(&low);
    release_stack_element(&high);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ui_ele_load_text),
        cmocka_unit_test(test_ui_ele_parent_resolution_from_cache),
        cmocka_unit_test(test_ui_ele_render_wrap),
        cmocka_unit_test(test_ui_layout_load_and_substitute),
        cmocka_unit_test(test_ui_cache_master_map),
        cmocka_unit_test(test_ui_layout_hud_overlay_slots),
        cmocka_unit_test(test_ui_layout_main_menu_focus_actions),
        cmocka_unit_test(test_ui_layout_remaining_menu_focus_actions),
        cmocka_unit_test(test_ui_ele_hidden_not_rendered),
        cmocka_unit_test(test_ui_ele_center_align),
        cmocka_unit_test(test_ui_ele_color_override),
        cmocka_unit_test(test_ui_ele_button_action),
        cmocka_unit_test(test_ui_ele_z_index_child_order),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}