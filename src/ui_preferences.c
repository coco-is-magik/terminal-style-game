#define _POSIX_C_SOURCE 200809L

#include "ui_preferences.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define UI_PREFERENCES_LINE_MAX 128

static const int ui_scale_presets[] = {100, 125, 150, 200};

bool ui_preferences_is_valid_scale(int scale_percent) {
    size_t i;
    for (i = 0; i < sizeof(ui_scale_presets) / sizeof(ui_scale_presets[0]); i++) {
        if (ui_scale_presets[i] == scale_percent) return true;
    }
    return false;
}

static char *trim(char *text) {
    char *end;
    while (*text != '\0' && isspace((unsigned char)*text)) text++;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) end--;
    *end = '\0';
    return text;
}

static bool parse_int(const char *text, int *value) {
    char *end;
    long parsed;
    if (!text || !value || *text == '\0') return false;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || *end != '\0' || parsed < 0 || parsed > 10000) return false;
    *value = (int)parsed;
    return true;
}

static UiPreferencesIoResult load_file(const char *path, int *scale_percent) {
    FILE *file;
    char line[UI_PREFERENCES_LINE_MAX];
    bool have_version = false;
    bool have_scale = false;
    int version = 0;
    int scale = 0;

    if (!path || !scale_percent || path[0] == '\0') return UI_PREFERENCES_IO_INVALID;
    file = fopen(path, "rb");
    if (!file) {
        return errno == ENOENT ? UI_PREFERENCES_IO_MISSING : UI_PREFERENCES_IO_FAILED;
    }

    while (fgets(line, sizeof(line), file)) {
        char *key;
        char *value;
        char *equals;
        size_t length = strlen(line);
        if (length > 0 && line[length - 1] != '\n' && !feof(file)) {
            fclose(file);
            return UI_PREFERENCES_IO_INVALID;
        }
        key = trim(line);
        if (*key == '\0' || *key == '#') continue;
        equals = strchr(key, '=');
        if (!equals) {
            fclose(file);
            return UI_PREFERENCES_IO_INVALID;
        }
        *equals = '\0';
        value = trim(equals + 1);
        key = trim(key);
        if (strcmp(key, "version") == 0) {
            if (have_version || !parse_int(value, &version)) {
                fclose(file);
                return UI_PREFERENCES_IO_INVALID;
            }
            have_version = true;
        } else if (strcmp(key, "ui_scale_percent") == 0) {
            if (have_scale || !parse_int(value, &scale)) {
                fclose(file);
                return UI_PREFERENCES_IO_INVALID;
            }
            have_scale = true;
        } else {
            fclose(file);
            return UI_PREFERENCES_IO_INVALID;
        }
    }
    if (ferror(file) || fclose(file) != 0) return UI_PREFERENCES_IO_FAILED;
    if (!have_version || !have_scale || version != UI_PREFERENCES_VERSION ||
        !ui_preferences_is_valid_scale(scale)) {
        return UI_PREFERENCES_IO_INVALID;
    }
    *scale_percent = scale;
    return UI_PREFERENCES_IO_OK;
}

void ui_preferences_init(UiPreferences *preferences,
                         const char *default_path,
                         const char *user_path) {
    int loaded_scale;
    if (!preferences) return;
    memset(preferences, 0, sizeof(*preferences));
    preferences->default_scale_percent = UI_PREFERENCES_EMERGENCY_SCALE;
    preferences->active_scale_percent = UI_PREFERENCES_EMERGENCY_SCALE;
    preferences->last_save_result = UI_PREFERENCES_IO_OK;
    if (user_path && strlen(user_path) < sizeof(preferences->user_path)) {
        memcpy(preferences->user_path, user_path, strlen(user_path) + 1);
    }

    preferences->default_load_result = load_file(default_path, &loaded_scale);
    if (preferences->default_load_result == UI_PREFERENCES_IO_OK) {
        preferences->default_scale_percent = loaded_scale;
        preferences->active_scale_percent = loaded_scale;
    }
    preferences->user_load_result = load_file(user_path, &loaded_scale);
    if (preferences->user_load_result == UI_PREFERENCES_IO_OK) {
        preferences->active_scale_percent = loaded_scale;
    }
}

