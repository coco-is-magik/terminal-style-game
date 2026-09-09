#define _POSIX_C_SOURCE 200809L

#include "ui_document.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool valid_name(const char *name) {
    size_t i;
    size_t length;
    if (!name || name[0] == '\0') return false;
    length = strlen(name);
    if (length >= UI_DOCUMENT_NAME_CAPACITY) return false;
    for (i = 0U; i < length; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!(isalnum(c) || c == '_' || c == '-')) return false;
    }
    return true;
}

static bool valid_content(const char *content) {
    size_t i;
    size_t length;
    if (!content) return false;
    length = strlen(content);
    if (length >= UI_DOCUMENT_CONTENT_CAPACITY) return false;
    for (i = 0U; i < length; i++) {
        unsigned char c = (unsigned char)content[i];
        if (c < 32U || c > 126U || c == '\\') return false;
    }
    return true;
}

static bool copy_string(char *destination, size_t capacity, const char *source) {
    size_t length;
    if (!destination || !source || capacity == 0U) return false;
    length = strlen(source);
    if (length >= capacity) return false;
    if (destination != source) memcpy(destination, source, length + 1U);
    return true;
}

static const char *type_name(UiDocumentElementType type) {
    if (type == UI_DOCUMENT_ELEMENT_CONTAINER) return "container";
    if (type == UI_DOCUMENT_ELEMENT_TEXT) return "text";
    if (type == UI_DOCUMENT_ELEMENT_BUTTON) return "button";
    return NULL;
}

static bool parse_type(const char *text, UiDocumentElementType *out_type) {
    if (!text || !out_type) return false;
    if (strcmp(text, "container") == 0) *out_type = UI_DOCUMENT_ELEMENT_CONTAINER;
    else if (strcmp(text, "text") == 0) *out_type = UI_DOCUMENT_ELEMENT_TEXT;
    else if (strcmp(text, "button") == 0) *out_type = UI_DOCUMENT_ELEMENT_BUTTON;
    else return false;
    return true;
}

static bool parse_u32(const char *text, uint32_t *out_value, bool allow_zero) {
    char *end = NULL;
    unsigned long value;
    if (!text || !out_value || text[0] == '\0' || text[0] == '-') return false;
    value = strtoul(text, &end, 10);
    if (!end || *end != '\0' || value > UINT32_MAX || (!allow_zero && value == 0UL))
        return false;
    *out_value = (uint32_t)value;
    return true;
}

static bool parse_int(const char *text, int *out_value) {
    char *end = NULL;
    long value;
    if (!text || !out_value || text[0] == '\0') return false;
    value = strtol(text, &end, 10);
    if (!end || *end != '\0' || value < INT_MIN || value > INT_MAX) return false;
    *out_value = (int)value;
    return true;
}

static const char *anchor_name(UiDocumentAnchor anchor) {
    if (anchor == UI_DOCUMENT_ANCHOR_START) return "start";
    if (anchor == UI_DOCUMENT_ANCHOR_CENTER) return "center";
    if (anchor == UI_DOCUMENT_ANCHOR_END) return "end";
    if (anchor == UI_DOCUMENT_ANCHOR_STRETCH) return "stretch";
    return NULL;
}

static bool parse_anchor(const char *text, UiDocumentAnchor *out_anchor) {
    if (!text || !out_anchor) return false;
    if (strcmp(text, "start") == 0) *out_anchor = UI_DOCUMENT_ANCHOR_START;
    else if (strcmp(text, "center") == 0) *out_anchor = UI_DOCUMENT_ANCHOR_CENTER;
    else if (strcmp(text, "end") == 0) *out_anchor = UI_DOCUMENT_ANCHOR_END;
    else if (strcmp(text, "stretch") == 0) *out_anchor = UI_DOCUMENT_ANCHOR_STRETCH;
    else return false;
    return true;
}

static UiDocumentLayout default_element_layout(void) {
    UiDocumentLayout layout = {0, 0, 1, 1, UI_DOCUMENT_ANCHOR_START,
                               UI_DOCUMENT_ANCHOR_START, 100};
    return layout;
}

static bool layout_valid(UiDocumentLayout layout) {
    bool horizontal_stretch;
    bool vertical_stretch;
    if (layout.horizontal_anchor < UI_DOCUMENT_ANCHOR_START ||
        layout.horizontal_anchor > UI_DOCUMENT_ANCHOR_STRETCH ||
        layout.vertical_anchor < UI_DOCUMENT_ANCHOR_START ||
        layout.vertical_anchor > UI_DOCUMENT_ANCHOR_STRETCH ||
        layout.scale_percent < 25 || layout.scale_percent > 400) return false;
    horizontal_stretch = layout.horizontal_anchor == UI_DOCUMENT_ANCHOR_STRETCH;
    vertical_stretch = layout.vertical_anchor == UI_DOCUMENT_ANCHOR_STRETCH;
    if ((horizontal_stretch && (layout.x < 0 || layout.width < 0)) ||
        (!horizontal_stretch && layout.width <= 0) ||
        (vertical_stretch && (layout.y < 0 || layout.height < 0)) ||
        (!vertical_stretch && layout.height <= 0)) return false;
    return true;
}

