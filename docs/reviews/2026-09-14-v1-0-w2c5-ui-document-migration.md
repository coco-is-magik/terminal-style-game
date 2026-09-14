# V1-0 W2-C5 Authored UI Document Persistence Migration — 2026-09-14

## Outcome

**W2-C5 complete. Authored UI saves now use `platform_fs` for file sync, destination
inspection, and replacement. Pre-commit failures preserve destination bytes and all
caller-owned document/workspace state; committed durability warnings update saved identity and
remain truthful through UI workspace and editor feedback.**

This is the fifth owner-only W2-C increment selected by
[`../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md`](../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md).
Directory creation, decal, sprite-folder publication, catalogs, and locale conversion remain
unchanged.

## Contract extensions

`UI_DOCUMENT_OK_DURABILITY_WARNING` was appended to `UiDocumentResult`, preserving every
existing enum number. `ui_document_result_is_committed` identifies ordinary and warning
commits.

`UI_MENU_WORKSPACE_OK_DURABILITY_WARNING` was appended to `UiMenuWorkspaceResult`, also
preserving existing values. `ui_menu_workspace_result_is_committed` ensures warning commits
are not mapped to `UI_MENU_WORKSPACE_SAVE_FAILED`.

A warning save updates:

- the live workspace document;
- the saved workspace document;
- saved-state and path identity in every allocated history snapshot;
- dirty-state classification;
- Save-and-close behavior after successful catalog refresh.

The editor renders "Menu saved; durability warning" distinctly from "Menu save failed".

## Implementation

### `src/ui_document.c`

- Direct descriptor `fsync` is replaced by `platform_fs_sync_file`.
- `platform_fs_inspect_nofollow` determines whether the destination exists.
- Direct `rename` is replaced by `platform_fs_replace`.
- Not-committed failure removes the temporary candidate and returns the existing I/O error.
- Either committed state updates `document.path` and marks the current state saved.
- Committed durability warning returns the appended warning result.

Authored UI validation, V1/V2 migration, V3 parsing and serialization, flow-reference export,
same-directory temporary creation, stream write/flush/close, and transactional load remain
owned by the UI document module.

### Fault injection

Added `src/ui_document_internal.h` with explicit per-call sync, replacement, and durability
fault values. Added `src/ui_menu_workspace_internal.h` to route those faults through focused
workspace save and confirmation tests. No global mutable failure state was introduced.

### Workspace and editor handling

`ui_menu_workspace_save` commits its candidate for either committed document result. Warning
Save-and-close still refreshes the catalog and returns to the chooser, while preserving the
warning result when refresh succeeds. A catalog refresh failure retains the existing
`UI_MENU_WORKSPACE_CATALOG_FAILED` result rather than being mislabeled as a save failure.

Heap-owned history allocation, eviction, undo/redo ownership, directory creation, and catalog
implementation are unchanged.

### `Makefile`

No Make edit was required. W2-C4 already made every standalone UI runner that links
`ui_document.c` receive `platform_path` and `platform_fs`; combined project and editor runners
receive each platform implementation once through existing source groups. Forced dry runs
confirmed one `platform_fs.c` input for the UI document, UI workspace, and unified-editor
targets. Runner count remains 63.

## Test coverage

`tests/test_ui_document.c` now has fourteen tests covering:

- all existing authored-tree, validation, flow-reference, V1/V2 migration, V3 parsing,
  hierarchy, visual, and transactional-load behavior;
- byte equality between ordinary and warning saves, including V3 header and final LF;
- sync/replacement failure preserving stale destination bytes;
- complete document/path/dirty identity preservation before commit;
- warning commit updating path and clean state;
- committed-result classification.

`tests/test_ui_menu_workspace.c` now has thirteen tests covering:

- pre-commit failure preserving the complete workspace, including heap pointers and history;
- warning commit updating live/saved documents and allocated history snapshots;
- workspace committed-result classification;
- warning Save-and-close catalog refresh and chooser transition;
- existing chooser, creation, catalog, hierarchy, visual, pointer, preview, history, and
  undo/redo behavior.

## Verification

