#include "ui_render_adapter.h"

#include <string.h>

static SDL_Color color(UiDocumentColor value) {
    return (SDL_Color){value.red, value.green, value.blue, value.alpha};
}

static const UiRenderElementState *find_state(const UiRenderElementState *states,
                                               size_t count, UiElementId id) {
    size_t i;
    for (i = 0U; i < count; i++) if (states[i].element_id == id) return &states[i];
    return NULL;
}

static bool in_clip(UiResolvedRect clip, int x, int y) {
    return x >= clip.x && y >= clip.y && x < clip.x + clip.width &&
           y < clip.y + clip.height;
}

static UiRenderResult validate_inputs(
    const UiDocument *document, const AssetRegistry *assets,
    const UiRenderElementState *states, size_t state_count,
    const UiRenderTheme *theme, const UiCanvas *canvas,
    UiResolvedElement resolved[UI_DOCUMENT_MAX_ELEMENTS], size_t *resolved_count
) {
    size_t i;
    if (!document || !assets || !assets->materials || !assets->palettes ||
        !assets->material_names || !theme || !canvas || !canvas->cells ||
        !canvas->touched || canvas->width <= 0 || canvas->height <= 0 ||
        (state_count > 0U && !states) || state_count > UI_DOCUMENT_MAX_ELEMENTS)
        return UI_RENDER_INVALID_ARGUMENT;
    if (ui_document_validate(document) != UI_DOCUMENT_OK)
        return UI_RENDER_INVALID_DOCUMENT;
    for (i = 0U; i < state_count; i++) {
        size_t j;
        if (!ui_document_find_element(document, states[i].element_id))
            return UI_RENDER_INVALID_STATE;
        for (j = 0U; j < i; j++)
            if (states[j].element_id == states[i].element_id)
                return UI_RENDER_INVALID_STATE;
    }
    if (ui_layout_resolve(document, canvas->width, canvas->height,
                          resolved, resolved_count) != UI_LAYOUT_RESOLVE_OK)
        return UI_RENDER_LAYOUT_ERROR;
    for (i = 0U; i < document->element_count; i++) {
        const UiDocumentVisual *visual = &document->elements[i].visual;
        const SpriteAsset *sprite;
        size_t cell_count;
        size_t cell_index;
        if (visual->mode != UI_DOCUMENT_VISUAL_SPRITE) continue;
        sprite = asset_registry_get_sprite(assets, visual->sprite_id);
        if (!sprite || sprite->cols > SPRITE_PATTERN_MAX_COLS ||
            sprite->rows > SPRITE_PATTERN_MAX_ROWS) return UI_RENDER_MISSING_SPRITE;
        cell_count = (size_t)sprite->cols * (size_t)sprite->rows;
        for (cell_index = 0U; cell_index < cell_count; cell_index++) {
            PatternCell cell = sprite->pattern[cell_index];
            if (cell.glyph == 0U || cell.glyph == (uint8_t)' ') continue;
            if (!material_id_is_loaded(assets, cell.material_id))
                return UI_RENDER_MISSING_MATERIAL;
            if (assets->materials[cell.material_id].palette_id <= 0 ||
                assets->materials[cell.material_id].palette_id > ASSET_ID_MAX)
                return UI_RENDER_MISSING_MATERIAL;
        }
    }
    return UI_RENDER_OK;
}

static void state_colors(const UiDocumentVisual *visual,
                         const UiRenderElementState *state,
                         const UiRenderTheme *theme,
                         SDL_Color *fg, SDL_Color *bg) {
    *fg = color(visual->foreground);
    *bg = color(visual->background);
    if (!state) return;
    if (state->disabled) {
        *fg = color(theme->disabled_foreground);
        *bg = color(theme->disabled_background);
    } else if (state->pressed) {
        *fg = color(theme->pressed_foreground);
        *bg = color(theme->pressed_background);
    } else if (state->focused) {
        *fg = color(theme->focused_foreground);
        *bg = color(theme->focused_background);
    }
}

