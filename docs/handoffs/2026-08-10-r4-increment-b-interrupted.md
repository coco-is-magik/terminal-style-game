# R4 Increment B Interrupted Handoff — 2026-08-10

> **Superseded:** Increment B completed on 2026-08-10. Use
> `../R4_INCREMENT_B_IMPLEMENTATION_RECORD_2026-08-10.md`; Increment C is next.

## Status

**In progress and unverified.** Increment A is complete. Increment B source work was
interrupted after the initial command/document seam compiled, but before Increment B
tests, controller integration, or closeout gates were completed.

Do not mark Increment B complete from the current source diff.

## Next action

Add focused failing tests to `tests/test_command_system.c` for typed surface, ambient,
place-wall, and remove-wall execute/undo/redo behavior before changing more production
code.

## Repository state reviewed

- Branch: `dev`, based on `0638a0e`, tracking `origin/dev`.
- The working tree contains the complete uncommitted Increment A changes plus partial
  Increment B source changes.
- `git diff --check` passed during this review.
- No Increment B implementation record exists because the increment is not complete.
- `README.md` and `LICENSE` were not changed during this review.

## Partial Increment B source present

The interrupted diff currently adds:

1. typed mutation kinds for wall/floor/ceiling materials, ambient intensity, wall
   placement, and wall removal;
2. `CommandExecutionContext` with borrowed assets and an optional current-player cell;
3. checked execute/undo/redo entry points and typed convenience wrappers;
4. authored surface/occupancy queries and wall-decal attachment lookup; and
5. internal surface, occupancy, and ambient mutation helpers that keep the compatibility
   `Map` synchronized with authored occupancy/wall material.

These are implementation facts, not completion evidence.

## Verification actually performed

The affected strict runners compiled after the partial source changes:

- `build/test-command-system`
- `build/test-editor-domain`
- `build/test-unified-editor`

Their pre-existing suites then reported:

| Runner | Result |
|---|---:|
| command system | 22/22 passed |
| editor domain | 6/6 passed |
| unified editor | 47/47 passed |

These suites contain no focused Increment B command tests. They establish only that the
partial API did not break the existing covered behavior. They do not verify the new
surface, ambient, construction, attachment, spawn, or player-safety paths.

The command runner wrapper reported a nonzero tool status after printing a complete
22/22 pass because the shell session closed around the trailing `exit`; use a direct
fresh build/run when resuming rather than treating the wrapper status as product failure.

## Required work before Increment B can complete

### 1. Command regression tests

Add deterministic coverage for:

- wall/floor/ceiling material normal, no-change, invalid, and unloaded-material cases;
- ambient range, no-change, exact execute/undo/redo restoration;
- place/remove normal and no-change behavior with latent surfaces preserved; and
- duplicate authored-field rejection, grouped rollback, state exhaustion, history OOM,
  redo invalidation, and cursor/state preservation on failure.

### 2. Construction safety tests

Cover execute, undo, and redo for:

- authored spawn collision;
- current-player collision through explicit context;
- wall decal attachment blocking removal; and
- floor/ceiling decals not blocking wall removal.

### 3. Review the partial core implementation

Before accepting it, specifically check:

- safety preflight for grouped occupancy transitions that target related cells;
- repair-diagnostic refresh and rollback behavior when assets are supplied;
- compatibility of the historical context-free wall-material wrapper; and
- whether duplicate rules correctly distinguish separate surface fields while rejecting
  conflicting occupancy requests on one cell.

### 4. Controller integration boundary

Increment B requires the controller to compose current-player/assets context and rebuild
derived runtime state after successful occupancy or ambient commands. The current
`unified_editor` still calls context-free execute/undo/redo, maps none of the new command
results to status text, and refreshes runtime only for light commands.

Inspector controls and horizontal selection remain Increment C/D scope; do not pull them
into Increment B merely to exercise the command API.

### 5. Closeout gates

After focused tests pass, run:

```sh
make -B build/test-command-system build/test-editor-domain build/test-unified-editor
./build/test-command-system
./build/test-editor-domain
./build/test-unified-editor
make check
make asan
make ubsan
make matrix
git diff --check
```

Record exact results in a new Increment B implementation record. Do not reuse Increment
A gate counts as Increment B evidence.

## Known risks

- New result codes may shift assumptions in status mapping or tests that treat the enum
  as exhaustive.
- Refreshing repair diagnostics during each surface apply can make grouped rollback
  behavior observable and needs direct tests.
- Player safety is only effective when the interactive caller supplies current camera
  cell context; context-free wrappers cannot enforce it.
- Occupancy changes update collision immediately through the compatibility `Map`, so a
  failed grouped command must restore both authored and derived values exactly.

## Stop reason

The prior implementation session was interrupted. This review intentionally documents
the partial state instead of continuing implementation under a repo-status request.
Further research would not change the next safe action: add the missing Increment B
command tests, use their failures to review/fix the partial core, then integrate the
controller context and runtime refresh.
