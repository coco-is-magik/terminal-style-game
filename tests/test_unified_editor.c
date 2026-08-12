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

/* Editor behavior tests use the production logical viewport height. */
#define unified_editor_update(editor, input, camera, delta) \
    unified_editor_update((editor), (input), (camera), (delta), 160)
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
    path_in_tmpdir(path, sizeof(path), "r2_legacy.txt");
    remove(path);
    path_in_tmpdir(path, sizeof(path), "r2_native.tscene");
    remove(path);
    path_in_tmpdir(path, sizeof(path), "r2_saved.tscene");
    remove(path);
    path_in_tmpdir(path, sizeof(path), "native_with_content.tscene");
    remove(path);
    path_in_tmpdir(path, sizeof(path), "light_pick.tscene");
    remove(path);
    path_in_tmpdir(path, sizeof(path), "new_interactive.tscene");
    remove(path);
    path_in_tmpdir(path, sizeof(path), "import_interactive.tscene");
    remove(path);
    path_in_tmpdir(path, sizeof(path), "dirty_saved.tscene");
    remove(path);
    path_in_tmpdir(path, sizeof(path), "shortcut_saved.tscene");
    remove(path);
    path_in_tmpdir(path, sizeof(path), "exit_saved.tscene");
    remove(path);
    path_in_tmpdir(path, sizeof(path), "phase6_saved.tscene");
    remove(path);
    path_in_tmpdir(path, sizeof(path), "open_a.tscene");
    remove(path);
    path_in_tmpdir(path, sizeof(path), "open_b.tscene");
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
    "222\n"
    "202\n"
    "222\n";

static const char *NATIVE_SCENE =
    "scene_type = terminal_scene\nscene_version = 3\nname = \"workflow\"\n"
    "width = 3\nheight = 3\norigin_x = 0\norigin_y = 0\n"
    "next_instance_id = 1\nambient_intensity = 0.25\n"
    "spawn = 1.5,1.5,0\neast_growth = -\nsouth_growth = -\n\n"
    "[occupancy]\n1 1 1\n1 0 1\n1 1 1\n\n"
    "[wall_materials]\n001 001 001\n001 001 001\n001 001 001\n\n"
    "[floor_materials]\n001 001 001\n001 001 001\n001 001 001\n\n"
    "[ceiling_materials]\n001 001 001\n001 001 001\n001 001 001\n";


static const char *NATIVE_SCENE_WITH_PICKABLE_LIGHT =
    "scene_type = terminal_scene\nscene_version = 3\nname = \"light_pick\"\n"
    "width = 5\nheight = 5\norigin_x = 0\norigin_y = 0\n"
    "next_instance_id = 12\nambient_intensity = 0.2\n"
    "spawn = 1.5,2.5,0\neast_growth = -\nsouth_growth = -\n\n"
    "[occupancy]\n1 1 1 1 1\n1 0 0 0 1\n1 0 0 0 1\n"
    "1 0 0 0 1\n1 1 1 1 1\n\n"
    "[wall_materials]\n001 001 001 001 001\n001 001 001 001 001\n"
    "001 001 001 001 001\n001 001 001 001 001\n001 001 001 001 001\n\n"
    "[floor_materials]\n001 001 001 001 001\n001 001 001 001 001\n"
    "001 001 001 001 001\n001 001 001 001 001\n001 001 001 001 001\n\n"
    "[ceiling_materials]\n001 001 001 001 001\n001 001 001 001 001\n"
    "001 001 001 001 001\n001 001 001 001 001\n001 001 001 001 001\n\n"
    "[light 11]\nposition = 2.5,2.5\n"
    "color = 255,255,255,255\nintensity = 1\nradius = 3\n";

static const char *NATIVE_SCENE_WITH_CONTENT =
    "scene_type = terminal_scene\nscene_version = 3\nname = \"content\"\n"
    "width = 3\nheight = 3\norigin_x = 0\norigin_y = 0\n"
    "next_instance_id = 3\nambient_intensity = 0.3\n"
    "spawn = 1.5,1.5,0\neast_growth = -\nsouth_growth = -\n\n"
    "[occupancy]\n1 1 1\n1 0 1\n1 1 1\n\n"
    "[wall_materials]\n001 001 001\n001 001 001\n001 001 001\n\n"
    "[floor_materials]\n001 001 001\n001 001 001\n001 001 001\n\n"
    "[ceiling_materials]\n001 001 001\n001 001 001\n001 001 001\n\n"
    "[light 1]\n"
    "position = 1.5,1.5\n"
    "color = 255,255,255,255\n"
    "intensity = 1\n"
    "radius = 4\n\n"
    "[decal_instance 2]\n"
    "asset_kind = decal_pattern\n"
    "asset_id = 6\n"
    "surface = floor\n"
    "position = 1.5,1.5,0\n"
    "size = 0.5,0.5\n"
    "glyph_step = 0,0\n"
    "depth = 0.1\n"
    "rotation = 0\n";

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
    {
        PatternCell cell = {'A', 1};
        assert_true(asset_registry_set_decal_pattern(&g_assets, 6, 1, 1,
                                                     &cell));
    }
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
    ed->inspector_kind = EDITOR_INSPECTOR_WALL_MATERIAL;
}

