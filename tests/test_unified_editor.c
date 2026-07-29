/**
 * test_unified_editor.c — Unified editor controller (Phases 4–5)
 *
 * Phase 4: init/destroy, load, mode toggle, hover, select, Escape hierarchy.
 * Phase 5: material validation, apply via command history, undo/redo/save,
 *          picker navigation, unsaveable ID reporting.
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

static int read_text_file(const char *path, char *out, size_t out_sz) {
    FILE *fp = fopen(path, "rb");
    size_t n;
    if (!fp) return -1;
    n = fread(out, 1, out_sz - 1, fp);
    out[n] = '\0';
    fclose(fp);
    return 0;
}

static void rm_rf_tmpdir(void) {
    if (!g_tmpdir_ready) return;
    char path[512];
    path_in_tmpdir(path, sizeof(path), "map.txt");
    remove(path);
    path_in_tmpdir(path, sizeof(path), "bad.txt");
    remove(path);
    path_in_tmpdir(path, sizeof(path), "map_saved.txt");
    remove(path);
    path_in_tmpdir(path, sizeof(path), "a_current.txt");
    remove(path);
    path_in_tmpdir(path, sizeof(path), "b_target.txt");
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

static const char *SECOND_MAP =
    "22\n"
    "22\n";

static AssetRegistry g_assets;

static void mark_material_loaded(int id, const char *name) {
    g_assets.materials[id].id = id;
    snprintf(g_assets.material_names[id],
             sizeof(g_assets.material_names[id]),
             "%s", name);
}

static int group_setup(void **state) {
    (void)state;
    config_init_defaults();
    if (make_tmpdir() != 0) return -1;
    asset_registry_init(&g_assets);
    /* Loaded materials: 1, 2, and 12 (unsavable live ID). */
    mark_material_loaded(1, "mat1");
    mark_material_loaded(2, "mat2");
    mark_material_loaded(12, "mat12");
    g_assets.material_count = 3;
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

static void select_east_wall(UnifiedEditorState *ed) {
    ed->selection.type = SELECTION_WALL_FACE;
    ed->selection.value.wall_face.map_x = 4;
    ed->selection.value.wall_face.map_y = 2;
    ed->selection.value.wall_face.face = WALL_FACE_WEST;
    ed->inspector_open = true;
}

static MaterialId wall_mat(const UnifiedEditorState *ed) {
    MaterialId mat = 0;
    WallMaterialRef ref =
        editor_wall_face_to_material_ref(ed->selection.value.wall_face);
    scene_document_get_wall_material(&ed->document, ref, &mat);
    return mat;
}

static MaterialId east_wall_mat(const UnifiedEditorState *ed) {
    MaterialId mat = 0;
    WallMaterialRef ref = {4, 2};

    scene_document_get_wall_material(&ed->document, ref, &mat);
    return mat;
}

static bool grid_contains_text(const Grid *grid, const char *text) {
    size_t text_len;

    if (!grid || !text) return false;
    text_len = strlen(text);
    if (text_len == 0 || text_len > (size_t)grid->width) return false;

    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x <= grid->width - (int)text_len; x++) {
            size_t i;
            for (i = 0; i < text_len; i++) {
                if (grid->cells[y * grid->width + x + (int)i].glyph !=
                    (uint8_t)text[i]) {
                    break;
                }
            }
            if (i == text_len) return true;
        }
    }
    return false;
}

static int load_editor(UnifiedEditorState *ed, const char *name) {
    char path[512];
    path_in_tmpdir(path, sizeof(path), name);
    if (write_text_file(path, VALID_MAP) != 0) return -1;
    if (!unified_editor_init(ed, &g_assets)) return -1;
    if (unified_editor_load_scene(ed, path) != SCENE_LOAD_OK) {
        unified_editor_destroy(ed);
        return -1;
    }
    return 0;
}

static EditorInputConsumption update_with(
    UnifiedEditorState *ed,
    Camera *cam,
    InputState *in
) {
    return unified_editor_update(ed, in, cam, 0.016);
}

static int prepare_catalog_maps(char *first, size_t first_size,
                                char *second, size_t second_size) {
    path_in_tmpdir(first, first_size, "a_current.txt");
    path_in_tmpdir(second, second_size, "b_target.txt");
    if (write_text_file(first, VALID_MAP) != 0) return -1;
    if (write_text_file(second, SECOND_MAP) != 0) return -1;
    return 0;
}

