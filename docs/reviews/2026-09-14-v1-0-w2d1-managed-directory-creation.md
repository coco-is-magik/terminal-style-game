# V1-0 W2-D1 Managed One-Level Directory Creation — 2026-09-14

## Outcome

**W2-D1 complete. `platform_fs_ensure_directory` now provides non-recursive, no-follow managed
directory creation with strict UTF-8/native conversion on Windows. `ui_menu_workspace` and
`unified_editor` use it without
changing their domain-visible failure contracts or caller-owned state on failure.**

This increment implements the D8 contract from
[`../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md`](../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md).
Sprite-folder publication, catalog enumeration, locale conversion, and unrelated directory
fixtures remain unchanged.

## Platform contract

`platform_fs_ensure_directory(path, mode, error)`:

- creates exactly the requested directory and never creates missing parents;
- accepts an existing ordinary directory without changing it;
- rejects an existing regular file with `PLATFORM_FS_WRONG_TYPE`;
- rejects POSIX symbolic links and Windows reparse points with `PLATFORM_FS_WRONG_TYPE`;
- uses the supplied mode only for POSIX creation;
- uses `CreateDirectoryW` and strict UTF-8/native conversion on Windows;
- reclassifies an already-existing object without following links if creation races;
- preserves typed native error context for failed creation;
- never infers, repairs, or recursively creates another path.

`PLATFORM_FS_FAULT_ENSURE_DIRECTORY` was appended to the private platform fault enum. Existing
fault values remain unchanged. The fault applies only when creation is required; an existing
safe directory remains a successful no-change operation.

## Owner migrations

### `ui_menu_workspace`

The `menus` directory creation in authored-menu creation now calls the platform capability.
The existing domain mapping is preserved:

- managed directory failure returns `UI_MENU_WORKSPACE_SAVE_FAILED`;
- invalid menu names and path overflow retain `UI_MENU_WORKSPACE_INVALID_NAME`;
- successful directory creation prepares the same staged, dirty, path-owned document;
- failed creation does not reset the current document, catalog, mode, history, or text state.

`ui_menu_workspace_internal_create_document` provides a private per-call directory fault seam.

### `unified_editor`

The `sprites` directory preparation used by sprite save and sprite-canvas creation now calls
the platform capability. Existing callers still translate failure to
`EDITOR_STATUS_SAVE_FAILED`. The helper remains read-only with respect to editor state.

`unified_editor_internal_ensure_sprite_directory` in
`src/unified_editor_internal.h` provides a private per-call fault seam. The existing global
runtime-build test hook was not extended.

### Build linkage

No Make edit was required. Platform capability, UI workspace, unified-editor, and shipping
application targets each already link exactly one `platform_fs.c` through existing source
groups. Runner inventory remains 63.

## Test coverage

### Platform capability

`tests/test_platform_capabilities.c` now covers:

- creation of one multilingual UTF-8 directory;
- ordinary existing-directory success/no-change;
- POSIX mode application on initial creation;
- existing regular-file rejection;
- symbolic-link/reparse-point rejection;
- missing-parent failure without recursive creation;
- typed injected access failure with no directory created;
- invalid argument handling.

The native Windows fixture used a real directory symbolic link/reparse point; no
reparse-unavailable marker was emitted.

### UI workspace

`tests/test_ui_menu_workspace.c` verifies:

- injected managed-directory failure returns the existing save failure;
- the complete workspace remains byte-for-byte unchanged on failure;
- no directory is created by the injected failure;
- absent `menus` directory creation succeeds and stages the same authored menu;
- a `menus` regular-file collision fails without replacing the file or mutating workspace
  state.

### Unified editor

`tests/test_unified_editor.c` verifies:

- injected `sprites` directory failure leaves editor state unchanged;
- absent directory creation succeeds and returns the expected path;
- an existing ordinary directory succeeds without changing editor state;
- an existing regular file is rejected without changing editor state.

