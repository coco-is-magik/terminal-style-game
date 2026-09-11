#define _POSIX_C_SOURCE 200809L

#include "ui_menu_workspace.h"
#include "ui_layout_resolver.h"
#include "ui_nested_inspector.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void select_id(UiMenuWorkspace *workspace, UiElementId id);
static UiMenuWorkspaceResult record_change(UiMenuWorkspace *workspace,
                                           const UiDocument *before,
                                           const UiDocument *after,
                                           UiElementId before_selected_id,
                                           UiElementId after_selected_id);

static void clear_history(UiMenuWorkspace *workspace) {
    size_t i;
    for (i = 0U; i < workspace->change_count; i++) {
        free(workspace->changes[i].before);
        free(workspace->changes[i].after);
        workspace->changes[i].before = NULL;
        workspace->changes[i].after = NULL;
    }
    workspace->change_count = 0U;
    workspace->change_cursor = 0U;
}

static void reset_document(UiMenuWorkspace *workspace) {
    free(workspace->pointer_before);
    workspace->pointer_before = NULL;
    workspace->pointer_mode = UI_MENU_POINTER_NONE;
    ui_document_init(&workspace->document);
    workspace->saved_document = workspace->document;
    clear_history(workspace);
    workspace->element_index = 0U;
    workspace->property = UI_MENU_PROPERTY_X;
    memset(&workspace->nested_cursor, 0, sizeof(workspace->nested_cursor));
    workspace->preview_resolution = UI_MENU_PREVIEW_RESOLUTION_80X25;
    workspace->preview_scale = UI_MENU_PREVIEW_SCALE_100;
    workspace->preview_field = UI_MENU_PREVIEW_FIELD_RESOLUTION;
    workspace->has_document = false;
}

void ui_menu_workspace_init(UiMenuWorkspace *workspace) {
    if (!workspace) return;
    memset(workspace, 0, sizeof(*workspace));
    map_catalog_init(&workspace->catalog);
    reset_document(workspace);
}

void ui_menu_workspace_clear(UiMenuWorkspace *workspace) {
    if (!workspace) return;
    free(workspace->pointer_before);
    workspace->pointer_before = NULL;
    clear_history(workspace);
    map_catalog_clear(&workspace->catalog);
    ui_menu_workspace_init(workspace);
}

static const UiResolvedElement *find_resolved_element(
    const UiResolvedElement *resolved, size_t count, UiElementId id
) {
    size_t i;
    for (i = 0U; i < count; i++)
        if (resolved[i].element_id == id) return &resolved[i];
    return NULL;
}

static bool point_in_rect(UiResolvedRect rect, int x, int y) {
    int64_t right = (int64_t)rect.x + rect.width;
    int64_t bottom = (int64_t)rect.y + rect.height;
    return x >= rect.x && y >= rect.y && (int64_t)x < right && (int64_t)y < bottom;
}

UiMenuWorkspaceResult ui_menu_workspace_pointer_press(
    UiMenuWorkspace *workspace, int pointer_x, int pointer_y,
    int viewport_width, int viewport_height
) {
    UiResolvedElement resolved[UI_DOCUMENT_MAX_ELEMENTS];
    size_t count = 0U;
    size_t i;
    UiElementId hit_id = 0U;
    const UiResolvedElement *selected_resolved;
    const UiDocumentElement *selected;
    if (!workspace || viewport_width <= 0 || viewport_height <= 0)
        return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return UI_MENU_WORKSPACE_INACTIVE;
    if (!workspace->has_document ||
        (workspace->mode != UI_MENU_WORKSPACE_HIERARCHY &&
         workspace->mode != UI_MENU_WORKSPACE_PROPERTIES) || workspace->pointer_before)
        return UI_MENU_WORKSPACE_NO_ACTION;
    if (ui_layout_resolve(&workspace->document, viewport_width, viewport_height,
                          resolved, &count) != UI_LAYOUT_RESOLVE_OK)
        return UI_MENU_WORKSPACE_MUTATION_FAILED;
    selected = ui_menu_workspace_selected_element(workspace);
    selected_resolved = selected
        ? find_resolved_element(resolved, count, selected->id) : NULL;
    for (i = 0U; i < count; i++) {
        const UiDocumentElement *element = ui_document_find_element(
            &workspace->document, resolved[i].element_id);
        if (element && element->id != 1U && element->visual.visible_by_default &&
            point_in_rect(resolved[i].clip, pointer_x, pointer_y) &&
            (hit_id == 0U || resolved[i].paint_order >
             find_resolved_element(resolved, count, hit_id)->paint_order))
            hit_id = element->id;
    }
    if (hit_id == 0U) return UI_MENU_WORKSPACE_NO_ACTION;
    workspace->pointer_before = malloc(sizeof(*workspace->pointer_before));
    if (!workspace->pointer_before) return UI_MENU_WORKSPACE_MUTATION_FAILED;
    *workspace->pointer_before = workspace->document;
    workspace->pointer_element_id = hit_id;
    workspace->pointer_start_x = pointer_x;
    workspace->pointer_start_y = pointer_y;
    workspace->pointer_mode = selected && selected->id == hit_id && selected_resolved &&
        selected_resolved->rect.width > 0 && selected_resolved->rect.height > 0 &&
        pointer_x == selected_resolved->rect.x + selected_resolved->rect.width - 1 &&
        pointer_y == selected_resolved->rect.y + selected_resolved->rect.height - 1
        ? UI_MENU_POINTER_RESIZE : UI_MENU_POINTER_MOVE;
    select_id(workspace, hit_id);
    workspace->mode = UI_MENU_WORKSPACE_HIERARCHY;
    return UI_MENU_WORKSPACE_OK;
}

UiMenuWorkspaceResult ui_menu_workspace_pointer_motion(
    UiMenuWorkspace *workspace, int pointer_x, int pointer_y
) {
    UiDocument candidate;
    const UiDocumentElement *element;
    UiDocumentLayout layout;
    int64_t delta_x;
    int64_t delta_y;
    size_t i;
    if (!workspace) return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return UI_MENU_WORKSPACE_INACTIVE;
    if (!workspace->pointer_before || workspace->pointer_mode == UI_MENU_POINTER_NONE)
        return UI_MENU_WORKSPACE_NO_ACTION;
    element = ui_document_find_element(
        workspace->pointer_before, workspace->pointer_element_id);
    if (!element || element->id == 1U) return UI_MENU_WORKSPACE_MUTATION_FAILED;
    delta_x = (int64_t)pointer_x - workspace->pointer_start_x;
    delta_y = (int64_t)pointer_y - workspace->pointer_start_y;
    layout = element->layout;
    if (workspace->pointer_mode == UI_MENU_POINTER_MOVE) {
        if ((int64_t)layout.x + delta_x < INT_MIN ||
            (int64_t)layout.x + delta_x > INT_MAX ||
            (int64_t)layout.y + delta_y < INT_MIN ||
            (int64_t)layout.y + delta_y > INT_MAX)
            return UI_MENU_WORKSPACE_MUTATION_FAILED;
        layout.x = (int)((int64_t)layout.x + delta_x);
        layout.y = (int)((int64_t)layout.y + delta_y);
    } else {
        int minimum_width = layout.horizontal_anchor == UI_DOCUMENT_ANCHOR_STRETCH ? 0 : 1;
        int minimum_height = layout.vertical_anchor == UI_DOCUMENT_ANCHOR_STRETCH ? 0 : 1;
        if ((int64_t)layout.width + delta_x < minimum_width ||
            (int64_t)layout.width + delta_x > INT_MAX ||
            (int64_t)layout.height + delta_y < minimum_height ||
            (int64_t)layout.height + delta_y > INT_MAX)
            return UI_MENU_WORKSPACE_MUTATION_FAILED;
        layout.width = (int)((int64_t)layout.width + delta_x);
        layout.height = (int)((int64_t)layout.height + delta_y);
    }
    candidate = *workspace->pointer_before;
    if (ui_document_set_layout(&candidate, element->id, layout) != UI_DOCUMENT_OK)
        return UI_MENU_WORKSPACE_MUTATION_FAILED;
    workspace->document = candidate;
    for (i = 0U; i < workspace->document.element_count; i++)
        if (workspace->document.elements[i].id == workspace->pointer_element_id) {
            workspace->element_index = i;
            break;
        }
    return UI_MENU_WORKSPACE_OK;
}