static void test_initial_chooser_load_and_escape(void **state) {
    UnifiedEditorState ed;
    Camera cam;
    InputState in;
    char first[512];
    char second[512];
    (void)state;

    assert_int_equal(prepare_catalog_maps(first, sizeof(first), second,
                                          sizeof(second)), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_begin_map_open(&ed, g_tmpdir), MAP_CATALOG_OK);
    assert_false(unified_editor_has_document(&ed));
    assert_int_equal(ed.modal, EDITOR_MODAL_MAP_CHOOSER);
    camera_init(&cam, 7.0, 8.0, 0.25, PI / 2.0);

    zero_input(&in);
    in.editor_confirm_pressed = true;
    assert_true(update_with(&ed, &cam, &in).keyboard_consumed);
    assert_true(unified_editor_has_document(&ed));
    assert_string_equal(ed.document.path, first);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);
    assert_float_equal(cam.transform.pos.x, 7.0, 0.0001);

    unified_editor_destroy(&ed);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_begin_map_open(&ed, g_tmpdir), MAP_CATALOG_OK);
    zero_input(&in);
    in.editor_cancel_pressed = true;
    update_with(&ed, &cam, &in);
    assert_true(ed.request_exit_to_main_menu);
    assert_false(unified_editor_has_document(&ed));
    unified_editor_destroy(&ed);
    remove(first);
    remove(second);
}

static void test_ctrl_o_and_catalog_failure_preserve_document(void **state) {
    UnifiedEditorState ed;
    Camera cam;
    InputState in;
    char first[512];
    char second[512];
    char missing_root[512];
    char *root_before;
    SelectionTarget selection_before;
    (void)state;

    assert_int_equal(prepare_catalog_maps(first, sizeof(first), second,
                                          sizeof(second)), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, first), SCENE_LOAD_OK);
    assert_int_equal(unified_editor_begin_map_open(&ed, g_tmpdir), MAP_CATALOG_OK);
    zero_input(&in);
    in.editor_cancel_pressed = true;
    camera_init(&cam, 4.0, 5.0, 0.5, PI / 2.0);
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);

    select_east_wall(&ed);
    selection_before = ed.selection;
    zero_input(&in);
    in.editor_open_pressed = true;
    assert_true(update_with(&ed, &cam, &in).keyboard_consumed);
    assert_int_equal(ed.modal, EDITOR_MODAL_MAP_CHOOSER);
    assert_int_equal(ed.map_catalog.count, 2);

    root_before = ed.map_root;
    path_in_tmpdir(missing_root, sizeof(missing_root), "missing-directory");
    assert_int_equal(unified_editor_begin_map_open(&ed, missing_root),
                     MAP_CATALOG_OPEN_FAILED);
    assert_int_equal(ed.modal, EDITOR_MODAL_MAP_CHOOSER);
    assert_int_equal(ed.status, EDITOR_STATUS_CATALOG_FAILED);
    assert_ptr_equal(ed.map_root, root_before);
    assert_string_equal(ed.map_root, g_tmpdir);
    assert_int_equal(ed.map_catalog.count, 2);
    assert_string_equal(ed.document.path, first);
    assert_memory_equal(&ed.selection, &selection_before,
                        sizeof(selection_before));
    assert_true(ed.inspector_open);
    assert_float_equal(cam.transform.pos.x, 4.0, 0.0001);

    unified_editor_destroy(&ed);
    remove(first);
    remove(second);
}

static void test_clean_switch_and_failed_load_preserve(void **state) {
    UnifiedEditorState ed;
    Camera cam;
    InputState in;
    char first[512];
    char second[512];
    SelectionTarget selection_before;
    (void)state;

    assert_int_equal(prepare_catalog_maps(first, sizeof(first), second,
                                          sizeof(second)), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, first), SCENE_LOAD_OK);
    assert_int_equal(unified_editor_begin_map_open(&ed, g_tmpdir), MAP_CATALOG_OK);
    ed.map_chooser_index = 1;
    camera_init(&cam, 4.0, 5.0, 0.5, PI / 2.0);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    assert_string_equal(ed.document.path, second);
    assert_int_equal(ed.document.map.width, 2);
    assert_int_equal(ed.history.count, 0);
    assert_float_equal(cam.transform.pos.y, 5.0, 0.0001);

    assert_int_equal(unified_editor_load_scene(&ed, first), SCENE_LOAD_OK);
    select_east_wall(&ed);
    selection_before = ed.selection;
    assert_int_equal(unified_editor_begin_map_open(&ed, g_tmpdir), MAP_CATALOG_OK);
    ed.map_chooser_index = 1;
    assert_int_equal(remove(second), 0);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.modal, EDITOR_MODAL_MAP_CHOOSER);
    assert_int_equal(ed.status, EDITOR_STATUS_LOAD_FAILED);
    assert_string_equal(ed.document.path, first);
    assert_memory_equal(&ed.selection, &selection_before, sizeof(selection_before));
    assert_true(ed.inspector_open);
    assert_float_equal(cam.transform.pos.x, 4.0, 0.0001);

    unified_editor_destroy(&ed);
    remove(first);
}

