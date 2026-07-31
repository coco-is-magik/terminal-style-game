# R1 World, Scene, Ownership, and Identity Decision Plan — 2026-07-31

## Status and scope

**Decision-complete.** This document resolves the six R1 outcomes required by
`FEATURE_ROADMAP.md`. It is a requirements and architecture decision record, not an
R2 implementation plan. No native scene parser, serializer, migration, runtime
adapter, vertical renderer, or editor feature is implemented by R1.

R0 Review B found no blocker before this work. The accepted decisions were made
with the user on 2026-07-31 after project-local research of current map, world,
renderer, editor, asset, persistence, ownership, and test boundaries.

## Goal

Give R2 and later phases one coherent answer for:

1. world topology and vertical capability;
2. native scene packaging and compatibility;
3. geometry, collision, appearance, and optical semantics;
4. stable instance identity and references;
5. authored, reusable, runtime, and cache ownership;
6. initial schemas, migration, validation, diagnostics, and transaction boundaries.

## Decision summary

| Decision | Accepted policy |
|---|---|
| World topology | Height-aware 2.5D grid; one traversable vertical interval per X/Y; no stacked traversable spaces |
| Scene package | One versioned text scene file; reusable assets remain separate referenced documents |
| Legacy compatibility | Explicit import to a new native scene; legacy source is preserved; no native downgrade |
| Domain semantics | Geometry, collision, visual appearance, sight blocking, light blocking, and future optics are independent typed properties |
| Stable identity | One scene-wide monotonic persisted `uint64_t` namespace; zero invalid; IDs never reused |
| Ownership | `SceneDocument` owns all authored scene state; `AssetRegistry` owns reusable definitions; runtime views/caches are derived |
| Invalid structure | Reject candidate transactionally and preserve live state |
| Missing reusable assets | Load visibly in repair mode, preserve unresolved typed reference, block normal Save until repaired |
| Diagnostics | No silent abnormal outcomes; permanent cataloged IDs; exact-once boundary logging |

## Required behavior

### 1. Height-aware 2.5D world contract

- There is one logical cell at each X/Y grid coordinate.
- The model may represent floor and ceiling heights, slopes/ramps, raised platforms,
  pits, player/camera Z, vertical collision, and true angular pitch in later phases.
- At any traversable X/Y position, at most one floor/ceiling interval is walkable.
- The model does not represent bridges over tunnels, stacked rooms, or two
  independently traversable spaces at the same X/Y.
- X/Y remains grid-relative; one cell remains one horizontal world unit unless a
  separately approved format migration changes that contract.
- Persisted vertical values must be bounded, finite, deterministic, and validated.
  The precise numeric representation and slope interpolation belong to R4/R8 plans.
- Future height schemas must enforce a positive documented minimum clearance and
  reject impossible floor/ceiling relationships.
- Current `Camera.pitch` remains a grid-row horizon displacement until R8 explicitly
  implements angular pitch; R1 does not rename existing behavior retroactively.

### 2. Native scene and legacy compatibility

- One native scene file owns all scene-authored state needed to open the complete
  scene transactionally.
- The file has an unambiguous type discriminator and integer schema version.
- Reusable material, palette, decal-pattern, sprite, animation, and UI definitions
  remain separate asset documents referenced by typed stable asset identifiers.
- Legacy digit-grid map files are accepted through an explicit import adapter, not
  treated as native versioned scenes.
- Import builds and validates a complete candidate scene before commit.
- Import never modifies the legacy source. The first native Save requires an
  explicit native scene destination.
- Native scenes never silently save or downgrade to the legacy digit-grid format.
- Unsupported/newer native versions and unknown current-version fields reject; no
  field is silently discarded.
- R2 scene v1 is a compatibility foundation. It must not pretend to implement the
  R4 surface model or R8 vertical world early. Those phases receive explicit later
  schema versions and migrations.

### 3. Independent authored semantics

The long-term scene model must represent these concepts independently:

- geometry or boundary existence;
- player/entity collision behavior;
- visual surface and reusable material reference;
- camera/sight-ray blocking;
- light-ray blocking/transmission;
- future opacity, translucency, reflectivity, and other optical properties.

