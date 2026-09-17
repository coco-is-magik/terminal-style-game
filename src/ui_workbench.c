#include "ui_workbench.h"

#include <stdio.h>
#include <stdlib.h>
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

static const char *context_container(MenuId context) {
    switch (context) {
        case MENU_MAIN: return "main_menu_container";
        case MENU_PAUSE: return "pause_menu_container";
        case MENU_SETTINGS: return "settings_container";
        case MENU_CONFIRM_QUIT: return "confirm_menu_container";
        default: return NULL;
    }
}

static void set_status(UiWorkbench *workbench, const char *status) {
    if (!workbench) return;
    (void)snprintf(workbench->status, sizeof(workbench->status), "%s",
                   status ? status : "");
}

static void copy_bounded(char *destination, size_t capacity, const char *source) {
    size_t length;
    if (!destination || capacity == 0U || !source) return;
    length = strlen(source);
    if (length >= capacity) length = capacity - 1U;
    memcpy(destination, source, length);
    destination[length] = '\0';
}

static bool property_matches_element(UiWorkbenchProperty property,
                                     const UiElement *element) {
    if (!element) return false;
    return element->type == UI_ELE_ANIMATION
        ? property >= UI_WORKBENCH_PROPERTY_PRESET
        : property <= UI_WORKBENCH_PROPERTY_FOCUS_EFFECT;
}

static void normalize_property(UiWorkbench *workbench) {
    UiElement *element = ui_workbench_current_element(workbench);
    if (!workbench || !element || property_matches_element(workbench->property, element)) return;
    workbench->property = element->type == UI_ELE_ANIMATION
        ? UI_WORKBENCH_PROPERTY_PRESET : UI_WORKBENCH_PROPERTY_STYLE;
}

void ui_workbench_init(UiWorkbench *workbench) {
    if (!workbench) return;
    memset(workbench, 0, sizeof(*workbench));
    workbench->context = MENU_NONE;
    workbench->property = UI_WORKBENCH_PROPERTY_STYLE;
    map_catalog_init(&workbench->catalog);
}