static void select_native_east_wall(UnifiedEditorState *ed) {
    ed->selection.type = SELECTION_WALL_FACE;
    ed->selection.value.wall_face.map_x = 2;
    ed->selection.value.wall_face.map_y = 1;
    ed->selection.value.wall_face.face = WALL_FACE_WEST;
    ed->inspector_open = true;
    ed->inspector_kind = EDITOR_INSPECTOR_WALL_MATERIAL;
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

static void set_scene_root(UnifiedEditorState *ed, const char *root) {
    free(ed->scene_root);
    ed->scene_root = strdup(root);
    assert_non_null(ed->scene_root);
}

static void enter_save_name_and_confirm(UnifiedEditorState *ed, Camera *cam,
                                        const char *name) {
    InputState in;

    assert_int_equal(ed->modal, EDITOR_MENU_SAVE);
    zero_input(&in);
    snprintf(in.text_input, sizeof(in.text_input), "%s", name);
    in.text_input_len = (int)strlen(in.text_input);
    update_with(ed, cam, &in);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(ed, cam, &in);
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
    /* Legacy-current chooser now imports through the SceneDocument boundary. */
    assert_null(ed.document.path);
    assert_string_equal(scene_document_get_legacy_source_path(&ed.document),
                        first);
    assert_true(scene_document_is_imported_unsaved(&ed.document));
    assert_true(scene_document_is_dirty(&ed.document));
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
    char native_first[512];
    char native_second[512];
    char missing_root[512];
    char *root_before;
    SelectionTarget selection_before;
    (void)state;

    assert_int_equal(prepare_catalog_maps(first, sizeof(first), second,
                                          sizeof(second)), 0);
    path_in_tmpdir(native_first, sizeof(native_first), "open_a.tscene");
    path_in_tmpdir(native_second, sizeof(native_second), "open_b.tscene");
    assert_int_equal(write_text_file(native_first, NATIVE_SCENE), 0);
    assert_int_equal(write_text_file(native_second, NATIVE_SCENE), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_load_scene(&ed, first), SCENE_LOAD_OK);
    assert_int_equal(unified_editor_begin_native_open(&ed, g_tmpdir),
                     MAP_CATALOG_OK);
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
    assert_int_equal(ed.chooser_kind, EDITOR_CHOOSER_NATIVE_OPEN);
    assert_string_equal(ed.map_catalog.entries[0].name, "open_a.tscene");

    root_before = ed.scene_root;
    path_in_tmpdir(missing_root, sizeof(missing_root), "missing-directory");
    assert_int_equal(unified_editor_begin_native_open(&ed, missing_root),
                     MAP_CATALOG_OPEN_FAILED);
    assert_int_equal(ed.modal, EDITOR_MODAL_MAP_CHOOSER);
    assert_int_equal(ed.status, EDITOR_STATUS_CATALOG_FAILED);
    assert_ptr_equal(ed.scene_root, root_before);
    assert_string_equal(ed.scene_root, g_tmpdir);
    assert_int_equal(ed.map_catalog.count, 2);
    assert_string_equal(ed.document.path, first);
    assert_memory_equal(&ed.selection, &selection_before,
                        sizeof(selection_before));
    assert_true(ed.inspector_open);
    assert_float_equal(cam.transform.pos.x, 4.0, 0.0001);

    unified_editor_destroy(&ed);
    remove(first);
    remove(second);
    remove(native_first);
    remove(native_second);
}

static void test_native_open_and_legacy_import_chooser_switch(void **state) {
    UnifiedEditorState ed;
    Camera cam;
    InputState in;
    Grid *grid;
    char legacy[512];
    char native[512];
    (void)state;

    path_in_tmpdir(legacy, sizeof(legacy), "a_current.txt");
    path_in_tmpdir(native, sizeof(native), "open_a.tscene");
    assert_int_equal(write_text_file(legacy, VALID_MAP), 0);
    assert_int_equal(write_text_file(native, NATIVE_SCENE), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_begin_legacy_import(&ed, g_tmpdir),
                     MAP_CATALOG_OK);
    assert_int_equal(unified_editor_begin_native_open(&ed, g_tmpdir),
                     MAP_CATALOG_OK);
    camera_init(&cam, 1.5, 1.5, 0.0, PI / 2.0);

    grid = grid_create(260, 30);
    assert_non_null(grid);
    unified_editor_render_text_overlay(&ed, grid);
    assert_true(grid_contains_text(grid, "OPEN SCENE"));
    assert_true(grid_contains_text(grid, "open_a.tscene"));
    grid_destroy(grid);

    zero_input(&in);
    in.editor_import_pressed = true;
    assert_true(update_with(&ed, &cam, &in).keyboard_consumed);
    assert_int_equal(ed.chooser_kind, EDITOR_CHOOSER_LEGACY_IMPORT);
    assert_int_equal(ed.map_catalog.count, 1);
    assert_string_equal(ed.map_catalog.entries[0].name, "a_current.txt");

    zero_input(&in);
    in.editor_open_pressed = true;
    assert_true(update_with(&ed, &cam, &in).keyboard_consumed);
    assert_int_equal(ed.chooser_kind, EDITOR_CHOOSER_NATIVE_OPEN);
    assert_int_equal(ed.map_catalog.count, 1);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    assert_string_equal(scene_document_get_path(&ed.document), native);
    assert_false(scene_document_is_imported_unsaved(&ed.document));

    unified_editor_destroy(&ed);
    remove(legacy);
    remove(native);
}

static void test_r2_typed_workflows_transactional(void **state) {
    UnifiedEditorState ed;
    char legacy[512];
    char native[512];
    char saved[512];
    MapCell *old_cells;
    (void)state;
    path_in_tmpdir(legacy, sizeof(legacy), "r2_legacy.txt");
    path_in_tmpdir(native, sizeof(native), "r2_native.tscene");
    path_in_tmpdir(saved, sizeof(saved), "r2_saved.tscene");
    assert_int_equal(write_text_file(legacy, "11\n10\n"), 0);
    assert_int_equal(write_text_file(native, NATIVE_SCENE), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_new_scene(&ed), SCENE_LOAD_OK);
    assert_true(unified_editor_has_document(&ed));
    assert_null(ed.document.path);
    assert_true(scene_document_is_dirty(&ed.document));
    assert_int_equal(ed.document.map.width, 10);
    assert_int_equal(ed.runtime_world.num_lights, 0);
    old_cells = ed.document.map.cells;
    assert_int_not_equal(unified_editor_open_native(&ed, legacy), SCENE_LOAD_OK);
    assert_ptr_equal(ed.document.map.cells, old_cells);
    assert_true(scene_document_is_dirty(&ed.document));
    assert_int_equal(unified_editor_import_legacy(&ed, legacy), SCENE_LOAD_OK);
    assert_true(unified_editor_has_document(&ed));
    assert_true(scene_document_is_imported_unsaved(&ed.document));
    assert_null(ed.document.path);
    assert_string_equal(ed.document.legacy_source_path, legacy);
    assert_int_equal(unified_editor_save(&ed), SCENE_SAVE_NO_PATH);
    assert_int_equal(unified_editor_save_as(&ed, saved, "converted"),
                     SCENE_SAVE_OK);
    assert_string_equal(ed.document.path, saved);
    assert_string_equal(ed.document.name, "converted");
    assert_false(scene_document_is_imported_unsaved(&ed.document));
    assert_false(scene_document_is_dirty(&ed.document));
    assert_int_equal(ed.status, EDITOR_STATUS_SAVED);
    assert_int_equal(unified_editor_open_native(&ed, native), SCENE_LOAD_OK);
    assert_string_equal(ed.document.name, "workflow");
    assert_false(scene_document_is_dirty(&ed.document));
    unified_editor_destroy(&ed);
}

static void save_pathless_document_through_shortcut(UnifiedEditorState *ed,
                                                    Camera *cam,
                                                    const char *name,
                                                    const char *expected_path) {
    InputState in;
    Grid *grid;

    free(ed->scene_root);
    ed->scene_root = strdup(g_tmpdir);
    assert_non_null(ed->scene_root);

    zero_input(&in);
    in.editor_save_pressed = true;
    assert_true(update_with(ed, cam, &in).keyboard_consumed);
    assert_int_equal(ed->modal, EDITOR_MENU_SAVE);
    assert_int_equal(ed->save_menu_stage, EDITOR_SAVE_MENU_EDIT_NAME);
    assert_true(scene_document_is_dirty(&ed->document));

    grid = grid_create(260, 30);
    assert_non_null(grid);
    unified_editor_render_text_overlay(ed, grid);
    assert_true(grid_contains_text(grid, "SAVE SCENE"));
    assert_true(grid_contains_text(grid, "Backspace=delete"));
    grid_destroy(grid);

    zero_input(&in);
    snprintf(in.text_input, sizeof(in.text_input), "%s", name);
    in.text_input_len = (int)strlen(in.text_input);
    assert_true(update_with(ed, cam, &in).keyboard_consumed);

    zero_input(&in);
    in.editor_confirm_pressed = true;
    assert_true(update_with(ed, cam, &in).keyboard_consumed);
    assert_int_equal(ed->modal, EDITOR_MODAL_NONE);
    assert_int_equal(ed->status, EDITOR_STATUS_SAVED);
    assert_false(scene_document_is_dirty(&ed->document));
    assert_string_equal(scene_document_get_path(&ed->document), expected_path);
    assert_int_equal(access(expected_path, F_OK), 0);
}

static void test_save_menu_backspace_and_empty_name_feedback(void **state) {
    UnifiedEditorState ed;
    Camera cam;
    InputState in;
    Grid *grid;
    (void)state;

    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_new_scene(&ed), SCENE_LOAD_OK);
    camera_init(&cam, 1.5, 1.5, 0.0, PI / 2.0);

    zero_input(&in);
    in.editor_save_pressed = true;
    update_with(&ed, &cam, &in);
    zero_input(&in);
    memcpy(in.text_input, "ab", 3U);
    in.text_input_len = 2;
    update_with(&ed, &cam, &in);
    assert_string_equal(ed.save_as_name, "ab");
    zero_input(&in);
    in.editor_text_backspace_pressed = true;
    update_with(&ed, &cam, &in);
    assert_string_equal(ed.save_as_name, "a");
    zero_input(&in);
    in.editor_text_backspace_pressed = true;
    update_with(&ed, &cam, &in);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.modal, EDITOR_MENU_SAVE);
    assert_int_equal(ed.status, EDITOR_STATUS_INVALID_SCENE_NAME);

    grid = grid_create(260, 30);
    assert_non_null(grid);
    unified_editor_render_text_overlay(&ed, grid);
    assert_true(grid_contains_text(grid, "Status: Invalid scene name"));
    grid_destroy(grid);
    unified_editor_destroy(&ed);
}

static void test_dirty_import_save_then_new_continues(void **state) {
    UnifiedEditorState ed;
    Camera cam;
    InputState in;
    char legacy[512];
    char saved[512];
    char source_before[64];
    char source_after[64];
    (void)state;

    path_in_tmpdir(legacy, sizeof(legacy), "r2_legacy.txt");
    path_in_tmpdir(saved, sizeof(saved), "new_interactive.tscene");
    remove(saved);
    assert_int_equal(write_text_file(legacy, "11\n10\n"), 0);
    assert_int_equal(read_text_file(legacy, source_before,
                                    sizeof(source_before)), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_import_legacy(&ed, legacy), SCENE_LOAD_OK);
    free(ed.scene_root);
    ed.scene_root = strdup(g_tmpdir);
    assert_non_null(ed.scene_root);
    camera_init(&cam, 1.5, 1.5, 0.0, PI / 2.0);

    zero_input(&in);
    in.editor_new_pressed = true;
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.modal, EDITOR_MODAL_DIRTY_OPEN_PROMPT);
    assert_int_equal(ed.pending_action, EDITOR_PENDING_NEW);
    zero_input(&in);
    in.editor_next_pressed = true;
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.dirty_open_choice, EDITOR_DIRTY_OPEN_SAVE);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.modal, EDITOR_MENU_SAVE);
    assert_int_equal(ed.save_return_menu, EDITOR_MODAL_DIRTY_OPEN_PROMPT);

    zero_input(&in);
    memcpy(in.text_input, "new_interactive", sizeof("new_interactive"));
    in.text_input_len = (int)strlen(in.text_input);
    update_with(&ed, &cam, &in);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);

    assert_int_equal(access(saved, F_OK), 0);
    assert_int_equal(read_text_file(legacy, source_after,
                                    sizeof(source_after)), 0);
    assert_string_equal(source_after, source_before);
    assert_int_equal(ed.pending_action, EDITOR_PENDING_NONE);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);
    assert_null(ed.document.path);
    assert_true(scene_document_is_dirty(&ed.document));
    assert_int_equal(ed.document.map.width, 10);
    assert_int_equal(ed.document.map.height, 6);
    unified_editor_destroy(&ed);
}

