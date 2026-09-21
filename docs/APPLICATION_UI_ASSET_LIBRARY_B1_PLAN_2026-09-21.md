# Application UI Asset Library B1 — Button Visual Library, Button Motion, and Image-Space Planning — 2026-09-21

```text
Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §2.1 (accepted motion boundary),
§3.1 (palette roles), §3.3 (motion roles), §4.2 (white-dominant editor interface with richer
chromatic activity in previews/live tools/visualizations), §4.4 (motion budget), §5.1 (citation
protocol), and §5.2 (drift alarm).
```

## Status and authority

**Approved direction; B1.1 bounded gallery slice implemented and automatically verified.**

The application UI workbench is generally approved for the current project place. It does what it
needs to do now: bounded application-menu editing, immediate canonical saves, reusable element and
animation cloning, scoped property editing, faithful runtime preview, reduced-motion preview, and
regression coverage for the accepted behavior. It must remain open to future targeted adjustments as
UI needs expand, but current work should move from workbench mechanics to authored UI assets.

This plan starts the next focused workstream: building a useful application UI asset library one asset
class at a time. B1 begins with buttons because they are the most immediately felt missing asset class
and the safest place to prove richer motion and chromatic effects without destabilizing the whole UI.

This plan is subordinate to:

- [`UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md`](UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md) — binding
  colour, motion, transition, and interface-feel authority;
- [`APPLICATION_UI_EDITOR_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-09-18.md`](APPLICATION_UI_EDITOR_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-09-18.md)
  — active application UI editor/workbench plan;
- [`UI_WORKBENCH_ASSET_REFERENCE.md`](UI_WORKBENCH_ASSET_REFERENCE.md) — current application UI
  element and animation asset reference.

## Problem statement

The current workbench is usable, but the authored UI vocabulary is sparse. Compared with the palette
and motion demos, the actual application UI has too few selectable button styles, too few animation
and effect presets, and too little chromatic motion material. The UI benchmark/workbench coverage is
also centered on menu surfaces; later work must cover non-menu UI surfaces once the menu language is
better established.

The missing quality is felt in this order:

1. **Movement of animations** — current effects do not yet carry the smooth, coordinated,
   display-native motion from the inspiration notes.
2. **Colour/chromatic effects** — the palette is generally acceptable, but bounded RGB/chromatic
   separation effects are not available as reusable authored material.
3. **Asset variety** — there is no large library of button variants grounded in the guiding
   inspiration source.
4. **Menu art space** — there is no first-class way to place authored ASCII art on a menu as art
   independent from ordinary scalable controls.
5. **Expanded benchmark surfaces** — non-menu UI surfaces need a later benchmark once the menu design
   and asset vocabulary mature.

## Current inventory baseline

Current application UI supports `container`, `text`, `button`, and `animation` elements under
`assets/ui_elements/`, composed by `assets/ui_layouts/`.

Current style/effect vocabulary is intentionally small:

- button styles: `plain`, `bracket`, `inverse`;
- container styles: `plain`, `frame`;
- text styles: `plain`, `bright`;
- transitions: `none`, `center_out`, `perimeter_burst`, `local_glitch`;
- focus/effect presets: `none`, `focus_pulse`, `focus_glitch`, `input_hold_short`;
- animation presets: `pause_glitch`, `center_out`, `perimeter_burst`, `local_glitch`;
- animation triggers: `context_enter`, `context_exit`, `focus`, `activate`, `while_visible`;
- animation orientations: `horizontal`, `vertical`, `radial`.

`input_hold_short` remains non-production preview metadata. Any production use requires separate
implementation and tests.

## Design constraints

### Required behavior

1. Proceed one asset class at a time. B1 starts with buttons.
2. Prefer data-only asset expansion before adding new C behavior.
3. Promote repeated data patterns into new style/effect presets only after concrete examples prove
   that a named behavior is needed.
4. Keep the workbench as the editing surface for application UI assets.
5. Keep controls, labels, focus markers, and hit targets stable and readable while decorative
   material moves around them.
6. Use explicit elapsed time, stable identities, deterministic paths, exact endpoints, and bounded
   replay.
