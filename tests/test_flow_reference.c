#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "../src/flow_reference.h"

static void build_graph(FlowDocument *document, FlowNodeId *scene,
                        FlowNodeId *menu) {
    FlowEdgeId edge;
    flow_document_init(document);
    assert_int_equal(flow_document_add_node(document, FLOW_NODE_MENU,
                                            "missions", menu), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_add_node(document, FLOW_NODE_SCENE,
                                            "hub", scene), FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(document, 1U, "start", *menu, &edge),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(document, *menu, "play", *scene, &edge),
                     FLOW_DOCUMENT_OK);
    assert_int_equal(flow_document_connect(document, *scene, "exit", *menu, &edge),
                     FLOW_DOCUMENT_OK);
}

static void test_validates_typed_assets_and_ports(void **state) {
    FlowDocument document;
    FlowNodeId scene, menu;
    const char *menu_ports[] = {"play"};
    const char *scene_ports[] = {"exit"};
    FlowReferenceEntry entries[] = {
        {FLOW_NODE_SCENE, "hub", scene_ports, 1U},
        {FLOW_NODE_MENU, "missions", menu_ports, 1U}
    };
    FlowReferenceCatalog catalog = {entries, 2U};
    (void)state;
    build_graph(&document, &scene, &menu);
    assert_int_equal(flow_reference_validate_catalog(&catalog), FLOW_REFERENCE_OK);
    assert_int_equal(flow_reference_validate_document(&document, &catalog),
                     FLOW_REFERENCE_OK);
    assert_ptr_equal(flow_reference_find(&catalog, FLOW_NODE_SCENE, "hub"),
                     &entries[0]);
}

static void test_rejects_missing_type_and_port_references(void **state) {
    FlowDocument document;
    FlowNodeId scene, menu;
    const char *menu_ports[] = {"back"};
    const char *scene_ports[] = {"exit"};
    FlowReferenceEntry entries[] = {
        {FLOW_NODE_SCENE, "hub", scene_ports, 1U},
        {FLOW_NODE_MENU, "missions", menu_ports, 1U}
    };
    FlowReferenceCatalog catalog = {entries, 2U};
    (void)state;
    build_graph(&document, &scene, &menu);
    assert_int_equal(flow_reference_validate_document(&document, &catalog),
                     FLOW_REFERENCE_MISSING_PORT);
    menu_ports[0] = "play";
    entries[0].type = FLOW_NODE_MENU;
    assert_int_equal(flow_reference_validate_document(&document, &catalog),
                     FLOW_REFERENCE_TYPE_MISMATCH);
    entries[0].name = "other";
    assert_int_equal(flow_reference_validate_document(&document, &catalog),
                     FLOW_REFERENCE_MISSING_ASSET);
}

static void test_rejects_duplicate_and_invalid_catalog_data(void **state) {
    const char *duplicate_ports[] = {"exit", "exit"};
    FlowReferenceEntry entries[] = {
        {FLOW_NODE_SCENE, "hub", duplicate_ports, 2U},
        {FLOW_NODE_SCENE, "hub", NULL, 0U}
    };
    FlowReferenceCatalog catalog = {entries, 2U};
    (void)state;
    assert_int_equal(flow_reference_validate_catalog(&catalog),
                     FLOW_REFERENCE_DUPLICATE_PORT);
    entries[0].port_count = 1U;
    assert_int_equal(flow_reference_validate_catalog(&catalog),
                     FLOW_REFERENCE_DUPLICATE_ASSET);
    entries[1].name = "bad/name";
    assert_int_equal(flow_reference_validate_catalog(&catalog),
                     FLOW_REFERENCE_INVALID_ENTRY);
    assert_int_equal(flow_reference_validate_catalog(NULL),
                     FLOW_REFERENCE_INVALID_ARGUMENT);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_validates_typed_assets_and_ports),
        cmocka_unit_test(test_rejects_missing_type_and_port_references),
        cmocka_unit_test(test_rejects_duplicate_and_invalid_catalog_data)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}