static void test_save_menu_overwrite_and_cancel_are_visible(void **state) {
    UnifiedEditorState ed;
    Camera cam;
    InputState in;
    Grid *grid;
    char existing[512];
    (void)state;

    path_in_tmpdir(existing, sizeof(existing), "overwrite_target.tscene");
    remove(existing);
    assert_int_equal(write_text_file(existing, "existing"), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_new_scene(&ed), SCENE_LOAD_OK);
    set_scene_root(&ed, g_tmpdir);
    camera_init(&cam, 1.5, 1.5, 0.0, PI / 2.0);

    zero_input(&in);
    in.editor_save_pressed = true;
    update_with(&ed, &cam, &in);
    zero_input(&in);
    memcpy(in.text_input, "overwrite_target", sizeof("overwrite_target"));
    in.text_input_len = (int)strlen(in.text_input);
    update_with(&ed, &cam, &in);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.modal, EDITOR_MENU_SAVE);
    assert_int_equal(ed.save_menu_stage,
                     EDITOR_SAVE_MENU_CONFIRM_OVERWRITE);

    grid = grid_create(260, 30);
    assert_non_null(grid);
    unified_editor_render_text_overlay(&ed, grid);
    assert_true(grid_contains_text(grid, "File exists:"));
    assert_true(grid_contains_text(grid, "> Overwrite"));
    grid_destroy(grid);

    zero_input(&in);
    in.editor_next_pressed = true;
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.save_choice, EDITOR_SAVE_EDIT_NAME);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.save_menu_stage, EDITOR_SAVE_MENU_EDIT_NAME);
    assert_string_equal(ed.save_as_name, "overwrite_target");
    zero_input(&in);
    in.editor_cancel_pressed = true;
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);
    assert_int_equal(ed.pending_action, EDITOR_PENDING_NONE);
    assert_true(scene_document_is_dirty(&ed.document));

    unified_editor_destroy(&ed);
    remove(existing);
}

static void test_native_ctrl_s_menu_saves_existing_destination(void **state) {
    UnifiedEditorState ed;
    Camera cam;
    InputState in;
    char native[512];
    char contents[2048];
    (void)state;

    path_in_tmpdir(native, sizeof(native), "r2_native.tscene");
    assert_int_equal(write_text_file(native, NATIVE_SCENE), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_open_native(&ed, native), SCENE_LOAD_OK);
    select_native_east_wall(&ed);
    assert_int_equal(unified_editor_set_wall_material(&ed, 12), CMD_RESULT_OK);
    assert_int_not_equal(ed.status, EDITOR_STATUS_UNSAVABLE_MATERIAL_ID);
    camera_init(&cam, 1.5, 1.5, 0.0, PI / 2.0);

    zero_input(&in);
    in.editor_save_pressed = true;
    update_with(&ed, &cam, &in);
    assert_int_equal(ed.modal, EDITOR_MENU_SAVE);
    assert_string_equal(ed.save_as_name, "workflow");
    assert_false(ed.save_force_new_path);
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);

    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);
    assert_int_equal(ed.status, EDITOR_STATUS_SAVED);
    assert_false(scene_document_is_dirty(&ed.document));
    assert_string_equal(scene_document_get_path(&ed.document), native);
    assert_int_equal(read_text_file(native, contents, sizeof(contents)), 0);
    assert_non_null(strstr(contents, "012"));
    unified_editor_destroy(&ed);
    remove(native);
}

static void test_pathless_ctrl_s_routes_to_interactive_save_as(void **state) {
    UnifiedEditorState ed;
    Camera cam;
    char legacy[512];
    char new_saved[512];
    char imported_saved[512];
    (void)state;

    path_in_tmpdir(legacy, sizeof(legacy), "r2_legacy.txt");
    path_in_tmpdir(new_saved, sizeof(new_saved), "new_interactive.tscene");
    path_in_tmpdir(imported_saved, sizeof(imported_saved),
                   "import_interactive.tscene");
    assert_int_equal(write_text_file(legacy, "11\n10\n"), 0);
    camera_init(&cam, 1.5, 1.5, 0.0, PI / 2.0);

    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_new_scene(&ed), SCENE_LOAD_OK);
    save_pathless_document_through_shortcut(&ed, &cam, "new_interactive",
                                            new_saved);
    unified_editor_destroy(&ed);

    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_import_legacy(&ed, legacy), SCENE_LOAD_OK);
    save_pathless_document_through_shortcut(&ed, &cam, "import_interactive",
                                            imported_saved);
    unified_editor_destroy(&ed);
}


static void test_light_hover_selection_uses_stable_id(void **state) {
    UnifiedEditorState ed;
    Camera cam;
    InputState in;
    Grid *grid;
    char path[512];
    (void)state;

    path_in_tmpdir(path, sizeof(path), "light_pick.tscene");
    assert_int_equal(write_text_file(path, NATIVE_SCENE_WITH_PICKABLE_LIGHT), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_open_native(&ed, path), SCENE_LOAD_OK);
    camera_init(&cam, 1.5, 2.5, 0.0, PI / 2.0);

    zero_input(&in);
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(ed.hover.valid);
    assert_int_equal(ed.hover.target.type, SELECTION_LIGHT);
    assert_int_equal(ed.hover.target.value.light.id, 11U);

    zero_input(&in);
    in.editor_select_pressed = true;
    assert_true(unified_editor_update(&ed, &in, &cam, 0.016)
                    .keyboard_consumed);
    assert_int_equal(ed.selection.type, SELECTION_LIGHT);
    assert_int_equal(ed.selection.value.light.id, 11U);
    assert_true(ed.inspector_open);
    assert_int_equal(ed.inspector_kind, EDITOR_INSPECTOR_LIGHT);
    assert_int_equal(ed.light_field, EDITOR_LIGHT_FIELD_X);

    grid = grid_create(260, 30);
    assert_non_null(grid);
    unified_editor_render_text_overlay(&ed, grid);
    assert_true(grid_contains_text(grid, "Select light:11"));
    assert_true(grid_contains_text(grid, "Inspector: point light"));
    assert_true(grid_contains_text(grid, "> X"));
    grid_destroy(grid);

    zero_input(&in);
    in.editor_cancel_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(ed.selection.type, SELECTION_NONE);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);

    unified_editor_destroy(&ed);
    remove(path);
}

static void test_light_inspector_edits_through_history_and_runtime(void **state) {
    UnifiedEditorState ed;
    Camera cam;
    InputState in;
    const SceneLight *light;
    char path[512];
    (void)state;

    path_in_tmpdir(path, sizeof(path), "light_pick.tscene");
    assert_int_equal(write_text_file(path, NATIVE_SCENE_WITH_PICKABLE_LIGHT), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_open_native(&ed, path), SCENE_LOAD_OK);
    camera_init(&cam, 1.5, 2.5, 0.0, PI / 2.0);
    zero_input(&in);
    unified_editor_update(&ed, &in, &cam, 0.016);
    zero_input(&in);
    in.editor_select_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);

    zero_input(&in);
    in.editor_increase_pressed = true;
    assert_true(unified_editor_update(&ed, &in, &cam, 0.016)
                    .keyboard_consumed);
    light = scene_document_find_light(&ed.document, 11U);
    assert_non_null(light);
    assert_true(light->x == 2.75);
    assert_true(ed.runtime_world.lights[0].pos.x == 2.75);
    assert_int_equal(ed.history.count, 1U);
    assert_true(scene_document_is_dirty(&ed.document));

    zero_input(&in);
    in.editor_next_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(ed.light_field, EDITOR_LIGHT_FIELD_Y);
    zero_input(&in);
    in.editor_next_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_int_equal(ed.light_field, EDITOR_LIGHT_FIELD_RED);
    zero_input(&in);
    in.editor_decrease_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    light = scene_document_find_light(&ed.document, 11U);
    assert_int_equal(light->red, 254U);
    assert_int_equal(ed.runtime_world.lights[0].color.r, 254U);
    assert_int_equal(ed.history.count, 2U);

    assert_int_equal(unified_editor_undo(&ed), CMD_RESULT_OK);
    assert_int_equal(scene_document_find_light(&ed.document, 11U)->red, 255U);
    assert_int_equal(ed.runtime_world.lights[0].color.r, 255U);
    assert_int_equal(unified_editor_undo(&ed), CMD_RESULT_OK);
    assert_true(scene_document_find_light(&ed.document, 11U)->x == 2.5);
    assert_true(ed.runtime_world.lights[0].pos.x == 2.5);
    assert_int_equal(unified_editor_redo(&ed), CMD_RESULT_OK);
    assert_true(ed.runtime_world.lights[0].pos.x == 2.75);

    assert_int_equal(unified_editor_save(&ed), SCENE_SAVE_OK);
    assert_false(scene_document_is_dirty(&ed.document));
    unified_editor_destroy(&ed);

    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_open_native(&ed, path), SCENE_LOAD_OK);
    light = scene_document_find_light(&ed.document, 11U);
    assert_non_null(light);
    assert_true(light->x == 2.75);
    assert_int_equal(light->red, 255U);
    assert_true(ed.runtime_world.lights[0].pos.x == 2.75);

    unified_editor_destroy(&ed);
    remove(path);
}