static void test_dirty_switch_cancel_discard_and_failure(void **state) {
    UnifiedEditorState ed;
    Camera cam;
    InputState in;
    char first[512];
    char second[512];
    (void)state;

    assert_int_equal(prepare_catalog_maps(first, sizeof(first), second,
                                          sizeof(second)), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, first), SCENE_LOAD_OK);
    select_east_wall(&ed);
    assert_int_equal(unified_editor_set_wall_material(&ed, 2), CMD_RESULT_OK);
    assert_int_equal(unified_editor_begin_map_open(&ed, g_tmpdir), MAP_CATALOG_OK);
    ed.map_chooser_index = 1;
    camera_init(&cam, 3.0, 3.0, 0.0, PI / 2.0);

    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.modal, EDITOR_MODAL_DIRTY_OPEN_PROMPT);
    assert_int_equal(ed.dirty_open_choice, EDITOR_DIRTY_OPEN_CANCEL);
    zero_input(&in);
    in.editor_cancel_pressed = true;
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.modal, EDITOR_MODAL_MAP_CHOOSER);
    assert_true(scene_document_is_dirty(&ed.document));
    assert_string_equal(ed.document.path, first);

    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    zero_input(&in);
    in.editor_previous_pressed = true;
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.dirty_open_choice, EDITOR_DIRTY_OPEN_DISCARD);
    assert_int_equal(remove(second), 0);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.modal, EDITOR_MODAL_MAP_CHOOSER);
    assert_true(scene_document_is_dirty(&ed.document));
    assert_string_equal(ed.document.path, first);
    assert_int_equal(east_wall_mat(&ed), 2);
    assert_int_equal(ed.history.count, 1);

    assert_int_equal(write_text_file(second, SECOND_MAP), 0);
    assert_int_equal(unified_editor_begin_map_open(&ed, NULL), MAP_CATALOG_OK);
    ed.map_chooser_index = 1;
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    zero_input(&in);
    in.editor_previous_pressed = true;
    update_with(&ed, &cam, &in);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    assert_string_equal(ed.document.path, second);
    assert_false(scene_document_is_dirty(&ed.document));
    assert_int_equal(ed.selection.type, SELECTION_NONE);
    assert_int_equal(ed.history.count, 0);

    unified_editor_destroy(&ed);
    remove(first);
    remove(second);
}

static void test_dirty_save_success_then_switch_and_save_failure(void **state) {
    UnifiedEditorState ed;
    Camera cam;
    InputState in;
    char first[512];
    char second[512];
    char saved[256];
    (void)state;

    assert_int_equal(prepare_catalog_maps(first, sizeof(first), second,
                                          sizeof(second)), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, first), SCENE_LOAD_OK);
    select_east_wall(&ed);
    assert_int_equal(unified_editor_set_wall_material(&ed, 2), CMD_RESULT_OK);
    assert_int_equal(unified_editor_begin_map_open(&ed, g_tmpdir), MAP_CATALOG_OK);
    ed.map_chooser_index = 1;
    camera_init(&cam, 3.0, 3.0, 0.0, PI / 2.0);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    zero_input(&in);
    in.editor_next_pressed = true;
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.dirty_open_choice, EDITOR_DIRTY_OPEN_SAVE);
    assert_int_equal(remove(second), 0);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.modal, EDITOR_MODAL_MAP_CHOOSER);
    assert_int_equal(ed.status, EDITOR_STATUS_LOAD_FAILED);
    assert_string_equal(ed.document.path, first);
    assert_false(scene_document_is_dirty(&ed.document));
    assert_int_equal(east_wall_mat(&ed), 2);
    assert_int_equal(ed.history.count, 1);
    assert_int_equal(read_text_file(first, saved, sizeof(saved)), 0);
    assert_non_null(strstr(saved, "2"));

    assert_int_equal(write_text_file(second, SECOND_MAP), 0);
    assert_int_equal(unified_editor_begin_map_open(&ed, NULL), MAP_CATALOG_OK);
    ed.map_chooser_index = 1;
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    assert_string_equal(ed.document.path, second);

    assert_int_equal(unified_editor_load_scene(&ed, first), SCENE_LOAD_OK);
    select_east_wall(&ed);
    assert_int_equal(unified_editor_set_wall_material(&ed, 12), CMD_RESULT_OK);
    assert_int_equal(unified_editor_begin_map_open(&ed, NULL), MAP_CATALOG_OK);
    ed.map_chooser_index = 1;
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    zero_input(&in);
    in.editor_next_pressed = true;
    update_with(&ed, &cam, &in);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.modal, EDITOR_MODAL_DIRTY_OPEN_PROMPT);
    assert_int_equal(ed.status, EDITOR_STATUS_UNSAVABLE_MATERIAL_ID);
    assert_string_equal(ed.document.path, first);
    assert_true(scene_document_is_dirty(&ed.document));
    assert_int_equal(east_wall_mat(&ed), 12);
    assert_int_equal(ed.history.count, 1);

    unified_editor_destroy(&ed);
    remove(first);
    remove(second);
}