UiMenuWorkspaceResult ui_menu_workspace_pointer_release(UiMenuWorkspace *workspace) {
    UiDocument before;
    UiElementId selected_id;
    UiMenuWorkspaceResult result;
    if (!workspace) return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return UI_MENU_WORKSPACE_INACTIVE;
    if (!workspace->pointer_before) return UI_MENU_WORKSPACE_NO_ACTION;
    before = *workspace->pointer_before;
    selected_id = workspace->pointer_element_id;
    free(workspace->pointer_before);
    workspace->pointer_before = NULL;
    workspace->pointer_mode = UI_MENU_POINTER_NONE;
    if (workspace->document.state.current_state == before.state.current_state)
        return UI_MENU_WORKSPACE_OK;
    result = record_change(workspace, &before, &workspace->document,
                           selected_id, selected_id);
    if (result == UI_MENU_WORKSPACE_OK) select_id(workspace, selected_id);
    else workspace->document = before;
    return result;
}

UiMenuWorkspaceResult ui_menu_workspace_pointer_cancel(UiMenuWorkspace *workspace) {
    UiElementId selected_id;
    if (!workspace) return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return UI_MENU_WORKSPACE_INACTIVE;
    if (!workspace->pointer_before) return UI_MENU_WORKSPACE_NO_ACTION;
    selected_id = workspace->pointer_element_id;
    workspace->document = *workspace->pointer_before;
    free(workspace->pointer_before);
    workspace->pointer_before = NULL;
    workspace->pointer_mode = UI_MENU_POINTER_NONE;
    select_id(workspace, selected_id);
    return UI_MENU_WORKSPACE_OK;
}

static void select_id(UiMenuWorkspace *workspace, UiElementId id) {
    size_t i;
    workspace->element_index = 0U;
    for (i = 0U; i < workspace->document.element_count; i++)
        if (workspace->document.elements[i].id == id) {
            workspace->element_index = i;
            return;
        }
}

static UiMenuWorkspaceResult refresh_catalog(UiMenuWorkspace *workspace) {
    char path[UI_DOCUMENT_PATH_CAPACITY];
    struct stat metadata;
    int written = snprintf(path, sizeof(path), "%s/menus", workspace->asset_root);
    if (written < 0 || (size_t)written >= sizeof(path))
        return UI_MENU_WORKSPACE_CATALOG_FAILED;
    if (stat(path, &metadata) != 0) {
        if (errno != ENOENT) return UI_MENU_WORKSPACE_CATALOG_FAILED;
        map_catalog_clear(&workspace->catalog);
        return UI_MENU_WORKSPACE_OK;
    }
    if (!S_ISDIR(metadata.st_mode) || map_catalog_refresh_extension(
            &workspace->catalog, path, ".tui") != MAP_CATALOG_OK)
        return UI_MENU_WORKSPACE_CATALOG_FAILED;
    return UI_MENU_WORKSPACE_OK;
}

UiMenuWorkspaceResult ui_menu_workspace_open(UiMenuWorkspace *workspace,
                                             const char *asset_root) {
    UiMenuWorkspace candidate;
    UiMenuWorkspaceResult result;
    if (!workspace || !asset_root || asset_root[0] == '\0' ||
        strlen(asset_root) >= sizeof(workspace->asset_root))
        return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    ui_menu_workspace_init(&candidate);
    memcpy(candidate.asset_root, asset_root, strlen(asset_root) + 1U);
    result = refresh_catalog(&candidate);
    if (result != UI_MENU_WORKSPACE_OK) return result;
    candidate.active = true;
    candidate.mode = UI_MENU_WORKSPACE_CHOOSER;
    clear_history(workspace);
    map_catalog_clear(&workspace->catalog);
    *workspace = candidate;
    return UI_MENU_WORKSPACE_OK;
}

bool ui_menu_workspace_is_dirty(const UiMenuWorkspace *workspace) {
    return workspace && workspace->active && workspace->has_document &&
           ui_document_is_dirty(&workspace->document);
}

const UiDocumentElement *ui_menu_workspace_selected_element(
    const UiMenuWorkspace *workspace
) {
    if (!workspace || !workspace->active || !workspace->has_document ||
        workspace->element_index >= workspace->document.element_count) return NULL;
    return &workspace->document.elements[workspace->element_index];
}

static void step(size_t *index, size_t count, bool previous) {
    if (count == 0U) { *index = 0U; return; }
    (void)ui_nested_inspector_step(index, count, previous);
}

static void sync_menu_nested_index(UiMenuWorkspace *workspace, size_t index) {
    size_t *nested = ui_nested_inspector_index(&workspace->nested_cursor);
    if (nested) *nested = index;
}

static bool enter_menu_nested(UiMenuWorkspace *workspace, size_t child_count) {
    sync_menu_nested_index(workspace, workspace->element_index);
    if (!ui_nested_inspector_enter(&workspace->nested_cursor, child_count)) return false;
    return true;
}

static void escape_menu_nested(UiMenuWorkspace *workspace) {
    if (ui_nested_inspector_escape(&workspace->nested_cursor)) {
        size_t *index = ui_nested_inspector_index(&workspace->nested_cursor);
        if (index && *index < workspace->document.element_count)
            workspace->element_index = *index;
    }
}

static bool workspace_element_descends_from(const UiDocument *document,
                                            const UiDocumentElement *element,
                                            UiElementId ancestor_id) {
    size_t depth = 0U;
    while (element && element->parent_id != 0U && depth++ < document->element_count) {
        if (element->parent_id == ancestor_id) return true;
        element = ui_document_find_element(document, element->parent_id);
    }
    return false;
}

static bool adjacent_sibling_exists(const UiMenuWorkspace *workspace, bool later) {
    const UiDocumentElement *selected = ui_menu_workspace_selected_element(workspace);
    size_t i;
    if (!selected) return false;
    if (later) {
        for (i = workspace->element_index + 1U; i < workspace->document.element_count; i++)
            if (workspace->document.elements[i].parent_id == selected->parent_id)
                return true;
    } else {
        i = workspace->element_index;
        while (i > 0U) {
            i--;
            if (workspace->document.elements[i].parent_id == selected->parent_id)
                return true;
        }
    }
    return false;
}

bool ui_menu_workspace_reparent_target_available(const UiMenuWorkspace *workspace,
                                                  size_t element_index) {
    const UiDocumentElement *selected = ui_menu_workspace_selected_element(workspace);
    const UiDocumentElement *candidate;
    if (!selected || element_index >= workspace->document.element_count) return false;
    candidate = &workspace->document.elements[element_index];
    return selected->id != 1U && candidate->type == UI_DOCUMENT_ELEMENT_CONTAINER &&
           candidate->id != selected->id && candidate->id != selected->parent_id &&
           !workspace_element_descends_from(
               &workspace->document, candidate, selected->id);
}

