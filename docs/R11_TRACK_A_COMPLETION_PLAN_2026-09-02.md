# R11 Track A Completion Plan — 2026-09-02

## Status and authority

**Completed.** R11 I1 sprite runtime, I2 sprite authoring, and I3 minimal triggers
are all implemented and verified, including bundled manual visual/input acceptance.
This document is the file-by-file execution plan that was followed through the
authorized I3 trigger increment, its Q4 closeout, and the decision gates required
before the broader deferred R11 outcomes may be implemented.

The binding decisions remain in `R11_DECISION_RECORD_2026-08-28.md`; the approved
I1-I3 scope and gates remain in
`R11_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-28.md`. If this plan conflicts
with either document, the decision record and approved requirements win until a
new decision record explicitly amends them.

## What “Track A to completion” means

There are two distinct completion boundaries and they must not be conflated:

1. **Approved R11 scope closeout:** implement and verify I3 minimal triggers,
   perform the required Q4 review, and close the approved I1-I3 scope. This is the
   next executable work.
2. **All six roadmap outcomes:** animation, typed objects/components, and any
   game-mode spawn expansion remain behind the written R11 stop boundary. Each
   needs a separate Q1 decision/requirements package before implementation.

The existing single authored spawn already satisfies the baseline spawn outcome.
Game-mode-dependent expansion is not required unless a concrete game-mode
requirement is approved. Animation and objects remain named roadmap outcomes, so
R11 cannot honestly be marked fully Verified against all six broad outcomes until
they are either implemented or explicitly reclassified by an amended roadmap
decision.

## Requirements and regression guard

### Required for I3

- Authored bounded trigger regions with stable `SceneInstanceId` identity.
- One typed condition: `enter_region`.
- Closed typed actions: `set_flag`, `teleport_to_spawn`, and `toggle_light`.
- Strict scene-format validation, additive migration, canonical round-trip, and
  repair diagnostics for dangling references.
- Deterministic runtime firing exactly once per distinct outside-to-inside entry.
- Undo/redo for trigger insert, edit, and remove.
- Unified-editor placement, selection, inspection, removal, and visible failures.
- Runtime/editor adapters invoke a pure session module; the session owns no
  authored document, renderer, input, allocation during tick, or global time.

### Existing invariants that must remain unchanged

- `SceneDocument` is the sole owner of scene-authored values; `WorldState` and
  session state are disposable derived/runtime state.
- The command system is the sole caller of `scene_document_internal_*` authored
  mutations.
- Stable IDs are scene-wide, nonzero, monotonic, persisted through
  `next_instance_id`, and preserved by undo/redo.
- Native scene v1-v8 inputs remain readable through explicit migrations. Unknown
  keys remain strict errors; native saves use the new canonical version only.
- Trigger firing never mutates authored scene data or command history.
- Action references use typed stable IDs. No coordinate-only identity, dangling
  references, arbitrary action strings, generic dictionaries, or scripting.
- Sprite rendering, optical behavior, lighting, collision, vertical physics,
  scene Save/Discard, and current editor input-consumption behavior remain intact.
- No reusable nested-inspector framework is introduced during I3; use the current
  local inspector pattern and leave the generalized component in R12.

### I3 decisions that must be locked before schema code begins

The 2026-08-28 decision record locks the vocabulary but not enough wire semantics
to freeze scene v9 safely. Resolve and record these in an I3 requirements
amendment:

1. **Region geometry:** recommended minimal representation is a normalized,
   axis-aligned half-open rectangle `[min_x,max_x) × [min_y,max_y)` with finite
   coordinates inside map world bounds and strictly positive extents. Decide
   boundary inclusion explicitly.
2. **Action cardinality/order:** decide whether a trigger has exactly one action
   or a bounded ordered action list. Recommended I3 minimum is exactly one action
   per trigger; multiple actions can be represented by colocated triggers and do
   not require nested allocation.
3. **`set_flag` model:** no flag definition or runtime flag store exists. Decide
   flag identity, value domain, capacity, initial state, and persistence. The
   smallest reversible model is a bounded session-only numeric flag table with
   typed integer IDs and boolean values; it must not silently become authored
   persistent gameplay state.
