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
| **D11** | Conversion failure **[new, implemented]** | **Never silent: state exactly what did not fit and abort.** Source metrics, cell size, the specific glyph(s) and which bit overflowed. Fitting remedies — `squash` (anisotropic), `scale` (downscale with a declared threshold), `normalize` (re-baseline/advance/pad to the cell) — are **off by default, opt-in per font or per glyph range**, and each prints why it is being offered and what it will cost legibility. Policy and pixel behaviour are `src/font_fit` (see below); the CLI wiring lands with `--convert` | Silent auto-fit (hides an unsuitable font until the art is unreadable); refusing without remedy (leaves a 7×7 or 16×16 font stuck when a one-flag fix exists) |
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
| `--convert <face> --out PATH` | Verify hashes, convert, report per-glyph failures; refuse rather than guess | **implemented** — FreeType at build time only; `--fit` selects a remedy, `--size` the decode size, `--file` the file of a multi-file face |

The verifier's output is checked end to end against real vendor files. Downloaded
`misaki_ttf_2021-05-05.zip` from <https://littlelimit.net/misaki.htm> (794,317 bytes, matching the
size the page states), extracted three 1.1 MB TTFs, and generated a **local** manifest from their
real digests — never committed, because D2 says the bytes do not live here:

| Case | Result |
|---|---|
| Three real files, two faces (one with two files) | `PASS`, exit 0 |
| `misaki_mincho.ttf` copied over `misaki_gothic.ttf` (wrong style fetched) | `HASH-MISMATCH`, both digests, exit 1 |
| `misaki_mincho.ttf` deleted | `MISSING` + "obtain it from <url>", exit 1 |
| Example manifest with placeholder digests | parses; every file reports `HASH-MISMATCH` against the real installed digest |

`assets/fonts.example.txt` is what ships instead of a manifest: the Misaki shape, the required
keys, and a passing run's output — with 64-zero digests, so it identifies nothing and cannot be
mistaken for a claim about anyone's bytes. A test parses that file and asserts its digests are
still placeholders, so it cannot rot into a stale claim; filling it in with the real digests
verifies the real archive, which is how its shape was confirmed rather than assumed.

A tool a build step depends on has to distinguish "the fonts are wrong" from "I could not check
the fonts", or the build will repair the wrong thing, so the three exit codes are the interface.

Failure output names the facts, not a verdict — for example: `misaki 8x8: 47 glyphs exceed the
cell (worst: U+9AD9 at 9x8, +1 column); rerun with --squash to compress 1 column, or choose a
font whose declared metrics fit`. Remedies are the opt-in flags from `D11`, and each states what
it will do before it does it.

### The fit policy (what `D11` looks like, and now is)

`src/font_fit` is the policy and nothing else: a decoded 1bpp mask in, either an 8×8 cell out or a
refusal carrying the overshoot. No allocation, no FreeType, no printing — so the behaviour is
testable with synthetic masks and identical on every machine. Four modes, and the difference
between them is a real decision rather than an ordering:

| Mode | What it does | Right for | Cost |
|---|---|---|---|
| `none` | the strike must already be ≤ 8×8; placed at its own origin | native 8×8 bitmap faces (Misaki) | none — and it is the default |
| `squash` | anisotropic nearest-neighbour, applied **only to axes that overflow** | 9×8, the one-column-off case | drops the unsampled column/row |
| `scale` | isotropic box filter by the integer factor that brings the longer axis to 8, then centre | 16×16 strikes | thickens hairlines |
| `normalize` | crop to ink, centre horizontally, ink bottom on row 7. **No resampling** | "present but placed for a different box" | none, but refuses when the ink itself is too big |

Three decisions inside that worth knowing:

- **Squash does not touch an axis that already fits.** Stretching a small bitmap up to the cell is
  a different and usually wrong choice, so a mode named squash does not make it on the side.
- **The scale threshold is a quarter, not a half.** At 8×8 a hairline that survives is worth a
  slightly bolder stroke; a half-coverage threshold erases 1px strokes entirely, which produces a
  legible-looking font with missing strokes — that reads as a broken glyph, not a scaled one. A
  test scales a 16×16 one-pixel ring and asserts the ring is still a 1px ring at 8×8.
