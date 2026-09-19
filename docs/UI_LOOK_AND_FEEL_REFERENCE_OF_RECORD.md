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
| **UI scale** | the shared `ui_preferences` setting, 100/125/150/200, applied through `ui_compositor` |
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
| Accepted diagnostic specimens | `make ui-theme-demo`, `make ui-motion-demo` ([`reviews/2026-09-16-v1-1-full-palette-demo.md`](reviews/2026-09-16-v1-1-full-palette-demo.md), [`reviews/2026-09-16-v1-1-ui-motion-demo.md`](reviews/2026-09-16-v1-1-ui-motion-demo.md)) | **Manually approved** palette and D6 motion references. The editor must look and move like these. |
| Live application-UI editor | `make ui-workbench` ([`reviews/2026-09-17-ui-workbench-i1.md`](reviews/2026-09-17-ui-workbench-i1.md)) | The proven host pattern: adapter palette, offscreen canvas, one composited layer, dedicated footer rows. Scale treatment is corrected in §4.7 — the I1 host left the editor interface at a fixed 100%, which is superseded. |
| Determinism harness pattern | `tests/benchmark_*.c`, `benchmark-*` / `stability-*` make targets | Determinism is proven by checksums and snapshots, never by prose. Reuse this pattern for frame gates. |
| Visual direction source | [`inspiration_and_notes/`](inspiration_and_notes/) | The primary references in section 1. |

## 2.1 The accepted motion boundary (from the approved motion specimen)

These boundaries were manually approved on 2026-09-16 and remain binding:

- Controlled display registration and glyph reassembly — **not** persistent or global glitch.
- **Stable** text, focus markers, controls, hit targets, semantic state, and interaction
  eligibility **while nearby decorative material moves.**
- Semantic warning/error/success/destructive colours remain authoritative and are **never**
  decorative RGB trails.
- Palette accent/focus traces are compared against exactly one isolated literal red/cyan sample;
  literal RGB is **diagnostic effect material, not a semantic token.**
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

## 4.3 Preview-first, panes by progressive disclosure

The faithful preview is the primary surface. Hierarchy and inspector panes appear when editing and
collapse back to preview-first. The direction requires **both** faithful previews **and** a display
that feels alive; a permanently pane-cluttered editor sacrifices the first for no gain.

## 4.4 Motion budget

- Context enter/exit: `MAJOR_ENTER` 160 ms / `MAJOR_EXIT` 120 ms, glyph reassembly.
- Focus move and small feedback: `FEEDBACK` 80 ms.
- Relationship cues: `RELATIONSHIP` 120 ms.
- **Nothing in motion may move, displace, obscure, or delay a control, a focus marker, or a hit
  target.** The material moves; the controls do not.
- Reduced motion: immediate and non-spatial.
- **Forbidden:** persistent/global glitch, random flicker, decorative trails, particle bursts used
  in place of a real transformation, and any animation that reads as a filter over the interface.

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
scales with the `ui_preferences` setting, through the compositor, at the same 100/125/150/200
presets as normal run.

The authored preview is the thing being edited. It must stay a **faithful copy of the authored UI
at a fixed preview scale**, independent of the workbench interface scale. Changing the workbench
interface scale must never resize, recolour, or reflow the authored preview. Any future preview
zoom is a separate, explicitly named control — not the workbench UI scale.

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
independently; the authored preview stays faithful at its own fixed scale. The word "chrome" is also
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
3. A new motion treatment outside the bounded vocabulary or outside the 80/160/120/120 ms roles.
4. Any motion that displaces, obscures, or delays a control, focus marker, or hit target.
5. Any animation that reads as a global/persistent glitch or as a filter over the interface.
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