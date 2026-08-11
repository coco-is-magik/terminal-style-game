# R4 Increment D Implementation Record — 2026-08-10

## Start here

**Increment D is complete. R4 remains Active. Increment E is next.**

### Next action

Add focused renderer tests that lock the current hard-coded floor and ceiling
backgrounds before replacing them with authored per-cell material sampling.

Do not start workflow closeout or mark R4 Verified yet.

## Delivered

1. **Typed surface inspectors**
   - Wall, floor, and ceiling inspectors share three typed fields: Material,
     Construction, and Ambient.
   - Up/Down selects a field; Left/Right edits material or ambient; Enter applies.
2. **Command-only authored edits**
   - Wall/floor/ceiling materials, Place/Remove Wall, and ambient changes remain behind
     typed requests and `CommandHistory`.
   - Missing material references remain visible; an empty material registry is a safe,
     explicit UI state.
3. **Ambient controls**
   - Ambient supports bounded `0.05` steps and direct numeric values in `0.00..1.00`.
   - Step, direct entry, undo, redo, runtime refresh, invalid input, and Escape behavior
     are covered.
4. **Failure-atomic runtime refresh**
   - Runtime-affecting edits build a candidate runtime before replacing the live view.
   - A failed runtime build rolls back authored state and restores history count, cursor,
     and next state ID while preserving runtime, selection, and inspector state.
5. **Construction selection lifecycle**
   - Spawn/player/decal safety remains enforced without UI exceptions.
   - A successful Place/Remove Wall closes the stale surface inspector only after the
     runtime commit succeeds. Validation and runtime-build failures preserve the UI.

## Four regressions resolved

| Regression | Resolution |
|---|---|
| Light edit used Down as a value gesture | Test now exercises the accepted Up/Down-field and Left/Right-value contract while retaining authored/runtime/history assertions. |
| Exit Save-and-Exit used Right | Test now uses the modal's accepted next-choice action and retains the full persistence workflow. |
| Phase-6 material picker used Down | Test now edits the Material field with Right and retains apply/undo/redo/save/reload assertions. |
| Construction targeted the spawn cell | Test retains atomic spawn rejection, adds injected runtime-build rollback, then proves successful construction on a valid cell. |

The stronger construction regression exposed and fixed a production defect: bounded
horizontal selection alone did not become invalid after occupancy changed. The
controller now closes construction selection only after authored and runtime commit.

## Verification evidence

The Makefile remained unchanged:

```text
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror
```

| Gate | Result |
|---|---:|
| Strict editor-domain | 8/8 passed |
| Strict unified-editor | 52/52 passed |
| `make check` | passed |
| Sequential `make asan` | passed; no sanitizer diagnostics |
| Sequential `make ubsan` | passed; no sanitizer diagnostics |
| Sequential `make matrix` | 8/8 modes passed |
| Final normal strict `make check` | passed |
| `git diff --check` | passed before documentation closeout |

Sanitizer and matrix targets ran sequentially because they clean and reuse `build/`.
The final check restored the normal strict build state.

## Scope preserved

- The typed domain seam remains pure, headless, allocation-free, and request-only.
- `SceneDocument` remains authored truth; runtime maps and `WorldState` remain derived.
- Material changes do not create geometry.
- No spawn/player/decal safety rule was weakened.
- No slope, variable height, Z movement, per-face wall material, or authored horizontal
  rendering was introduced.
- R4 is not Verified until Increments E and F plus the planned acceptance/review gates
  are complete.

## Increment E checklist

1. Lock current out-of-bounds floor/ceiling backgrounds with exact framebuffer tests.
2. Add authored wall/floor/ceiling material and lighting checksum fixtures.
3. Implement bounded per-cell horizontal material lookup without avoidable hot-loop
   resolution work.
4. Verify missing materials, extreme pitch, decals, highlights, and map dimensions.
5. Run renderer benchmark/stability, ASan, UBSan, matrix, and final strict checks.
