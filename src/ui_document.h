/** ui_document.h — Staged authored game UI screen document. */
#ifndef UI_DOCUMENT_H
#define UI_DOCUMENT_H

#include "asset_document.h"
#include "flow_reference.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UI_DOCUMENT_VERSION 3U
#define UI_DOCUMENT_VERSION_V2 2U
#define UI_DOCUMENT_VERSION_V1 1U
#define UI_DOCUMENT_MAX_ELEMENTS 64U
#define UI_DOCUMENT_NAME_CAPACITY FLOW_NAME_CAPACITY
#define UI_DOCUMENT_CONTENT_CAPACITY 256U
#define UI_DOCUMENT_PATH_CAPACITY 1024U

typedef uint32_t UiElementId;

typedef enum {
    UI_DOCUMENT_KIND_MENU = 0
} UiDocumentKind;

typedef enum {
    UI_DOCUMENT_ELEMENT_CONTAINER = 0,
    UI_DOCUMENT_ELEMENT_TEXT,
    UI_DOCUMENT_ELEMENT_BUTTON
} UiDocumentElementType;

typedef enum {
    UI_DOCUMENT_ANCHOR_START = 0,
    UI_DOCUMENT_ANCHOR_CENTER,
    UI_DOCUMENT_ANCHOR_END,
    UI_DOCUMENT_ANCHOR_STRETCH
} UiDocumentAnchor;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    UiDocumentAnchor horizontal_anchor;
    UiDocumentAnchor vertical_anchor;
    int scale_percent;
} UiDocumentLayout;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
} UiDocumentColor;

typedef enum {
    UI_DOCUMENT_VISUAL_NATIVE = 0,
    UI_DOCUMENT_VISUAL_SPRITE
} UiDocumentVisualMode;

typedef enum {
    UI_DOCUMENT_ALIGN_LEFT = 0,
    UI_DOCUMENT_ALIGN_CENTER,
    UI_DOCUMENT_ALIGN_RIGHT
} UiDocumentAlign;

typedef struct {
    UiDocumentVisualMode mode;
    UiDocumentColor foreground;
    UiDocumentColor background;
    uint8_t fill_glyph;
    uint8_t border_glyph;
    uint16_t sprite_id;
    UiDocumentAlign align;
    bool fill_enabled;
    bool border_enabled;
    bool visible_by_default;
} UiDocumentVisual;

typedef struct {
    UiElementId id;
    UiElementId parent_id;
    UiDocumentElementType type;
    char name[UI_DOCUMENT_NAME_CAPACITY];
    char content[UI_DOCUMENT_CONTENT_CAPACITY];
    char flow_port[UI_DOCUMENT_NAME_CAPACITY];
    UiDocumentLayout layout;
    UiDocumentVisual visual;
} UiDocumentElement;

typedef struct {
    UiDocumentKind kind;
    char name[UI_DOCUMENT_NAME_CAPACITY];
    int design_width;
    int design_height;
    UiDocumentElement elements[UI_DOCUMENT_MAX_ELEMENTS];
    size_t element_count;
    UiElementId next_element_id;
    AssetDocumentState state;
    char path[UI_DOCUMENT_PATH_CAPACITY];
} UiDocument;

typedef struct {
    const char *ports[FLOW_REFERENCE_MAX_PORTS];
    FlowReferenceEntry entry;
} UiFlowReferenceView;

typedef enum {
    UI_DOCUMENT_OK = 0,
    UI_DOCUMENT_INVALID_ARGUMENT,
    UI_DOCUMENT_INVALID_NAME,
    UI_DOCUMENT_INVALID_CONTENT,
    UI_DOCUMENT_INVALID_TYPE,
    UI_DOCUMENT_FULL,
    UI_DOCUMENT_ID_EXHAUSTED,
    UI_DOCUMENT_DUPLICATE_ID,
    UI_DOCUMENT_DUPLICATE_NAME,
    UI_DOCUMENT_INVALID_ROOT,
    UI_DOCUMENT_MISSING_PARENT,
    UI_DOCUMENT_PARENT_NOT_CONTAINER,
    UI_DOCUMENT_PARENT_CYCLE,
    UI_DOCUMENT_INVALID_PORT,
    UI_DOCUMENT_DUPLICATE_PORT,
    UI_DOCUMENT_TOO_MANY_PORTS,
    UI_DOCUMENT_INVALID_LAYOUT,
    UI_DOCUMENT_INVALID_VISUAL,
    UI_DOCUMENT_PARSE_ERROR,
    UI_DOCUMENT_UNSUPPORTED_VERSION,
    UI_DOCUMENT_NO_PATH,
    UI_DOCUMENT_IO_ERROR
} UiDocumentResult;

void ui_document_init(UiDocument *document);
bool ui_document_is_dirty(const UiDocument *document);
const UiDocumentElement *ui_document_find_element(const UiDocument *document,
                                                  UiElementId id);
UiDocumentResult ui_document_create_menu(UiDocument *document, const char *name);
UiDocumentResult ui_document_set_design_size(UiDocument *document,
                                             int width, int height);
UiDocumentResult ui_document_add_element(UiDocument *document,
                                         UiDocumentElementType type,
                                         UiElementId parent_id,
                                         const char *name,
                                         const char *content,
                                         const char *flow_port,
                                         UiElementId *out_id);
UiDocumentResult ui_document_set_layout(UiDocument *document,
                                        UiElementId element_id,
                                        UiDocumentLayout layout);
UiDocumentResult ui_document_set_visual(UiDocument *document,
                                        UiElementId element_id,
                                        UiDocumentVisual visual);
UiDocumentResult ui_document_set_content(UiDocument *document,
                                         UiElementId element_id,
                                         const char *content);
UiDocumentResult ui_document_set_flow_port(UiDocument *document,
                                           UiElementId element_id,
                                           const char *flow_port);
UiDocumentResult ui_document_rename_element(UiDocument *document,
                                            UiElementId element_id,
                                            const char *name);
UiDocumentResult ui_document_reparent_subtree(UiDocument *document,
                                              UiElementId element_id,
                                              UiElementId new_parent_id);
UiDocumentResult ui_document_move_subtree_earlier(UiDocument *document,
                                                  UiElementId element_id);
UiDocumentResult ui_document_move_subtree_later(UiDocument *document,
                                                UiElementId element_id);
UiDocumentResult ui_document_remove_subtree(UiDocument *document,
                                            UiElementId element_id);
UiDocumentResult ui_document_validate(const UiDocument *document);
UiDocumentResult ui_document_build_flow_reference(
    const UiDocument *document,
    UiFlowReferenceView *out_view
);
UiDocumentResult ui_document_load(UiDocument *document, const char *path);
UiDocumentResult ui_document_save_as(UiDocument *document, const char *path);
UiDocumentResult ui_document_save(UiDocument *document);

#endif