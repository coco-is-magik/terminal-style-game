#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/flow_project_catalog.h"
#include "../src/asset_loader.h"
#include "../src/config.h"

static int write_file(const char *path, const char *text) {
    FILE *file = fopen(path, "w");
    if (!file) return -1;
    if (fputs(text, file) < 0 || fclose(file) != 0) return -1;
    return 0;
}

static void test_checked_in_catalog_is_typed_sorted_and_valid(void **state) {
    FlowProjectCatalog catalog;
    AssetRegistry assets;
    const FlowReferenceEntry *menu;
    (void)state;
    config_init_defaults();
    assert_true(asset_registry_init(&assets));
    assert_true(asset_loader_load_registry(&assets, "assets"));
    flow_project_catalog_init(&catalog);
    assert_int_equal(flow_project_catalog_refresh(&catalog, "assets", &assets),
                     FLOW_PROJECT_CATALOG_OK);
    assert_true(catalog.count >= 5U);
    menu = flow_reference_find(flow_project_catalog_reference(&catalog),
                               FLOW_NODE_MENU, "main_menu");
    assert_non_null(menu);
    assert_int_equal(menu->port_count, 1U);
    assert_string_equal(menu->ports[0], "start_game");
    assert_int_equal(flow_reference_validate_catalog(
        flow_project_catalog_reference(&catalog)), FLOW_REFERENCE_OK);
    asset_registry_clear(&assets);
}

static void test_invalid_refresh_preserves_catalog(void **state) {
    FlowProjectCatalog catalog;
    FlowProjectCatalog before;
    AssetRegistry assets;
    char root[] = "/tmp/tsg_flow_catalog_XXXXXX";
    char menus[256];
    char path[256];
    (void)state;
    config_init_defaults();
    assert_non_null(mkdtemp(root));
    assert_true(asset_registry_init(&assets));
    assert_true(asset_loader_load_registry(&assets, "assets"));
    assert_true(snprintf(menus, sizeof(menus), "%s/menus", root) > 0);
    assert_int_equal(mkdir(menus, 0700), 0);
    assert_true(snprintf(path, sizeof(path), "%s/bad.tui", menus) > 0);
    assert_int_equal(write_file(path, "ui_version=99\n"), 0);
    flow_project_catalog_init(&catalog);
    assert_int_equal(flow_project_catalog_refresh(&catalog, "assets", &assets),
                     FLOW_PROJECT_CATALOG_OK);
    before = catalog;
    assert_int_equal(flow_project_catalog_refresh(&catalog, root, &assets),
                     FLOW_PROJECT_CATALOG_INVALID_ASSET);
    assert_memory_equal(&catalog, &before, sizeof(catalog));
    assert_int_equal(unlink(path), 0);
    assert_int_equal(rmdir(menus), 0);
    assert_int_equal(rmdir(root), 0);
    asset_registry_clear(&assets);
}

static void test_staged_menu_overlay_is_failure_atomic(void **state) {
    FlowProjectCatalog catalog;
    FlowProjectCatalog before;
    FlowReferenceEntry entries[1];
    const char *ports[] = {"new_port"};
    const char *duplicate_ports[] = {"same", "same"};
    const FlowReferenceEntry *menu;
    (void)state;
    flow_project_catalog_init(&catalog);
    entries[0] = (FlowReferenceEntry){FLOW_NODE_MENU, "menu", ports, 1U};
    assert_int_equal(flow_project_catalog_overlay_entry(&catalog, &entries[0]),
                     FLOW_PROJECT_CATALOG_OK);
    menu = flow_reference_find(flow_project_catalog_reference(&catalog),
                               FLOW_NODE_MENU, "menu");
    assert_non_null(menu);
    assert_string_equal(menu->ports[0], "new_port");
    before = catalog;
    entries[0] = (FlowReferenceEntry){FLOW_NODE_MENU, "menu", duplicate_ports, 2U};
    assert_int_equal(flow_project_catalog_overlay_entry(&catalog, &entries[0]),
                     FLOW_PROJECT_CATALOG_INVALID_ARGUMENT);
    assert_memory_equal(&catalog, &before, sizeof(catalog));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_checked_in_catalog_is_typed_sorted_and_valid),
        cmocka_unit_test(test_invalid_refresh_preserves_catalog)
        ,cmocka_unit_test(test_staged_menu_overlay_is_failure_atomic)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}