static void test_authoritative_map_updates_without_state_reset(void **state) {
    (void)state;
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);
    select_east_wall(&ed);

    Map *map_before = scene_document_get_map_for_runtime(&ed.document);
    AssetRegistry *assets_before = ed.assets;
    SelectionTarget selection_before = ed.selection;

    assert_int_equal(unified_editor_set_wall_material(&ed, 2), CMD_RESULT_OK);
    assert_ptr_equal(scene_document_get_map_for_runtime(&ed.document), map_before);
    assert_ptr_equal(ed.assets, assets_before);
    assert_int_equal(map_get(map_before, 4, 2)->material_id, 2);
    assert_memory_equal(&ed.selection, &selection_before, sizeof(selection_before));
    assert_true(ed.inspector_open);

    assert_int_equal(unified_editor_undo(&ed), CMD_RESULT_OK);
    assert_ptr_equal(scene_document_get_map_for_runtime(&ed.document), map_before);
    assert_int_equal(map_get(map_before, 4, 2)->material_id, 1);

    assert_int_equal(unified_editor_redo(&ed), CMD_RESULT_OK);
    assert_ptr_equal(scene_document_get_map_for_runtime(&ed.document), map_before);
    assert_int_equal(map_get(map_before, 4, 2)->material_id, 2);

    unified_editor_destroy(&ed);
}

static void test_overlay_marks_unloaded_selected_material_missing(void **state) {
    (void)state;
    char path[512];
    UnifiedEditorState ed;
    Grid *grid;

    path_in_tmpdir(path, sizeof(path), "map.txt");
    assert_int_equal(write_text_file(path, "3\n"), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, path), SCENE_LOAD_OK);
    ed.selection.type = SELECTION_WALL_FACE;
    ed.selection.value.wall_face = (WallFaceRef){0, 0, WALL_FACE_WEST};

    grid = grid_create(100, 30);
    assert_non_null(grid);
    unified_editor_render_overlay(&ed, grid);
    assert_true(grid_contains_text(grid, "mat:3 (missing)"));

    grid_destroy(grid);
    unified_editor_destroy(&ed);
}

/* ===================================================================
 *  Phase 4 tests
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

    ed.selection.type = SELECTION_WALL_FACE;
    ed.selection.value.wall_face.map_x = 4;
    ed.selection.value.wall_face.map_y = 2;
    ed.selection.value.wall_face.face = WALL_FACE_WEST;
    ed.inspector_open = true;
    ed.hover.valid = true;
    ed.mode = EDITOR_MODE_EDIT;
    ed.history.count = 3;
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
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);
    double x0 = cam.transform.pos.x;
    double y0 = cam.transform.pos.y;
    double a0 = cam.transform.angle;
    double p0 = cam.pitch;

    InputState in;
    zero_input(&in);
    in.editor_toggle_mode_pressed = true;
    in.forward = true;
    in.mouse_dx = 40.0f;

    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.mode, EDITOR_MODE_EDIT);
    assert_float_equal(cam.transform.pos.x, x0, 0.0001);
    assert_float_equal(cam.transform.pos.y, y0, 0.0001);
    assert_float_equal(cam.transform.angle, a0, 0.0001);
    assert_float_equal(cam.pitch, p0, 0.0001);

    zero_input(&in);
    in.editor_toggle_mode_pressed = true;
    c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.mode, EDITOR_MODE_WALK);

    unified_editor_destroy(&ed);
}

static void test_hover_invalidated_each_frame(void **state) {
    (void)state;
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, PI, PI / 2.0);

    ed.hover.valid = true;
    ed.hover.target.type = SELECTION_WALL_FACE;
    ed.hover.target.value.wall_face.map_x = 99;

    InputState in;
    zero_input(&in);
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_false(ed.hover.valid);

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
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);

    InputState in;
    zero_input(&in);
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
    /* Picker should land on current wall material (1). */
    assert_int_equal(ed.highlighted_material, 1);

    unified_editor_destroy(&ed);
}

