# Future Features and Deferred Work

## Purpose and authority

This document preserves desired features, follow-up ideas, unresolved design
questions, and deferred engineering work. It is an **idea inventory**, not an
authoritative roadmap, implementation plan, commitment, or priority order.

Dependency order, engineering gates, review checkpoints, and the current next
phase are maintained separately in `docs/FEATURE_ROADMAP.md`.

An unchecked item means “wanted or worth investigating,” not “ready to build.”
Before implementation, each substantial item needs its own requirements
discussion and scoped plan. Accepted current behavior remains defined by
`docs/EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md` until deliberately revised.

Planning labels used below:

- **Ready for focused planning** — desired behavior is reasonably clear.
- **Needs product decision** — multiple valid user-visible models exist.
- **Needs architecture/design** — the current model cannot represent it safely.
- **Research track** — feasibility, cost, or interaction rules need evidence.
- **Ongoing maintenance** — recurring work, not a one-time feature.

These labels describe readiness, not priority.

## Current constraints affecting future work

1. The world is a fixed-size 2D grid. One material ID per cell means `0` is
   empty/passable and values above `0` are full-height solid walls.
2. Maps save one decimal digit per cell, so only material IDs `0..9` persist.
3. Floors and ceilings are global fixed planes, not authored cell surfaces.
4. Camera pitch shifts the screen horizon; it is not a true 3D pitch angle. The
   camera and collision model have no Z coordinate.
5. Lights, decals, sprites, and spawn live in runtime `WorldState`; the current
   `SceneDocument` owns and saves only the map.
6. A material references a shared palette and four distance-band glyphs.
7. `decal_painter` and `decal_io` provide tested headless primitives, but no
   integrated document, save/discard workflow, or UI.
8. The window resizes by scaling a fixed logical grid. UI assets mostly use
   absolute grid coordinates and do not responsively reflow.
9. Rendering assumes the nearest opaque wall hit per column. Layered
   transparency, reflected views, and stacked geometry are not primitives.

## Build and performance maintenance

- [ ] **Research track — explicit `-O3` build profile:** the strict default is
  now `-O2` (set during the 2026-08-21 heightfield performance work; the earlier
  "strict unoptimized default" guidance is superseded). All optimization-only
  diagnostics exposed by `-O2 -Werror` were resolved without suppression.
  Remaining research: evaluate a separate strict `-O3` Make profile, verifying
  framebuffer checksums, aggregate tests, sanitizers, tracker modes, and renderer
  benchmarks independently. Do not use optimization to conceal algorithmic
  regressions.

## Cross-cutting decisions for later discussion

### Versioned scene persistence — **Needs architecture/design**

A future scene may own map dimensions/origin, wall/floor/ceiling surfaces,
ambient settings, spawn, lights, decal instances, and later objects/triggers.
Reusable material and decal assets should likely remain referenced assets.

Questions:

- One versioned file, a manifest plus files, or a directory/package?
- How are current digit maps imported and preserved?
- Which multi-file saves must be atomic?
- Which runtime data remains derived rather than serialized?
- What compatibility guarantees do older scenes receive?

### Extensible per-cell block serialization — **Needs product decision**

**Wanted:** Move per-cell material tokens from fixed 3 decimal digits to fixed-width
hex blocks, e.g. `XXX-XXX-XXX`, where each 3-digit block is one typed field; new
per-cell fields are added three hex digits at a time.

Shape of the idea:

- One `-XXX` block = one `0x000..0xFFF` value (max 4096 per field).
- New fields (per-face wall materials, flags, surface policy) append a block
  instead of forcing section/version churn.
- Stays in the same human-editable grid style as today's `001 001 001`.

Current constraints (verified 2026-08-10):

- The 255 material ceiling comes from three places, NOT from digit width:
  `SceneAuthoredCell` stores `uint8_t` IDs, `AssetRegistry` slots are
  `materials[256]`/`palettes[256]`, and the parser caps blocks with
  `parse_uint_range(..., 255U, ...)`. Hex blocks widen the token to 4096 but
  capacity only moves if the cell type, registry, and loader widen together.
- 3 hex digits cap at 4096 per field; exceeding that breaks fixed width. This is
  extensibility-by-new-block, not unbounded capacity.
