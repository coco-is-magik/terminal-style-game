#include "entity_trigger_session.h"

#include <math.h>
#include <string.h>

static bool point_inside(const SceneTrigger *trigger, double x, double y) {
    return x >= trigger->min_x && x < trigger->max_x &&
           y >= trigger->min_y && y < trigger->max_y;
}

static bool light_exists(const SceneLight *lights, size_t count,
                         SceneInstanceId id) {
    size_t i;
    for (i = 0U; i < count; i++) if (lights[i].id == id) return true;
    return false;
}

static bool trigger_valid(const SceneTrigger *trigger,
                          const SceneLight *lights, size_t light_count) {
    if (!trigger || trigger->id == SCENE_INSTANCE_ID_INVALID ||
        !isfinite(trigger->min_x) || !isfinite(trigger->min_y) ||
        !isfinite(trigger->max_x) || !isfinite(trigger->max_y) ||
        trigger->min_x >= trigger->max_x || trigger->min_y >= trigger->max_y ||
        trigger->condition != SCENE_TRIGGER_CONDITION_ENTER_REGION ||
        trigger->action < SCENE_TRIGGER_ACTION_SET_FLAG ||
        trigger->action > SCENE_TRIGGER_ACTION_TOGGLE_LIGHT) return false;
    if (trigger->action == SCENE_TRIGGER_ACTION_SET_FLAG)
        return trigger->flag_id >= 1U &&
               trigger->flag_id <= SCENE_TRIGGER_FLAG_CAPACITY;
    if (trigger->action == SCENE_TRIGGER_ACTION_TOGGLE_LIGHT)
        return trigger->target_id != SCENE_INSTANCE_ID_INVALID &&
               light_exists(lights, light_count, trigger->target_id);
    return true;
}

static bool was_inside(const EntityTriggerSession *session, SceneInstanceId id) {
    size_t i;
    for (i = 0U; i < SCENE_MAX_TRIGGERS; i++)
        if (session->trigger_ids[i] == id) return session->inside[i];
    return false;
}

void entity_trigger_session_init(EntityTriggerSession *session) {
    if (session) memset(session, 0, sizeof(*session));
}

void entity_trigger_session_reset(EntityTriggerSession *session) {
    entity_trigger_session_init(session);
}

bool entity_trigger_session_flag(const EntityTriggerSession *session,
                                 uint8_t flag_id) {
    return session && flag_id >= 1U && flag_id <= SCENE_TRIGGER_FLAG_CAPACITY
        ? session->flags[flag_id - 1U] : false;
}

bool entity_trigger_session_light_enabled(const EntityTriggerSession *session,
                                          SceneInstanceId light_id) {
    size_t i;
    if (!session || light_id == SCENE_INSTANCE_ID_INVALID) return false;
    for (i = 0U; i < session->disabled_light_count; i++)
        if (session->disabled_lights[i] == light_id) return false;
    return true;
}

static void toggle_light(EntityTriggerSession *session, SceneInstanceId id) {
    size_t i;
    for (i = 0U; i < session->disabled_light_count; i++) {
        if (session->disabled_lights[i] == id) {
            session->disabled_lights[i] =
                session->disabled_lights[--session->disabled_light_count];
            return;
        }
    }
    if (session->disabled_light_count < SCENE_MAX_LIGHTS)
        session->disabled_lights[session->disabled_light_count++] = id;
}

EntityTriggerResult entity_trigger_session_tick(
    EntityTriggerSession *session, const SceneTrigger *triggers,
    size_t trigger_count, const SceneLight *lights, size_t light_count,
    double spawn_x, double spawn_y, double spawn_angle,
    double player_x, double player_y, double delta_seconds,
    EntityTriggerTickResult *out_result
) {
    size_t order[SCENE_MAX_TRIGGERS];
    size_t i, j;
    EntityTriggerSession next;
    EntityTriggerTickResult result;
    (void)delta_seconds;
    if (!session || !out_result || trigger_count > SCENE_MAX_TRIGGERS ||
        light_count > SCENE_MAX_LIGHTS || (trigger_count && !triggers) ||
        (light_count && !lights) || !isfinite(spawn_x) || !isfinite(spawn_y) ||
        !isfinite(spawn_angle) || !isfinite(player_x) || !isfinite(player_y) ||
        !isfinite(delta_seconds) || delta_seconds < 0.0)
        return ENTITY_TRIGGER_INVALID_ARGUMENT;
    for (i = 0U; i < trigger_count; i++) {
        size_t duplicate;
        if (!trigger_valid(&triggers[i], lights, light_count))
            return ENTITY_TRIGGER_INVALID_DATA;
        for (duplicate = 0U; duplicate < i; duplicate++)
            if (triggers[duplicate].id == triggers[i].id)
                return ENTITY_TRIGGER_INVALID_DATA;
        order[i] = i;
        for (j = i; j > 0U &&
             triggers[order[j - 1U]].id > triggers[order[j]].id; j--) {
            size_t swap = order[j - 1U]; order[j - 1U] = order[j]; order[j] = swap;
        }
    }
    next = *session;
    result = (EntityTriggerTickResult){player_x, player_y, 0.0, false, 0U};
    for (i = 0U; i < trigger_count; i++) {
        size_t index = order[i];
        const SceneTrigger *trigger = &triggers[index];
        bool now_inside = point_inside(trigger, player_x, player_y);
        if (now_inside && !was_inside(session, trigger->id)) {
            result.fired_count++;
            if (trigger->action == SCENE_TRIGGER_ACTION_SET_FLAG)
                next.flags[trigger->flag_id - 1U] = trigger->flag_value;
            else if (trigger->action == SCENE_TRIGGER_ACTION_TOGGLE_LIGHT)
                toggle_light(&next, trigger->target_id);
            else {
                result.player_x = spawn_x; result.player_y = spawn_y;
                result.player_angle = spawn_angle; result.teleported = true;
            }
        }
    }
    memset(next.trigger_ids, 0, sizeof(next.trigger_ids));
    memset(next.inside, 0, sizeof(next.inside));
    for (i = 0U; i < trigger_count; i++) {
        next.trigger_ids[i] = triggers[i].id;
        next.inside[i] = point_inside(&triggers[i], result.player_x, result.player_y);
    }
    *session = next;
    *out_result = result;
    return ENTITY_TRIGGER_OK;
}