/**
 * test_scene_document.c — SceneDocument lifecycle, load, save, dirty state
 *
 * All file I/O uses a test-created temporary directory. Checked-in assets
 * under assets/maps/ are never written.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>

#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/scene_document.h"
#include "../src/scene_document_internal.h"
#include "../src/config.h"

/* ===================================================================
 *  Temp directory helpers
 * =================================================================== */

static char g_tmpdir[] = "/tmp/tsg_scene_doc_XXXXXX";
static int g_tmpdir_ready = 0;

static int make_tmpdir(void) {
    memcpy(g_tmpdir, "/tmp/tsg_scene_doc_XXXXXX", sizeof("/tmp/tsg_scene_doc_XXXXXX"));
    if (!mkdtemp(g_tmpdir)) return -1;
    g_tmpdir_ready = 1;
    return 0;
}

static void path_in_tmpdir(char *out, size_t out_sz, const char *name) {
    snprintf(out, out_sz, "%s/%s", g_tmpdir, name);
}

static int write_text_file(const char *path, const char *text) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    size_t len = strlen(text);
    if (fwrite(text, 1, len, fp) != len) {
        fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0) return -1;
    return 0;
}

static char *read_text_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
    long size = ftell(fp);
    if (size < 0) { fclose(fp); return NULL; }
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return NULL; }
    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(fp); return NULL; }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

static int count_files_with_prefix(const char *dir, const char *prefix) {
    DIR *d = opendir(dir);
    if (!d) return -1;
    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strncmp(ent->d_name, prefix, strlen(prefix)) == 0) count++;
    }
    closedir(d);
    return count;
}

