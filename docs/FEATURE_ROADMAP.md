# Feature Roadmap

## Purpose and authority

This document is the authoritative **dependency sequence** for future feature
work. It records which foundations must precede later capabilities, where
architecture decisions are required, and what evidence permits progress to the
next phase.

This roadmap:

- does not estimate time, effort, delivery dates, or staffing;
- does not replace a feature requirements or implementation plan;
- may be reordered when a review, prototype, or decision invalidates an
  assumption;
- promotes selected ideas from `docs/TODO.md`, which remains the complete,
  unordered inventory of desired and deferred work;
- preserves current accepted behavior from
  `docs/EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md` until a planned and tested
  change deliberately revises that contract.

Only one major roadmap phase should normally be active at a time. Independent
research may proceed in parallel when it does not mutate shared architecture or
force an unresolved product decision.

## Roadmap status vocabulary

- **Proposed** — sequenced here but not yet ready for implementation.
- **Decision-blocked** — one or more named product/architecture choices remain.
- **Ready to plan** — prerequisites and decisions are sufficient to write a
  scoped requirements/implementation plan.
- **Active** — an approved detailed plan is being executed.
- **Verified** — the phase exit gate passed and evidence is recorded.
- **Superseded** — replaced by a documented later decision or phase structure.

Code existing in the tree does not by itself make a phase Verified. The phase's
exit gate and applicable quality gates must pass.

---

# Engineering principles for every phase

## 1. Prefer validated data to hard-coded behavior

For every feature, ask in this order:

1. Can the user-visible content or policy be represented as data?
2. Does an existing format express it without overloading unrelated fields?
3. Does the data need a schema version, stable ID, migration, explicit default,
   or missing-reference policy?
4. Can runtime code consume it through a narrow validated adapter?
5. Is new code required because the behavior is genuinely algorithmic rather
   than declarative?

Prefer data for scenes, surfaces, materials, palettes, decals, lights, placed
objects, triggers, UI layouts, actions, bindings, presets, defaults, capability
flags, validation ranges, and render properties.

Keep code responsible for parsing and validation, deterministic transitions,
commands and undo/redo, rendering, collision and physics, ownership and resource
management, and behavior that cannot safely be passive data.

“Data-focused” does not mean an untyped scripting layer or a generic dictionary
of magic properties. Data must have typed fields, explicit semantics, validation,
versions when persistent, and deterministic fallback/error behavior.

## 2. One authoritative owner per domain

- Scene-authored state belongs to a scene document.
- Reusable material, decal, sprite, animation, and UI definitions belong to
  their asset documents.
- Runtime caches and compatibility views are derived and are never a second
  editable source of truth.
- UI invokes domain commands; it does not mutate authored structures directly.
- Renderer, collision, lighting, and simulation consume authoritative or derived
  read views through narrow interfaces.
- Save/discard boundaries are explicit when scene and reusable-asset documents
  are open together.

## 3. Preserve narrow modules and dependency direction

The intended dependency direction is:

```text
versioned formats and schemas
            ↓
domain documents, validation, and commands
            ↓
runtime adapters and derived data
            ↓
renderer, collision, lighting, and simulation

editor UI → domain commands (never direct mutation)
```

Do not turn `app.c`, `unified_editor.c`, or a generic property system into the
owner of every new domain. Each domain needs a narrow public interface, explicit
lifecycle, and testable failure behavior.

## 4. Keep domain logic deterministic and headless

File parsing, validation, migration, command execution, selection transforms,
document dirty state, stable-ID allocation, and asset editing must remain
testable without SDL rendering. UI, input, and file-picker adapters sit outside
those boundaries.

## 5. Make persistent change compatible and reversible

Every format change requires:

- an explicit version or unambiguous format discriminator;
- transactional load and save;
- old-format import/migration tests where compatibility is promised;
- no silent truncation or lossy fallback;
- documented defaults and missing-reference behavior;
- clear authored-versus-derived data ownership;
- preservation of the previous live document and destination on failure.

## 6. Build only proven seams

Future-proof through explicit data models, narrow interfaces, stable formats,
and tests. Do not create broad frameworks, generic engines, or speculative
abstractions before multiple concrete users establish the shared requirement.

## 7. Documentation and tests change with behavior

Each meaningful increment records what changed, why, failures encountered,
verification performed, and remaining uncertainty. A behavior change updates its
tests and stable contract together. Tests must not be removed or weakened merely
to obtain a pass.

---

# Reusable quality gates

## Q1 — Design readiness

Required before implementation of a phase or substantial feature:

- required and forbidden behavior is explicit;
- behavior-affecting product decisions are resolved;
- ownership, module boundaries, and dependency direction are named;
- data/schema/format effects and defaults are documented;
- migration, compatibility, and missing-reference rules are known;
- realistic failure paths and transaction boundaries are listed;
- deterministic tests, manual checks, and exit conditions are defined;
- rejected shortcuts and revisit conditions are recorded.

If these are not true, the feature remains Decision-blocked or Proposed.

## Q2 — Increment verification

Required after each small implementation increment:

- strict C11 build passes with `-Wall -Wextra -Wpedantic -Werror`;
- narrow focused tests pass;
- realistic validation, allocation, I/O, rollback, and no-change paths are tested;
- documentation is updated while the evidence is current;
- unrelated contracts and tests remain intact;
- generated/runtime adapters do not become hidden authored state.

## Q3 — Phase regression

Required before a phase becomes Verified:

- the full aggregate suite passes;
- applicable build-feature matrices pass;
- sanitizer/leak checks run for changed ownership or allocation boundaries;
- format round-trip, migration, malformed-input, and restoration tests pass;
- relevant interactive acceptance runs and is recorded;
- benchmark/stability checks run when hot rendering or simulation paths change;
- stable documentation matches verified behavior;
- unresolved limitations are explicit rather than presented as completed work.

## Q4 — Full repository review

Required at the named roadmap checkpoints and whenever architecture risk warrants
an unscheduled review.

Review at minimum:

1. module responsibilities, public APIs, dependency direction, and coupling;
2. ownership, cleanup, allocation, I/O, and transactional failure paths;
3. duplication, obsolete compatibility code, and hidden sources of truth;
4. domain logic leaked into UI, `app.c`, renderer, or global state;
5. hard-coded behavior that should now be typed, validated data;
6. schema versions, migrations, defaults, and missing-reference behavior;
7. strict builds, tests, sanitizers, benchmarks, and manual acceptance evidence;
8. stable contracts, README, plans, and implementation reality for consistency;
9. accessibility, input consumption, and editor workflow regressions;
10. whether roadmap assumptions or ordering must change.

The initial Review A predates this storage convention and remains at
`docs/REPOSITORY_CODE_REVIEW_2026-07-28.md` so established references remain valid.
Store subsequent reviews under `docs/reviews/`, using a dated searchable name such
as:

```text
docs/reviews/2026-08-15-roadmap-r0-baseline.md
```

Classify every finding as one of:

- **Blocker before next phase**
- **Fix during next phase**
- **Deferred cleanup**
- **Accepted constraint**
- **Needs product decision**

Do not create empty review directories or placeholder review files.

---

# Dependency overview

| Phase | Purpose | Depends on | Unlocks | Status |
|---|---|---|---|---|
| R0 | Health baseline and visible editor wins | Completed editor foundation | Better usability and evidence for architectural work | Verified |
| R1 | World, format, ownership, and identity decisions | R0 review evidence | A coherent scene implementation | Verified |
| R2 | Versioned scene/document data foundation | R1 decisions | Complete-scene loading and placed-content ownership | Verified |
| R3 | Generalized editor domain foundation | R2 | Multiple typed target and command domains | Verified |
| R4 | Surface data and basic world construction | R2–R3 | Floor/ceiling editing, stable surface anchors | Verified |
| R5 | Reusable asset-document foundation | R2–R3; material identity policy | Material/decal authoring | Verified |
| R6 | Decal placement and point-light authoring | R4–R5 | Authored placed visual/environment content | Proposed |
| R7 | Structural editing and scale | R3–R6 | Resize-safe bulk world construction | Proposed |
| R8 | Vertical-world implementation | R1 decision; R4; R7 semantics | Heights, slopes, vertical movement, true pitch if chosen | Decision-blocked |
| R9 | Layered optical rendering | R4 geometry separation; R8 geometry if applicable | Translucency, mirrors, explicit invisible surfaces | Research track |
| R10 | Colored and expanded lighting | R6; R8–R9 interaction rules | Colored, spot, and researched advanced lighting | Proposed |
| R11 | Sprites, animation, objects, and triggers | R2–R3; R8 geometry if applicable | Broader gameplay authoring | Proposed |
| R12 | Responsive UI model and UI/menu authoring | R0 UI evidence; stable editor domain patterns | Visual UI authoring | Proposed |

Dependencies express minimum foundations, not permission to skip Q1 planning or
Q4 review findings.

---

# Roadmap phases

## R0 — Repository health and visible editor wins

**Status:** Verified

