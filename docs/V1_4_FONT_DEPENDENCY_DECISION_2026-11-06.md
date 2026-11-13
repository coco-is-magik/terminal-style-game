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

**Revision 2026-11-06, owner review.** Two of the choices below (`D2`, `D6`) were wrong and are
replaced in place, marked **[revised]**. The correction was factual, not stylistic: this record
argued for taller cells because CJK "cannot be read at 8×8", and that is false — 8×8 CJK bitmapped
fonts exist in production (Misaki: JIS first and second level, ~6,355 kanji plus kana, most glyphs
inset inside 7×7 so adjacent characters do not touch). Out of that came the rest of the owner's
correction, which this record now states as its own:

1. **The converter's job is font → 8×8. Supplying a font that fits is the user's job.** The cell
   size is a fixed property of the renderer, not something the font pipeline negotiates.
2. **No glyph images in the repository.** Tens of thousands of committed cell images would be
   unreviewable and would turn every font tweak into a binary diff.
3. **The contract is verification, not artefact.** The font manifest records each font's name,
   file hash(es) and source URL, so two installs can prove they converted the same bytes, and
   anyone can find the font again.
4. **A failed conversion must say exactly why and abort.** Fitting remedies exist as explicit,
   off-by-default options (`D11`).


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
| 8×8 CJK is achievable | Misaki: 8×8 Japanese bitmapped font, JIS 1st+2nd level, most glyphs inset inside 7×7 so neighbours do not touch, free (M+ licence), TTF form carries embedded bitmaps | <https://littlelimit.net/misaki.htm> |
| Governance | every dependency pinned, hash-verified, image-owned | build + platform profiles |

The decisive number is the **required glyph count**: CJK Unified Ideographs (URO) 20,992 +
hangul syllables 11,172 + kana/latin/punctuation ≈ 33,000 glyphs. That rules out hand-authored
art — nobody draws 33,000 cell images.

It does **not** force larger cells, which is where the first draft of this record went wrong.
Two facts separate cleanly: the glyph *count* makes hand art impossible, and the glyph *size* is
already solved by existing 8×8 CJK bitmap faces. Conflating them produced a proposal to redesign
the cell grid for a problem the grid did not have. The renderer's cell stays 8×8; how many glyphs
a given install can display becomes a property of the font the user supplies, which is why the
manifest must declare coverage rather than this record assuming it.

## The decision

**Convert with FreeType at build time, from fonts the user supplies; commit no glyph data; make the
font manifest the contract; keep the runtime dependency surface at zero for rasterisation.**

