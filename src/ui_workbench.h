/** ui_workbench.h — Small live application-UI asset tweak session. */
#ifndef UI_WORKBENCH_H
#define UI_WORKBENCH_H

#include "menu_state.h"
#include "map_catalog.h"
#include "ui_ele.h"
#include "ui_workbench_store.h"

#include <stdbool.h>

typedef enum {
    UI_WORKBENCH_PROPERTY_PARENT = 0,
    UI_WORKBENCH_PROPERTY_STYLE,
    UI_WORKBENCH_PROPERTY_TRANSITION,
    UI_WORKBENCH_PROPERTY_FOCUS_EFFECT,
    UI_WORKBENCH_PROPERTY_PRESET,
    UI_WORKBENCH_PROPERTY_TARGET,
    UI_WORKBENCH_PROPERTY_TRIGGER,
    UI_WORKBENCH_PROPERTY_ORIENTATION,
    UI_WORKBENCH_PROPERTY_LOOP,
    UI_WORKBENCH_PROPERTY_RANDOMIZE,
    UI_WORKBENCH_PROPERTY_WIDTH,
    UI_WORKBENCH_PROPERTY_HEIGHT,
    UI_WORKBENCH_PROPERTY_VISIBLE,
    UI_WORKBENCH_PROPERTY_ALIGN,
    UI_WORKBENCH_PROPERTY_Z_INDEX,
    UI_WORKBENCH_PROPERTY_COORDS,
    UI_WORKBENCH_PROPERTY_ACTION,
    UI_WORKBENCH_PROPERTY_CONTENT,
    UI_WORKBENCH_PROPERTY_FOREGROUND,
    UI_WORKBENCH_PROPERTY_BACKGROUND,
    UI_WORKBENCH_PROPERTY_ADD,
    UI_WORKBENCH_PROPERTY_REMOVE,
    UI_WORKBENCH_PROPERTY_COUNT
} UiWorkbenchProperty;

typedef enum {
    UI_WORKBENCH_PREVIEW_NORMAL,
    UI_WORKBENCH_PREVIEW_FOCUSED,
    UI_WORKBENCH_PREVIEW_COMPARE
} UiWorkbenchPreviewState;

typedef enum {
    UI_WORKBENCH_MODE_BROWSE = 0,
    UI_WORKBENCH_MODE_ADD,
    UI_WORKBENCH_MODE_REMOVE_CONFIRM,
    UI_WORKBENCH_MODE_HELP,
    UI_WORKBENCH_MODE_TEXT
} UiWorkbenchMode;

typedef enum {
    UI_WORKBENCH_OK = 0,
    UI_WORKBENCH_NO_CHANGE,
    UI_WORKBENCH_UNAVAILABLE_ACTION,
    UI_WORKBENCH_INVALID_ARGUMENT,
    UI_WORKBENCH_LOAD_FAILED,
    UI_WORKBENCH_SAVE_FAILED
} UiWorkbenchResult;

typedef struct {
    UiCache cache;
    MapCatalog catalog;
    UiLayout *layout;
    UiElement *elements[UI_CACHE_MAX];
    int element_count;
    MenuId context;
    int element_index;
    bool editing;
    UiWorkbenchProperty property;
    UiWorkbenchMode mode;
    UiWorkbenchPreviewState preview_state;
    bool remove_choice;
    double preview_elapsed_ms;
    bool preview_reduced_motion;
    size_t add_index;
    size_t help_page;
    char text_edit[256];
    char status[128];
    char membership_undo_name[UI_ELE_NAME_MAX];
    MenuId membership_undo_context;
    bool membership_undo_add;
} UiWorkbench;

void ui_workbench_init(UiWorkbench *workbench);
bool ui_workbench_category_enabled(const UiWorkbench *workbench, UiWorkbenchProperty property);
void ui_workbench_cycle_preview(UiWorkbench *workbench);
void ui_workbench_destroy(UiWorkbench *workbench);
UiWorkbenchResult ui_workbench_open(UiWorkbench *workbench, MenuId context);
UiElement *ui_workbench_current_element(UiWorkbench *workbench);
UiWorkbenchResult ui_workbench_cycle_element(UiWorkbench *workbench, int direction);
UiWorkbenchResult ui_workbench_toggle_editing(UiWorkbench *workbench);
UiWorkbenchResult ui_workbench_cycle_property(UiWorkbench *workbench, int direction);
UiWorkbenchResult ui_workbench_move(UiWorkbench *workbench, int dx, int dy);
UiWorkbenchResult ui_workbench_cycle_value(UiWorkbench *workbench, int direction);
UiWorkbenchResult ui_workbench_invoke(UiWorkbench *workbench);
UiWorkbenchResult ui_workbench_begin_add(UiWorkbench *workbench);
UiWorkbenchResult ui_workbench_cycle_add_source(UiWorkbench *workbench, int direction);
UiWorkbenchResult ui_workbench_confirm_add(UiWorkbench *workbench);
UiWorkbenchResult ui_workbench_request_remove(UiWorkbench *workbench);
UiWorkbenchResult ui_workbench_confirm_remove(UiWorkbench *workbench);
UiWorkbenchResult ui_workbench_undo_membership(UiWorkbench *workbench);
void ui_workbench_cancel_mode(UiWorkbench *workbench);
UiWorkbenchResult ui_workbench_open_help(UiWorkbench *workbench);
void ui_workbench_cycle_help(UiWorkbench *workbench, int direction);
UiWorkbenchResult ui_workbench_begin_text(UiWorkbench *workbench);
UiWorkbenchResult ui_workbench_text_input(UiWorkbench *workbench, const char *text, bool backspace);
UiWorkbenchResult ui_workbench_confirm_text(UiWorkbench *workbench);
const char *ui_workbench_add_source_name(const UiWorkbench *workbench);
const char *ui_workbench_property_name(UiWorkbenchProperty property);
const char *ui_workbench_current_value(const UiWorkbench *workbench);

#endif