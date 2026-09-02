#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "../src/entity_trigger_session.h"

static SceneTrigger make_trigger(SceneInstanceId id, SceneTriggerActionType action) {
    SceneTrigger trigger = {
        .id = id, .min_x = 1.0, .min_y = 1.0, .max_x = 2.0, .max_y = 2.0,
        .condition = SCENE_TRIGGER_CONDITION_ENTER_REGION, .action = action,
        .flag_id = 1U, .flag_value = true, .target_id = 9U
    };
    return trigger;
}

static void test_once_per_entry_and_half_open_edges(void **state) {
    EntityTriggerSession session;
    EntityTriggerTickResult result;
    SceneTrigger trigger = make_trigger(1U, SCENE_TRIGGER_ACTION_SET_FLAG);
    (void)state;
    entity_trigger_session_init(&session);
    assert_int_equal(entity_trigger_session_tick(&session, &trigger, 1U, NULL, 0U,
        0.0, 0.0, 0.0, 1.0, 1.0, 0.1, &result), ENTITY_TRIGGER_OK);
    assert_int_equal(result.fired_count, 1U);
    assert_true(entity_trigger_session_flag(&session, 1U));
    assert_int_equal(entity_trigger_session_tick(&session, &trigger, 1U, NULL, 0U,
        0.0, 0.0, 0.0, 1.5, 1.5, 0.1, &result), ENTITY_TRIGGER_OK);
    assert_int_equal(result.fired_count, 0U);
    assert_int_equal(entity_trigger_session_tick(&session, &trigger, 1U, NULL, 0U,
        0.0, 0.0, 0.0, 2.0, 1.5, 0.1, &result), ENTITY_TRIGGER_OK);
    assert_int_equal(entity_trigger_session_tick(&session, &trigger, 1U, NULL, 0U,
        0.0, 0.0, 0.0, 1.5, 1.5, 0.1, &result), ENTITY_TRIGGER_OK);
    assert_int_equal(result.fired_count, 1U);
}

static void test_teleport_is_nonrecursive_and_recomputes_inside(void **state) {
    EntityTriggerSession session;
    EntityTriggerTickResult result;
    SceneTrigger triggers[2] = {
        make_trigger(2U, SCENE_TRIGGER_ACTION_SET_FLAG),
        make_trigger(1U, SCENE_TRIGGER_ACTION_TELEPORT_TO_SPAWN)
    };
    triggers[0].min_x = 4.0; triggers[0].max_x = 5.0;
    (void)state;
    entity_trigger_session_init(&session);
    assert_int_equal(entity_trigger_session_tick(&session, triggers, 2U, NULL, 0U,
        4.5, 1.5, 0.75, 1.5, 1.5, 0.1, &result), ENTITY_TRIGGER_OK);
    assert_true(result.teleported);
    assert_true(result.player_x == 4.5 && result.player_angle == 0.75);
    assert_int_equal(result.fired_count, 1U);
    assert_false(entity_trigger_session_flag(&session, 1U));
    assert_int_equal(entity_trigger_session_tick(&session, triggers, 2U, NULL, 0U,
        4.5, 1.5, 0.75, 4.5, 1.5, 0.1, &result), ENTITY_TRIGGER_OK);
    assert_int_equal(result.fired_count, 0U);
}

static void test_toggle_light_reset_and_invalid_atomic(void **state) {
    EntityTriggerSession session, before;
    EntityTriggerTickResult result;
    SceneTrigger trigger = make_trigger(1U, SCENE_TRIGGER_ACTION_TOGGLE_LIGHT);
    SceneLight light = {.id = 9U};
    (void)state;
    entity_trigger_session_init(&session);
    assert_true(entity_trigger_session_light_enabled(&session, 9U));
    assert_int_equal(entity_trigger_session_tick(&session, &trigger, 1U, &light, 1U,
        0.0, 0.0, 0.0, 1.5, 1.5, 0.1, &result), ENTITY_TRIGGER_OK);
    assert_false(entity_trigger_session_light_enabled(&session, 9U));
    entity_trigger_session_reset(&session);
    assert_true(entity_trigger_session_light_enabled(&session, 9U));
    before = session;
    trigger.target_id = 10U;
    assert_int_equal(entity_trigger_session_tick(&session, &trigger, 1U, &light, 1U,
        0.0, 0.0, 0.0, 1.5, 1.5, 0.1, &result), ENTITY_TRIGGER_INVALID_DATA);
    assert_memory_equal(&session, &before, sizeof(session));
}

static void test_reordering_preserves_inside_identity(void **state) {
    EntityTriggerSession session;
    EntityTriggerTickResult result;
    SceneTrigger triggers[2] = {
        make_trigger(2U, SCENE_TRIGGER_ACTION_SET_FLAG),
        make_trigger(1U, SCENE_TRIGGER_ACTION_SET_FLAG)
    };
    SceneTrigger swap;
    triggers[0].flag_id = 2U;
    (void)state;
    entity_trigger_session_init(&session);
    assert_int_equal(entity_trigger_session_tick(&session, triggers, 2U, NULL, 0U,
        0.0, 0.0, 0.0, 1.5, 1.5, 0.1, &result), ENTITY_TRIGGER_OK);
    assert_int_equal(result.fired_count, 2U);
    swap = triggers[0]; triggers[0] = triggers[1]; triggers[1] = swap;
    assert_int_equal(entity_trigger_session_tick(&session, triggers, 2U, NULL, 0U,
        0.0, 0.0, 0.0, 1.5, 1.5, 0.1, &result), ENTITY_TRIGGER_OK);
    assert_int_equal(result.fired_count, 0U);
    triggers[1].id = triggers[0].id;
    assert_int_equal(entity_trigger_session_tick(&session, triggers, 2U, NULL, 0U,
        0.0, 0.0, 0.0, 1.5, 1.5, 0.1, &result), ENTITY_TRIGGER_INVALID_DATA);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_once_per_entry_and_half_open_edges),
        cmocka_unit_test(test_teleport_is_nonrecursive_and_recomputes_inside),
        cmocka_unit_test(test_toggle_light_reset_and_invalid_atomic)
        ,cmocka_unit_test(test_reordering_preserves_inside_identity)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}