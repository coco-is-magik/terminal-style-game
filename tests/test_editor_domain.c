/** test_editor_domain.c — Headless typed inspector adapter regressions */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <math.h>
#include <cmocka.h>

#include <stdlib.h>
#include <string.h>

#include "../src/editor_domain.h"

static void init_document(SceneDocument *document) {
    Map *map;
    scene_document_init(document);
    map = map_create(5, 4);
    assert_non_null(map);
    document->map = *map;
    free(map);
    document->lights = calloc(1U, sizeof(*document->lights));
    assert_non_null(document->lights);
    document->light_count = 1U;
    document->light_capacity = 1U;
    document->lights[0] = (SceneLight){
        .id = 11U, .x = 2.5, .y = 1.5,
        .red = 10U, .green = 20U, .blue = 30U, .alpha = 255U,
        .intensity = -1.0, .radius = 3.0
    };
}

static SelectionTarget light_target(SceneInstanceId id) {
    SelectionTarget target = {0};
    target.type = SELECTION_LIGHT;
    target.value.light.id = id;
    return target;
}

static SelectionTarget surface_target(SelectionType type, int x, int y) {
    SelectionTarget target = {0};
    target.type = type;
    target.value.horizontal = (HorizontalSurfaceRef){x, y};
    return target;
}

static SelectionTarget decal_target(SceneInstanceId id) {
    SelectionTarget target = {0};
    target.type = SELECTION_DECAL;
    target.value.decal.id = id;
    return target;
}

static void test_dispatch_and_wall_request(void **state) {
    SelectionTarget target = {0};
    EditorMutationRequest request;
    (void)state;

    assert_int_equal(editor_domain_inspector_kind(target), EDITOR_INSPECTOR_NONE);
    target.type = SELECTION_WALL_FACE;
    target.value.wall_face.map_x = 3;
    target.value.wall_face.map_y = 2;
    target.value.wall_face.face = WALL_FACE_NORTH;
    assert_int_equal(editor_domain_inspector_kind(target),
                     EDITOR_INSPECTOR_WALL_MATERIAL);
    assert_true(editor_domain_make_wall_material_request(target, 12, &request));
    assert_int_equal(request.type, EDITOR_MUTATION_SET_WALL_MATERIAL);
    assert_int_equal(request.data.wall_material.wall.map_x, 3);
    assert_int_equal(request.data.wall_material.wall.map_y, 2);
    assert_int_equal(request.data.wall_material.material, 12);
    assert_false(editor_domain_make_wall_material_request(
        light_target(11U), 1, &request));
}

static void test_light_metadata_and_formatting(void **state) {
    SceneDocument document;
    EditorLightFieldMetadata metadata;
    char text[32];
    int field;
    (void)state;

    init_document(&document);
    assert_int_equal(editor_domain_inspector_kind(light_target(11U)),
                     EDITOR_INSPECTOR_LIGHT);
    for (field = 0; field < EDITOR_LIGHT_FIELD_REMOVE; field++) {
        assert_true(editor_domain_light_field_metadata(
            (EditorLightField)field, &document.map, &metadata));
        assert_non_null(metadata.label);
        assert_true(metadata.minimum < metadata.maximum);
        assert_true(metadata.step > 0.0);
        assert_true(editor_domain_format_light_field(
            &document.lights[0], (EditorLightField)field, text, sizeof(text)));
        assert_true(text[0] != '\0');
    }
    assert_true(editor_domain_light_field_metadata(
        EDITOR_LIGHT_FIELD_X, &document.map, &metadata));
    assert_true(metadata.minimum == 0.0);
    assert_true(metadata.maximum == 4.99);
    assert_true(metadata.step == 0.25);
    assert_true(editor_domain_light_field_metadata(
        EDITOR_LIGHT_FIELD_RED, &document.map, &metadata));
    assert_true(metadata.minimum == 0.0 && metadata.maximum == 255.0);
    assert_false(editor_domain_light_field_metadata(
        EDITOR_LIGHT_FIELD_REMOVE, &document.map, &metadata));
    assert_true(editor_domain_format_light_field(
        &document.lights[0], EDITOR_LIGHT_FIELD_REMOVE, text, sizeof(text)));
    assert_string_equal(text, "Enter=remove");
    assert_int_equal(EDITOR_LIGHT_FIELD_COUNT, 8);
    assert_false(editor_domain_light_field_metadata(
        EDITOR_LIGHT_FIELD_COUNT, &document.map, &metadata));
    assert_false(editor_domain_light_field_metadata(
        EDITOR_LIGHT_FIELD_X, NULL, &metadata));
    scene_document_destroy(&document);
}