7. Reduced motion removes spatial/chromatic displacement and uses immediate/static continuity cues.
8. Chromatic effects are decorative display material, not semantic state indicators.
9. Semantic warning, error, success, destructive, disabled, focus, and selected states remain
   distinguishable without relying on decorative RGB separation.
10. All new motion, colour, animation, or transition work cites the reference of record.

### Forbidden shortcuts

- No workbench-only renderer.
- No second application UI animation evaluator.
- No global or persistent glitch filter over the interface.
- No random flicker or frame-count progression masquerading as motion.
- No motion that displaces, obscures, or delays a control, focus marker, hit target, urgent cancel,
  failure, or accessibility response.
- No hardcoded editor-interface colours.
- No treating literal RGB/chromatic traces as semantic warning/error/success/destructive colours.
- No broad `.tui` authored-UI migration in this B1 increment.
- No benchmark placeholder that silently becomes a gameplay requirement.

## Inspiration translation

The inspiration notes require a fictional display system rather than retro decoration. B1 translates
that into reusable button assets and effects:

- crisp cell/glyph forms with visible separation;
- precise controls with stable labels and dependable focus;
- coordinated regions and disturbances, not independent flicker;
- colour traces that separate and recombine as display material;
- precision → controlled disorder → precision for meaningful transitions;
- a few legible material behaviors rather than many generic noise effects.

## B1 asset-class sequence

### B1.1 Button visual library

Create a broad data-first library of reusable button assets. Start with authored element files and
composition patterns before adding new C presets.

Candidate button families:

1. plain technical buttons;
2. bracketed command buttons;
3. inverse / filled buttons;
4. panel-integrated buttons;
5. segmented buttons with cap/divider language;
6. compact utility buttons;
7. large command / hero buttons;
8. confirm / cancel / destructive variants that preserve semantic status clarity;
9. glyph-prefix or marker-prefix buttons;
10. high-emphasis main-menu buttons used sparingly.

Deliverables:

- reusable button element files under `assets/ui_elements/`;
- at least one diagnostic/gallery layout under `assets/ui_layouts/` or an equivalent bounded preview
  path;
- naming conventions for reusable button assets;
- workbench clone/edit verification;
- documented limitations for any button treatment that still needs a new preset.

#### B1.1 implementation record — 2026-09-21

The first bounded slice adds ten reusable `btn_<style>_<role>.txt` templates and the diagnostic
`button_gallery` composition. It intentionally uses only the existing `plain`, `bracket`, and
`inverse` styles, existing `focus_pulse` behavior, existing transitions, and accepted palette
values. The gallery is cache-registered for deterministic load/render coverage and clone-source
discovery, but it is not a new runtime menu or workbench navigation context.

Implemented families in this slice:

- plain technical;
- bracketed command;
- inverse/filled;
- focus-feedback specimen;
- compact plain and bracketed utilities;
- large inverse hero command;
- marker-prefix command;
- semantic confirm and neutral cancel pair.

Known limitations retained rather than hidden by new rendering code:

- panel-integrated treatment requires composition with a container;
- segmented controls require coordinated multi-element interaction semantics;
- a destructive button requires an approved semantic-color treatment and must not use decorative
  chromatic material as its meaning;
- B1.2 is implemented below; no B1.3 transition has started.

Automated evidence: the strict focused element suite passed 20/20 after adding gallery cache, parse,
parent, focus-order, palette, and representative rendering assertions. The strict workbench suite
passed 11/11; it separately owns generic clone/edit/remove behavior, and its normal-context test now
also checks that `btn_inverse_hero.txt` is present in the clone catalog and loaded cache. The full
`make test-ui-standards` aggregate passed. Separate focused ASan with leak detection and UBSan runs
also passed both suites at 20/20 and 11/11. Native visual acceptance of the gallery remains
unclaimed.

### B1.2 Button motion and chromatic effect requirements

After the first visual button library exists, add bounded shared evaluator presets for the missing
motion/chromatic language where data-only composition is insufficient.

Candidate shared effects:

- `edge_trace` — accent/focus trace travels along button boundary;
- `chromatic_register` — red/cyan or accent/focus traces briefly separate and settle around a stable
  button;
