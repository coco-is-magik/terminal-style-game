# Application UI Editor — Requirements and Implementation Plan — 2026-09-18

```
################################################################################
#                                                                              #
#   BEFORE DOING ANYTHING IN THIS PLAN, READ:                                   #
#                                                                              #
#     docs/UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md                              #
#                                                                              #
#   IT IS BINDING. IT IS THE GUIDING STAR FOR EVERY COLOUR, EVERY ANIMATION,   #
#   EVERY TRANSITION, AND EVERY LOOK-AND-FEEL DECISION.                         #
#   EVERY PHASE IN THIS PLAN MUST CITE THE CLAUSE IT SERVES.                    #
#                                                                              #
################################################################################
```

## Status and authority

**Approved plan; not an implementation claim.**

This is the current authority for application-UI authoring. It **supersedes**
[`UI_SCENE_SYSTEM_AND_EDITOR_IMPLEMENTATION_PLAN_2026-09-17.md`](UI_SCENE_SYSTEM_AND_EDITOR_IMPLEMENTATION_PLAN_2026-09-17.md)
as the active plan. That document is retained **as reference only**, with a withdrawal note, and no
longer authorises work.

This plan is subordinate to
[`UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md`](UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md). Where the two
disagree about look and feel, the reference of record wins.

It must preserve the product principles in
[`V1_PRODUCT_AND_AUTHORED_MODEL_FOUNDATION.md`](V1_PRODUCT_AND_AUTHORED_MODEL_FOUNDATION.md):
reuse proven systems rather than creating parallel products; keep authored data authoritative; keep
modules contained, single-purpose and testable; load creative choices from validated data; keep
deterministic defaults until the author takes control; do not silently rewrite creative intent; keep
routine authoring direct without scripting.

## Reference of record citation

```text
Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §1 (primary references),
§2 (existing foundation), §2.1 (accepted motion boundary), §3 (accepted values),
§4.1 (faithful preview), §4.2 (white-dominant chrome), §4.3 (preview-first),
§4.4 (motion budget), §4.5 (guidance lives inside the editor).
```

---

## 1. Scope — tightly confined

### 1.1 Step 1 (this plan)

Evolve `make ui-workbench` **in place** into a genuinely complete, readable, palette- and
motion-directed editor for **application UI**:

- data: `assets/ui_elements/*.txt`, `assets/ui_layouts/*.txt`;
- contexts: main menu, pause, settings, quit confirmation;
- front end: the existing `make ui-workbench` target and `--ui-workbench` host.

It is a **development / authoring tool**. It is not bundled in exports. It must not load a world,
map, project flow, or the unified editor.

### 1.2 Explicitly not in scope

- staged `.tui` UI Scenes and the unified editor's UI workspace;
- UI Scene migration phases (pause, protected UI, canonical root, legacy retirement);
- scripting, timeline, plugin systems, arbitrary callbacks;
- audio authoring;
- theme-persistence redesign beyond the existing token contract;
- pointer-model redesign beyond what the editor already needs;
- any second editor host, pane renderer, or animation renderer.

### 1.3 Step 2 — a note only

After Step 1 closes, a **separate plan** may reuse Step 1's interface contract, in-editor guidance
system, and anti-drift gates to improve the **in-project edit-mode UI editor** for staged `.tui` UI
Scenes. The existing `ui_editor_*`, `ui_menu_workspace`, `ui_document` v4, and the
playback/renderer seams remain in the tree, marked **unratified**, as that plan's raw material.

**No Step 2 work is authorised by this document.**

---

## 2. Why the previous attempt failed (cautionary record)

The prior work built a sound data architecture and then implemented its editor interface against
**none** of the reference of record: hardcoded chrome colours instead of the accepted palette; no
`ui_preferences` and no compositor, so preview scale was a no-op that **shrank** the preview; a
two-row footer with one row unconditionally overwritten; a selection marker written **inside** the
preview; a hardcoded preview origin ignoring `editor_baseline` and panel geometry. It self-certified
with "parity" tests that rendered both hosts through the same function, and no gate existed that
could see the composed frame.

**Root cause: the accepted direction was abandoned, and the process had no way to detect it.**

Two secondary errors to avoid:

1. The old plan's **mandatory three-pane composition** would have destroyed the workbench's best
   quality — a faithful, near-full-size preview. This plan treats pane layout as progressive
   disclosure (§5).
2. **"The module exists" was accepted as "the contract is met."** This plan requires a named test or
   artifact behind every claim (§7, gate G8).

The full anti-pattern list is recorded in the reference of record §4.6.

---

## 3. Acceptance surface — what "working editor" means

1. **End-to-end workflow.** Launch → choose context → edit → save → reload works, with no world or
   project initialisation.
2. **Readable at 1920×1080 without leaning in.** Token-derived chrome, clear panel and border
   division, pane focus visible without relying on colour alone.
3. **Faithful preview.** Identical rendering path to normal run. Scale changes preview size only,
   never chrome, and matches the persisted preference.
4. **Complete authoring.** Every field a user needs for Container / Text / Button / animation unit is
   reachable. Enumerated values are browsed, not typed. Add, clone, remove, and context-switch are
   unambiguous and reversible. Overlays never damage authored cells.
5. **Safe persistence.** Validated, same-directory, atomic; a failed write preserves both the file
   and the in-memory edit; status is always visible.
6. **Directed motion** per reference of record §1 and §4.4: bounded, deterministic,
   reduced-motion-safe, never displacing controls.
7. **Self-explanatory.** A user who is not the developer completes the §3 workflow using only the
   guidance shown **inside the editor** (§4). External documentation is a maintenance aid, not the
   acceptance criterion.

---

## 4. In-editor guidance (not documentation)

The acceptance criterion is the interface teaching itself.

- **Reuse the existing precedent.** `assets/editor_tooltips.txt` already defines a `key=text`
  format. It is currently **orphaned** — nothing in `src/` reads it. Extend it to cover every
  application-UI property, action, mode, context, and failure message, and give it a real consumer.
- **Context-sensitive, always visible.** The tooltip for the highlighted property, action, or mode
  is shown in the editor's own footer/help region at all times.
- **Fuller help reachable in-editor.** A help view that explains the workflow, contexts, element
  types, animation units, and save semantics is reachable without leaving the editor.
- **Includes failure guidance.** Validation and save failures explain what to change, not only that
  something failed.
- **Legible at 100/125/150/200%**, reduced-motion safe, and never overlapping authored preview cells.

**Gate:** every editable property and action has a tooltip; every failure path has actionable text;
a non-author completes the §3 workflow from in-editor guidance alone (recorded evidence).

---

## 5. Interface contract

Frozen in Phase A1 as a **literal rendered frame** at the 260×160 editor baseline. Until that
artifact exists, the following is the contract in prose.

### 5.1 Composition

- **Default view is the full-fidelity preview** (reference of record §4.1, §4.3). It is the primary
  surface, not a small inset.
- **Panes are progressive disclosure** (reference of record §4.3). The hierarchy pane and the
  inspector pane appear while editing and collapse back to preview-first.
- **Pane geometry derives from tokens** (`panel_inset_cells` 2, `border_cells` 1, `group_gap_cells`
  2, `major_section_gap_cells` 3, `base_space_cells` 1, `horizontal_label_padding_cells` 2).
- **Chrome spans the 260×160 baseline** (`editor_baseline_columns`/`rows`). Dead space in the editor
  frame is a defect, not a neutral choice.

### 5.2 Role mapping

| Surface | Token |
|---|---|
| Chrome backdrop | `canvas` |
| Pane bodies | `panel` |
| Focused pane / selected row / modal | `elevated` |
| Pane titles, active property values | `text_primary` |
| Labels, hints, tooltip text | `text_secondary` |
| Pane borders, dividers, selection markers | `border` |
| Active context, selection accent | `accent` |
| Focused control | `focus` |
| Selected row background | `selection_background` |
| Unavailable actions | `disabled_text` / `disabled_background` |
| Status meanings | `warning` / `error` / `success` / `destructive` |

Chrome obtains these **only** through `ui_app_theme_adapter`. The adapter gains the roles the editor
needs **additively**. No literal colour may appear in editor chrome modules.

### 5.3 Footer

Three dedicated rows, **never overwritten**, in every mode and state:

1. **Identity and status** — active context, selected element, dirty/save state.
2. **Controls** — the keys/actions currently available, plus the context-sensitive tooltip (§4).
3. **Diagnostics** — warning / error / success messages, including actionable failure text.

