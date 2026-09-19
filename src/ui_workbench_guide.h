/** ui_workbench_guide.h — Application-UI editor in-editor guidance contract. */
#ifndef UI_WORKBENCH_GUIDE_H
#define UI_WORKBENCH_GUIDE_H

#include "grid.h"
#include "ui_workbench.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Small owned guide entry and footer layout contract. Guidance text lives in
 * authored data (assets/editor_tooltips.txt, key=text format) and is
 * resolved once into an owned snapshot. The editor footer always shows the
 * active status plus the context-sensitive tooltip for the highlighted
 * property or the active mode. No preview cell is ever written by this
 * module.
 */
#define UI_WORKBENCH_GUIDE_KEY_MAX 64
#define UI_WORKBENCH_GUIDE_TEXT_MAX 192
#define UI_WORKBENCH_GUIDE_ENTRY_MAX 96

typedef struct {
    char key[UI_WORKBENCH_GUIDE_KEY_MAX];
    char text[UI_WORKBENCH_GUIDE_TEXT_MAX];
} UiWorkbenchGuideEntry;

typedef struct {
    UiWorkbenchGuideEntry entries[UI_WORKBENCH_GUIDE_ENTRY_MAX];
    size_t count;
    bool loaded;
} UiWorkbenchGuide;

typedef enum {
    UI_WORKBENCH_GUIDE_OK = 0,
    UI_WORKBENCH_GUIDE_INVALID_ARGUMENT,
    UI_WORKBENCH_GUIDE_LOAD_FAILED
} UiWorkbenchGuideResult;

const char *ui_workbench_guide_result_string(UiWorkbenchGuideResult result);
UiWorkbenchGuideResult ui_workbench_guide_load(UiWorkbenchGuide *guide,
                                               const char *path);
void ui_workbench_guide_destroy(UiWorkbenchGuide *guide);
const char *ui_workbench_guide_tooltip(const UiWorkbenchGuide *guide,
                                       const UiWorkbench *workbench);
size_t ui_workbench_guide_missing_count(const UiWorkbenchGuide *guide,
                                        const UiWorkbench *workbench);
bool ui_workbench_guide_footer_stable(const Grid *grid, int grid_columns,
                                      int grid_rows);

#endif /* UI_WORKBENCH_GUIDE_H */