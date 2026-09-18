#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/ui_document.h"
#include "../src/ui_document_internal.h"

static void assert_ui_save_committed(UiDocumentResult result) {
    assert_true(result == UI_DOCUMENT_OK ||
                result == UI_DOCUMENT_OK_DURABILITY_WARNING);
}

static char *read_file_bytes(const char *path) {
    char buffer[8192];
    FILE *file = fopen(path, "rb");
    size_t count;
    char *copy;
    assert_non_null(file);
    count = fread(buffer, 1U, sizeof(buffer) - 1U, file);
    assert_false(ferror(file));
    assert_int_equal(fclose(file), 0);
    buffer[count] = '\0';
    copy = malloc(count + 1U);
    assert_non_null(copy);
    memcpy(copy, buffer, count + 1U);
    return copy;
}

static void build_menu(UiDocument *document, UiElementId *panel,
                       UiElementId *button) {
    UiElementId text;
    assert_int_equal(ui_document_create_menu(document, "missions"), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(document, UI_DOCUMENT_ELEMENT_CONTAINER,
        1U, "panel", "", "", panel), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(document, UI_DOCUMENT_ELEMENT_TEXT,
        *panel, "title", "MISSIONS", "", &text), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(document, UI_DOCUMENT_ELEMENT_BUTTON,
        *panel, "play_button", "PLAY", "play", button), UI_DOCUMENT_OK);
}

static void test_create_tree_dirty_and_stable_ids(void **state) {
    UiDocument document;
    UiElementId panel, button;
    (void)state;
    assert_int_equal(ui_document_create_menu(&document, "missions"), UI_DOCUMENT_OK);
    assert_true(ui_document_is_dirty(&document));
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_CONTAINER,
        1U, "panel", "", "", &panel), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_TEXT,
        panel, "title", "MISSIONS", "", &button), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_BUTTON,
        panel, "play_button", "PLAY", "play", &button), UI_DOCUMENT_OK);
    assert_int_equal(panel, 2U);
    assert_int_equal(button, 4U);
    assert_true(ui_document_is_dirty(&document));
    assert_int_equal(ui_document_validate(&document), UI_DOCUMENT_OK);
    assert_string_equal(ui_document_find_element(&document, button)->content, "PLAY");
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_TEXT,
        button, "bad_child", "x", "", &panel), UI_DOCUMENT_PARENT_NOT_CONTAINER);
    assert_int_equal(document.element_count, 4U);
}

static void test_button_port_capacity_is_transactional(void **state) {
    UiDocument document;
    UiElementId out;
    char name[32];
    char port[32];
    size_t i;
    (void)state;
    assert_int_equal(ui_document_create_menu(&document, "many_buttons"), UI_DOCUMENT_OK);
    for (i = 0U; i < FLOW_REFERENCE_MAX_PORTS; i++) {
        assert_true(snprintf(name, sizeof(name), "button_%zu", i) > 0);
        assert_true(snprintf(port, sizeof(port), "port_%zu", i) > 0);
        assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_BUTTON,
            1U, name, "BUTTON", port, &out), UI_DOCUMENT_OK);
    }
    assert_int_equal(document.element_count, FLOW_REFERENCE_MAX_PORTS + 1U);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_BUTTON,
        1U, "overflow", "BUTTON", "overflow", &out), UI_DOCUMENT_TOO_MANY_PORTS);
    assert_int_equal(out, 0U);
    assert_int_equal(document.element_count, FLOW_REFERENCE_MAX_PORTS + 1U);
}

static void test_button_ports_export_and_validate_flow(void **state) {
    UiDocument document;
    UiElementId panel, button, second;
    UiFlowReferenceView view;
    FlowDocument flow;
    FlowNodeId menu;
    FlowEdgeId edge;
    FlowReferenceCatalog catalog;
    (void)state;
    build_menu(&document, &panel, &button);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_BUTTON,
        panel, "back_button", "BACK", "back", &second), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_build_flow_reference(&document, &view), UI_DOCUMENT_OK);
    assert_int_equal(view.entry.type, FLOW_NODE_MENU);
    assert_string_equal(view.entry.name, "missions");
    assert_int_equal(view.entry.port_count, 2U);
    assert_string_equal(view.entry.ports[0], "play");
    assert_string_equal(view.entry.ports[1], "back");
    catalog = (FlowReferenceCatalog){&view.entry, 1U};
    flow_document_init(&flow);
    assert_int_equal(flow_document_add_node(&flow, FLOW_NODE_MENU, "missions", &menu),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(&flow, 1U, "start", menu, &edge),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_reference_validate_document(&flow, &catalog), FLOW_REFERENCE_OK);
}