- **The refusal names the remedy and the cost**, and for `normalize` it quotes the *ink* overshoot
  rather than the strike's: a 16×16 strike holding 10×10 of ink is two columns too wide, not eight,
  and a message that says eight sends the user to the wrong fix.

### What `--convert` does, and what it measured

Decodes with FreeType at build time, fits through `src/font_fit`, and writes a deterministic set:
`"TSGFONT1"`, cell size, entry count, a provenance digest, then 16-byte entries sorted by
codepoint (u32 codepoint, u8 mode, u8 rows[8], padding). Hashing the output is the reproducibility
check; two runs are byte-identical.

Measured on the real Misaki archive, which is the case the whole design was built to survive:

| Case | Result |
|---|---|
| `misaki_gothic.ttf`, default | **embedded 8×8 bitmap strike** used, 7,171 glyphs, **0 refused, 0 unsupported**, all `none` |
| same, run twice | **byte-identical** output and digests |
| same, `--size 16` | rendering outlines at 16 px: **7,094 refusals**, eight named with exact overshoot (`U+0021 2x12 … 0 column(s) and 4 row(s)`), **no set written**, exit 1 |
| same, `--size 16 --fit squash` | 7,171 converted; strokes double — visibly cruder |
| same, `--size 16 --fit scale` | 7,171 converted; **full-width glyphs reproduce the font's own 8×8 bitmaps byte for byte** (U+3042 identical); half-width glyphs match in shape but sit centred rather than at their origin, which is exactly the cost `scale` states |
| `NotoColorEmoji.ttf` | refuses: no 8×8 strike, no 8 px size, and it lists the strikes it does have (109×109 colour) |

Two things that came out of running it rather than reasoning about it:

- **The strike is used when it exists.** Misaki's TTF carries its own 8×8 bitmaps, so the converter
  takes them instead of re-rasterising outlines — the designer's pixels, not a guess at them. The
  path is printed every run, because which path a face takes is a fact about that face.
- **`scale` at factor 2 reproduces native 8×8 glyphs.** That is independent evidence for the
  quarter-coverage threshold: a 16 px outline of あ, box-filtered to 8×8, lands on precisely the
  bitmap the font ships. A half-coverage threshold would have produced a different glyph, and a
  blank one for hairlines.

Also reported, not decided: the advance distribution (7,003 full-width, 168 half-width for
Misaki). That is the input the width-class question (register B2) needs, and it is why the tool
prints it rather than hiding it.

**Cell bit order is the renderer's, and pinned by test:** bit 0 is the leftmost pixel, matching
`renderer.c` (`row_pixels[0] = (row & 1)` … `row_pixels[7] = (row & 128)`) and `glyph_atlas.c`.
Reversing it would mirror every glyph in the game while any test that only counted ink still
passed, so `test_font_fit` asserts both corners of the cell.



### The set reader, and the determinism check

`src/glyph_set` is the only definition of the set's byte layout: `fontconv` encodes through it and
the runtime will decode through it, so producer and consumer cannot drift the way two
implementations of one format always do. Decoding is strict for the reasons a file from a build
step deserves: magic, cell size, exact length (trailing bytes are a *different* file, not a longer
one), count ceiling before any size arithmetic, ascending and unique codepoints (which is what makes
lookup a binary search and the writer deterministic), and mode values the fit policy can actually
produce. Each rejection is a distinct named result with the offending entry's index and codepoint,
and no rejection writes through the caller's pointer.

`fontconv --check-set PATH` closes the loop: it validates a converted set with that reader and
prints sample glyphs as ASCII art. Verified against the real artefact — the 7,171-glyph, 114,784-byte
Misaki set reads back with the right provenance and the right pixels, a truncated copy reports
`TRUNCATED`, a TTF passed by mistake reports `BAD_MAGIC`, and an absent file is distinguishable from
a corrupt one by exit code (2 versus 1).

Sharing the encoder cost nothing: after moving the format into `src/glyph_set`, converting Misaki
again produced **the same digest as before the refactor** (`5e85b3e4…`), which is the check that
made the move safe rather than hopeful.

**`make font-determinism FACE=<face>`** is the CI-shaped version of that claim: convert twice,
`cmp` the bytes, then validate. When fonts are absent it reports `FAIL-TOOL` naming the missing
manifest, per `VERIFICATION_POLICY.md`, rather than passing vacuously — a determinism check that
silently skips is worse than no check, because the green line is believed.



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