4. **Target rules:** specify which actions require a target. Recommended:
   `toggle_light` requires an existing `SceneLight.id`; `teleport_to_spawn` has no
   target; `set_flag` addresses the locked flag identifier type rather than a
   scene instance ID. Amend the broad “every action target” wording accordingly.
5. **Light toggling:** decide whether disabled state is session-only or authored.
   Recommended I3 behavior is session-only enabled state keyed by light ID, reset
   when the session/document is rebuilt; runtime lighting must consume that view
   without changing `SceneLight` or command history.
6. **Teleport/re-entry:** define whether teleport causes another trigger evaluation
   in the same tick. Recommended: evaluate one snapshot per tick, execute in
   stable trigger-ID order, update inside-state from the post-action player
   position, and never recursively fire during one tick.
7. **Editor behavior:** resolved 2026-09-02 — triggers run in native-scene editor
   Walk mode; Edit mode pauses firing and document mutation/rebuild resets session
   state. `APP_STATE_PLAYING` remains on its deprecated legacy loader until the
   editor feature roadmap is complete, then receives a separate native-scene
   ownership cleanup rather than a duplicate trigger source.
8. **Placement and selection:** lock placement key, default region dimensions,
   resize increments/minimums, overlap behavior, and world-space visualization.
   Do not add input mappings until these user-visible choices are accepted.

If any of these recommendations is rejected, update this plan before implementation.

---

# Stage 0 — Baseline and I3 requirements lock

## Documentation files

### `docs/R11_I3_TRIGGER_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-09-XX.md` — add

Create the binding I3 amendment before code. Record exact enum values, schema v9
grammar, capacities, region rules, action payloads, runtime ordering, reset rules,
editor controls, diagnostic behavior, manual checklist, and exit gates. Include
the eight decisions above and rejected alternatives.

### `docs/R11_DECISION_RECORD_2026-08-28.md` — amend

Link the I3 amendment and record only decisions that alter or clarify D4/D5. Do
not rewrite the original historical decision-time findings.

### `docs/FEATURE_ROADMAP.md`, `docs/TODO.md`, `docs/handoff.md` — update

Mark I3 as planned/active only after the amendment is locked. Keep animation,
objects, and game-mode spawn expansion explicitly deferred.

## Baseline commands

Before source changes, record fresh results for:

```sh
make -j2 check
make benchmark-colored-lighting
make benchmark-sprite-render
make smoke
git diff --check
```

Do not use a known noisy benchmark run as permission to raise a budget. Preserve
the current deterministic checksums and existing documented exceptions.

**Stage exit:** I3 wire/runtime/editor semantics are explicit and the baseline is
recorded. No source format constants change before this gate.

---

# Stage 1 — Pure trigger types and runtime session

Implement the headless behavior before persistence or authoring UI, matching the
roadmap rule that runtime semantics precede authoring.

## `src/scene_types.h` — edit

- Add only allocation-free authored value types locked by Stage 0:
  `SceneTriggerConditionType`, `SceneTriggerActionType`, bounded payload structs,
  and `SceneTrigger`.
- Add `SCENE_MAX_TRIGGERS` and any bounded flag capacity only after the memory and
  interaction limits are justified.
- Do **not** add `SCENE_VERSION_V9` yet; runtime types can be tested before the
  on-disk representation is frozen.
- Keep unions fully initialized/canonical so equality never depends on padding or
  inactive bytes; prefer field-wise comparison in commands/tests.

## `src/entity_trigger_session.h` — add

Define the smallest explicit runtime API. It should:

- borrow the authored trigger/light/spawn views;
- own per-trigger inside/outside state, session flags, and runtime light-enabled
  state if those models are approved;
- accept current player position and an explicit `delta_seconds` even if I3 does
  not consume elapsed time yet;
- return explicit effects/results needed by the camera/lighting adapter;
- expose reset/rebuild behavior and read-only flag/light-state queries for tests;
- remain independent of SDL, renderer, input, `SceneDocument`, command history,
  and globals.