static void open_pickable_light(UnifiedEditorState *ed, Camera *cam,
                                const char *path) {
    InputState in;
    assert_int_equal(write_text_file(path, NATIVE_SCENE_WITH_PICKABLE_LIGHT), 0);
    assert_true(unified_editor_init(ed, &g_assets));
    assert_int_equal(unified_editor_open_native(ed, path), SCENE_LOAD_OK);
    camera_init(cam, 1.5, 2.5, 0.0, PI / 2.0);
    zero_input(&in);
    unified_editor_update(ed, &in, cam, 0.016);
    zero_input(&in);
    in.editor_select_pressed = true;
    unified_editor_update(ed, &in, cam, 0.016);
}

static void test_light_inspector_numeric_entry_commit_cancel_and_validation(void **state) {
    UnifiedEditorState ed;
    Camera cam;
    InputState in;
    char path[512];
    Grid *grid;
    (void)state;
    path_in_tmpdir(path, sizeof(path), "light_numeric.tscene");
    open_pickable_light(&ed, &cam, path);
    ed.light_field = EDITOR_LIGHT_FIELD_INTENSITY;

    zero_input(&in);
    strcpy(in.text_input, "2.5"); in.text_input_len = 3;
    assert_true(unified_editor_update(&ed, &in, &cam, 0.016)
                    .keyboard_consumed);
    assert_true(ed.light_value_editing);
    grid = grid_create(260, 30);
    assert_non_null(grid);
    unified_editor_render_text_overlay(&ed, grid);
    assert_true(grid_contains_text(grid, "[2.5_]"));
    assert_true(grid_contains_text(grid, "illumination is scalar"));
    grid_destroy(grid);

    zero_input(&in); in.editor_confirm_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_false(ed.light_value_editing);
    assert_true(scene_document_find_light(&ed.document, 11U)->intensity == 2.5);
    assert_true(ed.runtime_world.lights[0].intensity == 2.5);

    zero_input(&in); strcpy(in.text_input, "999"); in.text_input_len = 3;
    unified_editor_update(&ed, &in, &cam, 0.016);
    zero_input(&in); in.editor_confirm_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(ed.light_value_editing);
    assert_int_equal(ed.status, EDITOR_STATUS_INVALID_NUMERIC_VALUE);
    zero_input(&in); in.editor_cancel_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_false(ed.light_value_editing);
    assert_true(ed.inspector_open);

    unified_editor_destroy(&ed);
    remove(path);
}

static void test_light_inspector_held_arrow_repeats_after_delay(void **state) {
    UnifiedEditorState ed;
    Camera cam;
    InputState in;
    char path[512];
    (void)state;
    path_in_tmpdir(path, sizeof(path), "light_repeat.tscene");
    open_pickable_light(&ed, &cam, path);

    zero_input(&in);
    in.editor_increase_pressed = true;
    in.held_arrow_right = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(scene_document_find_light(&ed.document, 11U)->x == 2.75);
    zero_input(&in); in.held_arrow_right = true;
    unified_editor_update(&ed, &in, &cam, 0.20);
    assert_true(scene_document_find_light(&ed.document, 11U)->x == 2.75);
    zero_input(&in); in.held_arrow_right = true;
    unified_editor_update(&ed, &in, &cam, 0.16);
    assert_true(scene_document_find_light(&ed.document, 11U)->x == 3.0);

    unified_editor_destroy(&ed);
    remove(path);
}

static void test_overlay_includes_new_scene_shortcut(void **state) {
    UnifiedEditorState ed;
    Grid *grid;
    (void)state;

    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_new_scene(&ed), SCENE_LOAD_OK);
    grid = grid_create(260, 20);
    assert_non_null(grid);
    unified_editor_render_text_overlay(&ed, grid);
    assert_true(grid_contains_text(grid, "Ctrl+N=new"));
    grid_destroy(grid);
    unified_editor_destroy(&ed);
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
    /* Switching via the legacy-current chooser now imports the target. */
    assert_null(ed.document.path);
    assert_string_equal(scene_document_get_legacy_source_path(&ed.document),
                        second);
    assert_true(scene_document_is_imported_unsaved(&ed.document));
    assert_int_equal(ed.document.map.width, 3);
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
    /* The previously loaded first document (legacy load) is preserved. */
    assert_string_equal(ed.document.path, first);
    assert_memory_equal(&ed.selection, &selection_before,
                        sizeof(selection_before));
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
    /* Load failed; original legacy-loaded first document is preserved. */
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
    /* Successful switch through legacy-current chooser imports second. */
    assert_null(ed.document.path);
    assert_string_equal(scene_document_get_legacy_source_path(&ed.document),
                        second);
    assert_true(scene_document_is_imported_unsaved(&ed.document));
    assert_true(scene_document_is_dirty(&ed.document));
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
    char saved[4096];
    char native_saved[512];
    (void)state;

    assert_int_equal(prepare_catalog_maps(first, sizeof(first), second,
                                          sizeof(second)), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    set_scene_root(&ed, g_tmpdir);
    path_in_tmpdir(native_saved, sizeof(native_saved), "dirty_saved.tscene");
    remove(native_saved);
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
    enter_save_name_and_confirm(&ed, &cam, "dirty_saved");
    assert_int_equal(ed.modal, EDITOR_MODAL_MAP_CHOOSER);
    assert_int_equal(ed.status, EDITOR_STATUS_LOAD_FAILED);
    /* Original legacy-loaded first document is preserved; save succeeded. */
    assert_string_equal(ed.document.path, native_saved);
    assert_false(scene_document_is_dirty(&ed.document));
    assert_int_equal(east_wall_mat(&ed), 2);
    assert_int_equal(ed.history.count, 1);
    assert_int_equal(read_text_file(native_saved, saved, sizeof(saved)), 0);
    assert_non_null(strstr(saved, "002"));

    assert_int_equal(write_text_file(second, SECOND_MAP), 0);
    assert_int_equal(unified_editor_begin_map_open(&ed, NULL), MAP_CATALOG_OK);
    ed.map_chooser_index = 1;
    zero_input(&in);
    in.editor_confirm_pressed = true;
    update_with(&ed, &cam, &in);
    /* Switch imports second through the SceneDocument boundary. */
    assert_null(ed.document.path);
    assert_string_equal(scene_document_get_legacy_source_path(&ed.document),
                        second);
    assert_true(scene_document_is_imported_unsaved(&ed.document));

    assert_int_equal(unified_editor_load_scene(&ed, first), SCENE_LOAD_OK);
    select_east_wall(&ed);
    assert_int_equal(unified_editor_set_wall_material(&ed, 12), CMD_RESULT_OK);
    {
        char missing_root[512];
        path_in_tmpdir(missing_root, sizeof(missing_root), "missing-save-root");
        set_scene_root(&ed, missing_root);
    }
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
    enter_save_name_and_confirm(&ed, &cam, "cannot_save");
    assert_int_equal(ed.modal, EDITOR_MENU_SAVE);
    assert_int_equal(ed.status, EDITOR_STATUS_SAVE_FAILED);
    assert_string_equal(ed.document.path, first);
    assert_true(scene_document_is_dirty(&ed.document));
    assert_int_equal(east_wall_mat(&ed), 12);
    assert_int_equal(ed.history.count, 1);

    unified_editor_destroy(&ed);
    remove(native_saved);
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
    assert_int_equal(write_text_file(path, "333\n303\n333\n"), 0);
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
    ed.inspector_kind = EDITOR_INSPECTOR_WALL_MATERIAL;
    ed.hover.valid = true;
    ed.mode = EDITOR_MODE_EDIT;
    assert_int_equal(command_history_set_ambient_intensity(
        &ed.history, &ed.document, 0.21), CMD_RESULT_OK);
    assert_int_equal(command_history_set_ambient_intensity(
        &ed.history, &ed.document, 0.30), CMD_RESULT_OK);
    assert_int_equal(command_history_set_ambient_intensity(
        &ed.history, &ed.document, 0.40), CMD_RESULT_OK);
    assert_int_equal(command_history_undo(&ed.history, &ed.document), CMD_RESULT_OK);
    assert_int_equal(ed.history.count, 3U);
    assert_int_equal(ed.history.cursor, 2U);

    assert_int_equal(unified_editor_load_scene(&ed, path), SCENE_LOAD_OK);
    assert_int_equal(ed.selection.type, SELECTION_NONE);
    assert_false(ed.hover.valid);
    assert_false(ed.inspector_open);
    assert_int_equal(ed.inspector_kind, EDITOR_INSPECTOR_NONE);
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
    ed.inspector_kind = EDITOR_INSPECTOR_WALL_MATERIAL;
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
    assert_int_equal(ed.inspector_kind, EDITOR_INSPECTOR_WALL_MATERIAL);
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
    char buf[4096];

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
    Grid *grid;
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
    grid = grid_create(260, 30);
    assert_non_null(grid);
    unified_editor_render_text_overlay(&ed, grid);
    assert_true(grid_contains_text(grid, "Inspector: wall surface"));
    assert_true(grid_contains_text(grid, "Up/Down=choose"));
    grid_destroy(grid);

    /* Enter opens the material submenu; Down moves within it. */
    zero_input(&in);
    in.editor_confirm_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(ed.material_picker_open);
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
    assert_false(ed.material_picker_open);
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
    char saved[512];
    set_scene_root(&ed, g_tmpdir);
    path_in_tmpdir(saved, sizeof(saved), "shortcut_saved.tscene");

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
    assert_int_equal(ed.modal, EDITOR_MENU_SAVE);
    enter_save_name_and_confirm(&ed, &cam, "shortcut_saved");
    assert_int_equal(ed.status, EDITOR_STATUS_SAVED);
    assert_false(scene_document_is_dirty(&ed.document));

    unified_editor_destroy(&ed);
    remove(saved);
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
    /* Reload of a legacy .txt source re-imports through the SceneDocument
       boundary, so the document is imported and unsaved. */
    assert_null(ed.document.path);
    assert_true(scene_document_is_imported_unsaved(&ed.document));
    assert_true(scene_document_is_dirty(&ed.document));
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
    char buf[4096];
    char saved[512];

    path_in_tmpdir(path, sizeof(path), "map_saved.txt");
    assert_int_equal(write_text_file(path, VALID_MAP), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    set_scene_root(&ed, g_tmpdir);
    path_in_tmpdir(saved, sizeof(saved), "exit_saved.tscene");
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
    assert_int_equal(ed.modal, EDITOR_MENU_SAVE);
    enter_save_name_and_confirm(&ed, &cam, "exit_saved");
    assert_true(ed.request_exit_to_main_menu);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);
    assert_false(scene_document_is_dirty(&ed.document));
    assert_int_equal(ed.status, EDITOR_STATUS_SAVED);

    assert_int_equal(read_text_file(saved, buf, sizeof(buf)), 0);
    assert_non_null(strstr(buf, "002"));

    unified_editor_destroy(&ed);
    remove(saved);
}

