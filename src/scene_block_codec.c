#include "scene_block_codec.h"

#include <string.h>

static bool hex_value(char c, uint16_t *out_value) {
    if (c >= '0' && c <= '9') {
        *out_value = (uint16_t)(c - '0');
        return true;
    }
    if (c >= 'A' && c <= 'F') {
        *out_value = (uint16_t)(10 + c - 'A');
        return true;
    }
    return false;
}

bool scene_block_parse(const char *text, uint16_t *out_value) {
    uint16_t value = 0U;
    if (!text || !out_value || strlen(text) != SCENE_BLOCK_HEX_DIGITS) {
        return false;
    }
    for (size_t i = 0U; i < SCENE_BLOCK_HEX_DIGITS; i++) {
        uint16_t digit;
        if (!hex_value(text[i], &digit)) return false;
        value = (uint16_t)((value << 4U) | digit);
    }
    *out_value = value;
    return true;
}

bool scene_block_format(uint16_t value, char out_text[SCENE_BLOCK_TEXT_SIZE]) {
    static const char digits[] = "0123456789ABCDEF";
    if (!out_text) return false;
    for (size_t i = 0U; i < SCENE_BLOCK_HEX_DIGITS; i++) {
        unsigned int shift = (unsigned int)((SCENE_BLOCK_HEX_DIGITS - 1U - i) * 4U);
        out_text[i] = digits[(value >> shift) & 0x0fU];
    }
    out_text[SCENE_BLOCK_HEX_DIGITS] = '\0';
    return true;
}

bool scene_block_is_null(uint16_t value) {
    return value == 0U;
}

bool scene_block_count(const char *row_text, size_t *out_count) {
    const char *cursor = row_text;
    size_t count = 0U;
    if (!row_text || !out_count) return false;
    while (*cursor != '\0') {
        char token[SCENE_BLOCK_TEXT_SIZE];
        uint16_t value;
        size_t length = 0U;
        while (*cursor == ' ' || *cursor == '\t') cursor++;
        if (*cursor == '\0') break;
        while (cursor[length] != '\0' && cursor[length] != ' ' &&
               cursor[length] != '\t') {
            length++;
        }
        if (length != SCENE_BLOCK_HEX_DIGITS) return false;
        memcpy(token, cursor, length);
        token[length] = '\0';
        if (!scene_block_parse(token, &value)) return false;
        count++;
        cursor += length;
    }
    *out_count = count;
    return true;
}

bool scene_block_tokens_equal(const char *left, const char *right) {
    uint16_t left_value;
    uint16_t right_value;
    return scene_block_parse(left, &left_value) &&
           scene_block_parse(right, &right_value) &&
           left_value == right_value;
}