Prefer fixed bounded arrays if the approved capacities make allocation-free tick
and deterministic reset practical. If setup allocation is required, specify
ownership and make rebuild transactional.

## `src/entity_trigger_session.c` — add

- Validate all runtime arguments before mutation.
- Detect outside-to-inside transitions using the locked boundary rule.
- Fire in canonical stable-ID order, independent of authored array order.
- Execute each eligible trigger once per entry; leaving and entering again permits
  one later firing.
- Apply the locked non-recursive teleport rule.
- Keep errors/failures atomic and deterministic.
- Never mutate the borrowed `SceneTrigger`, `SceneLight`, or spawn data.

## `tests/test_entity_trigger_session.c` — add first

Cover at minimum:

- outside, boundary, and inside behavior;
- one fire while remaining inside;
- leave/re-enter fires exactly once again;
- array-order independence and stable-ID execution order;
- each action and its payload;
- missing/disabled target defense even though document validation should reject it;
- teleport non-recursion and post-teleport inside-state;
- zero triggers/capacity boundary;
- reset/document-rebuild semantics;
- invalid arguments and failure atomicity;
- no authored input mutation.

## `Makefile` — edit

- Add `SRC_ENTITY_TRIGGER_SESSION`.
- Add `TEST_ENTITY_TRIGGER_SESSION_RUNNER` and a narrow source group.
- Add the runner to `test`, `check`, sanitizer, and coverage aggregates following
  existing per-test production-source conventions.

**Focused gate:** strict build and `build/test-entity-trigger-session` pass under
normal, ASan, and UBSan configurations.

---

# Stage 2 — Scene v9 format and transactional document ownership

## `src/scene_types.h` — edit

- Add `SCENE_VERSION_V9` and make it canonical only now that the Stage 0 grammar
  and Stage 1 behavior are fixed.
- Update the migration comment to retain v1-v8 as accepted inputs.

## `src/scene_format.h` — edit

- Add `SceneTrigger *triggers` and `trigger_count` to `SceneFormatCandidate`.
- Add `scene_format_migrate_v8_to_v9()`; older candidates gain an empty trigger
  collection without modifying existing values.

## `src/scene_format.c` — edit

- Extend candidate init/destroy and every allocation-failure cleanup path.
- Add a canonical repeated trigger record section using the exact Stage 0 grammar.
- Parse condition/action tokens into closed enums; reject unknown tokens and keys.
- Reject duplicate IDs, invalid/non-finite regions, invalid payload combinations,
  capacity overflow, and IDs inconsistent with `next_instance_id`.
- Validate typed target references after all records are parsed. A missing light
  target becomes a bounded repair diagnostic/load failure according to the locked
  policy, never a fallback or silent deletion.
- Serialize triggers in stable-ID order with exact canonical numeric formatting.
- Migrate v8 to v9 additively; preserve the complete v1→…→v9 chain.

## `tests/test_scene_format.c` — edit before implementation

Add deterministic tests for:

- exact v8→v9 migration with an empty trigger list;
- canonical v9 trigger parse/serialize/reparse;
- source-order independence and stable-ID output order;
- every condition/action token and payload;
- unknown condition/action/key rejection;
- malformed, non-finite, inverted, zero-area, and out-of-map regions;
- duplicate/zero/exhausted IDs and bad `next_instance_id`;
- dangling light target diagnostic;
- action payload fields forbidden for the wrong enum;
- trigger capacity and allocation-failure atomicity;
- legacy v1-v8 fixtures remain accepted.

## `src/scene_document.h` — edit

- Add owned `triggers`, `trigger_count`, and `trigger_capacity` fields.
- Add const query/find APIs; do not expose mutable trigger arrays publicly.
- Extend the top-level ownership comment.

## `src/scene_document_internal.h` — edit

Add command-system-only validation, insert, set, and remove helpers for triggers,
mirroring sprite/light stable-ID APIs.

## `src/scene_document.c` — edit

- Initialize, destroy, candidate-transfer, load, save, and clone/replacement paths
  for owned trigger storage.
- Apply v8→v9 migration in the existing migration chain.
- Keep load transactional: parse/validation/allocation failure leaves the live
  document and runtime untouched.
