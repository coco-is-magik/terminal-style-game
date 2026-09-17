# Unified UI Scene System and Editor — Requirements and Implementation Plan — 2026-09-17

## Status and authority

**Approved architecture and phased implementation plan; not an implementation claim.**

This document is the current authority for replacing the split application-UI and authored-Menu
systems with one versioned, data-driven UI Scene system and one reusable UI editor core. It records
the accepted requirements, failed prior approach, module boundaries, migration order, acceptance
gates, rollback boundaries, and next safe implementation step.

The current implementation remains the code described by [`ARCHITECTURE.md`](ARCHITECTURE.md),
[`CURRENT_STATUS.md`](CURRENT_STATUS.md), and the historical workbench record until a phase in this
plan is implemented and verified. This plan deliberately does not update stable documentation to
claim that the migration exists.

This plan must preserve the product principles in
[`V1_PRODUCT_AND_AUTHORED_MODEL_FOUNDATION.md`](V1_PRODUCT_AND_AUTHORED_MODEL_FOUNDATION.md):

- reuse proven systems instead of creating parallel products;
- keep authored data authoritative and runtime state derived;
- keep modules contained, single-purpose, and testable through narrow interfaces;
- load creative choices from validated data wherever practical;
- keep deterministic defaults active until the author takes control;
- do not infer, rewrite, reorder, or repair creative intent silently;
- keep routine authoring direct without requiring scripting or a universal graph.

## Product outcome

The project will have one canonical UI Scene document system used by:

1. protected application/bootstrap UI;
2. editable project UI, including the pause screen;
3. production runtime rendering and interaction;
4. a standalone UI editor launched through a dedicated Make target;
5. the UI-designing workspace embedded in the unified editor;
6. headless validation, preview, playback, and regression tests.

The standalone and embedded editors are hosts for the same editor core. They must not duplicate
document mutation, hierarchy, inspector, command history, rendering, interaction, animation
playback, validation, or persistence behavior.

The only intentionally compiled UI is a minimal emergency recovery surface. It must not grow into
a second full UI implementation.

## Confirmed current baseline

The following statements were confirmed by source and asset inspection on 2026-09-17:

- `UiDocument` version 3 is the authoritative staged format for authored
  `assets/menus/*.tui` Menu documents.
- `UiDocument` currently supports Container, Text, and Button elements, stable IDs, hierarchy,
  responsive layout, scale, native/sprite visuals, colors, fill, border, alignment, visibility,
  and Button flow ports.
- `UiMenuWorkspace` already owns staged documents, an exact saved snapshot, bounded undo/redo,
  hierarchy projection, typed add/remove/rename/reparent/reorder operations, pointer move/resize,
  preview settings, Save/Discard, and atomic single-document persistence.
- `ui_layout_resolver`, `ui_render_adapter`, `ui_interaction`, and `ui_menu_runtime` already provide
  separate headless layout, rendering, interaction, and runtime seams.
- `FlowProjectCatalog` currently scans validated direct children under `assets/menus/*.tui` and
  exposes their Button ports to project flow.
- The unified editor embeds `UiMenuWorkspace`, but its UI workspace presentation is currently
  rendered by a large host-specific function in `unified_editor.c`.
- The application bootstrap menus currently use a separate `UiElement` system under
  `assets/ui_elements/` and `assets/ui_layouts/`.
- The current application UI workbench is a separate immediate-write editor for that legacy
  system.
- `.tui` version 3 does not yet represent focus effects, entry/exit/activation effects,
  Animation elements, lifecycle triggers, or typed system actions.

No migration described below is implemented merely because this plan exists.

## Failed prior approach and lessons

### Attempt

Extend the lightweight application UI workbench with reusable Animation units, transition and
effect editing, add/remove operations, shared runtime rendering, and preview hotkeys.

### Result

Automated parser, evaluator, controller, build, sanitizer, and timeout-smoke checks passed, but the
interactive workbench was not usable enough to serve as the project's UI authoring foundation.
The work was reset to the prior implementation.

### Failure modes

1. The workbench scaled a full 260x160 grid-sized canvas rather than the bounded production menu
   canvas, so the visual result did not establish normal-runtime parity or readable authoring.
