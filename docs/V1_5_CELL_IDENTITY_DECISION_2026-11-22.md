# V1-5 Cell Identity Decision — 2026-11-22

## Status and authority

**Decided.** This record answers one question — what number identifies a glyph inside a cell, and can
that number live in the 64-bit word change detection is built on — and settles the representation
part of V1-5 required outcomes 1 and 5. It changes no production code yet; the implementation order
is below, and the phase exit gate is unchanged.

It supersedes the assumption (held in the handoff until now) that the migration required a 128-bit
state word. It does not supersede anything in
[`V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md`](V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md): the
8×8 cell, the no-fallback rule and the sentinel all stand.

## The question, in plain terms

Every frame, per cell, the SMC trackers compute a **fingerprint** — glyph + foreground + background
packed into one `uint64_t` — and compare it with the previous frame's to decide whether that cell
must be redrawn. 41,600 cells, 120 times a second.

**If two different glyphs produce the same fingerprint, the screen keeps stale pixels and nothing
reports an error.** So the fingerprint's width is a correctness constraint, not a preference. Today
it uses 8 bits for the glyph and 56 of its 64 bits overall. A codepoint above 255 cannot travel.

## Facts this rests on (measured 2026-11-22)

| Fact | Value | Source |
|---|---|---|
| Cell glyph field | `uint8_t` | `src/grid.h` |
| State bit budget | 8 glyph + 24 fg + 24 bg = **56 of 64** used | `src/smc_state_tracker.h` |
| Why 64 and not more | "Packed into a `uint64_t` so the SMC batch path can use the 8-byte fixed-size kernel" | same header |
| `sizeof(Cell)` | **9 bytes** (`SDL_Color` is four `Uint8`s, alignment 1) → 10 with a 16-bit glyph: **+41 KB per full screen** | compiled probe |
| Direct `cell->glyph` reads | 15, across 10 files | grep, corrected from an earlier bad estimate |
| `grid_set(..., uint8_t glyph, ...)` call sites | 87, across 13 files | `src/grid.h` + grep |
| `ui_canvas_set` call sites | 45 | grep |
| Scene file impact | **none** — scenes store decals and objects, not cells | `src/scene_format.c` |
| `glyph_block_cache` key | already `uint32_t glyph_id` | `src/glyph_block_cache.h` |
| Faces in scope | Misaki (JIS X 0208), unscii-8 (BMP + PUA): **both inside the BMP** | the manifest example |
| Real fonts that are not | Noto Sans Mono **92** codepoints above U+FFFF (first U+10780); Noto Sans Symbols2 **1,608** | compiled FreeType probe |

The last row matters: this is not a hypothetical ceiling. Ordinary fonts a user might own cross it.

## The decision

**D1 — The cell holds a 16-bit codepoint.** `Cell.glyph` becomes `uint16_t`, carrying U+0000–FFFF.
65,536 values, which covers every face currently declared and every BMP language. Adopted over a
32-bit identity because 16 + 24 + 24 = **exactly 64**, so the fingerprint keeps its width and the SMC
batch kernel keeps its shape.

**D2 — `CellState` stays one `uint64_t`.** New layout: glyph bits 0–15, fg 16–39, bg 40–63. No
reserved bits. The eight tracker modes keep an 8-byte compare; the parity gate re-runs because the
*field offsets* changed, not because the word did.

**D3 — Reserved values, since there is no separate `kind` field.**

