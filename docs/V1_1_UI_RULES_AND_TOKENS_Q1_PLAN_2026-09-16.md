# V1-1 UI Rules and Semantic Tokens — Q1 Plan — 2026-09-16

## Status and authority

**D1 palette and D6 motion decisions accepted on 2026-09-16.** The current semantic palette
values and demonstrated controlled-registration/glyph-reassembly vocabulary have manual
approval. D6 remains implemented only as a pure model and isolated diagnostic specimen;
real-context motion integration is separate V1-3 work. Unrelated D2–D5/D7 decisions retain
their own status and are not approved by this motion review.

This is the focused Q1 plan for roadmap phase **V1-1 — UI architecture, measurable
design rules, and semantic tokens**. It translates the accepted product direction into
objective contracts. It must not merge application/editor UI with authored-game UI,
change persisted documents, or itself implement V1-3 pointer/motion integration.

## Accepted constraints

1. Editor chrome defaults to a crisp, grid-aligned, white-dominant,
   selectively-saturated terminal-retrofuturist language with legibility ahead of
   decoration.
2. Authored games may opt into the same language as a template, but authors retain
   explicit control. The template is not hidden engine policy.
3. Shared facilities are immutable value-level tokens and pure services. Editor UI and
   authored UI keep separate documents, runtime state, adapters, persistence, and
   command/history ownership.
4. Keyboard-equivalent access, visible focus, color-independent states, deterministic
   behavior, clipping, painter order, and supported UI scales remain mandatory.
5. Reduced motion is mandatory. Motion never controls document mutation, focus or
   activation eligibility, input priority, or urgent cancel/quit/failure response.
6. Existing valid application UI files, authored Menu files, preferences, and scene
   formats remain compatible unless a later separately approved migration says otherwise.

## Forbidden shortcuts

- Do not encode one-off colors or spacing directly in migrated components.
- Do not share editor/authored documents or mutable runtime state to achieve visual
  consistency.
- Do not use brittle screenshot equality before renderer/font/platform parity exists.
- Do not treat color as the only state indicator.
- Do not introduce animation before reduced-motion equivalents and explicit-time rules
  are accepted.
- Do not let subjective inspiration become an unmeasured acceptance test.
- Do not redesign pointer behavior in V1-1; V1-1 defines the values and policies that
  V1-3 will consume.

## Q1 decision ledger

The following behavior-affecting decisions are unresolved and must be accepted before
implementation. Each decision must produce named constants or formulas, supported
ranges, fallback behavior, and a focused test owner.

### D1 — Semantic color roles and contrast

Define role names and default RGBA values for at least canvas, panel, elevated surface,
primary/secondary text, border, accent, focus, selection, disabled, warning, error,
success, and destructive action. Define objective contrast thresholds for text, focus,
and non-text boundaries, including how alpha is evaluated against known backgrounds.
Specify monochrome/color-deficient continuity cues and invalid-theme fallback.

**Evidence required:** contrast calculations over every required foreground/background
pair, boundary/fallback fixtures, and review at all supported scales.

### D2 — Spacing, density, borders, and geometry

Define the base logical spacing unit, allowed spacing steps, compact/default density,
minimum target dimensions, border weights, corner/junction rules, alignment rules, and
how fractional UI scaling rounds without gaps or overlap. Values must fit the accepted
minimum viewports and exact-cell authored content.

**Evidence required:** deterministic layout fixtures at 100%, 125%, 150%, and 200%,
minimum-viewport boundary cases, clipping, and adjacency checks.

### D3 — Typography and labels

Define semantic text roles using the current character/rendering capabilities without
preempting V1-4/V1-6 Unicode and font decisions. Define hierarchy, emphasis,
abbreviation, truncation, wrapping, concise-label, tooltip/help, and error-detail rules.

**Evidence required:** bounded label fixtures at minimum widths, no hidden essential
action, and keyboard-readable disclosure behavior. Font-family and shaping choices remain
V1-4 work.

### D4 — Focus, interaction, and status states

Define visual/state precedence for normal, hover-capable, focused, pressed, selected,
disabled, warning, error, success, and destructive combinations. Focus must remain
visible independently of pointer hover and color. Disabled and clipped controls remain
ineligible under existing interaction rules.

**Evidence required:** state-table tests in the relevant pure adapter/interaction owner,
including conflicting states and stale focus. Native pointer conversion remains V1-3.

### D5 — Minimum viewports, menu depth, and disclosure

Define minimum supported editor and authored-template viewports, maximum ordinary menu
depth before restructuring is required, which actions must remain directly reachable,
and when progressive disclosure is appropriate. Necessary advanced author control must
not be removed merely to satisfy a depth metric.

**Evidence required:** representative editor, asset picker, inspector, graph, and
authored-menu inventories with measured action depth and minimum-viewport layouts.

### D6 — Motion-policy tokens

Define semantic duration/easing roles only: immediate, feedback, major-context enter,
major-context exit, and any approved relationship cue. Define interruption, reversal,
explicit-time, frame-gap, and reduced-motion behavior. Submenus and nested subcontexts
normally update immediately inside a stable parent.

**Evidence required:** deterministic time-step tables and reduced-motion equivalence.
Actual context transition and pointer implementation remains V1-3.

### D7 — Token ownership and adapters

Define one immutable value model and two separate consumers:

- an application/editor adapter that cannot access authored UI documents;
- an authored-template adapter that copies/resolves defaults into authored presentation
  without sharing mutable editor state or silently overriding explicit author values.

