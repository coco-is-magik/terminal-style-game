# UI Look-and-Feel Reference of Record

```
################################################################################
#                                                                              #
#   THIS DOCUMENT IS BINDING. IT IS THE GUIDING STAR FOR EVERY COLOUR, EVERY   #
#   ANIMATION, EVERY TRANSITION, EVERY MOTION EFFECT, AND EVERY LOOK-AND-FEEL  #
#   DECISION IN THIS PROJECT.                                                  #
#                                                                              #
#   RETURN TO IT EVERY SINGLE TIME THE SUBJECT COMES UP.                       #
#   IT IS NOT BACKGROUND READING. IT IS A RULE OF ENGAGEMENT.                  #
#                                                                              #
################################################################################
```

## Why this document exists

Three days of development time were already lost to drifting away from this direction.
A UI editor interface was built that used hardcoded colours instead of the accepted palette,
had no scale/compositor integration, overwrote its own footer, drew selection markers inside
authored content, and ignored the accepted editor geometry — while its tests passed and its
records claimed the interface was implemented.

That happened because the reference material existed but was not treated as mandatory. This
document exists so that cannot happen again.

## Status and authority

This is a **living foundation document**, not a dated plan or a phase record. It is not
superseded by increments, phases, or reviews. It is superseded only by a deliberate, explicitly
recorded change of direction approved by the project owner.

Related authority:

- [`APPLICATION_UI_EDITOR_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-09-18.md`](APPLICATION_UI_EDITOR_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-09-18.md)
  — the active Step 1 plan. It cites this document and cannot override it.
- [`UI_DESIGN_AND_TEST_STANDARDS.md`](UI_DESIGN_AND_TEST_STANDARDS.md) — enforceable UI rules.
- [`UI_WORKBENCH_ASSET_REFERENCE.md`](UI_WORKBENCH_ASSET_REFERENCE.md) — application-UI asset and
  workbench control reference.

## Change of direction recorded 2026-10-07 — §1 is the target for the whole product

The project owner has authorized a deliberate change of this document. §2 and §4 had drifted into
treating the diagnostic specimens (`make ui-theme-demo`, `make ui-motion-demo`) as the thing the
product must match, and into reading §4.2's "white-dominant" rule as applying to the shipped menus.
The direction now in force:

1. **The primary references in §1 are the target — for the whole product, not only the app UI.**
   The authored menus, the in-game HUD, the game world, and the editor visualizations are all
   governed by §1.1, §1.2, and §1.3.
2. **The approved specimens are diagnostics, not the target.** `ui-theme-demo` and `ui-motion-demo`
   remain valid instruments for exercising the palette and the motion vocabulary. They are no longer
   the thing the product "must look and move like." Their sparseness is a property of a diagnostic
   probe, not of the product.
3. **Menus and display surfaces are dense, chromatic, and alive.** The "white-dominant, selectively
   coloured" rule in §4.2 is **scoped to the editor interface only** (the tool's own panes, footers,
   tooltips, and status text). It never applied, and does not apply, to authored menus, the HUD, the
   world, or visualizations — which §4.2 already exempted as "the preview, the live tool feedback,
   and the visualizations."
