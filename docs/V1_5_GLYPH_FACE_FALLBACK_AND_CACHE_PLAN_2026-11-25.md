# V1-5 Glyph Face, Fallback, and Cache — Plan and Amendments — 2026-11-25

## Provenance and status

**Proposed. Awaiting owner sign-off.** Nothing in this document is implemented; the tree is unchanged
by it.

This record answers three questions that V1-5 ran into and left open, and it amends two decisions in
[`V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md`](V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md) that
were overtaken on 2026-11-22 without anyone saying so. It is written as a **Q1 plan** under
[`FEATURE_ROADMAP.md`](FEATURE_ROADMAP.md) §"Q1 — Design readiness", because V1-5 has no focused
plan document of its own; the cell-identity decision record has been standing in for one.

Authority after sign-off:

| Document | Relationship |
|---|---|
| [`V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md`](V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md) | **Amended** here: D7's face slot (see D-A), D8's colour naming (see D-C). Everything else stands. |
| [`V1_5_CELL_IDENTITY_DECISION_2026-11-22.md`](V1_5_CELL_IDENTITY_DECISION_2026-11-22.md) | **Extended**, not amended: D1/D2 (16-bit codepoint, exact 64-bit state) are the constraint this plan is built on. |
| [`FEATURE_ROADMAP.md`](FEATURE_ROADMAP.md) | Sequencing authority; V1-5 required outcomes 4 and 6 are what this plans. |

## The three questions, in plain language

1. **Is one font on screen at a time the product?** If a document uses one face for art and another
   for text, does the file record which font drew what, or is "the current font" just a global switch?
   (`V1-5` currently has a global switch that D7 never intended.)
2. **Which increment comes next?** Finish verifying what is built, teach the converter to read
   bitmap font descriptions, or build the cache requirement 4 asks for?
3. **What happens when the installed font lacks a character?** The policy is decided — it must be
   loud. The code does not implement it for ASCII, and the diagnostic it promises does not exist.

## What the record says versus what the code does

These are not new discoveries; they are the reason this plan exists.

| Claim in a decision record | What the tree actually does |
|---|---|
| D7: identity is a **packed 32-bit `GlyphRef`** (2-bit kind, 4-bit face slot, 26-bit index) | `Cell.glyph` is a 16-bit codepoint; **no `GlyphRef` type exists anywhere in `src/`** |
| V1-4 §Consequences: "**`Cell` grows to 4 bytes of glyph**" | `Cell` is 10 bytes: 2 glyph + 2 pad + 6 colour |
| V1-5 preamble: *"does not supersede anything in `V1_4`"* | V1-5 D1/D3 superseded D7 outright — "no separate `kind` field", no face slot |
| D8 / V1-4 exit table: sentinel is "red background, **purple** foreground" | `glyph_source.h` implements **magenta** `#FF2BD6` |
| `ERROR_CATALOG.md` `TSG-ASSET-ENV-0001`: detection is "a cell's **`GlyphRef`**" | that type does not exist |
| D8: "**No fallback at all**" | `glyph_source_resolve` falls back to the built-in face for every codepoint `< 0x80` |
| `TSG-ASSET-ENV-0001` status "Planned (V1-5)" | never emitted anywhere in `src/` (consistent with Planned, but unimplemented) |
| Roadmap V1-4 status: "the 32-bit `GlyphRef` identity" | as above — stale |

## Facts this rests on

Measured on this host, 2026-11-25, from the tree as it stands.

| Fact | Value | Where |
|---|---|---|
| `CellState` budget | 16 glyph + 24 fg + 24 bg = **64 bits exactly** | `smc_state_tracker.h` |
| Adding a 4-bit face slot | 68 bits — **does not fit**; would force a 128-bit state or a 12-bit codepoint | arithmetic |
| `Cell` size | 10 bytes, guarded by `CASSERT`s | `renderer.c` |
| Set format | one set per face; **ascending codepoints**; `GLYPH_SET_DUPLICATE_CODEPOINT` is a named rejection; `provenance` = sha256 over that face's declared file digests | `glyph_set.h` |
| Draw sites resolving glyphs | 3 — `renderer.c:541`, `renderer.c:624`, `ui_compositor.c:144` | grep |
| Global source | `g_active_source`; set at `app_resources.c:35` (set) and `:67` (NULL); read at `glyph_paint.c:28`, `glyph_block_cache.c:82` | grep |
| Face's own canvas | every `UiLayer` owns a `const UiCanvas *`; max 16 layers | `ui_compositor.h` |
| Layer composition output | **pixels + `touched_cells`**, i.e. resolved at compose time | `ui_compositor_compose` |
| Layer ownership | renderer draws world grid *and* composes UI layers in one call | `renderer_draw_layers` → `renderer.c:668` |
| Layers as an authored asset | **do not exist** — every layer list is built in C at 5 sites (`app.c` ×2, `ui_workbench_runtime.c` ×3, two demo runtimes) | grep |
| `glyph_block_cache` production callers | **none** — its own test only | grep |
| `glyph_atlas` callers | **none** — one comment in `renderer.c:423` | grep |
| `renderer_cache_hits/misses` | reset in `renderer.c:442` and **never incremented** | grep |
| `USE_GLYPH_CACHE` | default `0`; a no-op mode today | `Makefile` |
| Glyph set residency | fully resident by design, capped at 1 MB; 8×8 set = 272–544 KB | `V1_4` budget section |
| Diagnostic emission pattern | `fprintf(stderr, "TSG-…")` at the app boundary; structured records are plain bounded values | `app.c`, `scene_diagnostic.h` |
| Test inventory guard | every focused test source must be in `make test` | `make check-test-inventory` |
| Baseline suite | 91 runners / 1005 tests, all green | measured 2026-11-25 |


