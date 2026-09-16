# V1-1 D1-D7 Candidate Decision Record — 2026-09-16

## Status

**Provisionally approved for isolated implementation and automated evaluation; not final
product approval. Every rule remains subject to manual visual/interaction review before
visible migration or final acceptance.**

This record converts the V1-1 inventory and authoritative accessibility guidance into
one concrete recommended rule set. Approval may accept the complete set or revise named
decisions. Until approval, V1-1 remains Q1 requirements/design work and V1-3 remains
decision-blocked.

## Evidence and interpretation boundary

Accessibility floors are drawn from the W3C Recommendation **WCAG 2.2, 12 December
2024**, plus its Understanding documents:

- [SC 1.4.3 Contrast (Minimum)](https://www.w3.org/TR/WCAG22/#contrast-minimum):
  ordinary text at least 4.5:1; large text at least 3:1; Level AA.
- [SC 1.4.11 Non-text Contrast](https://www.w3.org/TR/WCAG22/#non-text-contrast):
  information needed to identify active controls and states at least 3:1 against adjacent
  colors; Level AA.
- [SC 2.4.7 Focus Visible](https://www.w3.org/TR/WCAG22/#focus-visible): keyboard focus
  has a visible indicator; Level AA.
- [SC 2.4.13 Focus Appearance](https://www.w3.org/TR/WCAG22/#focus-appearance): the
  stronger Level AAA reference uses indicator area at least equivalent to a two-CSS-pixel
  perimeter and 3:1 focused/unfocused contrast.
- [SC 2.5.8 Target Size (Minimum)](https://www.w3.org/TR/WCAG22/#target-size-minimum):
  pointer targets are at least 24x24 CSS pixels, or meet its spacing/equivalent/inline/
  user-agent/essential exceptions; Level AA.
- [SC 2.3.3 Animation from Interactions](https://www.w3.org/TR/WCAG22/#animation-from-interactions):
  non-essential interaction motion can be disabled; Level AAA.

WCAG directly governs web content, while this project is a native application and game
engine. These criteria are adopted as measurable product accessibility floors, not
claimed as a formal web-conformance statement. WCAG does not prescribe this project's
palette, cell spacing, menu depth, or motion durations.

## Shared deterministic formulas

### Contrast

Use WCAG sRGB relative luminance. For each normalized channel `v = channel / 255`:

```text
linear(v) = v / 12.92                         when v <= 0.04045
linear(v) = ((v + 0.055) / 1.055) ^ 2.4      otherwise
L = 0.2126 * linear(R) + 0.7152 * linear(G) + 0.0722 * linear(B)
contrast = (max(L1, L2) + 0.05) / (min(L1, L2) + 0.05)
```

Do not round before threshold comparison. For alpha below 255, first resolve ordinary
RGBA channel compositing over the actual known opaque background, then apply the WCAG
linearized-luminance formula to the resolved color. Reject a token pair when its
background is unknown or variable and no guaranteed worst-case contrast can be proven.
Tests use declared color values, not backend antialiasing output.

### Scaled cell geometry

Retain the existing deterministic edge rule:

```text
scaled_edge(source_pixels, scale_percent) =
    floor(source_pixels * scale_percent / 100)
```

Adjacent elements derive both shared edges from this function; independently rounded
widths are forbidden. The current cell is 8x8 source pixels. This formula remains a
current-raster contract and must be parameterized when V1-5 replaces the raster.

## D1 — Semantic color roles and contrast

### Recommended default editor palette

All values are opaque sRGB RGBA:

| Role | Hex | Required use |
|---|---:|---|
| canvas | `#05080AFF` | deepest editor/application background |
| panel | `#0D1418FF` | ordinary panel/control background |
| elevated | `#162126FF` | modal/elevated context background |
| text-primary | `#F2F7F8FF` | ordinary text and labels |
| text-secondary | `#A8B4B8FF` | secondary but still active text |
| border | `#64757CFF` | active boundaries and separators |
| accent | `#67F5C2FF` | selected context and restrained emphasis |
| focus | `#A8FFE1FF` | keyboard focus indicator |
| selection-background | `#123D32FF` | selected row/control background |
| disabled-text | `#8D999DFF` | inactive labels |
| disabled-background | `#161D20FF` | inactive control background |
| warning | `#FFD166FF` | warning state |
| error | `#FF6B7AFF` | error state |
| success | `#71F79FFF` | success state |
| destructive | `#FF8894FF` | destructive action/state |

Measured candidate ratios include:

- primary/canvas 18.588:1; primary/panel 17.194:1;
- secondary/canvas 9.455:1; secondary/panel 8.746:1;
- border/canvas 4.187:1; border/panel 3.873:1;
- accent/canvas 14.745:1; focus/panel 15.974:1;
- primary/selection-background 11.174:1;
- disabled-text/disabled-background 5.831:1;
- warning/canvas 13.929:1; error/canvas 7.303:1;
- success/canvas 14.831:1; destructive/canvas 8.800:1.

### Rules

1. All current fixed-raster text is treated as ordinary text and requires at least
   4.5:1. V1-1 does not use WCAG's large-text exception.
2. Active control boundaries, focus, selection, warning, error, success, and destructive
   cues require at least 3:1 against every adjacent resolved color.
3. Focus, selection, warning, error, success, destructive, pressed, and disabled states
   each require a non-color cue: marker glyph, border/fill pattern, label, or shape.
4. Disabled controls remain visibly legible even though WCAG excludes inactive controls;
   they must not resemble active controls and remain interaction-ineligible.
5. Invalid or incomplete editor themes fall back atomically to the complete default
   palette. Per-role fallback is rejected because it can create untested pairs.
6. Authored Menu normal colors remain author-owned. The engine may validate or warn, but
   must not silently rewrite them. The shared template copies defaults only into newly
   created content or explicit author requests.

**Alternative rejected:** pure white on pure black everywhere. It passes contrast but
does not provide the accepted panel hierarchy or selective saturation and encourages
state to depend on color inversion alone.

## D2 — Spacing, density, borders, and geometry

### Recommended cell tokens

- base unit: 1 logical cell;
- spacing steps: 0, 1, 2, 3, 4, 6, and 8 cells;
- compact row: 1 cell high, keyboard-only or pointer-spacing-exception use;
- ordinary pointer control: minimum 3 cells high and 3 cells wide at 100%;
- ordinary horizontal label padding: 2 cells on each side when space permits;
- panel inset: 2 cells; related-item gap: 1 cell; group gap: 2 cells;
- major-section gap: 3 cells;
- ordinary border: one cell band using role-specific glyphs;
- focus indicator: dedicated marker/border cells plus a resolved focus color; it must
  not reduce the control's activation area.

At the current 8x8 raster, 3x3 cells equal 24x24 source pixels at 100% and grow at every
supported accessibility scale. An undersized target is allowed only when it satisfies the
WCAG spacing/equivalent/inline/essential exception and has a keyboard equivalent. Existing
one-row application controls remain compatible during migration because their vertical
centers are commonly separated by three cells; new ordinary pointer controls use 3-cell
height by default.

Compact and ordinary density are layout choices, not global zoom. UI scale remains
100/125/150/200%, and world/crosshair scale remains independent.

**Alternative rejected:** pixel-valued spacing tokens. They duplicate the current 8x8
raster assumption and would obstruct V1-5.

## D3 — Typography and labels

1. V1-1 defines semantic roles only: title, section heading, body, control label,
   secondary/help, status, and code/identifier. It does not select fonts, shaping,
   grapheme behavior, or large-text metrics; those remain V1-4/V1-6.
2. All roles use the current one-cell glyph raster and the 4.5:1 text floor.
3. Labels name the action or value directly. Prefer `Save`, `Discard`, `Cancel`, and
   `UI Scale` over instructional sentences inside controls.
4. Essential action names may not be silently truncated. When a label cannot fit, use in
   order: wider layout, accepted concise label, deterministic ASCII ellipsis plus adjacent
   full help text, or progressive disclosure. A clipped label with no full equivalent is
   invalid.
5. Technical identifiers remain visually distinguishable from display labels and are
   never rewritten for style.
6. Help/status text may wrap or scroll deterministically but cannot obscure the focused
   control, urgent error, or available cancel path.

## D4 — Focus, interaction, and status states

State precedence for visual resolution:

```text
disabled > urgent error > pressed > focused > selected > hover > normal
```

Warning/success/destructive meaning may coexist with focus or selection; focus markers
remain visible and take indicator color precedence without erasing the semantic label or
glyph cue.

Rules:

1. Keyboard focus is always visible when a keyboard-operable context is active.
2. Focus uses both `> <` or an equivalent border marker and the focus color. It is never
   represented only by hue, brightness, or animation.
3. At least one complete cell band or state-fill region around/within the focused control
   uses a color with at least 3:1 contrast against adjacent unfocused colors. This exceeds
   the area of a two-pixel perimeter for ordinary 8x8-cell controls when implemented as a
   full cell band; tests verify the resolved cells rather than backend pixels.
4. Pressed uses `#` or a filled equivalent; disabled uses `!` or an explicit disabled
   label/pattern; selected uses a persistent marker distinct from transient focus.
5. Hover never removes keyboard focus and never changes activation eligibility.
6. Invalid combinations resolve deterministically by precedence and never mutate input,
   focus, documents, or external outputs.

## D5 — Minimum viewports, menu depth, and disclosure

### Recommended viewport policy

- application/editor chrome baseline: 260x160 logical cells, preserving the currently
  shipped layout while V1-1 migrates bounded component families;
- authored template guaranteed minimum: 40x15 logical cells;
- authored template reference sizes: 40x15, 60x20, and 80x25;
- existing valid authored documents below or above those sizes remain valid; the template
  guarantee does not become a format restriction;
- all guarantees apply at effective logical dimensions after UI scale.

V1-1's first migration must prove its bounded component at the 260x160 editor baseline
and authored-template examples at all three reference sizes and all four scales. A later
phase may lower the editor minimum only after application layouts become responsive.

### Recommended depth policy

- ordinary task path: at most two simultaneous major contexts (stable parent plus one
  modal/major child);
- a confirmation prompt may temporarily be the second context, not a third hidden layer;
- nested inspectors, tabs, property groups, and submenus inside a stable parent are
  subcontexts and change immediately rather than growing the major-context stack;
- common actions—Save, Undo, Redo, Cancel/Back, Test/Preview, and primary create/open—must
  remain keyboard-reachable without traversing more than two major-context transitions;
- deeper information uses sections, search/filter, or progressive disclosure without
  removing advanced author control.

`MENU_STACK_MAX` remains a defensive storage bound and is not the design limit.

## D6 — Motion-policy tokens

> **Approved amendment (2026-09-16):** D6 means controlled display registration and
> glyph reassembly, never a persistent/global glitch effect. Semantic warning, error,
> success, and destructive colors are not decorative RGB trails. Palette-native
> accent/focus traces are the reusable candidate; one literal-RGB treatment exists only as
> an isolated diagnostic comparison. Stable text, focus, controls, hit targets, semantic
> states, and interaction eligibility do not move. The pure model and `make ui-motion-demo`
> specimen were manually approved with the current timings and visual treatment. This
> approves the vocabulary and constraints; real-context integration remains V1-3 work.

### Accepted timing roles

| Role | Duration | Use |
|---|---:|---|
| immediate | 0 ms | subcontext changes, focus, urgent state, cancel/quit/failure |
| feedback | 80 ms | bounded press/release emphasis without movement |
| major-enter | 160 ms | opening a major context |
| major-exit | 120 ms | closing a major context |
| relationship | 120 ms | rare local cue clarifying source/destination relationship |

Position/size transitions use normalized ease-out cubic:

```text
ease_out_cubic(t) = 1 - (1 - clamp(t, 0, 1))^3
```

Opacity/color-only feedback may use linear interpolation. Runtime samples explicit
monotonic elapsed time; it never advances by frame count. A delta at or beyond duration
completes deterministically. Interruption begins the new transition from the current
resolved presentation; reversal does not jump to the old endpoint.

Reduced-motion mode maps every duration to 0 ms and retains static continuity through
title, border, selection, and focus markers. Urgent cancel, quit, failure, focus,
activation eligibility, and document mutation are always immediate regardless of mode.
Reduced motion also forbids chromatic displacement and delayed interaction.

WCAG requires disablement of non-essential interaction motion but does not prescribe
these durations. The accepted values are product choices: short enough to preserve the
accepted responsive/tactile goal and bounded enough for deterministic testing.

**Alternative:** 120/100 ms enter/exit. This is more abrupt but valid if product review
finds the recommended major-context motion too prominent.

## D7 — Token ownership and adapters

1. Initial tokens are immutable compile-time values in one pure module. No file I/O,
   SDL, global mutable state, documents, command history, persistence, input, or time.
2. Validation and contrast/geometry formulas are pure functions with transactional output.
3. The editor/application adapter maps semantic roles to existing `ui_ele`, `app.c`, and
   unified-editor consumers without accessing authored Menu documents.
4. The authored-template adapter copies accepted defaults only during explicit creation,
   template application, or reset-to-template actions. Runtime interaction state may
   borrow transient focus/pressed/disabled roles but cannot overwrite persisted normal
   authored values.
5. Existing application UI assets and authored Menu v1-v3 files remain readable and
   byte-compatible. V1-1 adds no schema or migration.
6. User-selectable themes, project theme files, arbitrary palette persistence, and theme
   import/export remain deferred until ownership, validation, contrast-warning policy,
   and atomic persistence are separately approved.

**Alternative rejected:** load initial tokens from a new theme file. It adds persistence,
failure, migration, and ownership questions before the default contract is proven.

## Proposed first implementation increment after approval

1. Add a pure `ui_theme` value/policy module containing the D1 palette, contrast formula,
   D2 geometry tokens, D4 state precedence, and D6 duration/easing values.
2. Add `test-ui-theme` with exact palette, contrast threshold/no-rounding, alpha
   composition, scale geometry, invalid input, state precedence, timing, interruption,
   and reduced-motion tests.
3. Register it in `make test` and `make test-ui-standards`.
4. Do not migrate a visible component in the same increment. Q2 reviews the seam and
   objective tests before adapters or presentation change.

Rollback is deletion of the isolated module/runner and Make registration. No format,
asset, preference, or visible behavior changes in this first increment.

## Approval questions

Approval is required for the behavior-affecting project choices below:

1. D1 palette and atomic fallback.
2. D2 3x3-cell ordinary pointer target and spacing rhythm.
3. D3 label/truncation hierarchy.
4. D4 state precedence and full-cell focus treatment.
5. D5 260x160 editor baseline, 40x15 authored-template minimum, and two-major-context
   ordinary depth.
6. D6 80/160/120/120 ms timing roles and cubic easing.
7. D7 compile-time immutable first increment with no persistence.

The isolated pure policy module may be implemented under this provisional approval. No
adapter, visible migration, final standards promotion, persistence, or V1-3 work begins
until manual results are explicitly approved or the affected provisional rules revised.