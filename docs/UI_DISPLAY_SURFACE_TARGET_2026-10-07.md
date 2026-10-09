# UI Display-Surface Target — 2026-10-07

**Authority.** Derived from [`UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md`](UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md)
§1 under the **2026-10-07 change of direction** recorded in that document. Where this document and the
Reference of Record disagree, **the Reference of Record wins**.

**Status.** This is the concrete, per-surface translation of §1: it states *what each surface must
become*. It is not an implementation plan and authorizes no code change by itself. The first bounded
slice is chosen separately (see §10, D-4).

**Citation.** Work implementing any part of this document must cite both the governing Reference of
Record clause (§1.1/1.2/1.3, plus §2.1/§4.4 for the stability boundaries) and the section number here.

---

## 1. The shared display language

Every surface below is drawn from the same raw material and obeys the same character. The language is
fixed; only the amount and behaviour differ per surface.

- **Grid truth.** 260 × 160 cells (`config.ini` `grid_width`/`grid_height`), 8 × 8 per cell, with
  visible black gaps between lit cells (`cell_width`/`cell_height` = 8). Glyphs are crisp and
  grid-constrained.
- **One shared visual material.** The interface is *arrangements of a shared material*, not finished
  images exchanged (§1.2). Marks form groupings and boundaries; they do not merely translate.
- **Character.** "Rendered by a distinctive fictional display system, rather than decorated with retro
  effects" (§1.1). Crisp, separated light over diffuse glow. Clear rectangular forms. **Saturated
  primary and secondary colours.**
- **Liveness.** Enough ongoing activity to feel alive (§1.1) — behaviour-first, coordinated, never
  independent flicker (§1.1, §1.3).
- **The invariant.** *"Keep the controls dependable. Let the display feel alive."* (§1.1). Richness
  goes into material and display fields; controls, focus markers, hit targets, and semantic status stay
  stable (§2.1).
- **The identity (locked 2026-10-09).** **Black, white, red, green, blue, and flowy, smooth motion.** A
  black `canvas` surface, white `text_primary`, the neutral `text_secondary` material, and the three
  additive primaries — red `#FF2B2B`, green `#2BFF4F`, blue `#2B55FF` — as chromatic aberration and
  nothing else. Movement is one continuous material: waves travel across the fabric, and a state change
  surges in and retraces out rather than cutting between two pictures. See the change of direction
  recorded 2026-10-09 in
  [`UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md`](UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md).

## 2. Surfaces at a glance

| Surface | Assets / code | §1 role | Target character |
|---|---|---|---|
| Authored menus | `assets/ui_layouts/{main_menu,pause_menu,settings,confirm_quit}.txt`, `assets/ui_elements/*` | 1.1, 1.2 | Dense, chromatic, alive field behind crisp controls |
| HUD overlay | `assets/ui_layouts/hud_overlay.txt`, `hud_*` elements | 1.1, 1.3 | Live, coordinated instrument panel |
| Game world | `src/raycast.c`, `src/renderer.c`, `assets/materials/*.txt`, `assets/palettes/*.txt` | 1.1, 1.3 | Behaviour-first material textures |
| Editor visualizations | `src/unified_editor.c`, material/effect previews | 1.1, 1.3 | Rich chromatic behaviour in previews and live feedback |
| Editor interface | `ui_workbench*`, `ui_editor_*` | §4.2 (as scoped) | Restrained — the only white-dominant surface |

## 3. Per-surface targets

### 3.1 Authored menus (`main_menu`, `pause_menu`, `settings`, `confirm_quit`)

- **Material behaviour.** A coherent decorative field fills the surface behind the controls: it is a
  **solid fabric** whose density is carried by one **travelling wave** — a main wave and a finer ripple
  riding the diagonal from the surface's bottom-right corner to its top-left — so the sheet flows and
  folds as one material while its larger forms stay coherent (§1.1; change of direction recorded
  2026-10-09). Context enter/exit is glyph reassembly — precision → controlled disorder → precision
  (§1.2). All four authored menus carry a `living_field` backdrop (`animation_main_field`,
  `animation_living_field`, `animation_settings_field`, `animation_confirm_field`), and each declares
  `extent=surface` so the field fills the surface.
