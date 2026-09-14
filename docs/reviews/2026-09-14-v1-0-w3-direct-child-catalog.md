# V1-0 W3 Direct-Child Catalog Adapter — 2026-09-14

## Outcome

**W3 is implemented and verified at its focused Linux and native Windows boundaries. A
new `platform_catalog` adapter enumerates direct children with strict UTF-8 names and typed
no-follow metadata. `MapCatalog` retains extension filtering, owned names/paths,
deterministic sorting, complete-candidate cleanup, and transactional live replacement.**

This implements D9 from
[`../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md`](../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md)
under the active
[`../V1_0_REQUIREMENTS_AND_EVIDENCE_PLAN_2026-09-14.md`](../V1_0_REQUIREMENTS_AND_EVIDENCE_PLAN_2026-09-14.md).
It does not change asset formats, identifier syntax, catalog extension rules, locale
conversion, SMC, mirrors, or performance thresholds.

## Platform catalog contract

`platform_catalog_enumerate`:

- opens exactly one borrowed root and never recurses;
- validates the root as UTF-8 on every platform;
- yields one borrowed entry name plus typed regular-file, directory,
  link/reparse-point, or other classification;
- supports continue, successful early stop, and callback rejection;
- distinguishes invalid arguments, open failure, read/close failure, per-entry metadata
  failure, invalid path/name conversion, allocation failure, and callback rejection;
- follows no symbolic link or Windows reparse point to classify an entry; and
- owns no unbounded entry array or persistent catalog snapshot.

The POSIX implementation uses `opendir`, `readdir`, `dirfd`, and
`fstatat(..., AT_SYMLINK_NOFOLLOW)` inside the platform boundary. The Windows implementation
uses strict UTF-8/native path conversion plus `FindFirstFileW`/`FindNextFileW`; every
`FILE_ATTRIBUTE_REPARSE_POINT` entry is classified as link/reparse regardless of subtype.
An existing empty Windows directory is a successful empty enumeration even though the
native wildcard search reports no first file.

`src/platform_catalog_internal.h` provides per-call open, read, metadata, and name-
conversion faults. No global mutable fault state was introduced.

## MapCatalog migration

`MapCatalog` still owns:

- exact case-sensitive extension filtering;
- dynamic owned display names and full paths;
- bytewise deterministic `strcmp` sorting;
- complete candidate cleanup on every failure; and
- replacement of the live snapshot only after complete successful enumeration and sort.

`MAP_CATALOG_PATH_INVALID` was appended without renumbering previous result values. Invalid
UTF-8 roots or entry names now receive a truthful result rather than being mislabeled as
path length or interpreted through a host code page.

Direct POSIX directory APIs were removed from `map_catalog.c`. Its private test seam accepts
one explicit platform fault and preserves the public production APIs.

## Build and native harness integration

`SRC_PLATFORM_CATALOG` was added to the Makefile and included exactly once through
`SRC_MAP_CATALOG` for application and combined consumers. The focused platform-capability
runner links it directly; the focused map-catalog runner links it with `platform_path`.
Runner inventory remains 63.

The existing Windows platform preflight now builds, PE/import-checks, and runs both
`test-platform-capabilities.exe` and `test-map-catalog.exe` before the full application.
The harness retains one bounded phase, unchanged strict Make flags, typed failure
classification, log retrieval, and VM lifecycle ownership.

## Regression coverage

### Platform capabilities

The local platform suite now runs 10 tests and additionally covers:

- regular file, directory, and link/reparse classification;
- multilingual UTF-8 filename round trip through enumeration;
- successful early stop;
- callback rejection;
- invalid root and callback arguments;
- invalid UTF-8 root rejection; and
- injected open, read, metadata, and name-conversion failures.

The native Windows suite runs 13 tests and additionally proves multilingual enumeration and
a real file symbolic-link/reparse classification.

### MapCatalog

The local suite now runs eight tests and covers:

- lowercase `.txt` and `.tscene` filtering with uppercase exclusion;
- regular direct children only;
- deterministic sorting including a multilingual filename;
- exact owned names and paths;
- successful empty catalogs;
- a 100-byte valid filename without truncation;
- successful snapshot replacement;
- preservation of the prior snapshot after missing-root, open, read, metadata, conversion,
  and real invalid-POSIX-filename failures; and
- invalid argument behavior.

The native Windows suite runs seven applicable tests; the POSIX-only malformed-byte filename
test is intentionally absent because ordinary wide Windows enumeration cannot return an
ill-formed UTF-16 filename.

Existing flow-project-catalog, authored menu workspace, and unified-editor catalog behavior
remains covered by their focused suites.

## Verification evidence

