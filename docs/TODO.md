# Future Features and Deferred Work

## Purpose and authority

This document preserves desired features, follow-up ideas, unresolved design
questions, and deferred engineering work. It is an **idea inventory**, not an
authoritative roadmap, implementation plan, commitment, or priority order.

Dependency order, engineering gates, review checkpoints, and the current next
phase are maintained separately in `FEATURE_ROADMAP.md`. For a concise current
status summary, see `CURRENT_STATUS.md`.

An unchecked item means “wanted or worth investigating,” not “ready to build.”
Before implementation, each substantial item needs its own requirements
discussion and scoped plan. Accepted current behavior remains defined by
`EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md` until deliberately revised.

Planning labels used below:

- **Ready for focused planning** — desired behavior is reasonably clear.
- **Needs product decision** — multiple valid user-visible models exist.
- **Needs architecture/design** — the current model cannot represent it safely.
- **Research track** — feasibility, cost, or interaction rules need evidence.
- **Ongoing maintenance** — recurring work, not a one-time feature.

These labels describe readiness, not priority.

## Post-R12 future pillars — foundation complete, product depth still open

R0–R12 are verified roadmap foundations, not a claim that the editor or game-making
experience is feature-complete. Future planning should treat the following as large,
related design pillars that may graduate into standalone requirements documents or
future roadmap phases after discussion:

1. **Game-flow editor polish and graph UX.** R12 proves the data model, validation,
   staged workspace, and report-only runtime handoff. The current authoring UI is still
   barebones and rough; smoothing graph manipulation, validation feedback, visual
   readability, mouse/node interactions, and eventual target loading remains open.
2. **UI design system, feel, and interaction standards.** The current UI is a solid
   readable foundation, but reusable standards, motion, tactile feedback, audio-cue
   readiness, theme tokens, menu-depth discipline, and reduced verbosity need a
   dedicated design pass. The target inspiration is terminal-esque retro-future UI
   with System Shock / Alien / Blade Runner influence: alive, smooth, responsive, and
   characterful without overwhelming the user.
3. **Entity, sprite, animation, and attribute behavior model.** R11 verifies baseline
   sprites, objects, triggers, and animation authoring. It does not settle feature
   completeness, attribute tagging, entity behavior, variable sprite dimensions, scale,
   multi-view sprites, or whether standalone sprite placement should remain a general
   workflow.
4. **Spatial expressiveness and material control.** Heightfields, surface authoring,
   and optical rendering exist, but richer per-section material changes, bridges,
   ductwork, portal/visual tricks, complete reflections, and global-vs-individual
   material editing still need design.
5. **Player-facing game creation.** Start Game still uses the legacy path, projects are
   not exportable as completed games, and large systems such as procedural generation,
   scene templates, multiple controllable actors, inventory, conversations, overworlds,
   and personality/knowledge/stat models remain long-horizon ideas.

Keep these pillars in `TODO.md` until each has enough decisions to promote into
`docs/FEATURE_ROADMAP.md` or a focused planning document.

## Current constraints affecting future work

1. The world is a fixed-size 2D grid (`SCENE_MAX_WIDTH`/`SCENE_MAX_HEIGHT`),
   but cells are no longer full-height walls: authored cells carry occupancy,
   wall/floor/ceiling materials, floor/ceiling presence and height steps, and
   gravity data (R4/R8). Material `0` is empty/passable.
2. Legacy digit-grid maps save one decimal digit per cell, so only material IDs
   `0..9` persist through the legacy writer. Native `.tscene` scenes persist
   three-digit material IDs `001..255` (README documents the split).
3. Floors and ceilings are authored per-cell surfaces with presence and height
   steps; they are not global fixed planes (R4/R8).
4. Camera pitch remains a horizon offset; it is not a true 3D pitch angle and
   true angular pitch is deferred (R8). The camera and collision model now have
   a Z component through vertical physics (eye height, floor/ceiling heights).
5. `SceneDocument` owns and saves the map, authored cells, spawn, movement
   parameters, lights, decals, optical overrides, and (scene v8) sprite
   instances. `WorldState` is a derived runtime rebuild, not a second authored
   source of truth.
6. A material references a shared palette and four distance-band glyphs.
7. Decal authoring is integrated into the unified editor (surface-inspector
   Decals submenu, undo/redo, native persistence). `decal_io` and
   `decal_painter` remain available as tested headless primitives.
8. The window resizes by scaling a fixed logical grid. UI assets mostly use
   absolute grid coordinates and do not responsively reflow.
9. Rendering supports layered optical composition (R9): translucent layers,
   bounded one-bounce mirrors, per-column depth frontiers, and sprite/decal
   overlay depth ordering. Nearest-opaque-column is no longer the only path.

## Build and performance maintenance

The current deterministic maintenance gates are `make benchmark-headless`,
`make stability-fast`, and `make stability-headless`. The shipping optical benchmark
now records compatibility, opaque, translucent, and one-bounce mirror paths with
checksums and a 6 ms budget. Native display and platform profiles remain tracked in
[`PLATFORM_VERIFICATION_PROFILES.md`](PLATFORM_VERIFICATION_PROFILES.md).

- [ ] **Research track — explicit `-O3` build profile:** the strict default is
  now `-O2` (set during the 2026-08-21 heightfield performance work; the earlier
  "strict unoptimized default" guidance is superseded). All optimization-only
  diagnostics exposed by `-O2 -Werror` were resolved without suppression.
  Remaining research: evaluate a separate strict `-O3` Make profile, verifying
  framebuffer checksums, aggregate tests, sanitizers, tracker modes, and renderer
  benchmarks independently. Do not use optimization to conceal algorithmic
  regressions.

## Data-first configuration and external editability — **Needs architecture/design**

**Wanted:** Recommit to “from data” behavior where practical. User-set variables,
project-authored rules, startup settings, UI defaults, visual tokens, gameplay tuning,
and export/runtime metadata should generally live in typed, validated, text-based
configuration or asset files rather than scattered hard-coded constants.

This does **not** mean accepting arbitrary scripts or generic dictionaries. It means
choosing explicit schemas, defaults, migrations, and error behavior for data that users
or projects reasonably need to inspect or edit outside the program. Future planning
should audit at least:

- app/editor UI element definitions, style tokens, theme colors, motion parameters, and
  sound-cue mappings;
- project-level flow, export, scene-template, procedural-generation, and runtime-start
  manifests;
- entity attribute/tag schemas, behavior parameters, inventory/conversation data, and
  player/avatar defaults;
- asset-authoring defaults such as sprite dimensions, material-edit scope, and palette
  or material presets.

Each file family must define ownership, validation, missing-reference handling,
transactional save/discard rules, and whether it is per-user, per-project, per-scene, or
per-asset.

## UI design system, standards, motion, and tactile feel — **Promoted to v1 roadmap V1-1/V1-3**

**Wanted:** Turn the current readable UI foundation into a rock-solid, reusable design
system and implementation standard. New buttons, menus, property rows, graph widgets,
and editor workspaces should not need multiple attempts before alignment, spacing,
focus, state feedback, and navigation feel correct.

Target aesthetic and feel:

- terminal-esque and retro-future, with System Shock / Alien / Blade Runner as loose
  references rather than literal copies;
- characterful and alive without visual noise;
- incredibly responsive, smooth, and tactile;
- simple, direct, and precise, like a luxury manual car built for someone who likes
  tools that work without over-assisting;
- less verbose where concise labels, progressive disclosure, or better layout can make
  intent clear;
- resistant to burying common actions in deep menu layers.

Design work should define centralized tokens and reusable modules for colors, spacing,
focus/hover/active/disabled/error/success states, motion timings, transition policies,
keyboard/pointer equivalence, layout density, row labels, button affordances, graph-node
styling, modal prompts, and editor-vs-authored-game UI boundaries. It should preserve
contrast, focus visibility, deterministic input, and a reduced-motion path.

The first high-pressure consumer is the game-flow editor polish track, but the standards
should apply broadly to editor inspectors, authored Menu editing, asset pickers, and
future gameplay/project tools.

The accepted v1 sequencing, separate editor/authored-UI ownership boundary, semantic
token scope, pointer foundation, and major-context motion policy now live in
[`FEATURE_ROADMAP.md`](FEATURE_ROADMAP.md). Opening and closing major contexts receive
motion; submenus normally change immediately within their stable parent. Exact measurable
tokens, context roles, timings, and reduced-motion equivalents remain V1-1/V1-3 Q1 work,
not decisions to duplicate in this inventory.

### Palette hues outside the locked display identity — **Owner decision needed**

The display-surface identity was locked on 2026-10-09 as *black, white, red, green, blue and flowy,
smooth motion* (see the change of direction in
[`UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md`](UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md)). Two things in the
shipped tokens sit outside it, and neither is a defect yet — they are decisions waiting to be taken:

- the editor-interface roles still name hues the identity does not: `accent` `#67F5C2`, `focus`
  `#A8FFE1`, `warning` `#FFD166`, `success` `#71F79F`, `error` `#FF6B7A`, `destructive` `#FF8894`,
  `selection_background` `#123D32`, `editor_selection` `#70B7FF`;
- the **scene browser's focus-perimeter base draws `focus`**, so a mode-selection display surface
  still shows a mint hue the identity does not include. The authored menus' perimeter and the tide's
  wake now use the shared RGB chase (`ui_theme_chase_colour`), so that part of this note is closed;
  the scene browser base and the editor-interface roles remain open.

Closing this means either retiring those roles from display surfaces (and drawing the focus marker from
black/white/primaries) or re-deriving them inside the identity, with the same record discipline as every
other look change. The three secondary material colours (yellow, cyan, magenta) are already
editor-visualization tokens and are not drawn on a display surface.

### Theme and interface color customization — **Small feature after token contract**

Changing the main interface color is desirable, but should be implemented through the
shared token/theme contract rather than one-off color branches. A minimal version can be
a validated accent/theme preset in user preferences with safe fallback and preserved
contrast.

### Full-range color picking and transparent backgrounds — **Needs shared color/alpha design**

**Wanted:** Replace limited color-selection workflows with a robust picker that exposes
the full supported color range wherever authors choose colors. UI elements and authored
glyph cells should also be able to use a fully transparent background rather than being
forced to paint an opaque color behind their content.

Define one consistent color/alpha model across UI authoring, materials, sprites, decals,
and previews. The design must distinguish a transparent background from an absent/blank
cell, preserve transparency through save/load and copy/paste, show it clearly in editors,
and specify compositing, selection, contrast, and fallback behavior. Decide whether alpha
is background-only or independently available for foreground glyph color before changing
asset formats or renderer semantics.

