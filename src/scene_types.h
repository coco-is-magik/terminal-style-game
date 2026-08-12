/**
 * scene_types.h — Bounded authored scene value types
 *
 * These types contain authored values only. They own no nested allocations and
 * contain no resolved asset pointers or runtime caches.
 */

#ifndef SCENE_TYPES_H
#define SCENE_TYPES_H

#include <stddef.h>
#include <stdint.h>

#define SCENE_VERSION_V1 1U
#define SCENE_VERSION_V2 2U
#define SCENE_VERSION_V3 3U
#define SCENE_VERSION_V4 4U
/* Canonical writes use v4; v1/v2/v3 remain accepted migration inputs. */
#define SCENE_VERSION SCENE_VERSION_V4
#define SCENE_FILE_MAX_BYTES (2U * 1024U * 1024U)
#define SCENE_LINE_MAX_BYTES 4096U
#define SCENE_NAME_MAX 64U
#define SCENE_PATH_MAX 1024U
#define SCENE_MAX_WIDTH 512
#define SCENE_MAX_HEIGHT 256
#define SCENE_MAX_LIGHTS 64U
#define SCENE_MAX_DECALS 256U
#define SCENE_MAX_REPAIR_DIAGNOSTICS 256U

typedef uint64_t SceneInstanceId;

typedef enum {
    SCENE_CELL_OCCUPANCY_EMPTY = 0,
    SCENE_CELL_OCCUPANCY_WALL
} SceneCellOccupancy;

typedef struct {
    SceneCellOccupancy occupancy;
    uint16_t wall_material;
    uint16_t floor_material;
    uint16_t ceiling_material;
} SceneAuthoredCell;

typedef enum {
    SCENE_SURFACE_WALL = 0,
    SCENE_SURFACE_FLOOR,
    SCENE_SURFACE_CEILING
} SceneSurfaceKind;

#define SCENE_INSTANCE_ID_INVALID UINT64_C(0)
#define SCENE_INSTANCE_ID_EXHAUSTED UINT64_MAX
#define SCENE_INSTANCE_ID_MAX_ALLOCATABLE (UINT64_MAX - UINT64_C(1))

typedef enum {
    SCENE_ASSET_KIND_INVALID = 0,
    SCENE_ASSET_KIND_DECAL_PATTERN
} SceneAssetKind;

typedef struct {
    SceneAssetKind kind;
    uint16_t id;
} SceneAssetRef;

typedef enum {
    SCENE_DECAL_SURFACE_WALL = 0,
    SCENE_DECAL_SURFACE_FLOOR,
    SCENE_DECAL_SURFACE_CEILING
} SceneDecalSurface;

typedef struct {
    SceneInstanceId id;
    double x;
    double y;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
    double intensity;
    double radius;
} SceneLight;

typedef struct {
    SceneInstanceId id;
    SceneAssetRef asset;
    SceneDecalSurface surface;
    double x;
    double y;
    double z;
    int map_x;
    int map_y;
    int side;
    double u;
    double v;
    double width;
    double height;
    double glyph_step_u;
    double glyph_step_v;
    double depth;
    double rotation;
} SceneDecalInstance;

#endif /* SCENE_TYPES_H */
