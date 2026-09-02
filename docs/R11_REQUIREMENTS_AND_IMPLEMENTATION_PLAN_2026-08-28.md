# R11 Requirements and Implementation Plan — Sprites, Triggers, and Spawn — 2026-08-28

## Status

**Q1 decision record locked and I1–I3 increment plan recorded on 2026-08-28.**
The Q1 decision record is `R11_DECISION_RECORD_2026-08-28.md`; this document is
the Q2/Q3 increment plan. **I1 is Implemented and its automated exit gate passed
on 2026-08-28.** I2 scene v8 authoring code is present in the working tree
(commit `277fb3c`, 2026-09-02); its automated exit gate passed and the dedicated
implementation record now exists. Bundled I1+I2 manual visual/input acceptance
passed after the 2026-09-02 workflow-polish follow-up. R10 is released;
prerequisites are satisfied. The file-by-file path from this baseline through I3
and the deferred-outcome decision gates is recorded in
`R11_TRACK_A_COMPLETION_PLAN_2026-09-02.md`.

## Scope

1. **I1 — Sprite runtime.** Depth-sorted billboard rendering of decorative,
   light-map-lit `SpriteAsset` instances from `WorldState.sprites`, deterministic
   and headless, benchmark-gated.
2. **I2 — Sprite authoring.** Scene v8 `[sprites]` block,
   `SCENE_ASSET_KIND_SPRITE_PATTERN` references, v7→v8 migration, undo/redo,
   painter-style placement and selection.
3. **I3 — Minimal trigger pair.** Authored trigger regions with a closed typed
   condition/action vocabulary, validated references, runtime firing in the
   entity/trigger session, and round-trip.

Deferred with a written stop boundary: animation, objects, and game-mode spawn
expansion.

Locked decisions (Q1) in `R11_DECISION_RECORD_2026-08-28.md` are binding for all
increments.

## Prerequisites

- R2–R3: scene-owned identity (`SceneInstanceId`, persisted `next_instance_id`
  high-water), command history, scene ownership, authored spawn.
- R8: final geometry semantics (occupancy, vertical cells, heightfield).
- R9/R10: current renderer with per-column z-buffer and decal depth discipline,
  per-channel optical composition, colored/spot lighting, and the 6 ms frame
  budget precedent with the "no unmeasured budget claims" invariant.

## I1 — Sprite runtime

**Goal:** Sprites are visible, correctly depth-ordered against world geometry,
lit by the light map, deterministic, and measurable. No authored nor persisted
data changes.

**Task set:**

1. Implement billboard projection of `SpriteEntity` (pos, `sprite_id`) into
   renderer columns using the existing z-buffer; per-cell depth checks reuse the
   `DECAL_OCCLUSION_EPSILON` discipline. Render only into already-visible
   columns; never write `world_depths` and never occlude raycasts.
2. Resolve sprite pattern cells from `AssetRegistry.sprites[]`; cells compose
   through the existing per-channel `palette_sample` path so lighting and
   translucency apply consistently.
3. Correct depth sorting across multiple sprites and against walls/floors/decals.
4. Absent or invalid `sprite_id` assets render as no-ops without crashing or
   corrupting the frame (missing-asset safety).
5. Add the paired `benchmark-sprite-render` workload to the render-benchmark
   family and record budget evidence before shipping the increment.

### Increment exit-gate (I1)

Deterministic tests cover: projection math for a single sprite; depth ordering
behind walls and in front of farther geometry; two-sprite ordering; light-map
lit and anti-light dimmed appearance; missing-asset no-op; zero-sprite frame;
benchmark determinism and recorded vs budget measure. No scene-format, editor,
or lighting changes occur in I1.

Strict `-Wall -Wextra -Wpedantic -Werror`, ASan/UBSan, `make check`, smoke, and
diff check pass.

**Implementation status (2026-08-28):** Implemented; automated exit gate passed.
See `R11_INCREMENT_I1_IMPLEMENTATION_RECORD_2026-08-28.md`. Existing sprite
asset loading was already present and required no loader code change. Runtime
projection normalizes each sprite pattern to one world unit tall, preserves its
`cols/rows` aspect ratio, anchors it to the local floor, and shares the decal
overlay-depth frontier. Manual visual acceptance remains bundled with I2.