bool ui_menu_workspace_action_available(const UiMenuWorkspace *workspace,
                                        UiMenuAction action) {
    const UiDocumentElement *element = ui_menu_workspace_selected_element(workspace);
    if (!element || action < UI_MENU_ACTION_PROPERTIES || action >= UI_MENU_ACTION_COUNT)
        return false;
    if (action == UI_MENU_ACTION_PROPERTIES) return true;
    if (action == UI_MENU_ACTION_PREVIEW_SETTINGS) return true;
    if (action == UI_MENU_ACTION_ADD_CONTAINER || action == UI_MENU_ACTION_ADD_TEXT ||
        action == UI_MENU_ACTION_ADD_BUTTON)
        return element->type == UI_DOCUMENT_ELEMENT_CONTAINER;
    if (action == UI_MENU_ACTION_EDIT_CONTENT)
        return element->type == UI_DOCUMENT_ELEMENT_TEXT ||
               element->type == UI_DOCUMENT_ELEMENT_BUTTON;
    if (action == UI_MENU_ACTION_EDIT_PORT)
        return element->type == UI_DOCUMENT_ELEMENT_BUTTON;
    if (action == UI_MENU_ACTION_MOVE_EARLIER)
        return element->id != 1U && adjacent_sibling_exists(workspace, false);
    if (action == UI_MENU_ACTION_MOVE_LATER)
        return element->id != 1U && adjacent_sibling_exists(workspace, true);
    if (action == UI_MENU_ACTION_REPARENT) {
        size_t i;
        for (i = 0U; i < workspace->document.element_count; i++)
            if (ui_menu_workspace_reparent_target_available(workspace, i)) return true;
        return false;
    }
    if (action == UI_MENU_ACTION_REMOVE || action == UI_MENU_ACTION_RENAME)
        return element->id != 1U;
    return false;
}

bool ui_menu_workspace_property_available(const UiMenuWorkspace *workspace,
                                          UiMenuProperty property) {
    const UiDocumentElement *element = ui_menu_workspace_selected_element(workspace);
    if (!element || property < UI_MENU_PROPERTY_X || property >= UI_MENU_PROPERTY_COUNT)
        return false;
    if (property <= UI_MENU_PROPERTY_VERTICAL_ANCHOR) return element->id != 1U;
    if (property == UI_MENU_PROPERTY_VISUAL_MODE ||
        property == UI_MENU_PROPERTY_VISIBLE) return true;
    if (element->visual.mode == UI_DOCUMENT_VISUAL_SPRITE)
        return property == UI_MENU_PROPERTY_SPRITE_ID;
    if (property == UI_MENU_PROPERTY_SPRITE_ID) return false;
    if (property == UI_MENU_PROPERTY_ALIGNMENT)
        return element->type == UI_DOCUMENT_ELEMENT_TEXT ||
               element->type == UI_DOCUMENT_ELEMENT_BUTTON;
    if (property == UI_MENU_PROPERTY_FILL_GLYPH)
        return element->visual.fill_enabled;
    if (property == UI_MENU_PROPERTY_BORDER_GLYPH)
        return element->visual.border_enabled;
    return true;
}

static void select_first_property(UiMenuWorkspace *workspace) {
    size_t i;
    workspace->property = UI_MENU_PROPERTY_X;
    for (i = 0U; i < UI_MENU_PROPERTY_COUNT; i++)
        if (ui_menu_workspace_property_available(workspace, (UiMenuProperty)i)) {
            workspace->property = (UiMenuProperty)i;
            return;
        }
}

static void step_action(UiMenuWorkspace *workspace, bool previous) {
    size_t attempts;
    for (attempts = 0U; attempts < UI_MENU_ACTION_COUNT; attempts++) {
        step(&workspace->action_index, UI_MENU_ACTION_COUNT, previous);
        if (ui_menu_workspace_action_available(
                workspace, (UiMenuAction)workspace->action_index)) return;
    }
}

static UiMenuWorkspaceResult navigate(UiMenuWorkspace *workspace, bool previous) {
    if (!workspace) return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return UI_MENU_WORKSPACE_INACTIVE;
    if (workspace->mode == UI_MENU_WORKSPACE_CHOOSER)
        step(&workspace->chooser_index, workspace->catalog.count + 1U, previous);
    else if (workspace->mode == UI_MENU_WORKSPACE_HIERARCHY) {
        step(&workspace->element_index, workspace->document.element_count, previous);
        sync_menu_nested_index(workspace, workspace->element_index);
    }
    else if (workspace->mode == UI_MENU_WORKSPACE_PROPERTIES) {
        size_t attempts;
        size_t property = (size_t)workspace->property;
        for (attempts = 0U; attempts < UI_MENU_PROPERTY_COUNT; attempts++) {
            step(&property, UI_MENU_PROPERTY_COUNT, previous);
            if (ui_menu_workspace_property_available(
                    workspace, (UiMenuProperty)property)) {
                workspace->property = (UiMenuProperty)property;
                sync_menu_nested_index(workspace, property);
                return UI_MENU_WORKSPACE_OK;
            }
        }
        return UI_MENU_WORKSPACE_NO_ACTION;
    } else if (workspace->mode == UI_MENU_WORKSPACE_ACTIONS) {
        step_action(workspace, previous);
        sync_menu_nested_index(workspace, workspace->action_index);
    } else if (workspace->mode == UI_MENU_WORKSPACE_REPARENT) {
        size_t attempts;
        for (attempts = 0U; attempts < workspace->document.element_count; attempts++) {
            step(&workspace->reparent_index, workspace->document.element_count, previous);
            if (ui_menu_workspace_reparent_target_available(
                    workspace, workspace->reparent_index))
                return UI_MENU_WORKSPACE_OK;
        }
        return UI_MENU_WORKSPACE_NO_ACTION;
    } else if (workspace->mode == UI_MENU_WORKSPACE_PREVIEW_SETTINGS) {
        size_t field = (size_t)workspace->preview_field;
        step(&field, UI_MENU_PREVIEW_FIELD_COUNT, previous);
        workspace->preview_field = (UiMenuPreviewField)field;
    } else if (workspace->mode == UI_MENU_WORKSPACE_CLOSE_PROMPT) {
        size_t choice = (size_t)workspace->close_choice;
        step(&choice, UI_MENU_CLOSE_COUNT, previous);
        workspace->close_choice = (UiMenuCloseChoice)choice;
    } else return UI_MENU_WORKSPACE_NO_ACTION;
    return UI_MENU_WORKSPACE_OK;
}

UiMenuWorkspaceResult ui_menu_workspace_previous(UiMenuWorkspace *workspace) {
    return navigate(workspace, true);
}

UiMenuWorkspaceResult ui_menu_workspace_next(UiMenuWorkspace *workspace) {
    return navigate(workspace, false);
}

UiMenuWorkspaceResult ui_menu_workspace_open_actions(UiMenuWorkspace *workspace) {
    size_t action;
    if (!workspace) return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return UI_MENU_WORKSPACE_INACTIVE;
    if (workspace->mode != UI_MENU_WORKSPACE_HIERARCHY ||
        !ui_menu_workspace_selected_element(workspace))
        return UI_MENU_WORKSPACE_NO_ACTION;
    workspace->action_index = 0U;
    for (action = 0U; action < UI_MENU_ACTION_COUNT; action++)
        if (ui_menu_workspace_action_available(workspace, (UiMenuAction)action)) {
            if (!enter_menu_nested(workspace, UI_MENU_ACTION_COUNT))
                return UI_MENU_WORKSPACE_NO_ACTION;
            workspace->action_index = action;
            sync_menu_nested_index(workspace, action);
            workspace->mode = UI_MENU_WORKSPACE_ACTIONS;
            return UI_MENU_WORKSPACE_OK;
        }
    return UI_MENU_WORKSPACE_NO_ACTION;
}