static void test_invalid_select_preserves_prior(void **state) {
    (void)state;
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);

    select_east_wall(&ed);

    Camera cam;
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
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);

    ed.modal = EDITOR_MODAL_EXIT_PROMPT;
    ed.selection.type = SELECTION_NONE;

    InputState in;
    zero_input(&in);
    in.editor_select_pressed = true;
    in.editor_toggle_mode_pressed = true;
    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.selection.type, SELECTION_NONE);
    assert_int_equal(ed.mode, EDITOR_MODE_WALK);
    assert_int_equal(ed.modal, EDITOR_MODAL_EXIT_PROMPT);

    zero_input(&in);
    in.editor_cancel_pressed = true;
    c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);

    unified_editor_destroy(&ed);
}

static void test_escape_hierarchy_inspector_before_exit(void **state) {
    (void)state;
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);

    select_east_wall(&ed);

    InputState in;
    zero_input(&in);
    in.editor_cancel_pressed = true;
    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_false(ed.inspector_open);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);
    assert_int_equal(ed.selection.type, SELECTION_NONE);

    zero_input(&in);
    in.editor_cancel_pressed = true;
    c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.modal, EDITOR_MODAL_EXIT_PROMPT);

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
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);
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

/* ===================================================================
 *  Phase 5 tests
 * =================================================================== */

static void test_set_material_requires_selection(void **state) {
    (void)state;
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);

    CommandResult r = unified_editor_set_wall_material(&ed, 2);
    assert_int_equal(r, CMD_RESULT_INVALID_TARGET);
    assert_int_equal(ed.status, EDITOR_STATUS_INVALID_SELECTION);
    assert_false(scene_document_is_dirty(&ed.document));

    unified_editor_destroy(&ed);
}

static void test_set_material_rejects_unloaded(void **state) {
    (void)state;
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);
    select_east_wall(&ed);

    CommandResult r = unified_editor_set_wall_material(&ed, 99);
    assert_int_equal(r, CMD_RESULT_INVALID_TARGET);
    assert_int_equal(ed.status, EDITOR_STATUS_INVALID_MATERIAL);
    assert_int_equal(wall_mat(&ed), 1);
    assert_false(scene_document_is_dirty(&ed.document));
    assert_int_equal(ed.history.count, 0);

    unified_editor_destroy(&ed);
}

static void test_set_material_applies_and_dirties(void **state) {
    (void)state;
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);
    select_east_wall(&ed);

    assert_int_equal(wall_mat(&ed), 1);
    CommandResult r = unified_editor_set_wall_material(&ed, 2);
    assert_int_equal(r, CMD_RESULT_OK);
    assert_int_equal(wall_mat(&ed), 2);
    assert_true(scene_document_is_dirty(&ed.document));
    assert_int_equal(ed.history.count, 1);
    assert_int_equal(ed.history.cursor, 1);
    assert_int_equal(ed.status, EDITOR_STATUS_NONE);

    /* No-change assignment: no new history entry. */
    r = unified_editor_set_wall_material(&ed, 2);
    assert_int_equal(r, CMD_RESULT_NO_CHANGE);
    assert_int_equal(ed.history.count, 1);

    unified_editor_destroy(&ed);
}

static void test_undo_redo_via_wrappers(void **state) {
    (void)state;
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);
    select_east_wall(&ed);

    assert_int_equal(unified_editor_set_wall_material(&ed, 2), CMD_RESULT_OK);
    assert_int_equal(wall_mat(&ed), 2);

    assert_int_equal(unified_editor_undo(&ed), CMD_RESULT_OK);
    assert_int_equal(wall_mat(&ed), 1);
    assert_false(scene_document_is_dirty(&ed.document));

    assert_int_equal(unified_editor_redo(&ed), CMD_RESULT_OK);
    assert_int_equal(wall_mat(&ed), 2);
    assert_true(scene_document_is_dirty(&ed.document));

    assert_int_equal(unified_editor_undo(&ed), CMD_RESULT_OK);
    assert_int_equal(unified_editor_undo(&ed), CMD_RESULT_NOTHING_TO_UNDO);

    unified_editor_destroy(&ed);
}

static void test_unsaveable_material_id_status(void **state) {
    (void)state;
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);
    select_east_wall(&ed);

    CommandResult r = unified_editor_set_wall_material(&ed, 12);
    assert_int_equal(r, CMD_RESULT_OK);
    assert_int_equal(wall_mat(&ed), 12);
    assert_int_equal(ed.status, EDITOR_STATUS_UNSAVABLE_MATERIAL_ID);
    assert_true(scene_document_is_dirty(&ed.document));

    SceneSaveResult sr = unified_editor_save(&ed);
    assert_int_equal(sr, SCENE_SAVE_UNREPRESENTABLE_MATERIAL);
    assert_int_equal(ed.status, EDITOR_STATUS_UNSAVABLE_MATERIAL_ID);
    /* History and edit preserved after failed save. */
    assert_int_equal(ed.history.count, 1);
    assert_int_equal(wall_mat(&ed), 12);
    assert_true(scene_document_is_dirty(&ed.document));

    /* Undo restores savable state and clears unsaveable status. */
    assert_int_equal(unified_editor_undo(&ed), CMD_RESULT_OK);
    assert_int_equal(wall_mat(&ed), 1);
    assert_int_not_equal(ed.status, EDITOR_STATUS_UNSAVABLE_MATERIAL_ID);

    unified_editor_destroy(&ed);
}

