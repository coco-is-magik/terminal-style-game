#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "../src/scene_format.h"
#include "../src/scene_block_codec.h"

static const char V4_SCENE[] =
    "scene_type = terminal_scene\n"
    "scene_version = 4\n"
    "name = \"v4_room\"\n"
    "width = 2\nheight = 2\norigin_x = 0\norigin_y = 0\n"
    "next_instance_id = 1\nambient_intensity = 0.2\n"
    "spawn = 1.5,1.5,0\n"
    "east_growth = -\nsouth_growth = -\n\n"
    "[occupancy]\n1 1\n1 0\n\n"
    "[wall_materials]\nFFFF 00FF\n0001 0000\n\n"
    "[floor_materials]\n0000 1234\n0002 0000\n\n"
    "[ceiling_materials]\n0000 ABCD\n0003 0000\n";

static const char VALID_SCENE[] =
    "# metadata may be reordered and comments are full-line only\r\n"
    "height = 3\r\n"
    "scene_type = terminal_scene\r\n"
    "name = \"room_1\"\r\n"
    "width = 4\r\n"
    "scene_version = 1\r\n"
    "origin_y = 0\r\n"
    "origin_x = 0\r\n"
    "ambient_intensity = 0.2\r\n"
    "spawn = 1.5,1.5,-0\r\n"
    "next_instance_id = 8\r\n"
    "legacy_source_path = \"assets\\\\maps\\\\room_1.txt\"\r\n"
    "\r\n"
    "[cells]\r\n"
    "001 001 001 001\r\n"
    "001 000 000 001\r\n"
    "001 001 001 001\r\n"
    "\r\n"
    "[decal_instance 7]\r\n"
    "rotation = 0\r\n"
    "depth = 0.1\r\n"
    "glyph_step = 0,0\r\n"
    "size = 0.6,0.2\r\n"
    "uv = 0.2,0.4\r\n"
    "anchor = 2,1,0\r\n"
    "surface = wall\r\n"
    "asset_id = 6\r\n"
    "asset_kind = decal_pattern\r\n"
    "\r\n"
    "[light 3]\r\n"
    "radius = 4\r\n"
    "intensity = -1\r\n"
    "color = 255,128,0,255\r\n"
    "position = 2.5,1.5\r\n";

static const char CANONICAL_SCENE[] =
    "scene_type = terminal_scene\n"
    "scene_version = 1\n"
    "name = \"room_1\"\n"
    "width = 4\n"
    "height = 3\n"
    "origin_x = 0\n"
    "origin_y = 0\n"
    "next_instance_id = 8\n"
    "ambient_intensity = 0.20000000000000001\n"
    "spawn = 1.5,1.5,0\n"
    "legacy_source_path = \"assets\\\\maps\\\\room_1.txt\"\n"
    "\n"
    "[cells]\n"
    "001 001 001 001\n"
    "001 000 000 001\n"
    "001 001 001 001\n"
    "\n"
    "[light 3]\n"
    "position = 2.5,1.5\n"
    "color = 255,128,0,255\n"
    "intensity = -1\n"
    "radius = 4\n"
    "\n"
    "[decal_instance 7]\n"
    "asset_kind = decal_pattern\n"
    "asset_id = 6\n"
    "surface = wall\n"
    "anchor = 2,1,0\n"
    "uv = 0.20000000000000001,0.40000000000000002\n"
    "size = 0.59999999999999998,0.20000000000000001\n"
    "glyph_step = 0,0\n"
    "depth = 0.10000000000000001\n"
    "rotation = 0\n";