UiMenuWorkspaceResult ui_menu_workspace_request_remove(UiMenuWorkspace *workspace) {
    const UiDocumentElement *element;
    if (!workspace) return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return UI_MENU_WORKSPACE_INACTIVE;
    if (workspace->mode != UI_MENU_WORKSPACE_HIERARCHY &&
        workspace->mode != UI_MENU_WORKSPACE_ACTIONS)
        return UI_MENU_WORKSPACE_NO_ACTION;
    element = ui_menu_workspace_selected_element(workspace);
    if (!element || element->id == 1U) return UI_MENU_WORKSPACE_NO_ACTION;
    workspace->close_return_mode = workspace->mode;
    workspace->mode = UI_MENU_WORKSPACE_REMOVE_PROMPT;
    return UI_MENU_WORKSPACE_OK;
}

static UiMenuWorkspaceResult load_selected(UiMenuWorkspace *workspace) {
    const MapCatalogEntry *entry = map_catalog_get(&workspace->catalog,
                                                   workspace->chooser_index);
    UiDocument candidate;
    if (!entry) return UI_MENU_WORKSPACE_NO_ACTION;
    ui_document_init(&candidate);
    if (ui_document_load(&candidate, entry->path) != UI_DOCUMENT_OK)
        return UI_MENU_WORKSPACE_LOAD_FAILED;
    reset_document(workspace);
    workspace->document = candidate;
    workspace->saved_document = candidate;
    workspace->has_document = true;
    workspace->mode = UI_MENU_WORKSPACE_HIERARCHY;
    return UI_MENU_WORKSPACE_OK;
}

static bool valid_created_name(const char *name) {
    size_t i;
    if (!name || name[0] == '\0') return false;
    for (i = 0U; name[i] != '\0'; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!(isalnum(c) || c == '_' || c == '-')) return false;
    }
    return i < UI_DOCUMENT_NAME_CAPACITY;
}

static UiMenuWorkspaceResult create_document(UiMenuWorkspace *workspace) {
    char directory[UI_DOCUMENT_PATH_CAPACITY];
    char path[UI_DOCUMENT_PATH_CAPACITY];
    int written;
    if (!valid_created_name(workspace->create_name))
        return UI_MENU_WORKSPACE_INVALID_NAME;
    written = snprintf(directory, sizeof(directory), "%s/menus", workspace->asset_root);
    if (written < 0 || (size_t)written >= sizeof(directory))
        return UI_MENU_WORKSPACE_INVALID_NAME;
    if (mkdir(directory, 0775) != 0 && errno != EEXIST)
        return UI_MENU_WORKSPACE_SAVE_FAILED;
    written = snprintf(path, sizeof(path), "%s/%s.tui", directory,
                       workspace->create_name);
    if (written < 0 || (size_t)written >= sizeof(path))
        return UI_MENU_WORKSPACE_INVALID_NAME;
    if (access(path, F_OK) == 0) return UI_MENU_WORKSPACE_INVALID_NAME;
    reset_document(workspace);
    if (ui_document_create_menu(&workspace->document,
                                workspace->create_name) != UI_DOCUMENT_OK)
        return UI_MENU_WORKSPACE_INVALID_NAME;
    memcpy(workspace->document.path, path, strlen(path) + 1U);
    workspace->has_document = true;
    workspace->mode = UI_MENU_WORKSPACE_HIERARCHY;
    return UI_MENU_WORKSPACE_OK;
}

static bool element_name_exists(const UiDocument *document, const char *name) {
    size_t i;
    for (i = 0U; i < document->element_count; i++)
        if (strcmp(document->elements[i].name, name) == 0) return true;
    return false;
}

static bool button_port_exists(const UiDocument *document, const char *port) {
    size_t i;
    for (i = 0U; i < document->element_count; i++)
        if (document->elements[i].type == UI_DOCUMENT_ELEMENT_BUTTON &&
            strcmp(document->elements[i].flow_port, port) == 0) return true;
    return false;
}

static bool generate_element_identity(const UiDocument *document,
                                      UiDocumentElementType type,
                                      char name[UI_DOCUMENT_NAME_CAPACITY],
                                      char port[UI_DOCUMENT_NAME_CAPACITY]) {
    const char *prefix = type == UI_DOCUMENT_ELEMENT_CONTAINER ? "container" :
                         type == UI_DOCUMENT_ELEMENT_TEXT ? "text" : "button";
    size_t suffix;
    port[0] = '\0';
    for (suffix = 1U; suffix <= UI_DOCUMENT_MAX_ELEMENTS; suffix++) {
        int written = snprintf(name, UI_DOCUMENT_NAME_CAPACITY, "%s_%zu",
                               prefix, suffix);
        if (written < 0 || written >= (int)UI_DOCUMENT_NAME_CAPACITY) return false;
        if (element_name_exists(document, name)) continue;
        if (type == UI_DOCUMENT_ELEMENT_BUTTON) {
            written = snprintf(port, UI_DOCUMENT_NAME_CAPACITY, "button_%zu", suffix);
            if (written < 0 || written >= (int)UI_DOCUMENT_NAME_CAPACITY) return false;
            if (button_port_exists(document, port)) continue;
        }
        return true;
    }
    return false;
}

static UiMenuWorkspaceResult add_child(UiMenuWorkspace *workspace,
                                       UiDocumentElementType type) {
    const UiDocumentElement *parent = ui_menu_workspace_selected_element(workspace);
    UiDocument before;
    UiDocument candidate;
    UiElementId id;
    char name[UI_DOCUMENT_NAME_CAPACITY];
    char port[UI_DOCUMENT_NAME_CAPACITY];
    const char *content = type == UI_DOCUMENT_ELEMENT_TEXT ? "TEXT" :
                          type == UI_DOCUMENT_ELEMENT_BUTTON ? "BUTTON" : "";
    if (!parent || parent->type != UI_DOCUMENT_ELEMENT_CONTAINER)
        return UI_MENU_WORKSPACE_NO_ACTION;
    before = workspace->document;
    candidate = before;
    if (!generate_element_identity(&candidate, type, name, port) ||
        ui_document_add_element(&candidate, type, parent->id, name, content, port, &id) !=
            UI_DOCUMENT_OK)
        return UI_MENU_WORKSPACE_MUTATION_FAILED;
    {
        UiDocumentLayout layout = ui_document_find_element(&candidate, id)->layout;
        layout.width = type == UI_DOCUMENT_ELEMENT_CONTAINER ? 20 : 12;
        layout.height = type == UI_DOCUMENT_ELEMENT_CONTAINER ? 8 :
                        type == UI_DOCUMENT_ELEMENT_BUTTON ? 3 : 1;
        if (ui_document_set_layout(&candidate, id, layout) != UI_DOCUMENT_OK)
            return UI_MENU_WORKSPACE_MUTATION_FAILED;
    }
    {
        UiMenuWorkspaceResult result = record_change(
            workspace, &before, &candidate, parent->id, id);
        if (result == UI_MENU_WORKSPACE_OK) {
            select_id(workspace, id);
            workspace->mode = UI_MENU_WORKSPACE_HIERARCHY;
        }
        return result;
    }
}

static void begin_element_text_edit(UiMenuWorkspace *workspace, bool port) {
    const UiDocumentElement *element = ui_menu_workspace_selected_element(workspace);
    const char *source = port ? element->flow_port : element->content;
    size_t length = strlen(source);
    memcpy(workspace->edit_text, source, length + 1U);
    workspace->edit_text_length = length;
    workspace->mode = port ? UI_MENU_WORKSPACE_EDIT_PORT
                           : UI_MENU_WORKSPACE_EDIT_CONTENT;
}

static void begin_element_name_edit(UiMenuWorkspace *workspace) {
    const UiDocumentElement *element = ui_menu_workspace_selected_element(workspace);
    size_t length = strlen(element->name);
    memcpy(workspace->edit_text, element->name, length + 1U);
    workspace->edit_text_length = length;
    workspace->mode = UI_MENU_WORKSPACE_EDIT_NAME;
}

