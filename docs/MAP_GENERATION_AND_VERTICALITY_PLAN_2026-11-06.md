# Bounded map generation, verticality, and portals — plan

**Status: recorded 2026-11-06 from an owner discussion. Nothing implemented. The decisions in
§10 are open and are the gate for any work.**

This is the requirements-and-approach record for generating bounded maps offline and bringing
them into the editor, plus the verticality/portal vocabulary that makes tall worlds possible
inside a column raycaster. Roadmap position: **V1-G**, a cross-phase enabler (see
`FEATURE_ROADMAP.md`).

## 1. What exists today (the constraint set)

- The world is a **bounded dense grid**: `Map.cells` is a heap `width × height` array of
  `int material_id` (`0` = void), with a parallel light map. Dimensions are dynamic at
  runtime but capped by `SCENE_MAX_WIDTH 512` / `SCENE_MAX_HEIGHT 256`.
- **Per-cell cost ≈ 20–24 B**: 4 B material + light map + **16 B** `SceneAuthoredCell`
  (static-asserted as an R8 memory budget), paid for every cell whether authored or not.
- **Verticality is a heightfield.** Authored cells carry `floor_height_step`,
  `ceiling_height_step`, `gravity_scale_step`, `gravity_orientation`, and floor/ceiling
  presence. There are **no overhangs, no stacked levels, and no portals** in the model.
- The renderer is a **column raycaster**, and **camera pitch is a horizon offset, not true
  angular pitch** (`TODO.md` constraint 4). You cannot look up or down in the way that would
  expose a vertical trick.
- **No streaming**: nothing generates during travel; a map is loaded whole.
- The only ways into a `SceneDocument` are **loading a file** or
  `scene_document_create_new`, which hardcodes a 10×6 bordered room. There is **no
  "adopt this generated Map"** seam.
- The editor already opens `.tscene` files and the map chooser lists them, so *anything*
  written as a scene is importable with no new path.
- Runtime project execution (playing a scene) is not implemented; that is **V1-12**.

## 2. The approach: a staged pipeline producing a baked scene

Generation is a **pure, staged pipeline** from `(algorithm profile, parameters, seed)` to a
`SceneDocument`, written atomically as a normal `.tscene`.

```
params + seed
  -> S1 base field     materials, floor/ceiling height steps
  -> S2 shape          carve / erode / build / subdivide / maze
  -> S3 validate       flood fill, seal borders, place spawn/exit, reject pockets
  -> S4 decorate       decals, lights, objects, triggers, sprites
  -> S5 link levels    portals + `game.flow` edges to sibling levels
  -> S6 serialise      atomic write; nothing is written on cancel
```

Why baked rather than runtime: the map is **bounded and pre-made** (owner requirement), the
editor imports it for free (it *is* a scene), there is no runtime cost or streaming, the
result is diffable and hand-editable afterwards, and it is reproducible from the seed.

Stages exist so that **progress**, **cancel**, and **composition** are possible: each stage
reports `(stage, units_done, units_total)`.

## 3. Progress and cancellation

| Option | Assessment |
|---|---|
| **Sliced generation inside the frame loop** (recommended) | Runs a millisecond budget per frame — the codebase already thinks in "min spare time" budgets. No threads, no locks, no new sanitizer exposure. Progress bar and a cancel key fall out naturally. Output stays deterministic because stage results do not depend on slicing. |
| Background thread + atomics | Better wall-clock on giant maps, but the app is single-threaded with SMC state tracking in the render path; owes a thread-safety review first. |
| Separate process (`tools/mapgen` CLI) | Terminal progress bar; used for batch, preview, CI, and as the escape hatch when a map is too big to generate in-app. |

All three front-ends sit on one shared generator library, so results are identical. Cancel
discards; the atomic write happens only at S6, matching the repository's save discipline.

## 4. Verticality and portals — the Doom vocabulary, made explicit

Verticality is achieved with **baked non-euclidean tricks**, not with true 3D geometry. The
horizon-offset camera is what licenses this and should be treated as deliberate.

- **Sectors never need to be physically adjacent.** "Connected by a doorway" is a design
  statement about the grid, not a geometric fact. Two spaces that read as one continuous
  place may be arbitrarily far apart in map space.
- **The cut rule (the algorithm decides when to stop).** Each level has a budget: vertical
  step range, cell count, or extent. When a feature would exceed it, the generator cuts at a
  **natural occluder** — door frame, corner bend, shaft mouth, a lip you cannot see past —
  emits `portal_out` there, starts `level_N+1`, emits `portal_in` at a matching pose, and
  adds a flow edge between them.
- **Continuity is preserved across the seam**: facing, eye height and momentum carry over, and
  the cut is always at geometry that hides the transition.