## Verification

| Check | Result |
|---|---|
| Strict GCC platform runner | `PASS`, 7/7 on Linux |
| Strict Clang platform runner | `PASS`, 7/7 on Linux |
| Platform ASan + leak detection | `PASS`, 7/7, no report |
| Platform UBSan | `PASS`, 7/7, no report |
| Strict GCC UI-menu-workspace runner | `PASS`, 14/14 |
| Strict Clang UI-menu-workspace runner | `PASS`, 14/14 |
| UI-menu-workspace ASan + leak detection | `PASS`, 14/14, no report |
| UI-menu-workspace UBSan | `PASS`, 14/14, no report |
| Strict GCC unified-editor runner | `PASS`, 98/98 |
| Strict Clang unified-editor runner | `PASS`, 98/98 |
| Unified-editor ASan + leak detection | `PASS`, 98/98, no report |
| Unified-editor UBSan | `PASS`, 98/98, no report |
| Strict GCC/default-SMC application build | `PASS`, explicit status 0 |
| Strict Clang/default-SMC application build | `PASS`, explicit status 0 |
| Complete runner build | `PASS`, explicit status 0 |
| Complete `make test` | `PASS`, explicit status 0 and 63 pass markers |
| `make standards-core` | `PASS`, explicit status 0 |
| `make test-platform-harness` | `PASS`, explicit status 0 |
| Owner direct `mkdir` scan | `PASS`, zero calls |

### Native Windows profile

Executed without an external timeout:

```sh
PROFILE_VM_NAME=win10-survey make platform-test-windows
```

Observed:

- isolated platform runner is native PE x86-64 without MSYS/Cygwin imports;
- native platform preflight increased from 9/9 to 10/10 and passed;
- `W2D_ENSURE_DIRECTORY=PROVEN` was emitted;
- multilingual creation, existing directory, file rejection, missing-parent non-recursion,
  injected failure, and reparse rejection passed;
- no reparse-unavailable marker was emitted;
- strict application compilation reports zero `mkdir` diagnostics in
  `ui_menu_workspace.c` and `unified_editor.c`;
- full product remains `FAIL-PRODUCT` only in `decal_document`, `map_catalog`,
  `scene_format`, and `sprite_document`;
- VM cleanup passed and final state is `shut off`;
- the tracked Python cache modified by execution was restored from `HEAD`.

## Preserved invariants

- Directory creation is one-level and non-recursive.
- Existing safe directories are not modified.
- Files and links/reparse points are never accepted as managed asset directories.
- UI workspace and editor domain-visible failure results are unchanged.
- Caller-owned workspace/editor state remains unchanged on directory failure.
- Sprite document save/publication semantics are unchanged.
- Catalog refresh/enumeration semantics are unchanged.
- No locale, decal, mirror, SMC, or benchmark behavior changed.
- Existing platform enum values and the 63-runner inventory remain stable.

## Limitations

1. UI workspace and unified-editor focused runners remain POSIX-shaped and were not executed
   natively. Native evidence is the 10/10 capability preflight plus diagnostic-free strict
   compilation of both owners.
2. Directory creation is intentionally one-level. Missing parent directories are errors and
   are not repaired.
3. Windows does not receive or emulate POSIX mode bits; `CreateDirectoryW` uses safe platform
   defaults according to the accepted D8 contract.
4. This increment does not claim that sprite directory-tree publication is a single-file
   atomic replacement. That requires the separate W2-D2 transaction.

## Next safe action

Proceed to **W2-D2 — sprite-folder publication transaction**. First inspect
`sprite_document` candidate/backup/publication/restoration behavior and result/state contracts.
Add focused file-sync, directory move, restoration, cleanup, and partial-commit evidence before
replacing its direct operations. Do not combine W3 catalog enumeration or W4 locale conversion.

Pinned-SMC remediation and P1 performance remain independent. Do not externally timeout
platform profiles.