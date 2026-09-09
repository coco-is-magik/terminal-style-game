#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>

#include "../src/ui_interaction.h"

typedef struct {
    UiDocument document;
    UiElementId left;
    UiElementId right;
    UiElementId down;
    UiElementId overlap;
} Fixture;

static void add_button(UiDocument *document, const char *name, const char *port,
                       UiDocumentLayout layout, UiElementId *out_id) {
    assert_int_equal(ui_document_add_element(document, UI_DOCUMENT_ELEMENT_BUTTON,
        1U, name, name, port, out_id), UI_DOCUMENT_OK);
    assert_int_equal(ui_document_set_layout(document, *out_id, layout), UI_DOCUMENT_OK);
}

static Fixture fixture(void) {
    Fixture value;
    assert_int_equal(ui_document_create_menu(&value.document, "interaction"),
                     UI_DOCUMENT_OK);
    add_button(&value.document, "left", "left_port",
        (UiDocumentLayout){1, 1, 4, 2, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}, &value.left);
    add_button(&value.document, "right", "right_port",
        (UiDocumentLayout){10, 1, 4, 2, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}, &value.right);
    add_button(&value.document, "down", "down_port",
        (UiDocumentLayout){1, 8, 4, 2, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}, &value.down);
    add_button(&value.document, "overlap", "overlap_port",
        (UiDocumentLayout){1, 1, 4, 2, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}, &value.overlap);
    return value;
}

static void test_pointer_overlap_clip_and_failure_nonmutation(void **state) {
    Fixture f = fixture();
    UiElementId hit = 99U;
    UiInteractionSession session = {f.right};
    (void)state;
    assert_int_equal(ui_interaction_hit_test(&f.document, NULL, 0U, 20, 12,
                                             2, 1, &hit), UI_INTERACTION_OK);
    assert_int_equal(hit, f.overlap);
    assert_int_equal(ui_interaction_focus_pointer(&session, &f.document, NULL, 0U,
                                                  20, 12, 19, 11),
                     UI_INTERACTION_NO_HIT);
    assert_int_equal(session.focused_element_id, f.right);
    assert_int_equal(ui_interaction_hit_test(&f.document, NULL, 0U, 2, 2,
                                             5, 5, &hit), UI_INTERACTION_NO_HIT);
    assert_int_equal(hit, f.overlap);
}

static void test_eligibility_init_traversal_wrap_and_stale_recovery(void **state) {
    Fixture f = fixture();
    UiInteractionElementState states[] = {
        {f.left, true, true},
        {f.overlap, false, false}
    };
    UiInteractionSession session = {77U};
    (void)state;
    assert_int_equal(ui_interaction_session_init(&session, &f.document, states, 2U,
                                                 20, 12), UI_INTERACTION_OK);
    assert_int_equal(session.focused_element_id, f.right);
    assert_int_equal(ui_interaction_focus_next(&session, &f.document, states, 2U,
                                               20, 12, false), UI_INTERACTION_OK);
    assert_int_equal(session.focused_element_id, f.down);
    assert_int_equal(ui_interaction_focus_next(&session, &f.document, states, 2U,
                                               20, 12, false), UI_INTERACTION_OK);
    assert_int_equal(session.focused_element_id, f.right);
    session.focused_element_id = f.left;
    assert_int_equal(ui_interaction_focus_next(&session, &f.document, states, 2U,
                                               20, 12, true), UI_INTERACTION_OK);
    assert_int_equal(session.focused_element_id, f.down);
}

static void test_directional_navigation_and_tie_order(void **state) {
    Fixture f = fixture();
    UiInteractionElementState hidden_overlap = {f.overlap, false, false};
    UiInteractionSession session = {f.left};
    UiElementId tie;
    (void)state;
    assert_int_equal(ui_interaction_focus_direction(&session, &f.document,
        &hidden_overlap, 1U, 20, 12, UI_INTERACTION_DIRECTION_RIGHT),
        UI_INTERACTION_OK);
    assert_int_equal(session.focused_element_id, f.right);
    assert_int_equal(ui_interaction_focus_direction(&session, &f.document,
        &hidden_overlap, 1U, 20, 12, UI_INTERACTION_DIRECTION_LEFT),
        UI_INTERACTION_OK);
    assert_int_equal(session.focused_element_id, f.left);
    assert_int_equal(ui_interaction_focus_direction(&session, &f.document,
        &hidden_overlap, 1U, 20, 12, UI_INTERACTION_DIRECTION_DOWN),
        UI_INTERACTION_OK);
    assert_int_equal(session.focused_element_id, f.down);
    assert_int_equal(ui_interaction_focus_direction(&session, &f.document,
        &hidden_overlap, 1U, 20, 12, UI_INTERACTION_DIRECTION_UP),
        UI_INTERACTION_OK);
    assert_int_equal(session.focused_element_id, f.left);
    add_button(&f.document, "tie", "tie_port",
        (UiDocumentLayout){10, 1, 4, 2, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}, &tie);
    assert_int_equal(ui_interaction_focus_direction(&session, &f.document,
        &hidden_overlap, 1U, 20, 12, UI_INTERACTION_DIRECTION_RIGHT),
        UI_INTERACTION_OK);
    assert_int_equal(session.focused_element_id, f.right);
    session.focused_element_id = f.down;
    assert_int_equal(ui_interaction_focus_direction(&session, &f.document,
        &hidden_overlap, 1U, 20, 12, UI_INTERACTION_DIRECTION_DOWN),
        UI_INTERACTION_NO_DIRECTIONAL_TARGET);
    assert_int_equal(session.focused_element_id, f.down);
}

