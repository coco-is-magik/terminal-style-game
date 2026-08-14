# R7 Increment I2 Implementation Record — 2026-08-13

## Status

**Complete and verified.** Map-size and command-history memory limits are explicit,
resize and batch allocation failures are transactionally covered, and large-map /
deep-history / selection-revalidation stress tests pass. R7 I1 and I2 are complete,
so R7 is Verified.

## Delivered hardening

1. Map dimensions remain explicitly bounded at `512 × 256`; native parsing and
   resize enforce the same constants and resize uses `checked_size_2d` before all
   candidate-array allocations.
2. Retained command history is capped at **16 MiB**, including command-array
   capacity and retained wall-decal removal snapshots. Prospective growth is
   preflighted before allocation or mutation.
3. History-cap rejection returns `CMD_RESULT_HISTORY_LIMIT`, leaves authored
   state/history unchanged, and displays "Command history memory limit reached."
4. A read-only `command_history_retained_bytes()` diagnostic exposes exact retained
   accounting without exposing mutable internals.
5. Resize candidate allocations have a test-only allocator seam. Failure of each
   of the three allocations preserves original pointers, dimensions, growth
   provenance, and content.
6. Selection-set revalidation drops invalid members; a surviving primary remains
   primary, and if primary is removed the last surviving member becomes primary.

## Tests added

- Exact `512 × 256` authored/map/light allocation and east/south growth rejection.
- Each resize candidate-allocation failure boundary with pointer/content rollback.
- Deep alternating command history until deterministic 16 MiB policy rejection;
  retained bytes never exceed the cap and rejection is atomic.
- Selection revalidation where a companion and then the primary are removed.
- Existing I1 8-member batch validation/allocation rollback remains active.

## Verification evidence

Final checks run 2026-08-13:

| Check | Result |
|---|---|
| `make -B all` | **Pass** (`-Wall -Wextra -Wpedantic -Werror`) |
| `test-command-system` | **37/37** |
| `test-scene-document` | **44/44** |
| `test-editor-selection` | **22/22** |
| `test-unified-editor` | **73/73** |
| `make test` | **434/434 across 31 suites** |
| `make asan` | **Pass** |
| `make ubsan` | **Pass** |
| `make matrix` | **8/8 configurations** |
| `make smoke` | **Pass** — `{"smoke":"ok","map_width":10,"map_height":6}` |

## Static audit

- No unchecked map-dimension multiplication was added; resize allocation count is
  produced by `checked_size_2d`.
- UI/controller code does not directly mutate authored cell/light/decal arrays.
- History accounting includes command capacity and retained decal snapshots, and
  excludes redo entries that a successful post-undo command discards.
- Selection remains editor-transient; scene v4 and migration behavior are unchanged.

## Remaining follow-up

Attended I1 interaction review passed. I2 changes no interaction behavior; no
additional manual correctness gate is open. Generalized multi-boundary resize
batches remain deferred unless a later workflow needs them.