The initial Q4 review and its remediation sequence are complete:
[`REPOSITORY_CODE_REVIEW_2026-07-28.md`](REPOSITORY_CODE_REVIEW_2026-07-28.md).
All applicable remediation gates are verified in
[`REPOSITORY_REMEDIATION_ACTION_PLAN_2026-07-28.md`](REPOSITORY_REMEDIATION_ACTION_PLAN_2026-07-28.md).
That closed work established the health prerequisite required by Q2–Q4; it is not
an active phase and does not expand the visible editor outcomes below.

**Purpose:** Establish a reviewed baseline and deliver small editor improvements
before persistent world formats and shared architecture change.

**Prerequisites:** The completed unified wall-material editor and its regression
contract. The initial review and remediation prerequisites are satisfied.

**Required outcomes:**

1. Perform the initial Q4 full repository review and close its remediation plan.
   **Verified 2026-07-28; all applicable remediation phases and gates passed.**
2. Add world-space selected-wall highlighting through an editor-only overlay.
   **Implemented 2026-07-29:** allocation-free selected/hovered wall-face
   outlines and an adaptive center crosshair now compose after world rendering
   and before editor UI. Focused automated verification is recorded in
   `test-editor-highlight`; strict build, aggregate, and focused sanitizer checks
   pass. Focused interactive acceptance, including inspector-dismissal selection
   clearing, passed on 2026-07-29; this R0 outcome is verified. A closeout review
   also removed the renderer's hidden 1,024-column cutoff so world rendering and
   the highlight overlay share the same full-grid-width contract.
3. Add safe open/switch flow for current map files with dirty confirmation and
   transactional failure behavior.
   **Verified 2026-07-29:** Editor entry and
   `Ctrl+O` use a bounded in-game catalog of regular lowercase `.txt` direct
   children under `assets/maps/`. Dirty switches use Save/Discard/Cancel, and
   catalog/save/load failures preserve the live document according to
   `EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md`. Strict full build, focused
   runners, aggregate `make test`, smoke, and focused ASan+UBSan checks pass. A
   sanitizer-discovered empty-catalog `qsort(NULL, 0, ...)` call was fixed.
   Interactive acceptance confirmed initial chooser/Escape, `Ctrl+O`, and dirty
   Save/Discard switching after a same-frame Enter-edge leak was found and fixed.
   Root causes and the incomplete first input correction are preserved in
   `R0_MAP_OPEN_SWITCH_RCA_2026-07-29.md`.
4. Add a bounded UI zoom/accessibility mechanism, not a premature responsive
   layout framework.
   **Verified 2026-07-30:** the bounded implementation uses immutable
   `default_user.ini` defaults, runtime-owned `user.ini` persistence, 100/125/150/
   200% UI-only presets with a shipped 150% default, global shortcuts, and a
   Settings menu. It introduces a bounded ordered UI-layer compositor with one
   global inherited scale and fixed/internal per-layer policies so menus, HUD,
   and editor text can scale without changing the world grid, projection, or
   fixed 8x8 source font. User-facing per-role/per-element controls remain
   deferred. Strict focused and aggregate tests, the full build matrix, ASan,
   UBSan, and a bounded dummy-video runtime path pass as recorded in
   `R0_UI_ZOOM_ACCESSIBILITY_IMPLEMENTATION_RECORD_2026-07-30.md`. Manual 200%
   acceptance also found and closed a legacy quit-confirmation layout defect:
   unsupported container geometry placed its children near the top-left and
   outside the centered menu crop. The dialog now uses supported centered
   geometry with exact layout and all-preset visibility regressions.
5. Replace the fixed pitch clamp with a documented grid-relative safe range while
   retaining and accurately naming the current 2.5D horizon-offset model.
   **Verified 2026-07-31:** the policy clamps the existing horizon offset to `±viewport_rows`,
   passed explicitly from the active logical grid. It does not add angular pitch
   or vertical-world behavior. Automated gates and user-confirmed gameplay/editor
   acceptance passed. See
   `R0_GRID_RELATIVE_HORIZON_OFFSET_PLAN_2026-07-30.md` and
   `R0_GRID_RELATIVE_HORIZON_OFFSET_IMPLEMENTATION_RECORD_2026-07-30.md`.
6. Add deferred tracker-selection and benchmark-classification regression tests.
   **Verified 2026-07-28:** dedicated generic/indexed tracker runners, bounded
   build-mode coverage, application-option conflict tests, and benchmark-session
   classification tests were added during repository remediation.

**Data-first opportunities:** UI-scale presets, file-filter/path policy, highlight
style parameters, and pitch policy should be validated configuration where that
improves customization without hiding behavior.

