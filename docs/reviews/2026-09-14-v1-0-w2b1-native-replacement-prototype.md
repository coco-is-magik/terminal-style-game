# V1-0 W2-B1 Native Windows Replacement Prototype — 2026-09-14

## Outcome

**W2-B1 complete. The isolated native Windows platform runner passes 9/9 as a native PE
x86-64 executable. W2-B2 may migrate native scene save through the proven same-directory
capabilities while retaining conservative existing-destination durability warnings.**

This increment executes the prototype required by
[`../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md`](../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md)
and the W2-A handoff in
[`2026-09-14-v1-0-w2a-platform-foundation.md`](2026-09-14-v1-0-w2a-platform-foundation.md).
It does not migrate `scene_document` or any other product owner.

## Harness implementation

`tools/platform-profiles/windows_product.py` now performs a mandatory
`platform-capability-preflight` after dependency/inventory validation and before the strict
application build. The phase:

1. builds only `build/test-platform-capabilities` with the unchanged strict Make flags;
2. inspects `build/test-platform-capabilities.exe` as PE x86-64;
3. rejects MSYS2/Cygwin runtime imports;
4. runs the executable under a repository-owned 120-second bound;
5. retrieves `platform-capability-preflight.log` even when the phase fails;
6. classifies ordinary compile/test failure as `FAIL-PRODUCT`, compiler crash as
   `FAIL-TOOL`, timeout as `FAIL-TIMEOUT`, and missing log as `FAIL-TOOL`.

If preflight passes, all prior full-product phases continue in their original order. The
profile still stops at the first later product failure.

Harness fixtures were updated in:

- `tools/platform-profiles/test-windows-product.py`;
- `tools/platform-profiles/test-windows-guest.py`.

They protect phase order, unchanged Make policy, native-binary checks, focused failure
classification, log retrieval, and stopping before the application after a focused failure.

## Native test extension

`tests/test_platform_capabilities.c` retains six portable tests and adds three `_WIN32`
tests. Windows-only fixture operations use wide APIs. Native assertions cover:

- strict UTF-8/native round trip and malformed input rejection;
- file sync and typed native failures;
- absent-destination publication;
- existing-destination publication through multilingual paths;
- applicable hidden-attribute preservation;
- read-only destination rejection;
- open-handle sharing rejection;
- unchanged destination and retained temporary after pre-commit failures;
- symbolic-link/reparse-point no-follow classification;
- explicit cross-volume availability reporting.

Environment-dependent behavior is logged as `PROVEN` or `UNAVAILABLE`; unavailable evidence
is not converted into a pass claim.

## Final native evidence

Executed without an external timeout:

```sh
PROFILE_VM_NAME=win10-survey make platform-test-windows
```

The isolated log records:

```text
build/test-platform-capabilities.exe: file format pei-x86-64
native_imports=kernel32.dll ... cmocka.dll
W2B_ABSENT_REPLACE=PROVEN commit=committed
W2B_EXISTING_REPLACE=PROVEN same_volume=yes before=0x00000022 after=0x00000022 commit=durability-warning
W2B_READONLY_FAILURE=PROVEN native_error=5 commit=not-committed
W2B_SHARING_FAILURE=PROVEN native_error=32 commit=not-committed
W2B_REPARSE_CLASSIFICATION=PROVEN
W2B_CROSS_VOLUME=UNAVAILABLE current_volume=C
[  PASSED  ] 9 test(s).
```

The imports contain UCRT API-set DLLs and `cmocka.dll`; neither `msys-2.0.dll` nor
`cygwin1.dll` is present.

The complete profile then advances to the expected `strict-app-build` and remains
`FAIL-PRODUCT` in known unmigrated `fsync`/`fchmod`, catalog, locale, and two-argument
`mkdir` callers. This downstream failure does not invalidate the passing isolated phase.

Lifecycle evidence:

- initial VM state: `shut off`;
- started by harness: `true`;
- QGA shutdown: successful;
- cleanup outcome: `PASS`;
- final VM state: `shut off`.

Evidence is retained under `build/platform-profiles/windows-10-x64-gcc/`, especially:

- `platform-capability-preflight.log`;
- `strict-app-build.log`;
- `result.env`;
- `vm-lifecycle.log`.

## Guarantee assessment

### Proven for W2-B2