static void draw_native(UiCanvas *canvas, const UiDocumentElement *element,
                        const UiResolvedElement *resolved,
                        const UiRenderElementState *state,
                        const UiRenderTheme *theme) {
    SDL_Color fg;
    SDL_Color bg;
    int x;
    int y;
    int content_x;
    int64_t content_start;
    int64_t rect_right;
    int64_t rect_bottom;
    size_t length = strlen(element->content);
    uint8_t border = element->visual.border_glyph;
    state_colors(&element->visual, state, theme, &fg, &bg);
    if (element->visual.fill_enabled)
        for (y = resolved->clip.y; y < resolved->clip.y + resolved->clip.height; y++)
            for (x = resolved->clip.x; x < resolved->clip.x + resolved->clip.width; x++)
                (void)ui_canvas_set(canvas, x, y, element->visual.fill_glyph, fg, bg);
    if (element->visual.border_enabled && resolved->rect.width > 0 &&
        resolved->rect.height > 0) {
        if (element->type == UI_DOCUMENT_ELEMENT_BUTTON && state) {
            if (state->disabled) border = (uint8_t)'!';
            else if (state->pressed) border = (uint8_t)'#';
        }
        rect_right = (int64_t)resolved->rect.x + resolved->rect.width - 1;
        rect_bottom = (int64_t)resolved->rect.y + resolved->rect.height - 1;
        if (resolved->rect.y >= resolved->clip.y &&
            resolved->rect.y < resolved->clip.y + resolved->clip.height)
            for (x = resolved->clip.x;
                 x < resolved->clip.x + resolved->clip.width; x++)
                (void)ui_canvas_set(canvas, x, resolved->rect.y, border, fg, bg);
        if (rect_bottom >= resolved->clip.y &&
            rect_bottom < resolved->clip.y + resolved->clip.height)
            for (x = resolved->clip.x;
                 x < resolved->clip.x + resolved->clip.width; x++)
                (void)ui_canvas_set(canvas, x, (int)rect_bottom, border, fg, bg);
        if (resolved->rect.x >= resolved->clip.x &&
            resolved->rect.x < resolved->clip.x + resolved->clip.width)
            for (y = resolved->clip.y;
                 y < resolved->clip.y + resolved->clip.height; y++)
                (void)ui_canvas_set(canvas, resolved->rect.x, y, border, fg, bg);
        if (rect_right >= resolved->clip.x &&
            rect_right < resolved->clip.x + resolved->clip.width)
            for (y = resolved->clip.y;
                 y < resolved->clip.y + resolved->clip.height; y++)
                (void)ui_canvas_set(canvas, (int)rect_right, y, border, fg, bg);
    }
    if (length == 0U || resolved->rect.height <= 0) return;
    if (length > (size_t)resolved->rect.width) length = (size_t)resolved->rect.width;
    content_start = resolved->rect.x;
    if (element->visual.align == UI_DOCUMENT_ALIGN_CENTER)
        content_start += (resolved->rect.width - (int)length) / 2;
    else if (element->visual.align == UI_DOCUMENT_ALIGN_RIGHT)
        content_start += resolved->rect.width - (int)length;
    for (x = 0; x < (int)length; x++) {
        int64_t destination = content_start + x;
        if (destination >= resolved->clip.x &&
            destination < resolved->clip.x + resolved->clip.width &&
            in_clip(resolved->clip, (int)destination, resolved->rect.y)) {
            content_x = (int)destination;
            (void)ui_canvas_set(canvas, content_x, resolved->rect.y,
                                (uint8_t)element->content[x], fg, bg);
        }
    }
}