- **Overhangs** are expressible only as things you cannot see from the side: a raised walkway
  is an object/sprite above a floor, not new per-column geometry.
- **What cannot be faked**: a *see-through* connection between spaces that are not adjacent
  along the view ray. A door you walk through is fine; a window into a distant room requires
  true portal rendering (a sub-scene through a mask) and is out of scope for this plan.
- **Missing runtime pieces**: a within-map **teleport/portal trigger** (triggers and objects
  exist, so this is a new behaviour on existing data), **scene transition at runtime**
  (V1-12), and **editor support for authoring portal pairs** with a transition preview.

The generator therefore emits a **level graph**, not one monolithic map. That is both the
creative space the owner wants ("myhouse.wad"-style deliberate impossibility) and a smaller
memory problem per level.

## 5. Algorithms per biome

One **contract**, many **purpose-built** generators. A biome is its own passes tuned for a
unique feel, not one mega-algorithm with weights.

| Environment | Dominant passes |
|---|---|
| Caves | cellular automata (4–5 smoothing passes) or maze algorithms for winding tunnels; ceiling-height variation for volume; flood-fill sealing |
| Desert / dunes | gradient noise at 2–3 octaves → floor height steps; material banding by height; sparse scatter |
| Jungle | high-frequency height noise + dense scatter of objects/sprites; canopy via ceiling heights; let lighting carry the mood |
| Urban | street graph (grid or L-system) → BSP block subdivision → building footprints → per-building interior levels; furniture scatter |
| Dungeon | BSP rooms + corridors + door placement |

**Actor envelope is an input.** "Unique feel" is a gameplay property as much as an aesthetic
one — corridor width that fits the actor, street width for a chase, sightline length for
tension. Generators must take actor size (or an envelope profile) or they will produce caves
nothing can navigate. This ties generation to V1-13/V1-14.

## 6. Memory and map size

**Why the caps exist.** They were not chosen against a size target. `TODO.md` §"Current
constraints" records the fixed-size grid as a constraint of the original scene-format model,
and R8 separately budgeted the 16 B per-cell payload. Storage is already dynamic
(`Map.cells`, `light_map`, `authored_cells` are heap `width × height`); the caps are a
**policy limit**, not an allocation strategy.

**Blast radius — 21 sites, 8 files.** Three are real coupling (fixed arrays); the rest are
bounds checks:

| Site | Kind |
|---|---|
| `scene_document.h` — `east_growth[SCENE_MAX_WIDTH]`, `south_growth[SCENE_MAX_HEIGHT]` | fixed arrays in the document (~3 KB) |
| `scene_format.h` — the same two arrays in `SceneFormatCandidate` | fixed arrays on the parse path |
| `heightfield_trace.h` — buffer sized `SCENE_MAX_HEIGHT + 2` | fixed per-column trace buffer |
| `scene_format.c`, `scene_document.c`, `command_system.c` (`CMD_RESULT_MAP_LIMIT`), `editor_domain.c` | validation, limits, derived UI ranges |

**Cost per cell ≈ 20–24 B**, so 512×256 ≈ 3 MB, 1024×1024 ≈ 25 MB, 2048×2048 ≈ 100 MB,
4096×4096 ≈ 400 MB. Generation *speed* will not be the wall; memory and file size will be.

**Options**

1. **Raise the sane default** (2048×1024 ≈ 50 MB worst case, ~24 KB more fixed arrays). Cheap,
   immediate headroom; do it regardless.
2. **Build-time flag** (`-DSCENE_MAX_WIDTH=…`). The caps size structs, so this is legal only as
   a whole-project Makefile variable, never per file. A dev/CI knob, not a strategy.
3. **RAM-derived policy with a load-time guard.** `SDL_GetSystemRAM()` exists in the vendored
   SDL3. Derive an *acceptance threshold* from installed memory, estimate
   `width × height × per-cell bytes` from the scene header **before allocating**, and refuse or
   warn with a typed diagnostic. Keep the **format cap** a generous compile-time constant so a
   scene's validity never depends on the machine opening it: format limit fixed, policy budget
   derived, refusal a diagnostic rather than a parse failure.
4. **Shrink per cell (the real lever).** A **sparse authored layer** — default cell = zero,
   store only non-default cells chunked or hashed — drops a blank map from ~24 B to ~4–5 B per
   cell and makes gigantic maps affordable. The seam is narrow: `SceneHeightView` borrows
   `const SceneAuthoredCell *cells` + count, so it becomes an accessor and setters materialise
   entries. The light map could likewise be optional.

**Recommendation:** 1 + 3 now (raise the default, add the pre-allocation guard and the
warning); 4 when gigantic maps become a real goal. The load-time warning is unconditional and
cheap because the header carries the dimensions before anything allocates.