static void test_light_step_requests_are_typed_and_bounded(void **state) {
    SceneDocument document;
    EditorMutationRequest request;
    SelectionTarget target = light_target(11U);
    (void)state;

    init_document(&document);
    assert_true(editor_domain_make_light_step_request(
        &document, target, EDITOR_LIGHT_FIELD_X, 1, &request));
    assert_int_equal(request.type, EDITOR_MUTATION_SET_LIGHT);
    assert_int_equal(request.data.light.id, 11U);
    assert_true(request.data.light.value.x == 2.75);
    assert_true(document.lights[0].x == 2.5);

    assert_true(editor_domain_make_light_step_request(
        &document, target, EDITOR_LIGHT_FIELD_INTENSITY, -1, &request));
    assert_true(request.data.light.value.intensity == -1.25);
    assert_true(editor_domain_make_light_step_request(
        &document, target, EDITOR_LIGHT_FIELD_RED, 1, &request));
    assert_int_equal(request.data.light.value.red, 11U);

    document.lights[0].red = 255U;
    assert_true(editor_domain_make_light_step_request(
        &document, target, EDITOR_LIGHT_FIELD_RED, 1, &request));
    assert_int_equal(request.data.light.value.red, 255U);
    document.lights[0].radius = 0.25;
    assert_true(editor_domain_make_light_step_request(
        &document, target, EDITOR_LIGHT_FIELD_RADIUS, -1, &request));
    assert_true(request.data.light.value.radius == 0.25);
    scene_document_destroy(&document);
}

static void test_light_request_rejects_invalid_inputs(void **state) {
    SceneDocument document;
    EditorMutationRequest request;
    SelectionTarget target = light_target(99U);
    (void)state;

    init_document(&document);
    assert_false(editor_domain_make_light_step_request(
        &document, target, EDITOR_LIGHT_FIELD_X, 1, &request));
    target.type = SELECTION_NONE;
    assert_false(editor_domain_make_light_step_request(
        &document, target, EDITOR_LIGHT_FIELD_X, 1, &request));
    assert_false(editor_domain_make_light_step_request(
        &document, light_target(11U), EDITOR_LIGHT_FIELD_X, 0, &request));
    assert_false(editor_domain_make_light_step_request(
        NULL, light_target(11U), EDITOR_LIGHT_FIELD_X, 1, &request));
    assert_false(editor_domain_make_light_step_request(
        &document, light_target(11U), EDITOR_LIGHT_FIELD_X, 1, NULL));
    scene_document_destroy(&document);
}

static void test_light_value_requests_are_typed_and_bounded(void **state) {
    SceneDocument document;
    EditorMutationRequest request;
    SelectionTarget target = light_target(11U);
    (void)state;
    init_document(&document);
    assert_true(editor_domain_make_light_value_request(
        &document, target, EDITOR_LIGHT_FIELD_INTENSITY, 12.5, &request));
    assert_true(request.data.light.value.intensity == 12.5);
    assert_true(editor_domain_make_light_value_request(
        &document, target, EDITOR_LIGHT_FIELD_RED, 200.0, &request));
    assert_int_equal(request.data.light.value.red, 200U);
    assert_false(editor_domain_make_light_value_request(
        &document, target, EDITOR_LIGHT_FIELD_RED, 2.5, &request));
    assert_false(editor_domain_make_light_value_request(
        &document, target, EDITOR_LIGHT_FIELD_RADIUS, 0.0, &request));
    assert_false(editor_domain_make_light_value_request(
        &document, target, EDITOR_LIGHT_FIELD_X, NAN, &request));
    assert_false(editor_domain_make_light_value_request(
        &document, light_target(99U), EDITOR_LIGHT_FIELD_X, 1.0, &request));
    scene_document_destroy(&document);
}

