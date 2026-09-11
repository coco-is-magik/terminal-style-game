/** ui_menu_workspace.h — Headless staged authored-Menu workspace. */
#ifndef UI_MENU_WORKSPACE_H
#define UI_MENU_WORKSPACE_H

#include "map_catalog.h"
#include "ui_document.h"
#include "ui_nested_inspector.h"

#define UI_MENU_WORKSPACE_HISTORY_CAPACITY 32U

typedef enum {
    UI_MENU_WORKSPACE_CHOOSER = 0,
    UI_MENU_WORKSPACE_CREATE_NAME,
    UI_MENU_WORKSPACE_HIERARCHY,
    UI_MENU_WORKSPACE_PROPERTIES,
    UI_MENU_WORKSPACE_ACTIONS,
    UI_MENU_WORKSPACE_EDIT_CONTENT,
    UI_MENU_WORKSPACE_EDIT_PORT,
    UI_MENU_WORKSPACE_EDIT_NAME,
    UI_MENU_WORKSPACE_REPARENT,
    UI_MENU_WORKSPACE_PREVIEW_SETTINGS,
    UI_MENU_WORKSPACE_REMOVE_PROMPT,
    UI_MENU_WORKSPACE_CLOSE_PROMPT
} UiMenuWorkspaceMode;

typedef enum {
    UI_MENU_ACTION_PROPERTIES = 0,
    UI_MENU_ACTION_ADD_CONTAINER,
    UI_MENU_ACTION_ADD_TEXT,
    UI_MENU_ACTION_ADD_BUTTON,
    UI_MENU_ACTION_EDIT_CONTENT,
    UI_MENU_ACTION_EDIT_PORT,
    UI_MENU_ACTION_REMOVE,
    UI_MENU_ACTION_RENAME,
    UI_MENU_ACTION_REPARENT,
    UI_MENU_ACTION_MOVE_EARLIER,
    UI_MENU_ACTION_MOVE_LATER,
    UI_MENU_ACTION_PREVIEW_SETTINGS,
    UI_MENU_ACTION_COUNT
} UiMenuAction;

typedef enum {
    UI_MENU_PROPERTY_X = 0,
    UI_MENU_PROPERTY_Y,
    UI_MENU_PROPERTY_WIDTH,
    UI_MENU_PROPERTY_HEIGHT,
    UI_MENU_PROPERTY_SCALE,
    UI_MENU_PROPERTY_HORIZONTAL_ANCHOR,
    UI_MENU_PROPERTY_VERTICAL_ANCHOR,
    UI_MENU_PROPERTY_VISUAL_MODE,
    UI_MENU_PROPERTY_SPRITE_ID,
    UI_MENU_PROPERTY_ALIGNMENT,
    UI_MENU_PROPERTY_FOREGROUND_RED,
    UI_MENU_PROPERTY_FOREGROUND_GREEN,
    UI_MENU_PROPERTY_FOREGROUND_BLUE,
    UI_MENU_PROPERTY_FOREGROUND_ALPHA,
    UI_MENU_PROPERTY_BACKGROUND_RED,
    UI_MENU_PROPERTY_BACKGROUND_GREEN,
    UI_MENU_PROPERTY_BACKGROUND_BLUE,
    UI_MENU_PROPERTY_BACKGROUND_ALPHA,
    UI_MENU_PROPERTY_FILL_ENABLED,
    UI_MENU_PROPERTY_FILL_GLYPH,
    UI_MENU_PROPERTY_BORDER_ENABLED,
    UI_MENU_PROPERTY_BORDER_GLYPH,
    UI_MENU_PROPERTY_VISIBLE,
    UI_MENU_PROPERTY_COUNT
} UiMenuProperty;

typedef enum {
    UI_MENU_CLOSE_SAVE = 0,
    UI_MENU_CLOSE_DISCARD,
    UI_MENU_CLOSE_CANCEL,
    UI_MENU_CLOSE_COUNT
} UiMenuCloseChoice;

typedef enum {
    UI_MENU_POINTER_NONE = 0,
    UI_MENU_POINTER_MOVE,
    UI_MENU_POINTER_RESIZE
} UiMenuPointerMode;

typedef enum {
    UI_MENU_PREVIEW_RESOLUTION_40X15 = 0,
    UI_MENU_PREVIEW_RESOLUTION_60X20,
    UI_MENU_PREVIEW_RESOLUTION_80X25,
    UI_MENU_PREVIEW_RESOLUTION_COUNT
} UiMenuPreviewResolution;

typedef enum {
    UI_MENU_PREVIEW_SCALE_100 = 0,
    UI_MENU_PREVIEW_SCALE_125,
    UI_MENU_PREVIEW_SCALE_150,
    UI_MENU_PREVIEW_SCALE_200,
    UI_MENU_PREVIEW_SCALE_COUNT
} UiMenuPreviewScale;