| Check | Result |
|---|---|
| Strict GCC UI-document runner | `PASS`, 14/14 |
| Strict Clang UI-document runner | `PASS`, 14/14 |
| Strict GCC UI-menu-workspace runner | `PASS`, 13/13 |
| Strict Clang UI-menu-workspace runner | `PASS`, 13/13 |
| UI-document ASan + leak detection | `PASS`, 14/14, no report |
| UI-document UBSan | `PASS`, 14/14, no report |
| UI-menu-workspace ASan + leak detection | `PASS`, 13/13, no report |
| UI-menu-workspace UBSan | `PASS`, 13/13, no report |
| Flow project catalog | `PASS`, 3/3 |
| UI layout/render/interaction/runtime | `PASS`, 3/3, 6/6, 6/6, 6/6 |
| Unified-editor runner | `PASS`, 97/97 |
| Strict GCC/default-SMC application build | `PASS`, explicit status 0 |
| Strict Clang/default-SMC application build | `PASS`, explicit status 0 |
| Complete runner build | `PASS`, explicit status 0 |
| Complete `make test` | `PASS`, explicit status 0 and 63 pass markers |
| `make standards-core` | `PASS`, explicit status 0 |
| `make test-platform-harness` | `PASS`, explicit status 0 |

The first manual sanitizer execution attempt used an incorrectly shell-expanded `$ORIGIN`
linker string. Both binaries compiled but could not locate `libcmocka.so.0`. Rerunning the
same binaries with the repository vendor library path passed; this was a command construction
failure, not a product or sanitizer finding.

### Native Windows profile

Executed without an external timeout:

```sh
PROFILE_VM_NAME=win10-survey make platform-test-windows
```

Observed:

- isolated native platform preflight remains PE x86-64, import-clean, and 9/9 passing;
- strict application compilation includes `ui_document.c` with zero diagnostics;
- its previous direct `fsync` error is absent;
- full product remains `FAIL-PRODUCT` in later unmigrated owners:
  `decal_document`, `map_catalog`, `scene_format`, `sprite_document`,
  `ui_menu_workspace` directory creation, and `unified_editor` directory creation;
- VM cleanup passed and final state is `shut off`;
- the tracked Python cache modified by native execution was restored from `HEAD`.

## Preserved invariants

- Authored UI validation and all previous result values are unchanged.
- Exact V3 bytes and V1/V2 load migration behavior are unchanged.
- Flow-reference export and validation are unchanged.
- Transactional load behavior is unchanged.
- Pre-commit failure preserves destination, path, document, workspace, history, and dirty state.
- Committed data is never reported as unsaved.
- Save-and-close still refreshes the catalog before returning to the chooser.
- Heap history allocation and ownership are unchanged.
- No directory, catalog, sprite, decal, locale, SMC, mirror, or benchmark behavior changed.

## Limitations

1. Focused fixtures remain POSIX-shaped (`mkstemp`, `mkdtemp`, `mkdir`, `unlink`, `rmdir`), so
   owner runners were not executed natively on Windows. Native evidence is diagnostic-free
   strict owner compilation plus the 9/9 platform preflight.
2. Temporary creation, stream opening, cleanup, and load retain current CRT narrow-path calls.
   Broad Unicode authored-UI path ownership is later v1 work.
3. `ui_menu_workspace` still contains two-argument `mkdir`; that native diagnostic belongs to
   W2-D and is not an authored-document persistence failure.
4. Existing-destination replacement on Windows remains conservatively a committed durability
   warning according to the accepted platform contract.
5. Cross-volume replacement remains unavailable in the single-volume VM and is not required
   by the same-directory authored UI transaction.

## Next safe action

Proceed to **W2-D1 — managed one-level directory creation** for `ui_menu_workspace` and
`unified_editor`. Preserve non-recursive ownership, existing-directory behavior, file/reparse
rejection, project paths, and transactional editor/workspace state. Do not combine sprite
folder publication, catalog enumeration, or locale conversion into that increment.

W2-D sprite-folder publication, W3 catalogs, W4 locale conversion, pinned-SMC remediation,
and P1 performance remain independent. Do not externally timeout platform profiles.