- Adopting it is a v2→v3 migration reusing the v1→v2 pending/repair machinery.

Open decisions:

- Concrete consumer first: per-face wall materials (already deferred by R4),
  capacity above 255, or both?
- If capacity: choose typed width and registry size first; token base is secondary.
- Add blocks only when a real field exists, or reserve slots now?

Incremental E follows this idea only in the sense that missing authored surfaces
need an explicit render fallback; the block schema itself is deliberately parked.

### Height-aware 2.5D world questions — **Resolved 2026-08-19**

These questions were answered in `R8_DECISION_RECORD_2026-08-19.md`:

- Stacked rooms are **not** required; one traversable interval per X/Y remains
  authoritative (R1 contract).
- True unrestricted vertical rotation is **not** an R8 requirement: the
  horizon-offset pitch is kept and true angular pitch is deferred as a future
  option (record Decision 3).
- Slopes are **cell-aligned ramps only**, never arbitrary inclined planes
  (record Decision 2).
- The long-term renderer **remains a ray caster** with a continuous per-cell
  heightfield representation; sectors/portals, voxels, and full-3D geometry
  remain rejected (record Decision 1).

### Geometry, collision, and appearance separation — **Needs design; scoped in the R9 research plan**

Mirrors, glass, invisible collision, and pass-through surfaces require separate
properties for occupancy, player collision, ray/light interaction, visual
material, opacity, and reflectivity. These must not remain encoded by the
special meaning of material ID `0`. Scoped as RQ1/P1 in
`R9_OPTICAL_RESEARCH_PLAN_2026-08-21.md`.

### Stable IDs and atomic command groups — **Needs design**

Movable lights, decals, objects, bulk selections, and map resizing need stable
instance IDs. Multi-target operations should apply completely or not at all and
appear as one undoable user action. ID persistence/reuse, references after move
or deletion, and selection survival need explicit rules.

---

# Editor usability and workflow

## Selected-item highlighting — **Wall-face baseline verified; generalization deferred**

**Implemented baseline (2026-07-29):** The current wall editor draws a dashed
hovered wall-face outline, a solid persistent selected-wall-face outline, and an
adaptive center crosshair. Escape dismissal clears the persistent selection. The
overlay is allocation-free, respects nearest-wall occlusion, and does not mutate
authored materials. See `EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md` and
`EDITOR_HIGHLIGHT_IMPLEMENTATION_RECORD_2026-07-29.md`.

**Preserved future idea:** Generalize the same interaction vocabulary to floors,
ceilings, lights, decals, objects, bulk selections, and hidden targets after those
target types and occlusion semantics exist.

Likely implications:

- editor-only overlay; never mutate the authored material;
- reuse the verified wall-face vocabulary, then generalize by target type;
- distinguish hover, primary selection, and future bulk selection;
- remain clear at distance/oblique angles and not rely only on color;
- define hidden/occluded indicators and reduced-motion behavior.

### Hover-outline visibility — **Research track**

The R4 manual review found the unified `.` hover outline visually consistent across
walls, floors, and ceilings but too easy to miss against some authored materials and
viewing angles. Evaluate visibility improvements without returning to a filled overlay
that hides the material being inspected. Compare at minimum: a brighter or alternating
outline glyph, contrast-aware glyph selection, a two-tone/double edge where space
permits, and restrained animation or pulsing with a reduced-motion/static equivalent.
Test dark/bright palettes, missing-material purple, distance, oblique floor/ceiling
views, UI scale presets, and overlap with persistent `#` selection. Do not rely on
color alone or mutate authored cells.

## Open and switch saved scenes/files — **Current-map baseline implemented; scene-dependent later**

**Verified baseline (2026-07-29):** Main-menu
Editor and in-editor `Ctrl+O` open a sorted in-game list of regular lowercase
`.txt` direct children under `assets/maps/`. Dirty switches use Save/Discard/Cancel,
and discovery/save/load failures preserve the live document as defined by
`EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md`.

**Intentionally not included in the current-map baseline:** native dialogs,
editable/arbitrary paths, recursion, symlink following, recent files, New, Save As,
or a generic scene/list framework.

Preserved future scene/file implications:

