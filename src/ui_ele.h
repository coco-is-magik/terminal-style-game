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
#include <stddef.h>

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
    size_t content_capacity;
    int visible;
    int z_index;
    UiAlign align;
    bool has_fg;
    bool has_bg;
    SDL_Color fg;
    SDL_Color bg;
    char action[UI_ELE_NAME_MAX];
    bool focused;
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

/* ---- Element API ---- */

/**
 * ui_ele_load() — Load a single UI element definition from a file
 *
 * Parses an `assets/ui_elements/<name>.txt` file into a heap-allocated
 * UiElement.  Parent/child links are resolved later by the cache.
 *
 * @param path   Path to the element file
 * @param cache  Cache to register the loaded element into (may be NULL)
 * @return       New UiElement, or NULL on parse failure
 */
UiElement *ui_ele_load(const char *path, UiCache *cache);

/**
 * ui_ele_destroy() — Free a UI element and its children
 *
 * Recursively releases the element, its content string, and all cached
 * child references.  Safe to call with NULL.
 *
 * @param element  Element to destroy (may be NULL)
 */
void ui_ele_destroy(UiElement *element);

/**
 * ui_ele_set_content() — Replace the text content of an element
 *
 * The new text is duplicated on the heap.  Used for dynamic text slots
 * such as HUD values and status messages.
 *
 * @param element  Element whose content to update (must not be NULL)
 * @param content  New NUL-terminated text (may be empty)
 */
void ui_ele_set_content(UiElement *element, const char *content);
bool ui_ele_reserve_content(UiElement *element, size_t capacity);
bool ui_ele_set_content_bounded(UiElement *element, const char *content);
void ui_ele_set_colors(UiElement *element, SDL_Color fg, SDL_Color bg);
const char *ui_ele_get_action(const UiElement *element);

/**
 * ui_ele_render() — Draw an element and its children into the grid
 *
 * Renders the element at the given parent offset, then recursively renders
 * all children.  Containers and text/button elements are handled according
 * to their type and alignment.
 *
 * @param element    Element to render (must not be NULL)
 * @param grid       Destination grid
 * @param parent_x   Parent absolute X offset in grid cells
 * @param parent_y   Parent absolute Y offset in grid cells
 * @param fg         Default foreground colour to inherit
 * @param bg         Default background colour to inherit
 */
void ui_ele_render(UiElement *element, Grid *grid, int parent_x, int parent_y,
                   SDL_Color fg, SDL_Color bg);

/* ---- Cache API ---- */

/**
 * ui_cache_init() — Initialise a UI cache from a master map file
 *
 * Loads `assets/ui_layouts/master_map.txt` to discover the layout-element
 * associations used by the cache.  The cache must later be destroyed with
 * ui_cache_destroy().
 *
 * @param cache            Cache to initialise (must not be NULL)
 * @param master_map_path  Path to the master map file
 */
void ui_cache_init(UiCache *cache, const char *master_map_path);

/**
 * ui_cache_destroy() — Free all elements stored in a UI cache
 *
 * @param cache  Cache to destroy (may be NULL)
 */
void ui_cache_destroy(UiCache *cache);

/**
 * ui_cache_get() — Return a previously loaded element by name
 *
 * Looks up the element in the cache without loading from disk.  Returns
 * NULL if the name has not been loaded yet.
 *
 * @param cache  Cache to query (must not be NULL)
 * @param name   Element name to look up
 * @return       Cached UiElement, or NULL if not found
 */
UiElement *ui_cache_get(UiCache *cache, const char *name);

/**
 * ui_cache_load() — Load an element into the cache by name
 *
 * Searches `base_dir/<name>.txt`, loads it, and stores it in the cache.
 * If the element is already cached, the cached copy is returned.
 *
 * @param cache     Cache to load into (must not be NULL)
 * @param name      Element name (used as the filename stem)
 * @param base_dir  Directory containing the element files
 * @return          Loaded UiElement, or NULL on failure
 */
UiElement *ui_cache_load(UiCache *cache, const char *name, const char *base_dir);

/**
 * ui_cache_tick() — Load or refresh the elements needed by a layout
 *
 * Called each frame with the currently active layout name.  Ensures the
 * elements referenced by that layout are loaded into the cache.
 *
 * @param cache        Cache to update (must not be NULL)
 * @param active_layout  Name of the currently active layout
 * @param base_dir     Directory containing the element files
 */
void ui_cache_tick(UiCache *cache, const char *active_layout, const char *base_dir);

/* ---- Layout API ---- */

/**
 * ui_layout_load() — Load a layout from a file and resolve its elements
 *
 * Reads `assets/ui_layouts/<name>.txt` and loads the referenced elements
 * through the provided cache.
 *
 * @param path   Path to the layout file
 * @param cache  Cache to use for element lookups
 * @return       New UiLayout, or NULL on failure
 */
UiLayout *ui_layout_load(const char *path, UiCache *cache);

/**
 * ui_layout_substitute() — Replace a named slot in a layout with an element
 *
 * Used for dynamic content such as swapping tooltip text at runtime.
 *
 * @param layout     Layout to modify (must not be NULL)
 * @param slot_name  Name of the slot to replace
 * @param element    Element to insert into the slot (must not be NULL)
 */
void ui_layout_substitute(UiLayout *layout, const char *slot_name, UiElement *element);

/**
 * ui_layout_get_focused() — Return the focusable element at a given index
 *
 * Focusable elements are buttons.  The index is used for menu navigation.
 *
 * @param layout       Layout to query (must not be NULL)
 * @param focus_index  Zero-based index of the focusable element
 * @return             Focused UiElement, or NULL if index is out of range
 */
UiElement *ui_layout_get_focused(UiLayout *layout, int focus_index);

/**
 * ui_layout_set_focus() — Mark one button with a non-color-only focus indicator
 *
 * Clears focus from every button, then marks the requested focus index when it
 * exists. Passing an invalid index leaves all buttons unfocused.
 */
void ui_layout_set_focus(UiLayout *layout, int focus_index);

/**
 * ui_layout_focusable_count() — Count the focusable elements in a layout
 *
 * @param layout  Layout to query (must not be NULL)
 * @return        Number of focusable (button) elements
 */
int ui_layout_focusable_count(UiLayout *layout);

/**
 * ui_layout_render() — Render every element of a layout into the grid
 *
 * @param layout  Layout to render (must not be NULL)
 * @param grid    Destination grid
 * @param fg      Default foreground colour
 * @param bg      Default background colour
 */
void ui_layout_render(UiLayout *layout, Grid *grid, SDL_Color fg, SDL_Color bg);

/**
 * ui_layout_destroy() — Free a layout and its element references
 *
 * Does not destroy the cached elements themselves; the cache owns them.
 *
 * @param layout  Layout to destroy (may be NULL)
 */
void ui_layout_destroy(UiLayout *layout);

#endif /* UI_ELE_H */