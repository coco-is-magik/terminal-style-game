# UI Design and Test Standards

This document defines the UI rules that are precise enough to enforce today. It
does not invent the future visual design system, motion language, audio cues, or
theme-token vocabulary still awaiting product design.

## Current automated gate

`make test-ui-standards` runs the nine focused UI rule owners:

- `test-ui-ele` — legacy application UI parsing, hierarchy, alignment, visibility,
  focus/action data, z-order, strict recognized fields, and bounded substitution;
- `test-ui-preferences` — default/user precedence, valid scale presets, atomic
  persistence, invalid-file isolation, and active-session behavior after save failure;
- `test-ui-compositor` — transparent versus intentional-space cells, clipping,
  stable layer order, scale adjacency, restoration, and reference/optimized parity;
- `test-ui-document` — authored hierarchy, stable IDs, ports, validation,
  migration, round trip, and transactional failure;
- `test-ui-layout-resolver` — responsive anchors, sizing, clipping, and invalid
  input behavior;
- `test-ui-render-adapter` — authored visuals, scaling, clipping, selection markers,
  visibility, missing dependencies, and painter order;
- `test-ui-interaction` — topmost pointer hit testing, keyboard traversal,
  eligibility, deterministic replay, and nonmutation on invalid input;
- `test-ui-menu-runtime` — focus/press/activation semantics, typed target requests,
  deterministic replay, and external-output preservation on failure;
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

### Parsing and persistence

- Recognized numeric and RGBA fields use strict full-consumption parsing.
- Malformed recognized fields reject transactionally; unknown-field policy follows
  the owning format rather than one project-wide permissiveness rule.
- User preferences override immutable application defaults only after valid complete
  parsing. Preference writes are atomic; a failed write leaves the selected scale
  active for the current session and reports that it was not saved.

## Rules not yet defined

The following need a focused design decision before tests can enforce them:

- reusable spacing, density, typography, border, and color tokens;
- menu-depth and information-density limits;
- motion timing, reduced-motion behavior, tactile feedback, and audio-cue mapping;
- exact contrast targets and theme variants;
- native pointer behavior after display coordinate conversion;
- screenshot/pixel appearance across render backends and fonts.

When a rule is accepted, add it here first as a measurable invariant, identify one
focused runner, add normal/boundary/failure coverage, and only then include it in
`make test-ui-standards`. Do not enforce subjective appearance through brittle
screenshots without a stable renderer/font/platform contract.