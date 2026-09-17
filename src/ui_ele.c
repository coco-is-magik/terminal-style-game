/**
 * ui_ele.c — Data-driven terminal UI element implementation
 */

#include "ui_ele.h"
#include "number_parse.h"
#include "rgba_parse.h"
#include "ui_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static char *ui_strdup(const char *text) {
    size_t len;
    char *copy;

    if (!text) text = "";
    len = strlen(text);
    copy = malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, text, len + 1);
    return copy;
}

static void trim(char *text) {
    size_t len;
    char *start;

    if (!text) return;
    start = text;
    while (*start == ' ' || *start == '\t') start++;
    if (start != text) memmove(text, start, strlen(start) + 1);

    len = strlen(text);
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r' ||
                       text[len - 1] == ' ' || text[len - 1] == '\t')) {
        text[--len] = '\0';
    }
}

static void basename_no_ext(const char *path, char *out, size_t out_size) {
    const char *base;
    const char *dot;
    size_t len;

    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!path) return;

    base = strrchr(path, '/');
    base = base ? base + 1 : path;
    dot = strrchr(base, '.');
    len = dot && dot > base ? (size_t)(dot - base) : strlen(base);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, base, len);
    out[len] = '\0';
}

static int split_csv(char value[][UI_ELE_NAME_MAX], int max_count, const char *text) {
    char buf[4096];
    char *tok;
    int count = 0;

    if (!text || !value || max_count <= 0) return 0;
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    tok = strtok(buf, ",");
    while (tok && count < max_count) {
        trim(tok);
        if (tok[0] != '\0') {
            strncpy(value[count], tok, UI_ELE_NAME_MAX - 1);
            value[count][UI_ELE_NAME_MAX - 1] = '\0';
            count++;
        }
        tok = strtok(NULL, ",");
    }

    return count;
}

static UiElementType parse_type(const char *text) {
    if (text && strcmp(text, "button") == 0) return UI_ELE_BUTTON;
    if (text && strcmp(text, "text") == 0) return UI_ELE_TEXT;
    return UI_ELE_CONTAINER;
}

static UiCoordMode parse_coords(const char *text) {
    if (text && strcmp(text, "relative") == 0) return UI_COORD_RELATIVE;
    return UI_COORD_ABSOLUTE;
}

static UiAlign parse_align(const char *text) {
    if (text && strcmp(text, "center") == 0) return UI_ALIGN_CENTER;
    if (text && strcmp(text, "right") == 0) return UI_ALIGN_RIGHT;
    return UI_ALIGN_LEFT;
}

bool ui_ele_style_is_valid(UiElementType type, const char *style) {
    if (!style) return false;
    if (strcmp(style, "plain") == 0) return true;
    if (type == UI_ELE_BUTTON)
        return strcmp(style, "bracket") == 0 || strcmp(style, "inverse") == 0;
    if (type == UI_ELE_CONTAINER) return strcmp(style, "frame") == 0;
    if (type == UI_ELE_TEXT) return strcmp(style, "bright") == 0;
    return false;
}

bool ui_ele_transition_is_valid(const char *transition) {
    return transition && (strcmp(transition, "none") == 0 ||
        strcmp(transition, "center_out") == 0 ||
        strcmp(transition, "perimeter_burst") == 0 ||
        strcmp(transition, "local_glitch") == 0);
}

bool ui_ele_focus_effect_is_valid(const char *effect) {
    return effect && (strcmp(effect, "none") == 0 ||
        strcmp(effect, "focus_pulse") == 0 ||
        strcmp(effect, "focus_glitch") == 0 ||
        strcmp(effect, "input_hold_short") == 0);
}

static bool parse_color(const char *text, SDL_Color *out_color) {
    uint8_t channels[4];
    if (!out_color || !rgba_parse(text, channels)) return false;
    *out_color = (SDL_Color){channels[0], channels[1], channels[2], channels[3]};
    return true;
}

