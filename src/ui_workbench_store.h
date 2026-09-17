/** ui_workbench_store.h — Atomic canonical writes for application UI elements. */
#ifndef UI_WORKBENCH_STORE_H
#define UI_WORKBENCH_STORE_H

#include "ui_ele.h"

typedef enum {
    UI_WORKBENCH_STORE_OK = 0,
    UI_WORKBENCH_STORE_OK_DURABILITY_WARNING,
    UI_WORKBENCH_STORE_INVALID_ARGUMENT,
    UI_WORKBENCH_STORE_INVALID_ELEMENT,
    UI_WORKBENCH_STORE_IO_ERROR
} UiWorkbenchStoreResult;

bool ui_workbench_element_is_valid(const UiElement *element);
UiWorkbenchStoreResult ui_workbench_store_element(const UiElement *element,
                                                   const char *path);

#endif