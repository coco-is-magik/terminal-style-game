# UI Design and Test Standards

This document defines the UI rules that are precise enough to enforce today. It
does not promote the provisional motion language, audio cues, or remaining
theme-token decisions that still require product review.

## Current automated gate

`make test-ui-standards` runs the twenty focused UI rule owners:

- `test-ui-ele` — legacy application UI parsing, hierarchy, alignment, visibility,
  focus/action data, z-order, strict recognized fields, and bounded substitution;
- `test-ui-preferences` — default/user precedence, valid scale presets, atomic
  persistence, invalid-file isolation, and active-session behavior after save failure;
- `test-ui-compositor` — transparent versus intentional-space cells, clipping,
  stable layer order, scale adjacency, restoration, and reference/optimized parity;
- `test-ui-theme` — provisional semantic values, WCAG contrast math, scale geometry,
  state precedence, explicit-time motion, interruption, and reduced-motion policy;
- `test-ui-motion` — pure stable-ID motion sampling, exact phase boundaries,
  deterministic glyph paths, interruption/reversal, endpoints, invalid time, and
  immediate non-spatial reduced motion;
- `test-ui-pause-motion` — first real-context enter/exit boundaries, deterministic replay,
  reversal continuity, invalid/backward time, stable endpoints, and decoration-free reduced
  motion without menu ownership;
- `test-ui-animation` — exact pause-glitch destination parity, actual-glyph center-out,
  production/workbench trigger filtering, deterministic bounded random placement, looping,
  and Reduced Motion;
- `test-ui-workbench-store` — backward-compatible preset defaults, strict class compatibility,
  canonical atomic round trips, destination preservation, and self-parent rejection;
- `test-ui-workbench` — bounded application-context loading, ancestor-container inclusion,
  deterministic element/property navigation, safe action routing, and invalid-operation
  nonmutation;
- `test-ui-workbench-frame` — display-free 260x160 composed-frame contract: explicit-time
  determinism through recorded checksums, three-row footer distinctness across all four contexts,
  help-footer stability across supported scales, selection overlays outside authored cells, and
  invalid-input rejection;
- `test-ui-workbench-chrome` — single-owner chrome geometry and token roles: all role colours
  from provisional tokens, token borders plus non-colour pane focus, identity/controls/diagnostics
  footer rows, and invalid-input rejection;
- `test-ui-app-theme-adapter` — exact provisional application-menu role mapping, alpha
  preservation, stable repeated resolution, and invalid-output handling;
- `test-ui-theme-demo` — complete provisional role/state specimen, session-only scale
  controls, deterministic cells, bounds, transactionality, and runtime preconditions;
- `test-ui-motion-demo` — isolated registration/reassembly specimen, replay/pause/step
  controls, chromatic-channel comparison, stable controls, reduced motion, supported
  scales, deterministic cells, and runtime preconditions;
- `test-ui-document` — authored hierarchy, stable IDs, v1-v3 to v4 migration, explicit roles,
  Button flow/system/none bindings, effect metadata, flow-port export, canonical round trip,
  validation, and transactional failure;
- `test-ui-layout-resolver` — responsive anchors, sizing, clipping, and invalid
  input behavior;
- `test-ui-render-adapter` — authored visuals, scaling, clipping, selection markers,
  visibility, missing dependencies, painter order, effect slots, bounded Animation elements,
  deterministic explicit-time composition, and Reduced Motion;
- `test-ui-animation-playback` — explicit event/time sampling, loop and endpoint behavior,
  interruption, deterministic replay, invalid/backward time, and immediate Reduced Motion;
- `test-ui-system-action-policy` — frozen host-context permission matrix, typed denial, and
  invalid-input handling;
- `test-ui-interaction` — topmost pointer hit testing, keyboard traversal,
  eligibility, deterministic replay, and nonmutation on invalid input;
- `test-ui-menu-runtime` — focus/press/activation semantics, typed target requests,
  report-only flow/system requests, context enter/exit snapshots, deterministic playback,
  Reduced Motion, and external-output preservation on failure;
- `test-ui-menu-workspace` — staged authoring, hierarchy commands, exact bounded
  history, pointer move/resize transactions, preview settings, and persistence.

The gate is part of the complete `make test` inventory through its individual
runners. The named aggregate exists to give UI work a focused, reviewable command.

## Enforceable rules

### Data and ownership

- Application-owned UI data and authored game Menu documents remain separate.
- Authored UI changes are staged and use their document/workspace command history;
  preview/test behavior must not silently save, load targets, rewrite game flow, or
  mutate unrelated application state.
- Runtime adapters borrow authored data and preserve caller-owned output on invalid
  input where their API promises transactional behavior.
- The dedicated application UI workbench edits existing `assets/ui_elements/*.txt` assets and
  can clone an existing reusable element/animation into, or detach a direct member from, main,
  pause, settings, and quit-confirmation layouts. Detach preserves the source asset. It does not
  edit authored `.tui` menus, provide free-form creation, own history/drafts, or persist themes.
- Accepted workbench edits save immediately through validated same-directory temporary files,
  file sync, atomic replacement, and active-layout reload. Failed pre-commit writes preserve
  both the destination and the in-memory edit value.

