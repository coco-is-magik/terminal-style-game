/**
 * editor_types.h — Shared types for the unified in-world editor
 *
 * Header-only foundation used by SceneDocument, command history,
 * selection, and the unified editor controller. Scene instances are addressed
 * only by stable SceneInstanceId values; no runtime pointers are stored here.
 */

#ifndef EDITOR_TYPES_H
#define EDITOR_TYPES_H

#include "scene_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t DocumentStateId;
typedef int MaterialId;

typedef enum {
    WALL_FACE_NORTH = 0,
    WALL_FACE_SOUTH,
    WALL_FACE_EAST,
    WALL_FACE_WEST
} WallFace;

typedef struct {
    int map_x;
    int map_y;
} WallMaterialRef;

typedef struct {
    int map_x;
    int map_y;
    WallFace face;
} WallFaceRef;

typedef struct {
    SceneInstanceId id;
} LightSelectionRef;

typedef struct {
    SceneInstanceId id;
} DecalSelectionRef;

typedef struct {
    SceneInstanceId id;
} SpriteSelectionRef;
typedef struct { SceneInstanceId id; } TriggerSelectionRef;
typedef struct { SceneInstanceId id; } ObjectSelectionRef;

typedef struct {
    int map_x;
    int map_y;
} HorizontalSurfaceRef;

typedef enum {
    SELECTION_NONE = 0,
    SELECTION_WALL_FACE,
    SELECTION_LIGHT,
    SELECTION_FLOOR,
    SELECTION_CEILING,
    SELECTION_DECAL,
    SELECTION_SPRITE,
    SELECTION_TRIGGER,
    SELECTION_OBJECT
} SelectionType;

typedef struct {
    SelectionType type;
    union {
        WallFaceRef wall_face;
        LightSelectionRef light;
        DecalSelectionRef decal;
        SpriteSelectionRef sprite;
        TriggerSelectionRef trigger;
        ObjectSelectionRef object;
        HorizontalSurfaceRef horizontal;
    } value;
} SelectionTarget;

typedef struct {
    SelectionTarget target;
    double distance;
    bool valid;
} EditorHit;

#endif /* EDITOR_TYPES_H */
