#ifndef UI_PREFERENCES_H
#define UI_PREFERENCES_H

#include <stdbool.h>
#include <stddef.h>

#define UI_PREFERENCES_VERSION 1
#define UI_PREFERENCES_EMERGENCY_SCALE 150
#define UI_PREFERENCES_PATH_MAX 512

typedef enum {
    UI_PREFERENCES_IO_OK = 0,
    UI_PREFERENCES_IO_MISSING,
    UI_PREFERENCES_IO_INVALID,
    UI_PREFERENCES_IO_FAILED
} UiPreferencesIoResult;

typedef enum {
    UI_PREFERENCES_CHANGE_UNCHANGED = 0,
    UI_PREFERENCES_CHANGE_SAVED,
    UI_PREFERENCES_CHANGE_ACTIVE_NOT_SAVED
} UiPreferencesChangeResult;

typedef struct {
    int default_scale_percent;
    int active_scale_percent;
    UiPreferencesIoResult default_load_result;
    UiPreferencesIoResult user_load_result;
    UiPreferencesIoResult last_save_result;
    char user_path[UI_PREFERENCES_PATH_MAX];
} UiPreferences;

bool ui_preferences_is_valid_scale(int scale_percent);
void ui_preferences_init(UiPreferences *preferences,
                         const char *default_path,
                         const char *user_path);
int ui_preferences_scale(const UiPreferences *preferences);
int ui_preferences_default_scale(const UiPreferences *preferences);
UiPreferencesChangeResult ui_preferences_increase(UiPreferences *preferences);
UiPreferencesChangeResult ui_preferences_decrease(UiPreferences *preferences);
UiPreferencesChangeResult ui_preferences_reset(UiPreferences *preferences);
UiPreferencesIoResult ui_preferences_save(const UiPreferences *preferences);

#endif