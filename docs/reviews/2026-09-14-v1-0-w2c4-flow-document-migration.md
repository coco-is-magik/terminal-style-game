# V1-0 W2-C4 Flow Document Persistence Migration — 2026-09-14

## Outcome

**W2-C4 complete. Flow saves now use `platform_fs` for file sync, destination inspection,
and replacement. Pre-commit failures preserve destination bytes and the complete document;
committed durability warnings update path and saved/dirty identity without being reported as
unsaved by the flow workspace.**

This is the fourth owner-only W2-C increment selected by
[`../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md`](../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md).
Authored UI, decal, sprite, directory, catalog, and locale owners remain unchanged.

## Contract extensions

`FLOW_DOCUMENT_OK_DURABILITY_WARNING` was appended to `FlowDocumentResult`, preserving all
existing enum numbers. `flow_document_result_is_committed` identifies ordinary and warning
commits.

`FLOW_WORKSPACE_OK_DURABILITY_WARNING` was appended to `FlowWorkspaceResult`, also preserving
existing values. `flow_workspace_result_is_committed` ensures a warning save:

- updates the live and saved workspace documents;
- rewrites saved-state and path identity in retained history snapshots;
- closes a dirty workspace after a committed Save-and-close action;
- remains visible to the editor as a durability warning rather than a failed save.

## Implementation

### `src/flow_document.c`

- Direct descriptor `fsync` is replaced by `platform_fs_sync_file`.
- `platform_fs_inspect_nofollow` determines whether the destination exists.
- Direct `rename` is replaced by `platform_fs_replace`.
- Not-committed failure removes the temporary candidate and returns the existing I/O error.
- Either committed state updates `document.path` and marks the current document state saved.
- A committed durability warning returns the appended warning result.

Graph validation, exact serialization order and bytes, same-directory temporary creation,
stream write/flush/close, transactional load, and graph mutation remain owned by the flow
module.

### Fault injection

Added `src/flow_document_internal.h` and `src/flow_workspace_internal.h` with explicit
per-call sync, replacement, and durability fault paths. No global mutable failure state was
introduced.

### Workspace and editor feedback

`flow_workspace_save_as` now commits its candidate for either committed document result and
returns a distinct workspace warning for warning commits. The editor renders "Flow saved;
durability warning" for that result. Uncommitted outcomes still return
`FLOW_WORKSPACE_SAVE_FAILED`.

### `Makefile`

Standalone flow and UI runners that link `flow_document.c` now link `platform_path` and
`platform_fs` explicitly. Combined project-catalog and unified-editor runners already receive
the platform sources through scene-document groups, so they were not duplicated. Runner count
remains 63.

## Test coverage

`tests/test_flow_document.c` now has nine tests covering:

- existing graph construction, validation, round trip, and transactional load;
- exact canonical bytes and LF endings;
- sync/replacement failure preserving old destination bytes;
- complete path/snapshot/dirty-state preservation before commit;
- warning commit updating path and clean state;
- document committed-result classification.

`tests/test_flow_workspace.c` now has ten tests covering:

- workspace committed-result classification;
- warning propagation through `last_document_result`;
- warning commit updating live/saved documents;
- retained-history path and saved-state identity updates;
- existing save/load, undo/redo, dirty-close, mutation, and catalog behavior.

## Verification

| Check | Result |
|---|---|
| Strict GCC flow-document runner | `PASS`, 9/9 |
| Strict Clang flow-document runner | `PASS`, 9/9 |
| Strict GCC flow-workspace runner | `PASS`, 10/10 |
| Strict Clang flow-workspace runner | `PASS`, 10/10 |
| Flow-document ASan + leak detection | `PASS`, 9/9, no report |
| Flow-document UBSan | `PASS`, 9/9, no report |
| Flow-workspace ASan + leak detection | `PASS`, 10/10, no report |
| Flow-workspace UBSan | `PASS`, 10/10, no report |
| Flow reference/runtime/binding/scene-adapter | `PASS`, 3/3, 3/3, 5/5, 2/2 |
| Unified-editor runner | `PASS`, 97/97 |
| Strict GCC/default-SMC application build | `PASS`, explicit status 0 |
| Strict Clang/default-SMC application build | `PASS`, explicit status 0 |
| Complete runner build | `PASS`, explicit status 0 |
| Complete `make test` | `PASS`, explicit status 0 and 63 pass markers |
| `make standards-core` | `PASS`, explicit status 0 |
| `make test-platform-harness` | `PASS`, explicit status 0 |

### Native Windows profile

Executed without an external timeout:

```sh
PROFILE_VM_NAME=win10-survey make platform-test-windows
```

Observed:

- isolated native platform preflight remains PE x86-64, import-clean, and 9/9 passing;
- strict application compilation includes `flow_document.c` with zero diagnostics;
- its previous direct `fsync` error is absent;
- full product remains `FAIL-PRODUCT` in later unmigrated owners:
  `decal_document`, `map_catalog`, `scene_format`, `sprite_document`, `ui_document`,
  `ui_menu_workspace`, and `unified_editor`;
- VM cleanup passed and final state is `shut off`;
- the tracked Python cache modified by native execution was restored from `HEAD`.

## Preserved invariants

- Graph validation ordering and all previous result values are unchanged.
- Exact flow bytes and transactional load behavior are unchanged.
- Pre-commit failure preserves destination, path, graph, and dirty/saved state.
- Committed data is never reported as unsaved.
- Save As path identity changes only after commit.
- Existing workspace undo/redo and saved snapshot ownership remain intact.
- No unrelated persistence owner changed.
- SMC, mirror behavior, checked-in assets, and benchmark budgets are unchanged.

## Limitations

1. Focused owner fixtures remain POSIX-shaped (`mkstemp`, `unlink`), so owner runners were not
   executed natively on Windows. Native evidence is diagnostic-free strict owner compilation
   plus the 9/9 platform preflight.
2. Temporary creation, stream opening, cleanup, and load retain current CRT narrow-path calls.
   Broad Unicode flow-path ownership is later v1 work.
3. Existing-destination replacement on Windows remains conservatively a committed durability
   warning according to the accepted platform contract.
4. Cross-volume replacement remains unavailable in the single-volume VM and is not required
   by the same-directory flow transaction.

## Next safe action

Proceed to **W2-C5 — `ui_document` persistence migration**. Preserve authored UI validation,
exact bytes, path and saved/dirty identity, transactional load, and flow-reference behavior.
Add per-call sync/replacement/durability fault coverage and update UI workspace/editor handling
so committed warnings are never reported as failed saves.

W2-D directories/sprite folders, W3 catalogs, W4 locale conversion, pinned-SMC remediation,
and P1 performance remain independent. Do not externally timeout platform profiles.