| # | Decision | Chosen | Rejected, and why |
|---|---|---|---|
| **D1** | Glyph form at runtime | **Bitmap glyphs**, converted ahead of time | TTF at runtime — pays a dependency on every platform for a face set that never changes at runtime |
| **D2** | Where conversion happens **[revised]** | **Locally, at build/install, from user-supplied font files.** The converter plus a font manifest ship; **no glyph images are committed**. Two installs agree because they converted the same font bytes (verified, `D12`) with the same converter, not because a blob was checked in | Committed glyph images (the first draft): tens of thousands of unreviewable binary files, every font tweak a binary diff, and a blob nobody can regenerate; runtime FreeType (cache misses, dependency) |
| **D3** | UTF-8 / NFC / grapheme segmentation | **Vendored `utf8proc` (MIT), pinned** | ICU (enormous, data-heavy, locale surface); hand-rolled tables (a correctness trap) |
| **D4** | Shaping | **Not in v1.** Conversion-time only, if a cluster ever needs it | HarfBuzz at runtime — no required script needs context-dependent shaping; CJK/kana/hangul are one-to-one |
| **D5** | Bidi | **Deferred to V1-6, constrained**: table-driven UAX#9 subset generated at conversion time; until then RTL is refused with a typed diagnostic | fribidi — LGPL conflicts with the static and export paths (V1-19) |
| **D6** | Cell metrics **[revised]** | **8×8, always — one cell size, no profiles.** The converter fits the font to the cell; the user chooses a font that fits. Proven achievable for CJK by existing 8×8 bitmap faces | The first draft's 8×16/12×12/16×16 profiles: unnecessary (8×8 CJK exists), they changed the visual language for no gain, and they let an unfitting font off the hook instead of reporting it |
| **D7** | Glyph identity | **Packed 32-bit `GlyphRef`**: 2-bit kind (blank/transparent/missing/ink), 4-bit face slot, 26-bit catalog index | Per-cell `font_id` byte (register A1a) — cannot express blank vs transparent vs missing distinctly |
| **D8** | Missing glyph **[revised]** | **No fallback at all.** A glyph the cell's declared face lacks is never borrowed from another face, the system, or a substitute character — it renders as the derived sentinel (`X`, red background, purple foreground) and raises one diagnostic per (face, codepoint) | Per-face fallback stack (the first draft): it makes "which font drew this?" unknowable, hides coverage gaps behind a chain nobody audits, and turns one missing glyph into a resolution-order question. Tofu box: quiet enough to ship by accident. Replacement character: silent |
| **D9** | Fonts | **The user's, declared in the manifest.** Misaki (8×8 native, JIS 1st+2nd, M+ licence) is the reference that proves 8×8 CJK and the first thing the converter must handle; Noto Sans Mono/CJK (OFL-1.1) covers Latin and beyond-JIS CJK. Which fonts ship with a given build is a manifest question, not a code question | Unifont (GPL with font exception — export risk); assuming a font fits without converting it |
| **D10** | Unicode version | **Pinned to the vendored data release** (utf8proc + converter), recorded at vendor time | Floating "latest" — makes parity unprovable |
| **D11** | Conversion failure **[new]** | **Never silent: state exactly what did not fit and abort.** Source metrics, cell size, the specific glyph(s) and which bit overflowed. Fitting remedies — `squash` (anisotropic), `scale` (downscale with a declared threshold), `normalize` (re-baseline/advance/pad to the cell) — are **off by default, opt-in per font or per glyph range**, and each prints why it is being offered and what it will cost legibility | Silent auto-fit (hides an unsuitable font until the art is unreadable); refusing without remedy (leaves a 7×7 or 16×16 font stuck when a one-flag fix exists) |
| **D12** | Font verification **[new]** | **A font manifest is the contract**: per font, the name, every file's SHA-256, the source URL, the licence, the version, the declared coverage and the declared source metrics. The converter refuses a file whose hash does not match and prints the URL it expected; `--list-fonts` reports what is installed and what is missing. A generated glyph set records the hash it came from, so a stale set is detectable rather than merely suspicious | "Works on my machine" font sets with no identity (two installs silently differ); trusting filenames or font family names (both are trivially edited and routinely wrong) |


### The manifest and the converter's surface (what `D12` and `D11` look like)

A `key=value` text file, matching the format every other asset here uses (`assets/settings.txt`,
`assets/ui_layouts/*.txt`), one block per face:

```
font=jp
name=Misaki Gothic
file=misaki_gothic.ttf
sha256=<64 hex>
source=https://littlelimit.net/misaki.htm
licence=M+ FONTS
version=2021-05-05
coverage=JIS X 0208
cell=8x8
```

- `sha256=` repeats when a face is several files (per weight or per script), each paired with its
  own `file=`. A face is the set of files the block names, all verified or none.
- `source=` exists so the font can be found again without guessing — it is printed verbatim in
  errors, in `--list-fonts`, and in the generated set's provenance.
- `coverage=` and `cell=` are declarations the converter checks against the file, not notes.

Two commands carry the contract. **Both now exist** (`tools/fontconv.c`); `--convert` lands
with the fit policy below.

| Command | Purpose | State |
|---|---|---|
| `--list-fonts` | What is installed, what is missing, which hashes matched, which `source=` to fetch from | **implemented** — no FreeType needed, so it works on the machine where a mismatch is being diagnosed |
| `--convert <face>` | Verify hashes, convert, report per-glyph failures; refuse rather than guess | **next**: needs the fit policy (`D11`) |

The verifier's output is checked end to end against coreutils: `fontconv --list-fonts` agrees
with `sha256sum` on a 600 KB font file, exits 0 when every declared file matches, 1 when one is
missing or mismatched (printing expected and installed digests plus the declaring `source=`), and
2 when the manifest cannot be read or is malformed (printing the line and the key). A tool a build
step depends on has to distinguish "the fonts are wrong" from "I could not check the fonts", or
the build will repair the wrong thing.

