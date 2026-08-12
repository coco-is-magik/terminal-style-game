/** Fixed-width hexadecimal blocks used by versioned scene formats. */
#ifndef SCENE_BLOCK_CODEC_H
#define SCENE_BLOCK_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SCENE_BLOCK_HEX_DIGITS 4U
#define SCENE_BLOCK_TEXT_SIZE (SCENE_BLOCK_HEX_DIGITS + 1U)

bool scene_block_parse(const char *text, uint16_t *out_value);
bool scene_block_format(uint16_t value, char out_text[SCENE_BLOCK_TEXT_SIZE]);
bool scene_block_is_null(uint16_t value);
bool scene_block_count(const char *row_text, size_t *out_count);
bool scene_block_tokens_equal(const char *left, const char *right);

#endif /* SCENE_BLOCK_CODEC_H */