Defaults and legacy migration preserve current behavior: a positive legacy wall
cell becomes present, collidable, visible, sight-blocking, and light-blocking; a
zero cell becomes absent/passable/non-blocking. Independent representation permits
future invisible colliders, visible non-colliders, windows, and mirrors without
magic material IDs.

R1 does not choose every future optical enum or implement layered rendering. R4
defines basic surface records; R9 defines layered optical behavior after geometry
and vertical semantics are stable.

### 4. Stable identity and references

- `SceneInstanceId` is an unsigned 64-bit persisted value.
- `0` is reserved as invalid/null.
- Lights, placed decal instances, sprites/objects, triggers, and future scene
  instances share one scene-wide namespace.
- Each scene persists `next_instance_id` as a high-water mark greater than every
  existing and retired allocated ID.
- IDs increase monotonically and are never assigned to a different logical
  instance after deletion, undo, redo-branch truncation, save, reload, or migration.
- Successful creation consumes an ID only when its command can commit. Namespace
  exhaustion rejects creation without mutation.
- Undo of creation removes the instance but retires its ID; redo restores that ID.
  Undo of deletion restores the complete prior instance with its original ID.
- Movable-instance selection, commands, triggers, and references use IDs, never
  array indices, pointers, or coordinates as persistent identity.
- Reusable assets use typed references such as `{asset_kind, asset_id}`. Numeric
  equality across asset kinds does not imply shared identity.
- Duplicate, zero, wrong-kind, dangling, or invalid high-water references are
  rejected according to `ERROR_CATALOG.md`.

### 5. Ownership and dependency direction

```text
versioned scene file          reusable asset files
         |                             |
 parse / migrate / validate    parse / validate
         v                             v
  SceneDocument                  AssetRegistry
  authoritative authored         reusable definitions
         |                             |
         +------ resolve through typed references ------+
                               |
                      immutable runtime views
                               |
             renderer / collision / lighting / simulation
                               |
                       derived caches only
```

- `SceneDocument` is the sole owner of scene geometry, surfaces when introduced,
  ambient settings, spawn, placed instances, stable-ID allocator state, dirty/repair
  state, path identity, and scene command history.
- `AssetRegistry` owns reusable definitions. A scene stores references, not private
  mutable copies, unless a future asset format explicitly defines embedded assets.
- `WorldState` must not remain a second owner of authored placed content. R2 either
  converts it to a derived runtime view or splits its consumers into narrow runtime
  adapters. The transition must preserve current rendering while eliminating copy-
  synchronization as an authoring model.
- Renderer, collision, lighting, and simulation borrow immutable authoritative or
  derived views through narrow APIs. They do not mutate the document.
- Resolved pointers, light maps, spatial indices, projection data, renderer state,
  and caches are derived, invalidatable, reconstructible, and never serialized.
- UI invokes typed document commands. `app.c`, widgets, and serializers do not
  directly mutate authored storage.
- Review B finding RB-F1 is handled incrementally: R2 adds narrow editor queries
  only where the scene/runtime boundary concretely needs them; no speculative opaque
  rewrite is required.

### 6. Parse, migration, validation, repair, and commit

Native load follows one explicit transaction:

```text
read bounded bytes
  -> identify type/version
  -> parse untrusted syntax into candidate data
  -> migrate a supported older native version, if applicable
  -> validate structure, bounds, IDs, and references
  -> resolve reusable assets under strict/repair policy
  -> construct complete candidate document/runtime views
  -> commit once
```

Legacy import substitutes a deterministic import stage after format detection. No
stage before final commit may mutate the live document or document-dependent editor
state.

Structural failures reject the candidate. These include malformed syntax, missing
or duplicate required fields, unknown current-version fields, unsupported versions,
invalid dimensions or arithmetic, non-finite values, duplicate/zero IDs, invalid
high-water marks, and dangling references between scene instances.

A missing reusable asset is repairable because the scene itself can remain
structurally coherent:

1. commit the scene with a diagnostic list and repair-required state;
2. preserve the unresolved typed asset reference exactly;
3. render a conspicuous reserved fallback rather than hiding the object;
4. block normal Save until every unresolved reference is repaired;
5. do not silently rewrite the authored reference to the fallback.

### 7. Save transaction

Native Save follows:

