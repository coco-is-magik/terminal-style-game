# V1-5 Representation Change — Q4 Review — 2026-11-25

## Provenance and status

**Review only. No code changed by this document.** It discharges the roadmap's Q4 mandate — *"perform a
Q4 review after the Unicode/cell/SMC foundation"* — and is the last item of V1-5's exit gate other than
`utf8proc` and the two budgets that cannot be measured on this host.

Everything below is checked against the tree as it stands on 2026-11-25, after I0–I5 of
[`../V1_5_GLYPH_FACE_FALLBACK_AND_CACHE_PLAN_2026-11-25.md`](../V1_5_GLYPH_FACE_FALLBACK_AND_CACHE_PLAN_2026-11-25.md).
Claims are evidence, not impressions; where something was not run it says so.

## What is under review

One cross-cutting change, spread over a month and finished today:

| Element | Before | After |
|---|---|---|
| `Cell.glyph` | `uint8_t` | `uint16_t` |
| `sizeof(Cell)` | 9 | **10** (guarded by `CASSERT` 2/6/10) |
| `CellState` layout | glyph 0–7, colours 8… | glyph **0–15**, colours 16/24/32/40/48/56 — still exactly 64 bits |
| SMC stream sizes | written as literals | derived from the types (`CELL_STREAM_GLYPH_BYTES = sizeof(Cell.glyph)`) |
| Face selection | one process-wide mutable global | **one per surface**, owned by `Renderer` |
| Missing glyph | borrowed the built-in face for ASCII | **reported**, `TSG-ASSET-ENV-0001`, exactly once per (face, codepoint) |
| Glyph caches | two modules, neither wired | deleted — residency is the cache |

## Findings

### Ownership — sound

`AppResources` owns the glyph-set bytes; `Renderer` borrows a `const GlyphSet *` and cannot outlive
the owner; `GlyphSource` is a plain value the owning surface holds. The loader is the only writer of
the world source, and the chrome's source is never written at all. That is stated in headers, enforced
by `const`, and covered by a test that two sources in one process answer independently.

### Coupling — the one real debt, and it predates this work

`src/font8x8.h` both **declares and defines** `font8x8_basic[128][8]`. That forces *exactly one*
includer per binary, which is why three sibling files now exist whose only job is to include it:
`src/font8x8_face.c`, `tests/font8x8_test_data.c`, `tools/font8x8_face.c`. Deleting `glyph_atlas.c`
exposed this — it had been the app's owner by accident, and the linker said so before review did.

This is not a defect introduced today, but it is the clearest structural debt the change leaves
visible, and it will bite again the next time a binary is added. **Recommendation, deliberately not
done here** (it is invisible to behaviour and would be its own increment): split the data out of the
header — `font8x8_data.h` holds the definition, included by the three owner TUs; `font8x8.h` keeps
only `extern unsigned char font8x8_basic[128][8];`. Then a new binary cannot silently gain a duplicate
definition or a missing symbol.

### Hidden state — materially reduced

The process-wide mutable glyph selection is gone: `g_active_source` and both accessors were deleted,
and every draw site now receives the source it draws with. What remains is:
- the diagnostic counters in `renderer.c`, which are process-global by design (they are benchmark
  instrumentation and were already there);
- `GlyphMissingState`, which is **owned by the renderer** rather than being a module global — the
  whole point of putting it there, so two renderers cannot share a "reported" list.

### Data duplication — none in the representation

One definition of the set format (`glyph_set`), one of the text format (`font_hex`), one of the
precedence rule (`glyph_source`), one of the colour rule (`glyph_paint`), one of the emission site
(`app.c`). The per-face missing bitmap is not duplication: it is a per-subject record that resets when
the subject changes, and the reset is tested.

### Migrations and compatibility — none required, one incident

Scenes store decals and objects, not cells, so the widening needed no save migration. The one
migration-adjacent incident was the scene ceiling change (`53b67e9`), which left a test asserting the
old cell count for sixteen days because the build did not track headers. That mechanism is now closed
(`.headers.stamp`), and the stale expectation was corrected on 2026-11-25 — but the episode is the
reason this review insists on clean rebuilds as evidence rather than incremental ones.

### Deterministic behaviour — holds

No clock, no randomness and no environment-dependence entered the new code. Face choice is authored or
configured, never inferred from the host. The diagnostic dedupe is a bitmap, not a timing window, so
"reported once" is a property rather than an observation. The one stated trade-off is that the bitmap
is keyed to the *current* face, so swapping faces out and back re-reports once — deterministic,
documented, and tested (`test_each_face_reports_its_own_gap`).

### UI isolation — improved rather than merely preserved

The chrome has its own source precisely so the world's font cannot reach the interface. Before this
work there was one source for both surfaces, so a coverage-poor set would have corrupted the menus;
now it cannot.

### SMC and performance — unchanged in shape, safer in detail

The 8-byte `CellState` kernel is untouched, and `_Static_assert(sizeof(CellState) == 8)` in the
batch path makes that a compile-time fact rather than a convention. The defect class this work
actually created and then fixed — a hardcoded field size telling SMC to compare one byte of a
two-byte glyph (`8f36533`) — is now structurally prevented: the sizes are `sizeof()`-derived, so they
cannot drift from the type again.

