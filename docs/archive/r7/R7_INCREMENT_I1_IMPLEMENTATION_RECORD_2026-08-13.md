# R7 Increment I1 Implementation Record — 2026-08-13

## Status

**Complete and verified.** Ctrl+arrow surface multiselect, distinct primary
highlighting, and atomic Apply material / construction / Add decal batches are
implemented. I1's deterministic exit gate and Q2 pass; aggregate tests,
sanitizers, the configuration matrix, and headless smoke also pass. R7 remains
Active; I2 limits/stress hardening is next.

## Delivered behavior

1. Selecting a wall/floor/ceiling starts an editor-transient selection set; the
   last member is primary. Ctrl+arrow extends one face at a time.
2. Floor/ceiling extend on map X/Y. North/south wall faces extend along X;
   east/west faces extend along Y. Wrong-axis, out-of-bounds, empty-wall, and
   occluded candidates reject without changing the set.
3. All members are outlined; companions use the dashed style and primary uses
   the solid selected style. Overlay reports set count and primary.
4. Multiselect reduces actions to Apply material, contextual construction
   (remove selected walls / place walls on selected horizontal cells), and Add
   decal. Ambient and existing-decal selection are hidden.
5. Add decal creates one stable-ID instance per selected surface. Every batch is
   one command-history step and undo/redo restores the entire set atomically.

## Implementation boundaries

- `EditorSelectionSet` is fixed at 8 members, matching
  `EDITOR_COMMAND_MAX_MUTATIONS`; no unbounded allocation was introduced. Limit
  rejection is visible. I2 may replace this with explicit memory accounting.
- The set is transient and not serialized; scene v4 is unchanged.
- Batch decal IDs are allocated inside `command_system` and roll back on failed
  validation/history allocation. Distinct insert-decal IDs may coexist in one
  group; duplicate IDs remain rejected.
- Runtime-affecting batches use `editor_command_commit_runtime`, so runtime-build
  failure rolls document/history back.
- Interior wall-removal batches are atomic. East/south boundary wall batches
  reject as resize-blocked rather than bypassing R4 growth provenance or partly
  applying; generalized bounded resize batches remain I2 follow-up.
- Selection revalidation drops invalid face members after resize and resolves a
  surviving primary safely.

## Verification evidence

Final checks run 2026-08-13:

| Check | Result |
|---|---|
| `make -B all` | **Pass** (`-Wall -Wextra -Wpedantic -Werror`) |
| `test-command-system` | **36/36** |
| `test-input` | **12/12** |
| `test-editor-selection` | **22/22** |
| `test-editor-highlight` | **18/18** |
| `test-unified-editor` | **73/73** |
| `make test` | **432/432 across 31 suites** |
| `make asan` | **Pass** |
| `make ubsan` | **Pass** |
| `make matrix` | **8/8 configurations** |
| `make smoke` | **Pass** — `{"smoke":"ok","map_width":10,"map_height":6}` |

## Remaining follow-up

An attended visual review was subsequently completed and **confirmed working as
intended** (2026-08-13), including Ctrl+arrow direction feel, companion vs.
primary readability, occlusion stopping, and reduced-menu clarity. I2 is complete
and verified in `R7_INCREMENT_I2_IMPLEMENTATION_RECORD_2026-08-13.md`.