### 5.4 Overlays

Selection markers, focus handles, drag/resize handles, and motion material are drawn on their **own
layer**, outside authored preview cells. An overlay may never write into a cell that belongs to the
authored preview. Selection markers sit outside element bounds, following the proven workbench
pattern.

### 5.5 Scale

- Preview: centred scaled layer with `UI_SCALE_INHERIT_GLOBAL` through `ui_compositor`.
- Chrome: `UI_SCALE_FIXED_100` — chrome **never** scales with the preview.
- Preferences: `ui_preferences` with `default_user.ini` / `user.ini` precedence,
  100/125/150/200, persisted exactly as normal run.
- A reduced-motion path is always available.

### 5.6 Motion

Per reference of record §4.4: context enter/exit glyph reassembly at 160/120 ms; focus and small
feedback at 80 ms; relationship cues at 120 ms; nothing displaces controls; reduced motion is
immediate and non-spatial.

---

## 6. Direction compliance

Every phase that touches colour, motion, animation, transition, or interface feel must:

1. open its record with a reference-of-record citation (§5.1 of the reference);
2. cite the specific clause in the phase's design notes;
3. pass the §7 gates;
4. attach the composed-frame artifact that shows the result.

Chrome colour, motion vocabulary, animation behaviour, and transition behaviour are **directed
decisions, not open design problems.**

---

## 7. Anti-drift testing — implemented early

**Explicit non-goal: this plan does not test UI element positioning.** That is the editor's job. The
gates below exist for one purpose: to make the previous drift impossible to repeat. They are built
**first**, in Phase A1, before any interface change.

| Gate | Prevents | Mechanism | Phase |
|---|---|---|---|
| **G1 — Composed-frame oracle** | "The frame was never looked at." | A pure, display-free composition of the editor frame into the 260×160 grid, plus a `make ui-workbench-frame` dump and checksum/snapshot fixtures, reusing the repository's benchmark checksum-determinism pattern. | **A1** |
| **G2 — Independent-oracle rule** | Tautological parity tests (rendering both hosts through the same function). | Any equivalence claim must compare against an independent composition path or a committed snapshot. Calling one function twice is forbidden as evidence. | **A1** |
| **G3 — Palette/token conformance** | Hardcoded chrome colours — the exact prior failure. | A static/policy check for literal `SDL_Color` in editor chrome modules, plus a runtime assertion that chrome colours are token-derived; adapter extension covered by `test-ui-app-theme-adapter`. | **A2** |
| **G4 — Footer integrity** | The overwritten-row bug. | Assert the three footer rows are distinct, non-empty, and not overwritten in every mode and state. | **A2** |
| **G5 — Overlay separation** | Selection/editing handles damaging authored cells. | Assert no editor overlay writes into authored preview cells; overlay layer verified independently. | **A2** |
| **G6 — Scale independence and preview fidelity** | No-op/inverted preview scale; chrome moving with the preview. | Assert scale changes alter preview geometry only, never chrome; assert preview cells match normal-run rendering at 100/125/150/200. | **A3** |
| **G7 — Direction conformance** | Motion drift away from the reference of record. | Bounded vocabulary and timings (80/160/120/120 ms); deterministic replay; exact endpoints; reduced-motion frame equals the static frame; controls, focus markers, and hit targets stable while decorative material moves. | **A5** |
| **G8 — Evidence rule** | "Module exists" accepted as "contract met." | Every requirement maps to a named test or frame artifact in the phase record. Unbacked claims fail review. | **A1, enforced every phase** |
| **G9 — Guidance coverage** | Workflow completable only by the developer. | Assert every editable property/action/failure path has tooltip text; tooltips legible at all four scales and never overlapping authored cells. | **A4** |
| **G10 — No-parallel-implementation** | A second renderer/host created beside the existing ones. | Structure/policy check: no new editor host, pane renderer, or animation renderer; extensions go through existing owners. | **A1, enforced every phase** |

### 7.1 Gate design rules

- Gates must be **machine-checkable**. Prose review is not a gate.
- Gates must **fail loudly** with a diagnostic naming the violated clause.
- Gates are added **before** the behaviour they protect, not after.
- A gate may not be weakened to make a change pass. Weakening a gate requires a recorded change to
  this plan and the reference of record.