2. Editor chrome remained too small to read comfortably at the target display.
3. Workbench selection markers occupied the same cells as `focus_pulse`, overwriting the authored
   effect in the composed frame even though the isolated focus-effect test passed.
4. Continuous preview replay made busy layouts difficult to read.
5. Reusable Animation files were technically available through a flat filename chooser but were
   not discoverable as typed creative choices.
6. Preview commands depended on active context and selection in ways that were not visible to the
   user.
7. Narrow function tests and dummy-driver process-alive smokes did not test the launch-to-edit
   workflow, readability, composed overlays, or visible key responses.
8. A narrow preview-control request expanded into simultaneous changes to ownership, asset
   membership, input, trigger filtering, and documentation.

### Reason rejected

Continuing to expand the workbench would create a second robust editor beside the existing staged
`.tui` workspace and would require a later migration anyway. It violates the project's reuse and
single-source-of-truth standards.

### Mandatory lesson

No UI feature may be accepted from isolated helper tests alone. The composed editor frame,
production preview, default launch workflow, readable chrome, and manual creative workflow are
part of the behavior contract.

## Governing invariants

Every phase must preserve these invariants:

1. One versioned UI Scene format is the authoritative source for protected and editable UI.
2. One production renderer renders the same staged document in standalone preview, embedded
   preview, production runtime, and headless tests.
3. One editor workspace owns all staged mutations and history in both editor hosts.
4. Editor presentation never mutates document internals directly.
5. Runtime time, input, storage, flow navigation, and rendering remain outside the document model.
6. Playback receives explicit event identity, target identity, elapsed time, Reduced Motion state,
   and deterministic seed inputs where needed.
7. Preview and candidate browsing never save, load runtime targets, mutate project flow, or create
   history unless an authored value is explicitly accepted.
8. Project UI cannot make the editor or bootstrap application inaccessible.
9. Invalid project UI cannot prevent application startup or recovery.
10. Protected UI Scene files cannot be overwritten through either editor host.
11. Save failures preserve the previous destination and the complete staged editing session.
12. Existing `.tui` versions remain loadable through explicit deterministic migration.
13. No scripting, timeline editor, arbitrary callback, plugin system, or universal behavior graph
    is introduced by this work.
14. World `SceneDocument`, UI Scene documents, and `FlowDocument` remain separate domain owners.
15. The emergency fallback remains minimal and does not become an alternative customizable UI.

## Canonical ownership model

### UI Scene document

Owns only persistent authored state:

- document version, role, name, and design dimensions;
- stable element IDs and parent relationships;
- element type and painter order;
- layout, anchors, scale, and visibility;
- native/sprite visual configuration;
- Button binding kind and binding payload;
- element effect slots;
- Animation element configuration;
- strict validation and version migration;
- canonical serialization;
- path, current state, saved state, and dirty identity.

It must not render, read input, sample time, navigate flow, draw editor chrome, or load runtime
targets.

### UI Scene workspace

Owns headless authoring state and commands:

- staged document and saved snapshot;
- hierarchy projection and selected element;
- active pane, inspector property, choice candidate, and editor mode;
- bounded undo/redo history;
- add, remove, clone, rename, reparent, reorder, move, resize, and property commands;
- candidate accept/cancel semantics;
- preview configuration and playback request state;
- Save/Discard/Cancel workflow.

It must not depend on SDL, global application state, production wall-clock time, or a particular
editor host.

### UI Scene renderer

Owns production presentation into a bounded `UiCanvas`:

- layout resolution and clipping;
- deterministic painter order;
- native and sprite visual rendering;
- focused, pressed, disabled, and visible state presentation;
- element effects and Animation elements;
- Reduced Motion resolution;
- explicit-time animation composition.

It must not mutate the document, advance hidden time, read input, or load targets.

### UI Scene runtime

Owns runtime-only state transitions:

- runtime focus separate from editor selection;
- keyboard and pointer interaction;
- press/release and activation semantics;
- context-enter, context-exit, focus, activate, and while-visible lifecycle events;
- deterministic playback state driven by explicit time;
- typed flow-port and system-action requests.

It reports requests to the host. It does not execute application-state or flow target loads itself.

### UI Scene persistence

Owns one-document persistence:

- complete validation before writing;
- same-directory temporary file;
- file flush and synchronization;
- platform replacement with truthful commit-state reporting;
- destination preservation on pre-commit failure;
- staged-state preservation on every failed save.

