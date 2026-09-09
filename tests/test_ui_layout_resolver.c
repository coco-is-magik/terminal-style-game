#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>

#include "../src/ui_layout_resolver.h"

static UiDocument make_document(UiElementId *panel, UiElementId *child) {
    UiDocument document;
    assert_int_equal(ui_document_create_menu(&document, "layout_test"), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_CONTAINER,
        1U, "panel", "", "", panel), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_add_element(&document, UI_DOCUMENT_ELEMENT_TEXT,
        *panel, "child", "X", "", child), UI_DOCUMENT_OK);
    return document;
}

static const UiResolvedElement *find_resolved(const UiResolvedElement *elements,
                                              size_t count, UiElementId id) {
    size_t i;
    for (i = 0U; i < count; i++) if (elements[i].element_id == id) return &elements[i];
    return NULL;
}

static void test_all_anchors_and_scale_rounding(void **state) {
    UiElementId panel, child;
    UiDocument document = make_document(&panel, &child);
    UiResolvedElement resolved[UI_DOCUMENT_MAX_ELEMENTS];
    const UiResolvedElement *value;
    size_t count = 99U;
    (void)state;
    assert_int_equal(ui_document_set_layout(&document, panel,
        (UiDocumentLayout){2, 3, 20, 10, UI_DOCUMENT_ANCHOR_CENTER,
                           UI_DOCUMENT_ANCHOR_END, 150}), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_layout(&document, child,
        (UiDocumentLayout){1, -1, 3, 3, UI_DOCUMENT_ANCHOR_END,
                           UI_DOCUMENT_ANCHOR_CENTER, 150}), UI_DOCUMENT_OK);
    assert_int_equal(ui_layout_resolve(&document, 100, 50, resolved, &count),
                     UI_LAYOUT_RESOLVE_OK);
    value = find_resolved(resolved, count, panel);
    assert_non_null(value);
    assert_int_equal(value->rect.x, 37);
    assert_int_equal(value->rect.y, 32);
    assert_int_equal(value->rect.width, 30);
    assert_int_equal(value->rect.height, 15);
    value = find_resolved(resolved, count, child);
    assert_non_null(value);
    assert_int_equal(value->rect.x, 61);
    assert_int_equal(value->rect.y, 36);
    assert_int_equal(value->rect.width, 5);
    assert_int_equal(value->rect.height, 5);
}

static void test_stretch_nested_clip_and_small_viewport(void **state) {
    UiElementId panel, child;
    UiDocument document = make_document(&panel, &child);
    UiResolvedElement resolved[UI_DOCUMENT_MAX_ELEMENTS];
    const UiResolvedElement *value;
    size_t count;
    (void)state;
    assert_int_equal(ui_document_set_layout(&document, panel,
        (UiDocumentLayout){2, 1, 3, 2, UI_DOCUMENT_ANCHOR_STRETCH,
                           UI_DOCUMENT_ANCHOR_STRETCH, 275}), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_layout(&document, child,
        (UiDocumentLayout){-4, 2, 8, 8, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}), UI_DOCUMENT_OK);
    assert_int_equal(ui_layout_resolve(&document, 12, 7, resolved, &count),
                     UI_LAYOUT_RESOLVE_OK);
    value = find_resolved(resolved, count, panel);
    assert_int_equal(value->rect.x, 2);
    assert_int_equal(value->rect.y, 1);
    assert_int_equal(value->rect.width, 7);
    assert_int_equal(value->rect.height, 4);
    value = find_resolved(resolved, count, child);
    assert_int_equal(value->rect.x, -2);
    assert_int_equal(value->clip.x, 2);
    assert_int_equal(value->clip.y, 3);
    assert_int_equal(value->clip.width, 4);
    assert_int_equal(value->clip.height, 2);
}

static void test_paint_order_and_failure_preserve_output(void **state) {
    UiElementId panel, child;
    UiDocument document = make_document(&panel, &child);
    UiResolvedElement resolved[UI_DOCUMENT_MAX_ELEMENTS];
    UiResolvedElement before[UI_DOCUMENT_MAX_ELEMENTS];
    size_t count = 0U;
    size_t before_count;
    (void)state;
    assert_int_equal(ui_layout_resolve(&document, 80, 25, resolved, &count),
                     UI_LAYOUT_RESOLVE_OK);
    assert_int_equal(count, 3U);
    assert_int_equal(find_resolved(resolved, count, panel)->paint_order, 1U);
    assert_int_equal(find_resolved(resolved, count, child)->paint_order, 2U);
    memcpy(before, resolved, sizeof(before));
    before_count = count;
    document.elements[1].layout.scale_percent = 0;
    assert_int_equal(ui_layout_resolve(&document, 80, 25, resolved, &count),
                     UI_LAYOUT_RESOLVE_INVALID_DOCUMENT);
    assert_memory_equal(resolved, before, sizeof(before));
    assert_int_equal(count, before_count);
    assert_int_equal(ui_layout_resolve(&document, 0, 25, resolved, &count),
                     UI_LAYOUT_RESOLVE_INVALID_ARGUMENT);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_all_anchors_and_scale_rounding),
        cmocka_unit_test(test_stretch_nested_clip_and_small_viewport),
        cmocka_unit_test(test_paint_order_and_failure_preserve_output)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}