static UiDocumentVisual default_visual(UiDocumentElementType type) {
    UiDocumentVisual visual;
    memset(&visual, 0, sizeof(visual));
    visual.mode = UI_DOCUMENT_VISUAL_NATIVE;
    visual.foreground = (UiDocumentColor){255U, 255U, 255U, 255U};
    visual.background = (UiDocumentColor){0U, 0U, 0U, 255U};
    visual.fill_glyph = (uint8_t)' ';
    visual.border_glyph = (uint8_t)'#';
    visual.align = UI_DOCUMENT_ALIGN_LEFT;
    visual.fill_enabled = type == UI_DOCUMENT_ELEMENT_CONTAINER;
    visual.border_enabled = false;
    visual.visible_by_default = true;
    return visual;
}

static bool visual_valid(UiDocumentVisual visual) {
    if (visual.mode < UI_DOCUMENT_VISUAL_NATIVE ||
        visual.mode > UI_DOCUMENT_VISUAL_SPRITE ||
        visual.align < UI_DOCUMENT_ALIGN_LEFT ||
        visual.align > UI_DOCUMENT_ALIGN_RIGHT) return false;
    if (visual.mode == UI_DOCUMENT_VISUAL_SPRITE)
        return visual.sprite_id > 0U && visual.sprite_id <= UINT8_MAX;
    return visual.sprite_id == 0U;
}

static const char *visual_mode_name(UiDocumentVisualMode mode) {
    return mode == UI_DOCUMENT_VISUAL_NATIVE ? "native" :
           mode == UI_DOCUMENT_VISUAL_SPRITE ? "sprite" : NULL;
}

static bool parse_visual_mode(const char *text, UiDocumentVisualMode *out_mode) {
    if (!text || !out_mode) return false;
    if (strcmp(text, "native") == 0) *out_mode = UI_DOCUMENT_VISUAL_NATIVE;
    else if (strcmp(text, "sprite") == 0) *out_mode = UI_DOCUMENT_VISUAL_SPRITE;
    else return false;
    return true;
}

static const char *align_name(UiDocumentAlign align) {
    return align == UI_DOCUMENT_ALIGN_LEFT ? "left" :
           align == UI_DOCUMENT_ALIGN_CENTER ? "center" :
           align == UI_DOCUMENT_ALIGN_RIGHT ? "right" : NULL;
}

static bool parse_align(const char *text, UiDocumentAlign *out_align) {
    if (!text || !out_align) return false;
    if (strcmp(text, "left") == 0) *out_align = UI_DOCUMENT_ALIGN_LEFT;
    else if (strcmp(text, "center") == 0) *out_align = UI_DOCUMENT_ALIGN_CENTER;
    else if (strcmp(text, "right") == 0) *out_align = UI_DOCUMENT_ALIGN_RIGHT;
    else return false;
    return true;
}

static bool parse_color(const char *text, UiDocumentColor *out_color) {
    unsigned red, green, blue, alpha;
    char tail;
    if (!text || !out_color ||
        sscanf(text, "%u,%u,%u,%u%c", &red, &green, &blue, &alpha, &tail) != 4 ||
        red > 255U || green > 255U || blue > 255U || alpha > 255U) return false;
    *out_color = (UiDocumentColor){(uint8_t)red, (uint8_t)green,
                                   (uint8_t)blue, (uint8_t)alpha};
    return true;
}

static bool parse_bool(const char *text, bool *out_value) {
    if (!text || !out_value) return false;
    if (strcmp(text, "0") == 0) *out_value = false;
    else if (strcmp(text, "1") == 0) *out_value = true;
    else return false;
    return true;
}

static bool parse_u8(const char *text, uint8_t *out_value) {
    uint32_t value;
    if (!parse_u32(text, &value, true) || value > UINT8_MAX) return false;
    *out_value = (uint8_t)value;
    return true;
}

static bool parse_u16(const char *text, uint16_t *out_value) {
    uint32_t value;
    if (!parse_u32(text, &value, true) || value > UINT16_MAX) return false;
    *out_value = (uint16_t)value;
    return true;
}

static void trim_line(char *text) {
    size_t length;
    if (!text) return;
    length = strlen(text);
    while (length > 0U && (text[length - 1U] == '\n' ||
           text[length - 1U] == '\r')) text[--length] = '\0';
}

static bool element_fields_complete(
    uint32_t version,
    bool structural,
    bool layout,
    bool visual
) {
    if (!structural) return false;
    if (version == UI_DOCUMENT_VERSION_V1) return true;
    if (!layout) return false;
    if (version == UI_DOCUMENT_VERSION_V2) return true;
    return version == UI_DOCUMENT_VERSION && visual;
}

