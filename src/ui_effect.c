/** ui_effect.c — Registry table, binding validation, and effect definitions. */
#include "ui_effect.h"

#include "number_parse.h"

#include <stdio.h>
#include <string.h>

/* Reusable effect definitions live here; a binding points at one by name. */
#define UI_EFFECT_DIR "assets/ui_effects"

/*
 * The one place that knows every effect primitive. Adding a primitive means
 * adding a row here (with its constraints) and a matching render function in
 * ui_animation.c; loading, inspection, and rendering all read this table.
 */
static const UiEffectSpec effect_specs[] = {
    { UI_EFFECT_PAUSE_GLITCH, "pause_glitch", UI_ELE_ANIMATION,
      UI_EFFECT_TRIGGER_ANY, UI_EFFECT_ORIENT_ANY, UI_EFFECT_ANY_BOOL,
      UI_EFFECT_ANY_BOOL, false },
    { UI_EFFECT_CENTER_OUT, "center_out", UI_ELE_ANIMATION,
      UI_EFFECT_TRIGGER_ANY, UI_EFFECT_ORIENT_ANY, UI_EFFECT_ANY_BOOL,
      UI_EFFECT_ANY_BOOL, false },
    { UI_EFFECT_PERIMETER_BURST, "perimeter_burst", UI_ELE_ANIMATION,
      UI_EFFECT_TRIGGER_ANY, UI_EFFECT_ORIENT_ANY, UI_EFFECT_ANY_BOOL,
      UI_EFFECT_ANY_BOOL, false },
    { UI_EFFECT_LOCAL_GLITCH, "local_glitch", UI_ELE_ANIMATION,
      UI_EFFECT_TRIGGER_ANY, UI_EFFECT_ORIENT_ANY, UI_EFFECT_ANY_BOOL,
      UI_EFFECT_ANY_BOOL, false },
    { UI_EFFECT_EDGE_TRACE, "edge_trace", UI_ELE_BUTTON,
      UI_EFFECT_TRIGGER_FOCUS, UI_EFFECT_ORIENT_HORIZONTAL, 0, 0, false },
    { UI_EFFECT_CHROMATIC_REGISTER, "chromatic_register", UI_ELE_BUTTON,
      UI_EFFECT_TRIGGER_FOCUS, UI_EFFECT_ORIENT_HORIZONTAL, 0, 0, false },
    { UI_EFFECT_COMMAND_FLASH, "command_flash", UI_ELE_BUTTON,
      UI_EFFECT_TRIGGER_ACTIVATE, UI_EFFECT_ORIENT_HORIZONTAL, 0, 0, false },
    { UI_EFFECT_BUTTON_REASSEMBLE, "button_reassemble", UI_ELE_BUTTON,
      UI_EFFECT_TRIGGER_CONTEXT_ENTER | UI_EFFECT_TRIGGER_CONTEXT_EXIT,
      UI_EFFECT_ORIENT_RADIAL, 0, 0, false },
    { UI_EFFECT_PANEL_REGISTER, "panel_register", UI_ELE_CONTAINER,
      UI_EFFECT_TRIGGER_CONTEXT_ENTER | UI_EFFECT_TRIGGER_CONTEXT_EXIT,
      UI_EFFECT_ORIENT_RADIAL, 0, 0, false },
    { UI_EFFECT_LIVING_FIELD, "living_field", UI_ELE_ANIMATION,
      UI_EFFECT_TRIGGER_WHILE_VISIBLE, UI_EFFECT_ORIENT_ANY, 0, 0, true }
};

static unsigned int trigger_bit(const char *trigger) {
    if (!trigger) return 0U;
    if (strcmp(trigger, "context_enter") == 0) return UI_EFFECT_TRIGGER_CONTEXT_ENTER;
    if (strcmp(trigger, "context_exit") == 0) return UI_EFFECT_TRIGGER_CONTEXT_EXIT;
    if (strcmp(trigger, "focus") == 0) return UI_EFFECT_TRIGGER_FOCUS;
    if (strcmp(trigger, "activate") == 0) return UI_EFFECT_TRIGGER_ACTIVATE;
    if (strcmp(trigger, "while_visible") == 0) return UI_EFFECT_TRIGGER_WHILE_VISIBLE;
    return 0U;
}

