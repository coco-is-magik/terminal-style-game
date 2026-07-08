/**
 * glyph_block_cache.c — Cached pre-rasterized glyph blocks
 */

#include "glyph_block_cache.h"

/* font8x8_basic is defined in font8x8.h (included by renderer.c) */
extern const unsigned char font8x8_basic[128][8];
#include <string.h>

/* Cache entry */
typedef struct {
    GlyphBlockKey key;
    GlyphBlock block;
    bool valid;
} CacheEntry;

/* Static cache storage */
static CacheEntry g_cache[GLYPH_BLOCK_CACHE_SIZE];

/* Statistics */
static uint64_t g_hits = 0;
static uint64_t g_misses = 0;
static uint64_t g_evictions = 0;

/* Hash function for cache key */
static unsigned int hash_key(GlyphBlockKey key) {
    unsigned int h = 0;
    h ^= key.glyph_id * 1103515243;
    h ^= key.fg_rgba * 101511;
    h ^= key.bg_rgba * 3571;
    return h % GLYPH_BLOCK_CACHE_SIZE;
}

void glyph_block_cache_init(void) {
    memset(g_cache, 0, sizeof(g_cache));
    g_hits = g_misses = g_evictions = 0;
}

void glyph_block_cache_get_stats(uint64_t *hits, uint64_t *misses, uint64_t *evictions) {
    if (hits) *hits = g_hits;
    if (misses) *misses = g_misses;
    if (evictions) *evictions = g_evictions;
}

bool glyph_block_cache_lookup(GlyphBlockKey key, GlyphBlock *out) {
    unsigned int idx = hash_key(key);
    
    if (g_cache[idx].valid &&
        g_cache[idx].key.glyph_id == key.glyph_id &&
        g_cache[idx].key.fg_rgba == key.fg_rgba &&
        g_cache[idx].key.bg_rgba == key.bg_rgba) {
        if (out) *out = g_cache[idx].block;
        g_hits++;
        return true;
    }
    
    g_misses++;
    return false;
}

void glyph_block_cache_store(GlyphBlockKey key, GlyphBlock block) {
    unsigned int idx = hash_key(key);
    
    if (g_cache[idx].valid) {
        g_evictions++;
    }
    
    g_cache[idx].key = key;
    g_cache[idx].block = block;
    g_cache[idx].valid = true;
}

void glyph_block_cache_compute(GlyphBlockKey key, GlyphBlock *block_out) {
    const uint8_t *glyph_data = font8x8_basic[key.glyph_id < 128 ? key.glyph_id : 32];
    uint32_t fg = key.fg_rgba;
    uint32_t bg = key.bg_rgba;
    
    for (int y = 0; y < 8; y++) {
        uint8_t row = glyph_data[y];
        for (int x = 0; x < 8; x++) {
            uint32_t bit = 1 << x;
            block_out->pixels[y * 8 + x] = (row & bit) ? fg : bg;
        }
    }
}