static void test_exit_discard_and_exit_does_not_write(void **state) {
    (void)state;
    UnifiedEditorState ed;
    char path[512];
    char buf[4096];

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
    Grid *grid;
    char missing_root[512];
    path_in_tmpdir(missing_root, sizeof(missing_root), "missing-save-root");
    set_scene_root(&ed, missing_root);

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
    assert_int_equal(ed.modal, EDITOR_MENU_SAVE);
    enter_save_name_and_confirm(&ed, &cam, "cannot_save");
    assert_false(ed.request_exit_to_main_menu);
    assert_int_equal(ed.modal, EDITOR_MENU_SAVE);
    assert_int_equal(ed.status, EDITOR_STATUS_SAVE_FAILED);
    assert_int_equal(ed.last_scene_diagnostic.code,
                     SCENE_DIAGNOSTIC_ENV_TEMP_CREATE);
    assert_true(scene_document_is_dirty(&ed.document));
    assert_int_equal(east_wall_mat(&ed), 12);

    grid = grid_create(260, 30);
    assert_non_null(grid);
    unified_editor_render_text_overlay(&ed, grid);
    assert_true(grid_contains_text(grid, "TSG-SCENE-ENV-0003"));
    assert_true(grid_contains_text(grid, "temporary file creation failed"));
    grid_destroy(grid);

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
    char buf[4096];
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
    char native_saved[512];
    set_scene_root(&ed, g_tmpdir);
    path_in_tmpdir(native_saved, sizeof(native_saved), "phase6_saved.tscene");

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

    /* Apply material 2 via Enter-opened picker. */
    zero_input(&in);
    in.editor_confirm_pressed = true;
    unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(ed.material_picker_open);
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
    enter_save_name_and_confirm(&ed, &cam, "phase6_saved");
    assert_int_equal(ed.status, EDITOR_STATUS_SAVED);
    assert_false(scene_document_is_dirty(&ed.document));
    assert_int_equal(read_text_file(native_saved, buf, sizeof(buf)), 0);
    assert_non_null(strstr(buf, "002"));

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
    /* Reload uses the native destination established by Save. */
    assert_string_equal(ed.document.path, native_saved);
    assert_false(scene_document_is_imported_unsaved(&ed.document));
    assert_false(scene_document_is_dirty(&ed.document));
    /* Native file still has material 2 from prior save. */
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
    assert_null(strstr(buf, "2")); /* legacy source is never overwritten */
    assert_int_equal(read_text_file(native_saved, buf, sizeof(buf)), 0);
    assert_non_null(strstr(buf, "002")); /* native destination retains Save */

    unified_editor_destroy(&ed);
    remove(native_saved);
}

static bool path_ends_with(const char *path, const char *suffix) {
    size_t plen = path ? strlen(path) : 0;
    size_t slen = strlen(suffix);
    return plen >= slen && strcmp(path + plen - slen, suffix) == 0;
}

static void test_native_reload_preserves_light_decal_ambient(void **state) {
    (void)state;
    UnifiedEditorState ed;
    Camera cam;
    InputState in;
    char path[512];

    path_in_tmpdir(path, sizeof(path), "native_with_content.tscene");
    assert_int_equal(write_text_file(path, NATIVE_SCENE_WITH_CONTENT), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_open_native(&ed, path), SCENE_LOAD_OK);

    assert_string_equal(ed.document.name, "content");
    assert_non_null(scene_document_get_path(&ed.document));
    assert_string_equal(scene_document_get_path(&ed.document), path);
    assert_false(scene_document_is_imported_unsaved(&ed.document));
    assert_false(scene_document_is_dirty(&ed.document));
    assert_double_equal(scene_document_get_ambient_intensity(&ed.document),
                        0.3, 0.0001);
    assert_int_equal(ed.runtime_world.num_lights, 1);
    assert_int_equal(ed.runtime_world.num_decals, 1);

    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);
    select_native_east_wall(&ed);
    assert_int_equal(unified_editor_set_wall_material(&ed, 2), CMD_RESULT_OK);
    assert_true(scene_document_is_dirty(&ed.document));

    /* F5 reload of a dirty native scene prompts before reloading. */
    zero_input(&in);
    in.editor_reload_pressed = true;
    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.modal, EDITOR_MODAL_RELOAD_PROMPT);

    zero_input(&in);
    in.editor_confirm_pressed = true;
    c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);

    /* Native reload must keep the .tscene path and all authored content.
       The old buggy path ran through the legacy digit-grid loader and would
       drop lights, decals, ambient, and name. */
    assert_string_equal(scene_document_get_path(&ed.document), path);
    assert_false(scene_document_is_imported_unsaved(&ed.document));
    assert_false(scene_document_is_dirty(&ed.document));
    assert_string_equal(ed.document.name, "content");
    assert_double_equal(scene_document_get_ambient_intensity(&ed.document),
                        0.3, 0.0001);
    assert_int_equal(ed.runtime_world.num_lights, 1);
    assert_int_equal(ed.runtime_world.num_decals, 1);

    unified_editor_destroy(&ed);
    remove(path);
}

static void test_native_reload_clean_preserves_content(void **state) {
    (void)state;
    UnifiedEditorState ed;
    Camera cam;
    InputState in;
    char path[512];

    path_in_tmpdir(path, sizeof(path), "native_with_content.tscene");
    assert_int_equal(write_text_file(path, NATIVE_SCENE_WITH_CONTENT), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_open_native(&ed, path), SCENE_LOAD_OK);

    assert_string_equal(ed.document.name, "content");
    assert_int_equal(ed.runtime_world.num_lights, 1);
    assert_int_equal(ed.runtime_world.num_decals, 1);

    camera_init(&cam, 2.5, 2.5, 0.0, PI / 2.0);

    /* Clean F5 reload happens immediately without a prompt. */
    zero_input(&in);
    in.editor_reload_pressed = true;
    EditorInputConsumption c = unified_editor_update(&ed, &in, &cam, 0.016);
    assert_true(c.keyboard_consumed);
    assert_int_equal(ed.modal, EDITOR_MODAL_NONE);
    assert_false(scene_document_is_dirty(&ed.document));
    assert_string_equal(scene_document_get_path(&ed.document), path);
    assert_string_equal(ed.document.name, "content");
    assert_int_equal(ed.runtime_world.num_lights, 1);
    assert_int_equal(ed.runtime_world.num_decals, 1);

    unified_editor_destroy(&ed);
    remove(path);
}

