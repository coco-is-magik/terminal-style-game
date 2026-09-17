#include "ui_workbench.h"

#include <stdio.h>
#include <string.h>

static const char *layout_name(MenuId context) {
    switch (context) {
        case MENU_MAIN: return "main_menu";
        case MENU_PAUSE: return "pause_menu";
        case MENU_SETTINGS: return "settings";
        case MENU_CONFIRM_QUIT: return "confirm_quit";
        default: return NULL;
    }
}

static void set_status(UiWorkbench *workbench, const char *status) {
    if (!workbench) return;
    (void)snprintf(workbench->status, sizeof(workbench->status), "%s",
                   status ? status : "");
}

void ui_workbench_init(UiWorkbench *workbench) {
    if (!workbench) return;
    memset(workbench, 0, sizeof(*workbench));
    workbench->context = MENU_NONE;
    workbench->property = UI_WORKBENCH_PROPERTY_STYLE;
}

void ui_workbench_destroy(UiWorkbench *workbench) {
    if (!workbench) return;
    ui_layout_destroy(workbench->layout);
    ui_cache_destroy(&workbench->cache);
    ui_workbench_init(workbench);
}

UiWorkbenchResult ui_workbench_open(UiWorkbench *workbench, MenuId context) {
    const char *name;
    char path[UI_ELE_PATH_MAX];
    if (!workbench || !(name = layout_name(context))) return UI_WORKBENCH_INVALID_ARGUMENT;
    ui_layout_destroy(workbench->layout);
    workbench->layout = NULL;
    ui_cache_destroy(&workbench->cache);
    ui_cache_init(&workbench->cache, "assets/ui_layouts/master_map.txt");
    ui_cache_tick(&workbench->cache, name, "assets/ui_elements");
    if (snprintf(path, sizeof(path), "assets/ui_layouts/%s.txt", name) >=
        (int)sizeof(path)) return UI_WORKBENCH_LOAD_FAILED;
    workbench->layout = ui_layout_load(path, &workbench->cache);
    if (!workbench->layout || workbench->layout->element_count <= 0) {
        set_status(workbench, "Load failed");
        return UI_WORKBENCH_LOAD_FAILED;
    }
    workbench->element_count = 0;
    for (int i = 0; i < workbench->layout->element_count; i++) {
        UiElement *element = workbench->layout->elements[i];
        UiElement *parent = element ? element->parent : NULL;
        while (parent && workbench->element_count < UI_CACHE_MAX) {
            bool known = false;
            for (int j = 0; j < workbench->element_count; j++)
                if (workbench->elements[j] == parent) known = true;
            if (!known) workbench->elements[workbench->element_count++] = parent;
            parent = parent->parent;
        }
        if (element && workbench->element_count < UI_CACHE_MAX) {
            bool known = false;
            for (int j = 0; j < workbench->element_count; j++)
                if (workbench->elements[j] == element) known = true;
            if (!known) workbench->elements[workbench->element_count++] = element;
        }
    }
    workbench->context = context;
    workbench->element_index = 0;
    workbench->editing = false;
    set_status(workbench, "Loaded");
    return UI_WORKBENCH_OK;
}

UiElement *ui_workbench_current_element(UiWorkbench *workbench) {
    if (!workbench || !workbench->layout || workbench->element_index < 0 ||
        workbench->element_index >= workbench->element_count) return NULL;
    return workbench->elements[workbench->element_index];
}

UiWorkbenchResult ui_workbench_cycle_element(UiWorkbench *workbench, int direction) {
    int count;
    if (!workbench || !workbench->layout || direction == 0)
        return UI_WORKBENCH_INVALID_ARGUMENT;
    count = workbench->element_count;
    if (count <= 0) return UI_WORKBENCH_NO_CHANGE;
    workbench->element_index = (workbench->element_index +
                                (direction > 0 ? 1 : count - 1)) % count;
    workbench->editing = false;
    set_status(workbench, "Element changed");
    return UI_WORKBENCH_OK;
}

UiWorkbenchResult ui_workbench_toggle_editing(UiWorkbench *workbench) {
    if (!ui_workbench_current_element(workbench)) return UI_WORKBENCH_INVALID_ARGUMENT;
    workbench->editing = !workbench->editing;
    set_status(workbench, workbench->editing ? "Editing" : "Browsing");
    return UI_WORKBENCH_OK;
}

UiWorkbenchResult ui_workbench_cycle_property(UiWorkbench *workbench, int direction) {
    int value;
    if (!workbench || direction == 0) return UI_WORKBENCH_INVALID_ARGUMENT;
    value = (int)workbench->property + (direction > 0 ? 1 : UI_WORKBENCH_PROPERTY_COUNT - 1);
    workbench->property = (UiWorkbenchProperty)(value % UI_WORKBENCH_PROPERTY_COUNT);
    set_status(workbench, "Property changed");
    return UI_WORKBENCH_OK;
}