int ui_preferences_scale(const UiPreferences *preferences) {
    return preferences ? preferences->active_scale_percent : UI_PREFERENCES_EMERGENCY_SCALE;
}

int ui_preferences_default_scale(const UiPreferences *preferences) {
    return preferences ? preferences->default_scale_percent : UI_PREFERENCES_EMERGENCY_SCALE;
}

UiPreferencesIoResult ui_preferences_save(const UiPreferences *preferences) {
    char temporary_path[UI_PREFERENCES_PATH_MAX + 48];
    FILE *file;
    int fd;
    bool failed = false;
    if (!preferences || preferences->user_path[0] == '\0' ||
        !ui_preferences_is_valid_scale(preferences->active_scale_percent)) {
        return UI_PREFERENCES_IO_INVALID;
    }
    if (snprintf(temporary_path, sizeof(temporary_path), "%s.tmp.%ld",
                 preferences->user_path, (long)getpid()) >= (int)sizeof(temporary_path)) {
        return UI_PREFERENCES_IO_FAILED;
    }
    file = fopen(temporary_path, "wb");
    if (!file) return UI_PREFERENCES_IO_FAILED;
    if (fprintf(file, "version = %d\nui_scale_percent = %d\n",
                UI_PREFERENCES_VERSION, preferences->active_scale_percent) < 0) failed = true;
    if (!failed && fflush(file) != 0) failed = true;
    fd = fileno(file);
    if (!failed && (fd < 0 || fsync(fd) != 0)) failed = true;
    if (fclose(file) != 0) failed = true;
    if (failed) {
        remove(temporary_path);
        return UI_PREFERENCES_IO_FAILED;
    }
    if (rename(temporary_path, preferences->user_path) != 0) {
        remove(temporary_path);
        return UI_PREFERENCES_IO_FAILED;
    }
    return UI_PREFERENCES_IO_OK;
}

static UiPreferencesChangeResult activate_and_save(UiPreferences *preferences,
                                                    int scale_percent) {
    if (!preferences || preferences->active_scale_percent == scale_percent) {
        return UI_PREFERENCES_CHANGE_UNCHANGED;
    }
    preferences->active_scale_percent = scale_percent;
    preferences->last_save_result = ui_preferences_save(preferences);
    return preferences->last_save_result == UI_PREFERENCES_IO_OK
        ? UI_PREFERENCES_CHANGE_SAVED
        : UI_PREFERENCES_CHANGE_ACTIVE_NOT_SAVED;
}

UiPreferencesChangeResult ui_preferences_increase(UiPreferences *preferences) {
    size_t i;
    if (!preferences) return UI_PREFERENCES_CHANGE_UNCHANGED;
    for (i = 0; i + 1 < sizeof(ui_scale_presets) / sizeof(ui_scale_presets[0]); i++) {
        if (ui_scale_presets[i] == preferences->active_scale_percent) {
            return activate_and_save(preferences, ui_scale_presets[i + 1]);
        }
    }
    return UI_PREFERENCES_CHANGE_UNCHANGED;
}

UiPreferencesChangeResult ui_preferences_decrease(UiPreferences *preferences) {
    size_t i;
    if (!preferences) return UI_PREFERENCES_CHANGE_UNCHANGED;
    for (i = 1; i < sizeof(ui_scale_presets) / sizeof(ui_scale_presets[0]); i++) {
        if (ui_scale_presets[i] == preferences->active_scale_percent) {
            return activate_and_save(preferences, ui_scale_presets[i - 1]);
        }
    }
    return UI_PREFERENCES_CHANGE_UNCHANGED;
}

UiPreferencesChangeResult ui_preferences_reset(UiPreferences *preferences) {
    if (!preferences) return UI_PREFERENCES_CHANGE_UNCHANGED;
    return activate_and_save(preferences, preferences->default_scale_percent);
}