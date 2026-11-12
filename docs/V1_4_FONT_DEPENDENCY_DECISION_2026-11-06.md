# V1-4 Font Dependency Decision — 2026-11-06

## Status and authority

**Decided.** This record answers the three questions in §7 of
[`reviews/2026-11-06-v1-4-font-character-model-research.md`](reviews/2026-11-06-v1-4-font-character-model-research.md)
and closes V1-4's exit gate. It is the authority for the font/raster dependency set used by
V1-5 (glyph identity, raster service, cache, SMC migration), V1-6 (Unicode UI, editing,
clipboard, IME) and V1-7 (glyph-art formats). It changes no production code; it fixes the
choices those phases implement against.

It supersedes the *recommendation* in research §7 where they differ — §7 offered baked profiles
with hand-authored art as a serious option; this record rejects hand-authoring for the required
script set and says why.

## Objective

Retire the `uint8_t` glyph and the 128-glyph 8×8 face **without** buying any of these:

- a runtime dependency that must be pinned, hash-verified and image-owned for every platform;
- non-deterministic rasterisation, where the same codepoint yields different pixels on Linux
  and Windows;
- unbounded memory or a cache that can miss inside the 8.33 ms frame;
- implicit system-font fallback, which the locked product model forbids.

## Facts this decision rests on (measured)

| Fact | Value | Source |
|---|---|---|
| Glyph identity | one `uint8_t` per cell | `src/grid.h` |
| Cell metrics | renderer rejects anything ≠ 8×8 | `src/renderer.c` |
| Face | one, `font8x8_basic[128][8]`, compiled in | `src/font8x8.h` |
| Raster/shaping dependencies | **none**; `vendor/dist` holds SDL3, SDL3_mixer, cmocka, enet | directory listing |
| Frame envelope | 41,600 cells (260×160) in 8.33 ms @120 fps | `UI_DISPLAY_SURFACE_TARGET_2026-10-07.md` |
| Required scripts | CJK ideographs, kana, hangul, common Latin, punctuation/box drawing, combining sequences | V1-4 locked product model |
| Governance | every dependency pinned, hash-verified, image-owned | build + platform profiles |

The decisive number is the **required glyph count**: CJK Unified Ideographs (URO) 20,992 +
hangul syllables 11,172 + kana/latin/punctuation ≈ 33,000 glyphs. That is the fact that rules
out hand-authored art: nobody draws 33,000 cell images, and a curated Latin-only face would fail
the locked model's first priority.

## The decision

**Bake offline with FreeType; ship bitmaps; keep the runtime dependency surface at zero for
rasterisation.**

| # | Decision | Chosen | Rejected, and why |
|---|---|---|---|
| **D1** | Baseline face source | **Baked bitmap profiles** (register A3c) | TTF at runtime — pays a dependency on every platform for a face set that never changes at runtime |
| **D2** | How profiles are produced | **Build-time bake tool over a pinned TTF using FreeType; output committed with recorded hashes** (register A3a) | Hand-authored art (33k glyphs is not authorable); runtime FreeType (cache misses, dependency) |
| **D3** | UTF-8 / NFC / grapheme segmentation | **Vendored `utf8proc` (MIT), pinned** | ICU (enormous, data-heavy, locale surface); hand-rolled tables (a correctness trap) |
| **D4** | Shaping | **Not in v1.** Bake-time only, if a cluster ever needs it | HarfBuzz at runtime — no required script needs context-dependent shaping; CJK/kana/hangul are one-to-one |
| **D5** | Bidi | **Deferred to V1-6, constrained**: table-driven UAX#9 subset generated at bake time; until then RTL is refused with a typed diagnostic | fribidi — LGPL conflicts with the static and export paths (V1-19) |
| **D6** | Cell metrics | **Declared profiles**, not one global 8×8: 8×8 (glyph art, unchanged), 8×16 (primary text), plus measured 12×12/16×16 for accessibility | Variable cells (a new renderer, out of scope); per-glyph scaling (breaks the grid) |
| **D7** | Glyph identity | **Packed 32-bit `GlyphRef`**: 2-bit kind (blank/transparent/missing/ink), 4-bit profile, 26-bit catalog index | Per-cell `font_id` byte (register A1a) — cannot express blank vs transparent vs missing distinctly |
| **D8** | Fallback | **Declared per-profile chain, then the missing state**, visible diagnostic in authored content | System fonts, silent tofu, replacement character — all forbidden by the locked model |
| **D9** | Fonts | **Noto Sans Mono + Noto Sans Mono CJK (OFL-1.1)**; DejaVu Sans Mono as the box-drawing candidate — settled by the coverage test, not by assumption | Unifont (GPL with font exception — export risk); assuming coverage without the test |
| **D10** | Unicode version | **Pinned to the vendored data release** (utf8proc + bake tool), recorded at vendor time | Floating "latest" — makes parity unprovable |

### Why build-time FreeType is not the dependency the governance rule forbids

The rule exists so a *shipped binary* needs nothing outside the pinned image and so identical
inputs produce identical pixels everywhere. Build-time-only use satisfies both:

- **Runtime surface unchanged.** No `libfreetype` in the link path, the platform images or the
  exported build. The binary's dependency set stays exactly what it is today.