static unsigned int orientation_bit(const char *orientation) {
    if (!orientation) return 0U;
    if (strcmp(orientation, "horizontal") == 0) return UI_EFFECT_ORIENT_HORIZONTAL;
    if (strcmp(orientation, "vertical") == 0) return UI_EFFECT_ORIENT_VERTICAL;
    if (strcmp(orientation, "radial") == 0) return UI_EFFECT_ORIENT_RADIAL;
    return 0U;
}

int ui_effect_count(void) {
    return (int)(sizeof(effect_specs) / sizeof(effect_specs[0]));
}

const UiEffectSpec *ui_effect_spec_at(int index) {
    if (index < 0 || index >= ui_effect_count()) return NULL;
    return &effect_specs[index];
}

const UiEffectSpec *ui_effect_spec(UiEffectId id) {
    for (int i = 0; i < ui_effect_count(); i++)
        if (effect_specs[i].id == id) return &effect_specs[i];
    return NULL;
}

const UiEffectSpec *ui_effect_spec_by_name(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < ui_effect_count(); i++)
        if (strcmp(effect_specs[i].name, name) == 0) return &effect_specs[i];
    return NULL;
}

bool ui_effect_name_is_valid(const char *name) {
    return ui_effect_spec_by_name(name) != NULL;
}

bool ui_effect_binding_is_valid(const UiEffectSpec *spec, const UiElement *element) {
    unsigned int trigger;
    unsigned int orientation;
    if (!spec || !element) return false;
    if (element->type != UI_ELE_ANIMATION) return true;
    trigger = trigger_bit(element->trigger);
    orientation = orientation_bit(element->orientation);
    if (trigger == 0U || (spec->trigger_mask & trigger) == 0U) return false;
    if (orientation == 0U || (spec->orientation_mask & orientation) == 0U)
        return false;
    if (spec->loop != UI_EFFECT_ANY_BOOL && element->loop != spec->loop) return false;
    if (spec->randomize != UI_EFFECT_ANY_BOOL &&
        element->randomize != spec->randomize) return false;
    return element->target[0] != '\0' && element->loop >= 0 && element->loop <= 1 &&
           element->randomize >= 0 && element->randomize <= 1;
}

static char *effect_trim(char *text) {
    char *end;
    while (*text == ' ' || *text == '\t') text++;
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' ||
                          end[-1] == '\r'))
        *--end = '\0';
    return text;
}

bool ui_effect_definition_load(const char *name, UiEffectDefinition *out) {
    char path[UI_ELE_PATH_MAX];
    char line[512];
    FILE *f;
    bool valid = true;
    if (!name || name[0] == '\0' || !out) return false;
    if (snprintf(path, sizeof(path), "%s/%s.txt", UI_EFFECT_DIR, name) >=
        (int)sizeof(path))
        return false;
    f = fopen(path, "r");
    if (!f) return false;
    memset(out, 0, sizeof(*out));
    out->loop = -1;
    out->randomize = -1;
    (void)snprintf(out->name, sizeof(out->name), "%s", name);
    while (fgets(line, sizeof(line), f)) {
        char *eq;
        char *key;
        char *val;
        char *start = effect_trim(line);
        if (start[0] == '\0' || start[0] == '#') continue;
        eq = strchr(start, '=');
        if (!eq) continue;
        *eq = '\0';
        key = effect_trim(start);
        val = effect_trim(eq + 1);
        if (strcmp(key, "name") == 0) {
            (void)snprintf(out->name, sizeof(out->name), "%s", val);
        } else if (strcmp(key, "primitive") == 0) {
            if (strlen(val) >= sizeof(out->primitive)) valid = false;
            else (void)snprintf(out->primitive, sizeof(out->primitive), "%s", val);
        } else if (strcmp(key, "trigger") == 0) {
            if (strlen(val) >= sizeof(out->trigger)) valid = false;
            else (void)snprintf(out->trigger, sizeof(out->trigger), "%s", val);
        } else if (strcmp(key, "orientation") == 0) {
            if (strlen(val) >= sizeof(out->orientation)) valid = false;
            else (void)snprintf(out->orientation, sizeof(out->orientation), "%s", val);
        } else if (strcmp(key, "loop") == 0) {
            valid = number_parse_int(val, 0, 1, &out->loop);
        } else if (strcmp(key, "randomize") == 0) {
            valid = number_parse_int(val, 0, 1, &out->randomize);
        } else {
            valid = false; /* unknown key: fail loudly rather than ignore */
        }
        if (!valid) break;
    }
    if (ferror(f) != 0) valid = false;
    if (fclose(f) != 0) valid = false;
    return valid && ui_effect_name_is_valid(out->primitive);
}