static UiMenuWorkspaceResult commit_element_text(UiMenuWorkspace *workspace,
                                                 bool port) {
    const UiDocumentElement *element = ui_menu_workspace_selected_element(workspace);
    UiDocument before;
    UiDocument candidate;
    UiDocumentResult result;
    if (!element) return UI_MENU_WORKSPACE_NO_ACTION;
    before = workspace->document;
    candidate = before;
    result = port ? ui_document_set_flow_port(&candidate, element->id,
                                               workspace->edit_text)
                  : ui_document_set_content(&candidate, element->id,
                                             workspace->edit_text);
    if (result != UI_DOCUMENT_OK) return UI_MENU_WORKSPACE_MUTATION_FAILED;
    if (candidate.state.current_state == before.state.current_state) {
        workspace->mode = UI_MENU_WORKSPACE_ACTIONS;
        return UI_MENU_WORKSPACE_OK;
    }
    {
        UiMenuWorkspaceResult workspace_result = record_change(
            workspace, &before, &candidate, element->id, element->id);
        if (workspace_result == UI_MENU_WORKSPACE_OK)
            workspace->mode = UI_MENU_WORKSPACE_ACTIONS;
        return workspace_result;
    }
}

static UiMenuWorkspaceResult commit_element_name(UiMenuWorkspace *workspace) {
    const UiDocumentElement *element = ui_menu_workspace_selected_element(workspace);
    UiDocument before;
    UiDocument candidate;
    UiDocumentResult result;
    if (!element) return UI_MENU_WORKSPACE_NO_ACTION;
    before = workspace->document;
    candidate = before;
    result = ui_document_rename_element(&candidate, element->id, workspace->edit_text);
    if (result != UI_DOCUMENT_OK) return UI_MENU_WORKSPACE_MUTATION_FAILED;
    if (candidate.state.current_state == before.state.current_state) {
        workspace->mode = UI_MENU_WORKSPACE_ACTIONS;
        return UI_MENU_WORKSPACE_OK;
    }
    {
        UiMenuWorkspaceResult workspace_result = record_change(
            workspace, &before, &candidate, element->id, element->id);
        if (workspace_result == UI_MENU_WORKSPACE_OK)
            workspace->mode = UI_MENU_WORKSPACE_ACTIONS;
        return workspace_result;
    }
}

static UiMenuWorkspaceResult move_selected(UiMenuWorkspace *workspace, bool later) {
    const UiDocumentElement *element = ui_menu_workspace_selected_element(workspace);
    UiDocument before;
    UiDocument candidate;
    UiDocumentResult result;
    if (!element || element->id == 1U) return UI_MENU_WORKSPACE_NO_ACTION;
    before = workspace->document;
    candidate = before;
    result = later ? ui_document_move_subtree_later(&candidate, element->id)
                   : ui_document_move_subtree_earlier(&candidate, element->id);
    if (result != UI_DOCUMENT_OK) return UI_MENU_WORKSPACE_NO_ACTION;
    {
        UiElementId selected_id = element->id;
        UiMenuWorkspaceResult workspace_result = record_change(
            workspace, &before, &candidate, selected_id, selected_id);
        if (workspace_result == UI_MENU_WORKSPACE_OK) select_id(workspace, selected_id);
        return workspace_result;
    }
}

static UiMenuWorkspaceResult begin_reparent(UiMenuWorkspace *workspace) {
    size_t i;
    const UiDocumentElement *selected = ui_menu_workspace_selected_element(workspace);
    if (!selected || selected->id == 1U) return UI_MENU_WORKSPACE_NO_ACTION;
    for (i = 0U; i < workspace->document.element_count; i++) {
        if (ui_menu_workspace_reparent_target_available(workspace, i)) {
            workspace->reparent_index = i;
            workspace->mode = UI_MENU_WORKSPACE_REPARENT;
            return UI_MENU_WORKSPACE_OK;
        }
    }
    return UI_MENU_WORKSPACE_NO_ACTION;
}

static UiMenuWorkspaceResult commit_reparent(UiMenuWorkspace *workspace) {
    const UiDocumentElement *element = ui_menu_workspace_selected_element(workspace);
    UiDocument before;
    UiDocument candidate;
    UiElementId parent_id;
    if (!element || workspace->reparent_index >= workspace->document.element_count)
        return UI_MENU_WORKSPACE_NO_ACTION;
    parent_id = workspace->document.elements[workspace->reparent_index].id;
    before = workspace->document;
    candidate = before;
    if (ui_document_reparent_subtree(&candidate, element->id, parent_id) != UI_DOCUMENT_OK)
        return UI_MENU_WORKSPACE_MUTATION_FAILED;
    {
        UiMenuWorkspaceResult result = record_change(
            workspace, &before, &candidate, element->id, element->id);
        if (result == UI_MENU_WORKSPACE_OK) workspace->mode = UI_MENU_WORKSPACE_HIERARCHY;
        return result;
    }
}

static UiMenuWorkspaceResult remove_selected(UiMenuWorkspace *workspace) {
    const UiDocumentElement *element = ui_menu_workspace_selected_element(workspace);
    UiDocument before;
    UiDocument candidate;
    UiElementId removed_id;
    UiElementId parent_id;
    UiMenuWorkspaceResult result;
    if (!element || element->id == 1U) return UI_MENU_WORKSPACE_NO_ACTION;
    removed_id = element->id;
    parent_id = element->parent_id;
    before = workspace->document;
    candidate = before;
    if (ui_document_remove_subtree(&candidate, removed_id) != UI_DOCUMENT_OK)
        return UI_MENU_WORKSPACE_MUTATION_FAILED;
    result = record_change(workspace, &before, &candidate, removed_id, parent_id);
    if (result == UI_MENU_WORKSPACE_OK) {
        select_id(workspace, parent_id);
        workspace->mode = UI_MENU_WORKSPACE_HIERARCHY;
    }
    return result;
}

UiMenuWorkspaceResult ui_menu_workspace_save(UiMenuWorkspace *workspace) {
    UiDocument candidate;
    size_t i;
    if (!workspace) return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return UI_MENU_WORKSPACE_INACTIVE;
    if (!workspace->has_document) return UI_MENU_WORKSPACE_NO_ACTION;
    candidate = workspace->document;
    if (ui_document_save(&candidate) != UI_DOCUMENT_OK)
        return UI_MENU_WORKSPACE_SAVE_FAILED;
    workspace->document = candidate;
    workspace->saved_document = candidate;
    for (i = 0U; i < workspace->change_count; i++) {
        workspace->changes[i].before->state.saved_state = candidate.state.saved_state;
        workspace->changes[i].after->state.saved_state = candidate.state.saved_state;
        memcpy(workspace->changes[i].before->path, candidate.path,
               strlen(candidate.path) + 1U);
        memcpy(workspace->changes[i].after->path, candidate.path,
               strlen(candidate.path) + 1U);
    }
    return UI_MENU_WORKSPACE_OK;
}