## Audio seam and future sound-cue integration — **API first; implementation deferred**

Audio is not ready for full implementation, but UI and runtime systems should be designed
so sound cues can be added later without invasive rewrites. A future first step may be an
empty/stub audio module with a narrow API/ABI for typed events such as confirm, cancel,
focus move, invalid action, save success/failure, graph connection, and runtime target
handoff.

Initial scope should avoid committing to a backend, mixer, asset format, or dependency.
Tests can prove callers can emit typed audio events and that the no-audio implementation
is deterministic and safe. Real playback, volume controls, spatial audio, music, and
asset pipelines require separate planning.

## Mouse and pointer interaction model — **P1–P2 implemented 2026-11-06, P3 open**

Mouse use is not yet required for every workflow, but upcoming graph editing, object
rotation, authored Button activation, canvas editing, and richer scene manipulation need a
coherent pointer model. The model, its requirements and its phased plan are drafted in
[`UI_POINTER_INTERACTION_MODEL_2026-11-06.md`](UI_POINTER_INTERACTION_MODEL_2026-11-06.md).
P1 landed: every editor pointer surface takes its rectangle from the painter's own
geometry, the authored-Menu test surface no longer uses a literal origin, and flow-graph
node boxes highlight under the pointer. P2 landed: one shared authored hit test for the
press and the hover, a preview hover handle, and a grab that owns the pointer so a drag
pans the graph view without touching the document
([record](reviews/2026-11-06-pointer-region-contract.md)). P3 — rotation preview,
display-backed Button activation, wire gestures — still needs D3 and D4 answered, and
the agenda the model must satisfy remains:

- coordinate conversion across window pixels, logical grid cells, UI scale, authored Menu
  preview scale, and scene/editor views;
- click, drag, hover, press/release, cancel, and capture semantics;
- keyboard-equivalent accessibility for every pointer-only-looking action;
- node graph creation/connection/manipulation gestures;
- non-permanent object/entity rotation previews;
- reliable display-backed Button activation in R12 Test mode;
- tests for real-window or display-backed behavior where practical.

## Fonts, glyphs, and paste — decision register (options) — **Core decisions recorded 2026-11-06**

The three dimensions below are the font programme the owner asked to have written out. The
dependency and representation questions are now **answered** in
[`V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md`](V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md):
glyphs are converted locally at build time with FreeType from **user-supplied font files**, and
**no glyph data is committed** — the contract is a font manifest carrying name, per-file SHA-256
and source URL, so two installs can prove they converted the same bytes (A3a, revised); vendored
`utf8proc` for UTF-8/NFC/segmentation; no runtime rasteriser; no HarfBuzz in v1; bidi deferred to
V1-6 under an LGPL-free constraint; a **fixed 8×8 cell** — 8×8 CJK bitmap faces exist, so taller
text profiles were a solution to a problem that does not exist (A2b, revised); the packed 32-bit
`GlyphRef` (A1b/B1b); and conversion that aborts with the exact failing glyph and offers squash,
scale or normalise as **opt-in** remedies rather than fitting silently.

Axes still open, to be decided as their phase starts: A4 (which assets carry a face), A5
(ASCII-art alignment contract), A6 (animated face changes — pencils down until A4 exists), A7
(tooling), B2 width classes, B3 clusters beyond one-versus-many, B6 block/shade coverage source,
and all of C (paste safety) which is V1-6 work. The options are kept below because those choices
are still live; the ceiling they all bend around has not changed.

**Inputs gathered 2026-11-22** — [`reviews/2026-11-22-unscii-and-ertdfgcvb.md`](reviews/2026-11-22-unscii-and-ertdfgcvb.md),
research only, no decision taken:

- **unscii** is an independent arrival at this programme's premise (an art-compatible Unicode
  bitmap font family; its author's stated reason is the same chicken-and-egg problem V1-4 exists
  to break). `unscii-8` is 8×8 and **public domain**, with the block/shade/box-drawing coverage
  that `B6` is asking for, and the source ships a bitmap→font converter like ours. It is now the
  **third face in `assets/fonts.example.txt`**, beside Misaki.

  Measured while adding it: the **vectorised TTF cannot be used** — no 8×8 strike, outlines
  rasterised at 8 px, **227 of 3,159 glyphs overflow the cell**, exit 1, no set written. The
  `.hex` (Unifont hexdump, 3,191 glyphs, one line each) is the real artefact and needs no
  rasteriser. Consequence: a **`.hex` input path for `fontconv`** is the next worthwhile font-side
  increment — small, orthogonal to the cell migration, and it removes a host dependency. Note for
  whoever writes it: hex is MSB-first, our cells are LSB-first, so bits reverse within each byte.

- **ertdfgcvb Play** is the editor half: per-cell `main(coord, context)` with `pre`/`post`, shader
  -like by design, console-plus-hotkeys, programs as permalinks, Apache-2.0 code and **fonts
  deliberately not in the repository** (an independent arrival at our D2 rule). It is a better
  generator and we are a better canvas; the compatible adaptation is a generator that **bakes**
  into authored cells, exactly as `mapgen` bakes scenes — never a user-code runtime, which would
  make every frame non-deterministic by construction.


**Implemented 2026-11-06** (first slice of V1-5, all under `make test`): `src/sha256` (FIPS 180-4,
checked against published vectors including the million-`a` case), `src/font_manifest` (strict
`key=value` codec: unknown/duplicate/missing keys, unpaired `file`/`sha256`, `cell` that is not
8x8, canonical text that round-trips), and `tools/fontconv --list-fonts` (`make fontconv`), which
verifies installed files against the manifest and exits 0/1/2 as verified / mismatch-or-missing /
could-not-check. No font data is committed and `assets/fonts/` is ignored, so a checkout carries
the contract and not the bytes.

Exercised against the real archive: `misaki_ttf_2021-05-05.zip` downloaded from
<https://littlelimit.net/misaki.htm>, three 1.1 MB TTFs, a local manifest generated from their real
digests — `PASS` exit 0 for the good set, `HASH-MISMATCH` when the wrong style is copied over a
declared name, `MISSING` when one is deleted. That local manifest is **not** committed;
`assets/fonts.example.txt` ships instead, with 64-zero placeholder digests and a passing run's
output, and a test keeps it parseable and keeps its digests fake. Filling the example in with real
digests verifies the archive, which is how its shape was confirmed.


**Next:** the gate for anything visible in the game — the **cell identity migration**. `Cell.glyph`
is a byte and `CellState` packs glyph + fg + bg into a `uint64_t` (56 of 64 bits used), so a codepoint
wider than 255 cannot travel. Measured blast radius (2026-11-22): 15 direct `cell->glyph` reads, but
87 `grid_set` and 45 `ui_canvas_set` call sites to re-type — and **no scene-format impact**, since
scenes store decals and objects, not cells. The real fork is `CellState`: 128-bit state, or a 64-bit
glyph handle. That is a decision before it is code, and it deserves its own record.

An earlier note here said "124 call sites across 20 files"; that was a search result, not a
measurement — it counted decal `glyph_step` and UI `glyph_set` strings. Corrected with the
per-quantity table in the handoff.

Landed 2026-11-06, so that migration starts from a working resolver rather than a blank page:
`src/glyph_source` answers "which face draws this codepoint, or is it missing" in one place —
converted set first, built-in ASCII second, derived `X` sentinel last, never an error — and
`fontconv --render <hex codepoints>` draws through it, so the rule is observable today without
touching the renderer. The renderer still draws `font8x8_basic`; nothing visual changed.

Landed 2026-11-06, on top of the converter: `src/glyph_set` (the single definition of the set's
byte layout, strict validation with a named result per rejection, binary-search lookup) and
`make font-determinism FACE=<face>` (convert twice, compare bytes, validate the result; reports
`FAIL-TOOL` naming the missing manifest when fonts are absent rather than passing vacuously).
`fontconv --check-set` reads a real set back through that same module and prints sample glyphs,
which is how the producer and consumer were shown to agree on the 114,784-byte Misaki artefact.

`--convert` landed 2026-11-06 and was measured on the real archive: Misaki's TTF ships embedded
8×8 bitmaps, so its 7,171 glyphs convert through `none` with zero refusals and zero re-rendering;
two runs are byte-identical; at `--size 16` the run refuses 7,094 glyphs naming the exact
overshoot and writes nothing, while `--fit scale` converts them and reproduces the font's own 8×8
bitmaps for full-width glyphs byte for byte. That is the evidence behind the quarter-coverage
threshold: a box-filtered 16 px outline lands on the designer's bitmap rather than near it.

Also learned while measuring, for the width-class question (`B2`): Misaki reports 7,003 full-width
and 168 half-width advances, so mixed Latin/Japanese text cannot be one cell per codepoint without
deciding what a half-width glyph does in an 8×8 cell. The converter reports the split; the decision
belongs to B2.

Why these are decisions and not tasks: the ceiling is not the font, it is the **cell**.
`Cell.glyph` is a `uint8_t`; `renderer_create` rejects any cell that is not exactly 8×8;
the only face is `font8x8_basic[128][8]`, compiled into the binary; and every asset
schema (`Material.glyphs[4]`, `PatternCell`, `ui_elements content=`, `.tui content=`)
stores a byte. Each option below is really an option about that stack.

### A. Real font files, chosen per asset, animatable

A1. **Glyph identity in the cell** — (a) add a `font_id` byte beside `glyph` (256 faces
    per cell, +1 byte/cell ≈ +128 KB at the current 512×256 caps); (b) replace `glyph`
    with a packed 32-bit `(font_id, glyph)` (+3 bytes/cell ≈ +384 KB); (c) keep
    `glyph` and add a *profile* id that implies a whole look — least flexible, least
    schema churn.
A2. **Cell metrics** — (a) normalise every face onto one cell box, monospaced to the
    cell (keeps the renderer contract, allows many faces); (b) a small set of *cell
    profiles* (8×8, 8×16, 12×12…) with mixed profiles in one grid (touches renderer,
    raycast column mapping, layout resolver, hit testing, occlusion); (c) fully
    variable cells, i.e. a new renderer.
    *(Decided 2026-11-06: **A2a, one 8×8 cell.** A2b was built on a false premise —
    8×8 CJK bitmap faces exist, so the taller profiles solved nothing and changed the
    visual language for free.)*