static void test_shared_inspector_presentation_is_typed(void **state) {
    SceneDocument document;
    EditorInspectorPresentation inspector;
    EditorInspectorFieldPresentation field;
    (void)state;

    init_document(&document);
    assert_true(editor_domain_inspector_presentation(
        EDITOR_INSPECTOR_WALL_MATERIAL, &inspector));
    assert_string_equal(inspector.title, "wall surface");
    assert_true(editor_domain_inspector_field_presentation(
        EDITOR_INSPECTOR_LIGHT, EDITOR_LIGHT_FIELD_REMOVE,
        &document.map, &field));
    assert_string_equal(field.label, "Remove");
    assert_int_equal(field.kind, EDITOR_INSPECTOR_FIELD_CHOICE);
    assert_true(field.minimum == 0.0 && field.maximum == 0.0 && field.step == 0.0);

    assert_true(editor_domain_inspector_field_presentation(
        EDITOR_INSPECTOR_FLOOR_SURFACE, EDITOR_SURFACE_FIELD_GRAVITY_DIRECTION,
        &document.map, &field));
    assert_string_equal(field.label, "Cell gravity dir");
    assert_true(editor_domain_inspector_field_presentation(
        EDITOR_INSPECTOR_FLOOR_SURFACE, EDITOR_SURFACE_FIELD_MOVEMENT,
        &document.map, &field));
    assert_string_equal(field.label, "Map movement");
    assert_true(editor_domain_movement_field_presentation(
        EDITOR_MOVEMENT_FIELD_GRAVITY_MAGNITUDE, &field));
    assert_string_equal(field.label, "Map gravity");
    assert_string_equal(inspector.controls,
                        "Up/Down=choose  Enter=open/apply  Esc=back");
    assert_non_null(inspector.note);
    assert_int_equal(inspector.field_count, EDITOR_SURFACE_FIELD_COUNT);
    assert_true(editor_domain_inspector_field_presentation(
        EDITOR_INSPECTOR_WALL_MATERIAL, 0U, &document.map, &field));
    assert_string_equal(field.label, "Material");
    assert_int_equal(field.kind, EDITOR_INSPECTOR_FIELD_CHOICE);

    assert_true(editor_domain_inspector_presentation(
        EDITOR_INSPECTOR_LIGHT, &inspector));
    assert_string_equal(inspector.title, "point light");
    assert_string_equal(inspector.controls,
                        "Up/Down  Left/Right=edit  Type+Enter=set");
    assert_non_null(inspector.note);
    assert_int_equal(inspector.field_count, EDITOR_LIGHT_FIELD_COUNT);
    assert_true(editor_domain_inspector_field_presentation(
        EDITOR_INSPECTOR_LIGHT, EDITOR_LIGHT_FIELD_RADIUS,
        &document.map, &field));
    assert_string_equal(field.label, "Radius");
    assert_int_equal(field.kind, EDITOR_INSPECTOR_FIELD_NUMBER);
    assert_true(field.minimum == 0.25 && field.step == 0.25);

    assert_false(editor_domain_inspector_presentation(
        EDITOR_INSPECTOR_NONE, &inspector));
    assert_false(editor_domain_inspector_field_presentation(
        EDITOR_INSPECTOR_WALL_MATERIAL, EDITOR_SURFACE_FIELD_COUNT,
        &document.map, &field));
    assert_false(editor_domain_inspector_field_presentation(
        EDITOR_INSPECTOR_LIGHT, EDITOR_LIGHT_FIELD_COUNT,
        &document.map, &field));
    scene_document_destroy(&document);
}

static void test_surface_dispatch_metadata_and_requests(void **state) {
    SelectionTarget floor = surface_target(SELECTION_FLOOR, 2, 3);
    SelectionTarget ceiling = surface_target(SELECTION_CEILING, 1, 2);
    SelectionTarget wall = {0};
    EditorInspectorPresentation inspector;
    EditorInspectorFieldPresentation field;
    EditorMutationRequest request;
    (void)state;
    wall.type = SELECTION_WALL_FACE;
    wall.value.wall_face = (WallFaceRef){4, 1, WALL_FACE_WEST};

    assert_int_equal(editor_domain_inspector_kind(floor),
                     EDITOR_INSPECTOR_FLOOR_SURFACE);
    assert_int_equal(editor_domain_inspector_kind(ceiling),
                     EDITOR_INSPECTOR_CEILING_SURFACE);
    assert_true(editor_domain_inspector_presentation(
        EDITOR_INSPECTOR_FLOOR_SURFACE, &inspector));
    assert_string_equal(inspector.title, "floor surface");
    assert_int_equal(inspector.field_count, EDITOR_SURFACE_FIELD_COUNT);
    assert_true(editor_domain_inspector_field_presentation(
        EDITOR_INSPECTOR_FLOOR_SURFACE, EDITOR_SURFACE_FIELD_MATERIAL,
        NULL, &field));
    assert_int_equal(field.kind, EDITOR_INSPECTOR_FIELD_CHOICE);
    assert_true(editor_domain_inspector_field_presentation(
        EDITOR_INSPECTOR_WALL_MATERIAL, EDITOR_SURFACE_FIELD_CONSTRUCTION,
        NULL, &field));
    assert_string_equal(field.label, "Remove Wall");
    assert_true(editor_domain_inspector_field_presentation(
        EDITOR_INSPECTOR_CEILING_SURFACE, EDITOR_SURFACE_FIELD_AMBIENT,
        NULL, &field));
    assert_true(field.minimum == 0.0 && field.maximum == 1.0 && field.step == 0.05);

    assert_true(editor_domain_make_surface_material_request(floor, 12, &request));
    assert_int_equal(request.type, EDITOR_MUTATION_SET_FLOOR_MATERIAL);
    assert_int_equal(request.data.surface_material.map_x, 2);
    assert_true(editor_domain_make_surface_material_request(ceiling, 7, &request));
    assert_int_equal(request.type, EDITOR_MUTATION_SET_CEILING_MATERIAL);
    assert_true(editor_domain_make_surface_material_request(wall, 3, &request));
    assert_int_equal(request.type, EDITOR_MUTATION_SET_WALL_MATERIAL);
    assert_false(editor_domain_make_surface_material_request(floor, 0, &request));

    assert_true(editor_domain_make_construction_request(floor, &request));
    assert_int_equal(request.type, EDITOR_MUTATION_PLACE_WALL);
    assert_true(editor_domain_make_construction_request(wall, &request));
    assert_int_equal(request.type, EDITOR_MUTATION_REMOVE_WALL);
    assert_false(editor_domain_make_construction_request(light_target(11U), &request));
}

