/**
 * entity_trigger_session.h — Deterministic, allocation-free trigger runtime
 *
 * Consumes borrowed authored records and owns only disposable session state.
 * It owns no rendering, input, authored document, command history, or global time.
 */
#ifndef ENTITY_TRIGGER_SESSION_H
#define ENTITY_TRIGGER_SESSION_H

#include "scene_types.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    SceneInstanceId trigger_ids[SCENE_MAX_TRIGGERS];
    bool inside[SCENE_MAX_TRIGGERS];
    bool flags[SCENE_TRIGGER_FLAG_CAPACITY];
    SceneInstanceId disabled_lights[SCENE_MAX_LIGHTS];
    size_t disabled_light_count;
} EntityTriggerSession;

typedef struct {
    double player_x;
    double player_y;
    double player_angle;
    bool teleported;
    size_t fired_count;
} EntityTriggerTickResult;

typedef enum {
    ENTITY_TRIGGER_OK = 0,
    ENTITY_TRIGGER_INVALID_ARGUMENT,
    ENTITY_TRIGGER_INVALID_DATA
} EntityTriggerResult;

void entity_trigger_session_init(EntityTriggerSession *session);
void entity_trigger_session_reset(EntityTriggerSession *session);
bool entity_trigger_session_flag(const EntityTriggerSession *session,
                                 uint8_t flag_id);
bool entity_trigger_session_light_enabled(const EntityTriggerSession *session,
                                          SceneInstanceId light_id);
EntityTriggerResult entity_trigger_session_tick(
    EntityTriggerSession *session,
    const SceneTrigger *triggers,
    size_t trigger_count,
    const SceneLight *lights,
    size_t light_count,
    double spawn_x,
    double spawn_y,
    double spawn_angle,
    double player_x,
    double player_y,
    double delta_seconds,
    EntityTriggerTickResult *out_result
);

#endif /* ENTITY_TRIGGER_SESSION_H */