void ui_document_init(UiDocument *document) {
    if (!document) return;
    memset(document, 0, sizeof(*document));
    document->kind = UI_DOCUMENT_KIND_MENU;
    document->design_width = 80;
    document->design_height = 25;
    document->elements[0].id = 1U;
    document->elements[0].type = UI_DOCUMENT_ELEMENT_CONTAINER;
    (void)copy_string(document->elements[0].name,
                      sizeof(document->elements[0].name), "root");
    document->elements[0].layout = (UiDocumentLayout){
        0, 0, 80, 25, UI_DOCUMENT_ANCHOR_STRETCH,
        UI_DOCUMENT_ANCHOR_STRETCH, 100};
    document->elements[0].visual = default_visual(UI_DOCUMENT_ELEMENT_CONTAINER);
    document->element_count = 1U;
    document->next_element_id = 2U;
    asset_document_state_init(&document->state);
}

bool ui_document_is_dirty(const UiDocument *document) {
    return document && asset_document_state_is_dirty(&document->state);
}

const UiDocumentElement *ui_document_find_element(const UiDocument *document,
                                                  UiElementId id) {
    size_t i;
    if (!document || id == 0U) return NULL;
    for (i = 0U; i < document->element_count; i++)
        if (document->elements[i].id == id) return &document->elements[i];
    return NULL;
}

UiDocumentResult ui_document_create_menu(UiDocument *document, const char *name) {
    if (!document) return UI_DOCUMENT_INVALID_ARGUMENT;
    if (!valid_name(name)) return UI_DOCUMENT_INVALID_NAME;
    ui_document_init(document);
    (void)copy_string(document->name, sizeof(document->name), name);
    (void)asset_document_state_advance(&document->state);
    return UI_DOCUMENT_OK;
}

UiDocumentResult ui_document_set_design_size(UiDocument *document,
                                             int width, int height) {
    int old_width;
    int old_height;
    UiDocumentLayout old_root;
    if (!document) return UI_DOCUMENT_INVALID_ARGUMENT;
    if (ui_document_validate(document) != UI_DOCUMENT_OK)
        return UI_DOCUMENT_INVALID_ARGUMENT;
    if (width <= 0 || height <= 0) return UI_DOCUMENT_INVALID_LAYOUT;
    if (document->design_width == width && document->design_height == height)
        return UI_DOCUMENT_OK;
    old_width = document->design_width;
    old_height = document->design_height;
    old_root = document->elements[0].layout;
    document->design_width = width;
    document->design_height = height;
    document->elements[0].layout.width = width;
    document->elements[0].layout.height = height;
    if (ui_document_validate(document) != UI_DOCUMENT_OK ||
        !asset_document_state_advance(&document->state)) {
        document->design_width = old_width;
        document->design_height = old_height;
        document->elements[0].layout = old_root;
        return UI_DOCUMENT_ID_EXHAUSTED;
    }
    return UI_DOCUMENT_OK;
}

UiDocumentResult ui_document_add_element(UiDocument *document,
                                         UiDocumentElementType type,
                                         UiElementId parent_id,
                                         const char *name,
                                         const char *content,
                                         const char *flow_port,
                                         UiElementId *out_id) {
    UiDocumentElement *element;
    const UiDocumentElement *parent;
    UiDocumentResult validation;
    if (out_id) *out_id = 0U;
    if (!document || !out_id) return UI_DOCUMENT_INVALID_ARGUMENT;
    if (type < UI_DOCUMENT_ELEMENT_CONTAINER || type > UI_DOCUMENT_ELEMENT_BUTTON)
        return UI_DOCUMENT_INVALID_TYPE;
    if (!valid_name(name)) return UI_DOCUMENT_INVALID_NAME;
    if (!valid_content(content)) return UI_DOCUMENT_INVALID_CONTENT;
    if ((type == UI_DOCUMENT_ELEMENT_BUTTON && !valid_name(flow_port)) ||
        (type != UI_DOCUMENT_ELEMENT_BUTTON && (!flow_port || flow_port[0] != '\0')))
        return UI_DOCUMENT_INVALID_PORT;
    parent = ui_document_find_element(document, parent_id);
    if (!parent) return UI_DOCUMENT_MISSING_PARENT;
    if (parent->type != UI_DOCUMENT_ELEMENT_CONTAINER)
        return UI_DOCUMENT_PARENT_NOT_CONTAINER;
    if (document->element_count >= UI_DOCUMENT_MAX_ELEMENTS) return UI_DOCUMENT_FULL;
    if (document->next_element_id == 0U || document->next_element_id == UINT32_MAX)
        return UI_DOCUMENT_ID_EXHAUSTED;
    element = &document->elements[document->element_count];
    memset(element, 0, sizeof(*element));
    element->id = document->next_element_id;
    element->parent_id = parent_id;
    element->type = type;
    (void)copy_string(element->name, sizeof(element->name), name);
    (void)copy_string(element->content, sizeof(element->content), content);
    if (type == UI_DOCUMENT_ELEMENT_BUTTON)
        (void)copy_string(element->flow_port, sizeof(element->flow_port), flow_port);
    element->layout = default_element_layout();
    element->visual = default_visual(type);
    document->element_count++;
    document->next_element_id++;
    validation = ui_document_validate(document);
    if (validation != UI_DOCUMENT_OK || !asset_document_state_advance(&document->state)) {
        document->element_count--;
        document->next_element_id--;
        memset(element, 0, sizeof(*element));
        return validation != UI_DOCUMENT_OK ? validation : UI_DOCUMENT_ID_EXHAUSTED;
    }
    *out_id = element->id;
    return UI_DOCUMENT_OK;
}