| Check | Result |
|---|---|
| Pre-change MapCatalog | `PASS`, 5/5 |
| Pre-change platform capabilities | `PASS`, 8/8 |
| Final strict GCC MapCatalog | `PASS`, 8/8 |
| Final strict Clang MapCatalog | `PASS`, 8/8 |
| Final strict GCC platform capabilities | `PASS`, 10/10 |
| Final strict Clang platform capabilities | `PASS`, 10/10 |
| MapCatalog ASan + LeakSanitizer | `PASS`, 8/8, no report |
| MapCatalog UBSan | `PASS`, 8/8, no report |
| Platform ASan + LeakSanitizer | `PASS`, 10/10, no report |
| Platform UBSan | `PASS`, 10/10, no report |
| Flow project catalog | `PASS`, 3/3 |
| Authored menu workspace | `PASS`, 14/14 |
| Unified editor | `PASS`, 99/99 |
| Strict default GCC/SMC-stream application | `PASS` |
| Complete runner build | `PASS`, 63 runners |
| Complete `make test` | `PASS`, all 63 registered runners |
| UI standards aggregate | `PASS`, all nine owners |
| `make standards-core` | `PASS` |
| `make test-platform-harness` | `PASS` |
| Python harness syntax compilation | `PASS` |
| `make smoke` | `PASS`, `{"smoke":"ok","map_width":10,"map_height":6}` |

Strict C checks used `-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror`. Sanitizer builds
used the maintained `-O1 -g` ASan/LeakSanitizer or UBSan configuration.

## Native Windows evidence

The final native profile ran without an external timeout:

```text
PROFILE_VM_NAME=win10-survey make platform-test-windows
```

Observed:

- platform capability preflight passed 13/13;
- native `MapCatalog` passed 7/7;
- both focused executables are PE x86-64 and import neither MSYS nor Cygwin runtimes;
- `W3_CATALOG_ENUMERATION=PROVEN` and `W3_CATALOG_REPARSE=PROVEN` were emitted;
- multilingual names, empty directories, long names, case-sensitive filtering, sorting,
  fault preservation, and reparse exclusion passed;
- strict application compilation has zero `map_catalog.c` and `platform_catalog.c`
  diagnostics;
- the application now stops only in `scene_format.c` on W4's POSIX locale APIs; and
- VM cleanup passed through QGA, restoring `win10-survey` from `shut off` to `shut off`.

This completes W3 focused native evidence. It is not W5 full Windows acceptance because W4
correctly prevents later full-product phases from running.

## Failures and corrections

### Windows-only enum type mismatch

Attempt: first expanded native W3 preflight.

Result: strict native compilation failed before execution because a
`PlatformPathResult` from name conversion was stored in a `PlatformCatalogResult` variable.

Failure mode: GCC reported `-Wenum-conversion` and `-Wenum-compare`; Linux could not expose
the Windows-only branch.

Correction: use a correctly typed `PlatformPathResult` local and map it explicitly. Local
strict checks and the next native run passed compilation.

### Deep guest path exceeded the effective Windows path limit

Attempt: add a 200-byte filename fixture, below the ordinary per-component limit.

Result: local Linux passed, but native fixture creation returned `-1` because the deep guest
workspace plus component exceeded the environment's current effective path limit.

Failure mode: the test assumed extended-length Windows path policy that W3 does not adopt.

Correction: use a 100-byte filename, still longer than current authored identifiers and
sufficient to prove no fixed-buffer truncation. Strict Linux, ASan, native Windows, and the
aggregate passed. Extended-length Windows path policy remains later explicit path work.

### Verification concurrency was not accepted as execution evidence

Some strict GCC and Clang builds initially targeted the same output paths concurrently.
Both compilers returned success, but their resulting binaries were not used as
compiler-specific execution evidence. Final focused binaries were rebuilt and executed
sequentially for each compiler.

## Preserved invariants

- `MapCatalog` remains the sole owner of catalog snapshots.
- Enumeration remains direct-child and non-recursive.
- Symbolic links and all Windows reparse entries are excluded from eligible regular files.
- Extension matching remains exact and case-sensitive.
- Sorting remains deterministic bytewise `strcmp` order.
- Failed refresh preserves the complete prior live catalog.
- No filename is substituted, normalized, or interpreted through a host code page.
- Existing result numbers remain stable.
- Existing application/editor, flow, and menu catalog callers retain their public results
  and state behavior.
- No scene, asset, glyph, font, UI, mirror, SMC, or benchmark semantics changed.

## Limitations and remaining risks

1. Windows extended-length path policy is not introduced; the supported W3 fixture proves a
   100-byte component within the current guest workspace.
2. A disappearing real entry is represented by deterministic metadata-failure injection;
   creating that race as a stable cross-platform test would be nondeterministic.
3. Windows reparse proof uses a file symbolic link. The adapter rejects every reparse bit,
   including junctions and cloud placeholders, but those additional concrete subtypes were
   not separately created.
4. Full native Windows application/test acceptance remains blocked on W4 locale conversion.
5. C1/C2 newline, P1 performance, display/input, and final V1-0 closeout work remain open.

## Next safe action

Proceed to **W4 — locale-independent numeric conversion**. Add fixed parse/format corpus
tests first, implement `platform_number` POSIX/UCRT adapters, prove exact C-locale behavior
and no process-locale mutation, then migrate `scene_format` parsing and formatting in
separate bounded steps.

If UCRT canonical `%.17g` bytes differ from the accepted Linux corpus, stop and make a
focused formatter decision. Do not normalize output after serialization, mutate global
locale, or combine W4 with pinned-SMC or P1 performance work.