static void resolve_links(UiElement *element, UiCache *cache) {
    if (!element || !cache) return;

    if (element->parent_name[0] != '\0') {
        element->parent = ui_cache_get(cache, element->parent_name);
        if (!element->parent) {
            fprintf(stderr, "ui_ele: parent '%s' not found for '%s'; using screen root\n",
                    element->parent_name, element->name);
        }
    }

    for (int i = 0; i < element->child_count; i++) {
        element->children[i] = ui_cache_get(cache, element->child_names[i]);
        if (!element->children[i]) {
            fprintf(stderr, "ui_ele: child '%s' not found for '%s'\n",
                    element->child_names[i], element->name);
        }
    }
}

static void element_absolute_position(const UiElement *element, int parent_x, int parent_y,
                                      int *out_x, int *out_y) {
    int x = 0;
    int y = 0;

    if (!element) {
        if (out_x) *out_x = parent_x;
        if (out_y) *out_y = parent_y;
        return;
    }

    x = element->layout.x;
    y = element->layout.y;
    if (element->layout.coords_mode == UI_COORD_RELATIVE) {
        if (element->parent) {
            int parent_abs_x = 0;
            int parent_abs_y = 0;
            element_absolute_position(element->parent, parent_x, parent_y,
                                      &parent_abs_x, &parent_abs_y);
            x += parent_abs_x;
            y += parent_abs_y;
        } else {
            x += parent_x;
            y += parent_y;
        }
    }

    if (out_x) *out_x = x;
    if (out_y) *out_y = y;
}

bool ui_ele_absolute_bounds(const UiElement *element, int *out_x, int *out_y,
                            int *out_width, int *out_height) {
    int x;
    int y;
    if (!element || !out_x || !out_y || !out_width || !out_height) return false;
    element_absolute_position(element, 0, 0, &x, &y);
    *out_x = x;
    *out_y = y;
    *out_width = element->layout.width;
    *out_height = element->layout.height;
    return true;
}

