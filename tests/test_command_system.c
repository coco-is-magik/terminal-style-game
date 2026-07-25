/**
 * test_command_system.c — CommandHistory undo/redo and state-ID invariants
 *
 * File I/O uses an isolated temp directory. Checked-in assets are never written.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/command_system.h"
#include "../src/scene_document.h"
#include "../src/config.h"

/* Test hooks from command_system.c */
void command_history_set_allocator_for_test(
    void *(*alloc_fn)(size_t),
    void *(*realloc_fn)(void *, size_t),
    void (*free_fn)(void *)
);
void command_history_reset_allocator_for_test(void);

/* ===================================================================
 *  Temp helpers
 * =================================================================== */

static char g_tmpdir[] = "/tmp/tsg_cmd_sys_XXXXXX";
static int g_tmpdir_ready = 0;

static int make_tmpdir(void) {
    memcpy(g_tmpdir, "/tmp/tsg_cmd_sys_XXXXXX", sizeof("/tmp/tsg_cmd_sys_XXXXXX"));
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
    return fclose(fp) == 0 ? 0 : -1;
}

static void rm_rf_tmpdir(void) {
    if (!g_tmpdir_ready) return;
    /* Only a few known files; remove by pattern via shell-free unlink of known names. */
    char path[512];
    const char *names[] = {
        "map.txt", "save_after_undo.txt", NULL
    };
    for (int i = 0; names[i]; i++) {
        path_in_tmpdir(path, sizeof(path), names[i]);
        remove(path);
    }
    rmdir(g_tmpdir);
    g_tmpdir_ready = 0;
}

static int group_setup(void **state) {
    (void)state;
    config_init_defaults();
    command_history_reset_allocator_for_test();
    if (make_tmpdir() != 0) return -1;
    return 0;
}

static int group_teardown(void **state) {
    (void)state;
    command_history_reset_allocator_for_test();
    rm_rf_tmpdir();
    return 0;
}

/* Load a simple 2x2 map: materials 1 2 / 3 0 */
static void load_fixture(SceneDocument *doc) {
    char path[512];
    path_in_tmpdir(path, sizeof(path), "map.txt");
    assert_int_equal(write_text_file(path, "12\n30\n"), 0);
    scene_document_init(doc);
    assert_int_equal(scene_document_load(doc, path), SCENE_LOAD_OK);
    assert_int_equal(doc->current_state, 1);
    assert_int_equal(doc->saved_state, 1);
}

static MaterialId read_mat(const SceneDocument *doc, int x, int y) {
    MaterialId m = -999;
    WallMaterialRef ref = {x, y};
    assert_true(scene_document_get_wall_material(doc, ref, &m));
    return m;
}

/* ===================================================================
 *  Failing allocator
 * =================================================================== */

static int g_fail_realloc = 0;

static void *failing_realloc(void *ptr, size_t size) {
    (void)size;
    if (g_fail_realloc) {
        return NULL;
    }
    return realloc(ptr, size);
}

static void *passthrough_alloc(size_t size) { return malloc(size); }
static void passthrough_free(void *ptr) { free(ptr); }

/* ===================================================================
 *  Tests
 * =================================================================== */

static void test_init_lazy_no_alloc(void **state) {
    (void)state;
    CommandHistory h;
    command_history_init(&h, 1);
    assert_null(h.commands);
    assert_int_equal(h.count, 0);
    assert_int_equal(h.cursor, 0);
    assert_int_equal(h.capacity, 0);
    assert_int_equal(h.next_state_id, 2);
    command_history_destroy(&h);
    command_history_destroy(&h); /* idempotent */
}