static void draw_state_markers(UiCanvas *canvas,
                               const UiDocumentElement *element,
                               const UiResolvedElement *resolved,
                               const UiRenderElementState *state,
                               const UiRenderTheme *theme) {
    SDL_Color fg;
    SDL_Color bg;
    uint8_t left;
    uint8_t right;
    int x;
    int y;
    if (element->type != UI_DOCUMENT_ELEMENT_BUTTON || !state ||
        resolved->rect.width < 2 ||
        !(state->focused || state->pressed || state->disabled)) return;
    state_colors(&element->visual, state, theme, &fg, &bg);
    if (state->disabled) left = right = (uint8_t)'!';
    else if (state->pressed) left = right = (uint8_t)'#';
    else { left = (uint8_t)'>'; right = (uint8_t)'<'; }
    {
        x = resolved->rect.x;
        y = resolved->rect.y;
        if (in_clip(resolved->clip, x, y))
            (void)ui_canvas_set(canvas, x, y, left, fg, bg);
        {
            int64_t right_x = (int64_t)resolved->rect.x + resolved->rect.width - 1;
            if (right_x >= resolved->clip.x &&
                right_x < resolved->clip.x + resolved->clip.width &&
                in_clip(resolved->clip, (int)right_x, y))
                (void)ui_canvas_set(canvas, (int)right_x, y, right, fg, bg);
        }
    }
}

static void draw_sprite(UiCanvas *canvas, const UiDocumentElement *element,
                        const UiResolvedElement *resolved,
                        const AssetRegistry *assets) {
    const SpriteAsset *sprite = asset_registry_get_sprite(assets,
                                                           element->visual.sprite_id);
    int y;
    int x;
    for (y = resolved->clip.y; y < resolved->clip.y + resolved->clip.height; y++) {
        int relative_y = y - resolved->rect.y;
        int source_y = (int)((int64_t)relative_y * sprite->rows /
                             resolved->rect.height);
        for (x = resolved->clip.x; x < resolved->clip.x + resolved->clip.width; x++) {
            int relative_x = x - resolved->rect.x;
            int source_x = (int)((int64_t)relative_x * sprite->cols /
                                 resolved->rect.width);
            PatternCell cell = sprite->pattern[source_y * sprite->cols + source_x];
            const Material *material;
            SDL_Color fg;
            if (cell.glyph == 0U || cell.glyph == (uint8_t)' ') continue;
            material = &assets->materials[cell.material_id];
            fg = palette_sample(&assets->palettes[material->palette_id], 0.0,
                                (LightLevel){1.0, 1.0, 1.0});
            (void)ui_canvas_set(canvas, x, y,
                                cell.glyph, fg, (SDL_Color){0U, 0U, 0U, 0U});
        }
    }
}

UiRenderResult ui_render_document(
    const UiDocument *document, const AssetRegistry *assets,
    const UiRenderElementState *states, size_t state_count,
    const UiRenderTheme *theme, UiCanvas *canvas
) {
    UiResolvedElement resolved[UI_DOCUMENT_MAX_ELEMENTS];
    size_t resolved_count = 0U;
    size_t i;
    UiRenderResult validation = validate_inputs(document, assets, states, state_count,
                                                theme, canvas, resolved,
                                                &resolved_count);
    if (validation != UI_RENDER_OK) return validation;
    ui_canvas_clear(canvas);
    for (i = 0U; i < resolved_count; i++) {
        const UiDocumentElement *element = ui_document_find_element(
            document, resolved[i].element_id);
        const UiRenderElementState *state = find_state(states, state_count, element->id);
        bool visible = state ? state->visible : element->visual.visible_by_default;
        if (!visible || resolved[i].clip.width <= 0 || resolved[i].clip.height <= 0)
            continue;
        if (element->visual.mode == UI_DOCUMENT_VISUAL_NATIVE)
            draw_native(canvas, element, &resolved[i], state, theme);
        else draw_sprite(canvas, element, &resolved[i], assets);
        draw_state_markers(canvas, element, &resolved[i], state, theme);
    }
    return UI_RENDER_OK;
}