- Frame snapshots are evidence artifacts, not pixel-golden lock-in: they are refreshed deliberately
  with a review, never silently.

---

## 8. Phases, gates, rollback

Each phase is independently reviewable and reversible. A later phase must not begin until the
previous phase's gate passes and its manual acceptance (where named) is recorded.

### A1 — Contract, frame oracle, and anti-drift foundation

**Deliverables:** the interface contract frozen as a literal 260×160 frame artifact; the headless
frame-dump oracle (**G1**); the independent-oracle rule (**G2**); the evidence rule (**G8**); the
no-parallel-implementation check (**G10**); the reference-of-record document published and linked
from the documentation index.

**Exit gate:** the oracle reproduces **today's** workbench frame byte-for-byte *before* any behaviour
change; the contract frame is published; G2, G8, and G10 are in force and documented.

**Rollback:** documentation and the new oracle only. No production behaviour changes.

### A2 — Chrome re-base on tokens and geometry

**Deliverables:** the palette adapter extended additively to the full chrome role set; workbench
chrome rebuilt on tokens and token geometry — real pane bodies and borders, pane focus visible
without colour alone, three dedicated footer rows, overlays outside authored cells, dead space
eliminated across the 260×160 baseline.

**Reference of record:** §3.1, §3.2, §4.2, §4.3, §5.2–5.4.

**Exit gate:** **G3**, **G4**, **G5** pass; frame snapshots are token-derived; contrast floors hold;
`test-ui-workbench` and `test-ui-workbench-store` pass; no literal chrome colours remain.

**Rollback:** revert the chrome module; the adapter extension is additive and independently
reversible.

### A3 — Preview fidelity and scale parity

**Deliverables:** preview rendered as an offscreen canvas composited as a centred scaled layer;
`ui_preferences` loaded and persisted exactly as normal run; chrome pinned at 100%.

**Reference of record:** §4.1, §5.5.

**Exit gate:** **G6** passes — preview cells match normal-run rendering at 100/125/150/200, and
scale changes never move, resize, or recolour chrome.

**Rollback:** revert runtime changes; the previous preview path is restored.

### A4 — Authoring completeness and in-editor guidance

**Deliverables:** every editable property and action reachable; enumerated values browsed rather than
typed; add/clone/remove/context-switch unambiguous and reversible; always-visible status; the
guidance system of §4 with a real consumer for the tooltip data, including actionable failure text.

**Reference of record:** §4.5.

**Exit gate:** **G9** passes; a non-author completes the §3 workflow from in-editor guidance alone
(recorded evidence); failure paths preserve both file and in-memory edit.

**Rollback:** revert controller, property, and tooltip additions independently.

### A5 — Directed motion

**Deliverables:** bounded context enter/exit glyph reassembly; focus feedback; relationship cues —
all within the accepted vocabulary, on decorative layers, with the reduced-motion path.

**Reference of record:** §1.2, §1.3, §2.1, §3.3, §4.4.

**Exit gate:** **G7** passes — deterministic replay, exact endpoints, reduced-motion frame equals the
static frame, controls/focus markers/hit targets stable while decorative material moves; manual
review against the reference of record recorded.

**Rollback:** revert motion integration; the editor remains fully usable statically.

### A6 — Product gate and closeout

**Deliverables:** the §3 acceptance surface walked end-to-end on a native 1920×1080 display and
recorded; committed frame snapshots; corrected status/roadmap/README/docs; the superseded plan
archived with its withdrawal note; the Step 2 note published.

**Exit gate:** the named manual gate is recorded; every claim is backed by an artifact (§7 G8); every
colour/motion change cites the reference of record; no unratified claim remains in current
documentation.

**Rollback:** not applicable — this is a records phase.

---

## 9. Verification

### 9.1 New and extended automated gates

- `make ui-workbench-frame` — composed-frame dump plus snapshot/checksum fixtures (G1).
- Extended `test-ui-workbench`, `test-ui-workbench-store` (G4, G5, and authoring correctness).
- Extended `test-ui-app-theme-adapter` (G3 role coverage).
- Extended `test-ui-theme` (G3 contrast floors under the extended role set).
- Extended `test-ui-motion` and `test-ui-animation` (G7).
- New editor-chrome token-conformance policy check wired into `make standards` (G3, G10).
- New guidance-coverage check (G9).

