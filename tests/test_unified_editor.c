/**
 * test_unified_editor.c — Unified editor controller shell (Phase 4)
 *
 * Covers init/destroy, load success/fail, mode toggle without camera move,
 * hover invalidation, select, invalid select, input consumption, Escape
 * hierarchy. Material apply is Phase 5 and is not exercised here.
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

#include "../src/unified_editor.h"
#include "../src/assets.h"
#include "../src/camera.h"
#include "../src/config.h"
#include "../src/math.h"

/* ===================================================================
 *  Temp helpers
 * =================================================================== */

static char g_tmpdir[] = "/tmp/tsg_unified_ed_XXXXXX";
static int g_tmpdir_ready = 0;

static int make_tmpdir(void) {
    memcpy(g_tmpdir, "/tmp/tsg_unified_ed_XXXXXX", sizeof("/tmp/tsg_unified_ed_XXXXXX"));
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

static void rm_rf_tmpdir(void) {
    if (!g_tmpdir_ready) return;
    /* Only a few known files; keep simple. */
    char path[512];
    path_in_tmpdir(path, sizeof(path), "map.txt");
    remove(path);
    path_in_tmpdir(path, sizeof(path), "bad.txt");
    remove(path);
    rmdir(g_tmpdir);
    g_tmpdir_ready = 0;
}

/* 5x5: open center, wall on east edge at (4,2) material 1 */
static const char *VALID_MAP =
    "00000\n"
    "00000\n"
    "00001\n"
    "00000\n"
    "00000\n";

static AssetRegistry g_assets;

static int group_setup(void **state) {
    (void)state;
    config_init_defaults();
    if (make_tmpdir() != 0) return -1;
    asset_registry_init(&g_assets);
    /* Mark material 1 as loaded so later phases can use it; Phase 4 does not apply. */
    g_assets.materials[1].id = 1;
    g_assets.material_names[1][0] = '1';
    g_assets.material_names[1][1] = '\0';
    g_assets.material_count = 1;
    return 0;
}

static int group_teardown(void **state) {
    (void)state;
    rm_rf_tmpdir();
    return 0;
}

static void zero_input(InputState *in) {
    memset(in, 0, sizeof(*in));
}

/* ===================================================================
 *  Tests
 * =================================================================== */

static void test_init_destroy(void **state) {
    (void)state;
    UnifiedEditorState ed;
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_true(ed.active);
    assert_int_equal(ed.mode, EDITOR_MODE_WALK);
    assert_int_equal(ed.selection.type, SELECTION_NONE);
    assert_false(ed.hover.valid);
    assert_ptr_equal(ed.assets, &g_assets);
    unified_editor_destroy(&ed);
    assert_false(ed.active);
}

static void test_init_null_rejects(void **state) {
    (void)state;
    UnifiedEditorState ed;
    assert_false(unified_editor_init(NULL, &g_assets));
    assert_false(unified_editor_init(&ed, NULL));
}

static void test_load_success_resets_history_and_selection(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "map.txt");
    assert_int_equal(write_text_file(path, VALID_MAP), 0);

    UnifiedEditorState ed;
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, path), SCENE_LOAD_OK);

    /* Seed selection/history-ish UI state then reload. */
    ed.selection.type = SELECTION_WALL_FACE;
    ed.selection.value.wall_face.map_x = 4;
    ed.selection.value.wall_face.map_y = 2;
    ed.selection.value.wall_face.face = WALL_FACE_WEST;
    ed.inspector_open = true;
    ed.hover.valid = true;
    ed.mode = EDITOR_MODE_EDIT;
    ed.history.count = 3; /* will be wiped by reload path */
    ed.history.cursor = 2;

    assert_int_equal(unified_editor_load_scene(&ed, path), SCENE_LOAD_OK);
    assert_int_equal(ed.selection.type, SELECTION_NONE);
    assert_false(ed.hover.valid);
    assert_false(ed.inspector_open);
    assert_int_equal(ed.history.count, 0);
    assert_int_equal(ed.history.cursor, 0);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);
    assert_int_equal(ed.status, EDITOR_STATUS_NONE);
    assert_false(scene_document_is_dirty(&ed.document));

    unified_editor_destroy(&ed);
}