A3. **Raster source** — (a) SDL_ttf at **build** time into atlas pages (deterministic,
    no runtime dependency, no cache eviction); (b) SDL_ttf at **runtime** with an LRU
    cache (flexible; needs pre-warm, eviction and load-failure diagnostics);
    (c) baked bitmap faces only, a named hand-authored set (cheapest, no TTF).
    *(Decided 2026-11-06: **A3a, revised** — conversion at build time over fonts the user
    supplies, but no generated glyph data committed; a font manifest of names, file hashes
    and source URLs is the contract, and a failed conversion aborts with the exact glyph
    and reason, offering squash/scale/normalise as opt-in remedies.)*
A4. **Which assets carry a face** — material, sprite/decal pattern cells, `ui_elements`,
    authored `.tui` elements, editor chrome. Each is a schema + parser + validation +
    default decision; absent means "profile default", which is the migration story.
A5. **ASCII-art alignment** — (a) art is authored against a named face and the editor
    warns when the effective face differs; (b) art records its face and refuses to
    render under another; (c) art is font-agnostic by contract (only glyphs whose
    advance equals the cell).
A6. **Animated font changes** — (a) font id is animation-observable state, sampled like
    colour; (b) animations may switch face only at boundaries, never per frame;
    (c) faces are static per asset, not animatable.
A7. **Tooling** — pick and preview a face per asset, plus an alignment preview for art.

### B. Non-ASCII: other languages, custom symbols, emoji, block and shade glyphs

B1. **Encoding to identity** — (a) UTF-32 codepoint per cell; (b) interned glyph id per
    (face, codepoint, presentation selector); (c) cluster id into a grapheme table.
B2. **Width classes** — (a) double-width glyphs occupy two cells (glyph +
    continuation), full stop; (b) refuse wide characters outside faces declared
    wide-safe; (c) variable-width cells (see A2c).
B3. **Clusters** — (a) one visual cell may hold a grapheme cluster (combining marks,
    emoji ZWJ sequences); (b) single codepoints only, clusters refused.