static bool is_descendant(const UiElement *candidate, const UiElement *element) {
    const UiElement *cursor = candidate;
    while (cursor) {
        if (cursor == element) return true;
        cursor = cursor->parent;
    }
    return false;
}

static UiWorkbenchResult save_and_reload(UiWorkbench *workbench,
                                         const UiElement *element) {
    char path[UI_ELE_PATH_MAX];
    char selected_name[UI_ELE_NAME_MAX];
    UiWorkbenchStoreResult result;
    int i;
    if (!workbench || !element || snprintf(path, sizeof(path),
            "assets/ui_elements/%s.txt", element->name) >= (int)sizeof(path))
        return UI_WORKBENCH_INVALID_ARGUMENT;
    (void)snprintf(selected_name, sizeof(selected_name), "%s", element->name);
    result = ui_workbench_store_element(element, path);
    if (result != UI_WORKBENCH_STORE_OK &&
        result != UI_WORKBENCH_STORE_OK_DURABILITY_WARNING) {
        set_status(workbench, "Save failed");
        return UI_WORKBENCH_SAVE_FAILED;
    }
    if (ui_workbench_open(workbench, workbench->context) != UI_WORKBENCH_OK)
        return UI_WORKBENCH_LOAD_FAILED;
    for (i = 0; i < workbench->element_count; i++) {
        UiElement *candidate = workbench->elements[i];
        if (candidate && strcmp(candidate->name, selected_name) == 0) {
            workbench->element_index = i;
            break;
        }
    }
    workbench->editing = true;
    set_status(workbench, result == UI_WORKBENCH_STORE_OK
        ? "Saved live" : "Saved; durability warning");
    return UI_WORKBENCH_OK;
}

UiWorkbenchResult ui_workbench_move(UiWorkbench *workbench, int dx, int dy) {
    UiElement *element = ui_workbench_current_element(workbench);
    UiWorkbenchResult result;
    int old_x;
    int old_y;
    if (!workbench || !element || !workbench->editing ||
        (dx == 0 && dy == 0)) return UI_WORKBENCH_NO_CHANGE;
    old_x = element->layout.x;
    old_y = element->layout.y;
    element->layout.x += dx;
    element->layout.y += dy;
    result = save_and_reload(workbench, element);
    if (result == UI_WORKBENCH_SAVE_FAILED ||
        result == UI_WORKBENCH_INVALID_ARGUMENT) {
        element->layout.x = old_x;
        element->layout.y = old_y;
    }
    return result;
}

static const char *cycle_name(const char *current, const char *const *values,
                              size_t count, int direction) {
    size_t i;
    for (i = 0U; i < count; i++) {
        if (strcmp(current, values[i]) == 0) {
            size_t offset = direction > 0 ? 1U : count - 1U;
            return values[(i + offset) % count];
        }
    }
    return values[0];
}

static UiWorkbenchResult cycle_parent(UiWorkbench *workbench, UiElement *element,
                                      int direction) {
    UiElement *candidates[UI_LAYOUT_MAX_ELEMS + 1];
    int count = 1;
    int current = 0;
    int i;
    char old_parent[UI_ELE_NAME_MAX];
    UiCoordMode old_mode;
    UiWorkbenchResult result;
    candidates[0] = NULL;
    for (i = 0; i < workbench->element_count; i++) {
        UiElement *candidate = workbench->elements[i];
        if (candidate && candidate->type == UI_ELE_CONTAINER && candidate != element &&
            !is_descendant(candidate, element)) candidates[count++] = candidate;
    }
    for (i = 1; i < count; i++)
        if (strcmp(candidates[i]->name, element->parent_name) == 0) current = i;
    current = (current + (direction > 0 ? 1 : count - 1)) % count;
    (void)snprintf(old_parent, sizeof(old_parent), "%s", element->parent_name);
    old_mode = element->layout.coords_mode;
    if (candidates[current]) {
        (void)snprintf(element->parent_name, sizeof(element->parent_name), "%s",
                       candidates[current]->name);
        element->layout.coords_mode = UI_COORD_RELATIVE;
    } else {
        element->parent_name[0] = '\0';
        element->layout.coords_mode = UI_COORD_ABSOLUTE;
    }
    result = save_and_reload(workbench, element);
    if (result == UI_WORKBENCH_SAVE_FAILED ||
        result == UI_WORKBENCH_INVALID_ARGUMENT) {
        (void)snprintf(element->parent_name, sizeof(element->parent_name), "%s",
                       old_parent);
        element->layout.coords_mode = old_mode;
    }
    return result;
}