static void test_load_failure_preserves_state(void **state) {
    (void)state;
    char good[512];
    char bad[512];
    path_in_tmpdir(good, sizeof(good), "map.txt");
    path_in_tmpdir(bad, sizeof(bad), "missing_nope.txt");
    assert_int_equal(write_text_file(good, VALID_MAP), 0);

    UnifiedEditorState ed;
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, good), SCENE_LOAD_OK);

    ed.selection.type = SELECTION_WALL_FACE;
    ed.selection.value.wall_face.map_x = 4;
    ed.selection.value.wall_face.map_y = 2;
    ed.selection.value.wall_face.face = WALL_FACE_WEST;
    ed.inspector_open = true;
    ed.mode = EDITOR_MODE_EDIT;
    DocumentStateId cur = ed.document.current_state;
    DocumentStateId saved = ed.document.saved_state;
    size_t hist_count = ed.history.count;
    DocumentStateId next_id = ed.history.next_state_id;

    SceneLoadResult r = unified_editor_load_scene(&ed, bad);
    assert_int_not_equal(r, SCENE_LOAD_OK);
    assert_int_equal(ed.status, EDITOR_STATUS_LOAD_FAILED);
    assert_int_equal(ed.selection.type, SELECTION_WALL_FACE);
    assert_int_equal(ed.selection.value.wall_face.map_x, 4);
    assert_true(ed.inspector_open);
    assert_int_equal(ed.mode, EDITOR_MODE_EDIT);
    assert_int_equal(ed.document.current_state, cur);
    assert_int_equal(ed.document.saved_state, saved);
    assert_int_equal(ed.history.count, hist_count);
    assert_int_equal(ed.history.next_state_id, next_id);

    unified_editor_destroy(&ed);
}

static void test_tab_toggles_mode_without_moving_camera(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "map.txt");
    assert_int_equal(write_text_file(path, VALID_MAP), 0);

    UnifiedEditorState ed;
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, path), SCENE_LOAD_OK);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);
    double x0 = cam.transform.pos.x;
    double y0 = cam.transform.pos.y;
    double a0 = cam.transform.angle;
    double p0 = cam.pitch;

    InputState in;
    zero_input(&in);
    in.editor_toggle_mode_pressed = true;
    in.forward = true; /* must not move while toggling into edit */
    in.mouse_dx = 40.0f;

    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.mode, EDITOR_MODE_EDIT);
    assert_float_equal(cam.transform.pos.x, x0, 0.0001);
    assert_float_equal(cam.transform.pos.y, y0, 0.0001);
    assert_float_equal(cam.transform.angle, a0, 0.0001);
    assert_float_equal(cam.pitch, p0, 0.0001);


    /* Toggle back to walk */
    zero_input(&in);
    in.editor_toggle_mode_pressed = true;
    c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.mode, EDITOR_MODE_WALK);

    unified_editor_destroy(&ed);
}

static void test_hover_invalidated_each_frame(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "map.txt");
    assert_int_equal(write_text_file(path, VALID_MAP), 0);

    UnifiedEditorState ed;
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, path), SCENE_LOAD_OK);

    Camera cam;
    /* Face open space (west) — no wall hit expected. */
    camera_init(&cam, 2.5, 2.5, PI, PI / 2.0);

    /* Plant a stale hover that must be cleared. */
    ed.hover.valid = true;
    ed.hover.target.type = SELECTION_WALL_FACE;
    ed.hover.target.value.wall_face.map_x = 99;

    InputState in;
    zero_input(&in);
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_false(ed.hover.valid);

    /* Face east wall — hover becomes valid. */
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);
    zero_input(&in);
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(ed.hover.valid);
    assert_int_equal(ed.hover.target.value.wall_face.map_x, 4);
    assert_int_equal(ed.hover.target.value.wall_face.map_y, 2);

    unified_editor_destroy(&ed);
}

static void test_select_copies_valid_hover(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "map.txt");
    assert_int_equal(write_text_file(path, VALID_MAP), 0);

    UnifiedEditorState ed;
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, path), SCENE_LOAD_OK);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);

    InputState in;
    zero_input(&in);
    /* Establish hover */
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(ed.hover.valid);

    zero_input(&in);
    in.editor_select_pressed = true;
    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.selection.type, SELECTION_WALL_FACE);
    assert_int_equal(ed.selection.value.wall_face.map_x, 4);
    assert_int_equal(ed.selection.value.wall_face.map_y, 2);
    assert_int_equal(ed.selection.value.wall_face.face, WALL_FACE_WEST);
    assert_true(ed.inspector_open);
    assert_int_equal(ed.status, EDITOR_STATUS_NONE);

    unified_editor_destroy(&ed);
}

