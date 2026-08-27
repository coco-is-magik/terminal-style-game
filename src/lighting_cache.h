/**
 * lighting_cache.h — Lighting shadow ray cache for stationary light-to-tile queries
 *
 * Caches light-to-tile shadow ray results. These rays are stationary because
 * light positions and map topology don't change during normal gameplay, making
 * them ideal candidates for caching across player movement.
 *
 * Cache key includes:
 *   - map identity (prevents cross-map reuse)
 *   - map_revision (map topology changes)
 *   - lighting_revision (explicit bulk invalidation)
 *   - light_id (stable per-light identifier)
 *   - exact light position/radius bit patterns (geometric attenuation changes)
 *   - target_tile_x, target_tile_y (integer tile coordinates)
 *
 * Build flag: USE_LIGHTING_CACHE=1 to enable the cache.
 * Debug flag: LIGHTING_CACHE_VALIDATE=1 to validate hits against original computation.
 */

#ifndef LIGHTING_CACHE_H
#define LIGHTING_CACHE_H

#include <stdint.h>
#include <stdbool.h>

/* Cache key for light-to-tile shadow rays */
typedef struct {
    uint64_t map_identity;
    int map_revision;
    int lighting_revision;
    int light_id;
    uint64_t light_x_bits;
    uint64_t light_y_bits;
    uint64_t light_radius_bits;
    uint64_t light_direction_bits;
    uint64_t light_cone_bits;
    uint64_t light_falloff_bits;
    int light_type;
    int target_tile_x;
    int target_tile_y;
} LightShadowKey;

/* Cache result stores geometry only; current RGB/A and intensity apply on lookup. */
typedef struct {
    bool   blocked;       /* Is this tile shadowed from this light? */
    double distance;      /* Distance from light to tile center */
    double attenuation;   /* Light falloff factor (1.0 at center, 0.0 at radius edge) */
    double intensity;     /* Shadow-adjusted geometric attenuation */
} LightSampleResult;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * lighting_cache_init() — Initialize the lighting shadow cache
 *
 * Sets all entries to empty/invalid state. Safe to call multiple times.
 * No-op when USE_LIGHTING_CACHE is not defined.
 */
void lighting_cache_init(void);

/**
 * lighting_cache_lookup() — Look up a cached light-to-tile result
 *
 * Searches the cache for the given key. If found, writes the result to *out
 * and returns true. If not found, returns false and *out is unchanged.
 *
 * @param key  Cache key describing the light-ray query
 * @param out  Output pointer to receive cached result (if found)
 * @return     true if cached, false if miss
 */
bool lighting_cache_lookup(LightShadowKey key, LightSampleResult *out);

/**
 * lighting_cache_store() — Store a light-to-tile result in the cache
 *
 * Stores the result under the given key. If the cache is full, this evicts
 * an existing entry (direct-mapped array).
 *
 * @param key    Cache key describing the light-ray query
 * @param result The computed light sample to cache
 */
void lighting_cache_store(LightShadowKey key, LightSampleResult result);

/**
 * lighting_cache_invalidate_map() — Invalidate all cache entries for a map revision
 *
 * Called when map topology changes (doors, destructible tiles, etc).
 * This is a full cache clear in the current design.
 *
 * @param map_revision  The new map revision to set
 */
void lighting_cache_invalidate_map(int map_revision);

/**
 * lighting_cache_invalidate_lighting() — Increment lighting revision
 *
 * Called when any light property changes (position, radius, intensity).
 *
 * @param lighting_revision  The new lighting revision to set
 */
void lighting_cache_invalidate_lighting(int lighting_revision);

/**
 * lighting_cache_get_stats() — Get cache statistics
 *
 * Writes current cache hit/miss counts to provided pointers.
 * All pointers may be NULL if caller doesn't need those values.
 */
void lighting_cache_get_stats(uint64_t *hits, uint64_t *misses, uint64_t *evictions);

/**
 * lighting_cache_reset_stats() — Reset cache statistics to zero
 */
void lighting_cache_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* LIGHTING_CACHE_H */