- Implement const get/find and internal field-wise validation/mutations.
- Include trigger IDs in all scene-wide collision/high-water validation.
- Extend repair diagnostics and save validation for trigger references.
- Do not copy trigger runtime/session state into the document.

## `tests/test_scene_document.c` — edit

Cover null-safe lifecycle, ownership transfer, exact save/reopen, failed-load
preservation, malformed/dangling repair behavior, ID high-water, capacity,
allocation failure, and runtime-world rebuild parity. Explicitly assert session
state is not serialized or retained by a document reload.

**Focused gate:** `test-scene-format` and `test-scene-document` pass under strict,
ASan, and UBSan builds; checked-in legacy fixtures remain unchanged.

---

# Stage 3 — Undoable trigger authoring domain

## `src/command_system.h` — edit

- Add SET/INSERT/REMOVE trigger mutation types, request payloads, retained mutation
  payloads, and public `command_history_{set,insert,remove}_trigger()` wrappers.
- If removing a light with inbound trigger references is forbidden, add a precise
  result such as `CMD_RESULT_TRIGGER_REFERENCE_BLOCKED`; do not overload missing
  target or generic invalid-target status.
- Keep command byte accounting and `EDITOR_COMMAND_MAX_MUTATIONS` explicit. Do not
  raise group limits unless a tested workflow requires it.

## `src/command_system.c` — edit

- Add field-wise equality/validation rather than raw `memcmp` over union-bearing
  trigger values.
- Implement execute, rollback, undo, redo, retained-byte accounting, redo-branch
  truncation, state-ID exhaustion, and allocation-failure behavior.
- Allocate IDs only when insertion can commit; restore `next_instance_id`, state,
  and output values on failure.
- Protect inbound references: removing a referenced light must be rejected or
  atomically cascade only if Stage 0 explicitly approves cascading. Recommended:
  reject removal and report the referencing trigger ID.
- Preserve triggers when structural map edits occur; reject an edit that would
  leave a trigger region outside the resized world unless an explicit transform/
  cascade rule is approved.

## `tests/test_command_system.c` — edit first

Add insert/set/remove and undo/redo cases, stable ID preservation, no-change,
invalid region/action/target, shared-ID collisions, capacity, state-ID exhaustion,
history-byte limit, allocation failure, failed group rollback, redo truncation,
light-reference removal protection, and structural-resize protection.

**Focused gate:** strict `test-command-system` passes; ASan/UBSan cover all new
owned-history paths.

---

# Stage 4 — Trigger selection and inspector presentation

## `src/editor_types.h` — edit

- Add `TriggerSelectionRef` and `SELECTION_TRIGGER` using stable IDs only.
- Extend `SelectionTarget` without storing runtime pointers or coordinates as
  identity.

## `src/editor_selection.h` / `src/editor_selection.c` — edit

- Add a pure trigger-region picker that composes with existing wall/light/sprite/
  surface hits and uses the locked distance/tie policy.
- Extend target equality and revalidation for trigger IDs.
- Keep picking independent of renderer state and editor controller state.

## `tests/test_editor_selection.c` — edit first

Cover inside/edge/outside ray-region hits, occlusion policy, nearest/tie ordering,
array-order determinism, invalid region defense, and stable-ID target output.

## `src/editor_domain.h` / `src/editor_domain.c` — edit

- Add `EDITOR_INSPECTOR_TRIGGER` and typed trigger fields matching Stage 0:
  region coordinates/extents, condition, action, payload/target, and Remove.
- Add pure presentation/formatting/request constructors.
- Hide or mark non-applicable payload fields based on action; never permit stale
  inactive payload bytes to survive an action-type change.
- Keep numeric bounds derived from the current map and approved metadata.

## `tests/test_editor_domain.c` — edit first

Cover inspector labels/order/help, numeric bounds, enum cycling, action-dependent
payload presentation, request construction, no-change, invalid selection, and
map-bound region edits.

**Focused gate:** `test-editor-selection` and `test-editor-domain` pass strictly.

---

# Stage 5 — Unified editor trigger workflow

Do not start until Stages 1-4 establish tested runtime and authored semantics.