One UI Scene must remain one authoritative file so element, membership, and ordering changes do not
require the legacy multi-file element/layout/master-map transaction.

### UI Scene catalog

Owns sorted, direct-child discovery, extension filtering, document validation, duplicate-name
detection, protected/editable policy, and failed-refresh preservation.

The project-flow catalog consumes only editable project UI Scenes. Protected UI Scenes never
appear as ordinary project-flow assets.

### UI editor presentation

Owns only editor chrome and composition:

- hierarchy panel;
- bounded production preview panel;
- inspector and visual-choice panel;
- status/footer and error presentation;
- pane focus and non-color state markers;
- editor selection overlay on a separate layer;
- readable chrome scaling independent from preview scale.

It consumes workspace state and submits typed workspace actions. It does not own authored mutation.

### UI editor input adapter

Maps host input into one shared `UiEditorAction` vocabulary. The standalone and embedded hosts may
obtain events differently, but they submit the same actions to the same workspace APIs.

## Data locations and protection policy

### Protected system UI Scenes

Proposed location:

```text
assets/system_ui/
  bootstrap_main.tui
  settings.tui
  editor_entry.tui
  confirm_quit.tui
```

Protected scenes:

- use the same versioned UI Scene parser, validator, renderer, and runtime;
- are loaded from data rather than hard-coded layout definitions;
- are excluded from both editor Open choosers;
- reject editor Save/Save As destinations that resolve into the protected root;
- are excluded from project-flow catalog composition;
- are validated during startup;
- may use strictly validated bootstrap/system actions.

Protection is a catalog/root policy, not an editable `protected=1` flag inside a project file.

### Editable project UI Scenes

Proposed canonical location:

```text
assets/ui/
  pause_menu.tui
  inventory.tui
  dialogue.tui
  game_over.tui
```

Editable project UI Scenes:

- appear in the standalone and embedded editor catalogs;
- may appear as project-flow UI nodes where their role permits;
- can be created, duplicated, renamed, edited, and removed with reference validation;
- include the project's pause screen and everything reached after protected Start;
- use contextual system actions only through an explicit runtime policy.

The current `assets/menus/*.tui` root remains supported until an explicit migration phase moves or
copies existing assets safely. No directory rename is part of the first implementation increment.

## Bootstrap, project start, and recovery

The intended high-level flow is:

```text
Executable startup
    -> protected bootstrap_main UI Scene
    -> Start system action
    -> initialize/enter project FlowDocument Start
    -> editable world Scene or UI Scene
```

Protected bootstrap UI does not need to be represented as a project-flow node. This removes the
current naming and ownership collision between application main menu and authored
`assets/menus/main_menu.tui`.

### Emergency startup fallback

If required protected UI cannot load or validate, a compiled minimal recovery screen may expose:

- Open Editor;
- attempt project start only when validation proves it safe;
- Quit.

### Emergency pause fallback

If the editable project pause UI is missing or invalid, a compiled minimal recovery screen exposes:

- Resume;
- Return to protected bootstrap main;
- Open Editor.

Emergency UI is non-customizable, clearly identified as recovery UI, and implemented with existing
low-level rendering and input primitives. It is not required to support the full UI Scene visual or
animation feature set.

## Next UI Scene schema version

The next version extends, rather than replaces, current `.tui` versions 1-3.

### Element types

```text
container
text
button
animation
```

### Button bindings

A Button has one explicit binding kind.

Project-flow binding:

```text
binding=flow
port=start_game
action=
```

Controlled system binding:

```text
binding=system
port=
action=open_editor
```

The parser rejects an unknown binding, an unknown system action, or incompatible simultaneous
payloads. Runtime policy can reject a valid system action in an invalid context without mutating
runtime state.

Initial protected actions may include:

- `start_project`;
- `open_editor`;
- `open_settings`;
- `quit`;
- `confirm_quit`;
- `cancel`.

Initial contextual project actions may include:

- `resume`;
- `open_editor`;
- `open_settings`;
- `return_to_bootstrap`;
- `quit`.

The final enum and context matrix must be frozen in the schema increment before implementation of
runtime dispatch.

### Element effect slots

Ordinary visual elements may own fixed-preset references:

```text
entry_effect=center_out
exit_effect=none
focus_effect=focus_pulse
activate_effect=perimeter_burst
```

Effect slots are concise presentation behavior attached directly to one element. They do not own
scripts, timelines, or arbitrary callbacks.

### Animation elements

Animation elements represent explicit reusable or region-targeted presentation behavior:

```text
type=animation
preset=pause_glitch
target=panel
trigger=context_enter
orientation=horizontal
loop=0
randomize=0
```

Their layout fields define the target-relative or resolved region according to a schema rule that
must be made explicit and tested before format implementation.

### Lifecycle triggers

The initial fixed event vocabulary is:

- `context_enter`;
- `context_exit`;
- `focus`;
- `activate`;
- `while_visible`.

The runtime supplies event identity, target identity, elapsed time, Reduced Motion state, and any
stable deterministic seed. Documents never read global time or implicit randomness.

## Shared editor interaction contract

### Three-pane composition

Both hosts present the same logical interface:

```text
+------------------+----------------------------------------+----------------------+
| HIERARCHY        | PRODUCTION PREVIEW                     | INSPECTOR            |
| parent/children  | bounded UI Scene canvas                | complete properties  |
| type/state       | runtime focus independent of selection | candidate choices    |
+------------------+----------------------------------------+----------------------+
| MODE | document | selected | dirty | history | playback | status/errors/keys   |
+---------------------------------------------------------------------------------+
```

Required presentation behavior:

- preview and editor chrome are separate compositor layers;
- editor selection overlays never overwrite authored preview cells;
- chrome scale and preview scale are independent;
- pane focus is visible without relying only on color;
- controls are shown in a dedicated footer instead of one compressed tooltip line;
- the configured 1920x1080 display is manually accepted for comfortable reading;
- normal preview supports 100%, 125%, 150%, and 200% using the production scaling boundary.

### Hierarchy and lifecycle operations

The hierarchy supports:

- Add Container, Text, Button, and Animation;
- clone a reusable UI unit or template;
- remove;
- rename;
- reparent;
- move earlier/later in painter order;
- visibility and invalid-reference status;
- undo/redo for every accepted structural operation.

Adding is type-first. Reusable choices show type, preset/style, binding/trigger, and target rather
than only a filename.

### Inspector

Enter on a selected element opens the inspector. Properties are type-aware and unavailable values
are hidden or explicitly disabled rather than silently ignored.

Common fields include name, type, parent, visibility, layout, anchors, scale, painter order,
visual mode, colors, alignment, fill, border, and style where retained by the schema.

Button fields include content, binding kind, port/system action, focus effect, activation effect,
entry effect, and exit effect.

Animation fields include preset, target, trigger, orientation, loop, randomize, and region.

### Candidate preview before acceptance

Enter on an enumerated visual property opens a choice panel. Moving among choices:

- temporarily applies the candidate to preview state;
- automatically replays the relevant treatment;
- identifies the staged committed value and current candidate;
- creates no history and no dirty change;
- performs no disk write.

Enter accepts one undoable command. Escape restores the prior staged value exactly.

### Explicit playback

The shared playback controller supports:

- Preview Selected Effect;
- Preview Entry;
- Preview Exit;
- Preview Focus;
- Preview Activation;
- Preview Current Screen;
- Stop/Reset;
- deterministic Replay.

Status exposes event, preset, target, elapsed time, duration, loop state, and Reduced Motion state.
One-shot effects stop at the endpoint. Looping occurs only under an explicit preview command or the
authored runtime condition.

### Runtime interaction preview

Editor selection and runtime focus are distinct states. A runtime-interaction mode allows:

- directional Button focus using production interaction;
- production focus effects;
- press/release presentation;
- Enter activation reporting without loading the reported target;
- Escape back to editor control.

## Host boundaries

### Standalone host

The dedicated Make target should become `make ui-editor`. A temporary `ui-workbench` alias may
remain only during migration.

The standalone host initializes only:

- renderer and grid;
- UI-required shared asset registry data;
- editable UI Scene catalog;
- shared UI Scene workspace;
- shared editor presentation and input adapter;
- theme and preferences.

It does not initialize world state, gameplay, camera physics, active project-flow execution, or the
full unified editor.

### Embedded unified-editor host