- `cell_reassemble` — nearby cells loosen and resolve into the button boundary or supporting frame;
- `scan_lock` — a crisp scan/registration line locks onto the focused button;
- `command_flash` — short activation cue at button edges, never delaying action;
- `signal_noise_bounded` — seeded coherent local disturbance, not random flicker.

#### B1.2 implementation record — 2026-09-21

Implemented through the existing shared `ui_animation.c` evaluator:

- `edge_trace` — palette accent/focus traces travel on rows immediately adjacent to a focused Button;
- `chromatic_register` — palette accent/focus channels separate around a focused Button and settle,
  without carrying semantic meaning; the single literal red/cyan comparison remains isolated in the
  approved motion specimen;
- `command_flash` — palette-native marks converge outside the activated Button and never delay action
  dispatch.

All three are horizontal, non-randomized, Button-targeted 80 ms feedback units with `loop=0` and an
exact clean endpoint. `edge_trace` and `chromatic_register` are focus-triggered; `command_flash` is
activation-triggered. Reduced Motion returns before drawing. The evaluator's authored-cell
restoration keeps button labels, markers, colors, bounds, focus, actions, and hit targets unchanged.

Normal application menus now maintain explicit focus-event age and reset it when menu/focus identity
changes. This replaces the unsuitable menu-enter age for focus units and avoids continuous chromatic
cycling. The workbench already provides explicit event ages through the same evaluator. Workbench
preset selection normalizes constrained metadata transactionally, and cloning a button-effect unit
targets the first Button in the destination menu; menus without Buttons reject the clone.

Deferred candidates:

- `cell_reassemble` belongs with B1.3 transition evaluation rather than this feedback slice;
- `scan_lock` substantially overlaps the accepted `edge_trace` behavior and needs a distinct use case;
- `signal_noise_bounded` remains rejected until it has a behavior-first material definition that
  cannot degrade into random-looking glitch.

Development failures preserved:

1. Rollback snapshot declarations were initially patched into `normalize_property()` instead of
   `ui_workbench_cycle_value()`. Strict `-Werror` compilation rejected the unused/undefined variables;
   the declarations were moved before any test execution.
2. The first isolated clone regression attempted a second removal after reload without re-entering
   edit mode. It correctly received `UI_WORKBENCH_NO_CHANGE`; the test was corrected to restore the
   required edit-mode precondition.
3. Focus templates initially used `loop=1` to compensate for the normal runtime's menu-enter clock.
   That made exact 80 ms endpoints wrap to progress zero. The templates now use `loop=0`, and normal
   runtime owns an explicit focus-event clock that resets on menu/focus identity changes.
4. The first chromatic-register implementation reused the motion specimen's literal red/cyan values.
   Final reference audit rejected that for a reusable production preset: it now uses palette
   accent/focus channels, while the single literal comparison remains isolated in the approved
   diagnostic specimen. The same audit added strict `loop=0`, focused-target activation, and complete
   rollback when constrained workbench normalization cannot find a Button.

Automated evidence: strict focused suites pass at animation 7/7, element/gallery 20/20, workbench
11/11, and app modules 6/6; the strict application build passes. `make test-ui-standards` passes, as
do `make check-ui-workbench-frame check-ui-workbench-policy`. Separate focused ASan with leak
detection and UBSan runs pass all four owners at the same counts. The recorded workbench frame
checksums remain unchanged because the diagnostic gallery is not a workbench navigation context.
Native visual acceptance of the B1.2 gallery effects remains unclaimed.

Implementation path for a new production behavior:

1. add one named case in the shared `src/ui_animation.c` evaluator or the existing focused effect
   owner;
2. add strict parser validation in `ui_ele_*_is_valid()` as appropriate;
3. add deterministic normal and reduced-motion tests;
4. add one reusable `assets/ui_elements/animation_<name>.txt` or button template using the preset;
5. update `UI_WORKBENCH_ASSET_REFERENCE.md` with copyable examples;
6. verify normal run and workbench call the same behavior.

### B1.3 Button transition vocabulary

Only after B1.1/B1.2 demonstrate the button language, expand context transition presets for buttons
and small menu groups.

Candidate transition directions:

- `button_reassemble`;
- `button_release`;
- `panel_register`;
- `menu_resolve`;
- `edge_cascade`;
- `chromatic_settle`.