static void test_activation_and_invalid_inputs_preserve_outputs(void **state) {
    Fixture f = fixture();
    UiInteractionSession session = {f.right};
    UiInteractionActivation activation = {99U, "sentinel"};
    UiInteractionElementState disabled = {f.right, true, true};
    UiInteractionElementState duplicate[] = {
        {f.left, false, true}, {f.left, false, true}
    };
    (void)state;
    assert_int_equal(ui_interaction_activate(&session, &f.document, NULL, 0U,
                                             20, 12, &activation),
                     UI_INTERACTION_OK);
    assert_int_equal(activation.element_id, f.right);
    assert_string_equal(activation.flow_port, "right_port");
    activation = (UiInteractionActivation){99U, "sentinel"};
    assert_int_equal(ui_interaction_activate(&session, &f.document, &disabled, 1U,
                                             20, 12, &activation),
                     UI_INTERACTION_INVALID_SESSION);
    assert_int_equal(activation.element_id, 99U);
    assert_string_equal(activation.flow_port, "sentinel");
    assert_int_equal(ui_interaction_focus_next(&session, &f.document, duplicate, 2U,
                                               20, 12, false),
                     UI_INTERACTION_INVALID_STATE);
    assert_int_equal(session.focused_element_id, f.right);
    f.document.elements[0].parent_id = f.left;
    assert_int_equal(ui_interaction_focus_next(&session, &f.document, NULL, 0U,
                                               20, 12, false),
                     UI_INTERACTION_INVALID_DOCUMENT);
    assert_int_equal(session.focused_element_id, f.right);
}

static void test_valid_empty_menu_and_deterministic_replay(void **state) {
    UiDocument empty;
    Fixture f = fixture();
    UiInteractionSession empty_session = {77U};
    UiInteractionSession a;
    UiInteractionSession b;
    (void)state;
    assert_int_equal(ui_document_create_menu(&empty, "empty"), UI_DOCUMENT_OK);
    assert_int_equal(ui_interaction_session_init(&empty_session, &empty, NULL, 0U,
                                                 20, 12), UI_INTERACTION_OK);
    assert_int_equal(empty_session.focused_element_id, 0U);
    assert_int_equal(ui_interaction_focus_next(&empty_session, &empty, NULL, 0U,
                                               20, 12, false),
                     UI_INTERACTION_NO_ELIGIBLE);
    assert_int_equal(empty_session.focused_element_id, 0U);
    assert_int_equal(ui_interaction_session_init(&a, &f.document, NULL, 0U, 20, 12),
                     UI_INTERACTION_OK);
    assert_int_equal(ui_interaction_session_init(&b, &f.document, NULL, 0U, 20, 12),
                     UI_INTERACTION_OK);
    assert_int_equal(ui_interaction_focus_next(&a, &f.document, NULL, 0U,
                                               20, 12, false), UI_INTERACTION_OK);
    assert_int_equal(ui_interaction_focus_next(&b, &f.document, NULL, 0U,
                                               20, 12, false), UI_INTERACTION_OK);
    assert_int_equal(a.focused_element_id, b.focused_element_id);
}

static void test_empty_clipped_button_is_ineligible(void **state) {
    UiDocument document;
    UiElementId button;
    UiInteractionSession session = {77U};
    UiElementId hit = 88U;
    (void)state;
    assert_int_equal(ui_document_create_menu(&document, "clipped"), UI_DOCUMENT_OK);
    add_button(&document, "outside", "outside_port",
        (UiDocumentLayout){100, 100, 4, 2, UI_DOCUMENT_ANCHOR_START,
                           UI_DOCUMENT_ANCHOR_START, 100}, &button);
    assert_int_equal(ui_interaction_session_init(&session, &document, NULL, 0U,
                                                 20, 12), UI_INTERACTION_OK);
    assert_int_equal(session.focused_element_id, 0U);
    assert_int_equal(ui_interaction_hit_test(&document, NULL, 0U, 20, 12,
                                             19, 11, &hit), UI_INTERACTION_NO_HIT);
    assert_int_equal(hit, 88U);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_pointer_overlap_clip_and_failure_nonmutation),
        cmocka_unit_test(test_eligibility_init_traversal_wrap_and_stale_recovery),
        cmocka_unit_test(test_directional_navigation_and_tie_order),
        cmocka_unit_test(test_activation_and_invalid_inputs_preserve_outputs),
        cmocka_unit_test(test_valid_empty_menu_and_deterministic_replay),
        cmocka_unit_test(test_empty_clipped_button_is_ineligible)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}