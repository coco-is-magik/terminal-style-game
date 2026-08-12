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

static MaterialId read_surface(
    const SceneDocument *doc, int x, int y, SceneSurfaceKind surface
) {
    MaterialId material = -999;
    assert_true(scene_document_get_surface_material(
        doc, x, y, surface, &material));
    return material;
}

static SceneCellOccupancy read_occupancy(
    const SceneDocument *doc, int x, int y
) {
    SceneCellOccupancy occupancy = SCENE_CELL_OCCUPANCY_WALL;
    assert_true(scene_document_get_cell_occupancy(doc, x, y, &occupancy));
    return occupancy;
}

static void mark_material_loaded(AssetRegistry *assets, int id) {
    asset_registry_set_material(assets, id, 1, "#");
    assert_true(snprintf(assets->material_names[id],
                         sizeof(assets->material_names[id]), "%d", id) > 0);
    assets->material_count++;
}

static void add_two_lights(SceneDocument *doc) {
    doc->lights = calloc(2U, sizeof(*doc->lights));
    assert_non_null(doc->lights);
    doc->light_count = 2U;
    doc->light_capacity = 2U;
    doc->lights[0] = (SceneLight){
        .id = 11U, .x = 0.5, .y = 0.5,
        .red = 10U, .green = 20U, .blue = 30U, .alpha = 255U,
        .intensity = 1.0, .radius = 2.0
    };
    doc->lights[1] = (SceneLight){
        .id = 12U, .x = 1.5, .y = 0.5,
        .red = 40U, .green = 50U, .blue = 60U, .alpha = 255U,
        .intensity = 2.0, .radius = 3.0
    };
}