The embedded host borrows the unified editor's renderer, grid, asset registry, input, project root,
and flow validation context. It uses the same workspace, presentation, action vocabulary, renderer,
playback, and persistence modules as the standalone host.

Host-specific code is limited to lifecycle ownership, input translation, asset borrowing, and
composition into the host frame.

## Phased implementation plan

Each phase is independently reviewable and reversible. A later phase must not begin until the
previous phase's automated gate passes and any named manual acceptance is recorded.

### Phase 0 — specification and baseline protection

Status after writing this document: **plan recorded; implementation not started.**

Required work:

1. Treat this document as the sole current migration plan.
2. Add baseline composed-frame tests for current `.tui` rendering and workspace behavior before
   changing the format.
3. Inventory every legacy application menu action, element property, and visual behavior requiring
   migration.
4. Freeze the next schema version, Button binding enum, system-action context matrix, Animation
   region semantics, and migration defaults.
5. Define exact standalone/embedded host parity traces.

Exit gate:

- no production behavior changes;
- current focused and complete tests pass;
- baseline fixtures and format decisions are reviewable;
- unresolved schema choices are not hard-coded.

Rollback: documentation and baseline tests only.

### Phase 1 — extend the versioned UI Scene document

Required work:

1. Add the next `UiDocument` version.
2. Add explicit Button binding kind and system-action payload.
3. Add element effect slots.
4. Add Animation element type and fixed configuration fields.
5. Add strict class/field compatibility validation.
6. Migrate v1-v3 deterministically with documented defaults.
7. Serialize the new current version canonically.
8. Preserve flow-reference generation from flow-bound Buttons only.

Exit gate:

- v1-v3 fixtures load and migrate;
- current-version documents round-trip byte-stably where promised;
- malformed, duplicate, incompatible, and unknown recognized values reject transactionally;
- no application screen has migrated;
- existing runtime behavior remains unchanged.

Rollback: remove the new version path while retaining untouched v1-v3 behavior and fixtures.

### Phase 2 — shared production runtime and playback

Required work:

1. Extend the shared renderer for effect slots and Animation elements.
2. Add a deterministic playback state module with explicit event and elapsed-time inputs.
3. Extend runtime output to typed flow-port and system-action requests.
4. Add context-specific system-action policy outside the document/runtime core.
5. Add context-enter and context-exit snapshot behavior.
6. Preserve immediate stable interaction and Reduced Motion behavior.

Exit gate:

- headless renderer/runtime tests cover stable, focus, press, activate, entry, exit, loop, endpoint,
  interruption, invalid input, and Reduced Motion;
- no hidden global time or random state exists in the core;
- preview does not load targets or mutate flow;
- old application UI remains the active product path.

Rollback: leave the new schema loadable but unused by production application composition.

### Phase 3 — reusable UI Scene editor core

Required work:

1. Evolve `UiMenuWorkspace` into a UI Scene workspace without duplicating a second workspace.
2. Add Animation and effect properties to typed workspace commands.
3. Add candidate accept/cancel preview state.
4. Add explicit playback requests and status.
5. Keep bounded undo/redo and staged Save/Discard.
6. Extract host-specific workspace drawing from `unified_editor.c` into the shared UI editor
   presentation module.
7. Add the shared `UiEditorAction` input vocabulary.

Exit gate:

- all document edits are staged and undoable;
- candidate browsing is non-dirty and non-historical;
- acceptance creates exactly one history entry;
- the presentation module can render from workspace state without unified-editor internals;
- no application UI migration yet.

Rollback: retain the existing Menu workspace behavior and do not route hosts to the new modes.

### Phase 4 — standalone editor host and host parity

Required work:

1. Add `make ui-editor` and its run mode.
2. Compose readable hierarchy, production preview, inspector, and footer panes.
3. Keep chrome scale independent from preview scale.
4. Route both standalone and embedded hosts through the same workspace and presentation APIs.
5. Keep a temporary `make ui-workbench` alias only if required for migration communication.

Exit gate:

- equivalent action sequences produce identical staged documents, history positions, candidates,
  playback state, and preview cells in standalone and embedded hosts;
- the default launch workflow opens a useful editable document or clear chooser;
- manual 1920x1080 readability and smooth interaction acceptance passes;
- pointer and keyboard operations are both tested through composed host frames;
- no immediate-write legacy behavior is used by the new editor.