static void test_save_success_clears_dirty(void **state) {
    (void)state;
    UnifiedEditorState ed;
    char path[512];
    char buf[256];

    path_in_tmpdir(path, sizeof(path), "map_saved.txt");
    assert_int_equal(write_text_file(path, VALID_MAP), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, path), SCENE_LOAD_OK);
    select_east_wall(&ed);

    assert_int_equal(unified_editor_set_wall_material(&ed, 2), CMD_RESULT_OK);
    assert_true(scene_document_is_dirty(&ed.document));

    assert_int_equal(unified_editor_save(&ed), SCENE_SAVE_OK);
    assert_int_equal(ed.status, EDITOR_STATUS_SAVED);
    assert_false(scene_document_is_dirty(&ed.document));

    assert_int_equal(read_text_file(path, buf, sizeof(buf)), 0);
    assert_non_null(strstr(buf, "2"));

    unified_editor_destroy(&ed);
}

static void test_picker_next_prev_and_confirm(void **state) {
    (void)state;
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);

    /* Select wall so inspector opens with material 1 highlighted. */
    InputState in;
    zero_input(&in);
    unified_editor_update(&ed, &in, &cam, 0.016);
    zero_input(&in);
    in.editor_select_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(ed.inspector_open);
    assert_int_equal(ed.highlighted_material, 1);

    /* Next → material 2 */
    zero_input(&in);
    in.editor_next_pressed = true;
    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.highlighted_material, 2);

    /* Next → material 12 */
    zero_input(&in);
    in.editor_next_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(ed.highlighted_material, 12);

    /* Prev → material 2 */
    zero_input(&in);
    in.editor_previous_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(ed.highlighted_material, 2);

    /* Confirm applies highlighted material. */
    zero_input(&in);
    in.editor_confirm_pressed = true;
    c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(wall_mat(&ed), 2);
    assert_true(scene_document_is_dirty(&ed.document));

    unified_editor_destroy(&ed);
}

static void test_input_undo_redo_save_shortcuts(void **state) {
    (void)state;
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);
    select_east_wall(&ed);
    assert_int_equal(unified_editor_set_wall_material(&ed, 2), CMD_RESULT_OK);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);
    InputState in;

    zero_input(&in);
    in.editor_undo_pressed = true;
    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(wall_mat(&ed), 1);

    zero_input(&in);
    in.editor_redo_pressed = true;
    c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(wall_mat(&ed), 2);

    zero_input(&in);
    in.editor_save_pressed = true;
    c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.status, EDITOR_STATUS_SAVED);
    assert_false(scene_document_is_dirty(&ed.document));

    unified_editor_destroy(&ed);
}

static void test_reload_prompt_when_dirty(void **state) {
    (void)state;
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);
    select_east_wall(&ed);
    assert_int_equal(unified_editor_set_wall_material(&ed, 2), CMD_RESULT_OK);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);
    InputState in;

    zero_input(&in);
    in.editor_reload_pressed = true;
    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.modal, EDITOR_MODAL_RELOAD_PROMPT);
    /* Edit still present until confirmed. */
    assert_int_equal(wall_mat(&ed), 2);

    /* Confirm reload discards dirty edit. */
    zero_input(&in);
    in.editor_confirm_pressed = true;
    c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);
    assert_false(scene_document_is_dirty(&ed.document));
    assert_int_equal(ed.selection.type, SELECTION_NONE);

    unified_editor_destroy(&ed);
}

/* ===================================================================
 *  Phase 6 — exit prompt + vertical-slice acceptance
 * =================================================================== */

static void test_exit_resume_default_does_not_discard(void **state) {
    (void)state;
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);
    select_east_wall(&ed);
    assert_int_equal(unified_editor_set_wall_material(&ed, 2), CMD_RESULT_OK);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);
    InputState in;

    /* Close inspector, then open exit prompt. */
    zero_input(&in);
    in.editor_cancel_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    zero_input(&in);
    in.editor_cancel_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(ed.modal, EDITOR_MODAL_EXIT_PROMPT);
    assert_int_equal(ed.exit_choice, EDITOR_EXIT_RESUME);

    /* Enter on Resume: stay in editor, keep dirty edit. */
    zero_input(&in);
    in.editor_confirm_pressed = true;
    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);
    assert_false(ed.request_exit_to_main_menu);
    assert_true(scene_document_is_dirty(&ed.document));
    assert_int_equal(east_wall_mat(&ed), 2);

    unified_editor_destroy(&ed);
}