- versioned scene picker, validation, empty states, load errors, recent files, and
  Save As after the scene format and ownership model exist;
- migration/import policy for current digit-grid maps;
- scene-root path/name policy and safe asset-root handling.

Avoid making the UI permanently map-file-specific if a future level contains
map, lights, decals, spawn, and ambient data.

## UI scale and resizing — **Bounded accessibility zoom verified; responsive model deferred**

**Verified baseline (2026-07-30):** Menu, HUD, and editor text support independent
100%, 125%, 150%, and 200% UI scaling with a 150% default, immutable/runtime
preference precedence, atomic live persistence, global shortcuts, and Main/Pause
Settings access. The fixed crosshair and editor highlights remain unscaled. See
`R0_UI_ZOOM_ACCESSIBILITY_IMPLEMENTATION_RECORD_2026-07-30.md`.

**Wanted:** Comfortably resize editor text and controls.

Possible scopes:

1. UI zoom presets (100%, 125%, 150%, 200%) independent of world scale.
2. Runtime logical grid/cell resizing, recreating grid, framebuffer, texture,
   tracker state, and pointer-coordinate conversion.
3. Responsive reflow with anchors, percentages, min/max sizes, and flow/layout
   containers.

**Decision 2026-07-30:** R0 implements scope 1 only through the approved
`R0_UI_ZOOM_ACCESSIBILITY_PLAN_2026-07-30.md`. The accepted model uses immutable
`default_user.ini`, runtime-written `user.ini`, immediate persistent live
adjustment, 100/125/150/200% presets with a 150% shipped default, global keyboard
shortcuts, and a bounded Settings menu. UI is composited separately so world
scale and the fixed logical grid remain unchanged. Scopes 2 and 3 remain
deferred and require their own later decisions.

The current inspector uses direct `grid_print` while menus use `UiLayout`; the
verified R0 implementation converges their rendering through one bounded ordered
layer/compositor boundary. R0 exposes one global inherited scale while retaining
fixed-100% and internal explicit-preset layer policies so concrete UI subtrees
can be separated later without a renderer redesign. It deliberately does not
resolve the later responsive-layout model, runtime logical-grid resizing, or
broader supported-resolution policy.

### Independent UI-role and element sizing — **Deferred; foundation in R0**

**Wanted later:** Independently size concrete UI roles or element subtrees—for
example a 200% debug HUD, 150% editor inspector, 200% warning, and 100% crosshair—
with deterministic overlap and persistence.

R0 provides only the rendering seam: compact ordered layers, stable painter's
order, per-layer scale policy, anchors, clips, visibility, and fixed capacity.
The user-visible preference remains one global scale in version-1 user settings.

Before exposing independent controls, decide:

- which stable semantic roles are user-adjustable rather than arbitrary asset
  names;
- whether child elements inherit, override, or form separate layers;
- preference schema/version migration and missing-role behavior;
- settings-menu presentation and reset semantics;
- overlap, focus/hit-testing, clipping, accessibility, and performance limits;
- whether the capability belongs with the later responsive UI model rather than
  as isolated scale overrides.

Do not persist transient element pointers, file-order indices, or unrestricted
user-created layers. Promote only proven concrete roles/subtrees.

## Bulk selection and batch editing — **Needs product and command design**

**Wanted:** Select and edit multiple cells, surfaces, or instances.

Possible tools include rectangle, additive/subtractive selection, connected
region, and select-by-material/type. This implies a selection set, primary
selection, atomic batch commands, mixed-value inspector states, bounded memory,
and clear handling of invalid targets. Decide initial target types, mixed-type
selection, and persistence through structural edits.

---

# Surface and map authoring

## Edit floor and ceiling materials — **Needs architecture/design**

**Wanted:** Select floors and ceilings and assign materials independently.

A conservative first model is per-cell wall/occupancy, floor material, and
ceiling material at fixed global heights. It requires:

- a new scene/map format and migration;
- authored floor/ceiling rendering instead of hard-coded colors;
- horizontal-surface ray hits and surface coordinates;
- new selection variants, commands, highlights, and tests;
- rules for empty cells, outdoors/no-ceiling, borders, tiling, orientation, and
  glyph scale;