static const char CANONICAL_V2_SCENE[] =
    "scene_type = terminal_scene\n"
    "scene_version = 2\n"
    "name = \"room_1\"\n"
    "width = 4\n"
    "height = 3\n"
    "origin_x = 0\n"
    "origin_y = 0\n"
    "next_instance_id = 8\n"
    "ambient_intensity = 0.20000000000000001\n"
    "spawn = 1.5,1.5,0\n"
    "legacy_source_path = \"assets\\\\maps\\\\room_1.txt\"\n"
    "\n"
    "[occupancy]\n"
    "1 1 1 1\n"
    "1 0 0 1\n"
    "1 1 1 1\n"
    "\n"
    "[wall_materials]\n"
    "001 001 001 001\n"
    "001 009 009 001\n"
    "001 001 001 001\n"
    "\n"
    "[floor_materials]\n"
    "009 009 009 009\n"
    "009 009 009 009\n"
    "009 009 009 009\n"
    "\n"
    "[ceiling_materials]\n"
    "009 009 009 009\n"
    "009 009 009 009\n"
    "009 009 009 009\n"
    "\n"
    "[light 3]\n"
    "position = 2.5,1.5\n"
    "color = 255,128,0,255\n"
    "intensity = -1\n"
    "radius = 4\n"
    "\n"
    "[decal_instance 7]\n"
    "asset_kind = decal_pattern\n"
    "asset_id = 6\n"
    "surface = wall\n"
    "anchor = 2,1,0\n"
    "uv = 0.20000000000000001,0.40000000000000002\n"
    "size = 0.59999999999999998,0.20000000000000001\n"
    "glyph_step = 0,0\n"
    "depth = 0.10000000000000001\n"
    "rotation = 0\n";

static char *replace_once(const char *source, const char *old_text,
                          const char *new_text) {
    const char *match = strstr(source, old_text);
    size_t prefix;
    size_t result_size;
    char *result;
    assert_non_null(match);
    prefix = (size_t)(match - source);
    result_size = strlen(source) - strlen(old_text) + strlen(new_text);
    result = malloc(result_size + 1U);
    assert_non_null(result);
    memcpy(result, source, prefix);
    memcpy(result + prefix, new_text, strlen(new_text));
    memcpy(result + prefix + strlen(new_text), match + strlen(old_text),
           strlen(match + strlen(old_text)) + 1U);
    return result;
}

static SceneDiagnostic parse_rejected(const char *text,
                                      SceneFormatCandidate *candidate) {
    SceneDiagnostic diagnostic;
    assert_int_equal(scene_format_parse(text, strlen(text), "bad.tscene",
                                        candidate, &diagnostic),
                     SCENE_FORMAT_REJECTED);
    return diagnostic;
}

static void *fail_calloc(size_t count, size_t size) {
    (void)count;
    (void)size;
    return NULL;
}

static void test_parse_and_canonical_round_trip(void **state) {
    SceneFormatCandidate first;
    SceneFormatCandidate second;
    SceneFormatBuffer buffer = {0};
    SceneFormatBuffer second_buffer = {0};
    SceneDiagnostic diagnostic;
    (void)state;
    scene_format_candidate_init(&first);
    scene_format_candidate_init(&second);
    assert_int_equal(scene_format_parse(VALID_SCENE, strlen(VALID_SCENE),
                                        "room.tscene", &first, &diagnostic),
                     SCENE_FORMAT_OK);
    assert_int_equal(first.map.width, 4);
    assert_int_equal(first.map.height, 3);
    assert_non_null(first.map.light_map);
    assert_int_equal(first.light_count, 1);
    assert_int_equal(first.decal_count, 1);
    assert_int_equal(first.lights[0].id, 3);
    assert_int_equal(first.decals[0].id, 7);
    assert_string_equal(first.legacy_source_path, "assets\\maps\\room_1.txt");

    assert_int_equal(scene_format_serialize(&first, &buffer, &diagnostic),
                     SCENE_FORMAT_OK);
    assert_int_equal(buffer.size, strlen(CANONICAL_SCENE));
    assert_memory_equal(buffer.data, CANONICAL_SCENE, buffer.size);
    assert_int_equal(buffer.data[buffer.size - 1U], '\n');
    assert_null(strchr(buffer.data, '\r'));

    assert_int_equal(scene_format_parse(buffer.data, buffer.size, "canonical.tscene",
                                        &second, &diagnostic), SCENE_FORMAT_OK);
    assert_int_equal(scene_format_serialize(&second, &second_buffer, &diagnostic),
                     SCENE_FORMAT_OK);
    assert_int_equal(second_buffer.size, buffer.size);
    assert_memory_equal(second_buffer.data, buffer.data, buffer.size);
    scene_format_buffer_destroy(&second_buffer);
    scene_format_buffer_destroy(&buffer);
    scene_format_candidate_destroy(&second);
    scene_format_candidate_destroy(&first);
}