static void test_exit_save_and_exit_persists(void **state) {
    (void)state;
    UnifiedEditorState ed;
    char path[512];
    char buf[256];

    path_in_tmpdir(path, sizeof(path), "map_saved.txt");
    assert_int_equal(write_text_file(path, VALID_MAP), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, path), SCENE_LOAD_OK);
    select_east_wall(&ed);
    assert_int_equal(unified_editor_set_wall_material(&ed, 2), CMD_RESULT_OK);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);
    InputState in;

    zero_input(&in);
    in.editor_cancel_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    zero_input(&in);
    in.editor_cancel_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(ed.modal, EDITOR_MODAL_EXIT_PROMPT);

    /* Navigate Resume -> Save and Exit */
    zero_input(&in);
    in.editor_next_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(ed.exit_choice, EDITOR_EXIT_SAVE_AND_EXIT);

    zero_input(&in);
    in.editor_confirm_pressed = true;
    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_true(ed.request_exit_to_main_menu);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);
    assert_false(scene_document_is_dirty(&ed.document));
    assert_int_equal(ed.status, EDITOR_STATUS_SAVED);

    assert_int_equal(read_text_file(path, buf, sizeof(buf)), 0);
    assert_non_null(strstr(buf, "2"));

    unified_editor_destroy(&ed);
}

static void test_exit_discard_and_exit_does_not_write(void **state) {
    (void)state;
    UnifiedEditorState ed;
    char path[512];
    char buf[256];

    path_in_tmpdir(path, sizeof(path), "map_saved.txt");
    assert_int_equal(write_text_file(path, VALID_MAP), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, path), SCENE_LOAD_OK);
    select_east_wall(&ed);
    assert_int_equal(unified_editor_set_wall_material(&ed, 2), CMD_RESULT_OK);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);
    InputState in;

    zero_input(&in);
    in.editor_cancel_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    zero_input(&in);
    in.editor_cancel_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);

    /* Resume -> Save and Exit -> Discard and Exit */
    zero_input(&in);
    in.editor_next_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    zero_input(&in);
    in.editor_next_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(ed.exit_choice, EDITOR_EXIT_DISCARD_AND_EXIT);

    zero_input(&in);
    in.editor_confirm_pressed = true;
    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_true(ed.request_exit_to_main_menu);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);

    /* File on disk unchanged (still material 1). */
    assert_int_equal(read_text_file(path, buf, sizeof(buf)), 0);
    assert_null(strstr(buf, "2"));
    assert_non_null(strstr(buf, "1"));

    unified_editor_destroy(&ed);
}

static void test_exit_save_failure_blocks_exit(void **state) {
    (void)state;
    UnifiedEditorState ed;
    assert_int_equal(load_editor(&ed, "map.txt"), 0);
    select_east_wall(&ed);
    assert_int_equal(unified_editor_set_wall_material(&ed, 12), CMD_RESULT_OK);

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);
    InputState in;

    zero_input(&in);
    in.editor_cancel_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    zero_input(&in);
    in.editor_cancel_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);

    zero_input(&in);
    in.editor_next_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(ed.exit_choice, EDITOR_EXIT_SAVE_AND_EXIT);

    zero_input(&in);
    in.editor_confirm_pressed = true;
    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_false(ed.request_exit_to_main_menu);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);
    assert_int_equal(ed.status, EDITOR_STATUS_UNSAVABLE_MATERIAL_ID);
    assert_true(scene_document_is_dirty(&ed.document));
    assert_int_equal(east_wall_mat(&ed), 12);

    unified_editor_destroy(&ed);
}