- The Windows implementation accepts strict UTF-8 paths and calls wide filesystem APIs.
- A same-directory absent destination is published and reported committed.
- An existing destination is replaced with the candidate bytes.
- Hidden attributes observed in this fixture remain present after replacement.
- A read-only destination fails with `ERROR_ACCESS_DENIED` before commit; destination bytes
  remain old and candidate temporary bytes remain recoverable.
- A destination held without delete sharing fails with `ERROR_SHARING_VIOLATION` before
  commit; destination and temporary remain unchanged.
- a created Windows symbolic link is classified as a reparse point and not a regular file.
- The focused executable is native PE x86-64 without MSYS/Cygwin runtime imports.

### Conservative limits

- Existing-destination replacement remains
  `PLATFORM_COMMIT_COMMITTED_DURABILITY_WARNING`. The evidence proves publication and
  observed metadata behavior, not crash-consistent parent-directory durability across every
  Windows filesystem/storage stack.
- The VM exposes only writable `C:`. Cross-volume publication is therefore
  `UNAVAILABLE`, not passed. Native scene save creates its temporary in the destination
  directory, so cross-volume replacement is outside that accepted transaction by
  construction.
- Hidden-attribute preservation is proven. Other Windows security descriptors, alternate
  streams, encryption, compression, and filesystem-specific metadata are not claimed.
- Reparse classification is proven for a symbolic link. Junction/cloud/other reparse types
  remain W3 catalog acceptance cases; the implementation rejects the common reparse
  attribute independent of subtype.

## Verification

| Check | Result |
|---|---|
| Strict GCC portable focused runner | `PASS`, 6/6 |
| Strict Clang portable focused runner | `PASS`, 6/6 |
| Focused ASan + leak detection | `PASS`, 6/6, no report |
| Focused UBSan | `PASS`, 6/6, no report |
| Windows native focused runner | `PASS`, 9/9 |
| Windows PE/import inspection | `PASS` |
| Strict GCC/default-SMC application build | `PASS` locally |
| Strict Clang/default-SMC application build | `PASS` locally |
| Complete `make test` | `PASS`, explicit status 0, 63 pass markers |
| `make standards-core` | `PASS` |
| `make test-platform-harness` | `PASS` |
| Native Windows full product | expected `FAIL-PRODUCT` at known unmigrated strict app callers |
| Windows lifecycle cleanup | `PASS`, final `shut off` |

## Failures and corrections

1. The first native preflight built cleanly but its metadata fault assertion expected generic
   `IO_ERROR`; Windows correctly returned the more precise `ACCESS_DENIED`. The test now
   checks the platform-specific typed result and closes resources before assertions that can
   abort, preventing cascading setup failures.
2. The first harness implementation accidentally placed the original later phases after an
   unconditional preflight failure raise. Ten product-fixture assertions exposed the
   unreachable phases. Original ordering was restored; the fixture passes 11/11.
3. The first PE inspection used Make's suffixless target path. MinGW materializes `.exe`,
   and direct `objdump` does not infer it. Inspection and execution now use the concrete
   `.exe` path.
4. Cross-volume evidence could not be produced because the VM exposes only writable `C:`.
   This remains explicitly unavailable rather than simulated.

## Preserved invariants

- No document, catalog, numeric parser/formatter, editor, asset format, or runtime behavior
  changed.
- No document owner calls the platform adapter yet.
- Same-directory temp ownership and committed/uncommitted distinctions remain explicit.
- No global fault or last-error state was introduced.
- SMC, mirror, benchmark budgets, and checked-in assets were not changed.
- Platform commands retained repository-owned internal bounds; no external timeout was used.

## Next safe action

Proceed to **W2-B2 — native scene vertical slice**:

1. write/retain owner-level failure tests for every native scene save stage;
2. migrate only native scene metadata, file sync, and replacement operations through
   `platform_fs`;
3. preserve every existing `SceneSaveResult`, `SceneDiagnostic`, temporary-retention,
   identity, and dirty-state contract;
4. map existing-destination Windows success conservatively to the committed durability
   warning until stronger evidence exists;
5. rerun focused scene/platform tests, full local gates, and the native Windows preflight/
   profile without an external timeout.

Do not combine asset saves, directory creation, catalog enumeration, or locale conversion
with W2-B2. Pinned-SMC remediation and P1 performance remain independent.