### 9.2 Existing gates that must keep passing

`make test-build`, `make test`, `make test-ui-standards`, `make standards` (cppcheck, policy,
structure, inventory), `make check-project-structure`, `make check-test-inventory`, ASan, UBSan,
strict default application build, and the dummy-driver smoke.

### 9.3 Manual evidence

The §3 acceptance surface, recorded as a dated artifact, at every supported scale, with reduced
motion on and off. **No interface claim without a composed-frame artifact.**

---

## 10. Non-goals and forbidden shortcuts

- No workbench-only renderer. One shared animation evaluator.
- No literal colours in editor chrome.
- No second editor host, pane renderer, or animation renderer. Extend the existing owners.
- No `.tui` or staged-document authoring in Step 1.
- No scripting, timeline, plugin system, or arbitrary callbacks.
- No theme-persistence redesign beyond the existing token contract.
- No motion that displaces, obscures, or delays a control, focus marker, or hit target.
- No persistent/global glitch, random flicker, or decorative trails.
- No test that proves parity by calling one function twice.
- No claim without a composed-frame artifact.
- **No colour, motion, animation, or transition change without a reference-of-record citation.**
- No UI element positioning tests in these gates.

---

## 11. Open decisions to freeze in A1

1. Pane geometry in cells per context, and whether panes are fixed, overlay, or push the preview.
2. Default view: preview-first only, or preview-first with a remembered last pane.
3. Exact footer field order and which channel carries the context-sensitive tooltip.
4. Whether chrome motion applies to all panes or only to context transitions.
5. Which palette roles are added to `ui_app_theme_adapter` and their names.
6. Preview anchoring and centring behaviour across all four scales.
7. Typed-choice inventory per element type and property.
8. Tooltip key naming scheme and how keys map to property/action/mode identifiers.
9. Minimum context coverage for A4 (all four contexts, or a documented subset with a reason).
10. Whether the `make ui-workbench` help/footer reserves a permanent row for the tooltip or shares the
    controls row when needed.

---

## 12. Documentation lifecycle

- This plan is the active authority for Step 1.
- [`UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md`](UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md) is the active
  look-and-feel authority and is cited by every phase.
- [`UI_SCENE_SYSTEM_AND_EDITOR_IMPLEMENTATION_PLAN_2026-09-17.md`](UI_SCENE_SYSTEM_AND_EDITOR_IMPLEMENTATION_PLAN_2026-09-17.md)
  is **withdrawn** and retained as reference; its Phase 3/4 editor-interface statuses are no longer
  authorities.
- `reviews/2026-09-18-ui-scene-phase3-editor-core.md` and
  `reviews/2026-09-18-ui-scene-phase4-standalone-host.md` carry withdrawal banners.
- `CURRENT_STATUS.md`, `FEATURE_ROADMAP.md`, `docs/README.md`, and the root `README.md` are
  corrected to describe the staged UI Scene editor interface as withdrawn and deferred to Step 2,
  while its code remains in the tree.
- Stable documentation is updated only after behaviour is implemented and verified.
- Superseded documents are archived, never silently deleted.

---

## 13. Rollback summary

| Scope | Rollback |
|---|---|
| A1 | Documentation plus a new, unused oracle; no production behaviour change. |
| A2 | Revert the chrome module; the palette-adapter extension is additive. |
| A3 | Revert runtime preview/scale changes. |
| A4 | Revert controller/property/tooltip additions independently. |
| A5 | Revert motion integration; the editor stays fully usable statically. |
| A6 | Records only. |

The `ui-workbench` mode remains isolated behind its existing single run-mode branch throughout, so
every phase is independently reversible.

---

## 14. First safe implementation step

**A1 only.** Publish this plan and the reference of record; freeze the interface contract as a literal
260×160 frame artifact; build the composed-frame oracle (**G1**); put the independent-oracle (**G2**),
evidence (**G8**), and no-parallel-implementation (**G10**) rules in force; and prove the oracle
reproduces today's workbench frame byte-for-byte **before touching any production behaviour**.

That closes the gap that allowed the drift: the project gains the ability to *see* the composed editor
frame and to cite its guiding reference before and after every change.