UiMenuWorkspaceResult ui_menu_workspace_confirm(UiMenuWorkspace *workspace) {
    const UiDocumentElement *element;
    if (!workspace) return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return UI_MENU_WORKSPACE_INACTIVE;
    if (workspace->mode == UI_MENU_WORKSPACE_CHOOSER) {
        if (workspace->chooser_index == workspace->catalog.count) {
            workspace->create_name[0] = '\0';
            workspace->create_name_length = 0U;
            workspace->mode = UI_MENU_WORKSPACE_CREATE_NAME;
            return UI_MENU_WORKSPACE_OK;
        }
        return load_selected(workspace);
    }
    if (workspace->mode == UI_MENU_WORKSPACE_CREATE_NAME)
        return create_document(workspace);
    if (workspace->mode == UI_MENU_WORKSPACE_HIERARCHY) {
        element = ui_menu_workspace_selected_element(workspace);
        if (!element) return UI_MENU_WORKSPACE_NO_ACTION;
        select_first_property(workspace);
        if (!enter_menu_nested(workspace, UI_MENU_PROPERTY_COUNT))
            return UI_MENU_WORKSPACE_NO_ACTION;
        select_first_property(workspace);
        sync_menu_nested_index(workspace, (size_t)workspace->property);
        workspace->mode = UI_MENU_WORKSPACE_PROPERTIES;
        return UI_MENU_WORKSPACE_OK;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_EDIT_CONTENT)
        return commit_element_text(workspace, false);
    if (workspace->mode == UI_MENU_WORKSPACE_EDIT_PORT)
        return commit_element_text(workspace, true);
    if (workspace->mode == UI_MENU_WORKSPACE_EDIT_NAME)
        return commit_element_name(workspace);
    if (workspace->mode == UI_MENU_WORKSPACE_REPARENT)
        return commit_reparent(workspace);
    if (workspace->mode == UI_MENU_WORKSPACE_REMOVE_PROMPT)
        return remove_selected(workspace);
    if (workspace->mode == UI_MENU_WORKSPACE_ACTIONS) {
        UiMenuAction action = (UiMenuAction)workspace->action_index;
        if (!ui_menu_workspace_action_available(workspace, action))
            return UI_MENU_WORKSPACE_NO_ACTION;
        if (action == UI_MENU_ACTION_PROPERTIES) {
            select_first_property(workspace);
            workspace->mode = UI_MENU_WORKSPACE_PROPERTIES;
            return UI_MENU_WORKSPACE_OK;
        }
        if (action == UI_MENU_ACTION_ADD_CONTAINER)
            return add_child(workspace, UI_DOCUMENT_ELEMENT_CONTAINER);
        if (action == UI_MENU_ACTION_ADD_TEXT)
            return add_child(workspace, UI_DOCUMENT_ELEMENT_TEXT);
        if (action == UI_MENU_ACTION_ADD_BUTTON)
            return add_child(workspace, UI_DOCUMENT_ELEMENT_BUTTON);
        if (action == UI_MENU_ACTION_EDIT_CONTENT) {
            begin_element_text_edit(workspace, false);
            return UI_MENU_WORKSPACE_OK;
        }
        if (action == UI_MENU_ACTION_EDIT_PORT) {
            begin_element_text_edit(workspace, true);
            return UI_MENU_WORKSPACE_OK;
        }
        if (action == UI_MENU_ACTION_REMOVE)
            return ui_menu_workspace_request_remove(workspace);
        if (action == UI_MENU_ACTION_RENAME) {
            begin_element_name_edit(workspace);
            return UI_MENU_WORKSPACE_OK;
        }
        if (action == UI_MENU_ACTION_REPARENT) return begin_reparent(workspace);
        if (action == UI_MENU_ACTION_MOVE_EARLIER) return move_selected(workspace, false);
        if (action == UI_MENU_ACTION_MOVE_LATER) return move_selected(workspace, true);
        workspace->preview_field = UI_MENU_PREVIEW_FIELD_RESOLUTION;
        workspace->mode = UI_MENU_WORKSPACE_PREVIEW_SETTINGS;
        return UI_MENU_WORKSPACE_OK;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_CLOSE_PROMPT) {
        if (workspace->close_choice == UI_MENU_CLOSE_SAVE) {
            UiMenuWorkspaceResult result = ui_menu_workspace_save(workspace);
            if (result == UI_MENU_WORKSPACE_OK) {
                result = refresh_catalog(workspace);
                if (result != UI_MENU_WORKSPACE_OK) {
                    workspace->mode = UI_MENU_WORKSPACE_HIERARCHY;
                    return result;
                }
                workspace->has_document = false;
                workspace->mode = UI_MENU_WORKSPACE_CHOOSER;
            }
            return result;
        }
        if (workspace->close_choice == UI_MENU_CLOSE_DISCARD) {
            reset_document(workspace);
            workspace->mode = UI_MENU_WORKSPACE_CHOOSER;
            return UI_MENU_WORKSPACE_OK;
        }
        workspace->mode = workspace->close_return_mode;
        return UI_MENU_WORKSPACE_OK;
    }
    return UI_MENU_WORKSPACE_NO_ACTION;
}

UiMenuWorkspaceResult ui_menu_workspace_escape(UiMenuWorkspace *workspace) {
    if (!workspace) return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return UI_MENU_WORKSPACE_INACTIVE;
    if (workspace->mode == UI_MENU_WORKSPACE_CREATE_NAME) {
        workspace->mode = UI_MENU_WORKSPACE_CHOOSER;
        return UI_MENU_WORKSPACE_OK;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_EDIT_CONTENT ||
        workspace->mode == UI_MENU_WORKSPACE_EDIT_PORT ||
        workspace->mode == UI_MENU_WORKSPACE_EDIT_NAME ||
        workspace->mode == UI_MENU_WORKSPACE_ACTIONS) {
        workspace->mode = workspace->mode == UI_MENU_WORKSPACE_ACTIONS
            ? UI_MENU_WORKSPACE_HIERARCHY : UI_MENU_WORKSPACE_ACTIONS;
        if (workspace->mode == UI_MENU_WORKSPACE_HIERARCHY)
            escape_menu_nested(workspace);
        return UI_MENU_WORKSPACE_OK;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_REPARENT) {
        workspace->mode = UI_MENU_WORKSPACE_ACTIONS;
        return UI_MENU_WORKSPACE_OK;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_PREVIEW_SETTINGS) {
        workspace->mode = UI_MENU_WORKSPACE_ACTIONS;
        return UI_MENU_WORKSPACE_OK;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_REMOVE_PROMPT) {
        workspace->mode = workspace->close_return_mode;
        return UI_MENU_WORKSPACE_OK;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_PROPERTIES) {
        escape_menu_nested(workspace);
        workspace->mode = UI_MENU_WORKSPACE_HIERARCHY;
        return UI_MENU_WORKSPACE_OK;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_CLOSE_PROMPT) {
        workspace->mode = workspace->close_return_mode;
        return UI_MENU_WORKSPACE_OK;
    }
    if (workspace->mode == UI_MENU_WORKSPACE_HIERARCHY) {
        if (ui_menu_workspace_is_dirty(workspace)) {
            workspace->close_choice = UI_MENU_CLOSE_SAVE;
            workspace->close_return_mode = UI_MENU_WORKSPACE_HIERARCHY;
            workspace->mode = UI_MENU_WORKSPACE_CLOSE_PROMPT;
        } else {
            if (refresh_catalog(workspace) != UI_MENU_WORKSPACE_OK)
                return UI_MENU_WORKSPACE_CATALOG_FAILED;
            reset_document(workspace);
            workspace->mode = UI_MENU_WORKSPACE_CHOOSER;
        }
        return UI_MENU_WORKSPACE_OK;
    }
    workspace->active = false;
    return UI_MENU_WORKSPACE_OK;
}

static UiMenuWorkspaceResult record_change(UiMenuWorkspace *workspace,
                                           const UiDocument *before,
                                           const UiDocument *after,
                                           UiElementId before_selected_id,
                                           UiElementId after_selected_id) {
    UiDocument *before_copy;
    UiDocument *after_copy;
    size_t i;
    if (workspace->change_cursor >= UI_MENU_WORKSPACE_HISTORY_CAPACITY)
        return UI_MENU_WORKSPACE_HISTORY_FULL;
    before_copy = malloc(sizeof(*before_copy));
    after_copy = malloc(sizeof(*after_copy));
    if (!before_copy || !after_copy) {
        free(before_copy);
        free(after_copy);
        return UI_MENU_WORKSPACE_MUTATION_FAILED;
    }
    *before_copy = *before;
    *after_copy = *after;
    for (i = workspace->change_cursor; i < workspace->change_count; i++) {
        free(workspace->changes[i].before);
        free(workspace->changes[i].after);
        workspace->changes[i].before = NULL;
        workspace->changes[i].after = NULL;
    }
    workspace->changes[workspace->change_cursor] = (UiMenuWorkspaceChange){
        before_copy, after_copy, before_selected_id, after_selected_id};
    workspace->change_cursor++;
    workspace->change_count = workspace->change_cursor;
    workspace->document = *after;
    return UI_MENU_WORKSPACE_OK;
}