## Decisions

### D-A — A face belongs to a surface, never to a cell

**Chosen.** A cell holds a codepoint and nothing else. The face that answers for a cell is a property
of the **surface** that owns those cells: the world grid is one surface, each UI layer is another, the
application chrome is a third.

*Rejected — face slot in the cell (D7 as written).* 16 + 4 + 24 + 24 = **68 bits**. Getting back under
64 requires either a 12-bit codepoint (4,096 values; JIS X 0208 alone is 6,879 kanji, so it destroys
the reference face) or shrinking colour (rejected: it bands the gradients that are the look). It also
re-baselines the SMC batch kernel, stream signatures, checksums and restore, and doubles the state
buffer — all rejected by V1-5 D1/D2.

*Rejected — merge all faces into one set, first definition wins.* The format already refuses
`GLYPH_SET_DUPLICATE_CODEPOINT`, and last-wins is the exact failure D8 was written against: "which
font drew this?" becomes unknowable. Two faces overlapping on Latin is the ordinary case, not an edge.

*Rejected — face as a user preference in `config.ini`.* Cheapest, and the worst fit: an authored
document would render differently on two installs, which breaks every parity claim the SMC and font
work rest on.

**Consequence.** `Cell`, `CellState`, `pack_cell_state` and the stream field sizes are untouched, and
no save-format change is required for this decision to be true. The ability to *choose* a face per
surface is a separate, later increment (D-B and the deferred list).

### D-B — Layers are the extension point for more than one face at a time

**Chosen, not built.** When a surface needs its own face, the natural home is the layer, because a
layer already owns its own `UiCanvas` and composition already resolves glyphs to **pixels** rather
than merging cells. A per-layer face therefore needs no cell identity and no fingerprint change: the
face is resolved at compose time and disappears into the framebuffer.

**Measured from the existing structure, not from new code:** `UiLayer` carries `canvas`, `clip`,
`anchor`, `scale_policy` and `z_order`; `ui_compositor_compose` returns pixels plus a `touched_cells`
mask; the renderer drives both surfaces in one call.

*Rejected for now — build it.* Layer lists are built in C at five sites and no asset file declares a
layer, so a per-layer face would be a setting with nobody to author it.

**Consequence, recorded so it is not re-derived.** Two things must be true when layers become authored
data: `face=` is a layer key, and a face change must mark that layer's cells `touched` or the swap
leaves stale pixels. Per-cell faces inside **one** grid remain deferred to D5's indirection table
(a document-level table maps a small cell index to a 32-bit identity), which is the only form that
still fits 64 bits.

### D-C — A missing glyph is always loud; the picture is presentation

**Chosen.** The invariant is a set of properties, not a shape:

- occupies the whole cell, so it cannot read as a thin-stroked legitimate glyph;
- colours from outside the theme, so no theme or author can restyle or hide it;
- derived at draw time, never serialised, never authorable;
- exactly one diagnostic per (face, codepoint), carrying the face's provenance hash;
- deterministic: same inputs, same pixels.