Rollback: remove the standalone host route while preserving the shared editor core and existing
embedded workspace.

### Phase 5 — migrate the editable pause UI first

Required work:

1. Create a project-owned editable pause UI Scene.
2. Integrate gameplay pause invocation with the shared UI Scene runtime.
3. Support Resume, Open Editor, Open Settings, Return to Bootstrap, and Quit through validated
   contextual requests.
4. Author entry, exit, focus, activation, and reusable Animation behavior in data.
5. Activate the emergency pause fallback for missing or invalid project pause UI.
6. Keep the legacy pause path available behind a bounded rollback seam during acceptance.

Exit gate:

- pause remains immediate and safe under all actions;
- normal runtime and both editor previews match at stable and sampled motion phases;
- invalid/missing pause UI reaches emergency recovery;
- the pause UI can be edited, saved, reopened, and used without application restart where the
  runtime refresh contract permits;
- manual interaction and readability acceptance passes.

Rollback: route pause invocation back to the legacy application layout without deleting the new
UI Scene asset.

### Phase 6 — migrate protected system UI

Required work:

1. Add protected `bootstrap_main`, settings, editor-entry, and confirm-quit UI Scenes.
2. Route them through the shared renderer/runtime.
3. Enforce protected-root catalog and Save/Save As rejection.
4. Start project flow through the bootstrap Start system action.
5. Activate emergency startup recovery when protected data is unavailable or invalid.
6. Resolve the current authored `main_menu.tui` naming collision through an explicit project asset
   migration, never a silent rewrite.

Exit gate:

- startup, settings, editor entry, project start, quit confirmation, and recovery work from data;
- each protected asset has an independent missing/invalid recovery test;
- protected assets are absent from both editor choosers and project-flow catalog;
- old bootstrap UI remains available only through the rollback seam until manual acceptance.

Rollback: restore legacy bootstrap routing while preserving protected data and shared runtime.

### Phase 7 — canonical editable UI root and flow integration

Required work:

1. Establish `assets/ui/` as the canonical editable UI Scene root.
2. Migrate existing `assets/menus/*.tui` through an explicit tool or deterministic migration step.
3. Update project catalog and flow validation to consume editable UI Scenes by supported role.
4. Preserve names and flow references or report conflicts requiring author choice.
5. Keep compatibility loading only for a documented migration window.

Exit gate:

- no silent flow rewrite;
- duplicate names across old/new roots reject with actionable diagnostics;
- migrated project flow validates and runs;
- both editor hosts use the canonical root.

Rollback: continue reading the old root without deleting or rewriting original assets.

### Phase 8 — retire the duplicate legacy application UI model

This phase is forbidden until Phases 1-7 pass all automated and manual gates.

Required work:

1. Remove the legacy application workbench and immediate-write workflow.
2. Remove migrated `assets/ui_elements/` and `assets/ui_layouts/` ownership.
3. Remove `ui_ele` application-menu rendering only after no production consumer remains.
4. Archive superseded workbench documentation and preserve the failed-attempt record.
5. Update `ARCHITECTURE.md`, asset format documentation, current status, roadmap, test standards,
   and root README after final verification.

Exit gate:

- source search and project-structure checks find no accidental duplicate UI product path;
- protected, editable, standalone, embedded, runtime, flow, and emergency workflows pass;
- migration and recovery documentation is complete;
- complete strict, sanitizer, platform, and manual visual gates pass.

Rollback: not a file-level rollback. Phase 8 requires an accepted migration checkpoint and retained
release artifact or migration branch outside this plan's implementation scope.

## Required automated verification

### Document and migration

- v1-v3 migration defaults and current-version round-trip;
- invalid field, incompatible field, duplicate ID/name/port, missing target, parent cycle, and
  unsupported-version rejection;
- flow-reference output includes only valid flow-bound Buttons;
- system-bound Buttons never appear as flow ports;
- unchanged destination and output object on failed load/save where promised.

### Workspace and commands

- add/remove/clone/rename/reparent/reorder for all four element types;
- exact undo/redo and branch truncation;
- candidate browsing, accept, cancel, and one-command history;
- dirty/saved identity through Save/Discard and durability warning;
- protected destination rejection;
- failed save preserves staged work and original bytes.

### Rendering and playback

