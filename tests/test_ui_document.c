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
    char path[] = "/tmp/tsg_ui_document_XXXXXX";
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
    assert_int_equal(ui_document_save_as(&document, path), UI_DOCUMENT_OK);
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
    assert_int_equal(ui_document_save(&loaded), UI_DOCUMENT_OK);
    before = loaded;
    file = fopen(path, "w");
    assert_non_null(file);
    assert_true(fputs("ui_version=99\n", file) >= 0);
    assert_int_equal(fclose(file), 0);
    assert_int_equal(ui_document_load(&loaded, path), UI_DOCUMENT_PARSE_ERROR);
    assert_memory_equal(&loaded, &before, sizeof(loaded));
    assert_int_equal(unlink(path), 0);
}

static void test_v1_migrates_to_explicit_layout_defaults(void **state) {
    UiDocument document;
    char path[] = "/tmp/tsg_ui_document_v1_XXXXXX";
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
    char path[] = "/tmp/tsg_ui_document_mixed_XXXXXX";
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
    char path[] = "/tmp/tsg_ui_document_v2_XXXXXX";
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

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_create_tree_dirty_and_stable_ids),
        cmocka_unit_test(test_button_ports_export_and_validate_flow),
        cmocka_unit_test(test_rejects_duplicate_names_ports_and_invalid_content),
        cmocka_unit_test(test_button_port_capacity_is_transactional),
        cmocka_unit_test(test_validation_rejects_cycles_and_bad_roots),
        cmocka_unit_test(test_round_trip_save_and_transactional_load_failure)
        ,cmocka_unit_test(test_v1_migrates_to_explicit_layout_defaults)
        ,cmocka_unit_test(test_layout_mutations_reject_invalid_document_without_change)
        ,cmocka_unit_test(test_v1_rejects_v2_fields_on_earlier_element)
        ,cmocka_unit_test(test_v2_migrates_to_native_visual_defaults)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}