static void test_failed_parse_preserves_candidate(void **state) {
    SceneFormatCandidate candidate;
    SceneDiagnostic diagnostic;
    char *bad;
    MapCell *original_cells;
    (void)state;
    scene_format_candidate_init(&candidate);
    assert_int_equal(scene_format_parse(CANONICAL_SCENE, strlen(CANONICAL_SCENE),
                                        NULL, &candidate, &diagnostic), SCENE_FORMAT_OK);
    original_cells = candidate.map.cells;
    bad = replace_once(CANONICAL_SCENE, "width = 4", "width = 0");
    assert_int_equal(scene_format_parse(bad, strlen(bad), NULL, &candidate,
                                        &diagnostic), SCENE_FORMAT_REJECTED);
    assert_ptr_equal(candidate.map.cells, original_cells);
    assert_int_equal(candidate.map.width, 4);
    free(bad);
    scene_format_candidate_destroy(&candidate);
}

static void test_required_duplicate_unknown_and_syntax_diagnostics(void **state) {
    SceneFormatCandidate candidate;
    SceneDiagnostic diagnostic;
    char *text;
    (void)state;
    scene_format_candidate_init(&candidate);
    text = replace_once(CANONICAL_SCENE, "height = 3\n", "");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_REQUIRED_MISSING);
    free(text);
    text = replace_once(CANONICAL_SCENE, "height = 3\n", "height = 3\nheight = 3\n");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_DUPLICATE);
    free(text);
    text = replace_once(CANONICAL_SCENE, "height = 3\n", "mystery = 3\nheight = 3\n");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_UNKNOWN);
    free(text);
    text = replace_once(CANONICAL_SCENE, "name = \"room_1\"",
                        "name = \"room\\n1\"");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_SYNTAX);
    free(text);
    text = replace_once(CANONICAL_SCENE, "ambient_intensity = 0.20000000000000001",
                        "ambient_intensity = 0.2 # inline");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_NUMERIC);
    free(text);
    scene_format_candidate_destroy(&candidate);
}

static void test_numeric_dimensions_and_cells_diagnostics(void **state) {
    SceneFormatCandidate candidate;
    SceneDiagnostic diagnostic;
    char *text;
    (void)state;
    scene_format_candidate_init(&candidate);
    text = replace_once(CANONICAL_SCENE, "width = 4", "width = 513");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS);
    free(text);
    text = replace_once(CANONICAL_SCENE, "ambient_intensity = 0.20000000000000001",
                        "ambient_intensity = nan");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_NUMERIC);
    free(text);
    text = replace_once(CANONICAL_SCENE, "001 000 000 001", "001 000 000");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS);
    free(text);
    text = replace_once(CANONICAL_SCENE, "001 000 000 001", "001 000 000 256");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_NUMERIC);
    free(text);
    scene_format_candidate_destroy(&candidate);
}

static void test_instance_identity_and_section_fields(void **state) {
    SceneFormatCandidate candidate;
    SceneDiagnostic diagnostic;
    char *text;
    (void)state;
    scene_format_candidate_init(&candidate);
    text = replace_once(CANONICAL_SCENE, "[decal_instance 7]", "[decal_instance 3]");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_INSTANCE_ID);
    free(text);
    text = replace_once(CANONICAL_SCENE, "next_instance_id = 8", "next_instance_id = 7");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_HIGH_WATER);
    free(text);
    text = replace_once(CANONICAL_SCENE, "radius = 4\n", "");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_REQUIRED_MISSING);
    free(text);
    text = replace_once(CANONICAL_SCENE, "radius = 4\n", "radius = 4\nradius = 4\n");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_DUPLICATE);
    free(text);
    text = replace_once(CANONICAL_SCENE, "radius = 4\n", "radius = 4\nextra = 1\n");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_UNKNOWN);
    free(text);
    scene_format_candidate_destroy(&candidate);
}