B4. **Missing glyphs** — (a) per-face fallback stack, then a tofu box; (b) strict: a
    missing glyph is a load-time validation error; (c) silent replacement character.
    *(Decided 2026-11-06: **none of the three.** No fallback chain — a glyph the declared
    face lacks is never borrowed from anywhere — and no load-time rejection either, because
    absent text must stay inspectable. The cell renders a derived sentinel (`X`, red
    background, purple foreground, outside the theme) and raises `TSG-ASSET-ENV-0001` once
    per (face, codepoint). Same rule as the missing-decal `!`/`#` checker: derived, never
    serialised. See the decision record's "missing-glyph sentinel".)*
B5. **Font assets and packaging** — licensing, load-failure diagnostics per
    `ERROR_CATALOG.md`, and carrying faces through export/packaging (V1-19).
B6. **Block and shade art** — require coverage of U+2500–259F in a face declared
    art-safe, or author those glyphs explicitly per face.

### C. External paste, safely (especially ASCII art)

C1. **Trust boundary** — (a) one validator shared by typed and pasted text; (b) a
    separate paste sanitiser. (a) is fewer places to audit.
C2. **Control characters** — strip or reject ESC (pasted terminal art is full of ANSI
    SGR), BEL and other C0/C1; normalise CRLF; decide tab handling (expand for art).
C3. **Spoofing** — bidi overrides, zero-width characters and lookalikes: strip, permit
    and visibly mark, or refuse. Filenames must never accept them.
C4. **Width and clusters on paste** — refuse wide/mixed-width art, or accept it under a
    documented continuation-cell rule (pairs with B2).
C5. **ASCII-art specifics** — preserve column alignment absolutely (never re-wrap);
    pad or reject ragged rows explicitly; distinguish *paste as art* (into a sprite or
    decal canvas) from *paste as text*; refuse rather than mangle.
C6. **Limits** — bounded buffer and cell cap checked before allocating, with typed
    diagnostics for every rejection.
C7. **Platform** — SDL clipboard APIs only; no OSC 52, no shelling out, no network.

## Cross-cutting decisions for later discussion

### Versioned scene persistence — **Resolved 2026-08-28 (R1–R11 chain)**

Scene v1–v10 resolve the earlier open questions:

- One versioned `.tscene` file with strict unknown-key parsing and v1–v9
  migration; canonical writes are v10 (R1/R2/R4/R6/R10/R11).
- Legacy digit-grid maps import non-destructively through the chooser flow.
- Native saves are atomic; `SceneDocument` owns map, cells, spawn, movement,
  lights, decals, optical overrides, sprite instances, triggers, and simple
  objects. `WorldState` is a
  derived runtime rebuild.
- Older scenes get deterministic migration defaults (e.g. v6→v7 exact point
  lights; v7→v8 empty sprite list; v8→v9 empty trigger list; v9→v10 empty object
  list) and load-time
  repair diagnostics for dangling references.

Still open: per-scene ambient light and per-channel ambient (separate from R10),
and richer object attributes beyond `simple`.

### Extensible per-cell block serialization — **Needs product decision**

**Wanted:** Move per-cell material tokens from fixed 3 decimal digits to fixed-width
hex blocks, e.g. `XXX-XXX-XXX`, where each 3-digit block is one typed field; new
per-cell fields are added three hex digits at a time.

Shape of the idea:

- One `-XXX` block = one `0x000..0xFFF` value (max 4096 per field).
- New fields (per-face wall materials, flags, surface policy) append a block
  instead of forcing section/version churn.
- Stays in the same human-editable grid style as today's `001 001 001`.

Current constraints (verified 2026-08-10):

- The 255 material ceiling comes from three places, NOT from digit width:
  `SceneAuthoredCell` stores `uint8_t` IDs, `AssetRegistry` slots are
  `materials[256]`/`palettes[256]`, and the parser caps blocks with
  `parse_uint_range(..., 255U, ...)`. Hex blocks widen the token to 4096 but
  capacity only moves if the cell type, registry, and loader widen together.
- 3 hex digits cap at 4096 per field; exceeding that breaks fixed width. This is
  extensibility-by-new-block, not unbounded capacity.
- Adopting it is a v2→v3 migration reusing the v1→v2 pending/repair machinery.

Open decisions:

- Concrete consumer first: per-face wall materials (already deferred by R4),
  capacity above 255, or both?
- If capacity: choose typed width and registry size first; token base is secondary.
- Add blocks only when a real field exists, or reserve slots now?

Incremental E follows this idea only in the sense that missing authored surfaces
need an explicit render fallback; the block schema itself is deliberately parked.

### Height-aware 2.5D world questions — **Resolved 2026-08-19**

These questions were answered in [`archive/r8/R8_DECISION_RECORD_2026-08-19.md`](archive/r8/R8_DECISION_RECORD_2026-08-19.md):

- Stacked rooms are **not** required; one traversable interval per X/Y remains
  authoritative (R1 contract).
- True unrestricted vertical rotation is **not** an R8 requirement: the
  horizon-offset pitch is kept and true angular pitch is deferred as a future
  option (record Decision 3).
- Slopes are **cell-aligned ramps only**, never arbitrary inclined planes
  (record Decision 2).
- The long-term renderer **remains a ray caster** with a continuous per-cell
  heightfield representation; sectors/portals, voxels, and full-3D geometry
  remain rejected (record Decision 1).

### Geometry, collision, and appearance separation — **Resolved for optical/occupancy split; see open follow-ups**

Mirrors, glass, invisible collision, and pass-through surfaces require separate
properties for occupancy, player collision, ray/light interaction, visual
material, opacity, and reflectivity. These must not remain encoded by the
special meaning of material ID `0`. Resolved by the R9 optical model (see
[`archive/r9/R9_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-24.md`](archive/r9/R9_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-24.md) and the R9 closeout): authored occupancy,
per-channel transmission, opacity, sight-ray blocking, and reflectivity are
distinct scene fields; mirrors are bounded to one-bounce, authored-positive
reflectivity. Open follow-ups: per-face wall materials (deferred), reflected
decals (documented limitation), and emissive surfaces (R10 I3 DEFER).

### Stable IDs and atomic command groups — **Resolved for instance content**

Movable lights, decals, sprites, and object-style scene instances use the
64-bit `SceneInstanceId` namespace with `0` reserved and a persisted
`next_instance_id` high-water mark. Multi-target operations apply through the
transactional command system and appear as one undoable user action. ID
persistence/reuse, references after move or deletion, and selection survival
are covered for lights, decals, and sprites. Remaining: bulk selections, map
resize batching, and object/trigger reference rules (R11 I3 and later).

---

# Editor usability and workflow

## Selected-item highlighting — **Wall-face baseline verified; generalization deferred**

**Implemented baseline (2026-07-29):** The current wall editor draws a dashed
hovered wall-face outline, a solid persistent selected-wall-face outline, and an
adaptive center crosshair. Escape dismissal clears the persistent selection. The
overlay is allocation-free, respects nearest-wall occlusion, and does not mutate
authored materials. See `EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md` and
[`archive/editor-unification/EDITOR_HIGHLIGHT_IMPLEMENTATION_RECORD_2026-07-29.md`](archive/editor-unification/EDITOR_HIGHLIGHT_IMPLEMENTATION_RECORD_2026-07-29.md).

**Preserved future idea:** Generalize the same interaction vocabulary to floors,
ceilings, lights, decals, objects, bulk selections, and hidden targets after those
target types and occlusion semantics exist.

Likely implications:

- editor-only overlay; never mutate the authored material;
- reuse the verified wall-face vocabulary, then generalize by target type;
- distinguish hover, primary selection, and future bulk selection;
- remain clear at distance/oblique angles and not rely only on color;
- define hidden/occluded indicators and reduced-motion behavior.

### Hover-outline visibility — **Research track**

The R4 manual review found the unified `.` hover outline visually consistent across
walls, floors, and ceilings but too easy to miss against some authored materials and
viewing angles. Evaluate visibility improvements without returning to a filled overlay
that hides the material being inspected. Compare at minimum: a brighter or alternating
outline glyph, contrast-aware glyph selection, a two-tone/double edge where space
permits, and restrained animation or pulsing with a reduced-motion/static equivalent.
Test dark/bright palettes, missing-material purple, distance, oblique floor/ceiling
views, UI scale presets, and overlap with persistent `#` selection. Do not rely on
color alone or mutate authored cells.

## Open and switch saved scenes/files — **Current-map baseline implemented; scene-dependent later**

**Verified baseline (2026-07-29):** Main-menu
Editor and in-editor `Ctrl+O` open a sorted in-game list of regular lowercase
`.txt` direct children under `assets/maps/`. Dirty switches use Save/Discard/Cancel,
and discovery/save/load failures preserve the live document as defined by
`EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md`.

**Intentionally not included in the current-map baseline:** native dialogs,
editable/arbitrary paths, recursion, symlink following, recent files, New, Save As,
or a generic scene/list framework.

Preserved future scene/file implications:

- versioned scene picker, validation, empty states, load errors, recent files, and
  Save As after the scene format and ownership model exist;
- migration/import policy for current digit-grid maps;
- scene-root path/name policy and safe asset-root handling.

Avoid making the UI permanently map-file-specific if a future level contains
map, lights, decals, spawn, and ambient data.

## UI scale and resizing — **Bounded accessibility zoom verified; responsive model deferred**

**Verified baseline (2026-07-30):** Menu, HUD, and editor text support independent
100%, 125%, 150%, and 200% UI scaling with a 150% default, immutable/runtime
preference precedence, atomic live persistence, global shortcuts, and Main/Pause
Settings access. The fixed crosshair and editor highlights remain unscaled. See
[`archive/r0/R0_UI_ZOOM_ACCESSIBILITY_IMPLEMENTATION_RECORD_2026-07-30.md`](archive/r0/R0_UI_ZOOM_ACCESSIBILITY_IMPLEMENTATION_RECORD_2026-07-30.md).

**Wanted:** Comfortably resize editor text and controls.

Possible scopes:

1. UI zoom presets (100%, 125%, 150%, 200%) independent of world scale.
2. Runtime logical grid/cell resizing, recreating grid, framebuffer, texture,
   tracker state, and pointer-coordinate conversion.
3. Responsive reflow with anchors, percentages, min/max sizes, and flow/layout
   containers.

**Decision 2026-07-30:** R0 implements scope 1 only through the approved
[`archive/r0/R0_UI_ZOOM_ACCESSIBILITY_PLAN_2026-07-30.md`](archive/r0/R0_UI_ZOOM_ACCESSIBILITY_PLAN_2026-07-30.md). The accepted model uses immutable
`default_user.ini`, runtime-written `user.ini`, immediate persistent live
adjustment, 100/125/150/200% presets with a 150% shipped default, global keyboard
shortcuts, and a bounded Settings menu. UI is composited separately so world
scale and the fixed logical grid remain unchanged. Scopes 2 and 3 remain
deferred and require their own later decisions.

The current inspector uses direct `grid_print` while menus use `UiLayout`; the
verified R0 implementation converges their rendering through one bounded ordered
layer/compositor boundary. R0 exposes one global inherited scale while retaining
fixed-100% and internal explicit-preset layer policies so concrete UI subtrees
can be separated later without a renderer redesign. It deliberately does not
resolve the later responsive-layout model, runtime logical-grid resizing, or
broader supported-resolution policy.

### Independent UI-role and element sizing — **Deferred; foundation in R0**

**Wanted later:** Independently size concrete UI roles or element subtrees—for
example a 200% debug HUD, 150% editor inspector, 200% warning, and 100% crosshair—
with deterministic overlap and persistence.

R0 provides only the rendering seam: compact ordered layers, stable painter's
order, per-layer scale policy, anchors, clips, visibility, and fixed capacity.
The user-visible preference remains one global scale in version-1 user settings.

Before exposing independent controls, decide:

- which stable semantic roles are user-adjustable rather than arbitrary asset
  names;
- whether child elements inherit, override, or form separate layers;
- preference schema/version migration and missing-role behavior;
- settings-menu presentation and reset semantics;
- overlap, focus/hit-testing, clipping, accessibility, and performance limits;
- whether the capability belongs with the later responsive UI model rather than
  as isolated scale overrides.

Do not persist transient element pointers, file-order indices, or unrestricted
user-created layers. Promote only proven concrete roles/subtrees.

## Bulk selection and batch editing — **Needs product and command design**

**Wanted:** Select and edit multiple cells, surfaces, or instances.

Possible tools include rectangle, additive/subtractive selection, connected
region, and select-by-material/type. This implies a selection set, primary
selection, atomic batch commands, mixed-value inspector states, bounded memory,
and clear handling of invalid targets. Decide initial target types, mixed-type
selection, and persistence through structural edits.

## Broad Unicode glyph and font support — **Promoted to v1 roadmap V1-4 through V1-7**

The v1 roadmap now commits validated UTF-8/NFC text, grapheme-aware editing, shaping,
bidirectional layout, exact-cell glyph art, CJK/kana/hangul priority, project/custom
fonts, deterministic project fallback, visible missing-glyph diagnostics, IME input,
versioned asset migration, and Linux/Windows export parity. Ordinary text and glyph-art
cells are deliberately distinct primitives. Full/color emoji, arbitrary ZWJ sequences,
and glyph-art clusters requiring neighboring cells remain deferred.

The current one-byte `Cell`, material, sprite/decal, UI, dirty-stream, and built-in 8x8
raster assumptions require a measured cross-cutting migration. SMC remains mandatory;
the migration cannot remove, bypass, demote, or materially erase its accepted performance
gains. Dependency selection, fallback fonts, raster dimensions, explicit cell syntax,
cache budgets, and parity details remain V1-4 Q1 research rather than unresolved scope.

## Clipboard paste into editor canvases — **Promoted to v1 roadmap V1-6 through V1-8**

**Wanted:** Paste clipboard text directly into applicable editor canvases, including
multi-line ASCII art and valid glyphs that are difficult or impossible to type with the
current input method. Pasting should preserve line structure and supported Unicode glyphs
without requiring an import-file detour.

When pasted content extends beyond the current canvas, grow the canvas transactionally to
fit it rather than silently clipping ordinary valid input. Define anchor/cursor behavior,
line-ending and tab handling, rectangular versus ragged rows, maximum dimensions,
unsupported/invalid text feedback, and whether any destructive overlap requires preview
or confirmation. Paste plus any automatic resize should be one undoable action, and an
allocation or validation failure must leave the canvas unchanged.

---

# Surface and map authoring

## Edit floor and ceiling materials — **Needs architecture/design**

**Wanted:** Select floors and ceilings and assign materials independently.

A conservative first model is per-cell wall/occupancy, floor material, and
ceiling material at fixed global heights. It requires:

- a new scene/map format and migration;
- authored floor/ceiling rendering instead of hard-coded colors;
- horizontal-surface ray hits and surface coordinates;
- new selection variants, commands, highlights, and tests;
- rules for empty cells, outdoors/no-ceiling, borders, tiling, orientation, and
  glyph scale;
- shared surface-coordinate conventions with decals.

Decide fixed heights first versus variable heights, and whether every cell must
have both planes. Do not smuggle slopes into this feature without a decision.

## Place and remove walls — **Ready in bounds; resizing needs design**

**Wanted:** Turn empty cells into walls and remove walls.

Use distinct place/remove commands rather than weakening the current occupied
wall-material command. Define initial material, player/spawn safety, attached
decal/object handling, lighting invalidation, and exact undo restoration.
Removing a wall also needs a rule for whether floor/ceiling surfaces remain.

## Expand and contract map borders — **Needs architecture/design**

**Wanted:** Extend borders while building and contract unused space.

This implies transactional reallocation of authored/derived arrays, explicit
world origin when expanding west/north, remapping coordinate-based content,
undo data for cropped cells/attachments, overflow/dimension limits, and format
support for dimensions/origin.

Decide automatic versus explicit resize, growth granularity, maximum size, and
whether contraction refuses, prompts for, or undoably captures occupied crops.

## Bounded map generation, verticality, and portals — **Plan written; 5 decisions open**

**Wanted:** large maps generated offline (bounded, pre-made, importable into the editor),
multiple biome-specific algorithms, and vertical worlds built from Doom-style tricks rather
than true 3D geometry.

**Record:** [`MAP_GENERATION_AND_VERTICALITY_PLAN_2026-11-06.md`](MAP_GENERATION_AND_VERTICALITY_PLAN_2026-11-06.md)
— constraint set, staged pipeline (base field → shape → validate → decorate → link levels →
serialise), progress/cancel options, the portal cut rule and what cannot be faked, per-biome
algorithm families, memory analysis, determinism, validation, editor integration, and a phased
plan (M0–M5). Roadmap position: **V1-G**, a cross-phase enabler.

**Decisions open (plan §10):**
1. ~~map-size caps~~ — **answered 2026-11-06:** ceiling raised to 2048×1024 with a RAM-derived
   load-time guard; the ceiling has landed, the guard's platform-memory placement is still open;
2. levels — one map per level linked by portals, or wait for a multi-level model;
3. seed provenance — sidecar `<name>.tscene.gen` or an in-format key block;
4. execution — sliced in the frame loop plus a CLI, threads only if ever justified;
5. placement — a `mapgen` module plus `tools/mapgen`, with a "New generated scene" chooser entry.

**Known non-goals:** runtime/streamed generation, a true multi-level model in one map, and
portal *rendering* (see-through to a non-adjacent space).

## Per-face wall materials — **Needs model/format design**

**Wanted:** Different north/south/east/west materials on one occupied wall.

This requires separating occupancy from four surface references, face-aware ray
lookup/serialization, placement defaults, hidden/internal face rules, and
migration of the current material to appropriate faces. Coordinate this with
floor/ceiling data rather than creating another temporary encoding.

## Spatial expressiveness, verticality, portals, and surface detail — **Needs design**

**Wanted:** Make spaces feel more vertically and materially expressive without assuming a
full engine rewrite. The current perspective and physics model is acceptable as a
foundation, but authors need better ways to create things like ductwork, bridges,
height-varied areas, and visually distinct sections of large surfaces.

Future design should evaluate minimal extensions before replacing the engine model:

- per-section or overlay material changes on walls/floors/ceilings;
- direct decal/surface-paint workflows for visual subdivision;
- object/sprite/portal-like visual tricks for bridges, grates, catwalks, ducts, and
  false openings;
- whether limited portals or linked views can provide enough vertical complexity while
  preserving the accepted heightfield, collision, and renderer constraints;
- how lighting, mirrors, collision, selection, save/load, and editor previews behave
  around these tricks.

The goal is more authored verticality and spatial richness, not a premature commitment
to sectors, voxels, stacked rooms, or unrestricted 3D geometry.

---

# Material and reusable asset authoring

## Create and edit materials — **Needs focused requirements/document design**

**Wanted:** Create materials; edit name, base/near/mid/far colors and distance
glyphs; preview; Save, Save As, or Discard.

Likely implications:

- `MaterialDocument` with owned working copy, dirty state, undo/redo,
  validation, atomic save, and discard;
- previews at multiple distances/light levels and glyph-picker UI;
- safe `AssetRegistry` refresh only after commit;
- new-ID, naming, rename, duplicate, delete, and missing-reference policies;
- dependency display for shared palettes/materials;
- resolution of the map `0..9` versus registry `1..255` mismatch.

Decide whether palettes stay shared, what “base color” means relative to three
color stops, whether shared-palette changes propagate live, deletion policy,
and whether asset history is independent from scene history.

## Material edit scope: shared/global versus individual/forked edits — **Needs product decision**

**Wanted:** Support both editing a material globally so every surface/object using that
material changes, and editing only the selected use without unexpectedly mutating every
other reference.

A possible model is an explicit mode toggle plus “individual edit creates/forks a new
material” behavior, but this needs design before implementation. Decide:

- how the UI communicates “editing shared material used by N things” versus “forking for
  this selection only”;
- naming/ID policy for implied new materials;
- undo/redo and Save/Discard boundaries when an edit touches both asset and scene data;
- dependency display and find-all-usages behavior;
- merge/deduplicate workflows for accidentally forked materials;
- how palette sharing, missing references, export validation, and external text edits
  interact with material forking.

## Paint/save/discard reusable decal assets — **Resolved (R5/R6)**

**Implemented (2026-08-13):** `DecalDocument` and material/document tooling
provide owned working copies, dirty state, undo/redo, validation, atomic save,
and discard (R5). Decal authoring is integrated into the unified editor's
surface-inspector Decals submenu: **Add decal…** creates a new reusable pattern
asset (editable columns/rows), refreshes the registry, and places a straight-up
canvas instance; edits persist through native Save/Open (R6).

Remaining polish: glyph-picker UI, multi-distance preview, brush painting tool,
and explicit missing-reference UI beyond repair mode.

## Paint decals directly on any surface — **Partly ready; direct surface painting open**

**Implemented baseline:** Placing/editing decals on wall, floor, and ceiling
surfaces uses one surface-local origin/tangent/bitangent model; art remains
surface-anchored, not camera-facing.

**Still open:** Direct paint-while-viewing with a brush footprint, clipping,
resolution/physical glyph size, seams/corners, cross-surface strokes, and whether
painting starts a named unsaved asset requiring Save/Discard.

## Spray/place saved decals — **Resolved (R6)**

**Implemented (2026-08-13):** Saved decal instances are placed on valid surfaces
by stable ID, moved/rotated/scaled via bounded inspector fields, removed with
confirmation, grouped into the same undo/redo history, and persisted in native
Save/Open. Missing-asset support geometry uses visible repair mode.

Remaining: grouped spray-stroke undo, overlap/layering UI, spacing/randomization,
edge wrapping, and capacity/performance policy beyond existing limits.

## Sprite and animation authoring — **R11 foundation verified; v1 depth promoted to V1-8 through V1-10**

**Implemented (2026-08-28/09-02):** `SpriteAsset` patterns render as decorative
camera-facing billboards (R11 I1: depth-tested against world geometry and
decals, lit through the per-channel light map, no collision/ray/light/mirror
effects). Scene v8 sprite placement/selection/persistence and undo/redo land
with R11 I2 (code present in commit `277fb3c`; automated verification and
bundled I1+I2 manual acceptance passed).

R11 I5 verifies per-instance time-based playback. I6 implements folder-only static
and animated assets, integrated painter frame authoring, circular neighbor previews,
FPS/loop editing, staged Save/Discard, and atomic folder replacement. Automated
gates, manual acceptance, and the final R11 boundary review passed on 2026-09-04.

Historical follow-up observations are tracked in
[`archive/r11/R11_PLANNED_ANIMATION_IMPROVEMENTS_2026-09-04.md`](archive/r11/R11_PLANNED_ANIMATION_IMPROVEMENTS_2026-09-04.md):

1. investigate subtle frame-change flicker/jitter without assuming its cause;
2. add a convenience option to insert a copied frame;
3. expose sprite search/create/edit from object/entity sprite workflows; and
4. research sprite stacking and multiple-angle sprites.

The v1 roadmap promotes all four outcomes: evidence-first flicker correction,
add-selected-frame-as-copy, object/entity sprite search/create/edit, and typed directional
and stacked sprite systems. Multi-angle sprites and sprite stacking are required, not
optional research: directional views communicate movement/facing, while ordered reusable
layers support appearance customization, equipment, and visible state changes.

Still phase-entry work: exact angle buckets/ties/fallback, composition and entity visual
assembly schemas, layer transforms/synchronization, solid/occluding behavior, and complete
mirror participation. Live staged **Current** preview with explicit **Saved** switching and
sprite-to-object/entity authoring are committed V1-8 outcomes.

Sprite-painter tool depth also remains open. Add selectable brush sizes, reusable default
shape stamps, crop-to-content, and an explicit way to create a new animation frame by
copying the currently selected frame. The copied-frame operation is already preserved in
the linked R11 follow-up and should not replace the option to add an empty frame without a
separate product decision. Define shape/stamp placement, crop behavior for blank content,
frame-size interactions, preview, undo/redo, capacity limits, and non-mutating failure
behavior before implementation.

The verified R11 baseline should not be treated as feature-complete entity/sprite
authoring. Future planning should address:

- whether free-standing placed sprites should remain a general authoring workflow or be
  replaced/limited by an entity/object-first model where sprites are visuals attached to
  typed entities;
- a project-wide invariant that **one glyph maps to one glyph** wherever glyph art is
  authored or rendered. The current 8×8 sprite start is useful, but sprite cells should
  not secretly represent larger multi-glyph sections when the editor presents them as
  glyph cells;
- variable sprite canvas dimensions so sprites can expand or contract beyond 8×8 while
  retaining bounded memory, validation, persistence, preview, and editor usability;
- per-sprite or per-entity scale, with clear separation among visual size, occupied cell,
  collision size, lighting/occlusion bounds, and editor handles;
- directional and multi-view sprites: front/back/side/top/bottom or angle buckets,
  deterministic camera/world-relative selection, fallback/mirroring, and interaction with
  animation phase;
- rotation-driven authoring previews: while rotating an entity/object non-permanently,
  the sprite editor could select the view corresponding to the current visual angle (for
  example editing the top sprite when previewing from above);
- rough editor UX spots around sprite/entity creation, search, assignment, and staged
  asset versus scene transaction boundaries.

## Attribute tagging and entity behavior model — **Major roadmap candidate**

**Wanted:** A dedicated data model for tags/attributes that can eventually define entity
behavior generally. The current object/entity baseline does not settle this vision.

This needs a focused discussion before implementation. Open questions include:

- Are tags plain labels, typed components, capabilities, traits, rules, or some
  combination?
- Can tags carry typed values, constraints, defaults, or editor metadata?
- Are user-defined tags allowed, and if so where is the project-level tag schema stored?
- Which systems consume tags: triggers, AI, inventory, procedural generation,
  conversations, UI filtering, collision, rendering, export validation, or runtime
  behavior?
- How do tags interact with broader object attributes, stats, abilities, skills,
  morality, knowledge, faction, personality, and ownership?
- What is the validation and missing-tag policy for saved scenes, exported projects, and
  runtime-loaded content?
- How do we avoid accidentally introducing an unrestricted scripting system before its
  requirements are understood?

This likely deserves its own future roadmap phase or decision record because it will
shape entities, triggers, AI, conversations, inventory, procedural generation, and game
runtime behavior.

## Reusable nested inspector submenu — **Resolved by R12**

R12 closeout implemented `ui_nested_inspector` for shared wrapped stepping, bounded
indented row formatting, and retained-depth Enter/Escape cursor transitions. Flow and
Menu controllers use the shared presentation helpers. This section remains only as a
historical note; new nested-editor UX work belongs under the UI design-system and
game-flow polish sections above.

---

# Lighting and environment

## Place, select, move, and delete point lights — **Resolved (R6) and spot lights (R10 I2)**

**Implemented (2026-08-27):** Point lights are authored, selected, moved,
edited, removed, and persisted with stable scene IDs, undo/redo, and explicit
capacity errors through scene v7 (R6/R10). Spot lights add direction, cone, and
falloff with a v6→v7 migration (R10 I2). Colored per-channel illumination and
alpha authoring landed in R10 I1.

## Edit brightness, radius, and color — **Resolved (R10 I1/I2)**

**Implemented:** Intensity/radius editing fits the current `Light`; negative
anti-light subtracts per channel. `Map.light_map` is now a per-channel
`LightLevel { red, green, blue }` (R10 I1), so colored lights tint surfaces,
clamp per channel `[0,255]`, and compose through translucent layers. Alpha scales
the emitted channel weight. See `R10_INCREMENT_I1/I2_IMPLEMENTATION_RECORD`.

## Other light types — **Research track (R10 I3 recorded, all DEFER/one REJECT)**

Potential types:

- direction: global direction and shadow policy — DEFER (R10 I3 P1);
- area: multisampling/approximation with explicit cost/quality target — discrete
  DEFER, analytic REJECT (R10 I3 P2);
- emissive material: material-light coupling and update/bake behavior — DEFER
  with a documented v8 upgrade path (R10 I3 P3).

Each deferred type needs a separate Q1 review and a paired
`make benchmark-colored-lighting` extension before implementation. No
implementation order is committed.

## Per-scene ambient light — **Ready for product planning; format-dependent**

**Wanted:** Adjust ambient light live and save it with the scene.

Separate global default from per-scene override; add an undoable validated
property; recompute lighting narrowly; define interaction with anti-lights,
colored lighting, and outdoor/no-ceiling areas.

## Particle effects, editor, and attachments — **Needs product and architecture design**

**Wanted:** Add a particle system and a dedicated particle editor with enough options to
design reusable effects rather than hard-coding each effect in gameplay code. Candidate
controls include emission shape/rate, lifetime, motion, spread, gravity or acceleration,
glyphs, colors and transparency, size/scale, animation over lifetime, looping/bursts,
preview, and deterministic randomization where repeatability matters.

Particles should be attachable to appropriate authored/runtime things, but the valid
attachment targets need a design discussion first. Evaluate at least entities/objects,
world positions, surfaces, lights, projectiles, and other particle emitters without
assuming that every target is sensible. Define local-versus-world transforms, emitter
lifetime when its owner is removed, save/load ownership, editor selection and preview,
collision/lighting interaction, visibility in mirrors, bounded particle counts, quality
fallbacks, and runtime performance budgets.

---

# Camera and vertical world

## Increase/remove look-up/down bound — **Bounded 2.5D policy approved**

Current `Camera.pitch` is a horizon offset clamped to `[-100,100]`, not an angle.

Two different features are possible:

1. Wider 2.5D look: grid-height-relative limit, controlled off-screen horizon,
   and projection safety audit.
2. True unrestricted pitch: camera Z/angular pitch and a replacement of current
   projection assumptions, coupled to vertical-world design.

**Decision 2026-07-30:** R0 uses option 1 with an exact
`[-viewport_rows, +viewport_rows]` bound derived from the active logical grid.
This remains a 2.5D horizon offset. True angular pitch was referred to R8 and,
per [`archive/r8/R8_DECISION_RECORD_2026-08-19.md`](archive/r8/R8_DECISION_RECORD_2026-08-19.md), remains deferred beyond R8 (not an R8
requirement).
See [`archive/r0/R0_GRID_RELATIVE_HORIZON_OFFSET_PLAN_2026-07-30.md`](archive/r0/R0_GRID_RELATIVE_HORIZON_OFFSET_PLAN_2026-07-30.md).

Define expected behavior near straight up/down, extreme-pitch safety, decal
coherence, sensitivity/inversion, and avoid calling a wider horizon offset
“unlimited pitch.”

## Alternate camera and presentation modes — **Future view-system track**

**Wanted eventually:** camera/view modes beyond first person, such as isometric,
third-person, directly overhead, tactics-style, or editor-only overview modes.

This is not immediate, but it affects sprite direction authoring, object rotation,
multiple controllable actors, mouse selection, procedural/overworld views, and export
runtime expectations. Future planning must decide which views are editor-only versus
player-facing, how they share scene/render data, and whether they require separate
asset views or collision/selection models.

## Verticality and inclined-surface climbing — **Decisions locked 2026-08-19**

**Wanted:** Raised/lowered areas and traversable ramps/slopes.

The verticality decisions are resolved in [`archive/r8/R8_DECISION_RECORD_2026-08-19.md`](archive/r8/R8_DECISION_RECORD_2026-08-19.md):

- continuous per-cell floor/ceiling heights (heightfield); ramps are cell-aligned
  only;
- movement scope: fall/gravity, step-up, jump, and head-clearance, with per-map
  runtime-tunable parameters as versioned scene data (ladders are deferred from
  R8);
- renderer stays a ray caster with per-column height math (no rewrite), the view
  composed through the horizon-offset pitch plus eye height;
- v4 → v5 heightfield migration reuses the proven v1 → v2 → v3 → v4 machinery.

R8 is **Verified (2026-08-21)**. The requirements/implementation plan
([`archive/r8/R8_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-19.md`](archive/r8/R8_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-19.md)) now specifies the schema
fields, limits, parameter validation ranges, editor workflows, and renderer
performance budget; the heightfield itself remains separate from the per-surface
floor/ceiling materials already authored in v3.

---

# Optical and advanced rendering

## Mirrors — **Research track; scoped in the R9 research plan**

**Wanted:** Reflective surfaces showing the world behind the viewer.

Needs reflected secondary rays/views, recursion limits, depth/occlusion,
reflected entities/lights/decals, reflectivity separate from occupancy, and a
performance strategy (bounded resolution/update rate or stylization). Decide
surface types, bounce count, mirror-facing-mirror behavior, visual quality, and
interaction with translucency/invisible geometry. Scoped as RQ4/P4 in
[`archive/r9/R9_OPTICAL_RESEARCH_PLAN_2026-08-21.md`](archive/r9/R9_OPTICAL_RESEARCH_PLAN_2026-08-21.md) (single bounded bounce first;
mirror-facing-mirror stays out of the prototype).

**Planar mirror projection — Closed (2026-09-16):** The 2026-09-12 change preserved
incoming camera Z and removed a row-dependent asymmetric defect, but reflected
projection still omitted camera-to-mirror depth and source-column correction. Reflected
continuation columns now retain the complete optical-path projection. A deterministic
near/mid/far fixture checks final cells against an analytic oracle across yaw, off-axis
columns, camera height, and fractional pitch without broadening one-bounce scope. Direct
wall/floor/ceiling continuation oracles and manual approach, retreat, and oblique-view
confirmation complete the closure evidence.

### Complete mirror contents — **Deferred renderer/scene integration**

Current mirrors are intentionally limited and do not show everything in the world. A
future complete-reflection pass should investigate showing sprites, decals, lights,
objects/entities, and other non-wall/floor/ceiling content while preserving depth,
occlusion, one-bounce or otherwise bounded recursion, deterministic fallback, and the
surface-render budget. This should be planned with sprite/entity mirror visibility,
decal overlay ordering, lighting cost, and quality-preset decisions rather than patched
one content type at a time.

## Translucent materials — **Needs design and renderer research**

**Wanted:** Visible surfaces such as glass through which the world remains seen.

Rays must continue after a translucent hit and collect ordered layers, followed
by back-to-front compositing or ASCII dithering/stipple. Separate opacity,
collision, light transmission, and selection behavior; bound layers per ray;
define ordering with decals/sprites/mirrors/highlights. Decide alpha versus
dither, colored transmission/refraction, and partially transparent glyph cells.

## Invisible materials/collision — **Needs explicit semantics**

Do not use one ambiguous “invisible” flag. Distinguish invisible solid
collision, invisible passable markers, editor-hidden geometry, ray-transparent
barriers, and light-blocking/passing geometry. The editor needs a reveal toggle
and highlight so hidden geometry remains editable. Scoped as RQ1/P1 in
[`archive/r9/R9_OPTICAL_RESEARCH_PLAN_2026-08-21.md`](archive/r9/R9_OPTICAL_RESEARCH_PLAN_2026-08-21.md), which requires ray, light, and player
interaction to be independent typed fields rather than one flag.

---

# Placed entities and gameplay authoring

## Objects, triggers, sprite animation, and spawn — **R11 baselines delivered**

**Implemented trigger baseline (R11 I3, verified):**
scene v9 owns bounded `enter_region` records with one closed typed action
(`set_flag`, `teleport_to_spawn`, or `toggle_light`), stable-ID selection,
reference validation, undo/redo, persistence, highlighting, and editor-Walk
runtime firing. R11 I4 verifies static `simple` object assets/instances, basic
player collision, and per-instance sprite selection. R11 I5 verifies strict
folder-backed sprite-animation data and time-based runtime playback; R11 I6 verifies
folder-only static/animated authoring in the integrated sprite painter. Planned
follow-ups are in [`archive/r11/R11_PLANNED_ANIMATION_IMPROVEMENTS_2026-09-04.md`](archive/r11/R11_PLANNED_ANIMATION_IMPROVEMENTS_2026-09-04.md). Broader object
attributes and trigger graphs/conditions/actions remain future work. Game-mode spawn expansion is
explicitly unnecessary/deferred because no game modes or mature testing workflow
exist; the single authored spawn remains the verified baseline.

## Dimensioned entities and material-driven shapes — **Needs entity/renderer design**

**Wanted:** Give entities explicit dimensions and allow an entity's visual representation
to be either a sprite or a material applied to simple geometry. Material-driven geometry
is intended for entities that should read like walls, floors, and ceilings rather than as
billboarded sprites—for example projectiles and some environment decorations.

Keep the primitive set deliberately small: rectangular prisms (including cubes through
equal dimensions), pyramids, spheres, and cylinders. Authors should be able to manipulate
these through dimensions, support limited deformation and rounded edges where practical,
and combine multiple primitives into one entity visual. Before implementation, define:

- whether dimensions separately control visual bounds, collision, selection, lighting,
  occlusion, and attachment points;
- primitive transforms, anchors, local coordinate conventions, and composition limits;
- how one or more materials map onto faces or curved/rounded surfaces using the existing
  glyph-material rendering style;
- whether composed primitives are one reusable shape asset, typed entity components, or
  another explicit bounded model;
- editor creation/manipulation, preview, undo/redo, persistence, migration, and validation;
- rendering and collision budgets, overlap/self-intersection rules, mirrors, transparency,
  and deterministic fallback for unsupported detail.

Do not turn this into unrestricted mesh modeling or a general-purpose 3D editor without a
separate product decision.

## Start Game native-scene migration — **Post-editor cleanup**

`APP_STATE_PLAYING` still loads the deprecated digit map plus legacy `WorldState`
path and therefore does not consume native `SceneDocument` data such as authored
cells, scene instances, or triggers. Do not maintain duplicate gameplay feature
sources while editor architecture is changing rapidly. After the editor feature
roadmap stabilizes, migrate Start Game transactionally to native-scene ownership,
define scene selection/failure fallback, reuse the pure entity/trigger session,
and remove the deprecated runtime loading path with compatibility evidence.

## Player/avatar parameters and spawn authoring — **Focused future feature**

**Wanted:** Edit the player spawn point, spawn facing direction, and player/avatar size
now that collision and vertical movement are meaningful authored concerns.

Future design should define whether these are per-scene, per-project, per-game-mode, or
per-controllable-actor fields. It must handle collision clearance, eye height, movement
parameter interaction, save/load migration, editor visualization, undo/redo, and runtime
fallback if a spawn becomes invalid after map or object edits.

## Long-horizon gameplay systems — **Large future pillars**

These ideas are intentionally not immediate implementation tasks. They should be kept in
the inventory because they influence data modeling, attribute tagging, export, UI, and
runtime architecture decisions.

### Multiple controllable actors and command models

Future games may need more than one controllable character for tactics games, RPG mind
control, clones, party control, or ordering NPCs. This needs decisions about active actor
selection, camera ownership, input routing, turn/real-time models, AI handoff,
possession, command queues, and save data.

### Behavior, personality, knowledge, morality, abilities, skills, and stats

Wanted eventually: rich data attached to entities describing personality, knowledge,
morality, abilities, skills, stats, relationships, and behavior. This likely depends on
the attribute tagging/entity behavior model and must remain typed, inspectable, and
validated rather than an uncontrolled script layer.

### Inventory system families

Various inventory models should be discussed later: slot/grid inventories, equipment,
containers, shared party inventory, weight/volume, key-items, crafting inputs, and
diegetic world storage. This depends on entity attributes, UI standards, input model,
export, and save data.

### Conversation system and classification/compression-inspired design

Wanted eventually: a flexible conversation system using an approach inspired by how
classification can be done with gzip-like similarity/compression ideas. This needs
research before design: identify intended authoring workflow, runtime determinism,
content format, testability, and how classification interacts with personality,
knowledge, morality, tags, quests, and localization.

### Overworld map system

An overworld or macro-map would be useful for games spanning multiple areas. Future
planning should decide whether it is a separate scene type, graph layer, tile/map asset,
procedural layer, or game-flow view, and how it connects to travel, encounters, exports,
and player-facing UI.

---

# Interface, UI, and menu authoring

## Unified-editor game-flow and UI/menu workspace — **R12 verified**

**Wanted:** Visually author interfaces, layouts, and menus instead of manually
editing text assets. Keep this as a workspace/submode inside the unified Editor,
not a return to disconnected application states.

**R12 I1 implemented (2026-09-04):** a separate versioned `FlowDocument` directed
graph owns Start/Scene/Menu nodes and named progression edges. Stable IDs, cycles,
reachability, strict parsing, transactional load, and atomic save are covered by the
focused runner. Editor canvas and application/runtime integration remain later
increments. See [`archive/r12/R12_I1_FLOW_DOCUMENT_DECISION_AND_PLAN_2026-09-04.md`](archive/r12/R12_I1_FLOW_DOCUMENT_DECISION_AND_PLAN_2026-09-04.md).

**R12 I2 implemented (2026-09-04):** a borrowed typed catalog validates Scene/Menu
asset names and declared outgoing ports, while a pure allocation-free runtime session
navigates the current node through explicit named edges. It owns no I/O, rendering,
input, time, trigger state, or editor behavior. See
[`archive/r12/R12_I2_REFERENCE_VALIDATION_AND_RUNTIME_PLAN_2026-09-04.md`](archive/r12/R12_I2_REFERENCE_VALIDATION_AND_RUNTIME_PLAN_2026-09-04.md).

**R12 I3 implemented (2026-09-04):** versioned Menu `UiDocument` data owns one
validated stable-ID Container/Text/Button tree, transactional load, atomic save,
dirty identity, and derived unique Button flow ports compatible with I2. Layout,
scale, sprite/style, rendering, and editor integration remain absent by design. See
[`archive/r12/R12_I3_UI_DOCUMENT_MENU_SCHEMA_PLAN_2026-09-04.md`](archive/r12/R12_I3_UI_DOCUMENT_MENU_SCHEMA_PLAN_2026-09-04.md).

**R12 I4 implemented (2026-09-04):** `UiDocument` v2 adds design dimensions,
parent-relative integer rectangles, independent Start/Center/End/Stretch anchors, and
bounded local per-item scale. The pure allocation-free resolver returns deterministic
parent/viewport-clipped geometry in document paint order. Version 1 migrates to
explicit safe defaults. No renderer/editor/global-scale coupling was added. See
[`archive/r12/R12_I4_RESPONSIVE_LAYOUT_DECISION_AND_PLAN_2026-09-04.md`](archive/r12/R12_I4_RESPONSIVE_LAYOUT_DECISION_AND_PLAN_2026-09-04.md).

**R12 I5 implemented (2026-09-04):** `UiDocument` v3 adds native colors,
fill/border glyph policy, text alignment, default visibility, and numeric sprite visual
references. Transient focused/pressed/disabled/visible state stays outside persistence;
state colors use one caller theme and Buttons retain non-color-only markers. The pure
headless adapter renders into `UiCanvas`, clips all work, and preserves the destination
on invalid state or missing assets/materials. See
[`archive/r12/R12_I5_VISUALS_AND_HEADLESS_RENDER_PLAN_2026-09-04.md`](archive/r12/R12_I5_VISUALS_AND_HEADLESS_RENDER_PLAN_2026-09-04.md).

**R12 I6 implemented (2026-09-04):** a pure allocation-free interaction module owns
only focused stable element identity and provides painter-order clipped hit testing,
wrapped next/previous focus, deterministic four-way directional navigation, transient
hidden/disabled eligibility, pointer focus, and typed Button activation results. It
does not route input or invoke flow transitions. See
[`archive/r12/R12_I6_INTERACTION_SEMANTICS_PLAN_2026-09-04.md`](archive/r12/R12_I6_INTERACTION_SEMANTICS_PLAN_2026-09-04.md).

**R12 I7 implemented (2026-09-09):** native scene v11 adds unique bounded named
`exit_flow` trigger ports. Trigger ticks expose one deterministic borrowed exit request,
and `scene_flow_adapter` derives borrowed Scene entries for complete I2 catalog/graph
validation. The existing trigger inspector can designate an exit with a conventional
unique name. It does not invoke flow navigation or load targets. See
[`archive/r12/R12_I7_SCENE_EXIT_AND_CATALOG_ADAPTER_PLAN_2026-09-09.md`](archive/r12/R12_I7_SCENE_EXIT_AND_CATALOG_ADAPTER_PLAN_2026-09-09.md).

**R12 I8 implemented (2026-09-09):** `flow_binding` accepts either an I6 Button
activation from a Menu node or an I7 scene-exit request from a Scene node, advances a
local copy of `FlowRuntimeSession`, and returns a typed borrowed Scene/Menu target
request. Wrong source types, invalid activations, missing ports, invalid graphs/sessions,
and Start targets preserve session/output. It performs no loading, rendering, input,
I/O, or app/editor mutation. See
[`archive/r12/R12_I8_FLOW_BINDING_ADAPTER_PLAN_2026-09-09.md`](archive/r12/R12_I8_FLOW_BINDING_ADAPTER_PLAN_2026-09-09.md).

**R12 I9 implemented (2026-09-09):** `UiMenuRuntime` borrows authored Menu/render/flow
dependencies and owns only transient focus, pressed, visibility/disabled, viewport, and
active state. Explicit logical keyboard/pointer commands route through I6, rendering
routes through I5, and successful release hands a typed target through I8 before the
host deactivates. Menu reset/switch is transactional; the host performs no SDL event
processing, loading, persistence, or app/editor mutation. See
[`archive/r12/R12_I9_AUTHORED_MENU_RUNTIME_HOST_PLAN_2026-09-09.md`](archive/r12/R12_I9_AUTHORED_MENU_RUNTIME_HOST_PLAN_2026-09-09.md).

**R12 I10 implemented (2026-09-09):** non-repeating `G` opens a game-flow workspace
inside the unified editor. A headless staged controller presents Start/Scene/Menu nodes,
outgoing named connections, target choices, and dirty-close choices. Existing edge
targets can be rewired with bounded Ctrl+Z/Y history and atomic Ctrl+S/Save As/Discard.
First entry loads `<asset-root>/game.flow` transactionally; malformed input preserves a
recoverable retained graph. Scene editor state, app menus, and target loading remain
separate. Focused controller/editor tests, full sanitizers, optimized aggregate tests,
strict application build, and smoke pass. See
[`archive/r12/R12_I10_GAME_FLOW_WORKSPACE_PLAN_2026-09-09.md`](archive/r12/R12_I10_GAME_FLOW_WORKSPACE_PLAN_2026-09-09.md).

**R12 I11 implemented (2026-09-10):** `FlowProjectCatalog` transactionally composes
validated direct-child `scenes/*.tscene` and authored-game `menus/*.tui` documents and
their exported ports without scanning application UI assets. The `G` workspace displays
unconnected ports and absent catalog assets, supports atomic add+connect, connect/rewire,
and safe edge/node removal, and records each operation in bounded whole-document history.
See [`archive/r12/R12_I11_PROJECT_CATALOG_AND_GRAPH_CONSTRUCTION_PLAN_2026-09-10.md`](archive/r12/R12_I11_PROJECT_CATALOG_AND_GRAPH_CONSTRUCTION_PLAN_2026-09-10.md).

**R12 I12 implemented (2026-09-10):** non-repeating `Ctrl+U` opens the first visual
authored-game Menu workspace. It discovers `menus/*.tui`, transactionally opens or creates
Menu documents, renders a responsive I4/I5 preview beside a stable-ID hierarchy, edits
non-root x/y/width/height/scale with bounded undo/redo, and supports atomic Ctrl+S plus
dirty-close Save/Discard/Cancel. **2026-10-19:** the presentation is preview-first on the
workbench palette: shared panels for hierarchy/properties/actions, the preview framed and
right-aligned in the visible region with no material on it, and a windowed single-row property
list on short frames. See [`reviews/2026-10-19-editor-workspaces.md`](reviews/2026-10-19-editor-workspaces.md).
See [`archive/r12/R12_I12_VISUAL_MENU_WORKSPACE_PLAN_2026-09-10.md`](archive/r12/R12_I12_VISUAL_MENU_WORKSPACE_PLAN_2026-09-10.md).

**R12 I13 implemented (2026-09-11):** `E` opens typed element actions in the visual Menu
workspace. Containers create deterministic Container/Text/Button children with useful
default geometry; Text/Button content and unique Button flow ports edit through staged
text; Backspace confirms non-root subtree removal. Generalized bounded snapshot history
restores exact documents and hierarchy selection. See
[`archive/r12/R12_I13_MENU_ELEMENT_AUTHORING_PLAN_2026-09-11.md`](archive/r12/R12_I13_MENU_ELEMENT_AUTHORING_PLAN_2026-09-11.md).

**R12 I14 implemented (2026-09-11):** non-root elements use staged validated rename,
cycle-safe Container reparenting, and adjacent Move Earlier/Move Later actions. Adjacent
moves exchange complete sibling subtrees in painter order; exact bounded history preserves
stable selection, and save/reload preserves names, parents, and order. See
[`archive/r12/R12_I14_MENU_HIERARCHY_STRUCTURE_AUTHORING_PLAN_2026-09-11.md`](archive/r12/R12_I14_MENU_HIERARCHY_STRUCTURE_AUTHORING_PLAN_2026-09-11.md).

**R12 I15 implemented (2026-09-11):** typed scrolling Menu properties expose existing v3
anchors, Native/Sprite mode and Sprite ID, Text/Button alignment, authored foreground/background
channels, fill/border enablement and printable glyphs, and default visibility. Root visual-only
editing, typed row availability, exact history, preview updates, and persistence are covered.
See [`archive/r12/R12_I15_MENU_VISUAL_PROPERTY_AUTHORING_PLAN_2026-09-11.md`](archive/r12/R12_I15_MENU_VISUAL_PROPERTY_AUTHORING_PLAN_2026-09-11.md).

**R12 I16 implemented (2026-09-11):** the responsive preview accepts topmost visible pointer
selection, live drag move, and bounded resize from a visible bottom-right `+` handle. SDL
letterbox conversion occurs at the application edge; the headless workspace owns preview-local
cells, one temporary exact snapshot, one-command release, and exact Escape cancellation. See
[`archive/r12/R12_I16_MENU_POINTER_CANVAS_AUTHORING_PLAN_2026-09-11.md`](archive/r12/R12_I16_MENU_POINTER_CANVAS_AUTHORING_PLAN_2026-09-11.md).

**R12 I17 implemented (2026-09-11):** Preview Settings provides session-only 40×15,
60×20, and 80×25 base resolutions and 100%, 125%, 150%, and 200% UI scales. `Tab` enters
runtime-like testing through the existing `UiMenuRuntime`; arrows and pointer input use runtime
focus/press/activation semantics. A copied project catalog overlays the staged Menu ports before
validation, stale references are diagnosed, and successful activation displays a copied typed
target as `reported only`. No target loads, app state changes, `game.flow` rewrites, or
cross-document history are introduced. See
[`archive/r12/R12_I17_MENU_PREVIEW_VALIDATION_RUNTIME_TEST_PLAN_2026-09-11.md`](archive/r12/R12_I17_MENU_PREVIEW_VALIDATION_RUNTIME_TEST_PLAN_2026-09-11.md).

**R12 correction verification complete (2026-09-11):** the first manual review found history
exhaustion, masked Edit-preview colors, nested Actions-depth leakage, and a non-representative
checked-in flow. Histories now retain the newest 32 while accepting further edits; Edit shows
authored colors; direct Actions exits pop shared depth; typed Flow diagnostics explain failures;
and checked-in flow is Start → `main_menu` → `testscene`. Optimized aggregate, strict `make check`,
ASan/LeakSanitizer, UBSan, canonical eight-mode matrix, conflict rejection, smoke, legacy/current
guards, focused suites, and automated Q4 pass. The shared `ui_nested_inspector` owns common
wrapped navigation, bounded nested rows, and real parent/child Enter/Escape cursor transitions
used by Flow and Menu. At that checkpoint cppcheck was missing and installed Valgrind could not
produce evidence because it terminated with `SIGILL` during dynamic-loader startup. The final manual verification is
recorded in `reviews/2026-09-11-roadmap-r12-closeout.md`.

**Deferred authored-Menu pointer activation:** Test-mode keyboard focus and Enter activation are
the accepted R12 path. Display-backed pointer clicking did not activate Buttons reliably in the
second manual retest and is explicitly deferred by the user. A later scoped increment should
trace application-edge pointer coordinates through `UiMenuRuntime` down/up semantics, add a
real-window regression where practical, and make authored Buttons reliably clickable without
changing report-only target behavior. This is not an R12 verification blocker.

**Second-retest corrections verified (2026-09-11):** hierarchy rendering and Up/Down share a
derived parent-first projection while persisted painter order stays unchanged. Checked-in
`main_menu` adds an unconnected `extra_menu` port, making `main_menu.extra_menu → testmenu` an
obvious valid atomic Flow operation. Focused suites, optimized aggregate, strict `make check`,
ASan/LeakSanitizer, UBSan, isolated eight-mode matrix/conflict rejection, production build,
smoke, and guards pass. The user manually verified the remaining hierarchy and Flow display-backed
checks on 2026-09-11, so R12 is Verified and closed.

Likely capabilities:

- `UiDocument` with dirty state, undo/redo, atomic Save/Save As/Discard;
- canvas hit testing, highlight, drag, resize, reparent, order, duplicate/delete;
- hierarchy/tree and property inspector;
- text, color, dimensions, borders, focus order, actions, substitutions, and
  parent relationships;
- action/reference validation;
- preview across logical resolutions and UI scales;
- keyboard and pointer authoring with visible focus.

Implemented R12 capabilities include supported logical-resolution/UI-scale preview choices,
runtime-like Menu test mode, current action/reference diagnostics, and typed target request
reporting. Target loading remains intentionally out of scope. The compatibility boundary is
implemented: R12 does not discover, replace, or self-edit the application-owned editor shell,
`ui_layouts`, `ui_elements`, or application menus.

## Game-flow editor polish and graph UX — **Node graph landed 2026-10-19; interaction depth open**

**2026-10-19 (interaction pass):** the three-screen `NODES → CONNECTIONS → TARGETS` descent is
gone. The workspace opens on the selected node's `INSPECTOR` (`Left/Esc` shows `NODES`), with its
`PORTS` and explicit `ACTIONS`
(`+ Add node…`, `- Remove node`); `Enter` acts on the focused row, `Backspace` disconnects the
focused port, and the destination choice is a centred `CONNECT '<port>' TO…` modal with a
`NEW FROM CATALOG` section. `A` is the same Add action (first free port, first absent catalog
entry preselected) and refuses with a header reason when every port is connected. The graph
stays on the right, highlights the focused port's edge, and the inspector row owns the moving
perimeter. Short (200%-scale) viewports omit the key row, window the inspector around its focused
row, keep `A=add` in its hint, and constrain dialogs below the header. The R12 controller,
mutations, history and Save/Discard behaviour are unchanged; new controller entry points
(`flow_workspace_begin_connection`, `flow_workspace_begin_add_node`,
`flow_workspace_disconnect_port`, `flow_workspace_free_port_count`) expose the inspector
operations. Node positions are derived, not persisted. A click on a node box in `NODES` selects that
node (2026-11-06); the painter and the pointer host take the graph's first row from the same panel
planner, so a click lands on the box it points at. Remaining: pan/follow controls for very long
chains, dragging nodes, and native visual acceptance. The rest of this item stays as recorded
below.

R12 provides a solid foundation for authored flow data, validation, staging, catalog
composition, and safe graph mutations, but the current game-flow editor is still
barebones and rough. Treat polish as its own future item rather than assuming the verified
foundation is a finished authoring experience.

Future planning should address:

- graph readability, node/edge/port visual hierarchy, and compact status display;
- smoother create/connect/rewire/remove workflows;
- mouse-driven node graph operations with keyboard equivalents;
- clearer validation feedback for unreachable nodes, stale ports, missing assets, and
  impossible mutations;
- reducing menu depth and excessive wording while keeping errors actionable;
- visual distinction among Start, Scene, Menu, generated/catalog choices, and staged
  unsaved state;
- integration with the broader UI design-system, motion, sound-cue, and pointer-model
  tracks;
- eventual typed target loading/application flow invocation once runtime ownership and
  failure fallback are defined.

## Project export and playable game packaging — **Future major feature**

The editor exists to let users create their own games, but completed-project export has
not been designed. Future planning should define what it means to export a project and
what validation must pass first.

Open questions:

- export as a folder, archive, data pack, player-only runtime, or standalone build;
- what project manifest records scenes, flow graph, menus, assets, settings, required
  engine version, and startup behavior;
- whether generated content is baked, generated at runtime, or both;
- missing-reference, unused-asset, compatibility, and migration policies;
- whether exported games include editor-only data, debug metadata, or only runtime data;
- platform assumptions, deterministic loading, and failure fallback.

## Scene templates and procedural generation — **Future major feature / research track**

**Wanted:** Avoid forcing authors to start every scene from scratch. Support reusable
scene templates and procedural generation with visible, adjustable constraints.

Possible scopes:

1. **Scene templates:** mission-type, biome-type, gameplay-mode, or visual-style presets
   that preselect dimensions, movement parameters, materials, lighting, spawn defaults,
   flow/menu hooks, and starter assets.
2. **Static one-time generation:** generate a scene in the editor, then let the user
   fine-tune it manually as ordinary authored data.
3. **Dynamic/runtime generation:** generate on load or during play, with rules for how
   generated scenes participate in or bypass the authored flow graph.
4. **Parameter constraints:** expose user-editable bounds so generation stays within
   expected mission/biome/layout/material constraints.
5. **Example-based generation research:** investigate whether an authored example scene
   can seed or infer parameters for generating similar scenes.

This is a large future track. It depends on export, data-first project manifests,
attribute tagging, material/template libraries, validation, and performance safeguards.

---

# Benchmark and performance methodology

These are deferred engineering-quality follow-ups, not product priorities.

## Statistics and warm-up — **Needs focused planning**

- [ ] Add p95/p99 telemetry instead of relying on a single-outlier heuristic.
- [ ] Define/exclude a reproducible warm-up from average and worst collection.
- [ ] Emit measured/effective FPS in benchmark JSON.
- [ ] Record sufficient run context for reproducible comparison.

This needs bounded sample storage or online percentiles, explicit frame
inclusion rules, output tests, and documented spike interpretation.

## Separate acceptance and stress workloads — **Needs focused planning**

- [ ] Keep raycast acceptance and full-change stress as distinct targets/results.
- [ ] Decide whether stress has informational thresholds or a separate hard gate.
- [ ] Preserve comparable baseline, stream, dirty-cell, and other mode results.

## Build-mode/result regression tests — **Deferred**

- [ ] Test `ideal`, `pass_minimum`, allocation failure, and performance failure.
- [ ] Test default stream, explicit no-tracker, explicit alternatives, and
      conflicting-mode rejection.
- [ ] Preserve `USE_NO_STATE_TRACKER=1` as the intentional baseline.

---

# Documentation maintenance

- [x] Establish the stable editor requirements/regression contract.
- [x] Preserve historical evidence and mark superseded records clearly.
- [x] Expand this file into the requested non-authoritative idea inventory.
- [ ] Give active features scoped requirements/plans; do not treat this file as
      an executable specification.
- [ ] Update the stable contract, tests, and `README.md` together when accepted
      behavior changes.
- [ ] Preserve completed ideas or archive their decision/history; do not silently
      erase why they existed.
- [ ] Track the UI Scene migration under
      [`UI_SCENE_SYSTEM_AND_EDITOR_IMPLEMENTATION_PLAN_2026-09-17.md`](UI_SCENE_SYSTEM_AND_EDITOR_IMPLEMENTATION_PLAN_2026-09-17.md);
      promote slices to `FEATURE_ROADMAP.md` when they enter Active status.

## Adding an idea

Record: desired outcome; relevant current constraints; likely model, format,
renderer, editor, and test implications; shared dependencies; unresolved product
or architecture decisions; and planning readiness. Do not assign implementation
order here. Promote understood dependencies into `docs/FEATURE_ROADMAP.md` only
after discussion; keep unsequenced and rejected/deferred ideas here for context.