UiDocumentResult ui_document_set_layout(UiDocument *document,
                                        UiElementId element_id,
                                        UiDocumentLayout layout) {
    UiDocumentElement *element = NULL;
    UiDocumentLayout previous;
    size_t i;
    if (!document) return UI_DOCUMENT_INVALID_ARGUMENT;
    if (ui_document_validate(document) != UI_DOCUMENT_OK)
        return UI_DOCUMENT_INVALID_ARGUMENT;
    if (element_id == 1U || !layout_valid(layout)) return UI_DOCUMENT_INVALID_LAYOUT;
    for (i = 0U; i < document->element_count; i++)
        if (document->elements[i].id == element_id) {
            element = &document->elements[i];
            break;
        }
    if (!element) return UI_DOCUMENT_INVALID_ARGUMENT;
    if (memcmp(&element->layout, &layout, sizeof(layout)) == 0) return UI_DOCUMENT_OK;
    previous = element->layout;
    element->layout = layout;
    if (ui_document_validate(document) != UI_DOCUMENT_OK ||
        !asset_document_state_advance(&document->state)) {
        element->layout = previous;
        return UI_DOCUMENT_ID_EXHAUSTED;
    }
    return UI_DOCUMENT_OK;
}

UiDocumentResult ui_document_set_visual(UiDocument *document,
                                        UiElementId element_id,
                                        UiDocumentVisual visual) {
    UiDocumentElement *element = NULL;
    UiDocumentVisual previous;
    size_t i;
    if (!document) return UI_DOCUMENT_INVALID_ARGUMENT;
    if (ui_document_validate(document) != UI_DOCUMENT_OK)
        return UI_DOCUMENT_INVALID_ARGUMENT;
    if (!visual_valid(visual)) return UI_DOCUMENT_INVALID_VISUAL;
    for (i = 0U; i < document->element_count; i++)
        if (document->elements[i].id == element_id) {
            element = &document->elements[i];
            break;
        }
    if (!element) return UI_DOCUMENT_INVALID_ARGUMENT;
    if (memcmp(&element->visual, &visual, sizeof(visual)) == 0)
        return UI_DOCUMENT_OK;
    previous = element->visual;
    element->visual = visual;
    if (ui_document_validate(document) != UI_DOCUMENT_OK ||
        !asset_document_state_advance(&document->state)) {
        element->visual = previous;
        return UI_DOCUMENT_ID_EXHAUSTED;
    }
    return UI_DOCUMENT_OK;
}