- shared surface-coordinate conventions with decals.

Decide fixed heights first versus variable heights, and whether every cell must
have both planes. Do not smuggle slopes into this feature without a decision.

## Place and remove walls — **Ready in bounds; resizing needs design**

**Wanted:** Turn empty cells into walls and remove walls.

Use distinct place/remove commands rather than weakening the current occupied
wall-material command. Define initial material, player/spawn safety, attached
decal/object handling, lighting invalidation, and exact undo restoration.
Removing a wall also needs a rule for whether floor/ceiling surfaces remain.

## Expand and contract map borders — **Needs architecture/design**

**Wanted:** Extend borders while building and contract unused space.

This implies transactional reallocation of authored/derived arrays, explicit
world origin when expanding west/north, remapping coordinate-based content,
undo data for cropped cells/attachments, overflow/dimension limits, and format
support for dimensions/origin.

Decide automatic versus explicit resize, growth granularity, maximum size, and
whether contraction refuses, prompts for, or undoably captures occupied crops.

## Per-face wall materials — **Needs model/format design**

**Wanted:** Different north/south/east/west materials on one occupied wall.

This requires separating occupancy from four surface references, face-aware ray
lookup/serialization, placement defaults, hidden/internal face rules, and
migration of the current material to appropriate faces. Coordinate this with
floor/ceiling data rather than creating another temporary encoding.

---

# Material and reusable asset authoring

## Create and edit materials — **Needs focused requirements/document design**

**Wanted:** Create materials; edit name, base/near/mid/far colors and distance
glyphs; preview; Save, Save As, or Discard.

Likely implications:

- `MaterialDocument` with owned working copy, dirty state, undo/redo,
  validation, atomic save, and discard;
- previews at multiple distances/light levels and glyph-picker UI;
- safe `AssetRegistry` refresh only after commit;
- new-ID, naming, rename, duplicate, delete, and missing-reference policies;
- dependency display for shared palettes/materials;
- resolution of the map `0..9` versus registry `1..255` mismatch.

Decide whether palettes stay shared, what “base color” means relative to three
color stops, whether shared-palette changes propagate live, deletion policy,
and whether asset history is independent from scene history.

## Paint/save/discard reusable decal assets — **Needs focused requirements**

**Wanted:** Paint a decal, save specific changes or discard them, and reuse it.

Build a `DecalDocument` around `decal_painter`/`decal_io`: owned working grid,
dirty/saved identity, undo/redo, validation, atomic Save/Save As/Discard,
resize/crop, brushes, fill, erase, and preview. Keep reusable decal **assets**
separate from placed **instances**. Decide overwrite/version/duplicate behavior,
path/ID policy, and whether unsaved assets can be placed temporarily.

## Paint decals directly on any surface — **Needs interaction/coordinate design**

**Wanted:** Paint while viewing a wall, floor, ceiling, and eventually slopes.

Pointer hits should map into one surface-local origin/tangent/bitangent model and
edit a fixed 2D `PatternCell` grid projected onto the surface. Art must remain
surface-anchored, not camera-facing. Define brush footprint, clipping,
resolution/physical glyph size, seams/corners, cross-surface strokes, and whether
painting starts a named unsaved asset requiring Save/Discard.

## Spray/place saved decals — **Needs scene ownership, IDs, and commands**

**Wanted:** Repeatedly place saved decal instances on valid surfaces.

Needs scene-owned instances, preview, place/move/rotate/scale/duplicate/delete,
stable anchors, grouped spray-stroke undo, overlap/layering rules, capacity and
performance policy, missing-asset handling, and behavior when support geometry
is removed. Decide spacing/randomization and edge wrapping.

## Sprite and animation authoring — **Needs product and renderer requirements**

`SpriteAsset` exists, but generic sprite rendering does not. Define billboard
versus oriented/world plane behavior, animation timeline/file format, playback,
placement/IDs, painting-tool reuse, and occlusion with walls, decals,
translucency, and mirrors before designing authoring UI.

---

# Lighting and environment

## Place, select, move, and delete point lights — **Needs scene ownership**

**Wanted:** Author point lights interactively.