static void test_first_command_states(void **state) {
    (void)state;
    SceneDocument doc;
    load_fixture(&doc);

    CommandHistory h;
    command_history_init(&h, doc.current_state);

    WallMaterialRef ref = {0, 0};
    assert_int_equal(read_mat(&doc, 0, 0), 1);
    assert_int_equal(
        command_history_set_wall_material(&h, &doc, ref, 5),
        CMD_RESULT_OK);

    assert_int_equal(h.count, 1);
    assert_int_equal(h.cursor, 1);
    assert_int_equal(h.commands[0].before_state, 1);
    assert_int_equal(h.commands[0].after_state, 2);
    assert_int_equal(h.commands[0].data.set_wall_material.old_material, 1);
    assert_int_equal(h.commands[0].data.set_wall_material.new_material, 5);
    assert_int_equal(doc.current_state, 2);
    assert_int_equal(read_mat(&doc, 0, 0), 5);
    assert_int_equal(h.next_state_id, 3);
    assert_true(scene_document_is_dirty(&doc));

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_undo_first_restores_state_one(void **state) {
    (void)state;
    SceneDocument doc;
    load_fixture(&doc);
    CommandHistory h;
    command_history_init(&h, doc.current_state);

    WallMaterialRef ref = {0, 0};
    assert_int_equal(command_history_set_wall_material(&h, &doc, ref, 5), CMD_RESULT_OK);
    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_OK);

    assert_int_equal(doc.current_state, 1);
    assert_int_equal(read_mat(&doc, 0, 0), 1);
    assert_int_equal(h.cursor, 0);
    assert_int_equal(h.count, 1); /* redo still available */
    assert_false(scene_document_is_dirty(&doc)); /* back to saved_state */

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_redo_restores_new_material(void **state) {
    (void)state;
    SceneDocument doc;
    load_fixture(&doc);
    CommandHistory h;
    command_history_init(&h, doc.current_state);

    WallMaterialRef ref = {1, 0};
    assert_int_equal(command_history_set_wall_material(&h, &doc, ref, 7), CMD_RESULT_OK);
    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_OK);
    assert_int_equal(command_history_redo(&h, &doc), CMD_RESULT_OK);

    assert_int_equal(read_mat(&doc, 1, 0), 7);
    assert_int_equal(doc.current_state, 2);
    assert_int_equal(h.cursor, 1);
    assert_true(scene_document_is_dirty(&doc));

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_no_change_assignment(void **state) {
    (void)state;
    SceneDocument doc;
    load_fixture(&doc);
    CommandHistory h;
    command_history_init(&h, doc.current_state);

    WallMaterialRef ref = {0, 0};
    MaterialId cur = read_mat(&doc, 0, 0);
    assert_int_equal(
        command_history_set_wall_material(&h, &doc, ref, cur),
        CMD_RESULT_NO_CHANGE);

    assert_null(h.commands);
    assert_int_equal(h.count, 0);
    assert_int_equal(h.cursor, 0);
    assert_int_equal(h.next_state_id, 2);
    assert_int_equal(doc.current_state, 1);
    assert_false(scene_document_is_dirty(&doc));

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_invalid_coordinates(void **state) {
    (void)state;
    SceneDocument doc;
    load_fixture(&doc);
    CommandHistory h;
    command_history_init(&h, doc.current_state);

    WallMaterialRef bad = {99, 99};
    assert_int_equal(
        command_history_set_wall_material(&h, &doc, bad, 4),
        CMD_RESULT_INVALID_TARGET);

    assert_int_equal(h.count, 0);
    assert_int_equal(h.cursor, 0);
    assert_int_equal(doc.current_state, 1);
    assert_int_equal(read_mat(&doc, 0, 0), 1);

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_undo_at_zero(void **state) {
    (void)state;
    SceneDocument doc;
    load_fixture(&doc);
    CommandHistory h;
    command_history_init(&h, doc.current_state);

    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_NOTHING_TO_UNDO);
    assert_int_equal(doc.current_state, 1);

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_redo_at_count(void **state) {
    (void)state;
    SceneDocument doc;
    load_fixture(&doc);
    CommandHistory h;
    command_history_init(&h, doc.current_state);

    assert_int_equal(command_history_redo(&h, &doc), CMD_RESULT_NOTHING_TO_REDO);

    WallMaterialRef ref = {0, 1};
    assert_int_equal(command_history_set_wall_material(&h, &doc, ref, 8), CMD_RESULT_OK);
    assert_int_equal(command_history_redo(&h, &doc), CMD_RESULT_NOTHING_TO_REDO);

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_execute_after_undo_discards_redo(void **state) {
    (void)state;
    SceneDocument doc;
    load_fixture(&doc);
    CommandHistory h;
    command_history_init(&h, doc.current_state);

    WallMaterialRef a = {0, 0};
    WallMaterialRef b = {1, 1};

    assert_int_equal(command_history_set_wall_material(&h, &doc, a, 4), CMD_RESULT_OK);
    DocumentStateId after_first = doc.current_state; /* 2 */
    assert_int_equal(command_history_set_wall_material(&h, &doc, b, 5), CMD_RESULT_OK);
    DocumentStateId after_second = doc.current_state; /* 3 */
    DocumentStateId next_after_two = h.next_state_id; /* 4 */

    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_OK);
    assert_int_equal(h.cursor, 1);
    assert_int_equal(h.count, 2);

    /* Branch: new command at cursor 1 discards second command. */
    assert_int_equal(command_history_set_wall_material(&h, &doc, a, 6), CMD_RESULT_OK);

    assert_int_equal(h.count, 2);
    assert_int_equal(h.cursor, 2);
    assert_int_equal(doc.current_state, next_after_two); /* 4 — discarded IDs not reused */
    assert_int_equal(h.commands[1].after_state, next_after_two);
    assert_int_equal(h.commands[1].before_state, after_first);
    assert_int_equal(read_mat(&doc, 0, 0), 6);
    assert_int_equal(read_mat(&doc, 1, 1), 0); /* second command undone and discarded */

    /* Discarded after_second (3) must not appear as next_state_id. */
    assert_true(h.next_state_id > after_second);
    assert_int_equal(h.next_state_id, next_after_two + 1);

    assert_int_equal(command_history_redo(&h, &doc), CMD_RESULT_NOTHING_TO_REDO);

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_discarded_state_ids_never_reused(void **state) {
    (void)state;
    SceneDocument doc;
    load_fixture(&doc);
    CommandHistory h;
    command_history_init(&h, doc.current_state);

    WallMaterialRef ref = {0, 0};
    assert_int_equal(command_history_set_wall_material(&h, &doc, ref, 2), CMD_RESULT_OK); /* ->2 */
    assert_int_equal(command_history_set_wall_material(&h, &doc, ref, 3), CMD_RESULT_OK); /* ->3 */
    assert_int_equal(command_history_set_wall_material(&h, &doc, ref, 4), CMD_RESULT_OK); /* ->4 */

    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_OK); /* cursor 2, state 3 */
    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_OK); /* cursor 1, state 2 */

    DocumentStateId next_before = h.next_state_id; /* 5 */
    assert_int_equal(command_history_set_wall_material(&h, &doc, ref, 9), CMD_RESULT_OK);

    assert_int_equal(doc.current_state, next_before); /* 5, not 3 or 4 */
    assert_int_equal(h.commands[1].after_state, next_before);
    assert_int_equal(h.count, 2);
    assert_int_equal(h.next_state_id, next_before + 1);

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_undo_to_saved_reports_clean(void **state) {
    (void)state;
    SceneDocument doc;
    load_fixture(&doc);
    CommandHistory h;
    command_history_init(&h, doc.current_state);

    WallMaterialRef ref = {0, 0};
    assert_int_equal(command_history_set_wall_material(&h, &doc, ref, 5), CMD_RESULT_OK);
    assert_true(scene_document_is_dirty(&doc));
    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_OK);
    assert_false(scene_document_is_dirty(&doc));
    assert_int_equal(doc.current_state, doc.saved_state);

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_redo_away_from_saved_reports_dirty(void **state) {
    (void)state;
    SceneDocument doc;
    load_fixture(&doc);
    CommandHistory h;
    command_history_init(&h, doc.current_state);

    WallMaterialRef ref = {0, 0};
    assert_int_equal(command_history_set_wall_material(&h, &doc, ref, 5), CMD_RESULT_OK);
    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_OK);
    assert_false(scene_document_is_dirty(&doc));
    assert_int_equal(command_history_redo(&h, &doc), CMD_RESULT_OK);
    assert_true(scene_document_is_dirty(&doc));

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_save_after_undo_marks_historical_clean(void **state) {
    (void)state;
    SceneDocument doc;
    load_fixture(&doc);
    CommandHistory h;
    command_history_init(&h, doc.current_state);

    WallMaterialRef ref = {0, 0};
    assert_int_equal(command_history_set_wall_material(&h, &doc, ref, 5), CMD_RESULT_OK);
    assert_int_equal(command_history_set_wall_material(&h, &doc, ref, 6), CMD_RESULT_OK);
    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_OK);
    /* current_state == 2, material 5, saved still 1 */
    assert_true(scene_document_is_dirty(&doc));
    assert_int_equal(doc.current_state, 2);

    assert_int_equal(scene_document_save(&doc), SCENE_SAVE_OK);
    assert_int_equal(doc.saved_state, 2);
    assert_false(scene_document_is_dirty(&doc));

    /* History unchanged by save. */
    assert_int_equal(h.count, 2);
    assert_int_equal(h.cursor, 1);

    /* Redo makes dirty again. */
    assert_int_equal(command_history_redo(&h, &doc), CMD_RESULT_OK);
    assert_true(scene_document_is_dirty(&doc));
    assert_int_equal(doc.current_state, 3);

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_allocation_failure_unchanged(void **state) {
    (void)state;
    SceneDocument doc;
    load_fixture(&doc);
    CommandHistory h;
    command_history_init(&h, doc.current_state);

    g_fail_realloc = 1;
    command_history_set_allocator_for_test(
        passthrough_alloc, failing_realloc, passthrough_free);

    WallMaterialRef ref = {0, 0};
    assert_int_equal(
        command_history_set_wall_material(&h, &doc, ref, 5),
        CMD_RESULT_OUT_OF_MEMORY);

    assert_null(h.commands);
    assert_int_equal(h.count, 0);
    assert_int_equal(h.cursor, 0);
    assert_int_equal(h.capacity, 0);
    assert_int_equal(h.next_state_id, 2);
    assert_int_equal(doc.current_state, 1);
    assert_int_equal(read_mat(&doc, 0, 0), 1);
    assert_false(scene_document_is_dirty(&doc));

    g_fail_realloc = 0;
    command_history_reset_allocator_for_test();

    /*
     * Grow failure at capacity: fill until cursor == capacity, then fail
     * realloc. Invariant cursor <= count <= capacity means grow is never
     * required while a redo branch exists (cursor < count implies
     * cursor < capacity), so redo-preservation on OOM is structural.
     */
    assert_int_equal(command_history_set_wall_material(&h, &doc, ref, 5), CMD_RESULT_OK);
    while (h.cursor < h.capacity) {
        MaterialId cur = read_mat(&doc, 0, 0);
        MaterialId next = (MaterialId)((cur + 1) % 9);
        if (next == cur) next = (MaterialId)((next + 1) % 9);
        assert_int_equal(
            command_history_set_wall_material(&h, &doc, ref, next),
            CMD_RESULT_OK);
    }
    assert_int_equal(h.cursor, h.capacity);

    size_t cursor_before = h.cursor;
    size_t count_before = h.count;
    size_t cap_before = h.capacity;
    DocumentStateId next_before = h.next_state_id;
    DocumentStateId cur_before = doc.current_state;
    MaterialId mat_before = read_mat(&doc, 0, 0);

    g_fail_realloc = 1;
    command_history_set_allocator_for_test(
        passthrough_alloc, failing_realloc, passthrough_free);

    assert_int_equal(
        command_history_set_wall_material(&h, &doc, ref, 8),
        CMD_RESULT_OUT_OF_MEMORY);

    assert_int_equal(h.cursor, cursor_before);
    assert_int_equal(h.count, count_before);
    assert_int_equal(h.capacity, cap_before);
    assert_int_equal(h.next_state_id, next_before);
    assert_int_equal(doc.current_state, cur_before);
    assert_int_equal(read_mat(&doc, 0, 0), mat_before);

    g_fail_realloc = 0;
    command_history_reset_allocator_for_test();
    command_history_destroy(&h);
    scene_document_destroy(&doc);
}


static void test_state_id_exhaustion(void **state) {
    (void)state;
    SceneDocument doc;
    load_fixture(&doc);

    CommandHistory h;
    command_history_init(&h, UINT64_MAX);

    WallMaterialRef ref = {0, 0};
    assert_int_equal(
        command_history_set_wall_material(&h, &doc, ref, 5),
        CMD_RESULT_STATE_ID_EXHAUSTED);

    assert_null(h.commands);
    assert_int_equal(h.count, 0);
    assert_int_equal(doc.current_state, 1);
    assert_int_equal(read_mat(&doc, 0, 0), 1);

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_multiple_undo_redo_chain(void **state) {
    (void)state;
    SceneDocument doc;
    load_fixture(&doc);
    CommandHistory h;
    command_history_init(&h, doc.current_state);

    WallMaterialRef ref = {0, 0};
    assert_int_equal(command_history_set_wall_material(&h, &doc, ref, 2), CMD_RESULT_OK);
    assert_int_equal(command_history_set_wall_material(&h, &doc, ref, 3), CMD_RESULT_OK);
    assert_int_equal(command_history_set_wall_material(&h, &doc, ref, 4), CMD_RESULT_OK);

    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_OK);
    assert_int_equal(read_mat(&doc, 0, 0), 3);
    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_OK);
    assert_int_equal(read_mat(&doc, 0, 0), 2);
    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_OK);
    assert_int_equal(read_mat(&doc, 0, 0), 1);
    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_NOTHING_TO_UNDO);

    assert_int_equal(command_history_redo(&h, &doc), CMD_RESULT_OK);
    assert_int_equal(read_mat(&doc, 0, 0), 2);
    assert_int_equal(command_history_redo(&h, &doc), CMD_RESULT_OK);
    assert_int_equal(read_mat(&doc, 0, 0), 3);
    assert_int_equal(command_history_redo(&h, &doc), CMD_RESULT_OK);
    assert_int_equal(read_mat(&doc, 0, 0), 4);
    assert_int_equal(command_history_redo(&h, &doc), CMD_RESULT_NOTHING_TO_REDO);

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

/* ===================================================================
 *  Entry
 * =================================================================== */

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_lazy_no_alloc),
        cmocka_unit_test(test_first_command_states),
        cmocka_unit_test(test_undo_first_restores_state_one),
        cmocka_unit_test(test_redo_restores_new_material),
        cmocka_unit_test(test_no_change_assignment),
        cmocka_unit_test(test_invalid_coordinates),
        cmocka_unit_test(test_undo_at_zero),
        cmocka_unit_test(test_redo_at_count),
        cmocka_unit_test(test_execute_after_undo_discards_redo),
        cmocka_unit_test(test_discarded_state_ids_never_reused),
        cmocka_unit_test(test_undo_to_saved_reports_clean),
        cmocka_unit_test(test_redo_away_from_saved_reports_dirty),
        cmocka_unit_test(test_save_after_undo_marks_historical_clean),
        cmocka_unit_test(test_allocation_failure_unchanged),
        cmocka_unit_test(test_state_id_exhaustion),
        cmocka_unit_test(test_multiple_undo_redo_chain),
    };
    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