static void test_phase6_vertical_slice_acceptance(void **state) {
    (void)state;
    /*
     * Headless acceptance of plan §9 / Phase 6 checklist items that are
     * engine-boundary testable without SDL interactive play:
     *   open scene → hover → select → apply material → undo/redo →
     *   save → reload → dirty exit choices → failed-save blocks exit.
     */
    UnifiedEditorState ed;
    char path[512];
    char buf[256];
    MaterialId mat = 0;
    WallMaterialRef ref;

    path_in_tmpdir(path, sizeof(path), "map_saved.txt");
    assert_int_equal(write_text_file(path, VALID_MAP), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, path), SCENE_LOAD_OK);
    assert_false(scene_document_is_dirty(&ed.document));

    Camera cam;
    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);
    InputState in;

    /* Hover + select wall under crosshair. */
    zero_input(&in);
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(ed.hover.valid);
    zero_input(&in);
    in.editor_select_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(ed.selection.type, SELECTION_WALL_FACE);
    assert_true(ed.inspector_open);
    assert_int_equal(ed.highlighted_material, 1);

    /* Apply material 2 via picker. */
    zero_input(&in);
    in.editor_next_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(ed.highlighted_material, 2);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(wall_mat(&ed), 2);
    assert_true(scene_document_is_dirty(&ed.document));

    /* Undo / redo. */
    zero_input(&in);
    in.editor_undo_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(wall_mat(&ed), 1);
    assert_false(scene_document_is_dirty(&ed.document));
    zero_input(&in);
    in.editor_redo_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(wall_mat(&ed), 2);
    assert_true(scene_document_is_dirty(&ed.document));

    /* Save clears dirty and persists. */
    zero_input(&in);
    in.editor_save_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(ed.status, EDITOR_STATUS_SAVED);
    assert_false(scene_document_is_dirty(&ed.document));
    assert_int_equal(read_text_file(path, buf, sizeof(buf)), 0);
    assert_non_null(strstr(buf, "2"));

    /* Dirty again, then reload prompt discards. */
    assert_int_equal(unified_editor_set_wall_material(&ed, 1), CMD_RESULT_OK);
    assert_true(scene_document_is_dirty(&ed.document));
    zero_input(&in);
    in.editor_reload_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(ed.modal, EDITOR_MODAL_RELOAD_PROMPT);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);
    assert_false(scene_document_is_dirty(&ed.document));
    /* Reloaded file still has material 2 from prior save. */
    ref.map_x = 4;
    ref.map_y = 2;
    assert_true(scene_document_get_wall_material(&ed.document, ref, &mat));
    assert_int_equal(mat, 2);

    /* Dirty exit: Resume keeps edit; Discard exits without write. */
    select_east_wall(&ed);
    assert_int_equal(unified_editor_set_wall_material(&ed, 1), CMD_RESULT_OK);
    zero_input(&in);
    in.editor_cancel_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    zero_input(&in);
    in.editor_cancel_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(ed.exit_choice, EDITOR_EXIT_RESUME);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_false(ed.request_exit_to_main_menu);
    assert_true(scene_document_is_dirty(&ed.document));

    zero_input(&in);
    in.editor_cancel_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    zero_input(&in);
    in.editor_next_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    zero_input(&in);
    in.editor_next_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(ed.exit_choice, EDITOR_EXIT_DISCARD_AND_EXIT);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(ed.request_exit_to_main_menu);
    assert_int_equal(read_text_file(path, buf, sizeof(buf)), 0);
    assert_non_null(strstr(buf, "2")); /* disk still saved value */

    unified_editor_destroy(&ed);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        /* R0 current-map open/switch workflow */
        cmocka_unit_test(test_initial_chooser_load_and_escape),
        cmocka_unit_test(test_ctrl_o_and_catalog_failure_preserve_document),
        cmocka_unit_test(test_clean_switch_and_failed_load_preserve),
        cmocka_unit_test(test_dirty_switch_cancel_discard_and_failure),
        cmocka_unit_test(test_dirty_save_success_then_switch_and_save_failure),
        /* Phase 8 contract regressions */
        cmocka_unit_test(test_authoritative_map_updates_without_state_reset),
        cmocka_unit_test(test_overlay_marks_unloaded_selected_material_missing),
        /* Phase 4 */
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
        /* Phase 5 */
        cmocka_unit_test(test_set_material_requires_selection),
        cmocka_unit_test(test_set_material_rejects_unloaded),
        cmocka_unit_test(test_set_material_applies_and_dirties),
        cmocka_unit_test(test_undo_redo_via_wrappers),
        cmocka_unit_test(test_unsaveable_material_id_status),
        cmocka_unit_test(test_save_success_clears_dirty),
        cmocka_unit_test(test_picker_next_prev_and_confirm),
        cmocka_unit_test(test_input_undo_redo_save_shortcuts),
        cmocka_unit_test(test_reload_prompt_when_dirty),
        /* Phase 6 */
        cmocka_unit_test(test_exit_resume_default_does_not_discard),
        cmocka_unit_test(test_exit_save_and_exit_persists),
        cmocka_unit_test(test_exit_discard_and_exit_does_not_write),
        cmocka_unit_test(test_exit_save_failure_blocks_exit),
        cmocka_unit_test(test_phase6_vertical_slice_acceptance),
    };
    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}

