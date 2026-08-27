/**
 * lighting_cache.c — Lighting shadow ray cache implementation
 *
 * Direct-mapped hash table for stationary light-to-tile shadow ray results.
 * The cache persists across frames because light positions and map topology
 * are static during normal gameplay.
 *
 * Build flag: USE_LIGHTING_CACHE=1 enables the cache.
 */

#include "lighting_cache.h"
#include <string.h>

/* =================================================================== */
/* Configuration                                                   */
/* =================================================================== */

#ifndef LIGHTING_CACHE_SIZE
#define LIGHTING_CACHE_SIZE 4096
#endif

/* =================================================================== */
/* Cache entry structure                                             */
/* =================================================================== */

typedef struct {
    LightShadowKey key;
    LightSampleResult result;
    bool valid;
} CacheEntry;

/* =================================================================== */
/* Static data                                                       */
/* =================================================================== */

static CacheEntry g_cache[LIGHTING_CACHE_SIZE];
static int g_current_map_revision = 1;
static int g_current_lighting_revision = 1;

/* Statistics - use atomic-friendly types for potential future thread safety */
static uint64_t g_stats_hits = 0;
static uint64_t g_stats_misses = 0;
static uint64_t g_stats_evictions = 0;

/* =================================================================== */
/* Hash function                                                     */
/* =================================================================== */

static unsigned int hash_key(LightShadowKey key) {
    /* Simple hash combining every exact geometric key field.
     * Uses prime multipliers to reduce collisions. */
    unsigned int h = 0;
    h ^= (unsigned int)(key.map_identity ^ (key.map_identity >> 32U));
    h ^= (unsigned int)key.map_revision * 1103515243;
    h ^= (unsigned int)key.lighting_revision * 101511;
    h ^= (unsigned int)key.light_id * 3571;
    h ^= (unsigned int)(key.light_x_bits ^ (key.light_x_bits >> 32U));
    h ^= (unsigned int)(key.light_y_bits ^ (key.light_y_bits >> 32U));
    h ^= (unsigned int)(key.light_radius_bits ^ (key.light_radius_bits >> 32U));
    h ^= (unsigned int)(key.light_direction_bits ^ (key.light_direction_bits >> 32U));
    h ^= (unsigned int)(key.light_cone_bits ^ (key.light_cone_bits >> 32U));
    h ^= (unsigned int)(key.light_falloff_bits ^ (key.light_falloff_bits >> 32U));
    h ^= (unsigned int)key.light_type * 65537U;
    h ^= (unsigned int)key.target_tile_x * 131071;
    h ^= (unsigned int)key.target_tile_y * 1237;
    return h % LIGHTING_CACHE_SIZE;
}

/* =================================================================== */
/* Public API                                                          */
/* =================================================================== */

void lighting_cache_init(void) {
    memset(g_cache, 0, sizeof(g_cache));
    g_stats_hits = 0;
    g_stats_misses = 0;
    g_stats_evictions = 0;
    g_current_map_revision = 1;
    g_current_lighting_revision = 1;
}

void lighting_cache_get_stats(uint64_t *hits, uint64_t *misses, uint64_t *evictions) {
    if (hits) *hits = g_stats_hits;
    if (misses) *misses = g_stats_misses;
    if (evictions) *evictions = g_stats_evictions;
}

void lighting_cache_reset_stats(void) {
    g_stats_hits = 0;
    g_stats_misses = 0;
    g_stats_evictions = 0;
}

void lighting_cache_invalidate_map(int map_revision) {
    /* For simplicity, a map revision change clears the entire cache.
     * A more sophisticated implementation could track per-entry validity. */
    g_current_map_revision = map_revision;
    memset(g_cache, 0, sizeof(g_cache));
    g_stats_evictions += LIGHTING_CACHE_SIZE; /* Approximate for stats */
}

void lighting_cache_invalidate_lighting(int lighting_revision) {
    /* Same approach - clear cache when lighting properties change. */
    g_current_lighting_revision = lighting_revision;
    memset(g_cache, 0, sizeof(g_cache));
    g_stats_evictions += LIGHTING_CACHE_SIZE; /* Approximate for stats */
}

bool lighting_cache_lookup(LightShadowKey key, LightSampleResult *out) {
    unsigned int idx = hash_key(key);
    
    /* Check if key matches and entry is valid */
    if (g_cache[idx].valid &&
        g_cache[idx].key.map_identity == key.map_identity &&
        g_cache[idx].key.map_revision == key.map_revision &&
        g_cache[idx].key.lighting_revision == key.lighting_revision &&
        g_cache[idx].key.light_id == key.light_id &&
        g_cache[idx].key.light_x_bits == key.light_x_bits &&
        g_cache[idx].key.light_y_bits == key.light_y_bits &&
        g_cache[idx].key.light_radius_bits == key.light_radius_bits &&
        g_cache[idx].key.light_direction_bits == key.light_direction_bits &&
        g_cache[idx].key.light_cone_bits == key.light_cone_bits &&
        g_cache[idx].key.light_falloff_bits == key.light_falloff_bits &&
        g_cache[idx].key.light_type == key.light_type &&
        g_cache[idx].key.target_tile_x == key.target_tile_x &&
        g_cache[idx].key.target_tile_y == key.target_tile_y) {
        if (out) *out = g_cache[idx].result;
        g_stats_hits++;
        return true;
    }
    
    g_stats_misses++;
    return false;
}

void lighting_cache_store(LightShadowKey key, LightSampleResult result) {
    unsigned int idx = hash_key(key);
    
    /* Track eviction if we're overwriting a valid entry */
    if (g_cache[idx].valid) {
        g_stats_evictions++;
    }
    
    g_cache[idx].key = key;
    g_cache[idx].result = result;
    g_cache[idx].valid = true;
}