static SceneLight read_light(const SceneDocument *doc, SceneInstanceId id) {
    const SceneLight *light = scene_document_find_light(doc, id);
    assert_non_null(light);
    return *light;
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
    assert_int_equal(h.commands[0].mutations[0].data.wall_material.before, 1);
    assert_int_equal(h.commands[0].mutations[0].data.wall_material.after, 5);
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

static void test_empty_cell_is_not_a_wall_target(void **state) {
    (void)state;
    SceneDocument doc;
    load_fixture(&doc);
    CommandHistory h;
    command_history_init(&h, doc.current_state);

    WallMaterialRef empty = {1, 1};
    assert_int_equal(read_mat(&doc, 1, 1), 0);
    assert_int_equal(
        command_history_set_wall_material(&h, &doc, empty, 4),
        CMD_RESULT_INVALID_TARGET);
    assert_int_equal(read_mat(&doc, 1, 1), 0);
    assert_int_equal(h.count, 0);
    assert_int_equal(h.cursor, 0);
    assert_int_equal(doc.current_state, 1);
    assert_false(scene_document_is_dirty(&doc));

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
    WallMaterialRef b = {1, 0};

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
    assert_int_equal(read_mat(&doc, 1, 0), 2); /* second command undone and discarded */

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
        MaterialId next = (MaterialId)((cur % 9) + 1);
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

static void test_light_command_undo_redo_and_no_change(void **state) {
    SceneDocument doc;
    CommandHistory h;
    SceneLight before;
    SceneLight after;
    (void)state;

    load_fixture(&doc);
    add_two_lights(&doc);
    command_history_init(&h, doc.current_state);
    before = read_light(&doc, 11U);
    after = before;
    after.x = 1.25;
    after.red = 200U;
    after.intensity = -0.5;
    after.radius = 4.5;

    assert_int_equal(command_history_set_light(&h, &doc, 11U, &after), CMD_RESULT_OK);
    assert_true(read_light(&doc, 11U).x == 1.25);
    assert_int_equal(read_light(&doc, 11U).red, 200U);
    assert_true(read_light(&doc, 11U).intensity == -0.5);
    assert_int_equal(h.count, 1U);
    assert_int_equal(h.commands[0].mutation_count, 1U);
    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_OK);
    assert_true(read_light(&doc, 11U).x == before.x);
    assert_int_equal(read_light(&doc, 11U).red, before.red);
    assert_int_equal(command_history_redo(&h, &doc), CMD_RESULT_OK);
    assert_true(read_light(&doc, 11U).radius == 4.5);
    assert_int_equal(command_history_set_light(&h, &doc, 11U, &after),
                     CMD_RESULT_NO_CHANGE);

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_group_is_one_atomic_undo_step(void **state) {
    SceneDocument doc;
    CommandHistory h;
    EditorMutationRequest requests[2] = {0};
    SceneLight first_before;
    SceneLight second_before;
    (void)state;

    load_fixture(&doc);
    add_two_lights(&doc);
    command_history_init(&h, doc.current_state);
    first_before = read_light(&doc, 11U);
    second_before = read_light(&doc, 12U);
    requests[0].type = EDITOR_MUTATION_SET_LIGHT;
    requests[0].data.light.id = 11U;
    requests[0].data.light.value = first_before;
    requests[0].data.light.value.intensity = 3.5;
    requests[0].data.light.value.radius = 5.0;
    requests[1].type = EDITOR_MUTATION_SET_LIGHT;
    requests[1].data.light.id = 12U;
    requests[1].data.light.value = second_before;
    requests[1].data.light.value.green = 220U;
    requests[1].data.light.value.radius = 6.0;

    assert_int_equal(command_history_execute_group(&h, &doc, requests, 2U),
                     CMD_RESULT_OK);
    assert_int_equal(h.count, 1U);
    assert_int_equal(h.commands[0].mutation_count, 2U);
    assert_true(read_light(&doc, 11U).intensity == 3.5);
    assert_int_equal(read_light(&doc, 12U).green, 220U);
    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_OK);
    assert_true(read_light(&doc, 11U).intensity == first_before.intensity);
    assert_int_equal(read_light(&doc, 12U).green, second_before.green);
    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_NOTHING_TO_UNDO);
    assert_int_equal(command_history_redo(&h, &doc), CMD_RESULT_OK);
    assert_true(read_light(&doc, 11U).radius == 5.0);
    assert_true(read_light(&doc, 12U).radius == 6.0);

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_group_validation_and_oom_leave_state_unchanged(void **state) {
    SceneDocument doc;
    CommandHistory h;
    EditorMutationRequest requests[2] = {0};
    SceneLight before;
    (void)state;

    load_fixture(&doc);
    add_two_lights(&doc);
    command_history_init(&h, doc.current_state);
    before = read_light(&doc, 11U);
    requests[0].type = EDITOR_MUTATION_SET_LIGHT;
    requests[0].data.light.id = 11U;
    requests[0].data.light.value = before;
    requests[0].data.light.value.radius = 4.0;
    requests[1] = requests[0];
    requests[1].data.light.value.radius = 5.0;
    assert_int_equal(command_history_execute_group(&h, &doc, requests, 2U),
                     CMD_RESULT_INVALID_TARGET);
    assert_true(read_light(&doc, 11U).radius == before.radius);
    assert_int_equal(h.count, 0U);

    requests[1].data.light.id = 12U;
    requests[1].data.light.value = read_light(&doc, 12U);
    requests[1].data.light.value.radius = 0.0;
    assert_int_equal(command_history_execute_group(&h, &doc, requests, 2U),
                     CMD_RESULT_INVALID_TARGET);
    assert_true(read_light(&doc, 11U).radius == before.radius);
    assert_int_equal(h.count, 0U);

    requests[1].data.light.value.radius = 5.0;
    g_fail_realloc = 1;
    command_history_set_allocator_for_test(
        passthrough_alloc, failing_realloc, passthrough_free);
    assert_int_equal(command_history_execute_group(&h, &doc, requests, 2U),
                     CMD_RESULT_OUT_OF_MEMORY);
    assert_true(read_light(&doc, 11U).radius == before.radius);
    assert_true(read_light(&doc, 12U).radius == 3.0);
    assert_int_equal(h.count, 0U);
    assert_int_equal(doc.current_state, 1U);
    g_fail_realloc = 0;
    command_history_reset_allocator_for_test();

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_undo_failure_rolls_back_partial_group(void **state) {
    SceneDocument doc;
    CommandHistory h;
    EditorMutationRequest requests[2] = {0};
    SceneLight first;
    SceneLight second;
    (void)state;

    load_fixture(&doc);
    add_two_lights(&doc);
    command_history_init(&h, doc.current_state);
    first = read_light(&doc, 11U);
    second = read_light(&doc, 12U);
    requests[0].type = EDITOR_MUTATION_SET_LIGHT;
    requests[0].data.light.id = 12U;
    requests[0].data.light.value = second;
    requests[0].data.light.value.radius = 7.0;
    requests[1].type = EDITOR_MUTATION_SET_LIGHT;
    requests[1].data.light.id = 11U;
    requests[1].data.light.value = first;
    requests[1].data.light.value.radius = 6.0;
    assert_int_equal(command_history_execute_group(&h, &doc, requests, 2U),
                     CMD_RESULT_OK);

    doc.light_count = 1U;
    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_INVALID_TARGET);
    assert_true(read_light(&doc, 11U).radius == 6.0);
    assert_int_equal(h.cursor, 1U);
    assert_int_equal(doc.current_state, 2U);
    doc.light_count = 2U;

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_redo_failure_rolls_back_partial_group(void **state) {
    SceneDocument doc;
    CommandHistory h;
    EditorMutationRequest requests[2] = {0};
    SceneLight first;
    SceneLight second;
    (void)state;

    load_fixture(&doc);
    add_two_lights(&doc);
    command_history_init(&h, doc.current_state);
    first = read_light(&doc, 11U);
    second = read_light(&doc, 12U);
    requests[0].type = EDITOR_MUTATION_SET_LIGHT;
    requests[0].data.light.id = 11U;
    requests[0].data.light.value = first;
    requests[0].data.light.value.radius = 6.0;
    requests[1].type = EDITOR_MUTATION_SET_LIGHT;
    requests[1].data.light.id = 12U;
    requests[1].data.light.value = second;
    requests[1].data.light.value.radius = 7.0;
    assert_int_equal(command_history_execute_group(&h, &doc, requests, 2U),
                     CMD_RESULT_OK);
    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_OK);

    doc.light_count = 1U;
    assert_int_equal(command_history_redo(&h, &doc), CMD_RESULT_INVALID_TARGET);
    assert_true(read_light(&doc, 11U).radius == first.radius);
    assert_int_equal(h.cursor, 0U);
    assert_int_equal(doc.current_state, 1U);
    doc.light_count = 2U;

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_typed_surfaces_restore_exactly(void **state) {
    SceneDocument doc;
    CommandHistory h;
    AssetRegistry assets;
    CommandExecutionContext context = {0};
    (void)state;
    load_fixture(&doc);
    asset_registry_init(&assets);
    mark_material_loaded(&assets, 1);
    mark_material_loaded(&assets, 7);
    context.assets = &assets;
    command_history_init(&h, doc.current_state);

    assert_int_equal(command_history_set_surface_material(
        &h, &doc, 1, 1, SCENE_SURFACE_FLOOR, 7, &context), CMD_RESULT_OK);
    assert_int_equal(command_history_set_surface_material(
        &h, &doc, 1, 1, SCENE_SURFACE_CEILING, 7, &context), CMD_RESULT_OK);
    assert_int_equal(read_surface(&doc, 1, 1, SCENE_SURFACE_FLOOR), 7);
    assert_int_equal(read_surface(&doc, 1, 1, SCENE_SURFACE_CEILING), 7);
    assert_int_equal(read_occupancy(&doc, 1, 1), SCENE_CELL_OCCUPANCY_EMPTY);
    assert_int_equal(command_history_undo_checked(&h, &doc, &context), CMD_RESULT_OK);
    assert_int_equal(read_surface(&doc, 1, 1, SCENE_SURFACE_CEILING), 1);
    assert_int_equal(command_history_undo_checked(&h, &doc, &context), CMD_RESULT_OK);
    assert_int_equal(read_surface(&doc, 1, 1, SCENE_SURFACE_FLOOR), 1);
    assert_int_equal(command_history_redo_checked(&h, &doc, &context), CMD_RESULT_OK);
    assert_int_equal(command_history_redo_checked(&h, &doc, &context), CMD_RESULT_OK);
    assert_int_equal(read_surface(&doc, 1, 1, SCENE_SURFACE_FLOOR), 7);
    assert_int_equal(read_surface(&doc, 1, 1, SCENE_SURFACE_CEILING), 7);

    assert_int_equal(command_history_set_surface_material(
        &h, &doc, 1, 1, SCENE_SURFACE_FLOOR, 7, &context), CMD_RESULT_NO_CHANGE);
    assert_int_equal(command_history_set_surface_material(
        &h, &doc, 99, 1, SCENE_SURFACE_FLOOR, 7, &context),
        CMD_RESULT_INVALID_TARGET);
    assert_int_equal(command_history_set_surface_material(
        &h, &doc, 1, 1, SCENE_SURFACE_FLOOR, 8, &context),
        CMD_RESULT_MATERIAL_NOT_LOADED);
    assert_int_equal(command_history_set_surface_material(
        &h, &doc, 1, 1, SCENE_SURFACE_WALL, 7, &context),
        CMD_RESULT_INVALID_TARGET);

    command_history_destroy(&h);
    asset_registry_clear(&assets);
    scene_document_destroy(&doc);
}

static void test_wall_material_never_changes_occupancy(void **state) {
    SceneDocument doc;
    CommandHistory h;
    (void)state;
    load_fixture(&doc);
    command_history_init(&h, doc.current_state);
    assert_int_equal(command_history_set_wall_material(
        &h, &doc, (WallMaterialRef){0, 0}, 9), CMD_RESULT_OK);
    assert_int_equal(read_occupancy(&doc, 0, 0), SCENE_CELL_OCCUPANCY_WALL);
    assert_int_equal(read_surface(&doc, 0, 0, SCENE_SURFACE_WALL), 9);
    assert_int_equal(read_mat(&doc, 0, 0), 9);
    assert_int_equal(command_history_set_wall_material(
        &h, &doc, (WallMaterialRef){0, 0}, 0), CMD_RESULT_INVALID_TARGET);
    assert_int_equal(read_occupancy(&doc, 0, 0), SCENE_CELL_OCCUPANCY_WALL);
    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_ambient_command_range_and_history(void **state) {
    SceneDocument doc;
    CommandHistory h;
    double before;
    (void)state;
    load_fixture(&doc);
    command_history_init(&h, doc.current_state);
    before = scene_document_get_ambient_intensity(&doc);
    assert_int_equal(command_history_set_ambient_intensity(&h, &doc, 0.625),
                     CMD_RESULT_OK);
    assert_true(scene_document_get_ambient_intensity(&doc) == 0.625);
    assert_int_equal(command_history_set_ambient_intensity(&h, &doc, 0.625),
                     CMD_RESULT_NO_CHANGE);
    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_OK);
    assert_true(scene_document_get_ambient_intensity(&doc) == before);
    assert_int_equal(command_history_redo(&h, &doc), CMD_RESULT_OK);
    assert_true(scene_document_get_ambient_intensity(&doc) == 0.625);
    assert_int_equal(command_history_set_ambient_intensity(&h, &doc, -0.01),
                     CMD_RESULT_INVALID_TARGET);
    assert_int_equal(command_history_set_ambient_intensity(&h, &doc, 1.01),
                     CMD_RESULT_INVALID_TARGET);
    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_place_remove_preserve_latent_surfaces(void **state) {
    SceneDocument doc;
    CommandHistory h;
    (void)state;
    load_fixture(&doc);
    doc.spawn_x = 0.5;
    doc.spawn_y = 0.5;
    command_history_init(&h, doc.current_state);
    assert_int_equal(read_surface(&doc, 1, 1, SCENE_SURFACE_WALL), 1);
    assert_int_equal(command_history_place_wall(&h, &doc, 1, 1, NULL),
                     CMD_RESULT_OK);
    assert_int_equal(read_occupancy(&doc, 1, 1), SCENE_CELL_OCCUPANCY_WALL);
    assert_int_equal(read_mat(&doc, 1, 1), 1);
    assert_int_equal(command_history_place_wall(&h, &doc, 1, 1, NULL),
                     CMD_RESULT_NO_CHANGE);
    assert_int_equal(command_history_remove_wall(&h, &doc, 1, 1, NULL),
                     CMD_RESULT_OK);
    assert_int_equal(read_occupancy(&doc, 1, 1), SCENE_CELL_OCCUPANCY_EMPTY);
    assert_int_equal(read_mat(&doc, 1, 1), 0);
    assert_int_equal(read_surface(&doc, 1, 1, SCENE_SURFACE_WALL), 1);
    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_OK);
    assert_int_equal(read_occupancy(&doc, 1, 1), SCENE_CELL_OCCUPANCY_WALL);
    assert_int_equal(command_history_undo(&h, &doc), CMD_RESULT_OK);
    assert_int_equal(read_occupancy(&doc, 1, 1), SCENE_CELL_OCCUPANCY_EMPTY);
    assert_int_equal(command_history_redo(&h, &doc), CMD_RESULT_OK);
    assert_int_equal(command_history_redo(&h, &doc), CMD_RESULT_OK);
    assert_int_equal(read_occupancy(&doc, 1, 1), SCENE_CELL_OCCUPANCY_EMPTY);
    assert_int_equal(command_history_remove_wall(&h, &doc, 1, 1, NULL),
                     CMD_RESULT_NO_CHANGE);
    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_east_edge_growth_shrink_undo_redo_and_content_guard(void **state) {
    SceneDocument doc;
    CommandHistory h;
    CommandExecutionContext context = {0};
    SceneCellOccupancy occupancy;
    (void)state;
    load_fixture(&doc);
    doc.spawn_x = 0.5;
    doc.spawn_y = 0.5;
    command_history_init(&h, doc.current_state);

    assert_int_equal(command_history_remove_wall(&h, &doc, 1, 0, &context),
                     CMD_RESULT_OK);
    assert_int_equal(doc.map.width, 3);
    assert_int_equal(doc.map.height, 2);
    assert_int_equal(doc.east_growth_count, 1U);
    assert_int_equal(doc.east_growth[0], 0);
    assert_int_equal(read_occupancy(&doc, 1, 0), SCENE_CELL_OCCUPANCY_EMPTY);
    assert_int_equal(read_surface(&doc, 2, 0, SCENE_SURFACE_WALL), 2);
    assert_int_equal(read_occupancy(&doc, 2, 0), SCENE_CELL_OCCUPANCY_WALL);
    assert_int_equal(read_occupancy(&doc, 2, 1), SCENE_CELL_OCCUPANCY_EMPTY);

    assert_int_equal(command_history_undo_checked(&h, &doc, &context), CMD_RESULT_OK);
    assert_int_equal(doc.map.width, 2);
    assert_int_equal(doc.east_growth_count, 0U);
    assert_int_equal(read_occupancy(&doc, 1, 0), SCENE_CELL_OCCUPANCY_WALL);
    assert_int_equal(command_history_redo_checked(&h, &doc, &context), CMD_RESULT_OK);
    assert_int_equal(doc.map.width, 3);
    assert_int_equal(read_occupancy(&doc, 1, 0), SCENE_CELL_OCCUPANCY_EMPTY);

    doc.lights = calloc(1U, sizeof(*doc.lights));
    assert_non_null(doc.lights);
    doc.light_count = doc.light_capacity = 1U;
    doc.lights[0] = (SceneLight){.id = 7U, .x = 2.5, .y = 0.5,
        .alpha = 255U, .intensity = 1.0, .radius = 1.0};
    assert_int_equal(command_history_place_wall(&h, &doc, 1, 0, &context),
                     CMD_RESULT_RESIZE_BLOCKED);
    assert_true(scene_document_get_cell_occupancy(&doc, 1, 0, &occupancy));
    assert_int_equal(occupancy, SCENE_CELL_OCCUPANCY_EMPTY);
    doc.light_count = 0U;
    assert_int_equal(command_history_place_wall(&h, &doc, 1, 0, &context),
                     CMD_RESULT_OK);
    assert_int_equal(doc.map.width, 2);
    assert_int_equal(doc.east_growth_count, 0U);
    assert_int_equal(read_occupancy(&doc, 1, 0), SCENE_CELL_OCCUPANCY_WALL);
    assert_int_equal(command_history_undo_checked(&h, &doc, &context), CMD_RESULT_OK);
    assert_int_equal(doc.map.width, 3);
    assert_int_equal(read_occupancy(&doc, 1, 0), SCENE_CELL_OCCUPANCY_EMPTY);

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_construction_spawn_player_and_attachment_safety(void **state) {
    SceneDocument doc;
    CommandHistory h;
    CommandExecutionContext context = {0};
    (void)state;
    load_fixture(&doc);
    command_history_init(&h, doc.current_state);

    assert_int_equal(command_history_place_wall(&h, &doc, 1, 1, &context),
                     CMD_RESULT_SPAWN_BLOCKED);
    doc.spawn_x = 0.5;
    doc.spawn_y = 0.5;
    context.has_player_cell = true;
    context.player_map_x = 1;
    context.player_map_y = 1;
    assert_int_equal(command_history_place_wall(&h, &doc, 1, 1, &context),
                     CMD_RESULT_PLAYER_BLOCKED);
    context.has_player_cell = false;
    assert_int_equal(command_history_place_wall(&h, &doc, 1, 1, &context),
                     CMD_RESULT_OK);
    assert_int_equal(command_history_undo_checked(&h, &doc, &context), CMD_RESULT_OK);
    context.has_player_cell = true;
    assert_int_equal(command_history_redo_checked(&h, &doc, &context),
                     CMD_RESULT_PLAYER_BLOCKED);
    assert_int_equal(h.cursor, 0U);
    assert_int_equal(read_occupancy(&doc, 1, 1), SCENE_CELL_OCCUPANCY_EMPTY);
    context.has_player_cell = false;
    assert_int_equal(command_history_redo_checked(&h, &doc, &context), CMD_RESULT_OK);

    assert_int_equal(command_history_remove_wall(&h, &doc, 1, 1, &context),
                     CMD_RESULT_OK);
    context.has_player_cell = true;
    assert_int_equal(command_history_undo_checked(&h, &doc, &context),
                     CMD_RESULT_PLAYER_BLOCKED);
    assert_int_equal(read_occupancy(&doc, 1, 1), SCENE_CELL_OCCUPANCY_EMPTY);
    assert_int_equal(h.cursor, 2U);
    context.has_player_cell = false;
    assert_int_equal(command_history_undo_checked(&h, &doc, &context), CMD_RESULT_OK);

    doc.decals = calloc(1U, sizeof(*doc.decals));
    assert_non_null(doc.decals);
    doc.decal_count = doc.decal_capacity = 1U;
    doc.decals[0].surface = SCENE_DECAL_SURFACE_WALL;
    doc.decals[0].map_x = 1;
    doc.decals[0].map_y = 1;
    doc.decals[0].id = 27U;
    assert_int_equal(command_history_remove_wall(&h, &doc, 1, 1, &context),
                     CMD_RESULT_OK);
    assert_int_equal(doc.decal_count, 0U);
    assert_int_equal(command_history_undo_checked(&h, &doc, &context), CMD_RESULT_OK);
    assert_int_equal(read_occupancy(&doc, 1, 1), SCENE_CELL_OCCUPANCY_WALL);
    assert_int_equal(doc.decal_count, 1U);
    assert_int_equal(doc.decals[0].id, 27U);
    assert_int_equal(command_history_redo_checked(&h, &doc, &context), CMD_RESULT_OK);
    assert_int_equal(doc.decal_count, 0U);

    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_surface_group_duplicate_and_oom_are_atomic(void **state) {
    SceneDocument doc;
    CommandHistory h;
    EditorMutationRequest requests[2] = {0};
    (void)state;
    load_fixture(&doc);
    command_history_init(&h, doc.current_state);
    requests[0].type = EDITOR_MUTATION_SET_FLOOR_MATERIAL;
    requests[0].data.surface_material.map_x = 1;
    requests[0].data.surface_material.map_y = 1;
    requests[0].data.surface_material.material = 7;
    requests[1] = requests[0];
    requests[1].data.surface_material.material = 8;
    assert_int_equal(command_history_execute_group(&h, &doc, requests, 2U),
                     CMD_RESULT_INVALID_TARGET);
    assert_int_equal(read_surface(&doc, 1, 1, SCENE_SURFACE_FLOOR), 1);

    requests[0].type = EDITOR_MUTATION_PLACE_WALL;
    requests[0].data.occupancy.map_x = 1;
    requests[0].data.occupancy.map_y = 1;
    requests[1].type = EDITOR_MUTATION_REMOVE_WALL;
    requests[1].data.occupancy = requests[0].data.occupancy;
    assert_int_equal(command_history_execute_group(&h, &doc, requests, 2U),
                     CMD_RESULT_INVALID_TARGET);
    assert_int_equal(read_occupancy(&doc, 1, 1), SCENE_CELL_OCCUPANCY_EMPTY);

    requests[0].type = EDITOR_MUTATION_SET_FLOOR_MATERIAL;
    requests[0].data.surface_material.map_x = 1;
    requests[0].data.surface_material.map_y = 1;
    requests[0].data.surface_material.material = 7;
    requests[1].type = EDITOR_MUTATION_SET_CEILING_MATERIAL;
    requests[1].data.surface_material.map_x = 1;
    requests[1].data.surface_material.map_y = 1;
    requests[1].data.surface_material.material = 8;
    g_fail_realloc = 1;
    command_history_set_allocator_for_test(
        passthrough_alloc, failing_realloc, passthrough_free);
    assert_int_equal(command_history_execute_group(&h, &doc, requests, 2U),
                     CMD_RESULT_OUT_OF_MEMORY);
    assert_int_equal(read_surface(&doc, 1, 1, SCENE_SURFACE_FLOOR), 1);
    assert_int_equal(read_surface(&doc, 1, 1, SCENE_SURFACE_CEILING), 1);
    assert_int_equal(h.cursor, 0U);
    assert_int_equal(doc.current_state, 1U);
    g_fail_realloc = 0;
    command_history_reset_allocator_for_test();
    command_history_destroy(&h);
    scene_document_destroy(&doc);
}

static void test_undo_restores_missing_material_and_repair_state(void **state) {
    SceneDocument doc;
    CommandHistory h;
    AssetRegistry assets;
    CommandExecutionContext context = {0};
    (void)state;
    load_fixture(&doc);
    asset_registry_init(&assets);
    mark_material_loaded(&assets, 7);
    context.assets = &assets;
    command_history_init(&h, doc.current_state);
    doc.repair_diagnostics = calloc(12U, sizeof(*doc.repair_diagnostics));
    assert_non_null(doc.repair_diagnostics);
    doc.repair_diagnostic_capacity = 12U;
    assert_int_equal(command_history_set_surface_material(
        &h, &doc, 1, 1, SCENE_SURFACE_FLOOR, 7, &context), CMD_RESULT_OK);
    assert_int_equal(command_history_undo_checked(&h, &doc, &context), CMD_RESULT_OK);
    assert_int_equal(read_surface(&doc, 1, 1, SCENE_SURFACE_FLOOR), 1);
    assert_true(scene_document_is_repair_required(&doc));
    assert_int_equal(h.cursor, 0U);
    command_history_destroy(&h);
    asset_registry_clear(&assets);
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
        cmocka_unit_test(test_empty_cell_is_not_a_wall_target),
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
        cmocka_unit_test(test_light_command_undo_redo_and_no_change),
        cmocka_unit_test(test_group_is_one_atomic_undo_step),
        cmocka_unit_test(test_group_validation_and_oom_leave_state_unchanged),
        cmocka_unit_test(test_undo_failure_rolls_back_partial_group),
        cmocka_unit_test(test_redo_failure_rolls_back_partial_group),
        cmocka_unit_test(test_typed_surfaces_restore_exactly),
        cmocka_unit_test(test_wall_material_never_changes_occupancy),
        cmocka_unit_test(test_ambient_command_range_and_history),
        cmocka_unit_test(test_place_remove_preserve_latent_surfaces),
        cmocka_unit_test(test_east_edge_growth_shrink_undo_redo_and_content_guard),
        cmocka_unit_test(test_construction_spawn_player_and_attachment_safety),
        cmocka_unit_test(test_surface_group_duplicate_and_oom_are_atomic),
        cmocka_unit_test(test_undo_restores_missing_material_and_repair_state),
    };
    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