**Forbidden shortcuts:**

- Do not present current map opening as the final scene system.
- Do not mutate materials to highlight selection.
- Do not build full responsive UI layout before its semantics are decided.
- Do not call a wider screen-space horizon offset true unrestricted pitch.
- Do not grow editor behavior directly inside `app.c` without a narrow owner.

**Exit gate:** Q1–Q3 pass for all increments; interactive checks cover highlight,
file switching, UI scale, and extreme pitch; Review B below finds no blocker.

**Review checkpoint:** Review A before feature work and Review B after all R0
outcomes.

**Review B:** Verified 2026-07-31. No blocker remains before R1 planning. See
`reviews/2026-07-31-roadmap-r0-review-b.md`.

## R1 — Resolve world-model and persistence decisions

**Status:** Verified

**Purpose:** Lock the minimum decisions required by floor/ceiling data, lights,
decals, resize, verticality, glass, mirrors, and placed entities.

**Prerequisites:** R0 review findings and observed limits of the current editor.

**Required outcomes:**

1. Decide height-aware 2.5D versus stacked/full 3D requirements.
   **Verified 2026-07-31:** height-aware 2.5D with one traversable interval per
   X/Y; no stacked traversable spaces.
2. Decide versioned scene packaging and compatibility policy.
   **Verified 2026-07-31:** one versioned text scene, separate referenced reusable
   assets, and explicit non-destructive legacy import.
3. Define geometry, collision, visual surface, and optical-property separation.
   **Verified 2026-07-31:** these are independent typed authored semantics with
   legacy-preserving defaults.
4. Define stable ID allocation, persistence, references, and reuse rules.
   **Verified 2026-07-31:** one persisted scene-wide monotonic `uint64_t` namespace;
   zero invalid; IDs never reused; undo/redo preserves identity.
5. Define scene, reusable-asset, runtime-adapter, and derived-cache ownership.
   **Verified 2026-07-31:** `SceneDocument` owns authored scene state,
   `AssetRegistry` owns reusable definitions, and runtime views/caches are derived.
6. Specify initial schemas, migration behavior, and validation boundaries.
   **Verified 2026-07-31:** representative v1, future height-aware boundary,
   non-destructive import, strict structural validation, visible missing-asset repair,
   transactional load/save, and exact diagnostic requirements are documented.

**Data-first opportunities:** This phase defines what is authored data, what is a
reference, what is derived, and which rules belong in typed schemas.

**Forbidden shortcuts:**

- Do not implement a speculative generic engine.
- Do not select a world model implicitly through one feature patch.
- Do not encode new concepts into overloaded material IDs or magic values.
- Do not begin persistent scene implementation before compatibility and failure
  behavior are decided.

**Exit gate:** Q1 passes for R2; durable decision records exist for choices with
large compatibility/renderer consequences; representative schemas and migration
examples are reviewable.

**Exit evidence:** Verified in
`R1_REQUIREMENTS_AND_DECISION_PLAN_2026-07-31.md`,
`R1_SCENE_SCHEMA_SKETCH_2026-07-31.md`, and
`R1_DECISION_RECORD_2026-07-31.md`. Repository-wide diagnostics are governed by
`C_STYLE_AND_OWNERSHIP.md` and `ERROR_CATALOG.md`. R1 verification authorizes R2
planning only; no native scene behavior is implemented yet.

**Review checkpoint:** Findings are reviewed as part of the R2 plan; an
additional Q4 review is required if decisions materially redirect current module
boundaries.

## R2 — Versioned scene and data foundation

**Status:** Verified

**Purpose:** Implement the minimum versioned, transactional scene model selected
in R1 without duplicating authored state.

**Prerequisites:** Verified R1 decisions and an approved detailed R2 plan.

**Required outcomes:**

1. Versioned scene metadata and import of supported current maps.
2. Scene-owned map, spawn, ambient settings, and stable-ID infrastructure.
3. Scene-owned light and decal-instance collections, even if initial editing is
   deliberately limited.
4. Transactional parser, validator, serializer, migration, and missing-reference
   handling.
5. Narrow runtime adapters for existing map/world consumers.
6. Open, New, Save, Save As, reload, and dirty workflows for complete scenes.

**Data-first opportunities:** Scene composition, defaults, references, ambient,
spawn, and placed-instance properties are versioned data. Derived light maps and
renderer caches remain unsaved.

**Forbidden shortcuts:**

- No second editable map or synchronized authoring copy.
- No serializer awareness in editor widgets.
- No silent loss when importing current maps or encountering unknown data.
- No runtime cache promoted into persistent truth.