static void test_editor_documents_never_hold_legacy_save_path(void **state) {
    (void)state;
    UnifiedEditorState ed;
    char legacy[512];
    char saved[512];

    path_in_tmpdir(legacy, sizeof(legacy), "r2_legacy.txt");
    path_in_tmpdir(saved, sizeof(saved), "r2_saved.tscene");
    assert_int_equal(write_text_file(legacy, "11\n10\n"), 0);

    /* New scene has no path. */
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_new_scene(&ed), SCENE_LOAD_OK);
    assert_null(ed.document.path);
    assert_true(scene_document_is_dirty(&ed.document));
    unified_editor_destroy(&ed);

    /* Legacy import has provenance but no native path; Save is routed to Save As. */
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_import_legacy(&ed, legacy), SCENE_LOAD_OK);
    assert_null(ed.document.path);
    assert_non_null(scene_document_get_legacy_source_path(&ed.document));
    assert_true(scene_document_is_imported_unsaved(&ed.document));
    assert_int_equal(unified_editor_save(&ed), SCENE_SAVE_NO_PATH);
    assert_int_equal(unified_editor_save_as(&ed, saved, "converted"),
                     SCENE_SAVE_OK);
    assert_true(path_ends_with(ed.document.path, ".tscene"));
    assert_false(scene_document_is_imported_unsaved(&ed.document));
    assert_false(scene_document_is_dirty(&ed.document));
    unified_editor_destroy(&ed);

    /* Native open keeps a .tscene path and is not imported/unsaved. */
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_open_native(&ed, saved), SCENE_LOAD_OK);
    assert_true(path_ends_with(ed.document.path, ".tscene"));
    assert_false(scene_document_is_imported_unsaved(&ed.document));
    assert_false(scene_document_is_dirty(&ed.document));
    unified_editor_destroy(&ed);

    remove(legacy);
    remove(saved);
}

static void test_r4_increment_b_controller_commands_and_safety(void **state) {
    UnifiedEditorState ed;
    Camera camera;
    InputState input;
    char path[512];
    MaterialId material = 0;
    (void)state;
    path_in_tmpdir(path, sizeof(path), "r4_b_controller.tscene");
    assert_int_equal(write_text_file(path, NATIVE_SCENE), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_open_native(&ed, path), SCENE_LOAD_OK);

    camera_init(&camera, 1.5, 1.5, 0.0, PI / 2.0);
    zero_input(&input);
    update_with(&ed, &camera, &input);
    assert_true(ed.has_player_cell);
    assert_int_equal(ed.player_map_x, 1);
    assert_int_equal(ed.player_map_y, 1);
    assert_int_equal(unified_editor_place_wall(&ed, 1, 1),
                     CMD_RESULT_SPAWN_BLOCKED);
    assert_int_equal(ed.status, EDITOR_STATUS_SPAWN_BLOCKED);

    ed.document.spawn_x = 0.5;
    ed.document.spawn_y = 0.5;
    assert_int_equal(unified_editor_place_wall(&ed, 1, 1),
                     CMD_RESULT_PLAYER_BLOCKED);
    assert_int_equal(ed.status, EDITOR_STATUS_PLAYER_BLOCKED);
    camera.transform.pos.x = 0.5;
    camera.transform.pos.y = 0.5;
    update_with(&ed, &camera, &input);
    assert_int_equal(unified_editor_place_wall(&ed, 1, 1), CMD_RESULT_OK);
    assert_int_equal(ed.document.map.cells[4].material_id, 1);
    assert_int_equal(unified_editor_set_surface_material(
        &ed, 1, 1, SCENE_SURFACE_WALL, 2), CMD_RESULT_OK);
    assert_true(scene_document_get_surface_material(
        &ed.document, 1, 1, SCENE_SURFACE_WALL, &material));
    assert_int_equal(material, 2);
    assert_int_equal(ed.document.map.cells[4].material_id, 2);
    assert_int_equal(unified_editor_remove_wall(&ed, 1, 1), CMD_RESULT_OK);
    assert_int_equal(ed.document.map.cells[4].material_id, 0);
    assert_int_equal(unified_editor_undo(&ed), CMD_RESULT_OK);
    assert_int_equal(ed.document.map.cells[4].material_id, 2);
    assert_int_equal(unified_editor_redo(&ed), CMD_RESULT_OK);
    assert_int_equal(ed.document.map.cells[4].material_id, 0);

    assert_int_equal(unified_editor_set_ambient_intensity(&ed, 0.8), CMD_RESULT_OK);
    assert_true(ed.document.ambient_intensity == 0.8);
    assert_true(ed.runtime_world.ambient_intensity == 0.8);
    assert_int_equal(unified_editor_undo(&ed), CMD_RESULT_OK);
    assert_true(ed.document.ambient_intensity == 0.25);
    assert_true(ed.runtime_world.ambient_intensity == 0.25);
    assert_int_equal(unified_editor_set_surface_material(
        &ed, 1, 1, SCENE_SURFACE_FLOOR, 9),
        CMD_RESULT_MATERIAL_NOT_LOADED);
    assert_int_equal(ed.status, EDITOR_STATUS_INVALID_MATERIAL);

    unified_editor_destroy(&ed);
    remove(path);
}

static void test_r4_increment_c_horizontal_hover_and_selection(void **state) {
    UnifiedEditorState ed;
    Camera camera;
    InputState input;
    char path[512];
    (void)state;
    path_in_tmpdir(path, sizeof(path), "r4_c_selection.tscene");
    assert_int_equal(write_text_file(path, NATIVE_SCENE_WITH_PICKABLE_LIGHT), 0);
    assert_true(unified_editor_init(&ed, &g_assets));
    assert_int_equal(unified_editor_open_native(&ed, path), SCENE_LOAD_OK);
    camera_init(&camera, 2.5, 2.5, 0.0, PI / 2.0);
    camera.pitch = -70.0;
    zero_input(&input);
    update_with(&ed, &camera, &input);
    assert_true(ed.hover.valid);
    assert_int_equal(ed.hover.target.type, SELECTION_FLOOR);
    assert_true(editor_selection_is_valid_for_map(
        ed.hover.target, scene_document_get_map(&ed.document)));

    zero_input(&input);
    input.editor_select_pressed = true;
    update_with(&ed, &camera, &input);
    assert_int_equal(ed.selection.type, SELECTION_FLOOR);
    assert_true(ed.inspector_open);
    assert_int_equal(ed.inspector_kind, EDITOR_INSPECTOR_FLOOR_SURFACE);

    camera.pitch = 70.0;
    zero_input(&input);
    update_with(&ed, &camera, &input);
    assert_true(ed.hover.valid);
    assert_int_equal(ed.hover.target.type, SELECTION_CEILING);
    zero_input(&input);
    input.editor_select_pressed = true;
    update_with(&ed, &camera, &input);
    assert_int_equal(ed.selection.type, SELECTION_CEILING);
    assert_true(ed.inspector_open);
    assert_int_equal(ed.inspector_kind, EDITOR_INSPECTOR_CEILING_SURFACE);
    unified_editor_destroy(&ed);
    remove(path);
}

static void open_horizontal_inspector(
    UnifiedEditorState *editor,
    Camera *camera,
    const char *path,
    const char *scene_text,
    double pitch
) {
    InputState input;
    assert_int_equal(write_text_file(path, scene_text), 0);
    assert_true(unified_editor_init(editor, &g_assets));
    assert_int_equal(unified_editor_open_native(editor, path), SCENE_LOAD_OK);
    camera_init(camera, 1.5, 1.5, 0.0, PI / 2.0);
    camera->pitch = pitch;
    zero_input(&input);
    update_with(editor, camera, &input);
    assert_true(editor->hover.valid);
    zero_input(&input);
    input.editor_select_pressed = true;
    update_with(editor, camera, &input);
    assert_true(editor->inspector_open);
}