static void test_rejects_duplicate_names_ports_and_invalid_content(void **state) {
    UiDocument document;
    UiElementId panel, button, out;
    (void)state;
    build_menu(&document, &panel, &button);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_TEXT,
        panel, "title", "OTHER", "", &out), UI_DOCUMENT_DUPLICATE_NAME);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_BUTTON,
        panel, "other", "OTHER", "play", &out), UI_DOCUMENT_DUPLICATE_PORT);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_TEXT,
        panel, "bad", "line\nbreak", "", &out), UI_DOCUMENT_INVALID_CONTENT);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_TEXT,
        99U, "orphan", "X", "", &out), UI_DOCUMENT_MISSING_PARENT);
    assert_int_equal(document.element_count, 4U);
}

static void test_validation_rejects_cycles_and_bad_roots(void **state) {
    UiDocument document;
    UiElementId panel, button;
    (void)state;
    build_menu(&document, &panel, &button);
    document.elements[0].parent_id = panel;
    document.elements[1].parent_id = 1U;
    assert_int_equal(ui_document_validate(&document), UI_DOCUMENT_PARENT_CYCLE);
    document.elements[0].parent_id = 0U;
    document.elements[1].parent_id = panel;
    assert_int_equal(ui_document_validate(&document), UI_DOCUMENT_PARENT_CYCLE);
    document.elements[1].parent_id = 1U;
    memcpy(document.elements[0].name, "not_root", sizeof("not_root"));
    assert_int_equal(ui_document_validate(&document), UI_DOCUMENT_INVALID_ROOT);
}

static void test_round_trip_save_and_transactional_load_failure(void **state) {
    UiDocument document;
    UiDocument loaded;
    UiDocument before;
    UiElementId panel, button;
    char path[] = "build/tsg_ui_document_XXXXXX";
    int fd;
    FILE *file;
    (void)state;
    fd = mkstemp(path);
    assert_true(fd >= 0);
    assert_int_equal(close(fd), 0);
    build_menu(&document, &panel, &button);
    assert_int_equal(ui_document_set_design_size(&document, 120, 40), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_layout(&document, button,
        (UiDocumentLayout){-2, 3, 12, 2, UI_DOCUMENT_ANCHOR_END,
                           UI_DOCUMENT_ANCHOR_CENTER, 125}), UI_DOCUMENT_OK);
    {
        UiDocumentVisual visual = document.elements[3].visual;
        visual.border_enabled = true;
        visual.border_glyph = '*';
        visual.align = UI_DOCUMENT_ALIGN_RIGHT;
        assert_int_equal(ui_document_set_visual(&document, button, visual),
                         UI_DOCUMENT_OK);
    }
    assert_ui_save_committed(ui_document_save_as(&document, path));
    assert_false(ui_document_is_dirty(&document));
    memset(&loaded, 0x5a, sizeof(loaded));
    assert_int_equal(ui_document_load(&loaded, path), UI_DOCUMENT_OK);
    assert_string_equal(loaded.name, "missions");
    assert_int_equal(loaded.element_count, 4U);
    assert_int_equal(loaded.design_width, 120);
    assert_int_equal(ui_document_find_element(&loaded, button)->layout.scale_percent,
                     125);
    assert_true(ui_document_find_element(&loaded, button)->visual.border_enabled);
    assert_int_equal(ui_document_find_element(&loaded, button)->visual.border_glyph,
                     '*');
    assert_false(ui_document_is_dirty(&loaded));
    assert_ui_save_committed(ui_document_save(&loaded));
    before = loaded;
    file = fopen(path, "w");
    assert_non_null(file);
    assert_true(fputs("ui_version=99\n", file) >= 0);
    assert_int_equal(fclose(file), 0);
    assert_int_equal(ui_document_load(&loaded, path), UI_DOCUMENT_PARSE_ERROR);
    assert_memory_equal(&loaded, &before, sizeof(loaded));
    assert_int_equal(unlink(path), 0);
}