One residue worth naming: `pack_cell_state` uses explicit shift constants (16/24/32/…). They are
correct and tested, but they are still literals. Deriving them from the field widths, as the stream
sizes now are, would close the same class of mistake in the other half of the mechanism.

### Platform evidence — none, and not implied

No native Windows run, no display-backed acceptance, no platform survey. Nothing in this review should
be read as platform evidence. The change is platform-neutral by construction (no new I/O, no new
allocation, no platform calls in the new modules), but "neutral by construction" is an argument, not a
measurement.

### Accessibility — a real gap closed, one pattern retired

UI accessibility scale previously had one test that derived its probe coordinates from the very
function it was validating, so it could only prove self-consistency. It now has independent expected
values at all four presets, including two cases where truncation and rounding differ. The same
anti-pattern (`parity` between a function and itself) is called out in the UI-scene review; it is worth
grepping for elsewhere, but nothing in this change depends on it any more.

### Documentation — the phase's own biggest defect

The record set contradicted the code in five places, all now corrected at source: a 32-bit `GlyphRef`
that was never implemented, "Cell grows to 4 bytes", "purple" against an implemented magenta, a
catalogue entry that promised a diagnostic nobody had written, and a precedence comment that called
the built-in face a fallback while D8 forbade exactly that. The pattern is consistent enough to name:
**decisions were recorded, then amended in code without the amendment reaching the record.** The
mitigation that actually worked here was reading the code against the documents, not reading the
documents alone.

## Exit-gate assessment

| Gate item | Assessment |
|---|---|
| Reference/optimized parity | **Moot, and correctly so.** The optimised path *was* the atlas and the block cache; both are deleted, so there is one rasteriser left to be equal to itself. Recorded as a consequence of D-E rather than silently dropped |
| All SMC runners/modes pass | **PASS** — `make matrix` 7/7 (seven since `USE_GLYPH_CACHE` was removed with its feature), `make test` 92 runners / 1016 tests |
| Fallback is exceptional | **PASS, and this is the substantive change.** A source that declares a set and lacks a codepoint is now a diagnostic; only a source with *no* set answers from the built-in face, and that is a declaration (the chrome's), not a fallback. Verified end to end with a one-glyph set |
| ASCII benchmark/stability budgets | **NOT MEASURED.** `make benchmark-headless` / `stability-fast` were not run in this work. The raycast benchmark reports `fail_performance` on this host for pre-existing reasons, so a budget number from here would be host variance wearing a gate's clothes |
| Multilingual budgets | **NOT MEASURABLE HERE.** `assets/fonts/` is empty; there is no real multilingual set on this machine. `FAIL-MISSING-TOOL`-class evidence, not a pass |
| Q4 review | **This document** |

Leak gate, run for completeness rather than reclassified as an exit-gate item: `make leak` →
**`PASS: canonical containerized Valgrind leak gate passed`**, `0 errors`, `All heap blocks were
freed`. `test-core` links `$(SRC_RENDERER)`, which now includes `glyph_missing`, so the new module is
inside that run rather than beside it.

## Defects this work found

| Found by | Defect | Disposition |
|---|---|---|
| A clean rebuild | `test-r9-optical-semantics` asserting a cell count sixteen days stale | fixed; the header-dependency mechanism that hid it was already closed |
| Reading code against D8 | the ASCII fallback the decision forbade | removed (I4) |
| Reading the catalogue | `TSG-ASSET-ENV-0001` promised, never emitted | implemented and observed end to end (I4) |
| The linker | `glyph_atlas.c` was the app's only definer of `font8x8_basic` | one-line owner TU added (I5) |
| **UBSan** | indeterminate `_Bool` read from a `GlyphMissingState` used without reset | test fixed, precondition documented (I4) |
| This review | `pack_cell_state` shift constants remain literals | recommended, not done |
| This review | `font8x8.h` defining data forces one includer per binary | recommended, not done |

The UBSan entry is the one worth repeating: it was found by running a sanitizer on new code, not by
reading it.

## What remains before V1-5 can be called Verified

1. **`utf8proc` (D3)** — the last required outcome, and **blocked in this session**: there is no copy
   on disk, no system package, and the repository's only vendoring path is `vendor.sh`, which fetches
   by `git clone`. Dependency acquisition is deliberately a separate networked trust step (D10,
   VERIFICATION_POLICY), so inventing a second path to get it would be worse than leaving it open.
2. **Multilingual benchmark and stability budgets** — blocked on an installed face.
3. **ASCII budgets** — not measured; this host is not the approved benchmarking environment.
4. **`pack_cell_state` literal offsets** and the **`font8x8` header data** — recommended above, both
   small, neither behaviour-visible.

## Recommendation on roadmap order

`utf8proc` next, and only that: it is both V1-5's last outcome and V1-6's prerequisite. Starting V1-6's
text work before it would grow a second, hand-rolled UTF-8 decoder — the exact outcome D3 rejected as
"a correctness trap". Nothing else in V1-5 blocks it.