static void test_r4_increment_d_surface_material_and_construction_ui(void **state) {
    UnifiedEditorState editor;
    Camera camera;
    InputState input;
    Grid *grid;
    char path[512];
    MaterialId material = 0;
    DocumentStateId state_before;
    size_t history_before;
    int x;
    int y;
    (void)state;
    path_in_tmpdir(path, sizeof(path), "r4_d_surface.tscene");
    open_horizontal_inspector(
        &editor, &camera, path, NATIVE_SCENE_WITH_PICKABLE_LIGHT, -70.0);
    editor.selection.type = SELECTION_FLOOR;
    editor.selection.value.horizontal = (HorizontalSurfaceRef){2, 1};
    editor.inspector_kind = EDITOR_INSPECTOR_FLOOR_SURFACE;
    editor.surface_field = EDITOR_SURFACE_FIELD_MATERIAL;
    assert_int_equal(editor.inspector_kind, EDITOR_INSPECTOR_FLOOR_SURFACE);
    x = editor.selection.value.horizontal.map_x;
    y = editor.selection.value.horizontal.map_y;
    assert_true(scene_document_get_surface_material(
        &editor.document, x, y, SCENE_SURFACE_FLOOR, &material));
    assert_int_equal(material, 1);

    zero_input(&input); input.editor_confirm_pressed = true;
    update_with(&editor, &camera, &input);
    assert_true(editor.material_picker_open);
    zero_input(&input); input.editor_next_pressed = true;
    assert_true(update_with(&editor, &camera, &input).keyboard_consumed);
    assert_int_equal(editor.highlighted_material, 2);
    zero_input(&input); input.editor_confirm_pressed = true;
    update_with(&editor, &camera, &input);
    assert_true(scene_document_get_surface_material(
        &editor.document, x, y, SCENE_SURFACE_FLOOR, &material));
    assert_int_equal(material, 2);

    /* The same UI command must reject the authored spawn cell atomically. */
    history_before = editor.history.count;
    editor.selection.value.horizontal = (HorizontalSurfaceRef){1, 2};
    zero_input(&input); input.editor_next_pressed = true;
    update_with(&editor, &camera, &input);
    assert_int_equal(editor.surface_field, EDITOR_SURFACE_FIELD_CONSTRUCTION);
    zero_input(&input); input.editor_confirm_pressed = true;
    update_with(&editor, &camera, &input);
    assert_int_equal(editor.last_command_result, CMD_RESULT_SPAWN_BLOCKED);
    assert_int_equal(editor.status, EDITOR_STATUS_SPAWN_BLOCKED);
    assert_int_equal(editor.document.map.cells[2 * editor.document.map.width + 1].material_id,
                     0);
    assert_int_equal(editor.history.count, history_before);
    assert_true(editor.inspector_open);
    assert_int_equal(editor.selection.type, SELECTION_FLOOR);

    /* A distinct empty, non-spawn, non-player cell succeeds through history. */
    editor.selection.value.horizontal = (HorizontalSurfaceRef){x, y};
    state_before = editor.document.current_state;
    unified_editor_set_runtime_build_failure_for_test(true);
    zero_input(&input); input.editor_confirm_pressed = true;
    update_with(&editor, &camera, &input);
    unified_editor_set_runtime_build_failure_for_test(false);
    assert_int_equal(editor.last_command_result, CMD_RESULT_OUT_OF_MEMORY);
    assert_int_equal(editor.status, EDITOR_STATUS_OUT_OF_MEMORY);
    assert_int_equal(editor.document.map.cells[y * editor.document.map.width + x].material_id,
                     0);
    assert_int_equal(editor.document.current_state, state_before);
    assert_int_equal(editor.history.count, history_before);
    assert_true(editor.inspector_open);
    assert_int_equal(editor.selection.type, SELECTION_FLOOR);

    zero_input(&input); input.editor_confirm_pressed = true;
    update_with(&editor, &camera, &input);
    assert_int_equal(editor.last_command_result, CMD_RESULT_OK);
    assert_int_equal(editor.document.map.cells[y * editor.document.map.width + x].material_id,
                     1);
    assert_int_equal(editor.history.count, history_before + 1U);
    assert_true(scene_document_is_dirty(&editor.document));
    assert_false(editor.inspector_open);
    assert_int_equal(editor.selection.type, SELECTION_NONE);
    assert_int_equal(unified_editor_undo(&editor), CMD_RESULT_OK);
    assert_int_equal(editor.document.map.cells[y * editor.document.map.width + x].material_id,
                     0);

    grid = grid_create(260, 30);
    assert_non_null(grid);
    editor.selection.type = SELECTION_FLOOR;
    editor.selection.value.horizontal = (HorizontalSurfaceRef){x, y};
    editor.inspector_kind = EDITOR_INSPECTOR_FLOOR_SURFACE;
    editor.inspector_open = true;
    editor.surface_field = EDITOR_SURFACE_FIELD_CONSTRUCTION;
    unified_editor_render_text_overlay(&editor, grid);
    assert_true(grid_contains_text(grid, "Inspector: floor surface"));
    assert_true(grid_contains_text(grid, "> Place Wall"));
    assert_true(grid_contains_text(grid, "Ambient"));
    grid_destroy(grid);
    unified_editor_destroy(&editor);
    remove(path);
}

static void test_r4_increment_d_ambient_step_numeric_undo_redo_and_escape(void **state) {
    UnifiedEditorState editor;
    Camera camera;
    InputState input;
    char path[512];
    (void)state;
    path_in_tmpdir(path, sizeof(path), "r4_d_ambient.tscene");
    open_horizontal_inspector(&editor, &camera, path, NATIVE_SCENE, -70.0);
    editor.surface_field = EDITOR_SURFACE_FIELD_AMBIENT;
    zero_input(&input); input.editor_confirm_pressed = true;
    assert_true(update_with(&editor, &camera, &input).keyboard_consumed);
    assert_true(editor.light_value_editing);

    zero_input(&input); strcpy(input.text_input, "0.80"); input.text_input_len = 4;
    update_with(&editor, &camera, &input);
    assert_true(editor.light_value_editing);
    zero_input(&input); input.editor_confirm_pressed = true;
    update_with(&editor, &camera, &input);
    assert_false(editor.light_value_editing);
    assert_true(editor.document.ambient_intensity == 0.80);
    assert_true(editor.runtime_world.ambient_intensity == 0.80);
    assert_int_equal(unified_editor_undo(&editor), CMD_RESULT_OK);
    assert_true(editor.document.ambient_intensity == 0.25);
    assert_int_equal(unified_editor_redo(&editor), CMD_RESULT_OK);
    assert_true(editor.document.ambient_intensity == 0.80);

    zero_input(&input); input.editor_confirm_pressed = true;
    update_with(&editor, &camera, &input);
    zero_input(&input); strcpy(input.text_input, "9"); input.text_input_len = 1;
    update_with(&editor, &camera, &input);
    zero_input(&input); input.editor_confirm_pressed = true;
    update_with(&editor, &camera, &input);
    assert_true(editor.light_value_editing);
    assert_int_equal(editor.status, EDITOR_STATUS_INVALID_NUMERIC_VALUE);
    zero_input(&input); input.editor_cancel_pressed = true;
    update_with(&editor, &camera, &input);
    assert_false(editor.light_value_editing);
    assert_true(editor.inspector_open);
    unified_editor_destroy(&editor);
    remove(path);
}

static void test_r4_increment_d_empty_missing_and_runtime_failure_atomic(void **state) {
    UnifiedEditorState editor;
    Camera camera;
    InputState input;
    AssetRegistry empty_assets;
    Grid *grid;
    char path[512];
    DocumentStateId state_before;
    size_t history_before;
    double ambient_before;
    WorldState runtime_before;
    (void)state;
    path_in_tmpdir(path, sizeof(path), "r4_d_failure.tscene");
    assert_int_equal(write_text_file(path, NATIVE_SCENE), 0);
    asset_registry_init(&empty_assets);
    assert_true(unified_editor_init(&editor, &empty_assets));
    assert_int_equal(unified_editor_open_native(&editor, path), SCENE_LOAD_OK);
    editor.selection.type = SELECTION_FLOOR;
    editor.selection.value.horizontal = (HorizontalSurfaceRef){1, 1};
    editor.inspector_kind = EDITOR_INSPECTOR_FLOOR_SURFACE;
    editor.inspector_open = true;
    editor.surface_field = EDITOR_SURFACE_FIELD_MATERIAL;
    grid = grid_create(260, 30);
    assert_non_null(grid);
    unified_editor_render_text_overlay(&editor, grid);
    assert_true(grid_contains_text(grid, "Material   1 (missing)"));
    assert_true(grid_contains_text(grid, "(no loaded materials)"));
    grid_destroy(grid);

    ambient_before = editor.document.ambient_intensity;
    runtime_before = editor.runtime_world;
    state_before = editor.document.current_state;
    history_before = editor.history.count;
    unified_editor_set_runtime_build_failure_for_test(true);
    assert_int_equal(unified_editor_set_ambient_intensity(&editor, 0.75),
                     CMD_RESULT_OUT_OF_MEMORY);
    unified_editor_set_runtime_build_failure_for_test(false);
    assert_true(editor.document.ambient_intensity == ambient_before);
    assert_memory_equal(&editor.runtime_world, &runtime_before, sizeof(runtime_before));
    assert_int_equal(editor.document.current_state, state_before);
    assert_int_equal(editor.history.count, history_before);
    assert_true(editor.inspector_open);
    assert_int_equal(editor.selection.type, SELECTION_FLOOR);

    camera_init(&camera, 0.5, 0.5, 0.0, PI / 2.0);
    zero_input(&input);
    update_with(&editor, &camera, &input);
    unified_editor_destroy(&editor);
    asset_registry_clear(&empty_assets);
    remove(path);
}