static void test_save_commit_boundaries_preserve_bytes_and_identity(void **state) {
    const UiDocumentSaveFault failures[] = {
        UI_DOCUMENT_SAVE_FAULT_SYNC,
        UI_DOCUMENT_SAVE_FAULT_REPLACE
    };
    UiDocument baseline;
    UiElementId panel;
    UiElementId button;
    char baseline_path[] = "build/tsg_ui_document_baseline_XXXXXX";
    char destination[] = "build/tsg_ui_document_boundary_XXXXXX";
    int baseline_fd = mkstemp(baseline_path);
    int destination_fd = mkstemp(destination);
    char *expected;
    (void)state;
    assert_true(baseline_fd >= 0);
    assert_true(destination_fd >= 0);
    assert_int_equal(close(baseline_fd), 0);
    assert_int_equal(close(destination_fd), 0);
    build_menu(&baseline, &panel, &button);
    assert_ui_save_committed(ui_document_save_as(&baseline, baseline_path));
    expected = read_file_bytes(baseline_path);
    assert_true(strncmp(expected,
        "ui_version=4\nkind=ui_scene\nrole=screen\n",
        strlen("ui_version=4\nkind=ui_scene\nrole=screen\n")) == 0);
    assert_true(expected[strlen(expected) - 1U] == '\n');
    for (size_t i = 0U; i < sizeof(failures) / sizeof(failures[0]); i++) {
        UiDocument document;
        UiDocument before;
        FILE *file = fopen(destination, "wb");
        char *actual;
        assert_non_null(file);
        assert_true(fputs("old ui bytes\n", file) >= 0);
        assert_int_equal(fclose(file), 0);
        build_menu(&document, &panel, &button);
        before = document;
        assert_int_equal(ui_document_internal_save_as(
                             &document, destination, failures[i]),
                         UI_DOCUMENT_IO_ERROR);
        assert_memory_equal(&document, &before, sizeof(document));
        actual = read_file_bytes(destination);
        assert_string_equal(actual, "old ui bytes\n");
        free(actual);
    }
    {
        UiDocument document;
        char *actual;
        build_menu(&document, &panel, &button);
        assert_int_equal(ui_document_internal_save_as(
                             &document, destination,
                             UI_DOCUMENT_SAVE_FAULT_DURABILITY),
                         UI_DOCUMENT_OK_DURABILITY_WARNING);
        assert_true(ui_document_result_is_committed(UI_DOCUMENT_OK));
        assert_true(ui_document_result_is_committed(
            UI_DOCUMENT_OK_DURABILITY_WARNING));
        assert_false(ui_document_result_is_committed(UI_DOCUMENT_IO_ERROR));
        assert_false(ui_document_is_dirty(&document));
        assert_string_equal(document.path, destination);
        actual = read_file_bytes(destination);
        assert_string_equal(actual, expected);
        free(actual);
    }
    free(expected);
    assert_int_equal(unlink(destination), 0);
    assert_int_equal(unlink(baseline_path), 0);
}

static void test_v1_migrates_to_explicit_layout_defaults(void **state) {
    UiDocument document;
    char path[] = "build/tsg_ui_document_v1_XXXXXX";
    int fd = mkstemp(path);
    FILE *file;
    const UiDocumentElement *button;
    (void)state;
    assert_true(fd >= 0);
    file = fdopen(fd, "w");
    assert_non_null(file);
    assert_true(fputs("ui_version=1\nkind=menu\nname=old\nnext_element_id=3\n"
        "[element]\nid=1\nparent=0\ntype=container\nname=root\ncontent=\nport=\n"
        "[element]\nid=2\nparent=1\ntype=button\nname=play\ncontent=PLAY\nport=play\n",
        file) >= 0);
    assert_int_equal(fclose(file), 0);
    assert_int_equal(ui_document_load(&document, path), UI_DOCUMENT_OK);
    assert_int_equal(document.design_width, 80);
    assert_int_equal(document.design_height, 25);
    assert_int_equal(document.elements[0].layout.horizontal_anchor,
                     UI_DOCUMENT_ANCHOR_STRETCH);
    button = ui_document_find_element(&document, 2U);
    assert_non_null(button);
    assert_int_equal(button->layout.width, 1);
    assert_int_equal(button->layout.height, 1);
    assert_int_equal(button->layout.scale_percent, 100);
    assert_int_equal(unlink(path), 0);
}

static void test_layout_mutations_reject_invalid_document_without_change(void **state) {
    UiDocument document;
    UiDocument before;
    UiElementId panel, button;
    (void)state;
    build_menu(&document, &panel, &button);
    document.elements[0].parent_id = panel;
    before = document;
    assert_int_equal(ui_document_set_design_size(&document, 100, 30),
                     UI_DOCUMENT_INVALID_ARGUMENT);
    assert_memory_equal(&document, &before, sizeof(document));
    assert_int_equal(ui_document_set_layout(&document, button,
        (UiDocumentLayout){0, 0, 5, 1, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}),
        UI_DOCUMENT_INVALID_ARGUMENT);
    assert_memory_equal(&document, &before, sizeof(document));
}