**Exit gate:** Representative scenes round-trip; supported legacy maps migrate;
malformed input and save failures preserve prior state/destination; Q1–Q3 pass.

**Review checkpoint:** Review C, focused on ownership, migration, failure
atomicity, runtime adapters, and duplicate state.

**Detailed plan:**
`R2_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-07-31.md` and
`R2_NATIVE_SCENE_V1_SPEC_2026-07-31.md`.

## R3 — Generalized editor domain foundation

**Status:** Verified

**Prerequisites:** R2 scene ownership and stable IDs.

**Required outcomes:**

1. Typed selection targets that preserve current wall behavior.
2. Stable references for selected scene instances.
3. Atomic grouped/batch command transactions.
4. Domain-specific inspector and tool adapters behind narrow interfaces.
5. Target-specific world highlight providers.
6. Inspector migration toward the shared data-driven UI system.
7. Typed, validated property metadata only where multiple concrete domains prove
   the shared need.

**Data-first opportunities:** Inspector field labels, ranges, choices, and tool
metadata may be declarative when tied to typed domain accessors and validators.

**Forbidden shortcuts:**

- No universal untyped property bag.
- No direct UI mutation.
- No single giant command union/controller switch without domain seams.
- No abstraction justified only by hypothetical future callers.

**Exit gate:** At least two concrete target/domain types exercise the shared
selection/command/inspector seams; current wall workflow remains green; Q1–Q3
pass.

**Review checkpoint:** `reviews/2026-08-07-roadmap-r3-review-d.md`, focused on
generalization quality, API size, dependency direction, and editor-controller
growth. All automated and interactive gates pass; R3 is Verified.

**Detailed plan:** `R3_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-07.md`.

## R4 — Surface data and basic world construction

**Status:** Verified

**Active plan:** Q1 approved on 2026-08-10. Increments A–F and the manual-review
follow-up are implemented; automated gates and the amended interactive checklist
passed, and R4 was marked Verified on 2026-08-12. See
`R4_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-10.md` and
`R4_INCREMENT_F_IMPLEMENTATION_RECORD_2026-08-11.md` and
`R4_MANUAL_REVIEW_FOLLOWUP_IMPLEMENTATION_RECORD_2026-08-12.md`.

**Purpose:** Replace “one wall material ID is the whole cell” with explicit
authored surfaces sufficient for fixed-height floor/ceiling editing and basic
construction.

**Prerequisites:** R2 scene format and R3 typed editor domains.

**Required outcomes:**

1. Separate occupancy/collision from visual surface references.
2. Add fixed-height per-cell floor and ceiling material data.
3. Retain one wall material per cell in R4; per-face materials are deferred by the
   approved requirements.
4. Render, select, highlight, edit, undo, and persist horizontal surfaces.
5. Add distinct in-bounds wall place/remove commands.
6. Add per-scene ambient editing.
7. Persist reviewed east/south copy-growth and safe refill-shrink in native v3;
   unavailable west/north removal remains visible but unselectable.
8. Remove attached wall decals atomically and keep selected surface materials visible
   under border-only highlights.

**Data-first opportunities:** Surface references, defaults, orientation, tiling,
outdoor/no-ceiling state, collision, and render flags belong in typed scene data.

**Forbidden shortcuts:**

- Do not overload one material ID with occupancy and optical semantics.
- Do not introduce slopes or variable heights without the R8 model decision.
- Do not weaken the existing occupied-wall material command to create geometry.
- Do not hard-code surface behavior in inspector/render branches.

**Exit gate:** Current maps migrate; fixed-height wall/floor/ceiling edits
round-trip and undo exactly; wall construction handles attachments and player/
spawn safety according to requirements; Q1–Q3 pass.

**Review checkpoint:** Targeted architecture review; escalate to Q4 if surface
adapters or format changes reveal broad coupling.

**Detailed plan:** `R4_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-10.md`.

**Deferred schema candidate:** A fixed-width hex block cell format (`XXX-XXX-XXX`)
was considered during Increment E planning and parked as a post-R4 migration
candidate. It needs a concrete consumer decision (per-face wall materials and/or
more than 255 materials) before promotion from `docs/TODO.md`. Increment E proceeds
on the existing v2/v3 three-digit grids.

## R5 — Reusable asset-document foundation

**Status:** Verified — I1–I4, Q1–Q3, and Review E passed on 2026-08-12.

**Purpose:** Establish a safe, tested lifecycle for authoring reusable assets,
then implement material and decal documents through it.