static uint8_t adjust_channel(uint8_t value, int direction) {
    if (direction < 0) return value == 0U ? 0U : (uint8_t)(value - 1U);
    return value == UINT8_MAX ? UINT8_MAX : (uint8_t)(value + 1U);
}

static uint8_t cycle_glyph(uint8_t value, int direction) {
    if (value < 32U || value > 126U) value = (uint8_t)' ';
    if (direction < 0) return value == 32U ? 126U : (uint8_t)(value - 1U);
    return value == 126U ? 32U : (uint8_t)(value + 1U);
}

static void normalize_anchor_axis(UiDocumentLayout *layout, bool horizontal,
                                  UiDocumentAnchor anchor) {
    int *offset = horizontal ? &layout->x : &layout->y;
    int *extent = horizontal ? &layout->width : &layout->height;
    if (anchor == UI_DOCUMENT_ANCHOR_STRETCH) {
        if (*offset < 0) *offset = 0;
        if (*extent < 0) *extent = 0;
    } else if (*extent <= 0) *extent = 1;
}

static bool adjust_layout_property(UiDocumentLayout *layout,
                                   UiMenuProperty property, int direction) {
    if (property == UI_MENU_PROPERTY_X) {
        if ((direction < 0 && layout->x == INT_MIN) ||
            (direction > 0 && layout->x == INT_MAX)) return false;
        layout->x += direction;
    } else if (property == UI_MENU_PROPERTY_Y) {
        if ((direction < 0 && layout->y == INT_MIN) ||
            (direction > 0 && layout->y == INT_MAX)) return false;
        layout->y += direction;
    } else if (property == UI_MENU_PROPERTY_WIDTH) {
        int minimum = layout->horizontal_anchor == UI_DOCUMENT_ANCHOR_STRETCH ? 0 : 1;
        if ((direction < 0 && layout->width <= minimum) ||
            (direction > 0 && layout->width == INT_MAX)) return false;
        layout->width += direction;
    } else if (property == UI_MENU_PROPERTY_HEIGHT) {
        int minimum = layout->vertical_anchor == UI_DOCUMENT_ANCHOR_STRETCH ? 0 : 1;
        if ((direction < 0 && layout->height <= minimum) ||
            (direction > 0 && layout->height == INT_MAX)) return false;
        layout->height += direction;
    } else if (property == UI_MENU_PROPERTY_SCALE) {
        if ((direction < 0 && layout->scale_percent <= 25) ||
            (direction > 0 && layout->scale_percent >= 400)) return false;
        layout->scale_percent += direction * 25;
    } else if (property == UI_MENU_PROPERTY_HORIZONTAL_ANCHOR) {
        int value = (int)layout->horizontal_anchor + direction;
        if (value < UI_DOCUMENT_ANCHOR_START) value = UI_DOCUMENT_ANCHOR_STRETCH;
        if (value > UI_DOCUMENT_ANCHOR_STRETCH) value = UI_DOCUMENT_ANCHOR_START;
        layout->horizontal_anchor = (UiDocumentAnchor)value;
        normalize_anchor_axis(layout, true, layout->horizontal_anchor);
    } else if (property == UI_MENU_PROPERTY_VERTICAL_ANCHOR) {
        int value = (int)layout->vertical_anchor + direction;
        if (value < UI_DOCUMENT_ANCHOR_START) value = UI_DOCUMENT_ANCHOR_STRETCH;
        if (value > UI_DOCUMENT_ANCHOR_STRETCH) value = UI_DOCUMENT_ANCHOR_START;
        layout->vertical_anchor = (UiDocumentAnchor)value;
        normalize_anchor_axis(layout, false, layout->vertical_anchor);
    } else return false;
    return true;
}

static bool adjust_visual_property(UiDocumentVisual *visual,
                                   UiMenuProperty property, int direction) {
    uint8_t before_channel;
    if (property == UI_MENU_PROPERTY_VISUAL_MODE) {
        if (visual->mode == UI_DOCUMENT_VISUAL_NATIVE) {
            visual->mode = UI_DOCUMENT_VISUAL_SPRITE;
            visual->sprite_id = 1U;
        } else {
            visual->mode = UI_DOCUMENT_VISUAL_NATIVE;
            visual->sprite_id = 0U;
        }
    } else if (property == UI_MENU_PROPERTY_SPRITE_ID) {
        if (direction < 0) visual->sprite_id = visual->sprite_id <= 1U
            ? UINT8_MAX : (uint16_t)(visual->sprite_id - 1U);
        else visual->sprite_id = visual->sprite_id >= UINT8_MAX
            ? 1U : (uint16_t)(visual->sprite_id + 1U);
    } else if (property == UI_MENU_PROPERTY_ALIGNMENT) {
        int value = (int)visual->align + direction;
        if (value < UI_DOCUMENT_ALIGN_LEFT) value = UI_DOCUMENT_ALIGN_RIGHT;
        if (value > UI_DOCUMENT_ALIGN_RIGHT) value = UI_DOCUMENT_ALIGN_LEFT;
        visual->align = (UiDocumentAlign)value;
    } else if (property >= UI_MENU_PROPERTY_FOREGROUND_RED &&
               property <= UI_MENU_PROPERTY_BACKGROUND_ALPHA) {
        uint8_t *channel = NULL;
        switch (property) {
            case UI_MENU_PROPERTY_FOREGROUND_RED: channel = &visual->foreground.red; break;
            case UI_MENU_PROPERTY_FOREGROUND_GREEN: channel = &visual->foreground.green; break;
            case UI_MENU_PROPERTY_FOREGROUND_BLUE: channel = &visual->foreground.blue; break;
            case UI_MENU_PROPERTY_FOREGROUND_ALPHA: channel = &visual->foreground.alpha; break;
            case UI_MENU_PROPERTY_BACKGROUND_RED: channel = &visual->background.red; break;
            case UI_MENU_PROPERTY_BACKGROUND_GREEN: channel = &visual->background.green; break;
            case UI_MENU_PROPERTY_BACKGROUND_BLUE: channel = &visual->background.blue; break;
            case UI_MENU_PROPERTY_BACKGROUND_ALPHA: channel = &visual->background.alpha; break;
            default: return false;
        }
        before_channel = *channel;
        *channel = adjust_channel(*channel, direction);
        if (*channel == before_channel) return false;
    } else if (property == UI_MENU_PROPERTY_FILL_ENABLED)
        visual->fill_enabled = !visual->fill_enabled;
    else if (property == UI_MENU_PROPERTY_FILL_GLYPH)
        visual->fill_glyph = cycle_glyph(visual->fill_glyph, direction);
    else if (property == UI_MENU_PROPERTY_BORDER_ENABLED)
        visual->border_enabled = !visual->border_enabled;
    else if (property == UI_MENU_PROPERTY_BORDER_GLYPH)
        visual->border_glyph = cycle_glyph(visual->border_glyph, direction);
    else if (property == UI_MENU_PROPERTY_VISIBLE)
        visual->visible_by_default = !visual->visible_by_default;
    else return false;
    return true;
}