static void test_surface_applicability_and_bounds(void **state) {
    SceneFormatCandidate candidate;
    SceneDiagnostic diagnostic;
    char *text;
    (void)state;
    scene_format_candidate_init(&candidate);
    text = replace_once(CANONICAL_SCENE, "surface = wall", "surface = floor");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_REQUIRED_MISSING);
    free(text);
    text = replace_once(CANONICAL_SCENE, "uv = 0.20000000000000001,0.40000000000000002",
                        "uv = 1.1,0.4");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_NUMERIC);
    free(text);
    text = replace_once(CANONICAL_SCENE, "spawn = 1.5,1.5,0", "spawn = 0.5,0.5,0");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_NUMERIC);
    free(text);
    scene_format_candidate_destroy(&candidate);
}

static void test_exhausted_sentinel_and_negative_zero(void **state) {
    SceneFormatCandidate candidate;
    SceneFormatBuffer buffer = {0};
    SceneDiagnostic diagnostic;
    char *text;
    (void)state;
    scene_format_candidate_init(&candidate);
    text = replace_once(CANONICAL_SCENE, "next_instance_id = 8",
                        "next_instance_id = 18446744073709551615");
    assert_int_equal(scene_format_parse(text, strlen(text), NULL, &candidate,
                                        &diagnostic), SCENE_FORMAT_OK);
    assert_int_equal(candidate.next_instance_id, SCENE_INSTANCE_ID_EXHAUSTED);
    candidate.spawn_angle = -0.0;
    assert_int_equal(scene_format_serialize(&candidate, &buffer, &diagnostic),
                     SCENE_FORMAT_OK);
    assert_non_null(strstr(buffer.data, "spawn = 1.5,1.5,0\n"));
    free(text);
    scene_format_buffer_destroy(&buffer);
    scene_format_candidate_destroy(&candidate);
}

static void test_locale_independence_without_global_mutation(void **state) {
    SceneFormatCandidate candidate;
    SceneFormatBuffer buffer = {0};
    SceneDiagnostic diagnostic;
    const char *before;
    char saved[128];
    (void)state;
    before = setlocale(LC_NUMERIC, NULL);
    assert_non_null(before);
    strncpy(saved, before, sizeof(saved) - 1U);
    saved[sizeof(saved) - 1U] = '\0';
    scene_format_candidate_init(&candidate);
    assert_int_equal(scene_format_parse(CANONICAL_SCENE, strlen(CANONICAL_SCENE),
                                        NULL, &candidate, &diagnostic), SCENE_FORMAT_OK);
    assert_int_equal(scene_format_serialize(&candidate, &buffer, &diagnostic),
                     SCENE_FORMAT_OK);
    assert_string_equal(setlocale(LC_NUMERIC, NULL), saved);
    assert_non_null(strstr(buffer.data, "ambient_intensity = 0.20000000000000001\n"));
    scene_format_buffer_destroy(&buffer);
    scene_format_candidate_destroy(&candidate);
}

static void test_metadata_after_sections_and_floor_decal(void **state) {
    SceneFormatCandidate candidate;
    SceneFormatBuffer buffer = {0};
    SceneDiagnostic diagnostic;
    char *text;
    char *moved;
    (void)state;
    scene_format_candidate_init(&candidate);
    text = replace_once(CANONICAL_SCENE, "name = \"room_1\"\n", "");
    moved = replace_once(text, "[light 3]\n", "[light 3]\nname = \"room_1\"\n");
    free(text);
    text = replace_once(moved, "surface = wall\nanchor = 2,1,0\n"
                               "uv = 0.20000000000000001,0.40000000000000002\n",
                        "surface = floor\nposition = 2,1.5,0\n");
    free(moved);
    assert_int_equal(scene_format_parse(text, strlen(text), NULL, &candidate,
                                        &diagnostic), SCENE_FORMAT_OK);
    assert_int_equal(candidate.decals[0].surface, SCENE_DECAL_SURFACE_FLOOR);
    assert_int_equal(scene_format_serialize(&candidate, &buffer, &diagnostic),
                     SCENE_FORMAT_OK);
    assert_non_null(strstr(buffer.data, "surface = floor\nposition = 2,1.5,0\n"));
    free(text);
    scene_format_buffer_destroy(&buffer);
    scene_format_candidate_destroy(&candidate);
}

