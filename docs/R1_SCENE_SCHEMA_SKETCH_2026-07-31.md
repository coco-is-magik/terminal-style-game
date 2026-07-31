# R1 Representative Scene Schema and Migration Sketch — 2026-07-31

## Purpose and non-goals

This sketch makes the accepted R1 decisions reviewable. It is not an implemented
format specification and does not freeze punctuation, extension, maximum sizes,
floating representation, or the final parser API. R2 must resolve those details in
an approved implementation plan and versioned format specification.

Scene v1 deliberately represents the smallest complete-scene foundation compatible
with current behavior. Explicit floor/ceiling heights, wall faces, independent
collision/optical records, and slopes arrive through later versioned migrations in
R4/R8; they are requirements now, not fictional v1 fields.

## Representative native scene v1

```ini
scene_type = terminal_scene
scene_version = 1
name = First Room
width = 10
height = 6
origin_x = 0
origin_y = 0
next_instance_id = 3
ambient_intensity = 1.0
spawn = 1.5,1.5,0.0

[cells]
1111111111
1000000001
1000000001
1000000001
1000000001
1111111111

[light 1]
position = 2.5,2.5,0.5
intensity = 1.0
radius = 4.0
anti_light = false

[decal_instance 2]
asset_kind = decal_pattern
asset_id = 6
surface = wall,3,2,north
offset = 0.0,0.0
scale = 1.0
rotation_quarter_turns = 0
```

Semantics shown here:

- type and version are mandatory and unambiguous;
- dimensions are checked before allocation;
- `next_instance_id` is greater than persisted IDs `1` and `2`;
- lights and placed decals are scene-owned instances;
- decal pattern `6` is a typed reusable-asset reference, not embedded mutable art;
- current cell rows preserve current occupancy/material behavior in v1;
- no runtime light map, resolved pointer, cache, selection, or undo history is saved.

## Legacy digit-grid import

Legacy source:

```text
11111
10001
10201
11111
```

Deterministic imported candidate:

```text
scene_type            = terminal_scene
scene_version         = 1
width / height        = accepted padded legacy dimensions
origin                = 0,0
cells                 = exact accepted legacy material IDs
ambient_intensity     = documented current default
spawn                 = documented current spawn/default policy
placed instances      = empty
next_instance_id      = 1
source provenance     = legacy import metadata if R2 elects to preserve it
native destination    = unset until explicit Save As
```

The import transaction never modifies the legacy bytes. Saving requires a new native
destination. Ragged-row padding and accepted legacy-character behavior must either
match the current loader exactly or be deliberately revised by R2 requirements with
compatibility tests; it may not drift accidentally.

## Future height-aware schema direction

A later schema may represent concepts like:

```ini
[cell 3,2]
floor_height = 0.0
ceiling_height = 1.0
floor_surface = material,2
ceiling_surface = material,3

[boundary 3,2,north]
present = true
collides = true
visual_surface = material,4
blocks_sight = true
blocks_light = true
```

This is a semantic example only. It demonstrates independent properties and one
vertical interval per X/Y. It must not be copied into scene v1 or implemented before
R4/R8 choose storage, adjacency, interpolation, minimum clearance, and migration.

## Validation examples and exact planned diagnostics

### Missing required property

```ini
scene_type = terminal_scene
# scene_version missing
```

Result: reject candidate with `TSG-SCENE-INPUT-0002`; preserve live document.

### Duplicate instance identity

```ini
[light 7]
...
[decal_instance 7]
...
```

Result: reject candidate with `TSG-SCENE-INPUT-0008`. The namespace is scene-wide,
so different kinds do not permit the same instance ID.

### Invalid high-water mark

```ini
next_instance_id = 7
[light 7]
...
```

Result: reject with `TSG-SCENE-INPUT-0009`; the next ID must exceed every allocated
or persisted retired ID represented by the chosen format policy.

### Missing reusable asset

```ini
[decal_instance 2]
asset_kind = decal_pattern
asset_id = 999
```

Result: load the otherwise valid scene in repair mode with
`TSG-SCENE-INPUT-0011`. Preserve `{decal_pattern, 999}`, display the reserved
fallback, and block normal Save. A Save attempt returns
`TSG-SCENE-INPUT-0012` until a validated repair replaces the authored reference.

### Unknown current-version field

```ini
[light 1]
unicorn_power = 12
```

Result: reject with `TSG-SCENE-INPUT-0004`. Do not ignore or round-trip-drop it.

### Unsupported newer version

```ini
scene_type = terminal_scene
scene_version = 99
```

Result: reject with `TSG-SCENE-INPUT-0005`; preserve live state and report the
supported range.

### Save environment failure

If a valid scene cannot create its same-directory temporary file, return
`TSG-SCENE-ENV-0003`, log it once at the document/application boundary, preserve the
old destination bytes, and keep the document dirty.

## Candidate-state model

R2 should keep these states distinct:

```text
empty/uninitialized
valid clean native document
valid dirty native document
valid repair-required document (clean or dirty relative to loaded bytes)
valid imported-unsaved document
rejected candidate (never externally visible as live state)
```

Cancel, no-change, and optional absence are typed statuses rather than errors.
Repair-required is a valid document state accompanied by one or more input
diagnostics; it is not silent success.

## Transaction examples

### Failed load

1. Existing document A remains live.
2. Candidate B fails validation.
3. Candidate B is fully released.
4. Exact diagnostic ID and bounded context return to the owning boundary.
5. Boundary logs once and keeps A, its history, selection, path, dirty state, and
   runtime view unchanged.

### Successful repairable load

1. Candidate B is structurally valid.
2. One reusable asset reference is unresolved.
3. Candidate B records the unresolved typed reference and diagnostic.
4. Candidate B commits once with repair-required state.
5. Runtime view uses a visible reserved fallback without mutating authored data.
6. Normal Save remains disabled until repair.

### Failed Save replacement

1. Serialize to a same-directory temporary path.
2. Replacement fails with `TSG-SCENE-ENV-0005`.
3. Previous destination remains authoritative.
4. Document remains dirty with unchanged path/saved identity/history.
5. Diagnostic reports whether a recoverable temporary file remains and what action
   the user may take.

## R2 format questions still intentionally open

- native extension and filename policy;
- exact grammar and escaping;
- exact maximum dimensions/counts/string lengths;
- decimal versus fixed-point persisted numeric representation;
- whether scene v1 preserves import provenance;
- representation of retired-ID high-water state beyond `next_instance_id`;
- deterministic section ordering;
- temporary-file cleanup and crash-durability level;
- exact visible fallback definitions and repair UI.

These are bounded R2 planning decisions, not permission to reopen R1's world,
ownership, identity, compatibility, or validation policies.