UiElement *ui_ele_load(const char *path, UiCache *cache) {
    FILE *f;
    UiElement *element;
    char line[4096];
    bool valid = true;
    bool read_failed;
    bool close_failed;

    if (!path) return NULL;
    f = fopen(path, "r");
    if (!f) return NULL;

    element = calloc(1, sizeof(*element));
    if (!element) {
        fclose(f);
        return NULL;
    }
    basename_no_ext(path, element->name, sizeof(element->name));
    element->layout.coords_mode = UI_COORD_ABSOLUTE;
    element->type = UI_ELE_CONTAINER;
    element->visible = 1;
    element->align = UI_ALIGN_LEFT;
    element->fg = (SDL_Color){255, 255, 255, 255};
    element->bg = (SDL_Color){0, 0, 0, 255};
    (void)snprintf(element->style, sizeof(element->style), "plain");
    (void)snprintf(element->transition, sizeof(element->transition), "none");
    (void)snprintf(element->focus_effect, sizeof(element->focus_effect), "none");

    while (fgets(line, sizeof(line), f)) {
        char *eq;
        char *key;
        char *val;

        trim(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        key = line;
        val = eq + 1;
        trim(key);
        trim(val);

        if (strcmp(key, "name") == 0) {
            strncpy(element->name, val, sizeof(element->name) - 1);
        } else if (strcmp(key, "type") == 0) {
            element->type = parse_type(val);
        } else if (strcmp(key, "x") == 0) {
            valid = number_parse_int(val, INT_MIN, INT_MAX, &element->layout.x);
        } else if (strcmp(key, "y") == 0) {
            valid = number_parse_int(val, INT_MIN, INT_MAX, &element->layout.y);
        } else if (strcmp(key, "coords") == 0) {
            element->layout.coords_mode = parse_coords(val);
        } else if (strcmp(key, "width") == 0) {
            valid = number_parse_int(val, INT_MIN, INT_MAX, &element->layout.width);
        } else if (strcmp(key, "height") == 0) {
            valid = number_parse_int(val, INT_MIN, INT_MAX, &element->layout.height);
        } else if (strcmp(key, "parent") == 0) {
            strncpy(element->parent_name, val, sizeof(element->parent_name) - 1);
        } else if (strcmp(key, "children") == 0) {
            element->child_count = split_csv(element->child_names, UI_ELE_MAX_CHILDREN, val);
        } else if (strcmp(key, "content") == 0) {
            ui_ele_set_content(element, val);
        } else if (strcmp(key, "visible") == 0) {
            valid = number_parse_int(val, 0, 1, &element->visible);
        } else if (strcmp(key, "z_index") == 0) {
            valid = number_parse_int(val, INT_MIN, INT_MAX, &element->z_index);
        } else if (strcmp(key, "align") == 0) {
            element->align = parse_align(val);
        } else if (strcmp(key, "fg") == 0) {
            element->has_fg = parse_color(val, &element->fg);
            valid = element->has_fg;
        } else if (strcmp(key, "bg") == 0) {
            element->has_bg = parse_color(val, &element->bg);
            valid = element->has_bg;
        } else if (strcmp(key, "action") == 0) {
            strncpy(element->action, val, sizeof(element->action) - 1);
        } else if (strcmp(key, "style") == 0) {
            if (strlen(val) >= sizeof(element->style)) valid = false;
            else (void)snprintf(element->style, sizeof(element->style), "%s", val);
        } else if (strcmp(key, "transition") == 0) {
            if (strlen(val) >= sizeof(element->transition)) valid = false;
            else (void)snprintf(element->transition, sizeof(element->transition), "%s", val);
        } else if (strcmp(key, "focus_effect") == 0) {
            if (strlen(val) >= sizeof(element->focus_effect)) valid = false;
            else (void)snprintf(element->focus_effect, sizeof(element->focus_effect), "%s", val);
        }
        if (!valid) break;
    }

    read_failed = ferror(f) != 0;
    close_failed = fclose(f) != 0;
    if (read_failed || close_failed || !valid ||
        !ui_ele_style_is_valid(element->type, element->style) ||
        !ui_ele_transition_is_valid(element->transition) ||
        !ui_ele_focus_effect_is_valid(element->focus_effect)) {
        ui_ele_destroy(element);
        return NULL;
    }
    resolve_links(element, cache);
    return element;
}

void ui_ele_destroy(UiElement *element) {
    if (!element) return;
    free(element->content);
    free(element);
}

void ui_ele_set_content(UiElement *element, const char *content) {
    char *copy;
    size_t length;

    if (!element) return;
    if (!content) content = "";
    length = strlen(content);
    if (element->content && length + 1U <= element->content_capacity) {
        memcpy(element->content, content, length + 1U);
        return;
    }
    copy = ui_strdup(content);
    if (!copy) return;
    free(element->content);
    element->content = copy;
    element->content_capacity = length + 1U;
}

bool ui_ele_reserve_content(UiElement *element, size_t capacity) {
    char *replacement;
    size_t length;
    if (!element || capacity == 0U) return false;
    if (element->content && element->content_capacity >= capacity) return true;
    replacement = malloc(capacity);
    if (!replacement) return false;
    length = element->content ? strlen(element->content) : 0U;
    if (length >= capacity) {
        free(replacement);
        return false;
    }
    if (element->content) memcpy(replacement, element->content, length + 1U);
    else replacement[0] = '\0';
    free(element->content);
    element->content = replacement;
    element->content_capacity = capacity;
    return true;
}

bool ui_ele_set_content_bounded(UiElement *element, const char *content) {
    size_t length;
    if (!element || !element->content || !content) return false;
    length = strlen(content);
    if (length + 1U > element->content_capacity) return false;
    memcpy(element->content, content, length + 1U);
    return true;
}

void ui_ele_set_colors(UiElement *element, SDL_Color fg, SDL_Color bg) {
    if (!element) return;
    element->has_fg = true;
    element->has_bg = true;
    element->fg = fg;
    element->bg = bg;
}

const char *ui_ele_get_action(const UiElement *element) {
    if (!element || element->action[0] == '\0') return NULL;
    return element->action;
}

static int aligned_x(int x, int width, int len, UiAlign align) {
    if (width <= len) return x;
    if (align == UI_ALIGN_CENTER) return x + (width - len) / 2;
    if (align == UI_ALIGN_RIGHT) return x + (width - len);
    return x;
}

static void print_line(Grid *grid, int x, int y, int width, UiAlign align,
                       const char *line, int len, SDL_Color fg, SDL_Color bg) {
    int out_x = aligned_x(x, width, len, align);

    for (int i = 0; i < len; i++) {
        grid_set(grid, out_x + i, y, (unsigned char)line[i], fg, bg);
    }
}

static int emit_word_wrapped(Grid *grid, int x, int y, int width, int height,
                             UiAlign align, const char *text,
                             SDL_Color fg, SDL_Color bg) {
    char line[4096];
    int line_len = 0;
    int row = 0;
    const char *p = text ? text : "";

    if (width <= 0) {
        grid_print(grid, x, y, p, fg, bg);
        return 1;
    }

    while (*p) {
        char word[256];
        int word_len = 0;

        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n') {
            print_line(grid, x, y + row, width, align, line, line_len, fg, bg);
            row++;
            line_len = 0;
            p++;
            if (height > 0 && row >= height) return row;
            continue;
        }
        if (*p == '\0') break;

        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && word_len < (int)sizeof(word) - 1) {
            word[word_len++] = *p++;
        }
        word[word_len] = '\0';

        if (word_len > width) {
            int offset = 0;
            if (line_len > 0) {
                print_line(grid, x, y + row, width, align, line, line_len, fg, bg);
                row++;
                line_len = 0;
                if (height > 0 && row >= height) return row;
            }
            while (offset < word_len) {
                int take = word_len - offset;
                if (take > width) take = width;
                print_line(grid, x, y + row, width, align, word + offset, take, fg, bg);
                row++;
                offset += take;
                if (height > 0 && row >= height) return row;
            }
        } else {
            int add_space = line_len > 0 ? 1 : 0;
            if (line_len + add_space + word_len > width) {
                print_line(grid, x, y + row, width, align, line, line_len, fg, bg);
                row++;
                line_len = 0;
                if (height > 0 && row >= height) return row;
                add_space = 0;
            }
            if (add_space) line[line_len++] = ' ';
            memcpy(line + line_len, word, (size_t)word_len);
            line_len += word_len;
        }
    }

    if (line_len > 0 && (height <= 0 || row < height)) {
        print_line(grid, x, y + row, width, align, line, line_len, fg, bg);
        row++;
    }
    return row;
}