static void test_v1_rejects_v2_fields_on_earlier_element(void **state) {
    UiDocument document;
    char path[] = "build/tsg_ui_document_mixed_XXXXXX";
    int fd = mkstemp(path);
    FILE *file;
    (void)state;
    assert_true(fd >= 0);
    file = fdopen(fd, "w");
    assert_non_null(file);
    assert_true(fputs("ui_version=1\nkind=menu\nname=mixed\nnext_element_id=3\n"
        "[element]\nid=1\nparent=0\ntype=container\nname=root\ncontent=\nport=\n"
        "x=0\n"
        "[element]\nid=2\nparent=1\ntype=text\nname=label\ncontent=X\nport=\n",
        file) >= 0);
    assert_int_equal(fclose(file), 0);
    ui_document_init(&document);
    assert_int_equal(ui_document_load(&document, path), UI_DOCUMENT_PARSE_ERROR);
    assert_int_equal(unlink(path), 0);
}

static void test_v2_migrates_to_native_visual_defaults(void **state) {
    UiDocument document;
    char path[] = "build/tsg_ui_document_v2_XXXXXX";
    int fd = mkstemp(path);
    FILE *file;
    (void)state;
    assert_true(fd >= 0);
    file = fdopen(fd, "w");
    assert_non_null(file);
    assert_true(fputs("ui_version=2\nkind=menu\nname=old2\n"
        "design_width=80\ndesign_height=25\nnext_element_id=2\n"
        "[element]\nid=1\nparent=0\ntype=container\nname=root\ncontent=\nport=\n"
        "x=0\ny=0\nwidth=80\nheight=25\nh_anchor=stretch\nv_anchor=stretch\nscale=100\n",
        file) >= 0);
    assert_int_equal(fclose(file), 0);
    assert_int_equal(ui_document_load(&document, path), UI_DOCUMENT_OK);
    assert_int_equal(document.elements[0].visual.mode, UI_DOCUMENT_VISUAL_NATIVE);
    assert_true(document.elements[0].visual.fill_enabled);
    assert_true(document.elements[0].visual.visible_by_default);
    assert_int_equal(unlink(path), 0);
}

static void test_v3_rejects_invalid_color_transactionally(void **state) {
    UiDocument document;
    UiDocument before;
    char path[] = "build/tsg_ui_document_color_XXXXXX";
    int fd = mkstemp(path);
    FILE *file;
    (void)state;

    assert_true(fd >= 0);
    file = fdopen(fd, "w");
    assert_non_null(file);
    assert_true(fputs(
        "ui_version=3\nkind=menu\nname=bad_color\n"
        "design_width=80\ndesign_height=25\nnext_element_id=2\n"
        "[element]\nid=1\nparent=0\ntype=container\nname=root\ncontent=\nport=\n"
        "x=0\ny=0\nwidth=80\nheight=25\nh_anchor=stretch\nv_anchor=stretch\n"
        "scale=100\nvisual=native\nfg=256,0,0,255\nbg=0,0,0,255\n"
        "fill_enabled=1\nfill_glyph=32\nborder_enabled=0\nborder_glyph=35\n"
        "sprite_id=0\nalign=left\nvisible=1\n",
        file) >= 0);
    assert_int_equal(fclose(file), 0);

    assert_int_equal(ui_document_create_menu(&document, "preserved"), UI_DOCUMENT_OK);
    before = document;
    assert_int_equal(ui_document_load(&document, path), UI_DOCUMENT_PARSE_ERROR);
    assert_memory_equal(&document, &before, sizeof(document));
    assert_int_equal(unlink(path), 0);
}