**Prerequisites:** R2 scene/reference policy, R3 editor domain seams, and a
resolved material identity/deletion policy.

**Required outcomes:**

1. A narrow lifecycle contract: Open/New, owned working copy, dirty identity,
   undo/redo, validation, preview, atomic Save/Save As, and Discard.
2. `MaterialDocument` for name, palette/color, and glyph authoring.
3. `DecalDocument` reusing `decal_painter` and `decal_io` boundaries.
4. Safe asset-registry refresh after successful commit.
5. Dependency and missing-reference reporting.
6. Explicit scene-history versus asset-history/save boundaries.

**Data-first opportunities:** Materials, palettes, decal grids, names, defaults,
and validation limits remain reusable asset data. UI merely presents document
commands.

**Forbidden shortcuts:**

- No direct editing of the live registry as the document model.
- No non-atomic overwrite.
- No duplicated painter/persistence implementation.
- No deletion that silently leaves or rewrites references.
- No broad generic document framework before material and decal lifecycles prove
  their actual commonality.

**Exit gate:** New/edit/save/discard/reload workflows pass deterministic and
interactive tests; failed saves preserve assets and working state; dependency
rules are enforced; Q1–Q3 pass.

**Review checkpoint:** Review E, focused on ownership, document reuse versus
over-abstraction, asset/scene transaction boundaries, and registry lifetime.

## R6 — Decal placement and point-light authoring

**Status:** Proposed

**Purpose:** Use stable scene instances, typed selections, surface data, and
reusable assets to author placed visual/environment content.

**Prerequisites:** R4 stable surfaces and R5 asset documents.

**Required outcomes:**

1. Paint decal canvases projected onto wall/floor/ceiling surfaces using one
   surface-local coordinate model.
2. Save or discard reusable decal asset changes.
3. Place/spray, select, move, rotate, scale, duplicate, and delete decal
   instances.
4. Place, select, move, and delete point lights.
5. Edit point-light intensity, radius, and retained anti-light behavior.
6. Group spray strokes and drag updates into atomic commands.

**Data-first opportunities:** Decal assets, placement transforms, surface
anchors, light properties, capacity policy, and tool presets are validated data.

**Forbidden shortcuts:**

- No camera-facing decal art or camera-dependent glyph arrangement.
- No embedding a separate decal copy per instance without an explicit reason.
- No coordinate-only identity for movable instances.
- No claim of colored illumination merely because `Light.color` exists.

**Exit gate:** Save/discard and placed-instance workflows survive reload,
undo/redo, missing assets, and deleted support surfaces as specified; Q1–Q3 pass.

**Review checkpoint:** Targeted review of surface coordinates, instance identity,
command grouping, and lighting invalidation.

## R7 — Structural editing and scale

**Status:** Proposed

**Purpose:** Add operations that relocate coordinates or mutate many targets
after all affected scene content has stable semantics.

**Prerequisites:** R3 command groups, R4 construction, and R6 placed-instance
ownership.

**Required outcomes:**

1. Explicit map expansion/contraction with defined origin semantics.
2. Exact undo restoration of cropped cells, surfaces, and attachments.
3. Bulk selection sets and primary-selection behavior.
4. Atomic batch material, place, remove, and property operations.
5. Stable selection/reference behavior across resize.
6. Validated map-size, command-memory, and performance limits.

**Data-first opportunities:** Map origin/dimensions, resize policy, selection/tool
presets, and limits are explicit configuration or scene metadata as appropriate.

**Forbidden shortcuts:**

- No unbounded allocation or unchecked dimension multiplication.
- No partial batch mutation.
- No silent deletion during contraction.
- No ad hoc coordinate repair for each entity type.

**Exit gate:** Resize and bulk operations are transactional, bounded, saveable,
and exactly undoable; large-map and allocation-failure tests pass; Q1–Q3 pass.

**Review checkpoint:** Review F plus sanitizer/leak and large-map stress checks.

## R8 — Vertical-world implementation

**Status:** Decision-blocked

**Purpose:** Implement the world model selected in R1 rather than patching Z
behavior incrementally into incompatible 2D assumptions.

**Prerequisites:** R1 world decision, R4 surface model, and R7 structural
semantics. A dedicated research/requirements/implementation plan is mandatory.

**Candidate outcomes, subject to R1 decisions:**

1. Camera/player Z and vertical collision/physics.
2. Authored floor/ceiling heights and vertical wall segments.
3. Ramps/slopes and explicit climb/step/slide rules.
4. Height-aware rendering, selection, and highlights.
5. Height-aware decals, lights, objects, and editor handles.
6. True angular pitch if required by the chosen world model.