## `src/input.h` / `src/input.c` — edit only if Stage 0 assigns a new shortcut

- Add one edge-triggered placement action and reset/mapping tests.
- Preserve gameplay movement, sprite `P`, light placement, menu, painter, and
  Escape ownership. Do not overload a printable painter key while paint mode is
  active.

## `tests/test_input.c` — edit if input changes

Verify mapping, frame reset, modifiers, menu/editor mode isolation, and painter
conflict prevention.

## `src/unified_editor.h` — edit

- Add trigger modal/status/field/controller state only as required by the locked
  workflow.
- Add `EntityTriggerSession` ownership if editor Walk mode runs triggers.
- Keep trigger authoring local; do not introduce the R12 generic nested-menu
  abstraction in this increment.

## `src/unified_editor.c` — edit

- Compose trigger hover picking and selection validation.
- Implement default trigger placement, field navigation/editing, removal prompt,
  visible reference/capacity/status failures, and undo/redo through command APIs.
- Rebuild/reset the runtime trigger session transactionally whenever scene load,
  New/Open, command execute, undo, redo, or relevant asset/runtime rebuild changes
  its borrowed views.
- Render an editor-only trigger-region outline that does not mutate materials or
  authored cells. If the existing highlight module cannot represent regions
  cleanly, keep a narrow trigger overlay helper rather than distorting wall-face
  semantics.
- Pause or run trigger ticks by the Stage 0 editor-mode rule. Trigger actions must
  not dirty the document.
- Revalidate selection and close transient menus on selection/document changes,
  preserving the sprite submenu lifecycle regression fixes.

## `src/editor_highlight.h` / `src/editor_highlight.c` — edit only if appropriate

Add typed trigger-region visualization only if it fits the existing editor-only
overlay boundary without coupling highlighting to trigger session state.

## `tests/test_editor_highlight.c` — edit if highlight changes

Cover region outline visibility, clipping, selected/hover distinction, material/
map preservation, and overlap priority.

## `tests/test_unified_editor.c` — edit first

Add an end-to-end workflow covering placement, selection, immediate inspector
order/style, field editing, each action payload, remove confirmation, undo/redo,
save/reopen, dangling-target visible failure, transient-menu cleanup, and editor
Walk/Edit trigger behavior. Assert runtime actions do not dirty or mutate authored
trigger/light/spawn records.

**Focused gate:** input (if changed), selection, highlight (if changed), domain,
command, and unified-editor suites pass strictly and under focused sanitizers.

---

# Stage 6 — Application/runtime adapter and lighting integration

## `src/app.c` — edit

- Keep `APP_STATE_PLAYING` unchanged on its deprecated legacy map/world path; it
  has no authored trigger data and must not gain a parallel trigger source.
- Pass camera/player position and apply returned teleport effects without giving
  the session ownership of `Camera` or input.
- Ensure scene transitions/reloads reset the session before the next tick.
- Preserve menu pause/input ordering and do not permit same-frame trigger firing
  against a destroyed or replaced document.

## `src/lighting.h` / `src/lighting.c` — edit only if toggle state is session-only

- Add an optional borrowed enabled-state view or narrow predicate to skip disabled
  lights while preserving the exact existing path when no session view is supplied.
- Keep authored `SceneLight` and derived `WorldState.lights` unchanged.
- Extend lighting-cache keys/invalidation only if enabled state can affect cached
  output; never reuse stale light results after a toggle.

## `src/unified_editor.c` — integrate editor Walk adapter

Use the pure session API in the native-scene Walk path; do not duplicate trigger
semantics in the controller. A future Start Game migration must reuse this API.

## Runtime integration tests

- Add focused app-module tests if an existing test seam can drive session reset,
  pause, and teleport without SDL video.
- Extend `tests/test_lighting.c` and cache tests if light enable-state enters
  lighting APIs; verify legacy/null view parity and immediate toggle invalidation.
- Keep renderer tests unchanged unless a visible runtime contract genuinely changes.

**Focused gate:** trigger-session, app modules, lighting/cache where applicable,
vertical physics, camera, and smoke pass. Teleport must reconcile vertical state
through the existing reset seam rather than leaving stale Z velocity/grounding.

