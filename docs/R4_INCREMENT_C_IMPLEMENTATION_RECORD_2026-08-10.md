# R4 Increment C Implementation Record — 2026-08-10

## Status

**Complete.** R4 remains Active; Increment D is next. This record is the current R4
handoff.

## Delivered

1. **Typed horizontal targets**
   - Added distinct `SELECTION_FLOOR` and `SELECTION_CEILING` variants with bounded
     cell coordinates.
   - Existing wall-face identity and stable light IDs remain unchanged.
2. **Fixed-plane center picking**
   - The center-screen sample reuses the renderer's fixed-plane distance and
     `Camera.pitch` horizon-offset model.
   - Negative horizon offset selects floor; positive selects ceiling; level/near-horizon
     view produces no horizontal candidate.
   - Non-finite, invalid viewport, over-range, and out-of-bounds samples preserve the
     existing wall/light candidate.
3. **Occlusion and precedence**
   - Wall DDA and light picking retain their existing behavior.
   - Horizontal candidates replace an existing target only when strictly nearer.
   - A wall at or before the plane sample occludes that horizontal target.
4. **Derived highlights**
   - Floor and ceiling use visibly distinct selected/hover glyphs.
   - Highlight projection is allocation-free, bounded by grid/map dimensions, and uses
     the same fixed-plane and wall-occlusion math.
   - Selection wins over hover for the same target; borrowed camera/map/light inputs are
     not mutated.
5. **Controller composition**
   - Per-frame hover now composes wall, light, then horizontal candidates.
   - Floor/ceiling selections remain valid without opening an Increment D inspector.

## Test-first findings

- Initial strict linking failed because the new picker API had no implementation; this
  established the expected red baseline.
- Initial cardinal test expectations for west/north were one cell too far because C
  truncation of the exact positive sample was miscalculated; expectations were corrected
  to the renderer-equivalent cell.
- Strict compilation caught a helper-order implicit declaration and a fuzzy controller
  patch placement before broad tests.
- The first controller fixture selected the nearer wall, proving precedence worked; the
  test was changed to use a genuinely nearer steep-pitch horizontal sample.
- World-to-cell conversion now uses `floor()` so negative out-of-bounds coordinates do
  not truncate into cell zero.

## Verification evidence

| Gate | Result |
|---|---:|
| Strict editor-selection | 21/21 passed |
| Strict editor-highlight | 16/16 passed |
| Strict unified-editor | 49/49 passed |
| Final `make check` | 28/28 runners passed |
| Sequential full `make asan` | 28/28 runners passed; no diagnostics |
| Sequential full `make ubsan` | 28/28 runners passed; no diagnostics |
| Sequential `make matrix` | 8/8 modes passed |
| Final normal strict rebuild | passed |
| `git diff --check` | passed before documentation closeout |

## Performance and stability

The headless highlight benchmark now contains six deterministic scenarios, including
floor and ceiling targets.

| Run | Iterations | Average | Budget | Result |
|---|---:|---:|---:|---:|
| Benchmark | 20,000 | 0.216897 ms/scenario | 1.000 ms | passed |
| Stability | 100,000 | 0.217257 ms/scenario | 1.000 ms | passed |

Both runs reported deterministic checksums. These are headless highlight measurements,
not the renderer hot-path benchmark required for Increment E.

## Scope preserved

- Horizontal targets have no inspector or material controls yet; that is Increment D.
- Floor/ceiling rendering still uses the current constant backgrounds; authored material
  sampling and renderer benchmarks are Increment E.
- No angular pitch, height, slope, Z movement, or per-face wall material was introduced.

## Start Increment D here

1. Extend `editor_domain` with floor/ceiling surface, construction, and ambient adapters.
2. Add typed inspector metadata and request-construction tests before controller UI work.
3. Preserve loaded-material enumeration, numeric entry, repeat behavior, consumption,
   and Escape hierarchy.
4. Add transactional runtime-rebuild failure coverage around successful authored
   commands before closeout.