**Data-first opportunities:** Heights, slope definitions, climbability, sector or
cell topology, and movement properties must be versioned scene data.

**Forbidden shortcuts:**

- No implicit choice between heightfield, sectors, voxels, or full geometry.
- No second incompatible coordinate model for editor versus renderer.
- No fake “unlimited pitch” presented as true vertical viewing.
- No stacked-space claim from a model that cannot represent stacked spaces.

**Exit gate:** Defined by the dedicated R8 plan after research; it must include
geometry/collision/render agreement, format migration, editor workflows,
performance bounds, and Q1–Q3.

**Review checkpoint:** Review G before implementation and again before R8 is
Verified.

## R9 — Layered optical rendering

**Status:** Research track

**Purpose:** Establish explicit layered visibility before adding translucency and
bounded reflections.

**Prerequisites:** R4 geometry/appearance separation and the final geometry model
from R8 when it affects ray intersections.

**Candidate outcomes:**

1. Explicit invisible/collision/ray/light semantics.
2. Bounded multiple-hit representation and compositing.
3. Translucent materials and light-transmission rules.
4. Ordering with decals, sprites, highlights, and entities.
5. Mirrors with bounded reflected rays/views and recursion policy.

**Data-first opportunities:** Opacity, transmission, reflectivity, collision,
layer limits, and quality presets are typed material/scene/render data.

**Forbidden shortcuts:**

- No single ambiguous invisible flag.
- No unbounded recursion or layer collection.
- No collision behavior inferred accidentally from visual alpha.
- No mirror implementation that ignores depth, entities, or documented fallback.

**Exit gate:** Research prototypes establish correctness and performance bounds;
a detailed implementation plan then satisfies Q1–Q3.

**Review checkpoint:** Review H before architectural commitment and after the
implemented optical phase.

## R10 — Colored and expanded lighting

**Status:** Proposed

**Purpose:** Extend lighting only after world height and optical transmission
semantics are stable enough to avoid repeated migrations.

**Prerequisites:** R6 point-light authoring plus applicable R8/R9 geometry,
occlusion, and transmission rules.

**Required/candidate outcomes:**

1. Decide RGB versus deliberately stylized palette-relative accumulation.
2. Implement colored-light mixing and transmission.
3. Add spot lights with direction, cone, falloff, and editor controls.
4. Research directional, area, and emissive lighting separately.

**Data-first opportunities:** Light type, transform, color, intensity, radius,
cone, falloff, shadow policy, and quality settings are typed scene/asset data.

**Forbidden shortcuts:**

- No color stored but ignored by surface illumination.
- No new light type hidden behind overloaded fields.
- No area/emissive implementation without an explicit performance model.

**Exit gate:** Colored/spot behavior is deterministic, persistable, editable,
and benchmarked; each researched type is accepted, deferred, or rejected with
evidence; Q1–Q3 pass.

**Review checkpoint:** Targeted lighting/data/performance review; Q4 if renderer
or scene ownership changes broadly.

## R11 — Sprites, animation, objects, triggers, and spawn authoring

**Status:** Proposed

**Purpose:** Add broader gameplay-authored entities using the existing scene,
identity, command, selection, and vertical-world foundations.

**Prerequisites:** R2–R3 and the applicable final geometry semantics from R8.

**Required outcomes:**

1. Implement and validate sprite rendering before sprite authoring.
2. Add sprite asset/instance placement and selection.
3. Add animation data, playback, timeline, and authoring.
4. Define and place objects through typed data/components.
5. Define triggers, conditions, actions, references, and validation.
6. Expand spawn authoring according to game-mode requirements.

**Data-first opportunities:** Sprite patterns, animations, object definitions,
component values, trigger graphs/actions, and spawn records are versioned data.
Code supplies validated behavior handlers rather than one-off hard-coded objects.

**Forbidden shortcuts:**

- No authoring UI before runtime semantics exist and are tested.
- No action strings accepted without validation.
- No coordinate-only identity or dangling trigger references.
- No unrestricted scripting system introduced without separate requirements.

**Exit gate:** Runtime and authoring behavior round-trip, validate references,
undo/redo, and handle missing assets/actions safely; Q1–Q3 pass.

**Review checkpoint:** Q4 after major entity/trigger boundaries stabilize.

## R12 — Responsive UI model and UI/menu authoring

**Status:** Proposed

**Purpose:** Define the long-term responsive UI data model before building a
visual editor for it.

**Prerequisites:** R0 scale evidence and mature document/inspector patterns from
R3/R5. This phase may move earlier only after a Q4 review confirms its
dependencies are stable.