- **The surface is the display.** A menu is composed into the whole grid, exactly like the world and
  the editor, so it fills the same viewport at the same cell size — there is no inner box and no black
  surround. A bounded backdrop is still authorable (`extent=box`), which is the shape a floating panel
  over a paused game needs; see `reviews/2026-10-08-menu-surface.md`.
- **Chromatic treatment.** A neutral fabric whose saturated material appears **only as an RGB
  chromatic-aberration fringe**: in the **wake** behind every main crest of the travelling wave, and on
  the fabric's edges where it meets a control, the region border, or a gap — the three additive
  primaries, chosen by direction or by how far behind the crest the cell sits, never a secondary colour,
  never a monochrome wash, never a colour fill. The field is a **substrate**: it fills *behind* text and
  controls, so the menu reads as one continuous material.
- **Layer over the frame behind it.** A menu opens over the frame that was on screen when it opened,
  recessed through the theme's overlay response (`dim` 45 / `grey` 60), so pausing reads as a layer over
  the paused game rather than a takeover. Each layout chooses with `underlay=<dim|none>`
  (`src/ui_underlay.c`); see `reviews/2026-10-08-menu-underlay.md`. The recessed frame is decoration and
  never moves a control, a glyph, or a hit target.
- **Stays stable.** Button labels, the focus perimeter around the focused button, focus/selection state,
  hit targets, and interaction eligibility never move and are never obscured (§2.1, §4.4). Reduced
  motion draws no field and no displacement.

### 3.2 HUD overlay (`hud_overlay`)

- **Material behaviour.** The diagnostics (`hud_target_fps`, `hud_actual_fps`, `hud_avg_frame`,
  `hud_worst_frame`, `hud_min_spare`, …) read as a live instrument panel: coordinated register/trace
  activity proportional to load, not decorative noise (§1.1, §1.3).
- **Chromatic treatment.** Semantic: `success`/`warning`/`error` stay authoritative (§2.1). Chromatic
  activity is confined to frames and separators, never over the numbers.
- **Stays stable.** Every readout value and label is stable and legible; activity never displaces or
  obscures a number.

### 3.3 Game world (`raycast`/`normal`, materials, palettes)

- **Material behaviour.** Each material gets a *distinct* §1.3 response — flow (traveling patterns),
  viscosity/goop (gather and stretch), ripple (travel and settle), churn (fold and recirculate), drip
  (accumulation and release), wetness (changing highlights). "Do not apply one generic animated-noise
  treatment to every substance."
- **Chromatic treatment.** A live surface: richer chromatic activity is explicitly sanctioned (§4.2).
  Palettes (`near`/`mid`/`far` stops) plus accent/secondary material.
- **Stays stable.** The crosshair, world scale, and gameplay-affecting frames; motion is texture on
  the world, not a change to world geometry or collision.

### 3.4 Editor visualizations and live tool feedback

- **Material behaviour.** In-editor previews of material/effect behaviour use the same §1.3 language as
  the world. Visualizations may be richly chromatic (§4.2).