Decide whether initial tokens are compile-time constants or validated read-only loaded
data. Persistence, user theme selection, and project theme files are out of scope unless
Q1 explicitly promotes them with ownership, validation, and failure semantics.

**Evidence required:** dependency-direction review, invalid-input fallback tests, and
proof that token resolution does not mutate documents, history, dirty state, or runtime
interaction state.

## Required codebase inventory before decisions

1. Enumerate current application/editor colors, spacing literals, borders, label rules,
   focus markers, status colors, and viewport assumptions.
2. Enumerate authored UI visual fields and defaults separately.
3. Map each literal to a proposed semantic role or record why it remains content-owned.
4. Identify every consumer that would need an adapter and its current focused runner.
5. Record representative minimum/maximum layouts; do not infer rules from one screen.
6. Identify accessibility guidance used for contrast and motion thresholds and preserve
   the exact source/version in the decision record.

**Inventory result (2026-09-16):** Items 1-5 are complete in
[`reviews/2026-09-16-v1-1-ui-literal-consumer-inventory.md`](reviews/2026-09-16-v1-1-ui-literal-consumer-inventory.md).
The existing editor/application versus authored-Menu ownership split is viable; repeated
editor palettes and transient state colors are token candidates, while authored normal
colors/geometry remain content-owned. Item 6 remains part of the D1/D4/D6 candidate
decision record.

**Candidate decision result (2026-09-16):** Concrete D1-D7 recommendations and WCAG 2.2
sources are recorded in
[`reviews/2026-09-16-v1-1-d1-d7-candidate-decision.md`](reviews/2026-09-16-v1-1-d1-d7-candidate-decision.md).
They were initially approved for isolated implementation and automated evaluation. D1's
current palette and D6's current motion vocabulary have since passed manual evaluation and
are now accepted in `UI_DESIGN_AND_TEST_STANDARDS.md`; unrelated decisions retain their
separate status.

**Provisional policy increment (2026-09-16):** The pure `ui_theme` seam and focused
runner are implemented and verified as recorded in
[`reviews/2026-09-16-v1-1-provisional-ui-theme-policy.md`](reviews/2026-09-16-v1-1-provisional-ui-theme-policy.md).
No production adapter consumes it and no visible behavior changed.

**Motion vocabulary increment (2026-09-16):** `ui_motion` now provides pure explicit-time
phase/value sampling, stable-ID deterministic glyph paths, interruption/reversal, and
immediate non-spatial reduced motion. `make ui-motion-demo` presents major enter/exit,
feedback, relationship, palette-native versus isolated literal-RGB traces, reduced motion,
all scales, and replay/pause/step controls without loading product data. The implementation
and automated evidence are recorded in
[`reviews/2026-09-16-v1-1-ui-motion-demo.md`](reviews/2026-09-16-v1-1-ui-motion-demo.md).
The vocabulary subsequently received manual approval; no application context consumes
motion yet.

## Recommended incremental implementation after Q1 approval

1. Add a pure immutable token/value module with validation and no SDL, document, I/O,
   global-time, or persistence ownership.
2. Add separate editor and authored-template adapters.
3. Add objective token/contrast/layout tests and update
   `UI_DESIGN_AND_TEST_STANDARDS.md`.
4. Migrate one bounded editor component family while retaining its old view until
   focused and aggregate gates pass.
5. Migrate one authored-template example without changing existing authored documents.
6. Perform Q2 review before broader migration or V1-3 implementation.

Each increment must be independently reversible. No broad visual rewrite is accepted as
the first implementation increment.

## Verification ownership

- Existing `make test-ui-standards` remains the focused aggregate.
- `test-ui-ele`, `test-ui-compositor`, `test-ui-layout-resolver`,
  `test-ui-render-adapter`, `test-ui-interaction`, and `test-ui-menu-runtime` retain
  their current contracts.
- A new focused token/policy runner is appropriate only after D1-D7 define a narrow pure
  module; it must be included in `make test` and `make test-ui-standards`.
- Normal, boundary, invalid/fallback, ownership-separation, and reduced-motion cases are
  mandatory.
- Native visual review supplements deterministic cell/layout evidence; it does not
  replace it.

## Q1 exit gate

Q1 passes only when:

1. D1-D7 have accepted measurable answers with no conflicting owner or persistence
   model;
2. the current literal/consumer inventory is complete enough to bound the first
   migration;
3. objective rules and named future test owners are added to
   `UI_DESIGN_AND_TEST_STANDARDS.md`;
4. the first implementation increment, rollback point, and preserved compatibility are
   explicit;
5. V1-3 dependencies—especially motion roles, reduced-motion equivalence, scale and
   viewport conversion assumptions, focus visibility, and major-context policy—are
   sufficiently defined without implementing V1-3.

The D1/D6 dependencies are now accepted. Remaining V1-1 decisions continue under their own
gates; V1-3 motion integration may proceed only through a separate focused plan.

## Next action

Q2 architecture review passed and selected the application menu focus/selection palette
as the first bounded reversible consumer. That adapter and four-literal call-site
replacement are implemented as recorded in
[`reviews/2026-09-16-v1-1-application-menu-palette-adapter.md`](reviews/2026-09-16-v1-1-application-menu-palette-adapter.md).
The boundary was defined in
[`reviews/2026-09-16-v1-1-q2-policy-architecture-review.md`](reviews/2026-09-16-v1-1-q2-policy-architecture-review.md),
and the current palette and D6 motion vocabulary have since been manually approved. The next
safe motion action is a separately scoped V1-3 integration plan that selects one bounded
real context, preserves immediate interaction and reduced motion, and retains rollback. No
broad migration or persistence is authorized by this approval alone.