4. **"No glitch" means no independent flicker, not no richness.** §2.1, §4.4, and §5.2.5 continue to
   forbid *independent flicker* and *filters laid over the interface*. They do **not** forbid
   **coordinated, coherent, dense chromatic activity**, which §1.1 explicitly wants ("shapes can
   travel, patterns can flow, and colored traces can separate and recombine, but the larger forms
   should remain coherent"). The test is **coordinated pattern versus independent flicker**, not
   **sparse versus dense**.
5. **Both halves of §1.1 remain binding.** "Keep the controls dependable. Let the display feel
   alive." Richness goes into decorative material and display surfaces; controls, focus markers, hit
   targets, and semantic status stay crisp, stable, and immediate (§2.1, §4.4).

Nothing here relaxes determinism, explicit time, stable IDs, exact endpoints, reduced-motion
immediacy and non-spatiality, the palette/geometry tokens (§3), or the single shared evaluator (§2).
The concrete per-surface obligations are in
[`UI_DISPLAY_SURFACE_TARGET_2026-10-07.md`](UI_DISPLAY_SURFACE_TARGET_2026-10-07.md).

## Change of direction recorded 2026-10-09 — the backdrop fabric and the state transitions

The project owner directed that the backdrop motion and the state transitions be reworked:

> For the motion of the backdrop, lets try a solid field that is rippling gently. A smooth wave that
> goes from the bottom right corner up to the top left as the main centerpiece motion on a loop. Our
> rgb abberation follows in the wake of the wave and when it hits the menu elements. The 'wave' should
> be an effect on the 'fabric' of the backdrop. We can take inspiration from the way a flag or gentle
> water surface might move.
>
> For the transition, all the characters should surge in and cover the menu like a receding tide, then
> flow back out to their original place revealing the new menu elements. If we are going from
> menu -> game instead, they should instead flow out to grid positions and flicker through characters
> before settling on revealing the actual frame.
>
> The UI should feel flowy and fluid, and transitions should feel like a hologram changing smoothly
> between states with our RGB chromatic abberation to accent it.

This is authorized under §1.1 and §1.3 — "ripples should travel and settle", "the marks should be
simple; the behavior should carry the richness" — and is recorded as follows.

1. **The backdrop is a solid fabric, not a weave of patches.** `living_field` draws every blank cell
   in its region from the density ramp, so the material is continuous; its structure comes from that
   ramp, a static per-cell grain and a bottom-heavy density curve, never from gaps between shapes.
2. **One travelling wave is the centrepiece.** A main wave and a finer ripple ride the diagonal from
   the surface's bottom-right corner to its top-left at different wavelengths, and their beat is the
   flag-like lift and settle. Both advance an **integer** number of wavelengths per `AMBIENT` loop, so
   the material loops seamlessly on any surface size and no geometry is hidden in a constant.
   **The rate is a setting, the material is not:** the ambient loop defaults to the token value of
   1800 ms and is retuned by `backdrop_period_ms` in `config.ini` (default 2600 ms — the owner found
   the material a little fast, recorded 2026-10-09). Because the wave advances whole wavelengths per
   loop, the same phase composes the same surface at any period and one loop always lands back on it,
   so the speed can be tried at different values without the look moving: the wavelengths, the
   direction, the density curve, the grain and the chromatic wake are untouched, and no motion role or
   other duration changed.
3. **The aberration follows the wave.** The three additive primaries ride the **wake** behind every
   main crest as a sparse speckle, in addition to the existing directional fringe where the fabric
   meets a control or the region border. Colour stays decoration only: never a fill, never a secondary
   colour, never a spatial hue ramp (§3.4).
4. **State changes become a tide, and a menu→world change a settle.** Between menus the material
   surges in from the bottom-right corner and covers the surface, and the incoming surface drains the
   same material back out revealing its elements; going from a menu into the world the material flows
   out to grid positions, cycles through glyphs where the wave crosses it, and thins away so the live
   frame underneath is uncovered. Both are accented by the same chromatic wake, and **which transition
   plays is a fact about the destination** — the layout being left cannot know whether it is handing
   over to another surface or to the frame. The per-surface obligations, recorded as each was built
   (`docs/reviews/2026-10-09-state-transitions.md`):
   - **The windows come from the theme.** An exit runs `MAJOR_EXIT` and an enter `MAJOR_ENTER`; the
     incoming reveal begins on the frame the outgoing cover completes, so the two halves are one
     continuous motion with no frame where the surface is neither. No transition duration is written
     down anywhere as a literal.
   - **A transition is driven by its own window, never by the ambient clock.** The tide's wave advances
     over the cover it is crossing — an exit ends and the reveal that follows begins on the same cover,
     so they share one phase and the material never jumps at the handover — and the reveal retraces the
     cover exactly: the characters leave the way they arrived (owner direction 2026-10-09, after a first
     attempt driven by the ambient clock read as "a straight edge sweeping a still sheet ... more like a
     stutter between things"). Consequently the backdrop's speed is a setting
     (`backdrop_period_ms`) that no state change follows.
   - **A transition is the fabric, not a stripe.** The flood's edge rides the backdrop's anti-diagonal
     fold, so it arrives as a travelling swell rather than a straight line, and its body is the same
     woven sheet as the backdrop: the wave's band *plus* the per-cell weave, mapped into the top of the
     ramp, where a constant lift would clamp every cell to one glyph and read as a flat stripe.
   - **The cover is a sanction, and a narrow one.** `tide_cover`, `tide_reveal` and `settle`
     declare `covers_content` in the effect registry, which is what allows them to pass over authored
     content; the evaluator withholds the restore of authored cells only while such a primitive is
     *currently reporting a cover*, which ends with its window. Every other primitive keeps §4.4's
     guarantee that motion never obscures a control, and the props hold at every settled moment
     (`test_lifecycle_keeps_authored_cells_stable`).
   - **Colour stays decoration.** The tide's advancing edge and the settle's crest carry the same
     directional prismatic fringe and speckle as the backdrop's wake: never a fill, never a spatial hue
     ramp (§3.4).
   - **Nothing is a literal and nothing is hidden in a constant.** The materials are driven by the
     window they occupy; the backdrop's wave is advanced over its `AMBIENT` loop when it is at rest and
     over the transition window when it is a transient.
   - **Reduced motion is immediacy.** Both transitions draw nothing at all, which is the recorded
     no-motion response: the new state is simply there.

Nothing here relaxes determinism, explicit time, stable IDs, exact endpoints, reduced-motion immediacy
and non-spatiality, the palette/geometry tokens (§3), the 6 ms surface budget, or the single shared
evaluator (§2). Reduced motion still draws no fabric and no displacement.

## Change of direction recorded 2026-10-09 — the visual identity, locked

> Black, White, Red, Green, Blue and flowy, smooth motion.

That is the identity, in the owner's words, and it is **locked** here. It is not a starting point to be
re-derived and not a palette to be extended in passing: every display surface — the authored menus, the
HUD, the transitions, the world — is measured against it.

- **Black and white first.** The surface is `canvas` `#05080A`: black, and the space behind everything.
  Text is `text_primary` `#F2F7F8`: white. The shared material is `text_secondary` `#A8B4B8`, a neutral
  light grey, so the fabric reads as *light on black* rather than as a wash. A selected control is the
  inverse — `text_primary` behind `canvas`-coloured text. Emphasis is carried by brightness and by
  movement, never by hue.
- **The only colour is the three additive primaries.** `ui_theme_material_palette()` indices 0, 2 and 4:
  red `#FF2B2B`, green `#2BFF4F`, blue `#2B55FF`. They appear **only** as the chromatic aberration of
  the material — the speckle in the wake of a crest, the directional fringe where the fabric meets a
  control, the region border, or a gap (red left, blue right, green horizontal), the tide's advancing
  edge, the settle's crest. Never a fill, never a background, never a semantic status colour, never a
  spatial hue ramp, never a secondary hue (§3.4). The material set's other three colours — yellow
  `#FFE02B`, cyan `#2BE8FF`, magenta `#FF2BD6` — are **not part of this identity**: they exist as
  tokens and no display surface draws them.
- **Flowy, smooth motion.** One material, moving as one thing: waves travelling across the fabric, and
  state changes that surge and retrace as a single continuous motion instead of a cut between two
  pictures (§1.1–§1.3, and the transitions recorded above). Coordinated, never independent: no cuts, no
  flicker, no particles, no trails, no filters laid over the interface (§4.4).

**What this locks and what it exposes.** This note records the identity; it changes no code, and it is
the reference for anything that follows. Two things in the shipped tokens sit outside it and are
recorded here as **open reconciliations, not precedent**: the editor-interface roles carry hues the
identity does not name (`accent` `#67F5C2`, `focus` `#A8FFE1`, `warning` `#FFD166`, `success` `#71F79F`,
`error` `#FF6B7A`, `destructive` `#FF8894`, `selection_background` `#123D32`, `editor_selection`
`#70B7FF`); and **the menus' focus perimeter draws `accent`/`focus`**, so a display surface currently
shows a hue outside the identity. Retiring those hues or re-deriving them from black, white and the
primaries is a decision to take deliberately, under the same record discipline as everything else here —
not a silent edit. `test-ui-theme`'s contrast floors continue to apply to whatever survives.

## Rule of engagement

1. Before beginning any phase, and before approving any change that touches colour, motion,
   animation, transition, or interface feel, **re-read this document and the files it names.**
2. Every such phase record must open by citing the specific clause it serves, by number.
3. A change that cannot cite a clause in this document is **out of direction and rejected by
   default.** "It looked fine" is not a citation.
4. Where this document and a dated plan disagree about look and feel, **this document wins.**

Editor-interface colour, motion vocabulary, animation behaviour, and transition behaviour are
**directed decisions, not open design problems.** They are not to be re-invented, re-derived, or
"improved" in passing while doing something else.

## Terminology — plain words only

This document names things literally. Earlier revisions used "chrome", which appears in none of the
direction sources and reads as browser jargon.

| Term used here | Means |
|---|---|
| **authored UI** | the `assets/ui_elements/` and `assets/ui_layouts/` content a user is editing, and its faithful preview (§4.1) |
| **editor interface** | everything the editor itself draws that is not authored content: pane bodies and borders, footer rows, tooltips, status text, selection markers |
| **editor surface** | the 260×160 cell area the editor composes into; the token names remain `editor_baseline_columns` / `editor_baseline_rows` |
| **application UI scale** | the persisted `ui_preferences` setting used by normal run and by the authored preview, 100/125/150/200 |
| **workbench UI scale** | the independent session-local readability setting for the editor interface, 100/125/150/200 |
| **headless frame renderer** | `src/ui_workbench_frame.c`, which composes a frame with explicit time and no display, so it can be checksummed and reviewed |
| **matches normal run** | the preview renders the same cells normal run would (this replaces the word "parity") |
| **acceptance check** | one of the named checks G1–G10 in the Step 1 plan |

Code identifiers still use older names — `src/ui_workbench_chrome.h/.c`, `ui_workbench_chrome_*()`,
`UI_WORKBENCH_CHROME_ROLE_*`, `test-ui-workbench-chrome`. Where this document or a plan names a file,
target, or function, it uses the real identifier, because a document must never name something that
does not exist. Identifier renaming is deferred, tracked separately, and is not a behaviour change.

---

# 1. Primary references — the look and feel we are aiming at

These three directories under [`inspiration_and_notes/`](inspiration_and_notes/) are the source
material. Each contains a `note.txt` (the directive) with `.gif` and `.mp4` companions (the
moving reference). **Read the note text every time. Watch the media when the decision is about
motion, transition, or texture.**

## 1.1 `docs/inspiration_and_notes/light_and_motion/`

Media: `HR8flLgaoAAuEyO.gif`, `HR8flLgaoAAuEyO.mp4`

Governs: **palette, light, motion, and the overall display character.**

> Design a terminal-inspired game editor that feels rendered by a distinctive fictional display
> system, rather than decorated with retro effects. Build its graphic language from small square
> light cells, visible black gaps, clear rectangular forms, and saturated primary and secondary
> colors. Favor crisp, separated light over diffuse neon glow.
>
> The signature behavior is fluid motion expressed through discrete cells. Shapes can travel,
> patterns can flow, and colored traces can separate and recombine, but the larger forms should
> remain coherent. Animate coordinated patterns rather than independent flicker. The effect should
> feel native to how the machine draws, not like a glitch filter placed over the interface.
>
> Use a white-dominant, selectively colored treatment for the everyday workbench, and give richer
> chromatic activity substantial roles in live tools and visualizations. Preserve clear typography,
> precise controls, immediate responses, and faithful previews of the user's work. The atmosphere
> should be industrial, technical, and retrofuturist, with enough ongoing activity to make the
> system feel alive.
>
> Keep the controls dependable. Let the display feel alive.

Extracted obligations for our interface:

- The display must feel **rendered by a machine**, not decorated with retro effects.
- **Crisp, separated light** beats diffuse neon glow. Small square cells, visible black gaps,
  clear rectangles, saturated primaries/secondaries.
- Motion is **fluid through discrete cells**: coordinated patterns, never independent flicker,
  never a glitch filter laid over the interface.
- **The everyday workbench is white-dominant and selectively coloured.** Rich chromatic activity
  belongs to live tools and visualizations.
- Clear typography, precise controls, immediate responses, faithful previews.
- "Keep the controls dependable. Let the display feel alive." — both halves are required.

## 1.2 `docs/inspiration_and_notes/transitions/`

Media: `HDkbBr4bEAcKnjL.gif`, `HDkbBr4bEAcKnjL.mp4`, `HDnqcETXUAAXr1L.gif`,
`HDnqcETXUAAXr1L.mp4`, `HDpoQGgWsAA8cgI.gif`, `HDpoQGgWsAA8cgI.mp4`

Governs: **every state change, context enter/exit, and focus/activation transition.**

> Use glyph-based reassembly transitions: state changes in which the visible marks of one
> configuration redistribute and resolve into the next.
>
> Treat the interface's graphics as arrangements of a shared visual material, rather than finished
> images that are simply exchanged. For changes of form, let marks establish new groupings and
> boundaries. For changes of position, let the formation relocate through coordinated movement of
> its parts rather than only through rigid translation.
>
> Favor a movement from precision, through controlled disorder, back to precision. Allow edges to
> loosen, clusters to separate, and individual marks to take varied paths within a coherent overall
> transformation. The middle can become briefly abstract, but departure and arrival should remain
> visibly related.
>
> Give transitions a sense of release, redistribution, and resolution. These phases may overlap,
> with parts of the destination assembling while parts of the source are still moving. End in a
> clean, stable arrangement. Do not substitute unrelated particle bursts, random flicker, or
> decorative trails for the actual transformation.
>
> Express this behavior through the ASCII grid. Keep glyphs crisp and simple, and use coordinated
> changes in cell occupancy, grouping, and timing to carry the motion. Scale the amount of
> reassembly to the significance of the interaction, preserving the correspondence of meaningful
> objects and the stability of active controls.
>
> The state changes, but the visual material feels continuous.

Extracted obligations:

- Transitions are **reassembly of shared visual material**, not one image replacing another.
- Shape: **precision → controlled disorder → precision.** Departure and arrival must remain
  visibly related; the middle may go briefly abstract.
- Phases may **overlap**; the end must be **clean and stable**.
- **Forbidden:** unrelated particle bursts, random flicker, decorative trails used *instead of*
  the actual transformation.
- Reassembly is **scaled to the significance** of the interaction.
- Preserve the correspondence of meaningful objects and the **stability of active controls**.
- "The state changes, but the visual material feels continuous."

## 1.3 `docs/inspiration_and_notes/more_motion/`

Media: `G84iskPXIAEhgP2.gif`, `G84iskPXIAEhgP2.mp4`, `G9_vp58WYAEv6nQ.gif`,
`G9_vp58WYAEv6nQ.mp4`

Governs: **texture and behaviour language for animated material and editor visualizations.**

> Use behavior-first ASCII textures: a small vocabulary of fixed glyphs whose coordinated changes
> suggest material properties, surface activity, and physical response.
>
> Prioritize the behavior of a substance over a literal illustration of it. Represent flowing
> through traveling patterns, viscosity through dragging and connected deformation, dripping through
> accumulation and release, and wetness through changing highlight structures. Let the viewer
> encounter the material through what it does.
>
> Build richness from relationships between simple marks. Use spacing, density, direction,
> continuity, and negative space to organize the texture. Animate coherent regions and disturbances
> rather than independent character flicker. Keep the glyphs crisp and constrained to the grid while
> allowing the larger patterns they describe to evolve fluidly.
>
> Give each material a distinct response. Goop should gather and stretch; ripples should travel and
> settle; churn should fold and recirculate. Do not apply one generic animated-noise treatment to
> every substance. Favor a few legible behaviors over an accumulation of decorative detail.
>
> Apply this language to game-world materials, effects, and editor visualizations without requiring
> the surrounding controls to behave like those materials. The goal is not elaborate ASCII
> illustration or a conventional image passed through an ASCII filter. It is an expressive visual
> system designed around the capabilities of characters from the outset.
>
> The marks should be simple; the behavior should carry the richness.

Extracted obligations:

- **Behaviour over illustration.** The viewer learns a material from what it does.
- **Coherent regions, not independent character flicker.**
- A small vocabulary of fixed glyphs, crisp and grid-constrained, while the larger pattern evolves
  fluidly.
- Distinct responses per subject; **no single generic animated-noise treatment** for everything.
- A few legible behaviours beat accumulated decorative detail.
- Applies to **editor visualizations** as well, **without** making the surrounding controls behave
  like the material. The controls stay controls.

---

# 2. Existing foundation — also a respected direction

The repository already contains an accepted implementation of this direction. These are **solid
directions to respect and extend. They must not be forked, bypassed, or re-implemented beside.**

| Foundation | Path | Rule |
|---|---|---|
| Accepted provisional palette + geometry tokens | [`../src/ui_theme.h`](../src/ui_theme.h) / [`../src/ui_theme.c`](../src/ui_theme.c) — `ui_theme_provisional_tokens()` | All editor-interface colour and spacing derives from here. **No literal colours in the editor interface.** |
| Application palette adapter | [`../src/ui_app_theme_adapter.h`](../src/ui_app_theme_adapter.h) / `.c` | The only bridge from tokens to the editor interface. Extend it additively for the roles the editor needs. |
| Accepted motion vocabulary + timings | [`../src/ui_motion.h`](../src/ui_motion.h) / `.c`, plus the `ui_theme` motion roles | Bounded vocabulary only. Deterministic, explicit time, stable IDs, exact endpoints. |
| Compositor, canvas, scale policy | [`../src/ui_compositor.h`](../src/ui_compositor.h), [`../src/ui_canvas.h`](../src/ui_canvas.h) | The preview **and** the editor interface both scale with the UI scale setting (100/125/150/200). Magnifying must never push the editor interface or a control off the visible surface, or hide it behind the preview. |
| Shared UI scale preferences | [`../src/ui_preferences.h`](../src/ui_preferences.h) / `.c` | `default_user.ini` / `user.ini`, 100/125/150/200, same precedence and persistence as normal run. |
| Shared animation evaluator | [`../src/ui_animation.h`](../src/ui_animation.h) / `.c` | **One evaluator for normal run and editor. A workbench-only renderer is forbidden.** |
| Application UI model + atomic persistence | [`../src/ui_ele.h`](../src/ui_ele.h), [`../src/ui_workbench_store.h`](../src/ui_workbench_store.h), `platform_fs` | Validated same-directory temp file, flush, sync, atomic replace. Failure preserves both the file and the in-memory edit. |
| Accepted diagnostic specimens | `make ui-theme-demo`, `make ui-motion-demo` ([`reviews/2026-09-16-v1-1-full-palette-demo.md`](reviews/2026-09-16-v1-1-full-palette-demo.md), [`reviews/2026-09-16-v1-1-ui-motion-demo.md`](reviews/2026-09-16-v1-1-ui-motion-demo.md)) | **Manually approved** palette and D6 motion references — **diagnostics, not the target** (see the 2026-10-07 change of direction above). Use them to exercise the palette and the motion vocabulary; do not treat their sparseness as the product's look. |
| Live application-UI editor | `make ui-workbench` ([`reviews/2026-09-17-ui-workbench-i1.md`](reviews/2026-09-17-ui-workbench-i1.md)) | The proven host pattern: adapter palette, offscreen canvas, one composited layer, dedicated footer rows. Scale treatment is corrected in §4.7 — the I1 host left the editor interface at a fixed 100%, which is superseded. |
| Determinism harness pattern | `tests/benchmark_*.c`, `benchmark-*` / `stability-*` make targets | Determinism is proven by checksums and snapshots, never by prose. Reuse this pattern for frame gates. |
| Visual direction source | [`inspiration_and_notes/`](inspiration_and_notes/) | The primary references in section 1. |

## 2.1 The accepted motion boundary (from the approved motion specimen)

These boundaries were manually approved on 2026-09-16 and remain binding:

**Scope note (2026-10-07).** They are the *floor* — determinism, non-flicker, and control stability —
not a ceiling on richness. Per the change of direction above, dense *coordinated* chromatic activity
is expected in menus, the HUD, the world, and visualizations. The bullets below describe what must
stay true *while* it moves:

- Controlled display registration and glyph reassembly — **not** persistent or global glitch.
- **Stable** text, focus markers, controls, hit targets, semantic state, and interaction
  eligibility **while nearby decorative material moves.**
- Semantic warning/error/success/destructive colours remain authoritative and are **never**
  decorative RGB trails.
- Palette accent/focus traces are compared against exactly one isolated literal red/cyan sample;
  literal RGB is **diagnostic effect material, not a semantic token.**
- Decorative chromatic material (the saturated primary/secondary set in §3.4) is an approved,
  tokenised decorative channel: **never** a semantic status colour and **never** a literal.
  (Added 2026-10-07, D-1.)
- Explicit elapsed time, stable IDs, deterministic paths, exact endpoints, **no hidden randomness
  or frame-count progression.**
- **Reduced motion is immediate and non-spatial** — no chromatic displacement, no delayed
  interaction.
- Cell/glyph/layer composition only.

---

# 3. Accepted values — do not re-derive

These are the manually approved values currently in `src/ui_theme.c`. They are recorded here so
they are visible when design decisions are made. **`src/ui_theme.c` remains the single source of
truth**; this table is the reference copy.

## 3.1 Palette roles

**Scope (2026-10-09).** These are the *editor interface* roles. The **display surfaces** — authored
menus, the HUD, the transitions and the world — follow the locked identity instead: black `canvas`,
white `text_primary`, neutral `text_secondary`, and the three additive primaries of §3.4 as aberration,
and nothing else (see the change of direction recorded 2026-10-09). The roles below that name a hue the
identity does not — `accent`, `focus`, `warning`, `success`, `error`, `destructive`,
`selection_background`, `editor_selection` — are editor-interface state, and are the open
reconciliation recorded there.

| Role | Value | Intended use |
|---|---|---|
| `canvas` | `#05080A` | Editor-interface backdrop; the space behind everything |
| `panel` | `#0D1418` | Pane bodies |
| `elevated` | `#162126` | Focused pane, selected row, modal surface |
| `text_primary` | `#F2F7F8` | Pane titles, active values |
| `text_secondary` | `#A8B4B8` | Labels, hints, secondary prose |
| `border` | `#64757C` | Pane borders, dividers, selection markers |
| `accent` | `#67F5C2` | Active context, selection accent |
| `focus` | `#A8FFE1` | Focused control |
| `selection_background` | `#123D32` | Selected row background |
| `disabled_text` | `#8D999D` | Unavailable action text |
| `disabled_background` | `#161D20` | Unavailable action background |
| `warning` | `#FFD166` | Warning status |
| `error` | `#FF6B7A` | Error status |
| `success` | `#71F79F` | Success status |
| `destructive` | `#FF8894` | Destructive action/confirmation |

Contrast floors enforced by `test-ui-theme`: text pairs **≥ 4.5**, non-text pairs **≥ 3.0**.

## 3.2 Geometry tokens

| Token | Value | Meaning |
|---|---|---|
| `base_space_cells` | 1 | Minimum unit of space |
| `ordinary_target_cells` | 3 | Ordinary interactive target size |
| `horizontal_label_padding_cells` | 2 | Padding beside labels |
| `panel_inset_cells` | 2 | Content inset inside a panel |
| `related_item_gap_cells` | 1 | Gap inside a related group |
| `group_gap_cells` | 2 | Gap between groups |
| `major_section_gap_cells` | 3 | Gap between major sections |
| `border_cells` | 1 | Border thickness |
| `editor_baseline_columns` | **260** | **The editor surface width** |
| `editor_baseline_rows` | **160** | **The editor surface height** |
| `authored_minimum_columns` | 40 | Minimum authored UI width |
| `authored_minimum_rows` | 15 | Minimum authored UI height |
| `ordinary_major_context_limit` | 2 | Ordinary major-context limit |

`editor_baseline_columns`/`rows` matching the shipping `config.ini` `grid_width`/`grid_height`
(260×160) is intentional: **the editor surface is designed for the real display grid, not for a
small logical panel.**

## 3.3 Motion roles (D6)

| Role | Duration | Use |
|---|---|---|
| `UI_THEME_MOTION_IMMEDIATE` | 0 ms | Reduced-motion and inherently instant changes |
| `UI_THEME_MOTION_FEEDBACK` | 80 ms | Focus move, small feedback |
| `UI_THEME_MOTION_MAJOR_ENTER` | 160 ms | Major context enter |
| `UI_THEME_MOTION_MAJOR_EXIT` | 120 ms | Major context exit |
| `UI_THEME_MOTION_RELATIONSHIP` | 120 ms | Relationship cues |
| `UI_THEME_MOTION_AMBIENT` | 1800 ms | Ambient looping material field (`living_field`); added 2026-10-07 |

**Ambient role (2026-10-07).** `UI_THEME_MOTION_AMBIENT` was added under the change of direction
above so a continuously-flowing material field has an in-vocabulary period rather than a hidden
constant. A `while_visible` animation unit now maps to this role. The 80/160/120/120 ms transition
roles are unchanged.

**Backdrop speed (2026-10-09).** The token above stays 1800 ms. The loop the backdrop actually advances
over is `backdrop_period_ms` in `config.ini` (default **2600 ms**, range 400..60000), applied once at
startup through `ui_theme_set_ambient_period_ms()` so the material's rate can be tried without a
recompile. It is the backdrop's speed and nothing else: because the wave advances a whole number of
wavelengths per loop, phase 0 is the same surface at any period. **A state change no longer follows it
at all** — a transition is driven by its own window (see the transitions recorded 2026-10-09).

## 3.4 Decorative chromatic material (D-1)

Added 2026-10-07 under the change of direction above. A six-colour, **decorative-only** material set
for fields and material textures, per §1.1's "saturated primary and secondary colors." It is exposed
by `ui_theme_material_palette()` and is **never** a semantic status colour and **never** a literal.
`src/ui_theme.c` remains the single source of truth; this table is the reference copy.

| Index | Name | Value | Role |
|---|---|---|---|
| 0 | red | `#FF2B2B` | saturated primary |
| 1 | yellow | `#FFE02B` | saturated secondary |
| 2 | green | `#2BFF4F` | saturated primary |
| 3 | cyan | `#2BE8FF` | saturated secondary |
| 4 | blue | `#2B55FF` | saturated primary |
| 5 | magenta | `#FF2BD6` | saturated secondary |

All six are opaque and meet the non-text contrast floor (≥ 3.0) against `canvas` (`test-ui-theme`).

**Application (the `living_field`).** A field is a **solid fabric**: a neutral sheet of light cells
(`text_secondary`) drawn in every blank cell of its region, whose glyph density is carried by one
**travelling wave** — a main wave and a finer ripple riding the diagonal from the surface's
bottom-right corner to its top-left — plus a slow anti-diagonal fold and a static per-cell grain, so
the whole sheet lifts and settles the way a flag or a water surface does (change of direction recorded
2026-10-09). The density curve is bottom-heavy, so the fabric stays dim and quiet between crests and
the wave reads as *light moving through the material*. Saturated colour is **only a
chromatic-aberration fringe**: it rides the **wake** behind every main crest, and the *edges* where the
fabric meets a control, the region border, or a gap — and it is **only the three additive primaries**,
chosen by direction (red left, blue right, green horizontal) or by how far behind the crest it sits,
the way a channel-offset display splits a white form into R/G/B. The body of the sheet stays neutral,
so colour reads as fringing dust on the light — never a fill, never a secondary colour, never a spatial
hue ramp, never per-cell confetti. The field is a **substrate**: it paints only blank cells, so it fills
the surface *behind* text and controls and its fringes run up against the glyphs, keeping the menu one
continuous material rather than elements on a black plate. See
`docs/reviews/2026-10-07-pause-living-field.md` ("Field rework", "Fabric rework", "Chromatic-aberration
rework") and `docs/reviews/2026-10-09-backdrop-fabric.md`. The set is not required to be drawn as a
full-spectrum sweep; the fabric, not the colour, is the structure.

**Locked scope (2026-10-09).** On a display surface this is the *whole* of the colour: black and white
with the three additive primaries, and nothing else (change of direction recorded 2026-10-09). The
set's three secondary colours — yellow, cyan, magenta — are tokens for the editor's visualizations;
**no display surface draws them**, and a surface that wants a fourth hue is out of direction rather
than under-specified.

---

# 4. How this direction applies to the editor interface

These are the derived, binding interface consequences. They exist so that "what does the
direction mean for the editor" never has to be re-argued.

## 4.1 Faithful preview is mandatory

> "…faithful previews of the user's work."

The preview must use the **same rendering path as normal run**. The default view is a
**near-full-size, faithful preview** of the authored UI. An editor that shows a small, scaled,
approximate, or differently-coloured version of the user's UI violates the direction.

## 4.2 White-dominant, selectively coloured editor interface

> "Use a white-dominant, selectively colored treatment for the everyday workbench, and give richer
> chromatic activity substantial roles in live tools and visualizations."

Editor-interface colour is restrained. `text_primary` carries the emphasis; `accent` and `focus` are
used selectively for state, not as decoration. Rich chromatic activity belongs to the **preview, the
live tool feedback, and the visualizations** — not to the editor interface.

**Scope (2026-10-07).** This clause governs the **editor interface only**. Authored menus, the HUD,
the world, and visualizations are *display surfaces* and are governed by §1: they may be dense,
chromatic, and alive. See the change of direction above and
[`UI_DISPLAY_SURFACE_TARGET_2026-10-07.md`](UI_DISPLAY_SURFACE_TARGET_2026-10-07.md).

## 4.3 Preview-first, panes by progressive disclosure

The faithful preview is the primary surface. Hierarchy and inspector panes appear when editing and
collapse back to preview-first. The direction requires **both** faithful previews **and** a display
that feels alive; a permanently pane-cluttered editor sacrifices the first for no gain.

## 4.4 Motion budget

- Context enter/exit: `MAJOR_ENTER` 160 ms / `MAJOR_EXIT` 120 ms, glyph reassembly.
- Focus move and small feedback: `FEEDBACK` 80 ms.
- Relationship cues: `RELATIONSHIP` 120 ms.
- **Flowy and smooth is the standard, and it is locked (2026-10-09).** Display-surface motion is one
  continuous material: waves travel across the fabric, and a state change surges in and then retraces
  out as a single motion — the reveal unwinds exactly what the cover wound — rather than cutting between
  two pictures. What moves is the material; what stands still is the furniture.
- **Nothing in motion may move, displace, obscure, or delay a control, a focus marker, or a hit
  target.** The material moves; the controls do not.
- Reduced motion: immediate and non-spatial.
- **Forbidden:** persistent/global glitch, random flicker, decorative trails, particle bursts used
  in place of a real transformation, and any animation that reads as a filter over the interface.
  Coordinated, coherent, dense chromatic activity is **not** a "filter over the interface" and is not
  forbidden here; only *independent flicker* and *overlays laid over controls* are. See the 2026-10-07
  change of direction.

## 4.5 Guidance lives inside the editor

The interface must explain itself **where the user is**, not in a document they have to find. This
follows directly from "preserve clear typography, precise controls, immediate responses" and from
"Keep the controls dependable." A user who has to leave the tool to learn the tool is a failure of
the interface, not of the user.

## 4.6 The anti-patterns we already committed

Recorded so they are never repeated:

1. Hardcoded editor-interface colours instead of `ui_theme_provisional_tokens()`.
2. No `ui_preferences` and no compositor, so preview scale became a no-op that **shrank** the
   preview.
3. A two-row footer where one row was **unconditionally overwritten**, so the save/undo hint was
   never visible.
4. A selection marker written **inside** the preview, damaging authored cells.
5. A hardcoded preview origin ignoring `editor_baseline` and panel geometry.
6. A cramped fixed-column layout leaving most of the 260×160 grid empty, while claiming
   "three-pane composition".
7. "Parity" tests that rendered both hosts through the **same function** and therefore could not
   detect a wrong implementation.
8. Frame tests that asserted only the presence of a few strings anywhere on the grid.
9. Declaring a phase "implemented" while deferring the only gate that could have rejected it.
10. Leaving the editor interface permanently at 100% so the guidance text is too small to read at
    1920×1080, while the preview beside it scales normally.
11. **The root cause: treating this document's direction as optional.**

## 4.7 The workbench interface scales; the authored preview does not follow it

The workbench UI scale setting is a **readability control for the tool itself**, not a zoom for the
thing being edited. A 1920×1080 display shows the 260×160 surface larger than one person can
comfortably read, so the workbench interface — pane borders, footer rows, tooltips, status text —
uses an independent session-local scale through the compositor at 100/125/150/200.

The authored preview is the thing being edited. It must load at the **persisted application UI
scale** used by normal run, independent of the workbench interface scale. Changing the workbench
interface scale must never resize, recolour, or reflow the authored preview. Any future preview
zoom is a separate, explicitly named control — not either UI scale.

Two hard constraints, both from §4.4's "nothing displaces or obscures a control":

1. **Nothing leaves the surface.** At 200% a magnified row occupies two source rows, so half as many
   rows fit. The workbench interface must claim the room it needs — more footer rows, narrower panes —
   rather than let labels run past the edge or off-screen.
2. **Nothing is hidden or truncated.** Guidance text that does not fit one magnified row wraps to
   the next. Being unable to read it is the defect this section exists to prevent, so shrinking the
   text, dropping it, or clipping it instead of wrapping is not an acceptable resolution.

**Correction recorded 2026-09-19.** Earlier revisions of §2 and of the Step 1 plan stated that the
editor interface "never scales". A later 2026-09-18 correction stated that the preview and editor
interface "both obey the shared UI scale" — that coupled the tool to the thing being edited, which
is also wrong. Both wordings are superseded by this section: the workbench interface scales
independently; the authored preview uses the persisted application UI scale. The word "chrome" is also
retired: it appears in none of the direction sources, and it is replaced throughout this document by
"workbench interface" or "editor interface" so that the tool and the authored UI cannot be confused.

---

# 5. Citation protocol and drift alarm

## 5.1 Citation protocol

Every plan phase, implementation record, and review that touches colour, motion, animation,
transition, or interface feel must open with a line of this form:

```text
Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §<n>[.<m>] — <the clause served>
```

Worked examples:

- Replacing editor-interface literals with tokens → `§2 (existing foundation: palette adapter), §3.1 (palette roles), §4.2 (white-dominant editor interface)`
- Correcting interface scale → `§4.7 (the editor interface obeys the same UI scale as everything else)`
- Adding a context-enter transition → `§1.2 (glyph reassembly: precision → disorder → precision), §3.3 (MAJOR_ENTER 160 ms), §4.4 (motion budget)`
- Adding in-editor tooltips → `§4.5 (guidance lives inside the editor)`

## 5.2 Drift alarm

Any of the following is an **automatic rejection**, regardless of how good the change looks in
isolation:

1. A change touching colour, motion, animation, transition, or interface feel with **no §
   citation**.
2. A new hardcoded colour in the editor interface.
3. A new motion treatment outside the bounded vocabulary or outside the §3.3 role durations
   (0/80/160/120/120/1800 ms; the ambient role was added 2026-10-07).
4. Any motion that displaces, obscures, or delays a control, focus marker, or hit target.
5. Any animation that reads as a global/persistent glitch or as a filter over the interface.
   Coordinated, dense chromatic activity is **not** this; the alarm targets *independent flicker* and
   *overlays that read as a filter over controls*. See the 2026-10-07 change of direction.
6. A parity or equivalence claim proven by calling the same function twice.
7. An interface claim with no composed-frame artifact.
8. A requirement marked "met" without a named test or artifact behind it.
9. A new editor host, pane renderer, or animation renderer created beside the existing ones instead
   of extending them.
10. "It looked fine when I ran it."

## 5.3 What to do when the direction seems wrong

Do not silently deviate. If a clause appears to conflict with a practical need:

1. record the conflict explicitly in the phase record;
2. cite both the clause and the practical need;
3. continue with the clause in force until the project owner approves a recorded change to this
   document.

This document changes only deliberately, never by drift.