static void test_id_overflow_unknown_section_and_provenance_control(void **state) {
    SceneFormatCandidate candidate;
    SceneFormatBuffer buffer = {0};
    SceneDiagnostic diagnostic;
    char *text;
    char bad_path[] = {'a', '\n', 'b', '\0'};
    (void)state;
    scene_format_candidate_init(&candidate);
    text = replace_once(CANONICAL_SCENE, "[light 3]",
                        "[light 18446744073709551616]");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_UNKNOWN);
    free(text);
    text = replace_once(CANONICAL_SCENE, "[light 3]", "[future 3]");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_UNKNOWN);
    free(text);
    assert_int_equal(scene_format_parse(CANONICAL_SCENE, strlen(CANONICAL_SCENE),
                                        NULL, &candidate, &diagnostic), SCENE_FORMAT_OK);
    free(candidate.legacy_source_path);
    candidate.legacy_source_path = bad_path;
    assert_int_equal(scene_format_serialize(&candidate, &buffer, &diagnostic),
                     SCENE_FORMAT_REJECTED);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_SYNTAX);
    candidate.legacy_source_path = NULL;
    scene_format_buffer_destroy(&buffer);
    scene_format_candidate_destroy(&candidate);
}

static void test_file_line_and_nul_limits(void **state) {
    SceneFormatCandidate candidate;
    SceneDiagnostic diagnostic;
    char *large;
    char embedded[] = "scene\0data";
    (void)state;
    scene_format_candidate_init(&candidate);
    large = malloc(SCENE_FILE_MAX_BYTES + 2U);
    assert_non_null(large);
    memset(large, 'x', SCENE_FILE_MAX_BYTES + 1U);
    large[SCENE_FILE_MAX_BYTES + 1U] = '\0';
    diagnostic = parse_rejected(large, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS);
    free(large);
    assert_int_equal(scene_format_parse(embedded, sizeof(embedded) - 1U, NULL,
                                        &candidate, &diagnostic), SCENE_FORMAT_REJECTED);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_SYNTAX);
    scene_format_candidate_destroy(&candidate);
}

