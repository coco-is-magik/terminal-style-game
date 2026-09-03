#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/object_document.h"

static void test_create_validation_and_atomic_output(void **state) {
    AssetRegistry assets;
    PatternCell cell = {'O', 1U};
    char directory[] = "/tmp/tsg_object_doc_XXXXXX";
    char path[1024];
    uint16_t id = 0U;
    FILE *file;
    char content[256] = {0};
    (void)state;
    assert_non_null(mkdtemp(directory));
    assert_true(asset_registry_init(&assets));
    assert_true(asset_registry_set_sprite(&assets, 1U, 1, 1, &cell));
    assets.objects[1] = (ObjectAsset){.name = "existing", .sprite_id = 1U,
        .attributes = OBJECT_ATTRIBUTE_SIMPLE, .loaded = true};
    assert_int_equal(object_document_create_atomic(
        &assets, directory, "bad/name", 1U, 0.0, &id),
        OBJECT_DOCUMENT_INVALID_NAME);
    assert_int_equal(object_document_create_atomic(
        &assets, directory, "existing", 1U, 0.0, &id),
        OBJECT_DOCUMENT_DUPLICATE_NAME);
    assert_int_equal(object_document_create_atomic(
        &assets, directory, "crate", 2U, 0.0, &id),
        OBJECT_DOCUMENT_INVALID_SPRITE);
    assert_int_equal(object_document_create_atomic(
        &assets, directory, "crate", 1U, 1.25, &id), OBJECT_DOCUMENT_OK);
    assert_int_equal(id, 2U);
    snprintf(path, sizeof(path), "%s/2.txt", directory);
    file = fopen(path, "r"); assert_non_null(file);
    assert_true(fread(content, 1U, sizeof(content) - 1U, file) > 0U);
    assert_int_equal(fclose(file), 0);
    assert_non_null(strstr(content, "name=crate\nsprite_id=1\n"));
    assert_non_null(strstr(content, "front_direction=1.25\nattributes=simple\n"));
    assert_true(object_asset_name_has_prefix("Crate", "cr"));
    assert_int_equal(object_asset_find_by_name(&assets, "existing"), 1);
    (void)unlink(path); (void)rmdir(directory);
    asset_registry_clear(&assets);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_create_validation_and_atomic_output)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}