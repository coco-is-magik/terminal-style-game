# Roadmap R0 Review B — 2026-07-31

## Decision

**R0 is Verified. No blocker remains before R1 planning.**

R1 is **Ready to plan**, not Active: Review B satisfies its R0 evidence
prerequisite, while the world-model, persistence, identity, ownership, schema,
migration, and compatibility decisions named by R1 still require a scoped decision
plan before implementation.

## Scope

Review B evaluated the R0 exit gate and the roadmap's required editor/UI coupling,
accessibility, input, ownership, regression, and documentation baseline. It covered:

- unified-editor authoritative state and application coupling;
- map chooser/open/switch transactionality;
- input consumption across menu, editor, and application boundaries;
- UI zoom preferences, ordered composition, focus, clipping, and persistence;
- selected/hovered wall highlighting and crosshair ordering;
- grid-relative 2.5D horizon-offset behavior and terminology;
- obsolete compatibility paths and stable-document consistency;
- strict build, aggregate, sanitizer, smoke/runtime, and feature-matrix evidence.

This was not an R1 design review, full security audit, renderer redesign, responsive
UI plan, or vertical-world decision.

## Evidence

### Accepted R0 outcomes

1. Review A and repository remediation are closed in
   `REPOSITORY_REMEDIATION_ACTION_PLAN_2026-07-28.md`.
2. Editor world highlighting is verified in
   `EDITOR_HIGHLIGHT_IMPLEMENTATION_RECORD_2026-07-29.md`.
3. Current-map open/switch behavior is verified in
   `R0_MAP_OPEN_SWITCH_PLAN_2026-07-29.md` and the stable editor contract.
4. Bounded UI zoom is verified in
   `R0_UI_ZOOM_ACCESSIBILITY_IMPLEMENTATION_RECORD_2026-07-30.md`; its later
   performance remediation and 2026-07-31 visual acceptance are recorded in
   `R0_UI_ZOOM_PERFORMANCE_REMEDIATION_IMPLEMENTATION_RECORD_2026-07-30.md`.
5. Grid-relative horizon offset is verified, including 2026-07-31 gameplay/editor
   acceptance, in
   `R0_GRID_RELATIVE_HORIZON_OFFSET_IMPLEMENTATION_RECORD_2026-07-30.md`.
6. Tracker-selection and benchmark-classification regressions were closed during
   repository remediation.

### Fresh verification

All builds used strict C11 warnings: `-Wall -Wextra -Wpedantic -Werror`.

- focused `test-ui-ele`: **15/15 pass**;
- strict application build: **pass**;
- aggregate `make test`: **26 passing groups, zero failure markers**;
- `make sanitize`: **pass**, with no ASan or UBSan failure marker;
- `make matrix`: **8/8 modes pass** — no tracker, custom dirty cells, generic SMC,
  indexed SMC, batch SMC, stream SMC, lighting cache, and glyph cache.

Existing R0 records additionally contain successful smoke, projection, bounded
runtime, and focused interactive evidence. The layered-UI workload's retained
generic `fail_performance` classification is an accepted workload/threshold mismatch,
not a correctness failure; its deterministic checksum path remains useful.

## Confirmed architecture and preserved behavior

- `SceneDocument.map` remains the authoritative editable map; renderer, collision,
  camera, and selection consume that same state.
- Authored wall mutations continue through `CommandHistory`; UI does not directly
  mutate map cells.
- Map catalog refresh, dirty confirmation, save, and target load remain
  failure-preserving.
- Editor input consumption is propagated at the application boundary, and handled
  menu confirmation clears both confirm edges before same-frame state update.
- UI preference parsing/persistence remains headless, validated, transactional, and
  separate from composition.
- UI layers remain bounded and preallocated; world projection, logical grid, source
  font, texture count, and framebuffer ownership are unchanged.
- Camera pitch remains accurately defined as a grid-row horizon displacement, not
  angular pitch or vertical-world support.