static void test_v1_to_v2_migration_maps_authored_cells_exactly(void **state) {
    SceneFormatCandidate candidate;
    SceneFormatCandidate reparsed;
    SceneFormatBuffer buffer = {0};
    SceneDiagnostic diagnostic;
    SceneAuthoredCell *authored;
    (void)state;

    scene_format_candidate_init(&candidate);
    scene_format_candidate_init(&reparsed);
    assert_int_equal(scene_format_parse(CANONICAL_SCENE, strlen(CANONICAL_SCENE),
                                        "v1.tscene", &candidate, &diagnostic),
                     SCENE_FORMAT_OK);
    assert_int_equal(candidate.source_version, SCENE_VERSION_V1);
    assert_null(candidate.authored_cells);

    assert_int_equal(scene_format_migrate_v1_to_v2(&candidate, 9U, &diagnostic),
                     SCENE_FORMAT_OK);
    assert_int_equal(candidate.source_version, SCENE_VERSION_V2);
    assert_non_null(candidate.authored_cells);
    assert_int_equal(candidate.authored_cell_count, 12U);

    authored = candidate.authored_cells;
    assert_int_equal(authored[0].occupancy, SCENE_CELL_OCCUPANCY_WALL);
    assert_int_equal(authored[0].wall_material, 1U);
    assert_int_equal(authored[0].floor_material, 9U);
    assert_int_equal(authored[0].ceiling_material, 9U);
    assert_int_equal(authored[5].occupancy, SCENE_CELL_OCCUPANCY_EMPTY);
    assert_int_equal(authored[5].wall_material, 9U);
    assert_int_equal(authored[5].floor_material, 9U);
    assert_int_equal(authored[5].ceiling_material, 9U);
    assert_int_equal(authored[11].occupancy, SCENE_CELL_OCCUPANCY_WALL);
    assert_int_equal(authored[11].wall_material, 1U);
    assert_int_equal(scene_format_validate(&candidate, "v2.tscene", &diagnostic),
                     SCENE_FORMAT_OK);
    assert_int_equal(scene_format_serialize(&candidate, &buffer, &diagnostic),
                     SCENE_FORMAT_OK);
    assert_int_equal(buffer.size, strlen(CANONICAL_V2_SCENE));
    assert_memory_equal(buffer.data, CANONICAL_V2_SCENE, buffer.size);
    assert_int_equal(scene_format_parse(buffer.data, buffer.size, "v2.tscene",
                                        &reparsed, &diagnostic), SCENE_FORMAT_OK);
    assert_int_equal(reparsed.source_version, SCENE_VERSION_V2);
    assert_int_equal(reparsed.authored_cell_count, 12U);
    assert_int_equal(reparsed.authored_cells[5].occupancy,
                     SCENE_CELL_OCCUPANCY_EMPTY);
    assert_int_equal(reparsed.authored_cells[5].wall_material, 9U);
    assert_int_equal(reparsed.map.cells[5].material_id, 0);

    authored[5].occupancy = (SceneCellOccupancy)99;
    assert_int_equal(scene_format_validate(&candidate, "v2.tscene", &diagnostic),
                     SCENE_FORMAT_REJECTED);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_NUMERIC);
    authored[5].occupancy = SCENE_CELL_OCCUPANCY_EMPTY;

    scene_format_candidate_destroy(&reparsed);
    scene_format_buffer_destroy(&buffer);
    scene_format_candidate_destroy(&candidate);
    assert_null(candidate.authored_cells);
    assert_int_equal(candidate.authored_cell_count, 0U);
    assert_int_equal(candidate.source_version, 0U);
}

static void test_v2_rejects_missing_duplicate_and_malformed_grids(void **state) {
    SceneFormatCandidate candidate;
    SceneDiagnostic diagnostic;
    char *text;
    (void)state;

    scene_format_candidate_init(&candidate);
    text = replace_once(CANONICAL_V2_SCENE,
                        "[floor_materials]\n009 009 009 009\n"
                        "009 009 009 009\n009 009 009 009\n\n", "");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_REQUIRED_MISSING);
    free(text);

    text = replace_once(CANONICAL_V2_SCENE, "[ceiling_materials]\n",
                        "[ceiling_materials]\n009 009 009 009\n"
                        "009 009 009 009\n009 009 009 009\n\n"
                        "[ceiling_materials]\n");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_DUPLICATE);
    free(text);

    text = replace_once(CANONICAL_V2_SCENE, "1 0 0 1\n", "1 2 0 1\n");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_SYNTAX);
    free(text);

    text = replace_once(CANONICAL_V2_SCENE, "001 009 009 001\n",
                        "001 000 009 001\n");
    diagnostic = parse_rejected(text, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_NUMERIC);
    free(text);
    scene_format_candidate_destroy(&candidate);
}