Failure output names the facts, not a verdict — for example: `misaki 8x8: 47 glyphs exceed the
cell (worst: U+9AD9 at 9x8, +1 column); rerun with --squash to compress 1 column, or choose a
font whose declared metrics fit`. Remedies are the opt-in flags from `D11`, and each states what
it will do before it does it.

### The missing-glyph sentinel (what `D8` looks like)

The precedent is already in this codebase: `asset_registry_get_missing_decal_pattern()`
(`src/assets.c`) — a process-lifetime 2×2 `!`/`#` checker for a missing decal reference. Derived on
demand, never serialised, impossible to author or overwrite. A missing glyph follows the same rule.

**The cell: glyph `X`, purple foreground on a red background.** Both wrong at once, so the cell
reads as broken before it reads as a character. The `X` also carries the signal without colour —
it survives greyscale screenshots, colour-blind viewing and a monitor with saturation turned down.

- **Derived, never stored.** It is `GlyphRef` kind `missing` (`D7`): no catalog entry, no new
  identity space, nothing written into a scene, a stream or a save file. A document reopened
  against a thinner font re-derives it, so there is no stale sentinel to trust.
- **Not text.** The contrast floor governs text; this is a diagnostic mark. It is *meant* to be
  ugly, it is outside the look identity, and it is not themeable — a theme that could restyle it
  could also hide it.
- **Colour resolution.** "Purple" resolves to the palette's magenta `#FF2BD6`, the only
  purple-family value the project owns; the background is the palette red `#FF2B2B`. A hue outside
  those two constants is a new token and a look-record amendment, so it is flagged here rather
  than invented quietly.
- **Where it must survive:** runtime, editor preview, glyph-art view, `capture`/restore, and the
  packed `Cell` state — so SMC streams and checksums carry `kind=missing` instead of degrading it
  to a blank.
- **Once, not per frame.** Glyph resolution raises `TSG-ASSET-ENV-0001` once per (face, codepoint)
  per process. A screenful of missing kanji is forty diagnostics, not 41,600 per second.

### Why build-time conversion is not the dependency the governance rule forbids

The rule exists so a *shipped binary* needs nothing outside the pinned image and so identical
inputs produce identical pixels everywhere. Conversion-time-only use satisfies both:

- **Runtime surface unchanged.** No `libfreetype` in the link path, the platform images or the
  exported build. The binary's dependency set stays exactly what it is today.