static void test_ambient_metadata_format_step_and_value(void **state) {
    SceneDocument document;
    EditorAmbientMetadata metadata;
    EditorMutationRequest request;
    char text[16];
    (void)state;
    init_document(&document);
    document.ambient_intensity = 0.50;
    assert_true(editor_domain_ambient_metadata(&metadata));
    assert_true(metadata.minimum == 0.0 && metadata.maximum == 1.0);
    assert_true(metadata.step == 0.05);
    assert_int_equal(metadata.decimal_places, 2U);
    assert_true(editor_domain_format_ambient(0.5, text, sizeof(text)));
    assert_string_equal(text, "0.50");
    assert_false(editor_domain_format_ambient(NAN, text, sizeof(text)));
    assert_true(editor_domain_make_ambient_step_request(&document, 1, &request));
    assert_int_equal(request.type, EDITOR_MUTATION_SET_AMBIENT_INTENSITY);
    assert_true(request.data.ambient.intensity == 0.55);
    document.ambient_intensity = 0.99;
    assert_true(editor_domain_make_ambient_step_request(&document, 1, &request));
    assert_true(request.data.ambient.intensity == 1.0);
    assert_true(editor_domain_make_ambient_value_request(0.25, &request));
    assert_true(request.data.ambient.intensity == 0.25);
    assert_false(editor_domain_make_ambient_value_request(1.1, &request));
    assert_false(editor_domain_make_ambient_step_request(&document, 0, &request));
    scene_document_destroy(&document);
}

static void test_decal_inspector_fields_and_typed_requests(void **state) {
    SceneDocument document;
    EditorInspectorPresentation inspector;
    EditorInspectorFieldPresentation field;
    EditorMutationRequest request;
    char text[32];
    (void)state;
    init_document(&document);
    document.decals = calloc(1U, sizeof(*document.decals));
    assert_non_null(document.decals);
    document.decal_count = document.decal_capacity = 1U;
    document.decals[0] = (SceneDecalInstance){
        .id = 21U, .asset = {SCENE_ASSET_KIND_DECAL_PATTERN, 6U},
        .surface = SCENE_DECAL_SURFACE_FLOOR, .x = 2.5, .y = 1.5,
        .width = 1.0, .height = 0.5, .depth = 0.1, .rotation = 0.0};
    assert_int_equal(editor_domain_inspector_kind(decal_target(21U)),
                     EDITOR_INSPECTOR_DECAL);
    assert_true(editor_domain_inspector_presentation(
        EDITOR_INSPECTOR_DECAL, &inspector));
    assert_int_equal(inspector.field_count, EDITOR_DECAL_FIELD_COUNT);
    assert_true(editor_domain_decal_field_presentation(
        EDITOR_DECAL_FIELD_WIDTH, &field));
    assert_int_equal(field.kind, EDITOR_INSPECTOR_FIELD_NUMBER);
    assert_true(editor_domain_decal_field_presentation(
        EDITOR_DECAL_FIELD_REMOVE, &field));
    assert_int_equal(field.kind, EDITOR_INSPECTOR_FIELD_CHOICE);
    assert_true(editor_domain_format_decal_field(
        &document.decals[0], EDITOR_DECAL_FIELD_REMOVE, text, sizeof(text)));
    assert_string_equal(text, "Enter=remove");
    assert_true(editor_domain_make_decal_step_request(
        &document, decal_target(21U), EDITOR_DECAL_FIELD_WIDTH, 1, &request));
    assert_int_equal(request.type, EDITOR_MUTATION_SET_DECAL);
    assert_true(request.data.decal.value.width == 1.05);
    assert_true(editor_domain_make_decal_value_request(
        &document, decal_target(21U), EDITOR_DECAL_FIELD_ROTATION, 1.5, &request));
    assert_true(request.data.decal.value.rotation == 1.5);
    assert_false(editor_domain_make_decal_value_request(
        &document, decal_target(21U), EDITOR_DECAL_FIELD_REMOVE, 1.0, &request));
    scene_document_destroy(&document);
}