UiWorkbenchResult ui_workbench_cycle_value(UiWorkbench *workbench, int direction) {
    static const char *const button_styles[] = {"plain", "bracket", "inverse"};
    static const char *const container_styles[] = {"plain", "frame"};
    static const char *const text_styles[] = {"plain", "bright"};
    static const char *const transitions[] = {
        "none", "center_out", "perimeter_burst", "local_glitch"
    };
    static const char *const effects[] = {
        "none", "focus_pulse", "focus_glitch", "input_hold_short"
    };
    UiElement *element = ui_workbench_current_element(workbench);
    const char *next;
    char old_value[UI_ELE_PRESET_MAX];
    UiWorkbenchResult result;
    if (!workbench || !element || !workbench->editing || direction == 0)
        return UI_WORKBENCH_NO_CHANGE;
    if (workbench->property == UI_WORKBENCH_PROPERTY_PARENT)
        return cycle_parent(workbench, element, direction);
    if (workbench->property == UI_WORKBENCH_PROPERTY_STYLE) {
        const char *const *values = text_styles;
        size_t count = sizeof(text_styles) / sizeof(text_styles[0]);
        if (element->type == UI_ELE_BUTTON) {
            values = button_styles;
            count = sizeof(button_styles) / sizeof(button_styles[0]);
        } else if (element->type == UI_ELE_CONTAINER) {
            values = container_styles;
            count = sizeof(container_styles) / sizeof(container_styles[0]);
        }
        (void)snprintf(old_value, sizeof(old_value), "%s", element->style);
        next = cycle_name(element->style, values, count, direction);
        (void)snprintf(element->style, sizeof(element->style), "%s", next);
    } else if (workbench->property == UI_WORKBENCH_PROPERTY_TRANSITION) {
        (void)snprintf(old_value, sizeof(old_value), "%s", element->transition);
        next = cycle_name(element->transition, transitions,
                          sizeof(transitions) / sizeof(transitions[0]), direction);
        (void)snprintf(element->transition, sizeof(element->transition), "%s", next);
    } else {
        (void)snprintf(old_value, sizeof(old_value), "%s", element->focus_effect);
        next = cycle_name(element->focus_effect, effects,
                          sizeof(effects) / sizeof(effects[0]), direction);
        (void)snprintf(element->focus_effect, sizeof(element->focus_effect), "%s", next);
    }
    result = save_and_reload(workbench, element);
    if (result == UI_WORKBENCH_SAVE_FAILED ||
        result == UI_WORKBENCH_INVALID_ARGUMENT) {
        char *destination = workbench->property == UI_WORKBENCH_PROPERTY_STYLE
            ? element->style
            : workbench->property == UI_WORKBENCH_PROPERTY_TRANSITION
                ? element->transition : element->focus_effect;
        (void)snprintf(destination, UI_ELE_PRESET_MAX, "%s", old_value);
    }
    return result;
}

UiWorkbenchResult ui_workbench_invoke(UiWorkbench *workbench) {
    UiElement *element = ui_workbench_current_element(workbench);
    const char *action;
    if (!workbench || !element || element->type != UI_ELE_BUTTON ||
        !(action = ui_ele_get_action(element))) return UI_WORKBENCH_UNAVAILABLE_ACTION;
    if (strcmp(action, "open_settings") == 0)
        return ui_workbench_open(workbench, MENU_SETTINGS);
    if (strcmp(action, "quit") == 0)
        return ui_workbench_open(workbench, MENU_CONFIRM_QUIT);
    if (strcmp(action, "cancel") == 0 || strcmp(action, "back") == 0)
        return ui_workbench_open(workbench, MENU_MAIN);
    set_status(workbench, "Action unavailable in workbench");
    return UI_WORKBENCH_UNAVAILABLE_ACTION;
}

const char *ui_workbench_property_name(UiWorkbenchProperty property) {
    static const char *const names[] = {
        "parent", "style", "transition", "focus effect"
    };
    return property >= UI_WORKBENCH_PROPERTY_PARENT &&
        property < UI_WORKBENCH_PROPERTY_COUNT ? names[property] : "invalid";
}

const char *ui_workbench_current_value(const UiWorkbench *workbench) {
    UiElement *element;
    if (!workbench) return "invalid";
    element = ui_workbench_current_element((UiWorkbench *)workbench);
    if (!element) return "none";
    switch (workbench->property) {
        case UI_WORKBENCH_PROPERTY_PARENT:
            return element->parent_name[0] != '\0' ? element->parent_name : "screen root";
        case UI_WORKBENCH_PROPERTY_STYLE: return element->style;
        case UI_WORKBENCH_PROPERTY_TRANSITION: return element->transition;
        case UI_WORKBENCH_PROPERTY_FOCUS_EFFECT: return element->focus_effect;
        default: return "invalid";
    }
}