UiDocumentResult ui_document_validate(const UiDocument *document) {
    size_t i;
    size_t j;
    size_t root_count = 0U;
    size_t button_count = 0U;
    if (!document || document->kind != UI_DOCUMENT_KIND_MENU ||
        !valid_name(document->name) || document->element_count == 0U ||
        document->element_count > UI_DOCUMENT_MAX_ELEMENTS ||
        document->design_width <= 0 || document->design_height <= 0)
        return UI_DOCUMENT_INVALID_ARGUMENT;
    for (i = 0U; i < document->element_count; i++) {
        const UiDocumentElement *element = &document->elements[i];
        if (element->id == 0U || element->id >= document->next_element_id)
            return UI_DOCUMENT_DUPLICATE_ID;
        if (!valid_name(element->name)) return UI_DOCUMENT_INVALID_NAME;
        if (!valid_content(element->content)) return UI_DOCUMENT_INVALID_CONTENT;
        if (element->type < UI_DOCUMENT_ELEMENT_CONTAINER ||
            element->type > UI_DOCUMENT_ELEMENT_BUTTON) return UI_DOCUMENT_INVALID_TYPE;
        if (!layout_valid(element->layout)) return UI_DOCUMENT_INVALID_LAYOUT;
        if (!visual_valid(element->visual)) return UI_DOCUMENT_INVALID_VISUAL;
        for (j = 0U; j < i; j++) {
            if (document->elements[j].id == element->id)
                return UI_DOCUMENT_DUPLICATE_ID;
            if (strcmp(document->elements[j].name, element->name) == 0)
                return UI_DOCUMENT_DUPLICATE_NAME;
            if (element->type == UI_DOCUMENT_ELEMENT_BUTTON &&
                document->elements[j].type == UI_DOCUMENT_ELEMENT_BUTTON &&
                strcmp(document->elements[j].flow_port, element->flow_port) == 0)
                return UI_DOCUMENT_DUPLICATE_PORT;
        }
        if (element->parent_id == 0U) {
            if (element->id != 1U ||
                element->type != UI_DOCUMENT_ELEMENT_CONTAINER ||
                strcmp(element->name, "root") != 0 || element->layout.x != 0 ||
                element->layout.y != 0 ||
                element->layout.width != document->design_width ||
                element->layout.height != document->design_height ||
                element->layout.horizontal_anchor != UI_DOCUMENT_ANCHOR_STRETCH ||
                element->layout.vertical_anchor != UI_DOCUMENT_ANCHOR_STRETCH ||
                element->layout.scale_percent != 100)
                return UI_DOCUMENT_INVALID_ROOT;
            root_count++;
        } else {
            const UiDocumentElement *parent = ui_document_find_element(
                document, element->parent_id);
            size_t depth = 0U;
            if (!parent) return UI_DOCUMENT_MISSING_PARENT;
            if (parent->type != UI_DOCUMENT_ELEMENT_CONTAINER)
                return UI_DOCUMENT_PARENT_NOT_CONTAINER;
            while (parent->parent_id != 0U) {
                if (++depth >= document->element_count)
                    return UI_DOCUMENT_PARENT_CYCLE;
                parent = ui_document_find_element(document, parent->parent_id);
                if (!parent) return UI_DOCUMENT_MISSING_PARENT;
            }
            if (strcmp(parent->name, "root") != 0)
                return UI_DOCUMENT_INVALID_ROOT;
        }
        if (element->type == UI_DOCUMENT_ELEMENT_BUTTON) {
            if (!valid_name(element->flow_port)) return UI_DOCUMENT_INVALID_PORT;
            if (++button_count > FLOW_REFERENCE_MAX_PORTS)
                return UI_DOCUMENT_TOO_MANY_PORTS;
        } else if (element->flow_port[0] != '\0') return UI_DOCUMENT_INVALID_PORT;
    }
    return root_count == 1U ? UI_DOCUMENT_OK : UI_DOCUMENT_INVALID_ROOT;
}

UiDocumentResult ui_document_build_flow_reference(
    const UiDocument *document,
    UiFlowReferenceView *out_view
) {
    size_t i;
    UiDocumentResult validation;
    if (!document || !out_view) return UI_DOCUMENT_INVALID_ARGUMENT;
    validation = ui_document_validate(document);
    if (validation != UI_DOCUMENT_OK) return validation;
    memset(out_view, 0, sizeof(*out_view));
    for (i = 0U; i < document->element_count; i++) {
        if (document->elements[i].type != UI_DOCUMENT_ELEMENT_BUTTON) continue;
        if (out_view->entry.port_count >= FLOW_REFERENCE_MAX_PORTS)
            return UI_DOCUMENT_TOO_MANY_PORTS;
        out_view->ports[out_view->entry.port_count++] = document->elements[i].flow_port;
    }
    out_view->entry.type = FLOW_NODE_MENU;
    out_view->entry.name = document->name;
    out_view->entry.ports = out_view->ports;
    return UI_DOCUMENT_OK;
}

static bool write_document(FILE *file, const UiDocument *document) {
    size_t i;
    if (fprintf(file, "ui_version=%u\nkind=menu\nname=%s\n"
                "design_width=%d\ndesign_height=%d\nnext_element_id=%u\n",
                UI_DOCUMENT_VERSION, document->name,
                document->design_width, document->design_height,
                document->next_element_id) < 0) return false;
    for (i = 0U; i < document->element_count; i++) {
        const UiDocumentElement *element = &document->elements[i];
        const char *type = type_name(element->type);
        if (!type || fprintf(file,
            "[element]\nid=%u\nparent=%u\ntype=%s\nname=%s\ncontent=%s\nport=%s\n"
            "x=%d\ny=%d\nwidth=%d\nheight=%d\nh_anchor=%s\nv_anchor=%s\nscale=%d\n"
            "visual=%s\nfg=%u,%u,%u,%u\nbg=%u,%u,%u,%u\nfill_enabled=%u\n"
            "fill_glyph=%u\nborder_enabled=%u\nborder_glyph=%u\nsprite_id=%u\n"
            "align=%s\nvisible=%u\n",
            element->id, element->parent_id, type, element->name,
            element->content, element->flow_port, element->layout.x,
            element->layout.y, element->layout.width, element->layout.height,
            anchor_name(element->layout.horizontal_anchor),
            anchor_name(element->layout.vertical_anchor),
            element->layout.scale_percent, visual_mode_name(element->visual.mode),
            element->visual.foreground.red, element->visual.foreground.green,
            element->visual.foreground.blue, element->visual.foreground.alpha,
            element->visual.background.red, element->visual.background.green,
            element->visual.background.blue, element->visual.background.alpha,
            element->visual.fill_enabled ? 1U : 0U, element->visual.fill_glyph,
            element->visual.border_enabled ? 1U : 0U, element->visual.border_glyph,
            element->visual.sprite_id, align_name(element->visual.align),
            element->visual.visible_by_default ? 1U : 0U) < 0) return false;
    }
    return true;
}

