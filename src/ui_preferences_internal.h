/** ui_preferences_internal.h — Focused persistence fault seam. */
#ifndef UI_PREFERENCES_INTERNAL_H
#define UI_PREFERENCES_INTERNAL_H

#include "ui_preferences.h"

typedef enum {
    UI_PREFERENCES_SAVE_FAULT_NONE = 0,
    UI_PREFERENCES_SAVE_FAULT_SYNC,
    UI_PREFERENCES_SAVE_FAULT_REPLACE,
    UI_PREFERENCES_SAVE_FAULT_DURABILITY
} UiPreferencesSaveFault;

UiPreferencesIoResult ui_preferences_internal_save(
    const UiPreferences *preferences, UiPreferencesSaveFault fault
);
UiPreferencesChangeResult ui_preferences_internal_activate_and_save(
    UiPreferences *preferences, int scale_percent, UiPreferencesSaveFault fault
);

#endif
