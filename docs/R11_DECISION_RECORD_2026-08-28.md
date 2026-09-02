# R11 Decision Record — Sprites, Triggers, and Spawn — 2026-08-28

## Authority

This record captures the product and architecture decisions made in the 2026-08-28
design thread before R11 implementation begins. The decisions are binding for all
R11 increments and are grounded in existing code. The authoritative requirements
and increment plan is `R11_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-28.md`.

## Scope

R11 is "Sprites, animation, objects, triggers, and spawn authoring" from
`docs/FEATURE_ROADMAP.md`. The roadmap lists six required outcomes. This Q1 record
bounds which outcomes ship now and which are deferred with a written stop boundary.

## Code-grounding findings at decision time

- Sprite assets already exist as data: `SpriteAsset` is a 2D glyph/material pattern
  grid (`src/assets.h:74-84`) held in `AssetRegistry.sprites[0..255]` (index 0 null,
  `SPRITE_ID_CAPACITY`). Generic sprite rendering is deliberately not implemented.
- Sprite instances exist only as a runtime stub: `SpriteEntity { Vec2 pos; int
  sprite_id; }` (`src/world.h:68-71`), `MAX_SPRITES = 128`, `world_add_sprite()`
  implemented; nothing renders them and nothing persists them.
- `src/raycast.c` already maintains a per-column z-buffer
  (`raycast_heightfield_column_depth`, `z_buffer[x]`) and per-cell
  `grid->world_depths`, plus a decal overlay drawn with an
  `DECAL_OCCLUSION_EPSILON` depth discipline (`src/raycast.c:220-378`). The
  classic depth-sorted billboard model reuses this existing seam.
- `SceneInstanceId` is a scene-wide unsigned 64-bit namespace with `0` reserved,
  `UINT64_MAX` exhausted, and a persisted `next_instance_id` high-water mark
  (`src/scene_types.h:34,107-109`; `src/scene_document.h:53`).
- Authored spawn already exists: `SceneDocument` owns `spawn_pos`/`spawn_angle`,
  serialized and round-tripped; `world_init()` defaults to `(1.5, 1.5)` facing east
  (`src/world.h:107-109`, `src/world.c:45-49`, `tests/test_scene_document.c:184`).
- `vertical_physics` is the established pure-runtime boundary precedent: it
  consumes borrowed scene data, mutates runtime camera/state only, and owns no
  authored state, rendering, input, allocation, or global time
  (`src/vertical_physics.h:1-6`).
- Scene v7 is the current native format with strict unknown-key parsing; every
  prior version migrates forward. R10 locked the invariant that any scene version
  bump must be paired with a benchmark gate to measure the new workload.

## 1. D1 — Scope: three increments, three deferred outcomes

**Decision:** Scope B. I1 sprite **runtime**; I2 sprite **authoring and
persistence** (scene v8); I3 one minimal **trigger** pair. Animation, objects
(typed component-style data), and game-mode spawn expansion are deferred with a
written stop boundary.

**Rejected alternatives (recorded for evidence):**

- **All six outcomes in one phase:** contradicts the roadmap's own increment
  pattern (R5 I1–I4, R10 I1–I3) and its forbidden shortcut "no authoring UI
  before runtime semantics exist and are tested."
- **Minimal static-sprite-only scope:** the roadmap exit gate requires
  round-trip, reference validation, undo/redo, and missing-asset safety, none of
  which can be evidenced without authoring.

## 2. D2 — Rendering model: depth-sorted billboard

**Decision:** Classic depth-sorted billboard over the existing per-column
z-buffer. Sprites project as an upright glyph pattern; each pattern cell is drawn
only where its computed depth passes the same epsilon discipline already applied
to decals (`DECAL_OCCLUSION_EPSILON`).

**Validation against D3:** decorative semantics means the sprite never writes
`world_depths` and never occludes raycasts; it is drawn only into already-visible
columns, so the raycast, mirror, and light-shadow paths are untouched.

**Rejected alternatives (recorded for evidence):**

- **Cell-grid glyph tile:** smallest possible change but reads as an animated
  tile rather than an object and has no depth relationship with walls, decals, or
  optics.