- **Parity rests on identity, not on a blob.** Same converter version + same font file bytes
  (proved by `D12`'s SHA-256) = same pixels. This is a *weaker guarantee than a committed
  artefact in one way and a stronger one in another*: there is no blob to rot, but the guarantee
  now depends on the converter being deterministic, so determinism is a tested property of the
  tool rather than a fact about a file. The test is mapgen's `--reproduce` idea: convert on a
  second machine (or twice in CI) and compare digests. The research §3 rules apply to the
  conversion, not the frame: no hinting, fixed AA mode, one rounding function, integer baseline.
- **It keeps content out of git that git is bad at.** The rejected alternative put ~272 KB–1 MB of
  generated bitmap data in the repository as tens of thousands of files. Fonts already are the
  input; the converter is the recipe; committing the cake as well invites the recipe and the cake
  to disagree.
- **The shipped build is still self-contained.** A release build converts once and embeds the
  result. Players install nothing and need no font.

The honest costs, since this is the trade that got made:

- **A source checkout needs the fonts present to build glyph data.** That is a real prerequisite
  where the previous draft had none. It must fail loudly and helpfully — the error names the
  missing font, its expected hash and the URL to fetch it from (`D11`, `D12`).
- **The converter becomes load-bearing for every build path**, so it is not optional tooling and
  belongs in the verified tool set with its own tests.
- **FreeType becomes a host tool** on developer and CI machines. Where the *converter* gets it
  from — vendored build or system package — is a packaging detail of the tool, not of the game.

That is strictly less than 33,000 hand-authored glyphs, a runtime dependency on four platforms,
or a repository full of generated images nobody will review.

### Atlas budget (declared, not discovered)

One cell size means one budget line. 1bpp masks over the bounded required set (≈34,000 glyphs):

| Cell | Bytes/glyph (1bpp) | 34k glyphs | With 4-level AA (2bpp) |
|---|---|---|---|
| **8×8** | 8 | **272 KB** | **544 KB** |

**Declared cap: 1 MB resident for the glyph set, whatever faces the manifest declares.** The set
is fully resident (no eviction, no per-frame allocation), so the 8.33 ms budget sees a texture
read, not a raster. A font whose coverage would exceed the cap is a manifest-level discussion, not
a silent overflow. Cold cost exists only at load.


## Consequences for the phases this unblocks

**V1-5 (identity, raster service, cache, SMC migration)** implements D1–D12. Order that respects
the existing rollback point: typed `GlyphRef` + ASCII adapter first (old readers untouched), then
the catalog + human-readable glyph table in the scene, then the converted glyph set behind the same
interface the current atlas has, then SMC adapters. `Cell` grows to 4 bytes of glyph; that touches
dirty comparison, packed state, stream signatures, checksums, restore behaviour and every SMC
adapter — the list V1-5 item 5 already names, unchanged by this decision.

**V1-6 (Unicode UI, editing, clipboard, IME)** gets three of its groundwork pieces from here:
strict UTF-8 + NFC + segmentation (`utf8proc`), the missing-glyph diagnostic it must surface, and
the RTL refusal that stands in until bidi lands. Paste validation shares the same validator as
typed text (register C1a) — this decision adds no second path.

**V1-7 (glyph-art formats)** inherits D6/D7: one art cell is one face slot + index at 8×8, so
existing art and the 8×8 aesthetic survive the migration by construction.

**Not decided here, deliberately:**

- **Which fonts ship in the default manifest** (research item 2) — Misaki is the reference case and
  Noto the coverage case, but the shipping set, its licences and its exact file hashes belong to
  the manifest the converter is delivered with, not to a decision made before the converter exists.
- **Bidi implementation** — V1-6, constrained as in D5.
- **Where the converter gets FreeType from** — a packaging detail for the tool, not for the shipped
  product; either choice keeps the runtime surface at zero. Recorded so it is not mistaken for an
  unconsidered gap.
- **What "legible at 8×8" means for a given script** — that is a judgement per font. The converter
  reports facts (overflow, clipping, coverage); it cannot rule on taste, and it must not pretend to
  by silently rescaling.

## Risks, stated plainly

- **The converter is new code with new failure modes** (font parsing, hash verification, fitting
  remedies, reproducible output). Mitigation: its first test is determinism — convert twice/fleet-wide
  and compare digests — and every failure path is required to name the font, the glyph and the
  reason.
- **Nothing is committed, so a build can fail where the previous draft would not.** Fonts must be
  present and hash-correct before glyph data can be produced. Mitigation: `--list-fonts` makes the
  state inspectable, the error names the URL, and CI proves a clean checkout + documented font
  install is sufficient.
- **Coverage is now the user's choice, so two installs may render different character sets.** That
  is intended — but it means "the game supports Japanese" is a manifest claim, never a code claim.
  The missing-glyph state must stay visible so the difference can never pass as a bug in the art.
- **`utf8proc` is a real runtime dependency** — small, MIT, pure C, no allocation surprises, but it
  is pinned and image-owned like everything else, and it must be added to the vendoring records
  before use.

## Exit-gate mapping (V1-4)

| V1-4 exit-gate item | State |
|---|---|
| Dependencies selected | **decided** (D1–D4, D10) |
| Unicode version | **decided as a rule** — pinned to vendored data at vendor time (D10) |
| Fallback fonts | **answered** (D9, D12): the user's fonts, declared and verified per manifest — and no fallback chain exists at all (D8) |
| Missing-glyph behaviour | **decided** (D8): derived `X` sentinel, red background, purple foreground, one diagnostic per (face, codepoint); never serialised |
| Raster profile | **answered** (D6 revised): 8×8 only, no profile question left open |
| Parity contract | **decided** — font-hash + converter determinism, with the research §3 rules applied at conversion (D2, D12) |
| Resource budgets | **declared** — 1 MB resident glyph set, fully resident |
| Glyph identity and format direction | **decided** (D7) |
| Failure behaviour | **decided** (D11): abort with the exact reason; fitting is opt-in |

V1-4 is **closed**. V1-5 is **unblocked** and may begin at its first task (the converter, its
determinism test, and the font manifest) without further decisions.