| Value | Meaning |
|---|---|
| `0x0000` | blank (nothing drawn; the cell's own background still paints) |
| `0xFFFF` | transparent (do not paint at all; let the layer beneath show) |
| any other | a codepoint; **missing is derived at draw time** by `glyph_source`, never stored |

**D4 — The ceiling is loud. This is a first-class part of the decision, not a note.** A user who
loads a font with glyphs above U+FFFF must never be left hunting for a "broken" character:

- **Conversion reports every excluded codepoint.** `fontconv --convert` counts them, names the first
  several with their codepoints, and prints a delimited banner stating the cell holds 16 bits and that
  those glyphs were left out. The set is still written — a font with 92 astral glyphs is still worth
  using for its 3,400 BMP glyphs — so the warning is loud, not fatal.
- **A set containing a codepoint above the ceiling is invalid.** `glyph_set` rejects it with
  `GLYPH_SET_CODEPOINT_TOO_WIDE` and the offending entry index and codepoint. Such a set cannot have
  come from the converter, so its presence means corruption or a stale format, and neither should be
  read as ordinary data.
- **The renderer cannot silently drop one**, because a document cannot store one: the failure happens
  at conversion, where it can be explained, rather than at draw time, where it would look like an art
  bug. A codepoint above the ceiling that somehow reaches `glyph_source` resolves to the sentinel,
  which is already conspicuous.

**D5 — The astral escape hatch is an indirection table, specified now so deferral is cheap.** If rare
CJK extensions or emoji ever become required, a document gains a table (like a palette) mapping small
indices to 32-bit codepoints, and the cell stores an index with a reserved marker. That changes one
field's *meaning*, not the word width. Nothing is built for it today.


## Rejected options, and why

| Option | Why not |
|---|---|
| **32-bit codepoint, 128-bit state** | Exact for all Unicode, but 8 + 24 + 24 becomes 80 bits, so the state doubles to two words: the SMC batch kernel changes shape, all eight tracker modes, stream signatures and checksums re-baseline, and the state buffer goes 332 KB → 665 KB. Real cost for characters neither declared face contains. Kept as the direction D5 defers to. |
| **Handle into a glyph registry** | The 8-bit form allows **256 distinct glyphs on screen**; one screenful of Japanese exceeds that immediately, and a dropped glyph looks like an art bug. A 12-bit handle fits (60 of 64 bits) but needs a registry, a generation rule, and a defined behaviour for the frame that overflows the table — strictly worse than 65,536 direct values in the same word. |
| **Shrink colours to RGB565 to buy 16 bits** | Bands the gradients that are the entire look. The palette is locked; colour is the medium, not decoration. |

## What this changes, in order

1. **`glyph_set`** gains the ceiling invariant and its named rejection (independent of the cell; can
   land first and is worth doing first, because the converter must not emit what the reader refuses).
   — **landed 2026-11-22**, with `GLYPH_SET_CODEPOINT_TOO_WIDE` naming the entry, and the converter's
   enumerated banner verified against Noto Sans Mono (92 excluded) and a hand-crafted astral entry.
2. **`fontconv --convert`** stops emitting above-ceiling glyphs, counts them, and prints the banner.
   — **landed** with step 1.
3. **`grid_set` / `ui_canvas_set`** take a wider glyph; 132 call sites re-typed, each read for intent
   (some pass `' '`, some `0`, some a literal). — **landed**: both take `uint16_t`, and the call sites
   needed no edits because widening an argument is implicit; the ones that needed attention were the
   places that *read* a glyph back into an 8-bit local, and those turned out to be `PatternCell` (decal
   and sprite art), a different type that this decision does not touch.
4. **`Cell` + `CellState`** field widths and offsets, `pack_cell_state`, and every reader of the packed
   bits. — **landed**: `Cell.glyph` is `uint16_t`, `Cell` is 10 bytes, `pack_cell_state` puts the
   codepoint at bit 0 and the colours at 16/24/32/40/48/56. The repository already guarded the layout
   with `CASSERT`s on `offsetof(Cell, fg) == 1` and `sizeof(Cell) == 9`; those fired the moment the
   field changed, which is exactly what they were for, and now assert 2/6/10.
5. **Streams, checksums, block codec, capture/restore**, then `make matrix` re-baselined.
6. **`glyph_source`** and the renderer draw from a codepoint rather than a byte, calling the resolver
   that already exists.

## Verification of steps 3–4, as actually run (2026-11-22)

| Gate | Result |
|---|---|
| `make` (app, all sources) | `BUILD=0` |
| `make standards` | clean |
| `make test` | **88 suites, `TEST=0`**, zero failures |
| `make matrix` (8 tracker modes, each a **clean rebuild**) | **`MATRIX=0`**, all eight pass |
| `build/test-smc-state-tracker` | 2/2, including the new packed-layout test |

One caveat worth recording, because it is a property of this build system rather than of the change:
this Makefile lists source files explicitly and does not track header dependencies, so `make test`
after a header change rebuilt only the runners whose rules name the changed `.c` files — 18 of them.
`make test` was therefore *incomplete evidence* on its own. `make matrix` closes that gap by
`make clean`-ing between modes, so its eight passes are clean-rebuild passes.



Steps 1–2 are additive and reversible on their own: the set format gains an invariant before the cell
gains a field, so nothing downstream depends on them yet. Step 3 onwards is one change that must land
with step 4 — a widened `grid_set` that still packs an 8-bit state would be silently wrong for every
codepoint above 255, which is exactly the class of bug this record exists to prevent. Migrate the
reference renderer path before the optimised paths, per V1-5's existing rollback point.

## Not decided here

- **Width classes (`B2`)** — unchanged; a 16-bit codepoint says nothing about double-width cells.
- **Whether `0xFFFF` transparent is needed by any current feature.** It is reserved so that the first
  feature needing it does not need a format change; if nothing needs it, reserving it costs one value.
- **The indirection table's shape (D5)** — specified as a direction, not a format.
- **Bidi, segmentation, shaping** — V1-6 and D4/D5 of the font decision; untouched.

## Risks, stated plainly

- **16 bits is a ceiling someone will hit.** That is why D4 exists and why the warning is enumerated
  rather than summarised: the failure mode this record is designed to avoid is a user debugging a
  missing glyph that was excluded for a reason nobody told them.
- **Step 3 is mechanical but not trivial**: 132 call sites, and the interesting ones are the ones
  that pass a literal instead of a cell's value.
- **The parity gate will move.** Field offsets change inside the same word, so SMC output should be
  identical in behaviour and different in bytes; any behavioural difference is a real defect, not a
  re-baseline.

## Exit-gate mapping (V1-5)

| V1-5 requirement | Effect of this record |
|---|---|
| 1. Typed glyph reference, explicit blank/absent/missing | **decided** — 16-bit codepoint, reserved values, missing derived |
| 4. Bounded generation-aware caches | unblocked: cache keys can now carry a real identity |
| 5. Cell, dirty compare, packed state, streams, checksums, adapters | **decided**, implementation ordered above |
| 6. Logical cells, UI scale independence | unaffected in principle; must be re-tested after step 4 |