static void test_vertical_and_movement_domain_requests(void **state) {
    SceneDocument document;
    SelectionTarget floor = surface_target(SELECTION_FLOOR, 1, 1);
    SelectionTarget ceiling = surface_target(SELECTION_CEILING, 1, 1);
    EditorMutationRequest request;
    EditorInspectorFieldPresentation field;
    (void)state;
    init_document(&document);
    document.authored_cells = calloc(20U, sizeof(*document.authored_cells));
    assert_non_null(document.authored_cells);
    document.authored_cell_count = 20U;
    for (size_t i = 0U; i < 20U; i++) {
        document.authored_cells[i].floor_height_step =
            SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
        document.authored_cells[i].ceiling_height_step =
            SCENE_DEFAULT_CEILING_HEIGHT_STEP;
        document.authored_cells[i].floor_present = true;
        document.authored_cells[i].ceiling_present = true;
    }
    assert_true(editor_domain_inspector_field_presentation(
        EDITOR_INSPECTOR_FLOOR_SURFACE, EDITOR_SURFACE_FIELD_HEIGHT,
        &document.map, &field));
    assert_string_equal(field.label, "Height");
    assert_true(field.step == 0.25);
    assert_true(editor_domain_make_height_step_request(
        &document, floor, 1, &request));
    assert_int_equal(request.type, EDITOR_MUTATION_SET_CELL_VERTICAL);
    assert_int_equal(request.data.cell_vertical.value.floor_height_step,
                     UINT16_C(0x0040));
    assert_true(editor_domain_make_height_step_request(
        &document, ceiling, 1, &request));
    assert_int_equal(request.data.cell_vertical.value.ceiling_height_step,
                     UINT16_C(0x0140));
    assert_true(editor_domain_make_surface_presence_request(
        &document, floor, &request));
    assert_false(request.data.cell_vertical.value.floor_present);
    assert_true(editor_domain_make_gravity_direction_step_request(
        &document, floor, -1, &request));
    assert_int_equal(request.data.cell_vertical.value.gravity_orientation,
                     SCENE_GRAVITY_WEST);
    assert_true(editor_domain_make_gravity_scale_step_request(
        &document, floor, 1, &request));
    assert_int_equal(request.data.cell_vertical.value.gravity_scale_step,
                     UINT16_C(0x0040));
    assert_true(editor_domain_movement_field_presentation(
        EDITOR_MOVEMENT_FIELD_AIR_CONTROL, &field));
    assert_string_equal(field.label, "Air control");
    assert_true(editor_domain_make_movement_step_request(
        &document, EDITOR_MOVEMENT_FIELD_AIR_CONTROL, -1, &request));
    assert_int_equal(request.type, EDITOR_MUTATION_SET_MOVEMENT_PARAMETERS);
    assert_true(request.data.movement.value.air_control_scale == 0.95);
    assert_true(editor_domain_make_movement_step_request(
        &document, EDITOR_MOVEMENT_FIELD_GRAVITY_ORIENTATION, -1, &request));
    assert_int_equal(request.data.movement.value.gravity_orientation,
                     SCENE_GRAVITY_WEST);
    assert_false(editor_domain_make_height_step_request(
        &document, (SelectionTarget){0}, 1, &request));
    scene_document_destroy(&document);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_dispatch_and_wall_request),
        cmocka_unit_test(test_light_metadata_and_formatting),
        cmocka_unit_test(test_light_step_requests_are_typed_and_bounded),
        cmocka_unit_test(test_light_request_rejects_invalid_inputs),
        cmocka_unit_test(test_light_value_requests_are_typed_and_bounded),
        cmocka_unit_test(test_shared_inspector_presentation_is_typed),
        cmocka_unit_test(test_surface_dispatch_metadata_and_requests),
        cmocka_unit_test(test_ambient_metadata_format_step_and_value),
        cmocka_unit_test(test_decal_inspector_fields_and_typed_requests),
        cmocka_unit_test(test_vertical_and_movement_domain_requests),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}