static void effective_colors(const UiElement *element, SDL_Color fg, SDL_Color bg,
                             SDL_Color *out_fg, SDL_Color *out_bg) {
    SDL_Color draw_fg = element->has_fg ? element->fg : fg;
    SDL_Color draw_bg = element->has_bg ? element->bg : bg;
    if (element->type == UI_ELE_TEXT && strcmp(element->style, "bright") == 0) {
        UiThemeColor token = ui_theme_provisional_tokens()->palette.text_primary;
        draw_fg = (SDL_Color){token.red, token.green, token.blue, token.alpha};
    }
    if (strcmp(element->style, "inverse") == 0) {
        SDL_Color swap = draw_fg;
        draw_fg = draw_bg;
        draw_bg = swap;
    }
    *out_fg = draw_fg;
    *out_bg = draw_bg;
}

static void render_element_self(UiElement *element, Grid *grid, int parent_x,
                                int parent_y, SDL_Color fg, SDL_Color bg) {
    int abs_x;
    int abs_y;
    SDL_Color draw_fg;
    SDL_Color draw_bg;

    if (!element || !grid) return;
    if (!element->visible) return;
    element_absolute_position(element, parent_x, parent_y, &abs_x, &abs_y);

    effective_colors(element, fg, bg, &draw_fg, &draw_bg);

    if (element->type == UI_ELE_TEXT || element->type == UI_ELE_BUTTON) {
        emit_word_wrapped(grid, abs_x, abs_y,
                          element->layout.width, element->layout.height,
                          element->align, element->content, draw_fg, draw_bg);
        if (element->type == UI_ELE_BUTTON && element->focused &&
            element->layout.width >= 2) {
            grid_set(grid, abs_x, abs_y, '>', draw_fg, draw_bg);
            grid_set(grid, abs_x + element->layout.width - 1, abs_y,
                     '<', draw_fg, draw_bg);
        } else if (element->type == UI_ELE_BUTTON &&
                   strcmp(element->style, "bracket") == 0 &&
                   element->layout.width >= 2) {
            grid_set(grid, abs_x, abs_y, '[', draw_fg, draw_bg);
            grid_set(grid, abs_x + element->layout.width - 1, abs_y,
                     ']', draw_fg, draw_bg);
        }
        return;
    }

    if (strcmp(element->style, "frame") == 0 && element->layout.width >= 2 &&
        element->layout.height >= 2) {
        int x;
        int y;
        for (x = 0; x < element->layout.width; x++) {
            grid_set(grid, abs_x + x, abs_y, '-', draw_fg, draw_bg);
            grid_set(grid, abs_x + x, abs_y + element->layout.height - 1,
                     '-', draw_fg, draw_bg);
        }
        for (y = 0; y < element->layout.height; y++) {
            grid_set(grid, abs_x, abs_y + y, '|', draw_fg, draw_bg);
            grid_set(grid, abs_x + element->layout.width - 1, abs_y + y,
                     '|', draw_fg, draw_bg);
        }
        grid_set(grid, abs_x, abs_y, '+', draw_fg, draw_bg);
        grid_set(grid, abs_x + element->layout.width - 1, abs_y, '+', draw_fg, draw_bg);
        grid_set(grid, abs_x, abs_y + element->layout.height - 1, '+', draw_fg, draw_bg);
        grid_set(grid, abs_x + element->layout.width - 1,
                 abs_y + element->layout.height - 1, '+', draw_fg, draw_bg);
    }

}