static void test_invalid_select_preserves_prior(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "map.txt");
    assert_int_equal(write_text_file(path, VALID_MAP), 0);

    UnifiedEditorState ed;
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, path), SCENE_LOAD_OK);

    /* Prior selection */
    ed.selection.type = SELECTION_WALL_FACE;
    ed.selection.value.wall_face.map_x = 4;
    ed.selection.value.wall_face.map_y = 2;
    ed.selection.value.wall_face.face = WALL_FACE_WEST;
    ed.inspector_open = true;

    Camera cam;
    /* Aim into empty space so hover is invalid. */
    camera_init(&cam, 2.5, 2.5, PI, PI / 2.0);

    InputState in;
    zero_input(&in);
    in.editor_select_pressed = true;
    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.status, EDITOR_STATUS_INVALID_SELECTION);
    assert_int_equal(ed.selection.type, SELECTION_WALL_FACE);
    assert_int_equal(ed.selection.value.wall_face.map_x, 4);
    assert_int_equal(ed.selection.value.wall_face.map_y, 2);
    assert_true(ed.inspector_open);

    unified_editor_destroy(&ed);
}

static void test_input_consumption_blocks_multi_layer(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "map.txt");
    assert_int_equal(write_text_file(path, VALID_MAP), 0);

    UnifiedEditorState ed;
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, path), SCENE_LOAD_OK);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);

    /* Modal open: select must not change selection. */
    ed.modal = EDITOR_MODAL_EXIT_PROMPT;
    ed.selection.type = SELECTION_NONE;

    InputState in;
    zero_input(&in);
    in.editor_select_pressed = true;
    in.editor_toggle_mode_pressed = true;
    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.selection.type, SELECTION_NONE);
    assert_int_equal(ed.mode, EDITOR_MODE_WALK); /* toggle swallowed */
    assert_int_equal(ed.modal, EDITOR_MODAL_EXIT_PROMPT);

    /* Cancel closes modal and consumes. */
    zero_input(&in);
    in.editor_cancel_pressed = true;
    c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);

    unified_editor_destroy(&ed);
}

static void test_escape_hierarchy_inspector_before_exit(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "map.txt");
    assert_int_equal(write_text_file(path, VALID_MAP), 0);

    UnifiedEditorState ed;
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, path), SCENE_LOAD_OK);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);

    ed.inspector_open = true;
    ed.selection.type = SELECTION_WALL_FACE;
    ed.selection.value.wall_face.map_x = 4;
    ed.selection.value.wall_face.map_y = 2;
    ed.selection.value.wall_face.face = WALL_FACE_WEST;

    InputState in;
    zero_input(&in);
    in.editor_cancel_pressed = true;
    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_false(ed.inspector_open);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);
    /* Selection remains; only inspector closes. */
    assert_int_equal(ed.selection.type, SELECTION_WALL_FACE);

    /* Second Escape opens exit prompt. */
    zero_input(&in);
    in.editor_cancel_pressed = true;
    c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.modal, EDITOR_MODAL_EXIT_PROMPT);

    /* Third Escape cancels exit prompt. */
    zero_input(&in);
    in.editor_cancel_pressed = true;
    c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);
    assert_false(ed.request_exit_to_main_menu);

    unified_editor_destroy(&ed);
}

static void test_edit_mode_consumes_pointer(void **state) {
    (void)state;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "map.txt");
    assert_int_equal(write_text_file(path, VALID_MAP), 0);

    UnifiedEditorState ed;
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, path), SCENE_LOAD_OK);
    ed.mode = EDITOR_MODE_EDIT;

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);
    InputState in;
    zero_input(&in);
    in.mouse_dx = 10.0f;
    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.pointer_consumed);

    unified_editor_destroy(&ed);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_destroy),
        cmocka_unit_test(test_init_null_rejects),
        cmocka_unit_test(test_load_success_resets_history_and_selection),
        cmocka_unit_test(test_load_failure_preserves_state),
        cmocka_unit_test(test_tab_toggles_mode_without_moving_camera),
        cmocka_unit_test(test_hover_invalidated_each_frame),
        cmocka_unit_test(test_select_copies_valid_hover),
        cmocka_unit_test(test_invalid_select_preserves_prior),
        cmocka_unit_test(test_input_consumption_blocks_multi_layer),
        cmocka_unit_test(test_escape_hierarchy_inspector_before_exit),
        cmocka_unit_test(test_edit_mode_consumes_pointer),
    };
    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
