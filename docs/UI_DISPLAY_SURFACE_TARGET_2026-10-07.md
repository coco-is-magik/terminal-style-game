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

- **Material behaviour.** A coherent decorative field fills the surface behind the controls: shapes
  travel, patterns flow, and coloured traces separate and recombine while the larger forms stay
  coherent (§1.1). Context enter/exit is glyph reassembly — precision → controlled disorder →
  precision (§1.2). Today only `pause_menu` carries a decorative unit (`animation_pause_glitch`); the
  other menus use `transition=none` and have no field.
- **Chromatic treatment.** Saturated primary/secondary material (see §4), not a monochrome wash.
- **Stays stable.** Button labels, the `>`/`<` focus markers, focus/selection state, hit targets, and
  interaction eligibility never move and are never obscured (§2.1, §4.4). Reduced motion draws no
  field and no displacement.

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
  decoration. This is the **only** surface that stays white-dominant. Nothing here changes.

---

## 4. Chromatic rules

- **Semantic roles are authoritative and never decorative.** `warning`, `error`, `success`,
  `destructive` mean what they say (§2.1). Decorative chromatic channels use `accent` (#67F5C2) and
  `focus` (#A8FFE1), reaching the secondary hues (`warning`/`success`/`destructive`) where a material
  needs a second colour (see D-1).
- **Literal RGB is diagnostic material, not a token.** The single isolated red/cyan comparison stays in
  the approved motion specimen (§2.1).
- **"Saturated primary and secondary colours" (§1.1)** means the material must reach the palette's
  brightest, most separated hues at full cell occupancy — crisp separated light, not a dim wash.
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
| decorative field behind menus | button labels, `>`/`<` focus markers |
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
| Dense coordinated field | `living_field` shipped in the pause context (2026-10-07) | roll out to other surfaces |
| Chromatic material | accent/focus used on one surface | chromatic roles barely used; no material set |
| Material behaviours | none | no flow/ripple/churn/goop/drip |
| Menus alive behind controls | only `pause_menu` | main/settings/confirm static, `transition=none` |
| World material behaviour | static 4-glyph materials | no behaviour per material |
| Controls stable | yes | keep |

## 10. Open decisions (owner input required)

- **D-1 — Chromatic material set.** Do the 16 §3.1 roles suffice as the full §1 "primary and secondary"
  material, or do we add an additive `ui_theme` chromatic-material set for decorative fields? (Affects
  §4 and every surface.)
- **D-2 — Motion budget under richness.** **Resolved 2026-10-07:** an additive
  `UI_THEME_MOTION_AMBIENT` role (1800 ms) was added, and `while_visible` now maps to it, so a
  continuously-flowing field has an in-vocabulary period. (Was: the 80/160/120/120 ms role set was
  sized for bounded reassembly; a travelling field has no natural "end", so decide whether fields are
  `while_visible`, loop under the relationship role, or get a new named role.)
- **D-3 — World animation budget.** Is animated world material affordable inside the 6 ms surface
  budget, or does it need its own measured sub-budget? (Affects §3.3, §7.)
- **D-4 — First slice.** The bounded first slice is chosen from the options presented to the owner
  (pause field / main-menu field / world material / preset-only).