Move lights from transient runtime ownership into saveable scene data; add
stable IDs, highlights, place/move/delete commands, property inspection,
explicit capacity errors, and narrow lighting recomputation. Define valid
positions and drag/axis controls.

## Edit brightness, radius, and color — **Partly ready after ownership**

Intensity/radius editing fits the current `Light`; preserve negative anti-light
unless deliberately changed. Full colored illumination needs design because
`Map.light_map` stores scalar brightness despite `Light.color` existing.

Decide RGB/stylized palette-relative mixing, clamping/overbright behavior,
negative colored light, shadows during live dragging, and whether color affects
surfaces, light billboards, or both.

## Other light types — **Research track**

Potential types:

- spot: direction, cone, falloff, orientation controls;
- directional: global direction and shadow policy;
- area: multisampling/approximation with explicit cost/quality target;
- emissive material: material-light coupling and update/bake behavior.

Each needs a visual model, performance budget, serialization, selection,
inspector fields, and deterministic tests. No implementation order is committed.

## Per-scene ambient light — **Ready for product planning; format-dependent**

**Wanted:** Adjust ambient light live and save it with the scene.

Separate global default from per-scene override; add an undoable validated
property; recompute lighting narrowly; define interaction with anti-lights,
colored lighting, and outdoor/no-ceiling areas.

---

# Camera and vertical world

## Increase/remove look-up/down bound — **Bounded 2.5D policy approved**

Current `Camera.pitch` is a horizon offset clamped to `[-100,100]`, not an angle.

Two different features are possible:

1. Wider 2.5D look: grid-height-relative limit, controlled off-screen horizon,
   and projection safety audit.
2. True unrestricted pitch: camera Z/angular pitch and a replacement of current
   projection assumptions, coupled to vertical-world design.

**Decision 2026-07-30:** R0 uses option 1 with an exact
`[-viewport_rows, +viewport_rows]` bound derived from the active logical grid.
This remains a 2.5D horizon offset. True angular pitch was referred to R8 and,
per `R8_DECISION_RECORD_2026-08-19.md`, remains deferred beyond R8 (not an R8
requirement).
See `R0_GRID_RELATIVE_HORIZON_OFFSET_PLAN_2026-07-30.md`.

Define expected behavior near straight up/down, extreme-pitch safety, decal
coherence, sensitivity/inversion, and avoid calling a wider horizon offset
“unlimited pitch.”

## Verticality and inclined-surface climbing — **Decisions locked 2026-08-19**

**Wanted:** Raised/lowered areas and traversable ramps/slopes.

The verticality decisions are resolved in `R8_DECISION_RECORD_2026-08-19.md`:

- continuous per-cell floor/ceiling heights (heightfield); ramps are cell-aligned
  only;
- movement scope: fall/gravity, step-up, jump, and head-clearance, with per-map
  runtime-tunable parameters as versioned scene data (ladders are deferred from
  R8);
- renderer stays a ray caster with per-column height math (no rewrite), the view
  composed through the horizon-offset pitch plus eye height;
- v4 → v5 heightfield migration reuses the proven v1 → v2 → v3 → v4 machinery.

R8 is **Verified (2026-08-21)**. The requirements/implementation plan
(`R8_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-19.md`) now specifies the schema
fields, limits, parameter validation ranges, editor workflows, and renderer
performance budget; the heightfield itself remains separate from the per-surface
floor/ceiling materials already authored in v3.

---

# Optical and advanced rendering

## Mirrors — **Research track; scoped in the R9 research plan**

**Wanted:** Reflective surfaces showing the world behind the viewer.

Needs reflected secondary rays/views, recursion limits, depth/occlusion,
reflected entities/lights/decals, reflectivity separate from occupancy, and a
performance strategy (bounded resolution/update rate or stylization). Decide
surface types, bounce count, mirror-facing-mirror behavior, visual quality, and
interaction with translucency/invisible geometry. Scoped as RQ4/P4 in
`R9_OPTICAL_RESEARCH_PLAN_2026-08-21.md` (single bounded bounce first;
mirror-facing-mirror stays out of the prototype).

## Translucent materials — **Needs design and renderer research**

**Wanted:** Visible surfaces such as glass through which the world remains seen.