UiMenuWorkspaceResult ui_menu_workspace_adjust(UiMenuWorkspace *workspace,
                                               int direction) {
    UiDocument before;
    UiDocument candidate;
    UiDocumentLayout layout;
    const UiDocumentElement *element;
    if (!workspace || (direction != -1 && direction != 1))
        return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return UI_MENU_WORKSPACE_INACTIVE;
    if (workspace->mode == UI_MENU_WORKSPACE_PREVIEW_SETTINGS) {
        if (workspace->preview_field == UI_MENU_PREVIEW_FIELD_RESOLUTION) {
            size_t value = (size_t)workspace->preview_resolution;
            step(&value, UI_MENU_PREVIEW_RESOLUTION_COUNT, direction < 0);
            workspace->preview_resolution = (UiMenuPreviewResolution)value;
        } else {
            size_t value = (size_t)workspace->preview_scale;
            step(&value, UI_MENU_PREVIEW_SCALE_COUNT, direction < 0);
            workspace->preview_scale = (UiMenuPreviewScale)value;
        }
        return UI_MENU_WORKSPACE_OK;
    }
    if (workspace->mode != UI_MENU_WORKSPACE_PROPERTIES)
        return UI_MENU_WORKSPACE_NO_ACTION;
    element = ui_menu_workspace_selected_element(workspace);
    if (!element || !ui_menu_workspace_property_available(workspace, workspace->property))
        return UI_MENU_WORKSPACE_NO_ACTION;
    before = workspace->document;
    candidate = before;
    layout = element->layout;
    if (workspace->property <= UI_MENU_PROPERTY_VERTICAL_ANCHOR) {
        if (!adjust_layout_property(&layout, workspace->property, direction))
            return UI_MENU_WORKSPACE_NO_ACTION;
        if (ui_document_set_layout(&candidate, element->id, layout) != UI_DOCUMENT_OK)
            return UI_MENU_WORKSPACE_MUTATION_FAILED;
    } else {
        UiDocumentVisual visual = element->visual;
        if (!adjust_visual_property(&visual, workspace->property, direction))
            return UI_MENU_WORKSPACE_NO_ACTION;
        if (ui_document_set_visual(&candidate, element->id, visual) != UI_DOCUMENT_OK)
            return UI_MENU_WORKSPACE_MUTATION_FAILED;
    }
    return record_change(workspace, &before, &candidate, element->id, element->id);
}

void ui_menu_workspace_preview_dimensions(const UiMenuWorkspace *workspace,
                                          int *out_width, int *out_height) {
    static const int widths[] = {40, 60, 80};
    static const int heights[] = {15, 20, 25};
    size_t index = workspace && workspace->preview_resolution >= 0 &&
        workspace->preview_resolution < UI_MENU_PREVIEW_RESOLUTION_COUNT
        ? (size_t)workspace->preview_resolution : (size_t)UI_MENU_PREVIEW_RESOLUTION_80X25;
    int scale = ui_menu_workspace_preview_scale_percent(workspace);
    if (out_width) *out_width = widths[index] * 100 / scale;
    if (out_height) *out_height = heights[index] * 100 / scale;
}

int ui_menu_workspace_preview_scale_percent(const UiMenuWorkspace *workspace) {
    static const int scales[] = {100, 125, 150, 200};
    size_t index = workspace && workspace->preview_scale >= 0 &&
        workspace->preview_scale < UI_MENU_PREVIEW_SCALE_COUNT
        ? (size_t)workspace->preview_scale : (size_t)UI_MENU_PREVIEW_SCALE_100;
    return scales[index];
}

UiMenuWorkspaceResult ui_menu_workspace_append_text(UiMenuWorkspace *workspace,
                                                    const char *text) {
    size_t i;
    if (!workspace || !text) return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return UI_MENU_WORKSPACE_INACTIVE;
    if (workspace->mode != UI_MENU_WORKSPACE_CREATE_NAME &&
        workspace->mode != UI_MENU_WORKSPACE_EDIT_CONTENT &&
        workspace->mode != UI_MENU_WORKSPACE_EDIT_PORT &&
        workspace->mode != UI_MENU_WORKSPACE_EDIT_NAME)
        return UI_MENU_WORKSPACE_NO_ACTION;
    for (i = 0U; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];
        char *buffer = workspace->mode == UI_MENU_WORKSPACE_CREATE_NAME
            ? workspace->create_name : workspace->edit_text;
        size_t *length = workspace->mode == UI_MENU_WORKSPACE_CREATE_NAME
            ? &workspace->create_name_length : &workspace->edit_text_length;
        size_t capacity = workspace->mode == UI_MENU_WORKSPACE_CREATE_NAME
            ? sizeof(workspace->create_name) :
            (workspace->mode == UI_MENU_WORKSPACE_EDIT_PORT ||
             workspace->mode == UI_MENU_WORKSPACE_EDIT_NAME)
            ? UI_DOCUMENT_NAME_CAPACITY : sizeof(workspace->edit_text);
        bool identifier = workspace->mode != UI_MENU_WORKSPACE_EDIT_CONTENT;
        if ((identifier && !(isalnum(c) || c == '_' || c == '-')) ||
            (!identifier && (c < 32U || c > 126U || c == '\\'))) continue;
        if (*length + 1U >= capacity)
            return UI_MENU_WORKSPACE_INVALID_NAME;
        buffer[(*length)++] = (char)c;
        buffer[*length] = '\0';
    }
    return UI_MENU_WORKSPACE_OK;
}

UiMenuWorkspaceResult ui_menu_workspace_backspace(UiMenuWorkspace *workspace) {
    if (!workspace) return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return UI_MENU_WORKSPACE_INACTIVE;
    if (workspace->mode == UI_MENU_WORKSPACE_CREATE_NAME) {
        if (workspace->create_name_length == 0U) return UI_MENU_WORKSPACE_NO_ACTION;
        workspace->create_name[--workspace->create_name_length] = '\0';
    } else if (workspace->mode == UI_MENU_WORKSPACE_EDIT_CONTENT ||
               workspace->mode == UI_MENU_WORKSPACE_EDIT_PORT ||
               workspace->mode == UI_MENU_WORKSPACE_EDIT_NAME) {
        if (workspace->edit_text_length == 0U) return UI_MENU_WORKSPACE_NO_ACTION;
        workspace->edit_text[--workspace->edit_text_length] = '\0';
    } else return UI_MENU_WORKSPACE_NO_ACTION;
    return UI_MENU_WORKSPACE_OK;
}

UiMenuWorkspaceResult ui_menu_workspace_undo(UiMenuWorkspace *workspace) {
    UiMenuWorkspaceChange *change;
    DocumentStateId next_state;
    if (!workspace) return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return UI_MENU_WORKSPACE_INACTIVE;
    if (!workspace->has_document || workspace->change_cursor == 0U)
        return UI_MENU_WORKSPACE_NO_ACTION;
    change = &workspace->changes[workspace->change_cursor - 1U];
    next_state = workspace->document.state.next_state;
    workspace->document = *change->before;
    if (workspace->document.state.next_state < next_state)
        workspace->document.state.next_state = next_state;
    select_id(workspace, change->before_selected_id);
    workspace->change_cursor--;
    return UI_MENU_WORKSPACE_OK;
}

UiMenuWorkspaceResult ui_menu_workspace_redo(UiMenuWorkspace *workspace) {
    UiMenuWorkspaceChange *change;
    DocumentStateId next_state;
    if (!workspace) return UI_MENU_WORKSPACE_INVALID_ARGUMENT;
    if (!workspace->active) return UI_MENU_WORKSPACE_INACTIVE;
    if (!workspace->has_document || workspace->change_cursor >= workspace->change_count)
        return UI_MENU_WORKSPACE_NO_ACTION;
    change = &workspace->changes[workspace->change_cursor];
    next_state = workspace->document.state.next_state;
    workspace->document = *change->after;
    if (workspace->document.state.next_state < next_state)
        workspace->document.state.next_state = next_state;
    select_id(workspace, change->after_selected_id);
    workspace->change_cursor++;
    return UI_MENU_WORKSPACE_OK;
}