static void test_content_port_and_subtree_mutations_are_transactional(void **state) {
    UiDocument document;
    UiDocument before;
    UiElementId panel;
    UiElementId button;
    UiElementId child;
    (void)state;
    build_menu(&document, &panel, &button);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_TEXT,
        panel, "child", "CHILD", "", &child), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_content(&document, button, "BEGIN"), UI_DOCUMENT_OK);
    assert_string_equal(ui_document_find_element(&document, button)->content, "BEGIN");
    assert_int_equal(ui_document_set_flow_port(&document, button, "begin"), UI_DOCUMENT_OK);
    assert_string_equal(ui_document_find_element(&document, button)->flow_port, "begin");
    document.elements[3].binding = UI_DOCUMENT_BINDING_NONE;
    document.elements[3].flow_port[0] = '\0';
    assert_int_equal(ui_document_set_flow_port(&document, button, "restored"), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_find_element(&document, button)->binding,
                     UI_DOCUMENT_BINDING_FLOW);
    assert_string_equal(ui_document_find_element(&document, button)->flow_port, "restored");
    before = document;
    assert_int_equal(ui_document_set_content(&document, panel, "BAD"),
                     UI_DOCUMENT_INVALID_CONTENT);
    assert_memory_equal(&document, &before, sizeof(document));
    assert_int_equal(ui_document_set_flow_port(&document, child, "begin"),
                     UI_DOCUMENT_INVALID_PORT);
    assert_memory_equal(&document, &before, sizeof(document));
    assert_int_equal(ui_document_remove_subtree(&document, 1U), UI_DOCUMENT_INVALID_ROOT);
    assert_memory_equal(&document, &before, sizeof(document));
    assert_int_equal(ui_document_remove_subtree(&document, panel), UI_DOCUMENT_OK);
    assert_int_equal(document.element_count, 1U);
    assert_null(ui_document_find_element(&document, panel));
    assert_null(ui_document_find_element(&document, button));
    assert_null(ui_document_find_element(&document, child));
    assert_int_equal(ui_document_validate(&document), UI_DOCUMENT_OK);
}

static void test_hierarchy_mutations_are_transactional_and_preserve_subtrees(void **state) {
    UiDocument document;
    UiDocument before;
    UiElementId first;
    UiElementId first_child;
    UiElementId second;
    UiElementId second_child;
    UiElementId nested;
    DocumentStateId state_id;
    (void)state;
    assert_int_equal(ui_document_create_menu(&document, "hierarchy"), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_CONTAINER,
        1U, "first", "", "", &first), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_CONTAINER,
        1U, "second", "", "", &second), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_TEXT,
        first, "first_child", "A", "", &first_child), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_TEXT,
        second, "second_child", "B", "", &second_child), UI_DOCUMENT_OK);

    state_id = document.state.current_state;
    assert_int_equal(ui_document_rename_element(&document, first_child, "renamed"),
                     UI_DOCUMENT_OK);
    assert_string_equal(ui_document_find_element(&document, first_child)->name, "renamed");
    assert_true(document.state.current_state > state_id);
    before = document;
    assert_int_equal(ui_document_rename_element(&document, first_child, "second"),
                     UI_DOCUMENT_DUPLICATE_NAME);
    assert_memory_equal(&document, &before, sizeof(document));
    assert_int_equal(ui_document_rename_element(&document, 1U, "new_root"),
                     UI_DOCUMENT_INVALID_ROOT);
    assert_memory_equal(&document, &before, sizeof(document));

    assert_int_equal(ui_document_move_subtree_later(&document, first), UI_DOCUMENT_OK);
    assert_int_equal(document.elements[1].id, second);
    assert_int_equal(document.elements[2].id, second_child);
    assert_int_equal(document.elements[3].id, first);
    assert_int_equal(document.elements[4].id, first_child);
    assert_int_equal(ui_document_move_subtree_earlier(&document, first), UI_DOCUMENT_OK);
    assert_int_equal(document.elements[1].id, first);
    assert_int_equal(document.elements[2].id, first_child);
    assert_int_equal(document.elements[3].id, second);
    assert_int_equal(document.elements[4].id, second_child);
    before = document;
    assert_int_equal(ui_document_move_subtree_earlier(&document, first),
                     UI_DOCUMENT_INVALID_ARGUMENT);
    assert_memory_equal(&document, &before, sizeof(document));

    assert_int_equal(ui_document_reparent_subtree(&document, first_child, second),
                     UI_DOCUMENT_OK);
    assert_int_equal(ui_document_find_element(&document, first_child)->parent_id, second);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_CONTAINER,
        first, "nested", "", "", &nested), UI_DOCUMENT_OK);
    before = document;
    assert_int_equal(ui_document_reparent_subtree(&document, first, nested),
                     UI_DOCUMENT_PARENT_CYCLE);
    assert_memory_equal(&document, &before, sizeof(document));
    assert_int_equal(ui_document_reparent_subtree(&document, second, first_child),
                     UI_DOCUMENT_PARENT_NOT_CONTAINER);
    assert_memory_equal(&document, &before, sizeof(document));
    assert_int_equal(ui_document_reparent_subtree(&document, second, second_child),
                     UI_DOCUMENT_PARENT_NOT_CONTAINER);
    assert_memory_equal(&document, &before, sizeof(document));
    assert_int_equal(ui_document_reparent_subtree(&document, second, second),
                     UI_DOCUMENT_PARENT_CYCLE);
    assert_memory_equal(&document, &before, sizeof(document));
    assert_int_equal(ui_document_validate(&document), UI_DOCUMENT_OK);
}