### Layout and composition

- Anchors, dimensions, scale presets, clipping, visibility, z/painter order, and
  intentional transparent cells resolve deterministically.
- Scaled fractional edges remain adjacent; clipping cannot resurrect stale glyphs.
- Centered application dialogs and authored preview content remain visible within
  the supported logical resolutions and 100%, 125%, 150%, and 200% UI presets.
- The center crosshair and world scale remain independent from UI accessibility
  scale.

### Interaction and accessibility

- Keyboard traversal skips ineligible elements, has deterministic tie/order rules,
  and recovers safely from stale focus.
- Keyboard Enter is the accepted authored Test-mode activation path.
- Pointer down/up activation semantics are covered at the headless runtime boundary,
  but reliable application-edge, display-backed clicking remains deferred and must
  not be claimed by this gate.
- Hidden, disabled, fully clipped, and otherwise ineligible elements cannot activate.
- Press/activation mismatch and invalid input preserve externally visible outputs.

### Accepted motion vocabulary and integration boundary

- The motion vocabulary is controlled display registration and glyph reassembly, not a
  persistent or global glitch effect.
- Text, focus markers, controls, hit targets, semantic status, and activation eligibility
  remain stable while nearby decorative cells or glyph layers move.
- Warning, error, success, and destructive colors remain semantic. Decorative chromatic
  channels use palette accent/focus roles; one literal-RGB comparison is permitted only in
  the isolated diagnostic specimen and is not a reusable semantic token.
- Motion sampling uses explicit elapsed time, stable IDs, deterministic paths, exact
  endpoints, and no hidden randomness or frame-count progression.
- Reduced motion resolves immediately and non-spatially, with no chromatic displacement or
  delayed interaction.
- Prefer cell/glyph/layer composition. A framebuffer-level RGB split is not approved.
- These are accepted presentation constraints. They do not by themselves integrate motion
  into an application or authored-game context; V1-3 owns that separate implementation.
- The first bounded V1-3 consumer is pause-context decoration beneath an unchanged menu.
  Settings/confirmation remain immediate, and reduced motion is session-only until a
  preference-format migration is separately approved.
- The pure `ui_pause_motion` owner remains the accepted enter/exit regression model; production
  pause rendering is now the selectable `animation_pause_glitch` unit through the shared fixed
  animation evaluator.
- `focus_pulse` and `focus_glitch` are presentation-only normal-menu focus decorations driven
  by explicit runtime time. Stable `>`/`<` markers, content, bounds, focus, and activation stay
  unchanged; reduced motion suppresses animated decoration immediately.
- Reusable animation units and ordinary element transition presets use the same fixed production
  evaluator in normal run and workbench preview. Supported presets are pause glitch, center-out,
  perimeter burst, local glitch, edge trace, chromatic register, and command flash; triggers are
  context enter/exit, focus, activate, and while visible. Button effects are horizontal,
  non-randomized, button-targeted 80 ms feedback with exact clean endpoints. Edge trace and chromatic
  register use palette accent/focus roles; the single literal red/cyan comparison remains isolated in
  the approved motion specimen; command flash never delays activation. Normal application menus reset
  explicit focus age when menu/focus identity changes. Reduced Motion suppresses spatial treatment.
  `input_hold_short` has no production execution semantics until a separate non-blocking input design
  is approved.

### Parsing and persistence

- Recognized numeric and RGBA fields use strict full-consumption parsing.
- Malformed recognized fields reject transactionally; unknown-field policy follows
  the owning format rather than one project-wide permissiveness rule.
- User preferences override immutable application defaults only after valid complete
  parsing. Preference writes are atomic; a failed write leaves the selected scale
  active for the current session and reports that it was not saved.

## Rules not yet finally accepted

The following remaining D2–D5/D7 areas have candidate values or automated policy tests but
still require their own decisions before they become final enforceable presentation rules:

- reusable spacing, density, typography, border, and color tokens;
- menu-depth and information-density limits;
- tactile feedback and audio-cue mapping;
- exact contrast targets and theme variants;
- native pointer behavior after display coordinate conversion remains undefined for V1-3;
- screenshot/pixel appearance across render backends and fonts remains undefined.

These decisions are now tracked as D1-D7 in
[`V1_1_UI_RULES_AND_TOKENS_Q1_PLAN_2026-09-16.md`](V1_1_UI_RULES_AND_TOKENS_Q1_PLAN_2026-09-16.md).
The current static palette values and D6 motion vocabulary have manual approval. The
isolated policy seam, first application-menu palette adapter, static palette specimen, pure
motion model, and motion specimen are implemented and verified. The application pause context and
the B1.2 diagnostic button gallery consume the shared application-UI evaluator. Authored `.tui`
Menu animation remains separately owned and is not changed by B1.2.

When a rule is accepted, add it here first as a measurable invariant, identify one
focused runner, add normal/boundary/failure coverage, and only then include it in
`make test-ui-standards`. Do not enforce subjective appearance through brittle
screenshots without a stable renderer/font/platform contract.