**Required outcomes:**

1. Define anchors, constraints, flow, sizing, resolution, and focus semantics.
2. Version/migrate existing UI element/layout assets with compatibility tests.
3. Make game and editor UI responsive and independently scalable as specified.
   This includes reviewing the R0 ordered-layer seam and, where requirements
   justify it, adding versioned per-role or element-subtree scale overrides with
   deterministic overlap, inheritance, clipping, focus/hit-testing, reset, and
   missing-role behavior. Do not expose arbitrary asset names or unbounded layers
   as persistent user settings.
4. Add `UiDocument` with document lifecycle and validation.
5. Add visual hierarchy, property, canvas, drag/resize/reparent, and ordering
   workflows.
6. Add multi-resolution preview and action/reference validation.

**Data-first opportunities:** Layout structure, style tokens, focus order,
actions, bindings, constraints, templates, and responsive rules are validated
UI data.

**Forbidden shortcuts:**

- No visual editor for an absolute-coordinate format scheduled for replacement.
- No hard-coded resolution-specific branches as the responsive model.
- No return to disconnected editor application states.
- No editor that can corrupt the UI assets required to recover/edit it.

**Exit gate:** Existing UI migrates without regression; multiple supported
resolutions/scales pass layout, focus, pointer, keyboard, and accessibility
checks; authoring Save/Discard is transactional; Q1–Q3 pass.

**Review checkpoint:** Q4 before format commitment and after visual authoring is
integrated.

---

# Full repository review schedule

| Review | Trigger | Primary focus |
|---|---|---|
| A | Before R0 feature implementation | Baseline health, debt, boundaries, data/code audit |
| B | After R0 | Editor/UI coupling, accessibility, regression baseline |
| C | After R2 | Scene ownership, formats, migration, duplicate state |
| D | After R3 | Generalization quality, API size, module boundaries |
| E | After R5 | Asset-document reuse, transactions, over-abstraction |
| F | After R7 | Structural editing, ownership, memory, scale |
| G | Before and after R8 | Renderer/physics/world architecture and performance |
| H | Before and after R9 | Layering, recursion, optical correctness/performance |
| Continuing | Every later major phase | Full Q4 review and roadmap reassessment |

A review may add a blocker, split a phase, combine phases, or reorder future
work. Such changes are expected maintenance of the roadmap, not a failure to
follow it.

---

# Roadmap phase template

New or substantially revised phases use this structure:

```markdown
## RX — Phase name

**Status:** Proposed | Decision-blocked | Ready to plan | Active | Verified | Superseded

**Purpose:** Why the phase exists.

**Prerequisites:** Required earlier phases, evidence, and decisions.

**Required outcomes:** User-visible and domain-visible capabilities.

**Data-first opportunities:** What should be schemas, assets, or configuration.

**Forbidden shortcuts:** Regressions that would compromise later work.

**Exit gate:** Exact evidence required before progression.

**Review checkpoint:** Required review scope.

**Detailed plan:** Link added only when the phase becomes active.
```

The dependency table must also state what the phase unlocks.

---

# Roadmap maintenance process

## Deferred build-performance checkpoint

After the R0 UI zoom performance remediation, evaluate explicit strict `-O2` and
`-O3` profiles as a separate maintenance checkpoint. Entry requires resolving
optimization-only diagnostics without weakening `-Werror`; exit requires matching
framebuffer checksums, full tests/sanitizers/tracker matrix, and repeated native
renderer-only plus layered-UI measurements. The default build remains unchanged
until that evidence exists.

1. Preserve new unsequenced ideas in `docs/TODO.md` first.
2. Promote an idea here only after its dependencies and role are understood.
3. Resolve named decisions and pass Q1 before writing implementation code.
4. Create one scoped detailed plan when a phase becomes Active.
5. Update working notes continuously during implementation.
6. Pass Q2 after increments and Q3 at phase completion.
7. Perform Q4 at named checkpoints and record findings under `docs/reviews/`.
8. Update the stable regression contract when accepted behavior changes.
9. Update `README.md` last, after implementation and verification stabilize.
10. Preserve superseded phases and decisions with clear links; do not erase the
    evidence or reasoning that changed the roadmap.

## Next action

**R0–R4 are Verified.** The next phase is R5 (Reusable asset-document foundation),
currently Proposed. Per roadmap policy, promote it to Active only after scoped Q1
planning; the current R4 evidence is in
`R4_MANUAL_REVIEW_FOLLOWUP_IMPLEMENTATION_RECORD_2026-08-12.md`.