static void test_v3_growth_metadata_round_trips_and_is_versioned(void **state) {
    SceneFormatCandidate candidate;
    SceneFormatCandidate reparsed;
    SceneFormatBuffer buffer = {0};
    SceneDiagnostic diagnostic;
    char *v3;
    char *bad;
    (void)state;
    scene_format_candidate_init(&candidate);
    scene_format_candidate_init(&reparsed);
    v3 = replace_once(CANONICAL_V2_SCENE, "scene_version = 2\n",
        "scene_version = 3\n");
    bad = replace_once(v3, "spawn = 1.5,1.5,0\n",
        "spawn = 1.5,1.5,0\neast_growth = 1,2\nsouth_growth = 3\n");
    free(v3);
    assert_int_equal(scene_format_parse(bad, strlen(bad), "v3.tscene",
                                        &candidate, &diagnostic), SCENE_FORMAT_OK);
    free(bad);
    assert_int_equal(candidate.source_version, SCENE_VERSION_V3);
    assert_int_equal(candidate.east_growth_count, 2U);
    assert_int_equal(candidate.east_growth[1], 2);
    assert_int_equal(candidate.south_growth_count, 1U);
    assert_int_equal(scene_format_serialize(&candidate, &buffer, &diagnostic),
                     SCENE_FORMAT_OK);
    assert_non_null(strstr(buffer.data, "east_growth = 1,2\n"));
    assert_non_null(strstr(buffer.data, "south_growth = 3\n"));
    assert_int_equal(scene_format_parse(buffer.data, buffer.size, "v3.tscene",
                                        &reparsed, &diagnostic), SCENE_FORMAT_OK);
    assert_int_equal(reparsed.east_growth_count, 2U);
    scene_format_candidate_destroy(&reparsed);
    scene_format_buffer_destroy(&buffer);
    scene_format_candidate_destroy(&candidate);

    scene_format_candidate_init(&candidate);
    v3 = replace_once(CANONICAL_V2_SCENE, "scene_version = 2\n",
        "scene_version = 3\n");
    diagnostic = parse_rejected(v3, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_REQUIRED_MISSING);
    free(v3);
    bad = replace_once(CANONICAL_V2_SCENE, "spawn = 1.5,1.5,0\n",
        "spawn = 1.5,1.5,0\neast_growth = -\nsouth_growth = -\n");
    diagnostic = parse_rejected(bad, &candidate);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_REQUIRED_MISSING);
    free(bad);
    scene_format_candidate_destroy(&candidate);
}

static void test_v1_to_v2_migration_rejects_invalid_requests_transactionally(void **state) {
    SceneFormatCandidate candidate;
    SceneDiagnostic diagnostic;
    MapCell *original_cells;
    (void)state;

    scene_format_candidate_init(&candidate);
    assert_int_equal(scene_format_parse(CANONICAL_SCENE, strlen(CANONICAL_SCENE),
                                        "v1.tscene", &candidate, &diagnostic),
                     SCENE_FORMAT_OK);
    original_cells = candidate.map.cells;

    assert_int_equal(scene_format_migrate_v1_to_v2(&candidate, 0U, &diagnostic),
                     SCENE_FORMAT_REJECTED);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_NUMERIC);
    assert_int_equal(candidate.source_version, SCENE_VERSION_V1);
    assert_ptr_equal(candidate.map.cells, original_cells);
    assert_null(candidate.authored_cells);
    assert_int_equal(candidate.authored_cell_count, 0U);

    assert_int_equal(scene_format_migrate_v1_to_v2(&candidate, 65536U, &diagnostic),
                     SCENE_FORMAT_REJECTED);
    assert_int_equal(candidate.source_version, SCENE_VERSION_V1);
    assert_null(candidate.authored_cells);

    candidate.map.cells[0].material_id = -1;
    assert_int_equal(scene_format_migrate_v1_to_v2(&candidate, 1U, &diagnostic),
                     SCENE_FORMAT_REJECTED);
    assert_int_equal(candidate.source_version, SCENE_VERSION_V1);
    assert_null(candidate.authored_cells);
    candidate.map.cells[0].material_id = 1;
    scene_format_set_allocator_for_test(fail_calloc);
    assert_int_equal(scene_format_migrate_v1_to_v2(&candidate, 1U, &diagnostic),
                     SCENE_FORMAT_OUT_OF_MEMORY);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_ENV_ALLOCATION);
    assert_int_equal(candidate.source_version, SCENE_VERSION_V1);
    assert_null(candidate.authored_cells);
    assert_int_equal(candidate.authored_cell_count, 0U);
    scene_format_reset_allocator_for_test();
    scene_format_candidate_destroy(&candidate);
}