static void test_v3_migrates_binding_role_and_effect_defaults(void **state) {
    UiDocument document;
    const UiDocumentElement *button;
    (void)state;
    ui_document_init(&document);
    assert_int_equal(ui_document_load(&document, "assets/menus/main_menu.tui"),
                     UI_DOCUMENT_OK);
    assert_int_equal(document.role, UI_DOCUMENT_ROLE_SCREEN);
    button = ui_document_find_element(&document, 2U);
    assert_non_null(button);
    assert_int_equal(button->binding, UI_DOCUMENT_BINDING_FLOW);
    assert_string_equal(button->flow_port, "start_game");
    assert_string_equal(button->system_action, "");
    assert_string_equal(button->entry_effect, "none");
    assert_string_equal(button->exit_effect, "none");
    assert_string_equal(button->focus_effect, "none");
    assert_string_equal(button->activate_effect, "none");
}

static void test_v4_system_binding_effects_round_trip_and_do_not_export(void **state) {
    UiDocument document;
    UiDocument loaded;
    UiElementId panel;
    UiElementId button;
    UiDocumentElement *element;
    UiFlowReferenceView view;
    char path[] = "build/tsg_ui_document_v4_XXXXXX";
    int fd = mkstemp(path);
    char *bytes;
    (void)state;
    assert_true(fd >= 0);
    assert_int_equal(close(fd), 0);
    build_menu(&document, &panel, &button);
    document.role = UI_DOCUMENT_ROLE_OVERLAY;
    element = &document.elements[3];
    element->binding = UI_DOCUMENT_BINDING_SYSTEM;
    element->flow_port[0] = '\0';
    memcpy(element->system_action, "open_editor", sizeof("open_editor"));
    memcpy(element->entry_effect, "center_out", sizeof("center_out"));
    memcpy(element->exit_effect, "local_glitch", sizeof("local_glitch"));
    memcpy(element->focus_effect, "focus_pulse", sizeof("focus_pulse"));
    memcpy(element->activate_effect, "perimeter_burst", sizeof("perimeter_burst"));
    assert_int_equal(ui_document_validate(&document), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_build_flow_reference(&document, &view), UI_DOCUMENT_OK);
    assert_int_equal(view.entry.port_count, 0U);
    assert_ui_save_committed(ui_document_save_as(&document, path));
    bytes = read_file_bytes(path);
    assert_non_null(strstr(bytes, "ui_version=4\nkind=ui_scene\nrole=overlay\n"));
    assert_non_null(strstr(bytes, "binding=system\nport=\naction=open_editor\n"));
    assert_non_null(strstr(bytes, "entry_effect=center_out\nexit_effect=local_glitch\n"));
    free(bytes);
    ui_document_init(&loaded);
    assert_int_equal(ui_document_load(&loaded, path), UI_DOCUMENT_OK);
    element = &loaded.elements[3];
    assert_int_equal(loaded.role, UI_DOCUMENT_ROLE_OVERLAY);
    assert_int_equal(element->binding, UI_DOCUMENT_BINDING_SYSTEM);
    assert_string_equal(element->system_action, "open_editor");
    assert_string_equal(element->focus_effect, "focus_pulse");
    assert_string_equal(element->activate_effect, "perimeter_burst");
    assert_int_equal(unlink(path), 0);
}