**Decided 2026-11-06 (owner): options 1 + 3.** The ceiling is raised to
**2048 × 1024** (`SCENE_MAX_WIDTH`/`SCENE_MAX_HEIGHT`, worst case ≈ 50 MB) and a RAM-derived
load-time guard refuses an oversized scene with a typed diagnostic.

Landed: the ceiling; the guard (`src/platform_memory.c` measures installed memory,
`scene_format_load_cost_bytes`/`scene_format_load_fits_budget` are the pure policy, the check
sits in `allocate_candidate_arrays` before any allocation, and the refusal is
`SCENE_DIAGNOSTIC_ENV_MEMORY_BUDGET` → `TSG-SCENE-ENV-0008` with its catalog entry). The policy
is a documented build-time default (`SCENE_LOAD_MEMORY_BUDGET_PERCENT`, 25%) so it can be raised
or lowered per host without touching the load path; unknown installed memory means no budget and
the pre-guard behaviour, not a guess.

Cost consequences checked before landing: the document and parse-candidate growth arrays go from
~3 KB to ~12 KB, and the heightfield trace scratch becomes
`SCENE_MAX_WIDTH + SCENE_MAX_HEIGHT + 2` = 3074 intervals. Verified: app and affected runners
build, `test-scene-format` covers the pure cost/budget arithmetic with injected values,
`make test` is green at the new limits, `make standards` exit 0.

Follow-up recorded rather than improvised: promote the budget **fraction** into `config.ini` so it
is runtime data. That needs `SRC_CONFIG` in the format layer's dependencies, which it does not
link today; the build-time default exists precisely so this is a knob, not a rewrite.

### 6.1 Two guards, and the seam generated maps enter through

Three bounds now exist and are deliberately distinct:

| Bound | Question | Refusal |
| --- | --- | --- |
| Format ceiling (`SCENE_MAX_WIDTH/HEIGHT`) | what may a *file* declare? | `INPUT_DIMENSIONS` at parse |
| Configured ceiling (`SCENE_LOAD_MAX_CELLS`) | is this map too big for what this build is configured to load? | `INPUT_DIMENSIONS`, deterministic and identical on every host |
| Host budget (`SCENE_LOAD_MEMORY_BUDGET_PERCENT`) | can *this machine's* memory afford it? | `TSG-SCENE-ENV-0008` |

The configured ceiling is the one that answers the security question rather than the comfort
question: it is decided before any allocation and does not vary with the host, so a map cannot be
made to behave differently on a larger machine. It defaults to the format ceiling so nothing
narrows by accident, and `-DSCENE_LOAD_MAX_CELLS=n` lowers it per build or fork.

Hardening landed alongside it: the optical cell override array is sized from a pre-pass over the
file while the parse fills it with its own counter, and nothing compared the two. The candidate
now records its allocated capacity and the parse refuses past it. No exploitable mismatch was
demonstrated against today's pre-pass — the bound exists so that any future divergence there is a
typed rejection rather than a heap write.

**The seam (`scene_document_adopt_map`).** Generated content enters the document the same way a
file does, through the candidate → migration → transactional-commit pipeline, and passes the same
three bounds. `scene_document_create_new` is now implemented *in terms of* it (a new scene is an
adopted empty room), so the editor's New command and the generator cannot drift apart in their
defaults. The call consumes the caller's `Map`; the caller never keeps a handle after handing it
over. That is where `src/mapgen` will plug in: generators produce a `Map`, the document adopts it.

## 7. Determinism

- Use integer/hash noise, not floating-point gradients, or the same seed produces different
  maps on different machines.
- Derive per-stage seeds (`seed ^ stage_salt`) so stages are independently reproducible.
- Record provenance — seed, algorithm profile, parameters, generator version — so a level can
  be regenerated or audited. **Storage options:** (a) a **sidecar** `<name>.tscene.gen` (zero
  format risk, but two files to keep together); (b) a **known key block** inside the scene (a
  deliberate schema addition; the parsers have a strict unknown-key policy, so it cannot be
  smuggled in). Sidecar first; promote into the format later if desired.

## 8. Validation before play

Being bounded and pre-made makes validation cheap and worth doing at S3 rather than discovering
problems in playtest: flood-fill from the spawn, prove every exit/door is reachable, seal
border leaks, reject isolated pockets, and refuse or repair with a typed diagnostic. This needs
no navmesh — that is V1-14's problem — only a grid flood fill.

### 8.1 What the skeleton already guarantees (and what it does not)

`src/mapgen` (rooms-and-corridors, splitmix64, no floating point) and `tools/mapgen` landed as
the first slice. Guarantees the tests hold it to across seeds: border always solid, spawn cell
(1,1) open, **every open cell reachable from spawn** (test-side flood fill), identical bytes for
identical parameters, different bytes for different parameters, and every unusable parameter
refused rather than clamped. `tools/mapgen` is its own acceptance test — it writes a `.tscene`
and then reloads it through `scene_document_load_native`, so an unloadable generated scene fails
at generation time, not in the editor.