- **Decal-style surface projection:** conflates "object in space" with "surface
  decoration"; only supports pinned-to-surface behavior and misuses the decal
  cache/undo machinery.

## 3. D3 — World semantics: decorative and light-map-lit

**Decision:** Sprites are decorative initially: no collision, no sight-ray
blocking, no light shadowing, no mirror reflection. Their pattern cells are lit
by the light level at the sprite's anchored position through the existing
per-channel `palette_sample` scaling, so colored/anti-light maps affect sprites
consistently with floors.

**Implication:** the raycast, mirror, lighting, and lighting-cache suites keep
their current behavior and tests. Solid sprites become a later, separately gated
increment with its own performance model and test expansion.

## 4. D4 — Persistence, identity, and runtime boundary

**Decision:**

1. Scene **v8** adds an authored `[sprites]` block: typed sprite records
   (`id`, `asset_ref`, `x`, `y`), mirroring the light/decal record precedent with
   round-trip, load-time validation, and a v7→v8 migration where older files gain
   an empty sprite list and unchanged strict unknown-key parsing.
2. A new `SCENE_ASSET_KIND_SPRITE_PATTERN` asset kind joins
   `SCENE_ASSET_KIND_DECAL_PATTERN`; `AssetRegistry` remains the owner of
   definitions and scenes store validated references. Missing patterns produce
   repair diagnostics at load, matching the decal precedent.
3. Runtime logic moves into a new pure **entity/trigger session module** with an
   explicit `tick(delta_seconds)` interface, following the `vertical_physics`
   boundary precedent; the renderer and editor are adapters.
4. Undo/redo uses the existing command system with stable `SceneInstanceId`s
   preserved, matching R5/R6 identity rules.

## 5. D5 — Trigger/action vocabulary: closed typed enums

**Decision:** Ship one minimal trigger pair initially: a typed condition
(`enter_region`) and a small closed action enum (`set_flag`,
`teleport_to_spawn`, `toggle_light`). Every trigger/action target reference must
resolve to an existing scene-owned ID at load; unknown or dangling references
fail load with repair diagnostics. Code supplies validated behavior handlers;
trigger records are versioned data.

**Rejected alternatives (recorded for evidence):**

- **Action strings:** even validated strings buy nothing over typed enum IDs and
  add a parsing/error surface; the roadmap forbids accepting action strings
  without validation and this decision removes the hazard entirely.
- **Generic data-defined component tables:** materially larger spec and
  validation surface with no current requirement behind it.
- **Unrestricted scripting:** explicitly forbidden by the roadmap without
  separate requirements.

## Invariants (do not change)

- Scene v7 grammar and the colored/spot lighting runtime stay stable; scene v8 is
  additive.
- Any sprite render path enters the performance gate: extend the render-benchmark
  family with a paired sprite workload and record measurement before shipping.
  No "fits in budget" claim without a recorded measurement (R10 invariant).
- No authoring UI before the I1 runtime is implemented and tested (roadmap
  outcome 1 ordering).
- No unrestricted scripting, no unvalidated action references, no coordinate-only
  identity, no dangling trigger references.
- `MAX_SPRITES` and `SPRITE_ID_CAPACITY` bounds are enforced at every authoring
  and load boundary and covered by tests.

## Open follow-ups (recorded, not blockers)

- Animation data, playback, timeline, and authoring (roadmap outcome 3) —
  deferred.
- Objects through typed data/components (roadmap outcome 4) — deferred.
- Game-mode-dependent spawn expansion (roadmap outcome 6) — requires a game-mode
  requirement; today's single authored spawn (R1/R2) already satisfies the
  baseline "spawn authoring" outcome.
- Solid/occluding sprites: collision, sight-ray blocking, light shadowing, and
  mirror visibility — later gated increment with its own benchmark.
- Sprite asset authoring format (glyph/material grid file) — defined with the I2
  authoring increment; I1 consumes whatever sprite assets exist at runtime.
- Exact sprite lighting anchor rule and whether a dedicated per-cell fallback is
  needed — resolved and recorded in `R11_INCREMENT_I1_IMPLEMENTATION_RECORD`.