static void rm_rf_tmpdir(void) {
    if (!g_tmpdir_ready) return;
    DIR *d = opendir(g_tmpdir);
    if (d) {
        struct dirent *ent;
        char path[512];
        while ((ent = readdir(d)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;
            snprintf(path, sizeof(path), "%s/%s", g_tmpdir, ent->d_name);
            remove(path);
        }
        closedir(d);
    }
    rmdir(g_tmpdir);
    g_tmpdir_ready = 0;
}

static int group_setup(void **state) {
    (void)state;
    config_init_defaults();
    if (make_tmpdir() != 0) return -1;
    return 0;
}

static int group_teardown(void **state) {
    (void)state;
    rm_rf_tmpdir();
    return 0;
}

/* ===================================================================
 *  Tests
 * =================================================================== */

static void test_init_destroy_empty(void **state) {
    (void)state;
    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(doc.map.width, 0);
    assert_int_equal(doc.map.height, 0);
    assert_null(doc.map.cells);
    assert_null(doc.map.light_map);
    assert_null(doc.path);
    assert_int_equal(doc.current_state, 0);
    assert_int_equal(doc.saved_state, 0);
    assert_false(scene_document_is_dirty(&doc));
    scene_document_destroy(&doc);
    scene_document_destroy(&doc); /* idempotent */
}

static void test_load_valid_fixture(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "map_ok.txt");
    assert_int_equal(write_text_file(path, "111\n101\n111\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);
    assert_int_equal(doc.map.width, 3);
    assert_int_equal(doc.map.height, 3);
    assert_int_equal(doc.current_state, 1);
    assert_int_equal(doc.saved_state, 1);
    assert_false(scene_document_is_dirty(&doc));
    assert_non_null(doc.path);
    assert_string_equal(doc.path, path);

    MaterialId mat = -1;
    WallMaterialRef ref = {1, 1};
    assert_true(scene_document_get_wall_material(&doc, ref, &mat));
    assert_int_equal(mat, 0);

    ref.map_x = 0;
    ref.map_y = 0;
    assert_true(scene_document_get_wall_material(&doc, ref, &mat));
    assert_int_equal(mat, 1);

    scene_document_destroy(&doc);
}

static void test_repeated_load_no_leak(void **state) {
    (void)state;
    char path_a[512], path_b[512];
    path_in_tmpdir(path_a, sizeof(path_a), "map_a.txt");
    path_in_tmpdir(path_b, sizeof(path_b), "map_b.txt");
    assert_int_equal(write_text_file(path_a, "11\n11\n"), 0);
    assert_int_equal(write_text_file(path_b, "222\n202\n222\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path_a), SCENE_LOAD_OK);
    assert_int_equal(doc.map.width, 2);
    assert_int_equal(scene_document_load(&doc, path_b), SCENE_LOAD_OK);
    assert_int_equal(doc.map.width, 3);
    assert_int_equal(doc.map.height, 3);
    assert_string_equal(doc.path, path_b);
    assert_int_equal(doc.current_state, 1);
    assert_int_equal(doc.saved_state, 1);

    MaterialId mat = -1;
    WallMaterialRef ref = {1, 1};
    assert_true(scene_document_get_wall_material(&doc, ref, &mat));
    assert_int_equal(mat, 0);

    scene_document_destroy(&doc);
}

static void test_failed_load_preserves_document(void **state) {
    (void)state;
    char good[512], missing[512];
    path_in_tmpdir(good, sizeof(good), "keep_me.txt");
    path_in_tmpdir(missing, sizeof(missing), "does_not_exist.txt");
    assert_int_equal(write_text_file(good, "12\n21\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, good), SCENE_LOAD_OK);

    /* Mark dirty so we can prove state is preserved. */
    scene_document_internal_set_current_state(&doc, 5);
    assert_true(scene_document_is_dirty(&doc));
    char *old_path = doc.path;
    int old_w = doc.map.width;
    int old_h = doc.map.height;

    assert_int_equal(scene_document_load(&doc, missing), SCENE_LOAD_FILE_NOT_FOUND);
    assert_ptr_equal(doc.path, old_path);
    assert_string_equal(doc.path, good);
    assert_int_equal(doc.map.width, old_w);
    assert_int_equal(doc.map.height, old_h);
    assert_int_equal(doc.current_state, 5);
    assert_int_equal(doc.saved_state, 1);
    assert_true(scene_document_is_dirty(&doc));

    MaterialId mat = -1;
    WallMaterialRef ref = {0, 0};
    assert_true(scene_document_get_wall_material(&doc, ref, &mat));
    assert_int_equal(mat, 1);

    scene_document_destroy(&doc);
}

static void test_save_reload_round_trip(void **state) {
    (void)state;
    char src[512], dst[512];
    path_in_tmpdir(src, sizeof(src), "round_src.txt");
    path_in_tmpdir(dst, sizeof(dst), "round_dst.txt");
    const char *fixture = "1111\n1001\n1221\n1111\n";
    assert_int_equal(write_text_file(src, fixture), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, src), SCENE_LOAD_OK);

    /* Point save at a different path in the temp dir. */
    free(doc.path);
    doc.path = malloc(strlen(dst) + 1);
    assert_non_null(doc.path);
    memcpy(doc.path, dst, strlen(dst) + 1);

    scene_document_internal_set_wall_material(&doc, (WallMaterialRef){1, 1}, 3);
    scene_document_internal_set_current_state(&doc, 2);
    assert_true(scene_document_is_dirty(&doc));

    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_OK);
    assert_false(scene_document_is_dirty(&doc));
    assert_int_equal(doc.saved_state, 2);
    assert_int_equal(doc.current_state, 2);

    SceneDocument reloaded;
    scene_document_init(&reloaded);
    assert_int_equal(scene_document_load(&reloaded, dst), SCENE_LOAD_OK);
    assert_int_equal(reloaded.map.width, 4);
    assert_int_equal(reloaded.map.height, 4);

    MaterialId mat = -1;
    assert_true(scene_document_get_wall_material(&reloaded, (WallMaterialRef){1, 1}, &mat));
    assert_int_equal(mat, 3);
    assert_true(scene_document_get_wall_material(&reloaded, (WallMaterialRef){2, 2}, &mat));
    assert_int_equal(mat, 2);

    scene_document_destroy(&doc);
    scene_document_destroy(&reloaded);
}

static void test_save_rejects_material_above_nine(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "bad_mat.txt");
    assert_int_equal(write_text_file(path, "11\n11\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);
    assert_true(scene_document_internal_set_wall_material(
        &doc, (WallMaterialRef){0, 0}, 10));
    scene_document_internal_set_current_state(&doc, 2);

    assert_int_equal(scene_document_validate_for_save(&doc),
                     SCENE_SAVE_UNREPRESENTABLE_MATERIAL);
    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_UNREPRESENTABLE_MATERIAL);
    assert_int_equal(doc.saved_state, 1); /* unchanged */
    assert_true(scene_document_is_dirty(&doc));

    /* Destination still original content. */
    char *text = read_text_file(path);
    assert_non_null(text);
    assert_string_equal(text, "11\n11\n");
    free(text);

    scene_document_destroy(&doc);
}

static void test_save_rejects_material_below_zero(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "neg_mat.txt");
    assert_int_equal(write_text_file(path, "11\n11\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);
    assert_true(scene_document_internal_set_wall_material(
        &doc, (WallMaterialRef){1, 0}, -1));

    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_UNREPRESENTABLE_MATERIAL);
    assert_int_equal(doc.saved_state, 1);
    scene_document_destroy(&doc);
}

static void test_successful_save_updates_saved_state(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "save_ok.txt");
    assert_int_equal(write_text_file(path, "10\n01\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);
    scene_document_internal_set_current_state(&doc, 7);
    assert_true(scene_document_is_dirty(&doc));
    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_OK);
    assert_int_equal(doc.saved_state, 7);
    assert_false(scene_document_is_dirty(&doc));
    scene_document_destroy(&doc);
}

static void test_save_no_path(void **state) {
    (void)state;
    SceneDocument doc;
    scene_document_init(&doc);
    /* Fabricate a valid in-memory map without a path. */
    Map *m = map_create(2, 2);
    assert_non_null(m);
    map_set(m, 0, 0, 1);
    /* Move into document manually via load of a temp then clear path. */
    char path[512];
    path_in_tmpdir(path, sizeof(path), "tmp_for_nopath.txt");
    assert_int_equal(write_text_file(path, "11\n11\n"), 0);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);
    free(doc.path);
    doc.path = NULL;
    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_NO_PATH);
    map_destroy(m);
    scene_document_destroy(&doc);
}

static void test_failed_replace_preserves_destination(void **state) {
    (void)state;
    /*
     * Force rename failure by pointing destination at a path whose parent
     * directory does not exist after the temp file is created beside a
     * valid sibling path. We instead use a destination that is a directory,
     * so rename(file, dir) fails on Linux.
     */
    char dest_dir[512], seed[512];
    path_in_tmpdir(dest_dir, sizeof(dest_dir), "not_a_file");
    path_in_tmpdir(seed, sizeof(seed), "seed_map.txt");
    assert_int_equal(mkdir(dest_dir, 0700), 0);
    assert_int_equal(write_text_file(seed, "11\n11\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, seed), SCENE_LOAD_OK);

    free(doc.path);
    doc.path = malloc(strlen(dest_dir) + 1);
    assert_non_null(doc.path);
    memcpy(doc.path, dest_dir, strlen(dest_dir) + 1);

    scene_document_internal_set_current_state(&doc, 3);
    DocumentStateId saved_before = doc.saved_state;

    SceneSaveResult rc = scene_document_save(&doc);
    assert_int_equal(rc, SCENE_SAVE_REPLACE_FAILED);
    assert_int_equal(doc.saved_state, saved_before);
    assert_true(scene_document_is_dirty(&doc));

    /* No leftover temp files with our prefix. */
    assert_int_equal(count_files_with_prefix(g_tmpdir, ".scene_doc_"), 0);

    /* Directory still exists (was not replaced by a file). */
    struct stat st;
    assert_int_equal(stat(dest_dir, &st), 0);
    assert_true(S_ISDIR(st.st_mode));

    scene_document_destroy(&doc);
    rmdir(dest_dir);
}

static void test_temp_files_removed_after_unrepresentable(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "cleanup.txt");
    assert_int_equal(write_text_file(path, "11\n11\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);
    scene_document_internal_set_wall_material(&doc, (WallMaterialRef){0, 0}, 99);
    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_UNREPRESENTABLE_MATERIAL);
    /* Validation fails before temp creation — still no temps. */
    assert_int_equal(count_files_with_prefix(g_tmpdir, ".scene_doc_"), 0);
    scene_document_destroy(&doc);
}

static void test_light_map_not_serialized(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "light.txt");
    assert_int_equal(write_text_file(path, "10\n01\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);

    /* Poison light_map with non-zero values that must never appear in file. */
    size_t n = (size_t)doc.map.width * (size_t)doc.map.height;
    for (size_t i = 0; i < n; i++) {
        doc.map.light_map[i] = 123.456 + (double)i;
    }

    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_OK);
    char *text = read_text_file(path);
    assert_non_null(text);
    /* Exact digit grid only — no floats, no extra tokens. */
    assert_string_equal(text, "10\n01\n");
    free(text);

    /* Reload and confirm light_map starts clean (zeros from map_create). */
    SceneDocument again;
    scene_document_init(&again);
    assert_int_equal(scene_document_load(&again, path), SCENE_LOAD_OK);
    for (size_t i = 0; i < n; i++) {
        assert_true(again.map.light_map[i] == 0.0);
    }

    scene_document_destroy(&doc);
    scene_document_destroy(&again);
}

static void test_get_wall_material_oob(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "oob.txt");
    assert_int_equal(write_text_file(path, "11\n11\n"), 0);

    SceneDocument doc;
    scene_document_init(&doc);
    assert_int_equal(scene_document_load(&doc, path), SCENE_LOAD_OK);

    MaterialId mat = 42;
    assert_false(scene_document_get_wall_material(
        &doc, (WallMaterialRef){-1, 0}, &mat));
    assert_false(scene_document_get_wall_material(
        &doc, (WallMaterialRef){0, 99}, &mat));
    assert_false(scene_document_internal_set_wall_material(
        &doc, (WallMaterialRef){5, 5}, 1));

    scene_document_destroy(&doc);
}

static void test_accessors_null_safe(void **state) {
    (void)state;
    assert_null(scene_document_get_map(NULL));
    assert_null(scene_document_get_map_for_runtime(NULL));
    assert_false(scene_document_is_dirty(NULL));
    assert_int_equal(scene_document_validate_for_save(NULL),
                     SCENE_SAVE_INVALID_DOCUMENT);
    scene_document_init(NULL);
    scene_document_destroy(NULL);
}

/* ===================================================================
 *  Entry
 * =================================================================== */

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_destroy_empty),
        cmocka_unit_test(test_load_valid_fixture),
        cmocka_unit_test(test_repeated_load_no_leak),
        cmocka_unit_test(test_failed_load_preserves_document),
        cmocka_unit_test(test_save_reload_round_trip),
        cmocka_unit_test(test_save_rejects_material_above_nine),
        cmocka_unit_test(test_save_rejects_material_below_zero),
        cmocka_unit_test(test_successful_save_updates_saved_state),
        cmocka_unit_test(test_save_no_path),
        cmocka_unit_test(test_failed_replace_preserves_destination),
        cmocka_unit_test(test_temp_files_removed_after_unrepresentable),
        cmocka_unit_test(test_light_map_not_serialized),
        cmocka_unit_test(test_get_wall_material_oob),
        cmocka_unit_test(test_accessors_null_safe),
    };
    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