Deliberately not yet done: sealed-pocket rejection, verticality/portals, decoration, biome
vocabulary, themed material palettes, and persistence of the seed. Those are §5/§7/§10 work on
top of an interface that does not have to change to absorb them — `seed` is a parameter, not
stored state, precisely so the storage decision stays open.

Recorded observation from the first slice: the native save path requires a `.tscene` suffix, and
the document loader refuses `config_get() == NULL`. Both are pre-existing document contracts;
the tool states them up front (`--out PATH.tscene`) and calls `config_init_defaults()` rather
than papering over either.

### 8.2 Seed persistence: the sidecar, decided by building it

Decision 3 in §10 is answered: **a sidecar** (`PATH.tscene.mapgen`), holding only the parameters
needed to reproduce the map — `mapgen_version`, `seed`, `width`, `height`, `solid_material`,
`room_attempts`, `room_min_size`, `room_max_size`. Chosen over the alternatives for reasons that
turned out to be checkable rather than aesthetic:

- **The scene format is untouched.** Its parser has a strict unknown-key policy and a versioned
  schema; provenance in the scene would mean either a schema change or a tolerated-key exception
  in the one place the project is most strict. Neither is worth it for metadata.
- **The sidecar versions itself.** `mapgen_version` is refused when unsupported, so the sidecar
  can change shape independently of scenes — which is the actual requirement, since generation
  will gain biomes and verticality while the scene format stays put.
- **Losing it degrades gracefully.** A scene without a sidecar is still a perfectly good scene;
  it simply cannot be regenerated. That is the right failure mode for metadata.

The codec (`src/mapgen_seed`) is pure text in/out with no file access, so the tool owns the file
today and an editor "regenerate" command can reuse the parse tomorrow without either owning it.
Reading is strict on purpose: unknown key, duplicate key, missing key, unparseable value, two `=`
on a line, or an unsupported version are all refusals, and a refusal leaves the caller's
parameters untouched. A missing key is *not* defaulted — defaulting would silently produce a
different map than the one asked for, which is worse than an error. Hand-edited padding (aligned
columns, CRLF, blank lines) is accepted, because those are formatting, not meaning.

`tools/mapgen --reproduce PATH.tscene` is the acceptance test: it reads the sidecar, regenerates,
loads the scene, and compares every cell. Verified by hand: a correct record reports
`regenerates exactly`; changing the seed by one reports `does not match its generation record`;
an unknown key, a missing sidecar and a malformed record are each refused. So "the seed is
persisted" is demonstrated, not asserted.



## 9. Editor integration

- The map chooser gains **"New generated scene"** → a parameters screen (algorithm, size, seed,
  biome knobs, actor envelope) → the progress affordance of §3 → the finished scene opens.
- Afterwards it is an ordinary scene: hand-editable, saveable, undoable, importable.
- A **`tools/mapgen` CLI** shares the library for batch work, previews and CI.

## 10. Decisions to answer before implementation

1. **Caps** — **answered 2026-11-06:** raised to 2048×1024 with a RAM-derived load-time guard
   (§6). The ceiling has landed; the guard's placement is the only open piece.
2. **Levels** — one map per level linked by portals (recommended by §4), or wait for a
   multi-level model?
3. **Seed storage** — sidecar or in-format key block (§7)?
4. **Execution model** — sliced in the frame loop (recommended) plus CLI, threads later if ever?
5. **Where it lives** — a `mapgen` module + CLI, with the chooser entry in §9?

## 11. Phased plan

| Phase | Content | Gate |
|---|---|---|
| **M0** | Map-size policy (§6 options 1+3) and the load-time size guard; the "adopt a generated Map" seam | decisions 1 and 5 |
| **M1** | Pipeline skeleton: stages, progress, cancel, atomic write, validation pass, one trivial generator (a dune field) | M0 |
| **M2** | Portal contract: `portal_out`/`portal_in`, flow edges, within-map teleport trigger, editor authoring for a portal pair | decision 2; runtime transition from V1-12 |
| **M3** | Two real biome algorithms with their own invariants and tests (caves + one other) | M1 |
| **M4** | Further biomes (urban, jungle, dunes), actor envelope input, scatter/decorate passes | V1-13 for the envelope |
| **M5** | CLI, batch, preview gallery; sparse authored cells if gigantic maps are still wanted | M4 |

## 12. Non-goals

Runtime generation during play; infinite or streamed worlds; true 3D/multi-level geometry in
one map; portal *rendering* (see-through to a non-adjacent space); generating anything that
depends on systems that do not exist yet (actors, combat, inventory). Generated output is baked
content, not a runtime dependency.