typedef enum {
    UI_MENU_PREVIEW_FIELD_RESOLUTION = 0,
    UI_MENU_PREVIEW_FIELD_SCALE,
    UI_MENU_PREVIEW_FIELD_COUNT
} UiMenuPreviewField;

typedef enum {
    UI_MENU_WORKSPACE_OK = 0,
    UI_MENU_WORKSPACE_INVALID_ARGUMENT,
    UI_MENU_WORKSPACE_INACTIVE,
    UI_MENU_WORKSPACE_NO_ACTION,
    UI_MENU_WORKSPACE_CATALOG_FAILED,
    UI_MENU_WORKSPACE_LOAD_FAILED,
    UI_MENU_WORKSPACE_INVALID_NAME,
    UI_MENU_WORKSPACE_SAVE_FAILED,
    UI_MENU_WORKSPACE_MUTATION_FAILED,
    UI_MENU_WORKSPACE_HISTORY_FULL
} UiMenuWorkspaceResult;

typedef struct {
    UiDocument *before;
    UiDocument *after;
    UiElementId before_selected_id;
    UiElementId after_selected_id;
} UiMenuWorkspaceChange;

typedef struct {
    UiDocument document;
    UiDocument saved_document;
    MapCatalog catalog;
    UiMenuWorkspaceChange changes[UI_MENU_WORKSPACE_HISTORY_CAPACITY];
    size_t change_count;
    size_t change_cursor;
    size_t chooser_index;
    size_t element_index;
    size_t action_index;
    size_t reparent_index;
    UiNestedInspectorCursor nested_cursor;
    UiMenuProperty property;
    UiMenuWorkspaceMode mode;
    UiMenuWorkspaceMode close_return_mode;
    UiMenuCloseChoice close_choice;
    UiDocument *pointer_before;
    UiElementId pointer_element_id;
    UiMenuPointerMode pointer_mode;
    UiMenuPreviewResolution preview_resolution;
    UiMenuPreviewScale preview_scale;
    UiMenuPreviewField preview_field;
    int pointer_start_x;
    int pointer_start_y;
    char asset_root[UI_DOCUMENT_PATH_CAPACITY];
    char create_name[UI_DOCUMENT_NAME_CAPACITY];
    size_t create_name_length;
    char edit_text[UI_DOCUMENT_CONTENT_CAPACITY];
    size_t edit_text_length;
    bool has_document;
    bool active;
} UiMenuWorkspace;

void ui_menu_workspace_init(UiMenuWorkspace *workspace);
void ui_menu_workspace_clear(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_open(UiMenuWorkspace *workspace,
                                             const char *asset_root);
UiMenuWorkspaceResult ui_menu_workspace_previous(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_next(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_confirm(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_escape(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_adjust(UiMenuWorkspace *workspace,
                                               int direction);
UiMenuWorkspaceResult ui_menu_workspace_open_actions(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_request_remove(UiMenuWorkspace *workspace);
bool ui_menu_workspace_action_available(const UiMenuWorkspace *workspace,
                                        UiMenuAction action);
bool ui_menu_workspace_reparent_target_available(const UiMenuWorkspace *workspace,
                                                  size_t element_index);
bool ui_menu_workspace_property_available(const UiMenuWorkspace *workspace,
                                          UiMenuProperty property);
UiMenuWorkspaceResult ui_menu_workspace_pointer_press(
    UiMenuWorkspace *workspace, int pointer_x, int pointer_y,
    int viewport_width, int viewport_height);
UiMenuWorkspaceResult ui_menu_workspace_pointer_motion(
    UiMenuWorkspace *workspace, int pointer_x, int pointer_y);
UiMenuWorkspaceResult ui_menu_workspace_pointer_release(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_pointer_cancel(UiMenuWorkspace *workspace);
void ui_menu_workspace_preview_dimensions(const UiMenuWorkspace *workspace,
                                          int *out_width, int *out_height);
int ui_menu_workspace_preview_scale_percent(const UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_append_text(UiMenuWorkspace *workspace,
                                                    const char *text);
UiMenuWorkspaceResult ui_menu_workspace_backspace(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_save(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_undo(UiMenuWorkspace *workspace);
UiMenuWorkspaceResult ui_menu_workspace_redo(UiMenuWorkspace *workspace);
bool ui_menu_workspace_is_dirty(const UiMenuWorkspace *workspace);
const UiDocumentElement *ui_menu_workspace_selected_element(
    const UiMenuWorkspace *workspace
);

#endif /* UI_MENU_WORKSPACE_H */