## I2 — Sprite authoring and scene v8

**Goal:** Sprite instances are authored, validated, persisted, and reversible,
matching light/decal authoring quality. Runtime exists first (I1), so this
increment only adds the authoring seam.

**Task set:**

1. Add `SCENE_ASSET_KIND_SPRITE_PATTERN` to the asset-kind enum and carry sprite
   pattern references in scene sprite records.
2. Scene v8 `[sprites]` block: typed records (`id`, `asset_ref`, `x`, `y`);
   canonical writer emits the block; v7→v8 migration adds an empty list; older
   files remain readable; strict unknown-key parsing unchanged.
3. `SceneDocument` owns the authored sprite list (mirroring lights/decals);
   `WorldState` rebuild becomes the derived runtime view.
4. Editor placement and selection plus undo/redo via the existing command system
   with stable IDs; `MAX_SPRITES`/`SPRITE_ID_CAPACITY` bounds reported as
   validation errors, never silently dropped.
5. Missing patterns produce load repair diagnostics; save/reopen round-trips
   exactly.

**Implementation status (I2, 2026-09-02):** Implemented and verified, including
bundled I1+I2 manual acceptance — `SCENE_ASSET_KIND_SPRITE_PATTERN`,
`SceneSpriteInstance`, `scene_format_migrate_v7_to_v8`, the `[sprite_instance]`
parser/writer, `SceneDocument` ownership, `command_history_{insert,set,remove}_sprite`,
and editor `P`-key placement/selection with sprite inspector fields. Evidence is
recorded in `R11_INCREMENT_I2_IMPLEMENTATION_RECORD_2026-09-02.md`.

### Increment exit-gate (I2)

Deterministic tests cover: place/select/move/remove sprite, undo/redo preserves
IDs, v7 fixture migrates to v8, strict parser rejects unknown keys, missing
pattern repair diagnostic, capacity exhaustion, canonical round-trip, and paired
lighting/runtime rebuild after edits. Benchmark continues to pass with authoring
not affecting the frame loop.

## I3 — Minimal trigger pair

**Goal:** One authored condition and closed action vocabulary validate at load,
fire deterministically, and round-trip.

**Task set:**

1. Authored trigger records (condition `enter_region` with a world-space region;
   actions from the closed enum `set_flag`, `teleport_to_spawn`,
   `toggle_light`).
2. Load-time validation: every action target resolves to an existing scene-owned
   ID; dangling references fail load with repair diagnostics.
3. Runtime firing lives in the pure entity/trigger session
   (`tick(delta_seconds)`); the module owns no rendering or input.
4. Trigger records persist in the scene format with another additive version
   bump and migration; undo/redo covers trigger authoring.

### Increment exit-gate (I3)

Deterministic tests cover: enter/leave region firing exactly once per distinct
entry, closed-enum rejection of unknown actions, dangling-reference repair, flag
state transitions, teleport-to-spawn, toggle-light, round-trip, and undo/redo.
Cross-check against the R10 invariant: trigger work must not raise the lighting
or render benchmark above budget.

## Deferred and stop boundary

- Animation data, playback, timeline, and authoring (roadmap outcome 3).
- Objects through typed data/components (roadmap outcome 4).
- Game-mode-dependent spawn expansion (roadmap outcome 6) — deferred until a
  game-mode requirement exists; the single authored spawn remains the baseline.
- Solid/occluding sprites, sprite collision, sight blocking, light shadowing,
  and mirror visibility — separately gated future increment with its own
  performance model.

Each deferred item requires separate authorization before implementation
begins; none may be smuggled into an existing increment's task set.

## Review checkpoint

Q4 review after I3 (or earlier if major entity/trigger boundaries stabilize
before then), per the roadmap checkpoint.

## Verification evidence convention

Each increment records its own `R11_INCREMENT_I{1,2,3}_IMPLEMENTATION_RECORD`
with named tests, benchmark numbers and methodology, sanitizer status, and the
manual acceptance checklist. Claims follow the `docs/` evidence vocabulary
(Implemented / Verified / Tested / Benchmarked / Expected / Unverified).