static void test_animation_add_target_mutations_and_round_trip(void **state) {
    UiDocument document;
    UiDocument loaded;
    UiElementId panel;
    UiElementId button;
    UiElementId anim;
    const UiDocumentElement *element;
    UiDocumentResult result;
    char path[] = "build/tsg_ui_document_anim_XXXXXX";
    int fd = mkstemp(path);
    char *bytes;
    (void)state;
    assert_true(fd >= 0);
    assert_int_equal(close(fd), 0);
    build_menu(&document, &panel, &button);

    assert_int_equal(ui_document_add_animation(&document, 0U, "bad_anim", &anim),
                     UI_DOCUMENT_INVALID_ARGUMENT);
    assert_int_equal(ui_document_add_animation(&document, 999U, "bad_anim", &anim),
                     UI_DOCUMENT_INVALID_ARGUMENT);
    assert_int_equal(ui_document_add_animation(&document, panel, "bad name", &anim),
                     UI_DOCUMENT_INVALID_NAME);

    assert_int_equal(ui_document_add_animation(&document, panel, "panel_intro", &anim),
                     UI_DOCUMENT_OK);
    element = ui_document_find_element(&document, anim);
    assert_non_null(element);
    assert_int_equal(element->parent_id, 0U);
    assert_int_equal(element->layout.width, 0);
    assert_int_equal(element->layout.height, 0);
    assert_int_equal(element->animation.preset,
                     UI_DOCUMENT_ANIMATION_PRESET_CENTER_OUT);
    assert_int_equal(element->animation.orientation,
                     UI_DOCUMENT_ANIMATION_ORIENTATION_RADIAL);
    assert_int_equal(anim, 5U);
    element = ui_document_find_element(&document, anim);
    assert_non_null(element);
    assert_int_equal(element->type, UI_DOCUMENT_ELEMENT_ANIMATION);
    assert_int_equal(element->animation.target_id, panel);
    assert_int_equal(element->animation.trigger, UI_DOCUMENT_ANIMATION_TRIGGER_CONTEXT_ENTER);
    assert_false(element->animation.loop);
    assert_false(element->animation.randomize);

    result = ui_document_set_animation_target(&document, anim, button);
    assert_int_equal(result, UI_DOCUMENT_OK);
    assert_int_equal(ui_document_find_element(&document, anim)->animation.target_id, button);

    assert_int_equal(ui_document_set_animation_target(&document, anim, 0U),
                     UI_DOCUMENT_INVALID_ARGUMENT);
    assert_int_equal(ui_document_set_animation_target(&document, anim, 999U),
                     UI_DOCUMENT_INVALID_ARGUMENT);
    assert_int_equal(ui_document_set_animation_target(&document, panel, button),
                     UI_DOCUMENT_INVALID_ARGUMENT);

    assert_int_equal(ui_document_set_animation_fields(
        &document, anim,
        UI_DOCUMENT_ANIMATION_PRESET_CENTER_OUT,
        UI_DOCUMENT_ANIMATION_TRIGGER_FOCUS,
        UI_DOCUMENT_ANIMATION_ORIENTATION_VERTICAL,
        true, true), UI_DOCUMENT_OK);
    element = ui_document_find_element(&document, anim);
    assert_int_equal(element->animation.preset, UI_DOCUMENT_ANIMATION_PRESET_CENTER_OUT);
    assert_int_equal(element->animation.trigger, UI_DOCUMENT_ANIMATION_TRIGGER_FOCUS);
    assert_int_equal(element->animation.orientation, UI_DOCUMENT_ANIMATION_ORIENTATION_VERTICAL);
    assert_true(element->animation.loop);
    assert_true(element->animation.randomize);

    assert_int_equal(ui_document_set_animation_fields(
        &document, anim,
        (UiDocumentAnimationPreset)99,
        UI_DOCUMENT_ANIMATION_TRIGGER_FOCUS,
        UI_DOCUMENT_ANIMATION_ORIENTATION_VERTICAL,
        true, true), UI_DOCUMENT_INVALID_ARGUMENT);
    assert_int_equal(ui_document_set_animation_fields(
        &document, 999U,
        UI_DOCUMENT_ANIMATION_PRESET_CENTER_OUT,
        UI_DOCUMENT_ANIMATION_TRIGGER_FOCUS,
        UI_DOCUMENT_ANIMATION_ORIENTATION_VERTICAL,
        true, true), UI_DOCUMENT_INVALID_ARGUMENT);

    assert_ui_save_committed(ui_document_save_as(&document, path));
    bytes = read_file_bytes(path);
    assert_non_null(strstr(bytes, "type=animation\n"));
    assert_non_null(strstr(bytes, "animation_preset=center_out\n"));
    assert_non_null(strstr(bytes, "animation_target=4\n"));
    assert_non_null(strstr(bytes, "animation_trigger=focus\n"));
    assert_non_null(strstr(bytes, "animation_orientation=vertical\n"));
    assert_non_null(strstr(bytes, "animation_loop=1\n"));
    assert_non_null(strstr(bytes, "animation_randomize=1\n"));
    free(bytes);

    ui_document_init(&loaded);
    assert_int_equal(ui_document_load(&loaded, path), UI_DOCUMENT_OK);
    element = ui_document_find_element(&loaded, anim);
    assert_non_null(element);
    assert_int_equal(element->type, UI_DOCUMENT_ELEMENT_ANIMATION);
    assert_int_equal(element->animation.target_id, button);
    assert_int_equal(element->animation.preset, UI_DOCUMENT_ANIMATION_PRESET_CENTER_OUT);
    assert_int_equal(element->animation.trigger, UI_DOCUMENT_ANIMATION_TRIGGER_FOCUS);
    assert_int_equal(element->animation.orientation, UI_DOCUMENT_ANIMATION_ORIENTATION_VERTICAL);
    assert_true(element->animation.loop);
    assert_true(element->animation.randomize);
    assert_int_equal(unlink(path), 0);
}