- exact bounded preview dimensions;
- clipping, painter order, scale, native/sprite visuals, and selection-overlay separation;
- composed focus effect plus editor selection overlay;
- entry/exit/focus/activate/while-visible events;
- one-shot endpoints and explicit looping;
- deterministic replay from identical inputs;
- Reduced Motion immediate non-spatial result;
- no frame-count or hidden-random dependence.

### Runtime and policy

- runtime focus independent from editor selection;
- press/release and activation semantics;
- typed flow and system requests;
- valid action rejected safely in an invalid context;
- preview activation reports but does not load a target;
- protected assets excluded from project flow;
- emergency startup and pause recovery.

### Host parity

- shared action traces yield identical workspace state in standalone and embedded hosts;
- identical staged documents and explicit playback inputs yield identical preview cells in both
  hosts and the headless renderer;
- host tests cover only lifecycle, input translation, borrowing, and frame composition.

### Full project gates

At each behavior-affecting phase, run the narrowest owner tests first, then the complete relevant
UI aggregate and complete functional suite. Before migration closeout, run strict default and
required diagnostic builds, static/style/inventory/structure checks, ASan, UBSan, supported
platform profiles, and display-backed manual acceptance. Exact commands and results belong in each
phase's implementation record; this planning document does not claim future passes.

## Mandatory manual acceptance

Automated tests cannot approve editor usability. Each editor or production migration phase records
manual evidence for:

1. readable hierarchy, inspector, status, and controls at 1920x1080 without leaning close or
   squinting;
2. clear pane division and active-pane state;
3. smooth keyboard and pointer selection, move, resize, and property navigation;
4. visible candidate preview before acceptance;
5. visible focus, activation, entry, and exit effects unobscured by editor overlays;
6. correct 100%, 125%, 150%, and 200% preview scaling;
7. deterministic stop, reset, and replay;
8. Save, Discard, Undo, Redo, close prompt, failure message, and recovery workflows;
9. standalone and embedded workflows feeling and behaving equivalently;
10. emergency startup and emergency pause operation.

Dummy-driver timeout smokes remain useful for crash detection but cannot satisfy these checks.

## Scope exclusions

This plan does not authorize:

- scripting or arbitrary callbacks;
- a timeline editor;
- a generic behavior or event graph;
- a theme editor or custom-theme persistence redesign;
- audio authoring;
- localization architecture;
- a plugin system;
- a rewrite of project flow;
- merging world Scene, UI Scene, and Flow documents;
- a rewrite of the entire unified editor shell;
- editing protected system UI from inside the application;
- speculative responsive-layout features beyond the accepted anchor, design-size, clipping, and
  scale model;
- deletion of legacy UI before migration and rollback gates pass.

## Known decisions still required before Phase 1 code

The architecture is approved, but these format-level details must be frozen in Phase 0:

1. the exact next UI document version number;
2. the final UI Scene role vocabulary, if roles beyond Menu are persisted;
3. the exact system-action enum and per-host/per-context permission matrix;
4. whether effect slots are present on every element or only compatible element classes;
5. Animation target representation by stable ID, name, or versioned reference form;
6. Animation region coordinate semantics;
7. migration defaults for every new field;
8. how a project identifies its designated pause UI Scene;
9. the compatibility window and conflict policy for `assets/menus/` versus `assets/ui/`;
10. the minimum emergency UI input vocabulary and visual contract.

These are narrow phase-entry decisions, not permission to reopen the one-system architecture.

## Next safe implementation step

Perform **Phase 0 only**:

1. create a complete legacy application-UI migration inventory;
2. add composed-frame baseline tests for the current UI Scene renderer and embedded workspace;
3. define host-parity action traces;
4. write and review the next-version schema table, migration defaults, action policy, and Animation
   coordinate semantics;
5. stop before production format or runtime changes if any required decision remains ambiguous.

No new standalone editor shell, protected asset migration, pause migration, or legacy removal should
begin before that Phase 0 review passes.

## Documentation lifecycle

During implementation:

- each phase receives a dated implementation record with observed failures and verification;
- stable documentation is updated only after the corresponding behavior is implemented and
  verified;
- superseded workbench documents are archived rather than deleted;
- this plan remains the authority until implementation evidence deliberately updates or supersedes
  it;
- `LICENSE` is not part of this work.