UiDocumentResult ui_document_save_as(UiDocument *document, const char *path) {
    char temporary[UI_DOCUMENT_PATH_CAPACITY + 32U];
    int fd;
    FILE *file;
    bool failed = false;
    UiDocumentResult validation;
    if (!document || !path || path[0] == '\0' ||
        strlen(path) >= UI_DOCUMENT_PATH_CAPACITY) return UI_DOCUMENT_INVALID_ARGUMENT;
    validation = ui_document_validate(document);
    if (validation != UI_DOCUMENT_OK) return validation;
    if (snprintf(temporary, sizeof(temporary), "%s.tmp.XXXXXX", path) >=
        (int)sizeof(temporary)) return UI_DOCUMENT_INVALID_ARGUMENT;
    fd = mkstemp(temporary);
    if (fd < 0) return UI_DOCUMENT_IO_ERROR;
    file = fdopen(fd, "w");
    if (!file) {
        (void)close(fd); (void)unlink(temporary);
        return UI_DOCUMENT_IO_ERROR;
    }
    if (!write_document(file, document)) failed = true;
    if (!failed && fflush(file) != 0) failed = true;
    if (!failed && fsync(fd) != 0) failed = true;
    if (fclose(file) != 0) failed = true;
    if (!failed && rename(temporary, path) != 0) failed = true;
    if (failed) {
        (void)unlink(temporary);
        return UI_DOCUMENT_IO_ERROR;
    }
    (void)copy_string(document->path, sizeof(document->path), path);
    asset_document_state_mark_saved(&document->state);
    return UI_DOCUMENT_OK;
}

UiDocumentResult ui_document_save(UiDocument *document) {
    if (!document) return UI_DOCUMENT_INVALID_ARGUMENT;
    if (document->path[0] == '\0') return UI_DOCUMENT_NO_PATH;
    return ui_document_save_as(document, document->path);
}

