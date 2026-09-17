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
    UI_WORKBENCH_PROPERTY_COUNT
} UiWorkbenchProperty;

typedef enum {
    UI_WORKBENCH_MODE_BROWSE = 0,
    UI_WORKBENCH_MODE_ADD,
    UI_WORKBENCH_MODE_REMOVE_CONFIRM
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
    size_t add_index;
    char status[128];
} UiWorkbench;

void ui_workbench_init(UiWorkbench *workbench);
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
void ui_workbench_cancel_mode(UiWorkbench *workbench);
const char *ui_workbench_add_source_name(const UiWorkbench *workbench);
const char *ui_workbench_property_name(UiWorkbenchProperty property);
const char *ui_workbench_current_value(const UiWorkbench *workbench);

#endif