Rays must continue after a translucent hit and collect ordered layers, followed
by back-to-front compositing or ASCII dithering/stipple. Separate opacity,
collision, light transmission, and selection behavior; bound layers per ray;
define ordering with decals/sprites/mirrors/highlights. Decide alpha versus
dither, colored transmission/refraction, and partially transparent glyph cells.

## Invisible materials/collision — **Needs explicit semantics**

Do not use one ambiguous “invisible” flag. Distinguish invisible solid
collision, invisible passable markers, editor-hidden geometry, ray-transparent
barriers, and light-blocking/passing geometry. The editor needs a reveal toggle
and highlight so hidden geometry remains editable. Scoped as RQ1/P1 in
`R9_OPTICAL_RESEARCH_PLAN_2026-08-21.md`, which requires ray, light, and player
interaction to be independent typed fields rather than one flag.

---

# Placed entities and gameplay authoring

## Objects, triggers, and spawn editing — **Needs models and stable IDs**

Define object/component and asset-reference models, transforms under the chosen
world geometry, trigger shapes/conditions/actions, links and validation,
single/multiple spawn semantics, selection/property workflows, serialization,
missing references, and atomic undo/redo. Stable IDs and scene persistence are
prerequisites.

---

# Interface, UI, and menu authoring

## Unified-editor UI/menu workspace — **Separate feature-planning push**

**Wanted:** Visually author interfaces, layouts, and menus instead of manually
editing text assets. Keep this as a workspace/submode inside the unified Editor,
not a return to disconnected application states.

Likely capabilities:

- `UiDocument` with dirty state, undo/redo, atomic Save/Save As/Discard;
- canvas hit testing, highlight, drag, resize, reparent, order, duplicate/delete;
- hierarchy/tree and property inspector;
- text, color, dimensions, borders, focus order, actions, substitutions, and
  parent relationships;
- action/reference validation;
- preview across logical resolutions and UI scales;
- keyboard and pointer authoring with visible focus.

First decide absolute coordinates versus anchors/constraints/flow, responsive
behavior, supported resolutions, interactive preview, templates/styles, safe
self-editing, and migration of current assets. Building a visual editor for the
current absolute format immediately before replacing it would create avoidable
migration work.

---

# Benchmark and performance methodology

These are deferred engineering-quality follow-ups, not product priorities.

## Statistics and warm-up — **Needs focused planning**

- [ ] Add p95/p99 telemetry instead of relying on a single-outlier heuristic.
- [ ] Define/exclude a reproducible warm-up from average and worst collection.
- [ ] Emit measured/effective FPS in benchmark JSON.
- [ ] Record sufficient run context for reproducible comparison.

This needs bounded sample storage or online percentiles, explicit frame
inclusion rules, output tests, and documented spike interpretation.

## Separate acceptance and stress workloads — **Needs focused planning**

- [ ] Keep raycast acceptance and full-change stress as distinct targets/results.
- [ ] Decide whether stress has informational thresholds or a separate hard gate.
- [ ] Preserve comparable baseline, stream, dirty-cell, and other mode results.

## Build-mode/result regression tests — **Deferred**

- [ ] Test `ideal`, `pass_minimum`, allocation failure, and performance failure.
- [ ] Test default stream, explicit no-tracker, explicit alternatives, and
      conflicting-mode rejection.
- [ ] Preserve `USE_NO_STATE_TRACKER=1` as the intentional baseline.

---

# Documentation maintenance

- [x] Establish the stable editor requirements/regression contract.
- [x] Preserve historical evidence and mark superseded records clearly.
- [x] Expand this file into the requested non-authoritative idea inventory.
- [ ] Give active features scoped requirements/plans; do not treat this file as
      an executable specification.
- [ ] Update the stable contract, tests, and `README.md` together when accepted
      behavior changes.
- [ ] Preserve completed ideas or archive their decision/history; do not silently
      erase why they existed.

## Adding an idea

Record: desired outcome; relevant current constraints; likely model, format,
renderer, editor, and test implications; shared dependencies; unresolved product
or architecture decisions; and planning readiness. Do not assign implementation
order here. Promote understood dependencies into `docs/FEATURE_ROADMAP.md` only
after discussion; keep unsequenced and rejected/deferred ideas here for context.