static void test_animation_validation_rejects_bad_target_and_layout(void **state) {
    UiDocument document;
    UiDocument before;
    UiElementId panel;
    UiElementId button;
    UiElementId anim;
    (void)state;
    build_menu(&document, &panel, &button);
    assert_int_equal(ui_document_add_animation(&document, panel, "panel_intro", &anim),
                     UI_DOCUMENT_OK);

    before = document;
    document.elements[4].animation.target_id = anim;
    assert_int_equal(ui_document_validate(&document), UI_DOCUMENT_INVALID_ARGUMENT);
    document = before;

    document.elements[4].layout.horizontal_anchor = UI_DOCUMENT_ANCHOR_STRETCH;
    assert_int_equal(ui_document_validate(&document), UI_DOCUMENT_INVALID_LAYOUT);
    document = before;

    document.elements[1].animation.target_id = button;
    assert_int_equal(ui_document_validate(&document), UI_DOCUMENT_INVALID_ARGUMENT);
}

static void test_v4_invalid_binding_and_effect_reject(void **state) {
    UiDocument document;
    UiDocument before;
    UiElementId panel;
    UiElementId button;
    (void)state;
    build_menu(&document, &panel, &button);
    before = document;
    document.elements[3].binding = UI_DOCUMENT_BINDING_SYSTEM;
    memcpy(document.elements[3].system_action, "unknown", sizeof("unknown"));
    assert_int_equal(ui_document_validate(&document), UI_DOCUMENT_INVALID_BINDING);
    document = before;
    memcpy(document.elements[1].focus_effect, "focus_pulse", sizeof("focus_pulse"));
    assert_int_equal(ui_document_validate(&document), UI_DOCUMENT_INVALID_BINDING);
    document = before;
    memcpy(document.elements[3].entry_effect, "unknown", sizeof("unknown"));
    assert_int_equal(ui_document_validate(&document), UI_DOCUMENT_INVALID_EFFECT);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_create_tree_dirty_and_stable_ids),
        cmocka_unit_test(test_button_ports_export_and_validate_flow),
        cmocka_unit_test(test_rejects_duplicate_names_ports_and_invalid_content),
        cmocka_unit_test(test_button_port_capacity_is_transactional),
        cmocka_unit_test(test_validation_rejects_cycles_and_bad_roots),
        cmocka_unit_test(test_round_trip_save_and_transactional_load_failure)
        ,cmocka_unit_test(test_save_commit_boundaries_preserve_bytes_and_identity)
        ,cmocka_unit_test(test_v1_migrates_to_explicit_layout_defaults)
        ,cmocka_unit_test(test_layout_mutations_reject_invalid_document_without_change)
        ,cmocka_unit_test(test_v1_rejects_v2_fields_on_earlier_element)
        ,cmocka_unit_test(test_v2_migrates_to_native_visual_defaults)
        ,cmocka_unit_test(test_v3_rejects_invalid_color_transactionally)
        ,cmocka_unit_test(test_content_port_and_subtree_mutations_are_transactional)
        ,cmocka_unit_test(test_hierarchy_mutations_are_transactional_and_preserve_subtrees)
        ,cmocka_unit_test(test_v3_migrates_binding_role_and_effect_defaults)
        ,cmocka_unit_test(test_v4_system_binding_effects_round_trip_and_do_not_export)
        ,cmocka_unit_test(test_v4_invalid_binding_and_effect_reject)
        ,cmocka_unit_test(test_animation_add_target_mutations_and_round_trip)
        ,cmocka_unit_test(test_animation_validation_rejects_bad_target_and_layout)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