- **Parity gets stronger, not weaker.** Rasterisation happens **once**, on one machine, and the
  pixels are committed. Cross-platform parity stops being a hope about library versions and
  becomes a hash check. The research §3 rule set still applies — to the bake rather than the
  frame: no hinting, fixed AA mode, one rounding function, integer baseline.
- **It matches how this project already handles content.** Maps are baked offline by `mapgen`
  and shipped as data; glyphs become data the same way, and the test shape follows too
  (`--reproduce` for maps ≈ re-bake hash equality for fonts).
- **CI without FreeType still builds.** The bake tool is a developer/CI prerequisite, not a build
  prerequisite; the committed artefacts are what the build consumes.

The honest cost: a bake tool to write and audit, FreeType on the bake machine, and the discipline
that a re-bake is a reviewed artefact change rather than a side effect. That is strictly less
than 33,000 hand-authored glyphs or a runtime dependency on four platforms.

### Atlas budget (declared, not discovered)

1bpp masks over the bounded required set (≈33,000 glyphs + ≈1,000 Latin/punctuation):

| Profile | Bytes/glyph (1bpp) | 34k glyphs | With 4-level AA (2bpp) |
|---|---|---|---|
| 8×8 (glyph art) | 8 | 272 KB | 544 KB |
| **8×16 (primary text)** | 16 | **544 KB** | **1.09 MB** |
| 12×12 | 18 | 612 KB | 1.22 MB |
| 16×16 | 32 | 1.09 MB | 2.18 MB |

**Declared cap: 2 MB per resident profile, 4 MB resident total.** The atlas is fully resident
(no eviction, no per-frame allocation), so the 8.33 ms budget sees a texture read, not a raster.
Cold cost exists only at load, where research item 3's measurement plan applies.


## Consequences for the phases this unblocks

**V1-5 (identity, raster service, cache, SMC migration)** implements D1–D8. Order that respects
the existing rollback point: typed `GlyphRef` + ASCII adapter first (old readers untouched), then
the catalog + human-readable glyph table in the scene, then the baked-profile atlas behind the
same interface the current atlas has, then SMC adapters. `Cell` grows to 4 bytes of glyph; that
touches dirty comparison, packed state, stream signatures, checksums, restore behaviour and every
SMC adapter — the list V1-5 item 5 already names, unchanged by this decision.

**V1-6 (Unicode UI, editing, clipboard, IME)** gets three of its groundwork pieces from here:
strict UTF-8 + NFC + segmentation (`utf8proc`), the missing-glyph diagnostic it must surface, and
the RTL refusal that stands in until bidi lands. Paste validation shares the same validator as
typed text (register C1a) — this decision adds no second path.

**V1-7 (glyph-art formats)** inherits D6/D7: one art cell is one profile+index under the 8×8
profile, so existing art and the 8×8 aesthetic survive the migration by construction.

**Not decided here, deliberately:**

- **Raster profile measurement** (research item 3) — the decision unblocks it but does not answer
  it. Candidate profiles must be baked and measured against the 41,600-cell / 8.33 ms envelope,
  with CJK legibility reviewed by eye. That work is the first task V1-5 can start with.
- **Fallback font files** (research item 2) — criteria exist; hashes, licences and the coverage
  test are produced when the bake tool exists, not before. D9 names the candidates so the
  coverage test has an expected answer to falsify.
- **Bidi implementation** — V1-6, constrained as in D5.
- **Whether FreeType is vendored into the repo or treated as a host tool** — a packaging detail
  for the bake tool, not for the shipped product; either choice keeps the runtime surface at
  zero. Recorded so it is not mistaken for an unconsidered gap.

## Risks, stated plainly

- **The bake tool is new code with new failure modes** (font parsing, licence metadata,
  reproducible output). Mitigation: it is build-time only, its output is reviewable bytes, and
  its first test is re-bake equality — the same property that makes the maps trustworthy.
- **A committed artefact can rot silently** if the tool changes without a re-bake. Mitigation:
  the hash check runs in CI, so "artefact does not match tool" is a failure rather than a drift.
- **The 8×16 profile changes the visual language** of the application. That is a design decision
  the owner must accept at review, not something this record can settle; V1-5 keeps 8×8 for
  glyph art regardless, so the aesthetic is preserved where it matters most.
- **`utf8proc` is a real runtime dependency** — small, MIT, pure C, no allocation surprises, but
  it is pinned and image-owned like everything else, and it must be added to the vendoring
  records before use.

## Exit-gate mapping (V1-4)

| V1-4 exit-gate item | State |
|---|---|
| Dependencies selected | **decided** (D1–D4, D10) |
| Unicode version | **decided as a rule** — pinned to vendored data at vendor time (D10) |
| Fallback fonts | **selected** (D9); hashes/licences/coverage test follow with the bake tool |
| Raster profile | **direction decided** (D6); measurement is the first V1-5 task |
| Parity contract | **decided** — bake-once + hash equality, with the research §3 rules applied at bake |
| Resource budgets | **declared** — 2 MB/profile, 4 MB resident, fully resident atlas |
| Glyph identity and format direction | **decided** (D7) |

V1-4 is **closed**. V1-5 is **unblocked** and may begin at its first task (bake tool + profile
measurement) without further decisions.