---

# Stage 7 — I3 verification, implementation record, and manual acceptance

## `docs/R11_INCREMENT_I3_IMPLEMENTATION_RECORD_2026-09-XX.md` — add continuously

Record delivered behavior, exact files, failures/corrections, schema v9 grammar
and migration, ownership/session boundaries, tests, benchmark methodology/results,
manual checklist, limitations, and deferred work. Never reconstruct failures only
at the end.

## Required automated gates

Run sequentially where build artifacts or sanitizer configurations conflict:

```sh
make -j2 check
make -j2 asan
make -j2 ubsan
make matrix
make smoke
make benchmark-colored-lighting
make benchmark-sprite-render
git diff --check
```

Add a dedicated trigger-session benchmark only if measurements show a meaningful
per-tick workload or if the approved capacity could threaten the frame budget.
At minimum, confirm trigger work does not push existing lighting or sprite paths
over their gates. Record hardware/session context and deterministic checksums.

## Manual acceptance checklist

1. Place and visibly select a trigger region without obscuring world materials.
2. Edit region bounds; verify invalid/zero-area/out-of-map edits are rejected with
   visible feedback.
3. Configure and fire `set_flag`; remain inside without repeated firing, then
   leave/re-enter and observe exactly one additional transition.
4. Configure `teleport_to_spawn`; verify XY, angle if specified, vertical state,
   and no same-tick recursive trigger chain.
5. Configure `toggle_light`; verify immediate light enable/disable and repeatable
   re-entry behavior without scene dirty-state changes.
6. Attempt to remove a referenced light and confirm the locked reject/cascade rule.
7. Undo/redo trigger placement/edit/removal; save, restart, and reopen.
8. Verify malformed/dangling scene repair feedback and that failed Open preserves
   the current scene/session.
9. Verify editor Walk/Edit, menu pause, mouse lock, sprite painter, and movement
   input remain correct.

**Stage exit:** automated gates and manual acceptance pass, evidence is recorded,
and no unresolved I3 requirement is described as implemented.

---

# Stage 8 — Required Q4 review and approved-scope closeout

## `docs/reviews/2026-09-XX-roadmap-r11-entity-trigger-review.md` — add

Review:

- scene v9 schema/migration/repair behavior;
- `SceneDocument` ownership and command-only mutation;
- session lifecycle, reset, ordering, and determinism;
- authored/runtime light separation and cache invalidation;
- editor selection/input/accessibility/status behavior;
- reference deletion and structural-resize rules;
- allocation/failure cleanup and benchmark evidence;
- whether the established seams are mature enough for objects and animation.

Classify every finding using the roadmap Q4 categories. Close blockers before
marking the approved I1-I3 scope Verified.

## Closeout documentation — update last

- `docs/FEATURE_ROADMAP.md`: mark the approved I1-I3 scope Verified, but keep the
  broad R11 phase Active unless animation/objects are reclassified or delivered.
- `docs/R11_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-28.md`: link I3 evidence
  and Q4 findings.
- `docs/TODO.md`: reflect implemented triggers and exact remaining work.
- `docs/handoff.md`: point to the next authorized decision package.
- `README.md` and `assets/README.md`: update last, only for stable user-facing
  trigger format/workflow behavior.

---

# Stage 9 — Deferred outcome decision packages

These are plans-to-plan, not implementation authorization. Execute one Q1 package
at a time after the I3 Q4 review. Do not combine all three into one schema/UI push.

## 9A — Typed objects/components Q1 package

### Questions to resolve

- Which concrete first object behavior is required? A generic component framework
  without a real object is forbidden speculative infrastructure.
- Are reusable definitions asset-owned and instances scene-owned? This is the
  recommended ownership split, but exact component schemas must be known.
- Which component types are closed in the first increment, and which systems
  consume them?
- Object transform/Z/orientation, collision, sprite attachment, trigger identity,
  deletion/reference policy, capacity, missing definitions, and runtime tick order.

### Files to inspect/plan; do not edit before Q1 lock