```text
validate live document and repair state
  -> serialize deterministically to a same-directory temporary file
  -> write, flush, close, and perform any explicitly promised durability step
  -> atomically replace destination
  -> update saved identity/clean marker only after replacement succeeds
```

Any failure preserves the prior destination and document dirty/repair state. R2
must decide and document platform-specific temporary-file cleanup and whether it
promises crash durability beyond atomic replacement; it may not imply durability
that it does not implement and test.

### 8. Diagnostic contract

All new R2 paths follow `C_STYLE_AND_OWNERSHIP.md` and `ERROR_CATALOG.md`:

- no abnormal path is silent, including fallback and repair mode;
- one permanent ID identifies one exact detection case;
- lower layers return diagnostics; the owning boundary logs once;
- expected cancel/no-change/optional-absence states are typed `STATUS`, not errors;
- `INPUT`, `ENV`, and `BUG` remain distinguishable;
- every diagnostic documents context, recovery, preserved state, action, and test;
- internal invariant failures are bugs to fix, never relabeled user errors.

## Forbidden behavior

- No stacked-space claim, implicit sector graph, voxel model, or generic 3D engine.
- No world-model choice hidden inside one renderer/editor feature patch.
- No R4/R8 behavior smuggled into scene v1 without its own requirements and migration.
- No material ID overloaded as geometry, collision, visibility, and optics in the
  long-term model.
- No second editable map/world copy or synchronization-based authored ownership.
- No persistent array index, pointer, cache record, resolved pointer, or coordinate
  identity for movable instances.
- No ID reuse, including after undo-history branch removal.
- No in-place legacy conversion, silent native downgrade, unknown-field discard, or
  lossy missing-reference substitution.
- No partial load/save commit or dirty-state reset before successful replacement.
- No serializer or migration policy in UI widgets, renderer, or `app.c`.
- No global mutable last-error state, vague generic error code, duplicate logging, or
  silent fallback.
- No source implementation begins until an independently scoped R2 plan passes Q1.

## R2 planning requirements

Before R2 implementation, its plan must specify:

1. exact scene-v1 grammar, limits, default values, ordering, and extension policy;
2. exact legacy import mapping and source/destination workflow;
3. native path/extension and scene identity rules;
4. concrete `SceneDocument` data/API transition and `WorldState` disposition;
5. typed diagnostic/result API and exact logging owner;
6. asset fallback identity, appearance, repair workflow, and Save-block UI;
7. atomic-save platform behavior, temporary-file recovery, and durability claim;
8. deterministic serializer ordering and numeric representation;
9. allocation/capacity limits and checked arithmetic;
10. full tests below and an incremental implementation sequence.

## Required deterministic evidence for R2

- native parse/serialize/parse semantic equivalence;
- deterministic serialized bytes for equivalent scenes;
- legacy import maps every accepted current cell exactly and leaves source unchanged;
- imported current maps render and collide equivalently before later feature changes;
- malformed, missing, duplicate, unknown, and newer-version fields return exact IDs;
- dimensions, overflow, capacity, finite-number, allocation, and I/O failures preserve
  the live document;
- zero, duplicate, exhausted, invalid-high-water, dangling, and wrong-kind IDs reject;
- create/delete/undo/redo/save/reload preserve identity and never reuse IDs;
- missing asset enters visible repair mode, preserves its authored ID, and blocks Save;
- failed Save preserves destination bytes, path identity, dirty state, and history;
- runtime adapters derive from authoritative state and cannot mutate it;
- each abnormal path returns/emits the exact documented ID and logs exactly once;
- expected statuses emit no error log;
- aggregate, sanitizer, leak, matrix, smoke, and interactive scene workflows pass as
  applicable under Q2/Q3.

## R1 exit assessment

R1's six decisions are resolved, representative schema and migration examples are
provided in `R1_SCENE_SCHEMA_SKETCH_2026-07-31.md`, and alternatives/consequences are
recorded in `R1_DECISION_RECORD_2026-07-31.md`. The requirements identify owners,
forbidden behavior, diagnostics, failures, tests, and R2 entry conditions.

R1 may be marked **Verified** after documentation consistency and link checks find no
contradiction. That status authorizes R2 planning only, not implementation.