void ui_ele_render_self(UiElement *element, Grid *grid, int parent_x, int parent_y,
                        SDL_Color fg, SDL_Color bg) {
    render_element_self(element, grid, parent_x, parent_y, fg, bg);
}

void ui_ele_render(UiElement *element, Grid *grid, int parent_x, int parent_y,
                   SDL_Color fg, SDL_Color bg) {
    int abs_x;
    int abs_y;
    SDL_Color draw_fg;
    SDL_Color draw_bg;
    if (!element || !element->visible || !grid) return;
    element_absolute_position(element, parent_x, parent_y, &abs_x, &abs_y);
    effective_colors(element, fg, bg, &draw_fg, &draw_bg);
    render_element_self(element, grid, parent_x, parent_y, fg, bg);
    for (int pass = 0; pass < element->child_count; pass++) {
        int best = -1;
        for (int i = 0; i < element->child_count; i++) {
            int lower_count = 0;
            if (!element->children[i]) continue;
            for (int j = 0; j < element->child_count; j++) {
                if (!element->children[j]) continue;
                if (element->children[j]->z_index < element->children[i]->z_index ||
                    (element->children[j]->z_index == element->children[i]->z_index && j < i)) {
                    lower_count++;
                }
            }
            if (lower_count == pass) {
                best = i;
                break;
            }
        }
        if (best >= 0) {
            ui_ele_render(element->children[best], grid, abs_x, abs_y, draw_fg, draw_bg);
        }
    }
}