- **Stays stable.** The controls around a visualization do not behave like the material (§1.3: "the
  controls stay controls").

### 3.5 Editor interface (the tool itself) — restrained

- Per §4.2 as scoped on 2026-10-07: `text_primary` carries emphasis; `accent`/`focus` are state, not
  decoration. This is the **only** surface that stays white-dominant. The direction does not change.
- **First surface rebuilt 2026-10-08:** the scene browser (OPEN SCENE / IMPORT LEGACY MAP) is a framed
  panel whose colours are all `ui_app_theme_workbench_palette()` roles and whose selected row takes the
  shared focus perimeter in its shape-only form. The editor overlay's colour literals are retired; the
  selection no longer uses a hue of its own. See
  [`reviews/2026-10-08-editor-scene-browser.md`](reviews/2026-10-08-editor-scene-browser.md). The
  remaining editor surfaces are unchanged and share the same palette now.
- **The browser is a display surface, not a pane (2026-10-09).** The framed panel above was presented
  beside the authored menus and did not match them, so the browser was rebuilt on the menu's own form:
  the living-field material fills the frame through a `living_field` element with `extent = surface`,
  and title, entry count, rows and hint form one centred block over it, with the focused row taking the
  shared focus perimeter **with** its chromatic chase — the same treatment an authored focused button
  gets. It is classified as a **mode-selection display surface** (the family that contains the main,
  pause and settings menus), so §4.2's white-dominant restraint remains scoped to the editor's editing
  surfaces — its panes, footers, tooltips and status text — which are unchanged. The surface is centred
  on the frame while it is open, which is what keeps a block on a surface larger than the screen inside
  the visible band; the frame's footer copy is suppressed for it. See
  [`reviews/2026-10-09-editor-scene-browser-display-surface.md`](reviews/2026-10-09-editor-scene-browser-display-surface.md).

---

## 4. Chromatic rules

- **Semantic roles are authoritative and never decorative.** `warning`, `error`, `success`,
  `destructive` mean what they say (§2.1). Decorative chromatic channels use `accent`/`focus` or the
  additive decorative chromatic-material tokens (`ui_theme_material_palette()`, §3.4) — never a
  semantic hue and never a literal.
- **Literal RGB is diagnostic material, not a token.** The single isolated red/cyan comparison stays in
  the approved motion specimen (§2.1).
- **Black and white are the field; three primaries are the whole of the colour (locked 2026-10-09).** On
  a display surface the colour *is* the chromatic aberration: red `#FF2B2B`, green `#2BFF4F`, blue
  `#2B55FF` over a black `#05080A` surface, with `text_primary` white and `text_secondary` as the
  material. The decorative set's yellow, cyan and magenta are **not drawn** by a display surface.
- **"Saturated primary and secondary colours" (§1.1)** means the material must reach the palette's
  brightest, most separated hues at full cell occupancy — crisp separated light, not a dim wash.
- **Intermingled, not ordered.** A field is a neutral-dominant fabric carrying saturated traces; hue
  must intermix at the cell scale and must **never** be laid out as a spatial gradient or ramp, which
  reads as a literal rainbow rather than material (§1.1). The field's forms should be crisp
  rectangular patches that travel and fold as one coherent material, not flat stripes or rings. The
  `living_field` is the reference application.
- **Every colour derives from tokens** (§2, §3.1). No literals on any surface. If a material needs more
  hues than the 16 roles provide, that is an additive `ui_theme` decision (D-1), never an inline
  literal.

## 5. Motion vocabulary target

The accepted evaluator (`src/ui_animation.c`) offers nine bounded presets today: `pause_glitch`,
`center_out`, `perimeter_burst`, `local_glitch`, `edge_trace`, `chromatic_register`, `command_flash`,
`button_reassemble`, `panel_register`. §1 needs a **material-behaviour family** these do not cover:

| §1.3 behaviour | Meaning | Preset candidate |
|---|---|---|
| flow | traveling patterns | `material_flow` |
| ripple | travel and settle | `material_ripple` |
| churn | fold and recirculate | `material_churn` |
| goop / viscosity | gather, stretch, drag | `material_goop` |
| drip | accumulation and release | `material_drip` |
| field | a dense coordinated living surface | `living_field` |

Rules that carry over unchanged (§2, §2.1, §4.4): one shared evaluator for normal run and workbench;
explicit elapsed time; stable IDs; deterministic paths; exact endpoints; no hidden randomness or
frame-count progression; reduced motion immediate and non-spatial. Each new preset is added through
the five-step protocol in
[`UI_WORKBENCH_ASSET_REFERENCE.md`](UI_WORKBENCH_ASSET_REFERENCE.md) ("Adding one new preset later").

## 6. Stable-versus-alive matrix

| May move (material / display) | Must not move (controls / semantics) |
|---|---|
| decorative field behind menus | button labels, focus perimeter around the focused button |
| coloured traces, traveling patterns | focus/selection background and state |
| material textures in world / previews | hit targets, interaction eligibility |
| HUD frame/separator activity | HUD numeric readouts and labels |
| reassembly on context enter/exit | semantic status colour meaning |

---

## 7. Frame budget and performance envelope

- **Grid:** 260 × 160 = **41,600 cells** per frame; cells are 8 × 8; `target_fps = 120` (8.33 ms/frame).
- **Surface render budget:** **6 ms** — the unchanged P1–P3 budget
  ([`reviews/2026-09-15-v1-0-p1-surface-performance-reproduction.md`](reviews/2026-09-15-v1-0-p1-surface-performance-reproduction.md)
  and successors). Dense chromatic animation must fit inside it.
- **Consequence.** "Dense, edge-to-edge, every cell, every frame" is affordable only if update cost is
  bounded. Prefer: dirty-cell / dirty-region updates; bounded *active regions* (the whole grid need not
  change each frame); glyph + foreground-colour changes only; no per-frame full-surface recomposition.
- **Any slice must ship with a benchmark** proving it stays under budget, using the existing
  `benchmark-*` harness pattern (§2).

## 8. Non-negotiables (unchanged by this target)

1. One shared evaluator; no workbench-only or surface-only renderer (§2, §5.2.9).
2. All colour/spacing from `ui_theme` tokens (§2, §3); no literals.
3. Deterministic, explicit time; stable IDs; exact endpoints; no frame-count progression.
4. Reduced motion immediate and non-spatial (§2.1, §4.4).
5. Nothing in motion displaces, obscures, or delays a control, focus marker, or hit target (§4.4).
6. Semantic colours stay semantic (§2.1).
7. Faithful preview: the workbench preview matches normal run (§4.1).

## 9. Current capability versus target

| Need (§1) | Have today | Gap |
|---|---|---|
| Dense coordinated field | `living_field` in all four authored menus, filling the display (`extent=surface`, 2026-10-08) | roll out to remaining surfaces |
| Chromatic material | the three additive primaries (red, green, blue) drawn as chromatic aberration by all four menus, over a black surface with white text | roll the material out to the HUD and the world |
| Material behaviours | none | no flow/ripple/churn/goop/drip |
| Menus alive behind controls | all four menus carry a field, fill the viewport, and recess the frame behind them | keep; bounded (floating-panel) layouts are authorable but unused |
| World material behaviour | static 4-glyph materials | no behaviour per material |
| Controls stable | yes | keep |

## 10. Open decisions (owner input required)

- **D-1 — Chromatic material set.** **Resolved 2026-10-07:** an additive six-colour decorative
  chromatic-material set (`ui_theme_material_palette()`, §3.4) was added, and the `living_field` now
  draws it as sparse saturated specks over a white-dominant flowing fabric (reworked 2026-10-07; see
  the review record "Fabric rework"). (Was: do the 16 §3.1 roles suffice as the full §1 "primary and
  secondary" material, or do we add an additive set for decorative fields?)
  **Narrowed and locked 2026-10-09:** the display surfaces draw the **three additive primaries only** —
  red, green, blue — as chromatic aberration; the other three colours of the set are
  editor-visualization tokens and are not drawn on a display surface. The identity that locks this is
  recorded in
  [`UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md`](UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md) ("Change of
  direction recorded 2026-10-09 — the visual identity, locked").
- **D-2 — Motion budget under richness.** **Resolved 2026-10-07:** an additive
  `UI_THEME_MOTION_AMBIENT` role (1800 ms) was added, and `while_visible` now maps to it, so a
  continuously-flowing field has an in-vocabulary period. (Was: the 80/160/120/120 ms role set was
  sized for bounded reassembly; a travelling field has no natural "end", so decide whether fields are
  `while_visible`, loop under the relationship role, or get a new named role.)
- **D-3 — World animation budget.** Is animated world material affordable inside the 6 ms surface
  budget, or does it need its own measured sub-budget? (Affects §3.3, §7.)
- **D-4 — First slice.** The bounded first slice is chosen from the options presented to the owner
  (pause field / main-menu field / world material / preset-only).