static void test_scene_block_codec(void **state) {
    uint16_t value = 0U;
    char formatted[SCENE_BLOCK_TEXT_SIZE];
    size_t count = 0U;
    (void)state;

    assert_true(scene_block_parse("0000", &value));
    assert_true(scene_block_is_null(value));
    assert_true(scene_block_parse("FFFF", &value));
    assert_int_equal(value, UINT16_MAX);
    assert_true(scene_block_format(UINT16_C(0x12AF), formatted));
    assert_string_equal(formatted, "12AF");
    assert_true(scene_block_tokens_equal("00FF", "00FF"));
    assert_false(scene_block_tokens_equal("00FF", "00FE"));
    assert_true(scene_block_count("0001 00FF\tABCD", &count));
    assert_int_equal(count, 3U);
    assert_false(scene_block_parse("ffff", &value));
    assert_false(scene_block_parse("FFF", &value));
    assert_false(scene_block_count("0001 bad!", &count));
}

static void test_v4_hex_blocks_high_ids_and_nulls(void **state) {
    SceneFormatCandidate candidate;
    SceneFormatCandidate reparsed;
    SceneFormatBuffer buffer = {0};
    SceneDiagnostic diagnostic;
    char *bad;
    (void)state;

    scene_format_candidate_init(&candidate);
    scene_format_candidate_init(&reparsed);
    assert_int_equal(scene_format_parse(V4_SCENE, strlen(V4_SCENE), "v4.tscene",
                                        &candidate, &diagnostic), SCENE_FORMAT_OK);
    assert_int_equal(candidate.source_version, SCENE_VERSION_V4);
    assert_int_equal(candidate.authored_cells[0].wall_material, UINT16_MAX);
    assert_int_equal(candidate.authored_cells[1].floor_material, UINT16_C(0x1234));
    assert_int_equal(candidate.authored_cells[1].ceiling_material, UINT16_C(0xABCD));
    assert_int_equal(candidate.authored_cells[3].wall_material, 0U);
    assert_int_equal(scene_format_serialize(&candidate, &buffer, &diagnostic),
                     SCENE_FORMAT_OK);
    assert_non_null(strstr(buffer.data, "FFFF 00FF\n"));
    assert_non_null(strstr(buffer.data, "0000 1234\n"));
    assert_int_equal(scene_format_parse(buffer.data, buffer.size, "v4.tscene",
                                        &reparsed, &diagnostic), SCENE_FORMAT_OK);
    assert_int_equal(reparsed.authored_cells[1].ceiling_material,
                     UINT16_C(0xABCD));

    bad = replace_once(V4_SCENE, "FFFF 00FF\n", "ffff 00FF\n");
    diagnostic = parse_rejected(bad, &reparsed);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_SYNTAX);
    free(bad);
    bad = replace_once(V4_SCENE, "FFFF 00FF\n", "0000 00FF\n");
    diagnostic = parse_rejected(bad, &reparsed);
    assert_int_equal(diagnostic.code, SCENE_DIAGNOSTIC_INPUT_NUMERIC);
    free(bad);

    scene_format_buffer_destroy(&buffer);
    scene_format_candidate_destroy(&reparsed);
    scene_format_candidate_destroy(&candidate);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_parse_and_canonical_round_trip),
        cmocka_unit_test(test_failed_parse_preserves_candidate),
        cmocka_unit_test(test_required_duplicate_unknown_and_syntax_diagnostics),
        cmocka_unit_test(test_numeric_dimensions_and_cells_diagnostics),
        cmocka_unit_test(test_instance_identity_and_section_fields),
        cmocka_unit_test(test_surface_applicability_and_bounds),
        cmocka_unit_test(test_exhausted_sentinel_and_negative_zero),
        cmocka_unit_test(test_locale_independence_without_global_mutation),
        cmocka_unit_test(test_metadata_after_sections_and_floor_decal),
        cmocka_unit_test(test_id_overflow_unknown_section_and_provenance_control),
        cmocka_unit_test(test_file_line_and_nul_limits),
        cmocka_unit_test(test_v1_to_v2_migration_maps_authored_cells_exactly),
        cmocka_unit_test(test_v1_to_v2_migration_rejects_invalid_requests_transactionally),
        cmocka_unit_test(test_v2_rejects_missing_duplicate_and_malformed_grids),
        cmocka_unit_test(test_v3_growth_metadata_round_trips_and_is_versioned),
        cmocka_unit_test(test_scene_block_codec),
        cmocka_unit_test(test_v4_hex_blocks_high_ids_and_nulls)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