void ui_workbench_destroy(UiWorkbench *workbench) {
    if (!workbench) return;
    ui_layout_destroy(workbench->layout);
    ui_cache_destroy(&workbench->cache);
    map_catalog_clear(&workbench->catalog);
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
    for (int entry = 0; entry < workbench->cache.master_count; entry++)
        ui_cache_tick(&workbench->cache, workbench->cache.master_entries[entry].layout,
                      "assets/ui_elements");
    map_catalog_clear(&workbench->catalog);
    map_catalog_init(&workbench->catalog);
    if (map_catalog_refresh_extension(&workbench->catalog, "assets/ui_elements", ".txt")
        != MAP_CATALOG_OK) return UI_WORKBENCH_LOAD_FAILED;
    for (size_t catalog_index = 0U; catalog_index < workbench->catalog.count;
         catalog_index++) {
        const char *filename = workbench->catalog.entries[catalog_index].name;
        char stem[UI_ELE_NAME_MAX];
        size_t length = strlen(filename);
        if (length <= 4U || length - 4U >= sizeof(stem)) continue;
        memcpy(stem, filename, length - 4U);
        stem[length - 4U] = '\0';
        (void)ui_cache_load(&workbench->cache, stem, "assets/ui_elements");
    }
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
    workbench->mode = UI_WORKBENCH_MODE_BROWSE;
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
    normalize_property(workbench);
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
    UiElement *element;
    if (!workbench || direction == 0) return UI_WORKBENCH_INVALID_ARGUMENT;
    element = ui_workbench_current_element(workbench);
    do {
        value = (int)workbench->property +
            (direction > 0 ? 1 : UI_WORKBENCH_PROPERTY_COUNT - 1);
        workbench->property = (UiWorkbenchProperty)(value % UI_WORKBENCH_PROPERTY_COUNT);
    } while (element && element->type == UI_ELE_ANIMATION
        ? workbench->property <= UI_WORKBENCH_PROPERTY_FOCUS_EFFECT
        : workbench->property >= UI_WORKBENCH_PROPERTY_PRESET);
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
    static const char *const presets[] = {
        "pause_glitch", "center_out", "perimeter_burst", "local_glitch"
    };
    static const char *const triggers[] = {
        "context_enter", "context_exit", "focus", "activate", "while_visible"
    };
    static const char *const orientations[] = {"horizontal", "vertical", "radial"};
    UiElement *element = ui_workbench_current_element(workbench);
    const char *next;
    char old_value[UI_ELE_NAME_MAX];
    UiWorkbenchResult result;
    if (!workbench || !element || direction == 0 ||
        !property_matches_element(workbench->property, element))
        return UI_WORKBENCH_NO_CHANGE;
    if (element->type != UI_ELE_ANIMATION &&
        workbench->property == UI_WORKBENCH_PROPERTY_PARENT)
        return cycle_parent(workbench, element, direction);
    if (element->type == UI_ELE_ANIMATION) {
        if (workbench->property == UI_WORKBENCH_PROPERTY_TARGET) {
            int candidate_count = 0;
            int current = 0;
            UiElement *candidates[UI_LAYOUT_MAX_ELEMS];
            for (int i = 0; i < workbench->element_count; i++) {
                UiElement *candidate = workbench->elements[i];
                if (candidate && candidate->type != UI_ELE_ANIMATION)
                    candidates[candidate_count++] = candidate;
            }
            if (candidate_count == 0) return UI_WORKBENCH_NO_CHANGE;
            for (int i = 0; i < candidate_count; i++)
                if (strcmp(candidates[i]->name, element->target) == 0) current = i;
            current = (current + (direction > 0 ? 1 : candidate_count - 1)) % candidate_count;
            (void)snprintf(old_value, sizeof(old_value), "%s", element->target);
            (void)snprintf(element->target, sizeof(element->target), "%s",
                           candidates[current]->name);
        } else if (workbench->property == UI_WORKBENCH_PROPERTY_PRESET) {
            (void)snprintf(old_value, sizeof(old_value), "%s", element->preset);
            next = cycle_name(element->preset, presets,
                              sizeof(presets) / sizeof(presets[0]), direction);
            (void)snprintf(element->preset, sizeof(element->preset), "%s", next);
        } else if (workbench->property == UI_WORKBENCH_PROPERTY_TRIGGER) {
            (void)snprintf(old_value, sizeof(old_value), "%s", element->trigger);
            next = cycle_name(element->trigger, triggers,
                              sizeof(triggers) / sizeof(triggers[0]), direction);
            (void)snprintf(element->trigger, sizeof(element->trigger), "%s", next);
        } else if (workbench->property == UI_WORKBENCH_PROPERTY_ORIENTATION) {
            (void)snprintf(old_value, sizeof(old_value), "%s", element->orientation);
            next = cycle_name(element->orientation, orientations,
                              sizeof(orientations) / sizeof(orientations[0]), direction);
            (void)snprintf(element->orientation, sizeof(element->orientation), "%s", next);
        } else if (workbench->property == UI_WORKBENCH_PROPERTY_LOOP) {
            element->loop = !element->loop;
            old_value[0] = '\0';
        } else if (workbench->property == UI_WORKBENCH_PROPERTY_RANDOMIZE) {
            element->randomize = !element->randomize;
            old_value[0] = '\0';
        } else if (workbench->property == UI_WORKBENCH_PROPERTY_WIDTH) {
            element->layout.width += direction > 0 ? 1 : -1;
            old_value[0] = '\0';
        } else {
            element->layout.height += direction > 0 ? 1 : -1;
            old_value[0] = '\0';
        }
    } else if (workbench->property == UI_WORKBENCH_PROPERTY_STYLE) {
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
        if (element->type == UI_ELE_ANIMATION) {
            if (workbench->property == UI_WORKBENCH_PROPERTY_PRESET)
                copy_bounded(element->preset, sizeof(element->preset), old_value);
            else if (workbench->property == UI_WORKBENCH_PROPERTY_TARGET)
                copy_bounded(element->target, sizeof(element->target), old_value);
            else if (workbench->property == UI_WORKBENCH_PROPERTY_TRIGGER)
                copy_bounded(element->trigger, sizeof(element->trigger), old_value);
            else if (workbench->property == UI_WORKBENCH_PROPERTY_ORIENTATION)
                copy_bounded(element->orientation, sizeof(element->orientation), old_value);
            else if (workbench->property == UI_WORKBENCH_PROPERTY_LOOP) element->loop = !element->loop;
            else if (workbench->property == UI_WORKBENCH_PROPERTY_RANDOMIZE)
                element->randomize = !element->randomize;
            else if (workbench->property == UI_WORKBENCH_PROPERTY_WIDTH)
                element->layout.width -= direction > 0 ? 1 : -1;
            else element->layout.height -= direction > 0 ? 1 : -1;
        } else copy_bounded(destination, UI_ELE_PRESET_MAX, old_value);
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

UiWorkbenchResult ui_workbench_begin_add(UiWorkbench *workbench) {
    if (!workbench || !workbench->layout || workbench->catalog.count == 0U)
        return UI_WORKBENCH_NO_CHANGE;
    workbench->mode = UI_WORKBENCH_MODE_ADD;
    workbench->add_index = 0U;
    set_status(workbench, "Add existing unit: Up/Down choose, Enter add, Esc cancel");
    return UI_WORKBENCH_OK;
}

UiWorkbenchResult ui_workbench_cycle_add_source(UiWorkbench *workbench, int direction) {
    size_t count;
    if (!workbench || workbench->mode != UI_WORKBENCH_MODE_ADD || direction == 0)
        return UI_WORKBENCH_INVALID_ARGUMENT;
    count = workbench->catalog.count;
    if (count == 0U) return UI_WORKBENCH_NO_CHANGE;
    workbench->add_index = (workbench->add_index +
        (direction > 0 ? 1U : count - 1U)) % count;
    return UI_WORKBENCH_OK;
}

const char *ui_workbench_add_source_name(const UiWorkbench *workbench) {
    const MapCatalogEntry *entry;
    if (!workbench || workbench->mode != UI_WORKBENCH_MODE_ADD ||
        workbench->add_index >= workbench->catalog.count) return NULL;
    entry = map_catalog_get(&workbench->catalog, workbench->add_index);
    return entry ? entry->name : NULL;
}

static bool direct_layout_member(const UiWorkbench *workbench,
                                 const UiElement *element) {
    int i;
    if (!workbench || !workbench->layout || !element) return false;
    for (i = 0; i < workbench->layout->element_count; i++)
        if (workbench->layout->elements[i] == element) return true;
    return false;
}

UiWorkbenchResult ui_workbench_confirm_add(UiWorkbench *workbench) {
    const MapCatalogEntry *entry;
    const char *layout;
    const char *container;
    UiElement *source;
    UiElement clone;
    char stem[UI_ELE_NAME_MAX];
    char path[UI_ELE_PATH_MAX];
    char layout_path[UI_ELE_PATH_MAX];
    size_t length;
    int suffix;
    UiWorkbenchStoreResult result = UI_WORKBENCH_STORE_IO_ERROR;
    if (!workbench || workbench->mode != UI_WORKBENCH_MODE_ADD ||
        !(entry = map_catalog_get(&workbench->catalog, workbench->add_index)) ||
        !(layout = layout_name(workbench->context)) ||
        !(container = context_container(workbench->context)))
        return UI_WORKBENCH_INVALID_ARGUMENT;
    length = strlen(entry->name);
    if (length <= 4U || length - 4U >= sizeof(stem)) return UI_WORKBENCH_INVALID_ARGUMENT;
    memcpy(stem, entry->name, length - 4U);
    stem[length - 4U] = '\0';
    source = ui_cache_get(&workbench->cache, stem);
    if (!source) return UI_WORKBENCH_LOAD_FAILED;
    clone = *source;
    clone.parent = NULL;
    memset(clone.children, 0, sizeof(clone.children));
    clone.child_count = 0;
    if (clone.type == UI_ELE_ANIMATION) {
        clone.parent_name[0] = '\0';
        (void)snprintf(clone.target, sizeof(clone.target), "%s", container);
    } else if (clone.type != UI_ELE_CONTAINER) {
        (void)snprintf(clone.parent_name, sizeof(clone.parent_name), "%s", container);
        clone.layout.coords_mode = UI_COORD_RELATIVE;
    }
    for (suffix = 1; suffix < 1000; suffix++) {
        if (snprintf(clone.name, sizeof(clone.name), "%s_%s_%d",
                     layout, stem, suffix) >= (int)sizeof(clone.name) ||
            snprintf(path, sizeof(path), "assets/ui_elements/%s.txt", clone.name) >=
                (int)sizeof(path)) return UI_WORKBENCH_INVALID_ARGUMENT;
        result = ui_workbench_create_element(&clone, path);
        if (result == UI_WORKBENCH_STORE_OK ||
            result == UI_WORKBENCH_STORE_OK_DURABILITY_WARNING) break;
    }
    if (suffix >= 1000) return UI_WORKBENCH_SAVE_FAILED;
    if (snprintf(layout_path, sizeof(layout_path), "assets/ui_layouts/%s.txt", layout) >=
        (int)sizeof(layout_path) ||
        ui_workbench_store_membership(layout_path, "assets/ui_layouts/master_map.txt",
                                      layout, clone.name, true) != UI_WORKBENCH_STORE_OK) {
        (void)remove(path);
        set_status(workbench, "Add failed; source preserved");
        return UI_WORKBENCH_SAVE_FAILED;
    }
    if (ui_workbench_open(workbench, workbench->context) != UI_WORKBENCH_OK)
        return UI_WORKBENCH_LOAD_FAILED;
    for (int i = 0; i < workbench->element_count; i++)
        if (strcmp(workbench->elements[i]->name, clone.name) == 0)
            workbench->element_index = i;
    normalize_property(workbench);
    set_status(workbench, "Existing unit cloned and added");
    return UI_WORKBENCH_OK;
}

UiWorkbenchResult ui_workbench_request_remove(UiWorkbench *workbench) {
    UiElement *element = ui_workbench_current_element(workbench);
    if (!workbench || !element || !direct_layout_member(workbench, element))
        return UI_WORKBENCH_NO_CHANGE;
    for (int i = 0; i < workbench->element_count; i++) {
        UiElement *other = workbench->elements[i];
        if (other != element &&
            (strcmp(other->parent_name, element->name) == 0 ||
             (other->type == UI_ELE_ANIMATION && strcmp(other->target, element->name) == 0))) {
            set_status(workbench, "Remove rejected: active unit references selection");
            return UI_WORKBENCH_NO_CHANGE;
        }
    }
    workbench->mode = UI_WORKBENCH_MODE_REMOVE_CONFIRM;
    set_status(workbench, "Remove from this layout? Enter yes, Esc cancel");
    return UI_WORKBENCH_OK;
}

UiWorkbenchResult ui_workbench_confirm_remove(UiWorkbench *workbench) {
    UiElement *element = ui_workbench_current_element(workbench);
    const char *layout;
    char path[UI_ELE_PATH_MAX];
    if (!workbench || workbench->mode != UI_WORKBENCH_MODE_REMOVE_CONFIRM ||
        !element || !(layout = layout_name(workbench->context)) ||
        snprintf(path, sizeof(path), "assets/ui_layouts/%s.txt", layout) >=
            (int)sizeof(path)) return UI_WORKBENCH_INVALID_ARGUMENT;
    if (ui_workbench_store_membership(path, "assets/ui_layouts/master_map.txt",
                                      layout, element->name, false) != UI_WORKBENCH_STORE_OK) {
        set_status(workbench, "Remove failed; layout preserved");
        return UI_WORKBENCH_SAVE_FAILED;
    }
    if (ui_workbench_open(workbench, workbench->context) != UI_WORKBENCH_OK)
        return UI_WORKBENCH_LOAD_FAILED;
    set_status(workbench, "Removed from layout; source asset preserved");
    return UI_WORKBENCH_OK;
}

void ui_workbench_cancel_mode(UiWorkbench *workbench) {
    if (!workbench) return;
    workbench->mode = UI_WORKBENCH_MODE_BROWSE;
    set_status(workbench, "Cancelled");
}

const char *ui_workbench_property_name(UiWorkbenchProperty property) {
    static const char *const names[] = {
        "parent", "style", "transition", "focus effect", "preset", "target",
        "trigger", "orientation", "loop", "randomize"
        ,"width", "height"
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
        case UI_WORKBENCH_PROPERTY_PRESET: return element->preset;
        case UI_WORKBENCH_PROPERTY_TARGET: return element->target;
        case UI_WORKBENCH_PROPERTY_TRIGGER: return element->trigger;
        case UI_WORKBENCH_PROPERTY_ORIENTATION: return element->orientation;
        case UI_WORKBENCH_PROPERTY_LOOP: return element->loop ? "yes" : "no";
        case UI_WORKBENCH_PROPERTY_RANDOMIZE: return element->randomize ? "yes" : "no";
        case UI_WORKBENCH_PROPERTY_WIDTH: {
            static char width[24];
            (void)snprintf(width, sizeof(width), "%d", element->layout.width);
            return width;
        }
        case UI_WORKBENCH_PROPERTY_HEIGHT: {
            static char height[24];
            (void)snprintf(height, sizeof(height), "%d", element->layout.height);
            return height;
        }
        default: return "invalid";
    }
}