/**
 * glyph_block_cache.h — Cached pre-rasterized glyph blocks
 *
 * Caches fully composited 8×8 RGBA pixel blocks for each (glyph, fg, bg)
 * combination. This avoids repeated rasterization of the same glyph/color
 * combinations every frame.
 *
 * Build flag: USE_GLYPH_CACHE=1 to enable.
 */

#ifndef GLYPH_BLOCK_CACHE_H
#define GLYPH_BLOCK_CACHE_H

#include <stdint.h>
#include <stdbool.h>

/* Cache configuration */
#ifndef GLYPH_BLOCK_CACHE_SIZE
#define GLYPH_BLOCK_CACHE_SIZE 8192
#endif

/* Key for cached glyph block */
typedef struct {
    uint32_t glyph_id;           /* ASCII character (0-255) */
    uint32_t fg_rgba;            /* Packed RGBA foreground */
    uint32_t bg_rgba;            /* Packed RGBA background */
} GlyphBlockKey;

/* Cached composited 8x8 pixel block */
typedef struct {
    uint32_t pixels[64];         /* 8x8 RGBA pixels */
} GlyphBlock;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * glyph_block_cache_init() — Initialize the glyph cache
 *
 * Sets all entries to invalid state. Safe to call multiple times.
 */
void glyph_block_cache_init(void);

/**
 * glyph_block_cache_lookup() — Look up a cached glyph block
 *
 * @param key  Cache key (glyph, fg, bg)
 * @param out  Output pointer to receive cached block (if found)
 * @return     true if cached, false if miss
 */
bool glyph_block_cache_lookup(GlyphBlockKey key, GlyphBlock *out);

/**
 * glyph_block_cache_store() — Store a glyph block in the cache
 *
 * @param key    Cache key
 * @param block  The computed 8x8 pixel block to cache
 */
void glyph_block_cache_store(GlyphBlockKey key, GlyphBlock block);

/**
 * glyph_block_cache_compute() — Compute a glyph block from scratch
 *
 * Rasterizes the given glyph with fg/bg colors into the provided block buffer.
 * This is the original software rasterization logic extracted for reuse.
 */
void glyph_block_cache_compute(GlyphBlockKey key, GlyphBlock *block_out);

/**
 * glyph_block_cache_get_stats() — Get cache statistics
 */
void glyph_block_cache_get_stats(uint64_t *hits, uint64_t *misses, uint64_t *evictions);

#ifdef __cplusplus
}
#endif

#endif /* GLYPH_BLOCK_CACHE_H */