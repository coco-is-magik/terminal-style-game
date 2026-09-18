#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>

#include "../src/ui_editor_presentation.h"

static const UiRenderTheme theme = {
    {255, 255, 0, 255}, {0, 0, 80, 255},
    {0, 0, 0, 255}, {255, 255, 0, 255},
    {128, 128, 128, 255}, {20, 20, 20, 255}
};

static bool grid_contains_text(const Grid *grid, const char *text) {
    size_t length = strlen(text);
    int y;
    int x;
    for (y = 0; y < grid->height; y++)
        for (x = 0; x + (int)length <= grid->width; x++) {
            size_t i;
            for (i = 0U; i < length; i++)
                if (grid->cells[(size_t)y * (size_t)grid->width +
                                (size_t)x + i].glyph != (uint8_t)text[i]) break;
            if (i == length) return true;
        }
    return false;
}

static void test_renders_workspace_candidate_without_host_state(void **state) {
    UiMenuWorkspace workspace;
    AssetRegistry assets;
    UiElementId button;
    Grid *grid;
    DocumentStateId staged_state;
    (void)state;
    ui_menu_workspace_init(&workspace);
    assert_true(asset_registry_init(&assets));
    assert_int_equal(ui_document_create_menu(&workspace.document, "presentation"),
                     UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&workspace.document,
        UI_DOCUMENT_ELEMENT_BUTTON, 1U, "play", "PLAY", "play", &button),
        UI_DOCUMENT_OK);
    asset_document_state_mark_saved(&workspace.document.state);
    workspace.saved_document = workspace.document;
    workspace.active = true;
    workspace.has_document = true;
    workspace.mode = UI_MENU_WORKSPACE_PROPERTIES;
    workspace.element_index = 1U;
    workspace.property = UI_MENU_PROPERTY_FOCUS_EFFECT;
    staged_state = workspace.document.state.current_state;
    assert_int_equal(ui_menu_workspace_begin_candidate(
        &workspace, workspace.property), UI_MENU_WORKSPACE_OK);
    assert_int_equal(ui_menu_workspace_adjust_candidate(&workspace, 1),
                     UI_MENU_WORKSPACE_OK);
    grid = grid_create(120, 40);
    assert_non_null(grid);
    assert_true(ui_editor_presentation_render(&workspace, UI_MENU_WORKSPACE_OK,
                                              &assets, &theme, 25.0, grid));
    assert_true(grid_contains_text(grid, "UI SCENE EDITOR"));
    assert_true(grid_contains_text(grid, "candidate:yes"));
    assert_true(grid_contains_text(grid, "Focus Effect"));
    assert_true(grid_contains_text(grid, "center_out"));
    assert_int_equal(workspace.document.state.current_state, staged_state);
    assert_int_equal(workspace.change_count, 0U);
    assert_false(ui_menu_workspace_is_dirty(&workspace));
    grid_destroy(grid);
    asset_registry_clear(&assets);
    ui_menu_workspace_clear(&workspace);
}

static void test_rejects_invalid_arguments(void **state) {
    Grid *grid = grid_create(20, 10);
    (void)state;
    assert_non_null(grid);
    assert_false(ui_editor_presentation_render(
        NULL, UI_MENU_WORKSPACE_OK, NULL, NULL, 0.0, grid));
    grid_destroy(grid);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_renders_workspace_candidate_without_host_state),
        cmocka_unit_test(test_rejects_invalid_arguments)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}