void ui_cache_init(UiCache *cache, const char *master_map_path) {
    FILE *f;
    char line[4096];
    UiMasterEntry *current = NULL;

    if (!cache) return;
    memset(cache, 0, sizeof(*cache));
    if (!master_map_path) return;

    f = fopen(master_map_path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        char *eq;
        char *key;
        char *val;

        trim(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        key = line;
        val = eq + 1;
        trim(key);
        trim(val);

        if (strcmp(key, "layout") == 0 && cache->master_count < UI_MASTER_MAX) {
            current = &cache->master_entries[cache->master_count++];
            memset(current, 0, sizeof(*current));
            strncpy(current->layout, val, sizeof(current->layout) - 1);
        } else if (current && strcmp(key, "cache_next") == 0) {
            current->cache_next_count = split_csv(current->cache_next, UI_LAYOUT_MAX_ELEMS, val);
        } else if (current && strcmp(key, "drop_after") == 0) {
            current->drop_after_count = split_csv(current->drop_after, UI_LAYOUT_MAX_ELEMS, val);
        }
    }
    fclose(f);
}

void ui_cache_destroy(UiCache *cache) {
    if (!cache) return;
    for (int i = 0; i < cache->count; i++) {
        ui_ele_destroy(cache->items[i]);
        cache->items[i] = NULL;
    }
    cache->count = 0;
}

UiElement *ui_cache_get(UiCache *cache, const char *name) {
    if (!cache || !name) return NULL;
    for (int i = 0; i < cache->count; i++) {
        if (cache->items[i] && strcmp(cache->items[i]->name, name) == 0) return cache->items[i];
    }
    return NULL;
}

UiElement *ui_cache_load(UiCache *cache, const char *name, const char *base_dir) {
    char path[UI_ELE_PATH_MAX];
    UiElement *element;

    if (!cache || !name || !base_dir) return NULL;
    element = ui_cache_get(cache, name);
    if (element) return element;
    if (cache->count >= UI_CACHE_MAX) return NULL;

    snprintf(path, sizeof(path), "%s/%s.txt", base_dir, name);
    element = ui_ele_load(path, cache);
    if (!element) return NULL;
    cache->items[cache->count++] = element;
    resolve_links(element, cache);
    return element;
}

void ui_cache_tick(UiCache *cache, const char *active_layout, const char *base_dir) {
    if (!cache || !active_layout || !base_dir) return;
    for (int i = 0; i < cache->master_count; i++) {
        UiMasterEntry *entry = &cache->master_entries[i];
        if (strcmp(entry->layout, active_layout) != 0) continue;
        ui_cache_load(cache, entry->layout, base_dir);
        for (int j = 0; j < entry->cache_next_count; j++) {
            ui_cache_load(cache, entry->cache_next[j], base_dir);
        }
        return;
    }
}

UiLayout *ui_layout_load(const char *path, UiCache *cache) {
    FILE *f;
    UiLayout *layout;
    char line[512];

    if (!path) return NULL;
    f = fopen(path, "r");
    if (!f) return NULL;
    layout = calloc(1, sizeof(*layout));
    if (!layout) {
        fclose(f);
        return NULL;
    }
    basename_no_ext(path, layout->name, sizeof(layout->name));

    while (fgets(line, sizeof(line), f)) {
        char *eq;
        char *key;
        char *val;

        trim(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        key = line;
        val = eq + 1;
        trim(key);
        trim(val);

        if (strcmp(key, "name") == 0) {
            (void)snprintf(layout->name, sizeof(layout->name), "%s", val);
        } else if (strcmp(key, "elements") == 0) {
            char names[UI_LAYOUT_MAX_ELEMS][UI_ELE_NAME_MAX];
            int count = split_csv(names, UI_LAYOUT_MAX_ELEMS, val);
            for (int i = 0; i < count; i++) {
                if (layout->element_count < UI_LAYOUT_MAX_ELEMS) {
                    layout->elements[layout->element_count++] = ui_cache_get(cache, names[i]);
                }
            }
        } else if (strncmp(key, "slot_", 5) == 0 && layout->slot_count < UI_LAYOUT_MAX_SLOTS) {
            size_t slot_name_length = strlen(key + 5);
            if (slot_name_length >= UI_ELE_NAME_MAX)
                slot_name_length = UI_ELE_NAME_MAX - 1U;
            memcpy(layout->slot_names[layout->slot_count], key + 5,
                   slot_name_length);
            layout->slot_names[layout->slot_count][slot_name_length] = '\0';
            layout->slot_elements[layout->slot_count] = ui_cache_get(cache, val);
            layout->slot_count++;
        }
    }

    fclose(f);
    return layout;
}

void ui_layout_substitute(UiLayout *layout, const char *slot_name, UiElement *element) {
    if (!layout || !slot_name) return;
    for (int i = 0; i < layout->slot_count; i++) {
        if (strcmp(layout->slot_names[i], slot_name) == 0) {
            layout->slot_elements[i] = element;
            return;
        }
    }
    if (layout->slot_count < UI_LAYOUT_MAX_SLOTS) {
        strncpy(layout->slot_names[layout->slot_count], slot_name, UI_ELE_NAME_MAX - 1);
        layout->slot_elements[layout->slot_count] = element;
        layout->slot_count++;
    }
}

static UiElement *element_get_focused(UiElement *element, int *remaining) {
    if (!element || !remaining || !element->visible) return NULL;
    if (element->type == UI_ELE_BUTTON) {
        if (*remaining == 0) return element;
        (*remaining)--;
        return NULL;
    }

    for (int i = 0; i < element->child_count; i++) {
        UiElement *found = element_get_focused(element->children[i], remaining);
        if (found) return found;
    }
    return NULL;
}

static int element_focusable_count(UiElement *element) {
    int count = 0;

    if (!element || !element->visible) return 0;
    if (element->type == UI_ELE_BUTTON) return 1;
    for (int i = 0; i < element->child_count; i++) {
        count += element_focusable_count(element->children[i]);
    }
    return count;
}

UiElement *ui_layout_get_focused(UiLayout *layout, int focus_index) {
    int remaining = focus_index;

    if (!layout || focus_index < 0) return NULL;
    for (int i = 0; i < layout->element_count; i++) {
        UiElement *found = element_get_focused(layout->elements[i], &remaining);
        if (found) return found;
    }
    for (int i = 0; i < layout->slot_count; i++) {
        UiElement *found = element_get_focused(layout->slot_elements[i], &remaining);
        if (found) return found;
    }
    return NULL;
}

void ui_layout_set_focus(UiLayout *layout, int focus_index) {
    int count;

    if (!layout) return;
    count = ui_layout_focusable_count(layout);
    for (int i = 0; i < count; i++) {
        UiElement *button = ui_layout_get_focused(layout, i);
        if (button) button->focused = i == focus_index;
    }
}

int ui_layout_focusable_count(UiLayout *layout) {
    int count = 0;

    if (!layout) return 0;
    for (int i = 0; i < layout->element_count; i++) {
        count += element_focusable_count(layout->elements[i]);
    }
    for (int i = 0; i < layout->slot_count; i++) {
        count += element_focusable_count(layout->slot_elements[i]);
    }
    return count;
}

void ui_layout_render(UiLayout *layout, Grid *grid, SDL_Color fg, SDL_Color bg) {
    UiElement *rendered_parents[UI_LAYOUT_MAX_ELEMS];
    int rendered_parent_count = 0;
    if (!layout || !grid) return;
    for (int i = 0; i < layout->element_count; i++) {
        UiElement *ancestors[UI_LAYOUT_MAX_ELEMS];
        int ancestor_count = 0;
        UiElement *parent = layout->elements[i] ? layout->elements[i]->parent : NULL;
        while (parent && ancestor_count < UI_LAYOUT_MAX_ELEMS) {
            ancestors[ancestor_count++] = parent;
            parent = parent->parent;
        }
        for (int ancestor = ancestor_count - 1; ancestor >= 0; ancestor--) {
            parent = ancestors[ancestor];
            bool already_rendered = false;
            for (int j = 0; j < rendered_parent_count; j++)
                if (rendered_parents[j] == parent) already_rendered = true;
            if (!already_rendered && rendered_parent_count < UI_LAYOUT_MAX_ELEMS) {
                ui_ele_render_self(parent, grid, 0, 0, fg, bg);
                rendered_parents[rendered_parent_count++] = parent;
            }
        }
    }
    for (int pass = 0; pass < layout->element_count; pass++) {
        int best = -1;
        for (int i = 0; i < layout->element_count; i++) {
            int lower_count = 0;
            if (!layout->elements[i]) continue;
            for (int j = 0; j < layout->element_count; j++) {
                if (!layout->elements[j]) continue;
                if (layout->elements[j]->z_index < layout->elements[i]->z_index ||
                    (layout->elements[j]->z_index == layout->elements[i]->z_index && j < i)) {
                    lower_count++;
                }
            }
            if (lower_count == pass) {
                best = i;
                break;
            }
        }
        if (best >= 0) ui_ele_render(layout->elements[best], grid, 0, 0, fg, bg);
    }
    for (int i = 0; i < layout->slot_count; i++) {
        ui_ele_render(layout->slot_elements[i], grid, 0, 0, fg, bg);
    }
}

void ui_layout_destroy(UiLayout *layout) {
    free(layout);
}
