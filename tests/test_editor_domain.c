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
    for (field = 0; field < EDITOR_LIGHT_FIELD_COUNT; field++) {
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
    assert_int_equal(EDITOR_LIGHT_FIELD_COUNT, 7);
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
    assert_string_equal(inspector.title, "materials");
    assert_string_equal(inspector.controls, "Up/Down  Enter=apply");
    assert_non_null(inspector.note);
    assert_int_equal(inspector.field_count, 1U);
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
        EDITOR_INSPECTOR_WALL_MATERIAL, 1U, &document.map, &field));
    assert_false(editor_domain_inspector_field_presentation(
        EDITOR_INSPECTOR_LIGHT, EDITOR_LIGHT_FIELD_COUNT,
        &document.map, &field));
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
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}