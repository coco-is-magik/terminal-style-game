/**
 * scene_types.h — Bounded authored scene value types
 *
 * These types contain authored values only. They own no nested allocations and
 * contain no resolved asset pointers or runtime caches.
 */

#ifndef SCENE_TYPES_H
#define SCENE_TYPES_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define SCENE_VERSION_V1 1U
#define SCENE_VERSION_V2 2U
#define SCENE_VERSION_V3 3U
#define SCENE_VERSION_V4 4U
#define SCENE_VERSION_V5 5U
#define SCENE_VERSION_V6 6U
#define SCENE_VERSION_V7 7U
#define SCENE_VERSION_V8 8U
/* Canonical writes use v8; v1-v7 remain accepted migration inputs. */
#define SCENE_VERSION SCENE_VERSION_V8
#define SCENE_FILE_MAX_BYTES (8U * 1024U * 1024U)
#define SCENE_LINE_MAX_BYTES 4096U
#define SCENE_NAME_MAX 64U
#define SCENE_PATH_MAX 1024U
#define SCENE_MAX_WIDTH 512
#define SCENE_MAX_HEIGHT 256
#define SCENE_MAX_LIGHTS 64U
#define SCENE_MAX_DECALS 256U
#define SCENE_MAX_SPRITES 128U
#define SCENE_MAX_REPAIR_DIAGNOSTICS 256U

typedef uint64_t SceneInstanceId;

typedef enum {
    SCENE_CELL_OCCUPANCY_EMPTY = 0,
    SCENE_CELL_OCCUPANCY_WALL
} SceneCellOccupancy;

#define SCENE_HEIGHT_STEPS_PER_UNIT 256
#define SCENE_HEIGHT_MIN_STEP INT16_C(-0x0800)
#define SCENE_HEIGHT_MAX_STEP INT16_C(0x0800)
#define SCENE_DEFAULT_FLOOR_HEIGHT_STEP INT16_C(0x0000)
#define SCENE_DEFAULT_CEILING_HEIGHT_STEP INT16_C(0x0100)
#define SCENE_MIN_CLEARANCE_STEP INT16_C(0x0040)

typedef enum {
    SCENE_GRAVITY_INHERIT = 0,
    SCENE_GRAVITY_DOWN,
    SCENE_GRAVITY_UP,
    SCENE_GRAVITY_NORTH,
    SCENE_GRAVITY_SOUTH,
    SCENE_GRAVITY_EAST,
    SCENE_GRAVITY_WEST
} SceneGravityOrientation;

typedef struct {
    double gravity_magnitude;
    SceneGravityOrientation gravity_orientation;
    double step_height;
    double jump_impulse;
    double air_control_scale;
    double eye_height;
    double head_clearance;
} SceneMovementParameters;

typedef struct {
    uint8_t occupancy; /* SceneCellOccupancy value; byte storage keeps cells compact. */
    uint8_t gravity_orientation;
    bool floor_present;
    bool ceiling_present;
    uint16_t wall_material;
    uint16_t floor_material;
    uint16_t ceiling_material;
    int16_t floor_height_step;
    int16_t ceiling_height_step;
    uint16_t gravity_scale_step;
} SceneAuthoredCell;

typedef struct {
    int16_t floor_height_step;
    int16_t ceiling_height_step;
    uint16_t gravity_scale_step;
    uint8_t gravity_orientation;
    bool floor_present;
    bool ceiling_present;
    uint8_t reserved;
} SceneCellVertical;

_Static_assert(sizeof(SceneAuthoredCell) == 16U,
               "SceneAuthoredCell layout is part of the R8 memory budget");

static inline SceneMovementParameters scene_movement_parameters_default(void) {
    SceneMovementParameters parameters = {
        9.8, SCENE_GRAVITY_DOWN, 0.25, 3.2, 1.0, 0.5, 0.75
    };
    return parameters;
}

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
    SCENE_ASSET_KIND_DECAL_PATTERN,
    SCENE_ASSET_KIND_SPRITE_PATTERN
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

typedef enum {
    SCENE_LIGHT_POINT = 0,
    SCENE_LIGHT_SPOT
} SceneLightType;

#define SCENE_LIGHT_DIRECTION_MIN 0.0
#define SCENE_LIGHT_DIRECTION_MAX 6.28318530717958647692
#define SCENE_LIGHT_CONE_MIN 0.01745329251994329577
#define SCENE_LIGHT_CONE_MAX SCENE_LIGHT_DIRECTION_MAX
#define SCENE_LIGHT_SPOT_CONE_DEFAULT (SCENE_LIGHT_DIRECTION_MAX / 4.0)
#define SCENE_LIGHT_FALLOFF_MIN 0.1
#define SCENE_LIGHT_FALLOFF_MAX 8.0

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
    SceneLightType type;
    double direction;
    double cone;
    double falloff;
} SceneLight;

static inline void scene_light_set_point_defaults(SceneLight *light) {
    if (!light) return;
    light->type = SCENE_LIGHT_POINT;
    light->direction = 0.0;
    light->cone = SCENE_LIGHT_CONE_MAX;
    light->falloff = 1.0;
}

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

typedef struct {
    SceneInstanceId id;
    SceneAssetRef asset;
    double x;
    double y;
} SceneSpriteInstance;

#endif /* SCENE_TYPES_H */