These are not approved names or implementation promises. They are candidate behaviors to evaluate
against the reference of record and the actual button assets.

## ASCII-art image-space proposal

Menus also need a way to place authored ASCII art as art, independent from ordinary scalable controls.
This is accepted as a likely needed element class, but it should be implemented as a separate bounded
increment unless B1 menu/button composition proves it is immediately necessary.

Preferred concept:

```text
type=ascii_art
```

Possible minimal schema:

```text
name=main_menu_logo
type=ascii_art
parent=main_menu_container
coords=relative
x=4
y=2
width=64
height=18
visible=1
z_index=0
art_file=assets/ui_art/main_logo.txt
art_scale=100
fg=103,245,194,255
bg=5,8,10,255
transparent_space=1
align=center
style=plain
transition=none
focus_effect=none
content=
```

Initial constraints:

- plain text art files under a dedicated `assets/ui_art/` directory or another documented location;
- no focus, action, or hit target behavior;
- no automatic scaling with button/control UI scale;
- explicit art scale, initially native 100% unless a later plan approves integer scaling;
- spaces may be transparent when `transparent_space=1`;
- strict maximum dimensions and deterministic load failure;
- no Unicode/glyph expansion before the V1-4/V1-5 character/glyph foundation;
- no per-frame file loading;
- no hidden system-font or raster-image fallback.

Use cases:

- main-menu logo;
- system/faction emblem;
- warning sigil;
- machine readout block;
- mission stamp;
- decorative menu diagram;
- boot/loading block.

## Later expanded UI benchmark scope

Expanded benchmark coverage is deliberately later than B1 button work. Do not use the current menu-only
surface as proof of all UI readiness.

Candidate future benchmark surfaces:

1. main menu;
2. pause menu;
3. settings;
4. quit confirmation;
5. HUD overlay;
6. runtime prompt/status panel;
7. mission/objective panel placeholder;
8. loadout/character placeholder;
9. inventory/equipment placeholder;
10. notification/toast/status feedback;
11. authored-game UI sample after that owner is ready.

Placeholder benchmark surfaces must be labeled as placeholders and must not silently become gameplay
requirements.

## Verification expectations

For B1 button visual library:

- strict element parse/load tests for every new asset pattern where practical;
- workbench clone/edit smoke for representative assets;
- normal runtime preview of representative button variants;
- frame/checksum snapshots for gallery or menu preview where appropriate;
- no new hardcoded editor-interface colours;
- no change to `.tui` authored-menu documents.

For B1 button motion/chromatic effects:

- deterministic explicit-time tests at 0%, midpoints, and endpoints;
- reduced-motion frame equals or cleanly maps to static/non-spatial continuity;
- controls, focus markers, labels, and hit targets stay stable;
- semantic status colours remain distinguishable and are not decorative traces;
- normal run and workbench use the same evaluator;
- composed-frame artifacts are recorded before claims of acceptance.

For future ASCII-art image space:

- valid art load/render;
- missing file rejection;
- malformed/oversized art rejection;
- transparent-space behavior;
- bounds and clipping;
- explicit art scale behavior;
- workbench and normal runtime parity through the same renderer path.

## Recommended execution order

1. B1.1 — button visual library, data-first.
2. B1.2 — button motion/chromatic effect presets through the shared evaluator.
3. Main-menu button/gallery review using the new library.
4. ASCII-art image-space support if menu composition needs logos/art blocks.
5. Menu composition pass using the new assets.
6. Expanded UI benchmark surface plan and implementation.

## Stop conditions

Stop and return to planning if:

- a desired button appearance requires moving or obscuring the actual control;
- a chromatic effect becomes the only visible state indicator;
- a proposed preset needs a new renderer/evaluator beside the shared one;
- reduced motion cannot express the state change clearly;
- ASCII-art requirements start depending on Unicode/glyph behavior owned by V1-4/V1-5;
- benchmark placeholders start implying unapproved gameplay systems.

## Readiness to proceed

Implementation may begin with B1.1 when the next work session accepts this plan as the active focused
scope. The first implementation should create a small button-gallery slice, not a complete final menu
redesign, so the visual vocabulary can be reviewed before being propagated across every menu.