static UiDocumentResult parse_file(FILE *file, UiDocument *candidate) {
    enum { ROOT, ELEMENT } section = ROOT;
    char line[512];
    uint32_t version = 0U;
    bool have_version = false, have_kind = false, have_name = false, have_next = false;
    bool have_design_width = false, have_design_height = false;
    bool have_id = false, have_parent = false, have_type = false;
    bool have_element_name = false, have_content = false, have_port = false;
    bool have_x = false, have_y = false, have_width = false, have_height = false;
    bool have_h_anchor = false, have_v_anchor = false, have_scale = false;
    bool saw_v2_only_field = false;
    bool have_visual = false, have_fg = false, have_bg = false;
    bool have_fill_enabled = false, have_fill_glyph = false;
    bool have_border_enabled = false, have_border_glyph = false;
    bool have_sprite_id = false, have_align = false, have_visible = false;
    bool saw_v3_only_field = false;
    memset(candidate, 0, sizeof(*candidate));
    while (fgets(line, sizeof(line), file)) {
        char *separator;
        char *key;
        char *value;
        trim_line(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        if (strcmp(line, "[element]") == 0) {
            if (section == ELEMENT && !element_fields_complete(version,
                have_id && have_parent && have_type && have_element_name &&
                    have_content && have_port,
                have_x && have_y && have_width && have_height && have_h_anchor &&
                    have_v_anchor && have_scale,
                have_visual && have_fg && have_bg && have_fill_enabled &&
                    have_fill_glyph && have_border_enabled && have_border_glyph &&
                    have_sprite_id && have_align && have_visible))
                return UI_DOCUMENT_PARSE_ERROR;
            if (candidate->element_count >= UI_DOCUMENT_MAX_ELEMENTS)
                return UI_DOCUMENT_FULL;
            section = ELEMENT;
            have_id = have_parent = have_type = have_element_name = false;
            have_content = have_port = false;
            have_x = have_y = have_width = have_height = false;
            have_h_anchor = have_v_anchor = have_scale = false;
            have_visual = have_fg = have_bg = false;
            have_fill_enabled = have_fill_glyph = false;
            have_border_enabled = have_border_glyph = false;
            have_sprite_id = have_align = have_visible = false;
            memset(&candidate->elements[candidate->element_count++], 0,
                   sizeof(UiDocumentElement));
            continue;
        }
        separator = strchr(line, '=');
        if (!separator) return UI_DOCUMENT_PARSE_ERROR;
        *separator = '\0'; key = line; value = separator + 1;
        if (section == ROOT) {
            if (strcmp(key, "ui_version") == 0 && !have_version)
                have_version = parse_u32(value, &version, false);
            else if (strcmp(key, "kind") == 0 && !have_kind) {
                have_kind = strcmp(value, "menu") == 0;
                candidate->kind = UI_DOCUMENT_KIND_MENU;
            } else if (strcmp(key, "name") == 0 && !have_name)
                have_name = copy_string(candidate->name, sizeof(candidate->name), value);
            else if (strcmp(key, "design_width") == 0 && !have_design_width)
                have_design_width = parse_int(value, &candidate->design_width),
                saw_v2_only_field = true;
            else if (strcmp(key, "design_height") == 0 && !have_design_height)
                have_design_height = parse_int(value, &candidate->design_height),
                saw_v2_only_field = true;
            else if (strcmp(key, "next_element_id") == 0 && !have_next)
                have_next = parse_u32(value, &candidate->next_element_id, false);
            else return UI_DOCUMENT_PARSE_ERROR;
            if ((strcmp(key, "ui_version") == 0 && !have_version) ||
                (strcmp(key, "kind") == 0 && !have_kind) ||
                (strcmp(key, "name") == 0 && !have_name) ||
                (strcmp(key, "design_width") == 0 && !have_design_width) ||
                (strcmp(key, "design_height") == 0 && !have_design_height) ||
                (strcmp(key, "next_element_id") == 0 && !have_next))
                return UI_DOCUMENT_PARSE_ERROR;
        } else {
            UiDocumentElement *element =
                &candidate->elements[candidate->element_count - 1U];
            if (strcmp(key, "id") == 0 && !have_id)
                have_id = parse_u32(value, &element->id, false);
            else if (strcmp(key, "parent") == 0 && !have_parent)
                have_parent = parse_u32(value, &element->parent_id, true);
            else if (strcmp(key, "type") == 0 && !have_type)
                have_type = parse_type(value, &element->type);
            else if (strcmp(key, "name") == 0 && !have_element_name)
                have_element_name = copy_string(element->name, sizeof(element->name), value);
            else if (strcmp(key, "content") == 0 && !have_content)
                have_content = copy_string(element->content, sizeof(element->content), value);
            else if (strcmp(key, "port") == 0 && !have_port)
                have_port = copy_string(element->flow_port, sizeof(element->flow_port), value);
            else if (strcmp(key, "x") == 0 && !have_x)
                have_x = parse_int(value, &element->layout.x), saw_v2_only_field = true;
            else if (strcmp(key, "y") == 0 && !have_y)
                have_y = parse_int(value, &element->layout.y), saw_v2_only_field = true;
            else if (strcmp(key, "width") == 0 && !have_width)
                have_width = parse_int(value, &element->layout.width), saw_v2_only_field = true;
            else if (strcmp(key, "height") == 0 && !have_height)
                have_height = parse_int(value, &element->layout.height), saw_v2_only_field = true;
            else if (strcmp(key, "h_anchor") == 0 && !have_h_anchor)
                have_h_anchor = parse_anchor(value, &element->layout.horizontal_anchor),
                saw_v2_only_field = true;
            else if (strcmp(key, "v_anchor") == 0 && !have_v_anchor)
                have_v_anchor = parse_anchor(value, &element->layout.vertical_anchor),
                saw_v2_only_field = true;
            else if (strcmp(key, "scale") == 0 && !have_scale)
                have_scale = parse_int(value, &element->layout.scale_percent),
                saw_v2_only_field = true;
            else if (strcmp(key, "visual") == 0 && !have_visual)
                have_visual = parse_visual_mode(value, &element->visual.mode),
                saw_v3_only_field = true;
            else if (strcmp(key, "fg") == 0 && !have_fg)
                have_fg = parse_color(value, &element->visual.foreground),
                saw_v3_only_field = true;
            else if (strcmp(key, "bg") == 0 && !have_bg)
                have_bg = parse_color(value, &element->visual.background),
                saw_v3_only_field = true;
            else if (strcmp(key, "fill_enabled") == 0 && !have_fill_enabled)
                have_fill_enabled = parse_bool(value, &element->visual.fill_enabled),
                saw_v3_only_field = true;
            else if (strcmp(key, "fill_glyph") == 0 && !have_fill_glyph)
                have_fill_glyph = parse_u8(value, &element->visual.fill_glyph),
                saw_v3_only_field = true;
            else if (strcmp(key, "border_enabled") == 0 && !have_border_enabled)
                have_border_enabled = parse_bool(value, &element->visual.border_enabled),
                saw_v3_only_field = true;
            else if (strcmp(key, "border_glyph") == 0 && !have_border_glyph)
                have_border_glyph = parse_u8(value, &element->visual.border_glyph),
                saw_v3_only_field = true;
            else if (strcmp(key, "sprite_id") == 0 && !have_sprite_id)
                have_sprite_id = parse_u16(value, &element->visual.sprite_id),
                saw_v3_only_field = true;
            else if (strcmp(key, "align") == 0 && !have_align)
                have_align = parse_align(value, &element->visual.align),
                saw_v3_only_field = true;
            else if (strcmp(key, "visible") == 0 && !have_visible)
                have_visible = parse_bool(value, &element->visual.visible_by_default),
                saw_v3_only_field = true;
            else return UI_DOCUMENT_PARSE_ERROR;
            if ((strcmp(key, "id") == 0 && !have_id) ||
                (strcmp(key, "parent") == 0 && !have_parent) ||
                (strcmp(key, "type") == 0 && !have_type) ||
                (strcmp(key, "name") == 0 && !have_element_name) ||
                (strcmp(key, "content") == 0 && !have_content) ||
                (strcmp(key, "port") == 0 && !have_port) ||
                (strcmp(key, "x") == 0 && !have_x) ||
                (strcmp(key, "y") == 0 && !have_y) ||
                (strcmp(key, "width") == 0 && !have_width) ||
                (strcmp(key, "height") == 0 && !have_height) ||
                (strcmp(key, "h_anchor") == 0 && !have_h_anchor) ||
                (strcmp(key, "v_anchor") == 0 && !have_v_anchor) ||
                (strcmp(key, "scale") == 0 && !have_scale) ||
                (strcmp(key, "visual") == 0 && !have_visual) ||
                (strcmp(key, "fg") == 0 && !have_fg) ||
                (strcmp(key, "bg") == 0 && !have_bg) ||
                (strcmp(key, "fill_enabled") == 0 && !have_fill_enabled) ||
                (strcmp(key, "fill_glyph") == 0 && !have_fill_glyph) ||
                (strcmp(key, "border_enabled") == 0 && !have_border_enabled) ||
                (strcmp(key, "border_glyph") == 0 && !have_border_glyph) ||
                (strcmp(key, "sprite_id") == 0 && !have_sprite_id) ||
                (strcmp(key, "align") == 0 && !have_align) ||
                (strcmp(key, "visible") == 0 && !have_visible))
                return UI_DOCUMENT_PARSE_ERROR;
        }
    }
    if (ferror(file) || !have_version || !have_kind || !have_name || !have_next)
        return UI_DOCUMENT_PARSE_ERROR;
    if (section == ELEMENT && !element_fields_complete(version,
        have_id && have_parent && have_type && have_element_name && have_content &&
            have_port,
        have_x && have_y && have_width && have_height && have_h_anchor &&
            have_v_anchor && have_scale,
        have_visual && have_fg && have_bg && have_fill_enabled && have_fill_glyph &&
            have_border_enabled && have_border_glyph && have_sprite_id && have_align &&
            have_visible))
        return UI_DOCUMENT_PARSE_ERROR;
    if (version != UI_DOCUMENT_VERSION && version != UI_DOCUMENT_VERSION_V2 &&
        version != UI_DOCUMENT_VERSION_V1)
        return UI_DOCUMENT_UNSUPPORTED_VERSION;
    if (version >= UI_DOCUMENT_VERSION_V2 &&
        (!have_design_width || !have_design_height)) return UI_DOCUMENT_PARSE_ERROR;
    if (version == UI_DOCUMENT_VERSION_V1) {
        size_t i;
        if (saw_v2_only_field)
            return UI_DOCUMENT_PARSE_ERROR;
        candidate->design_width = 80;
        candidate->design_height = 25;
        for (i = 0U; i < candidate->element_count; i++)
            candidate->elements[i].layout = default_element_layout();
        if (candidate->element_count > 0U)
            candidate->elements[0].layout = (UiDocumentLayout){
                0, 0, 80, 25, UI_DOCUMENT_ANCHOR_STRETCH,
                UI_DOCUMENT_ANCHOR_STRETCH, 100};
    }
    if (version < UI_DOCUMENT_VERSION) {
        size_t i;
        if (saw_v3_only_field) return UI_DOCUMENT_PARSE_ERROR;
        for (i = 0U; i < candidate->element_count; i++)
            candidate->elements[i].visual = default_visual(candidate->elements[i].type);
    }
    asset_document_state_init(&candidate->state);
    return ui_document_validate(candidate);
}

UiDocumentResult ui_document_load(UiDocument *document, const char *path) {
    UiDocument candidate;
    UiDocumentResult result;
    FILE *file;
    if (!document || !path || path[0] == '\0' ||
        strlen(path) >= UI_DOCUMENT_PATH_CAPACITY) return UI_DOCUMENT_INVALID_ARGUMENT;
    file = fopen(path, "r");
    if (!file) return UI_DOCUMENT_IO_ERROR;
    result = parse_file(file, &candidate);
    if (fclose(file) != 0 && result == UI_DOCUMENT_OK) result = UI_DOCUMENT_IO_ERROR;
    if (result != UI_DOCUMENT_OK) return result;
    (void)copy_string(candidate.path, sizeof(candidate.path), path);
    *document = candidate;
    return UI_DOCUMENT_OK;
}