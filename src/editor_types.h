/**
 * editor_types.h — Shared types for the unified in-world editor
 *
 * Header-only foundation used by SceneDocument, command history,
 * selection, and the unified editor controller. No floor/ceiling/
 * entity-ID types until a later phase needs them.
 */

#ifndef EDITOR_TYPES_H
#define EDITOR_TYPES_H

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

typedef enum {
    SELECTION_NONE = 0,
    SELECTION_WALL_FACE
} SelectionType;

typedef struct {
    SelectionType type;
    union {
        WallFaceRef wall_face;
    } value;
} SelectionTarget;

typedef struct {
    SelectionTarget target;
    double distance;
    bool valid;
} EditorHit;

#endif /* EDITOR_TYPES_H */