static void test_r4_increment_f_checked_in_v2_workflow(void **state) {
    const char *fixture = "assets/scenes/r4_surface_workflow.tscene";
    UnifiedEditorState editor;
    UnifiedEditorState reopened;
    AssetRegistry fixture_assets;
    Camera camera;
    InputState input;
    char saved_path[512];
    MaterialId material = 0U;
    SceneCellOccupancy occupancy = SCENE_CELL_OCCUPANCY_WALL;
    size_t history_before;
    (void)state;

    path_in_tmpdir(saved_path, sizeof(saved_path), "r4_surface_workflow_saved.tscene");
    fixture_assets = g_assets;
    fixture_assets.materials[3].id = 3;
    fixture_assets.materials[4].id = 4;
    snprintf(fixture_assets.material_names[3],
             sizeof(fixture_assets.material_names[3]), "%s", "mat3");
    snprintf(fixture_assets.material_names[4],
             sizeof(fixture_assets.material_names[4]), "%s", "mat4");
    fixture_assets.material_count = 5;
    assert_true(unified_editor_init(&editor, &fixture_assets));
    assert_int_equal(unified_editor_open_native(&editor, fixture), SCENE_LOAD_OK);
    assert_false(scene_document_is_dirty(&editor.document));
    assert_false(scene_document_is_repair_required(&editor.document));
    assert_string_equal(scene_document_get_name(&editor.document),
                        "r4_surface_workflow");
    assert_int_equal(editor.document.map.width, 6);
    assert_int_equal(editor.document.map.height, 6);
    assert_int_equal(editor.runtime_world.num_lights, 1);
    assert_int_equal(editor.runtime_world.num_decals, 3);
    assert_true(scene_document_get_surface_material(
        &editor.document, 4, 2, SCENE_SURFACE_WALL, &material));
    assert_int_equal(material, 4U);
    assert_true(scene_document_get_surface_material(
        &editor.document, 1, 1, SCENE_SURFACE_FLOOR, &material));
    assert_int_equal(material, 1U);
    assert_true(scene_document_get_surface_material(
        &editor.document, 1, 1, SCENE_SURFACE_CEILING, &material));
    assert_int_equal(material, 4U);

    editor.selection.type = SELECTION_WALL_FACE;
    editor.selection.value.wall_face = (WallFaceRef){4, 2, WALL_FACE_WEST};
    assert_int_equal(unified_editor_set_wall_material(&editor, 2U), CMD_RESULT_OK);
    assert_int_equal(unified_editor_set_surface_material(
        &editor, 1, 1, SCENE_SURFACE_FLOOR, 3U), CMD_RESULT_OK);
    assert_int_equal(unified_editor_set_surface_material(
        &editor, 1, 1, SCENE_SURFACE_CEILING, 2U), CMD_RESULT_OK);
    assert_int_equal(unified_editor_set_ambient_intensity(&editor, 0.6),
                     CMD_RESULT_OK);

    history_before = editor.history.count;
    assert_int_equal(unified_editor_place_wall(&editor, 2, 2), CMD_RESULT_OK);
    assert_true(scene_document_get_cell_occupancy(
        &editor.document, 2, 2, &occupancy));
    assert_int_equal(occupancy, SCENE_CELL_OCCUPANCY_WALL);
    assert_int_equal(editor.history.count, history_before + 1U);
    assert_int_equal(unified_editor_undo(&editor), CMD_RESULT_OK);
    assert_true(scene_document_get_cell_occupancy(
        &editor.document, 2, 2, &occupancy));
    assert_int_equal(occupancy, SCENE_CELL_OCCUPANCY_EMPTY);
    assert_int_equal(unified_editor_redo(&editor), CMD_RESULT_OK);
    assert_true(scene_document_get_cell_occupancy(
        &editor.document, 2, 2, &occupancy));
    assert_int_equal(occupancy, SCENE_CELL_OCCUPANCY_WALL);
    assert_int_equal(unified_editor_remove_wall(&editor, 2, 2), CMD_RESULT_OK);

    assert_int_equal(unified_editor_save_as(
        &editor, saved_path, "r4_surface_workflow_saved"), SCENE_SAVE_OK);
    assert_false(scene_document_is_dirty(&editor.document));
    assert_true(scene_document_get_surface_material(
        &editor.document, 1, 1, SCENE_SURFACE_FLOOR, &material));
    assert_int_equal(material, 3U);

    assert_int_equal(unified_editor_set_ambient_intensity(&editor, 0.2),
                     CMD_RESULT_OK);
    camera_init(&camera, 1.5, 1.5, 0.0, PI / 2.0);
    zero_input(&input);
    input.editor_reload_pressed = true;
    update_with(&editor, &camera, &input);
    assert_int_equal(editor.modal, EDITOR_MODAL_RELOAD_PROMPT);
    zero_input(&input);
    input.editor_confirm_pressed = true;
    update_with(&editor, &camera, &input);
    assert_int_equal(editor.modal, EDITOR_MODAL_NONE);
    assert_false(scene_document_is_dirty(&editor.document));
    assert_true(scene_document_get_ambient_intensity(&editor.document) == 0.6);

    assert_true(unified_editor_init(&reopened, &fixture_assets));
    assert_int_equal(unified_editor_open_native(&reopened, saved_path), SCENE_LOAD_OK);
    assert_false(scene_document_is_repair_required(&reopened.document));
    assert_int_equal(reopened.runtime_world.num_lights, 1);
    assert_int_equal(reopened.runtime_world.num_decals, 3);
    assert_true(scene_document_get_surface_material(
        &reopened.document, 4, 2, SCENE_SURFACE_WALL, &material));
    assert_int_equal(material, 2U);
    assert_true(scene_document_get_surface_material(
        &reopened.document, 1, 1, SCENE_SURFACE_FLOOR, &material));
    assert_int_equal(material, 3U);
    assert_true(scene_document_get_surface_material(
        &reopened.document, 1, 1, SCENE_SURFACE_CEILING, &material));
    assert_int_equal(material, 2U);
    assert_true(scene_document_get_cell_occupancy(
        &reopened.document, 2, 2, &occupancy));
    assert_int_equal(occupancy, SCENE_CELL_OCCUPANCY_EMPTY);
    assert_true(scene_document_get_ambient_intensity(&reopened.document) == 0.6);

    unified_editor_destroy(&reopened);
    unified_editor_destroy(&editor);
    remove(saved_path);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        /* R0 current-map open/switch workflow */
        cmocka_unit_test(test_initial_chooser_load_and_escape),
        cmocka_unit_test(test_ctrl_o_and_catalog_failure_preserve_document),
        cmocka_unit_test(test_native_open_and_legacy_import_chooser_switch),
        cmocka_unit_test(test_r2_typed_workflows_transactional),
        cmocka_unit_test(test_pathless_ctrl_s_routes_to_interactive_save_as),
        cmocka_unit_test(test_save_menu_backspace_and_empty_name_feedback),
        cmocka_unit_test(test_dirty_import_save_then_new_continues),
        cmocka_unit_test(test_save_menu_overwrite_and_cancel_are_visible),
        cmocka_unit_test(test_native_ctrl_s_menu_saves_existing_destination),
        cmocka_unit_test(test_light_hover_selection_uses_stable_id),
        cmocka_unit_test(test_light_inspector_edits_through_history_and_runtime),
        cmocka_unit_test(test_light_inspector_numeric_entry_commit_cancel_and_validation),
        cmocka_unit_test(test_light_inspector_held_arrow_repeats_after_delay),
        cmocka_unit_test(test_overlay_includes_new_scene_shortcut),
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
        /* Increment 9 regression guards */
        cmocka_unit_test(test_native_reload_preserves_light_decal_ambient),
        cmocka_unit_test(test_native_reload_clean_preserves_content),
        cmocka_unit_test(test_editor_documents_never_hold_legacy_save_path),
        cmocka_unit_test(test_r4_increment_b_controller_commands_and_safety),
        cmocka_unit_test(test_r4_increment_c_horizontal_hover_and_selection),
        cmocka_unit_test(test_r4_increment_d_surface_material_and_construction_ui),
        cmocka_unit_test(test_r4_increment_d_ambient_step_numeric_undo_redo_and_escape),
        cmocka_unit_test(test_r4_increment_d_empty_missing_and_runtime_failure_atomic),
        cmocka_unit_test(test_r4_increment_f_checked_in_v2_workflow),
    };
    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}