`X` on magenta-over-red remains the current picture because it is already implemented and tested. It is
**not** part of the contract. The records currently pin the shape ("asserts the sentinel glyph and
exact colours"); that is the one sentence this plan changes — the test asserts the properties plus one
checksummed reference frame, so the picture can change without reopening a decision.

**Amendment to D8.** Two corrections, both bookkeeping. The sentinel's colours are named by *role*
("out-of-theme diagnostic foreground/background"), because "purple" in D8 and the exit table disagrees
with the magenta `#FF2BD6` in `glyph_source.h`. And "derived" is stated precisely: today the sentinel
borrows the built-in face's `X`, so it is shape-derived-from-a-face rather than face-independent —
acceptable at this size, recorded as a known limitation rather than promised away.

*Rejected — a configurable sentinel.* A diagnostic an author can restyle is one that can be hidden;
D8 already rejected this and it stands.

### D-D — The application chrome declares its own face

**Chosen.** The chrome's source is explicit and defaults to `{ set = NULL }` — the built-in ASCII face —
for its whole lifetime. **This is a declaration, not a fallback.** D8 forbids *borrowing another face*;
it says nothing about a surface using the face it declared. Nothing is inherited from the world source.

*Rejected — keep one global source.* Simpler, and it makes a coverage-poor face turn the menus into
magenta-on-red marks. That is truthful and unusable: an application that cannot show its own error is
a worse diagnostic than a missing glyph.

**Consequence.** The renderer holds two sources and is the only place that knows both. The loader may
only ever write the world source, and a test asserts the chrome source survives both a successful load
and a failed one untouched.

*Deferred, not rejected:* whether the chrome should later prefer the installed face when the manifest
declares complete ASCII coverage. The declaration is one line, so changing that policy costs nothing.

### D-E — Residency versus cache: decided by measurement, after the `.hex` increment

**Chosen — deferred, with a decision rule.** V1-4's own budget argument says the glyph set is *fully
resident* and that "the 8.33 ms budget sees a texture read, not a raster". If that holds for every
declared face simultaneously within the 1 MB cap, then **requirement 4 is satisfied by residency plus a
generation tag**, and the two orphan modules are dead code that should be deleted rather than wired. If
it does not hold, the block cache gains a generation key and a real consumer.

**The rule, applied in I5:**

| Condition | Action |
|---|---|
| Every declared face's set, resident at once, stays within 1 MB | Requirement 4 closes as "resident set + generation"; delete `glyph_atlas`, `glyph_block_cache`, `USE_GLYPH_CACHE`, and the two counters nothing increments |
| Any supported combination exceeds the cap | Wire `glyph_block_cache` into the draw path with a generation/epoch in its key, and bound its growth |

**Why not decide now.** The answer depends on how many faces a real manifest declares, and `.hex` is
what produces the second measurable face. Deciding before that measurement is guessing.

### D-F — Sequencing

`I0` verification closeout → `I1` outcome 6 → `I2` `.hex` → `I3` explicit sources → `I4` loud missing
→ `I5` cache decision.

This refines the agreed "re-test → `.hex` → revisit the cache" by inserting the face and fallback work
(`I3`, `I4`) **before** the cache revisit, and it keeps the cache last, as agreed. The reason is
technical rather than tidy: `I2` yields a second real face, which is both the evidence `I5` needs and a
second data point for proving `I3` changed no pixels.

Ordering rules applied throughout: **verification before construction; behaviour-neutral before
behaviour-changing; the cheapest reversible increment first.**

## Scope

| | Items |
|---|---|
| **Required** | The three decisions D-A, D-C, D-D; V1-5 outcomes 4 and 6; the `.hex` reader; the `TSG-ASSET-ENV-0001` diagnostic; correction of the stale claims listed below |
| **Forbidden** | Any change to `Cell` (10 bytes), `CellState` (64 bits), `pack_cell_state`, stream field sizes or `GLYPH_SET_*` rejections; any implicit fallback between faces; any second identity space; user code at runtime; a configurable diagnostic |
| **Preserved** | The three draw sites' behaviour for a codepoint the declared source has; identical pixels for a full-ASCII set; the sentinel's conspicuousness; one diagnostic per (face, codepoint); SMC as the shipping path; the 1 MB resident cap |
| **Deferred** | Per-surface *choice* of face (D-A's format key); layers as authored data (D-B); per-cell faces (D5's table); whether chrome may prefer the installed face (D-D). `utf8proc` (D3) was deferred out of this plan and then landed as I6 on the same day |

## Increments

Each increment is independently revertible and lands as its own change with its own verification.

### I0 — Close the in-progress step (verification only, no code)

**Purpose.** V1-5 step 6 is code-complete and its aggregate verification was explicitly deferred by
`b632998` ("Not claimed yet: the full `make test` and `make matrix` … they run as the next action").
Nothing else can honestly be built on unverified code.

**Touches.** Nothing. Build artifacts only.

**Verification.** `make clean`; `make all`; `make test`; `make matrix`; `make standards`. Record each
outcome in this plan's verification table and in `CURRENT_STATUS.md`.

**Expected baseline.** 91 runners / 1005 tests, `MATRIX=0` on all eight modes. `make standards` is
correct but slow: cppcheck over `src/` exceeds five minutes here and is dominated by
`src/ui_document.c`; it must be allowed to finish rather than recorded as `FAIL-TIMEOUT`.

**Rollback.** None needed — no source changes.

### I1 — V1-5 outcome 6: re-test logical cells and UI scale independence

**Purpose.** The decision record requires this "after step 4" and no evidence of it is recorded
anywhere. It is the cheapest required outcome to close: verification, not construction.

**Touches.** Test sources only. The audit starts from the runners that already own the concern —
`test-ui-layout-resolver`, `test-ui-preferences`, `test-ui-compositor`, `test-camera`,
`test-vertical-physics` — and adds assertions only to the owning runner. A new runner is created only
if the concern is genuinely unowned, because `check-test-inventory` requires every focused source to be
in `make test`.

**What must be asserted.** World projection arithmetic is unchanged by the 9→10-byte `Cell`. UI
accessibility scale (100/125/150/200%) produces the documented cell geometry and does not depend on
`sizeof(Cell)`. Nothing in either path reads a glyph back into an 8-bit local.

**Verification.** Those runners pass, `make test` stays green, and the result is recorded as evidence
rather than as a claim.

**Rollback.** Delete the added assertions.

### I2 — `.hex` input path for the converter

**Purpose.** The unscii review's named next font-side increment: it lets `fontconv` convert
bitmap-native faces with **no FreeType at all**, retires the "unscii works" claim by measurement, and —
the reason it is ordered here — produces the **second real face** that I3 and I5 need as evidence.

**Touches.** New `src/font_hex.c/.h` (pure: text in, `GlyphSetEntry` out, no I/O, no allocation);
wiring in `tools/fontconv.c`; new `tests/test_font_hex.c`; Makefile runner variable, rule and `test`
recipe line. `src/glyph_set` is unchanged — it remains the only definition of the *output* format.

**Contract.** Strict parse, one glyph per line, `CODEPOINT:HEXDATA`, exact width for an 8×8 cell.
Malformed input is refused by naming the line number and the reason, following the `GLYPH_SET_*`
naming convention, and no set is written. **Bit order is the risk**: Unifont hex is MSB-first, our
cells are LSB-first, so bits reverse within each byte. The review verified this against `U+0045`; the
test pins it.

**Verification.** `unscii-8.hex` converts to 3,191 glyphs with a digest reproducible across runs and
across sessions; the converted `A` matches the author's 2-pixel form; `make font-determinism` is usable
for a face installed from `.hex`; a malformed line is refused with its line number and nothing is
written.

**Rollback.** Remove the flag and the two files; the TTF path is untouched.

### I3 — Explicit sources: one per surface, no global

**Purpose.** Implements D-A and makes D-D possible. **Behaviour-neutral by construction** — this
increment must change no pixels for any set that contains the codepoints it is asked for.

**Touches.** `glyph_source` (delete `g_active_source`, `glyph_source_set_active`,
`glyph_source_active`); `glyph_paint_resolve` gains a `const GlyphSource *`; `ui_compositor_compose`
gains one; `Renderer` holds `world_source` and `ui_source`; `renderer.c:541`, `:624` and
`ui_compositor.c:144` pass the right one; `app_resources` keeps owning the `GlyphSet` bytes and a new
`renderer_set_world_glyph_set` borrows them; `glyph_block_cache` takes a source for the day it is
wired. Tests to update: `test_glyph_paint` (currently calls the global setter), `test_glyph_source`,
`test_glyph_block_cache`.

**Ownership, per `C_STYLE_AND_OWNERSHIP.md`.** `AppResources` owns the `GlyphSet`; the renderer borrows
a `const GlyphSet *` and must not outlive the owner. `ui_source` is `{ NULL }` for its whole life and
the loader never writes it.

**Verification.** **Pixel identity is the gate.** Capture reference frame checksums before the change
with a full-ASCII set and with no set, and require exact matches afterwards. Unit tests: resolve
through a source holding a set, through a source with `NULL`, and through two different sources in one
process. A successful load and a failed load both leave `ui_source` untouched.

**Rollback.** One change; restoring the global restores the previous behaviour exactly.

### I4 — Loud missing, completed: no ASCII fallback, and the diagnostic that was promised

**Purpose.** D8 is decided and the code does not implement it for ASCII. This increment closes the gap
and builds `TSG-ASSET-ENV-0001`, which is catalogued as `Planned (V1-5)` and emitted nowhere.

**The resolve change, exactly.** Today: `set has cp` → `SET`; else `cp < 0x80` → `BUILTIN`; else
`MISSING`. After: `set && set has cp` → `SET`; else **`set == NULL && cp < 0x80`** → `BUILTIN`; else
`MISSING`. A source with no set still answers ASCII from the built-in face — that *is* the chrome
source — while a source with a set that lacks a codepoint reports `MISSING` instead of borrowing.

**Touches.** `src/glyph_source.c` (the one condition); new `src/glyph_missing.c/.h`; `src/renderer.c`
(begin/end frame hooks); `src/app.c` (the printing boundary); `docs/ERROR_CATALOG.md` (status and the
stale `GlyphRef` wording); new `tests/test_glyph_missing_sentinel.c` with its own runner.

**The once-per-(face, codepoint) rule, and how it is kept.** A `GlyphMissingState` — a plain value
struct, no heap, no global — holds a **65,536-bit bitmap per source (8 KB)**, indexed by codepoint, plus
a per-frame affected-cell counter. Emitting is exact rather than approximate: no eviction, no
"already reported" list that can overflow, no re-reporting after a frame boundary. The bitmap resets
when the source's set identity changes (its provenance hash, or `NULL` → built-in), because a different
face is a different subject. Two sources therefore keep two independent bitmaps — which is what makes
D-D's chrome/world split produce the right *number* of diagnostics as well as the right ones.

**Where it is reported.** `glyph_paint_resolve` stays pure: it returns `MISSING` and paints the
sentinel; it does not print. `renderer` notes the missing codepoint while drawing; at end of frame the
app — the highest boundary, which owns the consequence "the run continues" — prints the structured
diagnostics, once each, in the `fprintf(stderr, "TSG-…")` form the project already uses. Structured
records are plain bounded values following the `scene_diagnostic` precedent, so no layer gains a
formatted-string allocator or a global last-error.

**Context fields**, per the catalogue: face name and provenance prefix, codepoint as `U+XXXX`, the
first offending cell coordinate, and the count of affected cells this frame.

**Verification** — the catalogue already names the runner, `test-glyph-missing-sentinel`. It asserts:
a set missing `U+0041` resolves to `MISSING` with the sentinel's exact colours; the same codepoint
across 100 frames produces **exactly one** diagnostic; a set containing `U+0041` produces `SET` and no
diagnostic; no set produces `BUILTIN` for ASCII and no diagnostic; two sources missing the same
codepoint produce exactly two, one per face; installing a different face resets the bitmap; and a save
plus a stream capture contain no trace of the sentinel. The colour naming discrepancy is resolved in
the same change, in the code's favour unless the palette says otherwise.

**Rollback.** Reverting restores the ASCII fallback and removes the diagnostic; nothing else depends on
either.

### I5 — Requirement 4: apply the residency rule, then build or delete

**Purpose.** Close V1-5 outcome 4 with evidence instead of assumption, using D-E's rule and the second
face produced by I2.

**Step 1, measure.** For every face the example manifest declares, compute resident set bytes from
`glyph_set_encoded_size(count) + GLYPH_SET_HEADER_SIZE` and compare the sum with V1-4's declared 1 MB
cap. The measurement is arithmetic on the converted sets, not a benchmark.

**Step 2, apply the rule.**

| Result | Consequence |
|---|---|
| Sum ≤ 1 MB, every combination | Requirement 4 closes as *resident set + generation*: add the generation/epoch to the `GlyphSource` (its provenance already distinguishes faces), document that residency is the cache, and **delete** `src/glyph_atlas.c/.h`, `src/glyph_block_cache.c/.h`, `tests/test_glyph_block_cache.c`, the `USE_GLYPH_CACHE` flag and its Makefile wiring, and `renderer_cache_hits/misses` — all four are unreferenced or unwired today |
| Sum > 1 MB for a supported combination | Wire `glyph_block_cache` into the draw path with `(generation, codepoint, fg, bg)` as its key and a bounded growth policy; the counter pair becomes real |

**Touches.** Either deletions or wiring; `Makefile` runner inventory in both cases, because
`check-test-inventory` requires the test list and the sources to agree.

**Verification.** Whichever branch: `make test` green with the inventory consistent, `make standards`
clean (`check-legacy-unused` should have nothing to say afterwards), and — for the wiring branch only —
`make benchmark-headless` and `make stability-fast` inside their budgets.

**Rollback.** The deletion branch is revertible as a single change; the wiring branch keeps the modules
it already has.

### I6 — `utf8proc`: strict UTF-8, canonical NFC, bounded clusters

**Purpose.** The last of V1-5's required outcomes — the normalization and segmentation half of outcome 2
— and V1-6's prerequisite. D3 selected vendored `utf8proc` and rejected a hand-rolled decoder as a
correctness trap; this is the increment that acts on that.

**Touches.** `vendor/src/utf8proc`, pinned to **tag `v2.12.0`** (commit `79cdd5a`); a tag-pinned clone
line in `vendor.sh`; new `src/text_utf8.c/.h`; new `tests/test_text_utf8.c` with the `test-text-utf8`
runner; `UTF8PROC_SRC` and `UTF8PROC_CFLAGS` in the Makefile.

**Contract, as three decisions rather than defaults.** *Strict*: overlong forms, surrogates and anything
above U+10FFFF are malformed, because a text path that accepts them will one day write them out again.
*NFC only*: NFKC is deliberately absent — the locked product model says it does not rewrite creative
content. *A cluster is bounded*: `TEXT_UTF8_MAX_CLUSTER_BYTES` (64) exists because one cell stores one
cluster only when a face resolves it to one bounded image, so an unbounded ZWJ sequence is refused where
the refusal can be explained.

**Two things the vendored API taught, recorded because a document cannot:**
- `utf8proc_data.c` is `#include`d by `utf8proc.c`, not a translation unit of its own. Compiling it
  separately fails with `unknown type name 'utf8proc_uint16_t'`.
- `utf8proc_grapheme_break_stateful` takes **(previous, current, &state)** — not `(codepoint, &state)` —
  and its state starts at **0**, not −1. The two-argument shape is an older API.

**Verification.** `test-text-utf8` **8/8**: valid input of each length; overlong, surrogate, truncated
and above-maximum sequences refused; NFC composes `e`+U+0301 to `é` and is idempotent; a small buffer is
refused with nothing partial written; combining marks, a regional-indicator flag and a ZWJ family each
come back as **one** cluster; the 64-byte bound is refused with the caller's offset untouched; malformed
bytes inside a walk are refused at the point they appear. `make test` **93 runners / 1024 tests, 0
failures**; `standards-core` 6/6 — `check-unsafe-calls` scopes to handwritten production C and does not
object, and the vendored file compiles clean under `-Wall -Wextra -Wpedantic -Werror` so it needs no
flag exemption.

**Not wired into the application yet, deliberately.** Nothing calls `text_utf8`: the text paths that
will are V1-6's. It is a foundation with tests — the same shape `font_manifest` had before the converter
used it — recorded here so it is not later mistaken for dead code.

**Rollback.** Delete the two files, the runner and the two Makefile variables; `vendor/` is ignored, so
no other change depends on it.

## Test plan

New and updated runners, and why each exists:

| Runner | Change | Covers |
|---|---|---|
| `test-font-hex` *(new)* | add | `.hex` parse, bit reversal, malformed-line refusal by line number, 3,191-glyph conversion |
| `test-text-utf8` *(new)* | add | strict validation (overlong, surrogate, truncated, above-max), NFC composition and idempotence, cluster segmentation for combining marks, flags and ZWJ families, and the cluster byte bound |
| `test-glyph-missing-sentinel` *(new; name already in `ERROR_CATALOG.md`)* | add | much of I4's contract; see above |
| `test-glyph-source` | update | the new condition: set-with-gap → `MISSING`, no-set → `BUILTIN`, set-with-glyph → `SET` |
| `test-glyph-paint` | update | resolve through an explicit source instead of the deleted global |
| `test-glyph-block-cache` | update or delete | takes a source if kept (I5 decides) |
| owning runner for scale/projection | update | outcome 6, per the I1 audit |
| `test-core`, `test-ui-compositor`, `test-renderer`-adjacent runners | unchanged behaviour, must stay green | pixel identity for I3, SMC parity |

Two process rules apply to every new runner: it must be registered in the Makefile's `test` recipe
(`check-test-inventory` enforces it), and naming it `*_RUNNER` gets it `.headers.stamp` automatically,
so a header change rebuilds it — the mechanism that failed to exist when `test-r9-optical-semantics`
went stale.

## Verification gates

| Gate | When | Pass condition |
|---|---|---|
| Q2 per increment | every increment | strict C11 build with `-Wall -Wextra -Wpedantic -Werror`; the increment's narrow runners pass; docs updated in the same change |
| `make test` | I0, and after every increment | 91+ runners, 0 failures |
| `make matrix` | I0 and I3 | seven tracker modes (was eight until `USE_GLYPH_CACHE` was deleted in I5), because I3 touches the draw path and I4 changes what it returns |
| `make standards` | I0 and any increment adding a runner | exit 0; cppcheck allowed its full runtime |
| `make asan` / `make ubsan` / `make leak` | I4 | new code is the only new memory-touching code; `make leak` needs Docker and is the canonical container gate |
| `make benchmark-headless` / `make stability-fast` | I5 wiring branch only | inside the approved budgets; not applicable if the modules are deleted |
| `make display-acceptance-linux` | I1 | manual, needs a video environment; if unavailable it is recorded as unrun, not as a pass |

**Not gated by this plan, and not to be claimed:** native Windows checks, `make font-determinism`
without an installed face, `make coverage`, `make platform-survey`.

## Risks

| Risk | Mitigation |
|---|---|
| **I3 stops being behaviour-neutral** | Pixel checksums captured before and after, with a full-ASCII set and with no set. A mismatch is a defect, not a re-baseline — the same rule the SMC parity gate uses |
| **Dangling `const GlyphSet *` after I3** | `AppResources` owns the bytes; the renderer borrows and must not outlive it. Lifetime stated in the header, and the smoke path re-checks it after load and after a failed load |
| **Removing the ASCII fallback makes a coverage-poor face unusable** | This is exactly why D-D is in the same plan. **If D-D were dropped, I4 must not land** — the two are one change in consequence, two changes in code |
| **Diagnostic volume** | The bitmap makes "report once" exact and O(1); the per-frame counter stays a counter, never per-cell logging. 8 KB per source is bounded and dwarfed by the 1 MB glyph cap |
| **`.hex` bit order** | Already the project's documented failure class (the mirrored-glyph bug). `U+0045` is the pinned case |
| **Deleting the cache modules regresses performance** | The rule measures residency first, and V1-4's budget argument already says the set is fully resident. If the numbers disagree, the wiring branch runs instead |
| **The slow gate is mistaken for a hang** | `make standards` exceeds five minutes on this host. It is recorded as `PASS` only when it finishes; a timeout is `FAIL-TIMEOUT` and blocks the increment |
| **Doc corrections deferred "until later"** | They are part of the increments, not follow-up work. The contradiction this plan opens with is the failure mode being fixed |

## Documentation updates required

Each lands with the increment that makes it true, not afterwards.

| Document | Change | Increment |
|---|---|---|
| `V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md` | D7 amended: face slot → surface scope. "`Cell` grows to 4 bytes" corrected. Sentinel colour named by role. Pointer to this plan | I3 |
| `V1_5_CELL_IDENTITY_DECISION_2026-11-22.md` | Steps 5 and 6 marked landed with their evidence. The "does not supersede anything in V1_4" sentence corrected | I0 |
| `ERROR_CATALOG.md` | `TSG-ASSET-ENV-0001` `Planned` → `Implemented`, canonical site named, `GlyphRef` wording removed | I4 |
| `FEATURE_ROADMAP.md` | V1-4 status: drop "the 32-bit `GlyphRef` identity". V1-5 status: current position and a pointer to this plan | I0, I3 |
| `CURRENT_STATUS.md` | Add the V1-5 position — the file currently mentions neither V1-4 nor V1-5 | I0 |
| `TODO.md` | Replace the stale "**Next:** the cell identity migration" paragraph. Record the layer-as-asset extension point. Record the `.hex` item as scheduled | I2 |
| `docs/README.md` | Index this plan under current authority | I0 |
| `src/glyph_source.h`, `src/glyph_set.h` | The comments describing ASCII as "the fallback" restated as a declared source | I4 |
| This plan | Verification record filled from I0 onward | I0+ |

## Deferred, with the condition that would revive each

- **A per-surface face key in the scene/UI format** — needs a second face (I2 supplies it) *and* an
  authored need to mix. Not needed for any decision above.
- **Layers as authored data** (D-B) — a UI-asset-library concern, not V1-5.
- **Per-cell faces** — D5's indirection table; only if art variants on one grid become a real
  requirement.
- **`utf8proc` / normalization / segmentation** (D3) — **landed as I6**; this line is kept so the
  deferral's outcome is visible rather than deleted.
- **Chrome preferring the installed face** — one line, once a manifest declares complete ASCII coverage.
- **Changing the sentinel's picture** — the contract is now property-based, so this no longer needs a
  decision.
- **PUA mapping sidecar** (unscii's `uns2uni.tr`) — conversion-time mapping, sorted and unique output,
  collisions rejected rather than last-wins.

## Where this leaves V1-5

All six increments landed on 2026-11-25, each with its own gate. Outcomes 1, 3, 4, 5 and 6 are closed
in full; outcome 2 is closed except its normalization/segmentation half, which is `utf8proc` (D3) and
stays scheduled as its own increment because it is a vendoring change that touches the platform
profiles.

Three things are worth being precise about rather than letting the table imply more than it shows:

- **"Reference/optimized parity"** in the phase's exit gate no longer has two paths to compare. The
  optimised path *was* the atlas and the block cache; both are deleted, and the reference rasteriser is
  the only one left. The gate is satisfied by there being nothing to diverge, and that is a consequence
  of D-E, not an oversight.
- **Multilingual benchmark and stability budgets** cannot be measured on this host: `assets/fonts/` is
  empty, so no real multilingual set exists here. They are `FAIL-MISSING-TOOL`-class evidence until a
  face is installed, and the plan says so rather than reporting a pass.
- **`make leak`** — the canonical container gate — **passes**: `PASS: canonical containerized Valgrind
  leak gate passed`, `0 errors`, `All heap blocks were freed`. `test-core` links `$(SRC_RENDERER)`, which
  now includes `glyph_missing`, so the new module is inside that run rather than beside it.

**Q4 is done.** [`reviews/2026-11-25-v1-5-q4-representation-review.md`](reviews/2026-11-25-v1-5-q4-representation-review.md)
reviews the representation change against the roadmap's mandate — ownership, coupling, hidden state,
duplication, migrations, determinism, UI isolation, SMC/performance, platform evidence, accessibility
and documentation — and records two pieces of debt it recommends but does not fix (`pack_cell_state`'s
literal shift offsets, and `font8x8.h` defining data rather than only declaring it).

What remains before the phase can be called Verified:

1. The **multilingual** budgets, once a face is installed, and the **ASCII** budgets on an approved
   benchmarking host. Both need something this environment does not have (an installed font, and a
   machine that is the agreed measuring stick).
2. The one deliberate deferral — whether the chrome should prefer an installed face that declares
   complete ASCII coverage.

**`utf8proc` landed as I6**, on the same day, so outcome 2 is now complete and V1-6 is unblocked. It was
briefly recorded here as blocked on a networked vendoring step; that was true of the environment and
not of the design, and the fix was to fetch it (tag-pinned) rather than to route around the decision.



Filled in as increments land. Nothing below is claimed yet.

| Increment | Gate | Result |
|---|---|---|
| I0 | `make all` / `make test` / `make matrix` / `make standards` | **PASS.** `make all` exits 0 (`build/ascii-fps`, 856,664 B, clean under `-Werror`); smoke returns `{"smoke":"ok","map_width":10,"map_height":6}`. `make test` exits **0** — **91 runners / 1005 tests, 0 failures**. `make matrix` exits **0** with **8/8** modes passing. `make standards` exits 0: `standards-core` 6/6 guards and the analysis policy re-measured; cppcheck measured in bounded batches with the identical flags, 269 of 270 inputs clean, the remaining input (`src/ui_document.c`) covered by the complete gate run earlier in the same session. **Caveat recorded rather than glossed:** `make test`, `standards-core` and the policy were re-run after the documentation edits; `make matrix` and the full cppcheck gate were measured before them. No source file changed between those runs — only `docs/`. |
| I1 | owning runner(s) for projection and scale | **PASS, with one added assertion.** Audit: UI accessibility scale is already exercised end-to-end at all four presets by `test_ui_compositor`'s `test_centered_menu_crop_visible_at_all_presets`, and world projection in logical cells by the runners that assert `logical_w`/pitch (`test_editor_selection`, `test_optical_render`, `test_heightfield_selective`, `test_r9_mirror_trace`). No new runner was created — ownership exists, so creating one would have violated the plan's own rule. **One real gap was found and closed:** the preset test derives its probe coordinates from `ui_compositor_scaled_edge` itself, so it cannot prove the scaling rule, only that it agrees with itself — the same self-comparison the UI-scene review criticised. Added `test_scaled_edges_have_independent_expectations` to `test_ui_compositor`: 18 hand-computed `(edge, preset, expected)` cases pinning the truncating rule, including two where truncation and rounding differ (`7@125→8`, `41@150→61`). Verified: `test-ui-compositor` 8/8, and `make test` **91 runners / 1006 tests, 0 failures**. Cell byte size was confirmed confined to allocation (`grid.c`) and the layout `CASSERT` (`renderer.c:64`); it appears in neither the scale nor the projection arithmetic, which is what outcome 6 asks for. |
| I2 | `test-font-hex`, `unscii-8.hex` conversion digest | **PASS on a synthetic fixture; the real archive is not installed here, so that half is unrun.** Built `src/font_hex.c/.h` (pure: text in, entries out, no I/O; strict, refusing with a line number; sorts and rejects duplicates so the set format's ascending-and-unique invariant holds where the entries are created rather than where they are validated) and `tests/test_font_hex.c` (7/7). Extracted `prepare_face_file` so D12's all-files-or-none rule has one definition for both input kinds, and added `fontconv --hex`, placed *outside* the FreeType guard so a build without FreeType can still convert a bitmap face. End-to-end on a two-glyph fixture: conversion writes 2 entries / 80 bytes, `--check-set` reads it back through the runtime reader and renders a correct, upright `A` and `E` (a wrong bit order would mirror both), and two independent conversions produce the identical set digest `53ac27bf…`. `make test` **92 runners / 1013 tests, 0 failures**; `standards-core` 6/6. **Not verified, and not to be claimed:** the real `unscii-8.hex` (3,191 glyphs) — that file is not on this machine and `assets/fonts/` is empty, so the review's conversion figure stays unproven until `fontconv --manifest … --fonts-dir … --convert unscii-8 --hex --out …` is run where the font is installed. Worth recording: the new test caught a mis-computed expectation of mine (`0x7C` is not a bit palindrome — reversed it is `0x3E`), not a defect in the reader, which is the test earning its place rather than confirming its author. |
| I3 | pixel checksums identical; `make test`; `make matrix` | **PASS — pixel-identical.** Removed the process-wide selection entirely (`g_active_source`, `glyph_source_set_active`, `glyph_source_active`); `glyph_paint_resolve`, `ui_compositor_compose` and `glyph_block_cache_compute` now take the source explicitly; `Renderer` holds `world_source` and `ui_source`, both initialised to the built-in face and only the world one written by the loader through `renderer_set_world_glyph_set`. The chrome's source is never touched by a load or a failed load, which is the property D-D depends on. `app_resources` no longer reaches into a global: it hands the set to the renderer when one exists, and the headless smoke path — which creates no renderer and draws nothing — is unaffected. Nine call sites updated across `src/` and three test files. **Gate: `make check-ui-workbench-frame` → `PASS: recorded workbench frame checksums match`** — the composited output is byte-identical, which is what "behaviour-neutral" had to mean. `make test` **92 runners / 1014 tests, 0 failures** (the extra test is `test_two_sources_answer_independently`, which pins the property the deleted global could not express: one source holding a set and another holding none answer differently for the same codepoint, and neither disturbs the other); `make matrix` **8/8**; `standards-core` 6/6; smoke `{"smoke":"ok",…}`. Two mistakes of mine surfaced here: a truncated grep hid a later use of `chosen` in `fontconv`, and the regrouped `glyph_block_cache.h` needed its own `glyph_source.h` include — both caught by the compiler rather than by review. |
| I4 | `test-glyph-missing-sentinel`; ASan/UBSan; `make leak` | **PASS, with the sanitizers run narrowly.** The resolve condition is now `set == NULL && codepoint < 0x80` → built-in, otherwise MISSING, so a coverage gap stops being filled from the application's own face. New `src/glyph_missing.c/.h` holds a **65,536-bit bitmap per source (8 KB)** keyed to the face's provenance, plus per-frame counters and a bounded list of eight newly-distinct reports with an overflow count — exact rather than approximate, because no eviction rule or overflow behaviour has to be invented. `Renderer` owns the state, notes misses at both draw sites with their cell coordinates, and `report_missing_glyphs` in `app.c` prints `TSG-ASSET-ENV-0001` once per newly distinct (face, codepoint) with the catalogue's context fields. **End-to-end observed:** with a one-glyph set installed, a 180-frame benchmark emitted eight named gaps (`face c0ec8660cc630f21 lacks U+004F at cell 92,0 (41600 affected this frame)`) plus `3 more distinct codepoint(s)` — and nine lines total for the whole run, so the dedupe holds across frames; removing the set restored silence. **Verification:** `test-glyph-missing-sentinel` 5/5; `make test` **93 runners / 1019 tests, 0 failures**; `make check-ui-workbench-frame` still matches; `standards-core` 6/6. ASan and UBSan were run on the new module's runner rather than across the whole suite (a full `make asan` is a clean rebuild of everything on this machine): **ASan clean, and UBSan found a real defect** — `load of value 136, which is not a valid value for type '_Bool'`, from a test using a `GlyphMissingState` without resetting it. The test was fixed and the precondition is now stated in the header; UBSan and ASan both clean afterwards. `make leak` needs Docker and was not run. |
| I5 | residency arithmetic; then either deletions or benchmarks | **PASS — residency won, and the orphans are gone.** Measured with the format's own constants: an entry is 16 bytes and the header 48, so Misaki (7,171 glyphs) is **114,784 bytes** — which reproduces the figure the font programme recorded for the real set exactly — and `unscii-8` (3,191) is 51,104. All three declared faces resident at once is **280,672 bytes, 26.8% of V1-4's 1 MiB cap**, so D-E's rule applied: requirement 4 closes as *resident set + generation*, and the unwired machinery was deleted — `src/glyph_atlas.c/.h`, `src/glyph_block_cache.c/.h`, `tests/test_glyph_block_cache.c`, the `USE_GLYPH_CACHE` flag with its `ifeq`, its `GLYPH_DEFS`, its app-command line define, its runner, and the two `renderer_cache_*` counters that nothing incremented. **Two consequences worth naming.** (a) `glyph_atlas.c` was the application's only definer of `font8x8_basic`, because `src/font8x8.h` defines the array; deleting it broke the app link, and the fix is the arrangement the repo already used twice — a one-line owner TU, `src/font8x8_face.c`, alongside `tests/font8x8_test_data.c` and `tools/font8x8_face.c`. (b) `make matrix` is now **seven** modes rather than eight, because `USE_GLYPH_CACHE=1` was one of them; that is a real reduction in coverage of a *deleted* feature, not a weakened gate, and it is annotated in the V1-5 record. **Verification after the deletions:** `make all` clean, `make test` **92 runners / 1016 tests, 0 failures** (three block-cache tests removed with the module), `make matrix` **7/7**, `make check-ui-workbench-frame` **checksums match** — pixel-identical, proving the deleted code really was unreachable — and `standards-core` 6/6 with no unused-module complaint. No benchmark comparison was needed: nothing in the render path changed. |
| I6 | `test-text-utf8`; full suite; standards | **PASS.** `utf8proc` vendored at tag **v2.12.0 / 79cdd5a**, fetched by a tag-pinned `vendor.sh` line so a fresh checkout reproduces it and a floating branch cannot silently change what "valid UTF-8" means (D10). `src/text_utf8.c/.h` is the project's only door to it: strict validation, NFC with no NFKC, and grapheme clusters bounded at 64 bytes and refused rather than truncated. `test-text-utf8` **8/8** — including the two clusters only real UAX#29 segmentation gets right (a regional-indicator flag and a ZWJ family are each *one* cluster) and the bound case whose caller offset is left untouched. `make test` **93 runners / 1024 tests, 0 failures**; `standards-core` 6/6, the vendored file clean under `-Wall -Wextra -Wpedantic -Werror` so no exemption was needed. Two API facts cost a build cycle each and are recorded in I6 above rather than re-learned. **Not wired into the application** — those are V1-6's text paths, and saying so is the difference between a foundation and dead code. |

## Sign-off

Approving this plan approves: **D-A** (face is a surface property), **D-B** (layers are the extension
point, not built), **D-C** (loud by property, `X` is presentation), **D-D** (chrome declares the
built-in face), **D-E** (residency decided by measurement after I2), **D-F** (the I0→I5 order), and the
two amendments to D7 and D8 with the documentation corrections listed above.

If any single decision is rejected, the increment that depends on it must be dropped rather than
reworked in flight — in particular I3 and I4 are one consequence, so D-D and D-C stand or fall together.

**First action after sign-off is I0**, which changes no code and produces the verified baseline the rest
of the plan is measured against.