- Removed legacy editor application states and duplicate painter/persistence paths
  did not return.

## Findings

### Blocker before next phase

**None open.**

#### RB-B1 — Menu focus lacked a non-color-only visible indicator

**Status: Verified resolved during Review B.**

The approved UI zoom plan requires focused controls to retain a non-color-only
distinction. Review found that `app.c` changed only button foreground/background
colors. The smallest correction adds a transient `focused` presentation state to
`UiElement`, a layout-level focus setter, and `>` / `<` edge markers rendered within
the existing button bounds. It does not change assets, focus order, actions, layout
geometry, authored state, persistence, or layer composition. Deterministic tests and
all closure gates above pass.

### Fix during next phase

#### RB-F1 — `UnifiedEditorState` remains a broad public structure

`app.c` reads several editor fields directly for orchestration and rendering. This
is not a second owner or current correctness defect, and the editor domain remains
headlessly tested. During R1/R2 planning, narrow queries should be added only where
the scene/runtime boundary concretely needs them; do not perform a speculative opaque
rewrite or move scene policy into `app.c`.

### Deferred cleanup

#### RB-D1 — Legacy UI staging remains a compatibility seam

Menus and editor text still render through one reusable staging grid before compact
canvas composition. The seam is bounded, preallocated, tested, and documented. Its
replacement belongs with R12 responsive UI semantics, not R1 or an R0 closeout
refactor.

#### RB-D2 — Optimized build profiles remain separate maintenance work

Strict `-O2`/`-O3` profiles remain deferred until optimization-only diagnostics and
checksum/performance comparisons are addressed under the roadmap's dedicated
maintenance checkpoint. The strict unoptimized default remains unchanged.

### Accepted constraints

#### RB-A1 — Current map workflow is intentionally not the final scene system

The chooser remains limited to sorted regular lowercase `.txt` direct children of
`assets/maps/`. New, Save As, arbitrary paths, recent files, and complete-scene
ownership wait for R2.

#### RB-A2 — UI zoom is bounded scaling, not responsive layout

R0 supports 100/125/150/200% UI-only scaling with clipping and one global preference.
Runtime grid resizing, reflow, per-role user settings, pointer-layout redesign, and
visual UI authoring remain R12 work.

#### RB-A3 — Horizon offset remains 2.5D

The accepted range is `[-viewport_rows, +viewport_rows]`. It does not provide camera
Z, angular pitch, vertical collision, slopes, or stacked geometry. Those require the
R1 world-model decision and later R8 planning.

#### RB-A4 — Environment-sensitive performance classifications are not correctness gates

Dummy-video and dense layered workloads may fail renderer-oriented timing thresholds
while completing safely and deterministically. No R0 correctness claim relies on
those timings; algorithmic regressions retain dedicated measurements and checksums.

### Needs product decision

The following are R1 work, not Review B defects:

- height-aware 2.5D versus stacked/full 3D requirements;
- scene packaging and compatibility policy;
- geometry/collision/appearance/optical separation;
- stable-ID persistence, references, allocation, and reuse;
- scene, reusable-asset, runtime-adapter, and cache ownership;
- initial schemas, migration behavior, and validation boundaries.

## Exit-gate assessment

- R0 required outcomes: **pass**.
- Q1 for completed bounded increments: **pass** through their approved plans.
- Q2 strict/focused/failure-path/documentation gates: **pass**.
- Q3 aggregate, matrix, sanitizer, interactive, and applicable stability evidence:
  **pass**, with environment-sensitive timing limits stated explicitly.
- Review B blocker classification: **no open blocker**.

## Next action

Create one scoped R1 requirements and decision plan. It must inventory required and
forbidden behavior, resolve the six named product/architecture decisions, define
representative versioned schemas and legacy-map migration examples, identify owners
and runtime adapters, and specify transactional failure tests. Do not implement R2
scene persistence or select a world model implicitly through a feature patch before
R1's exit gate passes.