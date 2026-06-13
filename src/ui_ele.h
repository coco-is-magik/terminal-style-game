/**
 * ui_ele.h — Data-driven terminal UI elements and layouts
 *
 * A UI element is a small, file-defined piece of terminal UI.  This proof of
 * concept supports containers and text elements, relative or absolute
 * positioning, simple cache lookup, layout composition, and wrapped text.
 */

#ifndef UI_ELE_H
#define UI_ELE_H

#include "grid.h"

#include <stdbool.h>

#define UI_ELE_NAME_MAX      64
#define UI_ELE_PATH_MAX     256
#define UI_ELE_MAX_CHILDREN  16
#define UI_CACHE_MAX         64
#define UI_LAYOUT_MAX_ELEMS  32
#define UI_LAYOUT_MAX_SLOTS  16
#define UI_MASTER_MAX        16

typedef enum {
    UI_ELE_CONTAINER = 0,
    UI_ELE_TEXT,
    UI_ELE_BUTTON
} UiElementType;

typedef enum {
    UI_COORD_ABSOLUTE = 0,
    UI_COORD_RELATIVE
} UiCoordMode;

typedef enum {
    UI_ALIGN_LEFT = 0,
    UI_ALIGN_CENTER,
    UI_ALIGN_RIGHT
} UiAlign;

typedef struct UiElement UiElement;

typedef struct {
    int x;
    int y;
    UiCoordMode coords_mode;
    int width;
    int height;
} UiElementLayout;

struct UiElement {
    char name[UI_ELE_NAME_MAX];
    UiElementType type;
    UiElementLayout layout;
    char parent_name[UI_ELE_NAME_MAX];
    UiElement *parent;
    char child_names[UI_ELE_MAX_CHILDREN][UI_ELE_NAME_MAX];
    UiElement *children[UI_ELE_MAX_CHILDREN];
    int child_count;
    char *content;
    int visible;
    int z_index;
    UiAlign align;
    bool has_fg;
    bool has_bg;
    SDL_Color fg;
    SDL_Color bg;
    char action[UI_ELE_NAME_MAX];
};

typedef struct {
    char layout[UI_ELE_NAME_MAX];
    char cache_next[UI_LAYOUT_MAX_ELEMS][UI_ELE_NAME_MAX];
    int cache_next_count;
    char drop_after[UI_LAYOUT_MAX_ELEMS][UI_ELE_NAME_MAX];
    int drop_after_count;
} UiMasterEntry;

typedef struct {
    UiElement *items[UI_CACHE_MAX];
    int count;
    UiMasterEntry master_entries[UI_MASTER_MAX];
    int master_count;
} UiCache;

typedef struct {
    char name[UI_ELE_NAME_MAX];
    UiElement *elements[UI_LAYOUT_MAX_ELEMS];
    int element_count;
    char slot_names[UI_LAYOUT_MAX_SLOTS][UI_ELE_NAME_MAX];
    UiElement *slot_elements[UI_LAYOUT_MAX_SLOTS];
    int slot_count;
} UiLayout;

UiElement *ui_ele_load(const char *path, UiCache *cache);
void ui_ele_destroy(UiElement *element);
void ui_ele_set_content(UiElement *element, const char *content);
void ui_ele_render(UiElement *element, Grid *grid, int parent_x, int parent_y,
                   SDL_Color fg, SDL_Color bg);

void ui_cache_init(UiCache *cache, const char *master_map_path);
void ui_cache_destroy(UiCache *cache);
UiElement *ui_cache_get(UiCache *cache, const char *name);
UiElement *ui_cache_load(UiCache *cache, const char *name, const char *base_dir);
void ui_cache_tick(UiCache *cache, const char *active_layout, const char *base_dir);

UiLayout *ui_layout_load(const char *path, UiCache *cache);
void ui_layout_substitute(UiLayout *layout, const char *slot_name, UiElement *element);
UiElement *ui_layout_get_focused(UiLayout *layout, int focus_index);
int ui_layout_focusable_count(UiLayout *layout);
void ui_layout_render(UiLayout *layout, Grid *grid, SDL_Color fg, SDL_Color bg);
void ui_layout_destroy(UiLayout *layout);

#endif /* UI_ELE_H */