#include "ui_system_action_policy.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static bool in_list(const char *action, const char *const *allowed, size_t count) {
    size_t i;
    for (i = 0U; i < count; i++)
        if (strcmp(action, allowed[i]) == 0) return true;
    return false;
}

UiSystemActionPolicyResult ui_system_action_policy_check(
    UiSystemActionContext context, const char *action
) {
    static const char *const bootstrap[] = {
        "start_project", "open_editor", "open_settings", "quit"
    };
    static const char *const settings[] = {
        "ui_scale_decrease", "ui_scale_increase", "ui_scale_reset",
        "toggle_reduced_motion", "back"
    };
    static const char *const confirmation[] = {"confirm_quit", "cancel"};
    static const char *const pause[] = {
        "resume", "open_editor", "open_settings", "return_to_bootstrap", "quit"
    };
    const char *const *allowed = NULL;
    size_t count = 0U;
    if (!action || action[0] == '\0' ||
        context < UI_SYSTEM_ACTION_CONTEXT_PROTECTED_BOOTSTRAP ||
        context > UI_SYSTEM_ACTION_CONTEXT_EMERGENCY_PAUSE)
        return UI_SYSTEM_ACTION_POLICY_INVALID_ARGUMENT;
    if (context == UI_SYSTEM_ACTION_CONTEXT_PROTECTED_BOOTSTRAP) {
        allowed = bootstrap;
        count = sizeof(bootstrap) / sizeof(bootstrap[0]);
    } else if (context == UI_SYSTEM_ACTION_CONTEXT_PROTECTED_SETTINGS) {
        allowed = settings;
        count = sizeof(settings) / sizeof(settings[0]);
    } else if (context == UI_SYSTEM_ACTION_CONTEXT_PROTECTED_CONFIRMATION) {
        allowed = confirmation;
        count = sizeof(confirmation) / sizeof(confirmation[0]);
    } else if (context == UI_SYSTEM_ACTION_CONTEXT_EDITABLE_PAUSE_OVERLAY) {
        allowed = pause;
        count = sizeof(pause) / sizeof(pause[0]);
    }
    return allowed && in_list(action, allowed, count)
        ? UI_SYSTEM_ACTION_POLICY_ALLOWED : UI_SYSTEM_ACTION_POLICY_DISALLOWED;
}