- `src/scene_types.h`, `scene_format.h/.c`, `scene_document.h/.c`: scene-owned
  object instances and the next additive version only after the model is fixed.
- `src/assets.h/.c`, `asset_loader.c`, `asset_document.h/.c`: reusable typed object
  definitions if approved; add a dedicated object document only when independent
  Save/Discard ownership is actually required.
- `src/entity_trigger_session.h/.c`: extend only for concrete runtime behavior;
  avoid renaming/generalizing it preemptively during I3.
- `src/command_system.h/.c`, `editor_types.h`, `editor_selection.h/.c`,
  `editor_domain.h/.c`, `unified_editor.h/.c`: stable-ID authoring after runtime
  semantics are tested.
- New focused tests should mirror ownership: `test_object_document.c` only if a
  reusable document exists; otherwise scene-format/document/runtime tests suffice.

### Q1 output

Create a dated object decision record and a small runtime-first increment plan.
Only then authorize files and schema version changes.

## 9B — Animation Q1 package

### Questions to resolve

- Does the first animation target sprite pattern frames, object transforms, or
  both? Choose one concrete target first.
- Frame representation: referenced sprite assets versus embedded cells; duration
  units/ranges; loop, once, ping-pong, and terminal-frame semantics.
- Definition ownership, instance playback state, deterministic time accumulation,
  pause/reset/save behavior, missing frame assets, and live editor preview.
- Timeline minimum UX and whether staged edits preview in the world.

### Files to inspect/plan; do not edit before Q1 lock

- `src/assets.h/.c`, `asset_loader.c`, `asset_document.h/.c`: reusable animation
  definitions and asset references; add `animation_document.h/.c` only if the
  approved data has independent dirty/history/save lifecycle.
- A new narrow `animation_player.h/.c` is preferred for deterministic runtime
  playback; integrate with `entity_trigger_session` only if ordering/shared state
  requires it rather than building a generic session framework first.
- `src/sprite_render.c` should consume a resolved current frame, not own time.
- `scene_types.h`, scene format/document, commands, editor domain/controller, and
  tests change only if animation selection/state is authored per instance.
- Add `test_animation_player.c` and, when applicable,
  `test_animation_document.c`; extend sprite benchmark if frame resolution enters
  the render hot path.

### Q1 output

Create a dated animation decision record and split runtime, data, and authoring
into separate increments. Runtime playback must pass before timeline authoring.

## 9C — Spawn expansion Q1 package, conditional

Do not start without a concrete game-mode requirement. The current single spawn
is already authored, persisted, and used. If a requirement appears, decide:

- one active spawn versus multiple named/team/checkpoint spawns;
- selection rules and deterministic fallback;
- stable identity/reference semantics;
- compatibility migration and editor workflow;
- trigger teleport target semantics and missing-spawn behavior.

Likely files after authorization are `scene_types.h`, `scene_format.h/.c`,
`scene_document.h/.c`, `command_system.h/.c`, `world.h/.c`,
`entity_trigger_session.h/.c`, editor domain/controller/selection, and their
focused tests. Do not replace the current fields with a collection without an
explicit migration/default policy.

---

# Final broad-R11 closeout criteria

R11 may be marked fully Verified only when one of these is true:

1. I1-I3, typed objects/components, and animation are implemented and verified;
   spawn expansion is either implemented for an approved game mode or explicitly
   recorded as unnecessary beyond the verified single-spawn baseline; or
2. a new Q1/roadmap decision explicitly reclassifies animation and objects as
   future phases/candidate outcomes and narrows the R11 exit gate accordingly.

In either case:

- all applicable Q1-Q3 gates pass per increment;
- the post-I3 and final entity-boundary Q4 findings are closed or explicitly
  deferred without blockers;
- complete strict, sanitizer, matrix, smoke, benchmark, migration, malformed-input,
  restoration, and manual acceptance evidence is recorded;
- stable documentation matches behavior and no pending item is presented as done.

## Recommended immediate next action

Draft and approve the Stage 0 I3 trigger requirements amendment. In particular,
resolve `set_flag` identity/state and `toggle_light` authored-versus-session state
before adding `SCENE_VERSION_V9` or any trigger source files.