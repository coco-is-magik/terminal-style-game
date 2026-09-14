# V1-0 W1 Windows Platform-Capability Decision — 2026-09-14

## Status and authority

**W1 complete; W2 file capability foundation is ready for focused implementation.**

This record resolves the platform-boundary decisions required by
[`V1_0_REQUIREMENTS_AND_EVIDENCE_PLAN_2026-09-14.md`](V1_0_REQUIREMENTS_AND_EVIDENCE_PLAN_2026-09-14.md)
before changing product code for native Windows. It is the Q1 authority for the
W2-W4 portability increments only. It does not claim that Windows compilation,
tests, display/input, or release support currently pass.

The decisions preserve the product philosophy, existing document ownership,
transactional failure contracts, strict warning policy, and native Windows evidence in
[`reviews/2026-09-14-v1-0-platform-profile-evidence.md`](reviews/2026-09-14-v1-0-platform-profile-evidence.md).

## Objective

Make Linux and Windows implementations satisfy the same domain-visible contracts without:

- scattering platform conditionals through documents and editor controllers;
- weakening atomic replacement, safe catalog discovery, or locale independence;
- introducing a second persistence owner;
- hiding committed-versus-uncommitted failure state;
- making later UTF-8/font work depend on host code pages;
- changing authored formats or runtime behavior merely to compile on Windows.

## Native evidence and exact scope

The Windows 10 UCRT64 strict build reached first-party code and identified four capability
families.

### 1. File durability and metadata

| Owner | Current operation | Existing domain result |
|---|---|---|
| `scene_document` | temporary file flush, mode preservation, file sync, replacement, parent-directory sync | detailed `SceneSaveResult` plus `SceneDiagnostic`; distinguishes committed durability warning |
| `decal_document` | temporary pattern file, file sync, replacement | `DECAL_DOCUMENT_IO_ERROR` |
| `material_document` | temporary file, file sync, replacement | `MATERIAL_DOCUMENT_IO_ERROR` |
| `object_document` | temporary file, file sync, replacement | `OBJECT_DOCUMENT_IO_ERROR` |
| `flow_document` | temporary file, file sync, replacement | `FLOW_DOCUMENT_IO_ERROR` |
| `ui_document` | temporary file, file sync, replacement | `UI_DOCUMENT_IO_ERROR` |
| `ui_preferences` | temporary file, file sync, replacement | `UI_PREFERENCES_IO_FAILED`; active session value remains selected |
| `sprite_document` | sync each frame and manifest, then replace a complete directory tree through temporary/backup directories | `SPRITE_DOCUMENT_IO_ERROR` |

Direct failing operations include `fsync` in seven non-sprite file/document owners, two
sprite frame/manifest sites, and native-scene parent-directory sync, plus `fchmod` for
existing native-scene mode preservation.

### 2. Safe direct-child catalog discovery

`map_catalog` uses `dirfd`, `fstatat`, and `AT_SYMLINK_NOFOLLOW` to accept only regular
lowercase-extension direct children while excluding directories and symbolic links. It
builds and sorts a complete candidate before replacing the live catalog.

### 3. Locale-independent scene numbers

`scene_format` uses:

- `newlocale`/`strtod_l`/`freelocale` for strict C-locale parsing;
- `newlocale`/`uselocale`/`freelocale` around canonical `%.17g` output;
- exact grammar validation, finite/range checks, negative-zero normalization, and
  nonmutation of process locale.

Windows UCRT exposes a different locale-object API. A direct global `setlocale` workaround
would violate thread safety and the accepted no-global-mutation regression.

### 4. Managed directory creation

`ui_menu_workspace` and `unified_editor` call POSIX `mkdir(path, mode)`. The former accepts
`EEXIST` without independently proving that the existing object is a safe directory; the
latter checks with `stat` first. Windows exposes a different signature and native path API.

### Adjacent future path requirement

V1-4 through V1-7 require UTF-8 authored data and custom-font paths. W1 therefore reserves
a strict UTF-8/native-path conversion seam now, but does not migrate current ASCII formats,
identifiers, or every filesystem call in W2-W4.

## Governing decisions

### D1 — Capability modules, not a persistence framework

Introduce narrow first-party platform capabilities:

```text
platform_fs       native file/directory operations and commit-state results
platform_catalog  direct-child enumeration metadata without following links
platform_number   locale-independent numeric parse/format operations
platform_path     strict UTF-8 <-> native path conversion at OS boundaries
```

Implementation may combine `platform_catalog` and `platform_path` privately with
`platform_fs` if that keeps headers narrower, but their public responsibilities and tests
remain separable.

These modules do not own:

- document serialization or validation;
- destination naming or project policy;
- document paths, dirty state, saved snapshots, or histories;
- temporary-file cleanup decisions after replacement failure;
- editor status messages;
- authored data or registry refresh;
- application-global last-error state.

Each document continues to own its workflow and maps a platform result into its existing
domain result and diagnostic contract.

### D2 — Platform isolation

- Use portable C in public headers.
- Place `_WIN32`/Win32/UCRT branches only in platform implementation files.
- Domain modules include platform headers but do not include `windows.h` or call `_w*`,
  Win32, or UCRT-specific APIs.
- POSIX implementation behavior remains the reference for existing Linux contracts.
- Windows uses wide native APIs after strict UTF-8 conversion; never use ACP/OEM/thread
  code pages for persisted or project paths.
- Adding these modules does not authorize a broad rewrite of unrelated asset-loader or
  runtime filesystem calls.

### D3 — Typed low-level result and commit state

Filesystem operations return a narrow typed result plus bounded native error context:

```text
PLATFORM_FS_OK
PLATFORM_FS_INVALID_ARGUMENT
PLATFORM_FS_NOT_FOUND
PLATFORM_FS_ALREADY_EXISTS
PLATFORM_FS_WRONG_TYPE
PLATFORM_FS_PATH_INVALID
PLATFORM_FS_ACCESS_DENIED
PLATFORM_FS_UNSUPPORTED
PLATFORM_FS_IO_ERROR
```

Where replacement can fail after the destination changed, the result also exposes commit
state explicitly:

```text
PLATFORM_COMMIT_NOT_COMMITTED
PLATFORM_COMMIT_COMMITTED
PLATFORM_COMMIT_COMMITTED_DURABILITY_WARNING
```

The platform layer returns a bounded native error number/category for diagnostics but does
not format or log untrusted operating-system strings. It has no mutable global last error.

Invalid arguments and all pre-commit failures leave caller outputs unchanged. A domain must
not report an ordinary save failure after `PLATFORM_COMMIT_COMMITTED*`; it marks its saved
identity/state according to the accepted workflow and reports a warning where its public
contract supports one.

### D4 — Single-file write sequence

Documents retain serialization. The shared capability sequence is:

1. create a collision-resistant same-directory temporary file with exclusive creation;
2. serialize through the document owner;
3. flush the language stream and check every operation;
4. sync the temporary file's bytes through `platform_fs_sync_file`;
5. close the stream/descriptor and check the result;
6. preserve destination metadata required by that document;
7. replace the destination through `platform_fs_replace`;
8. perform the strongest available parent-directory/replacement durability step;
9. return explicit commit state.

The platform module supplies primitives; it does not accept a serializer callback or own
the entire transaction in W2. This avoids merging document ownership and permits gradual
caller migration.

POSIX implementation uses the existing descriptor sync, mode, rename, and parent-directory
sync semantics.

Windows implementation uses strict UTF-8-to-UTF-16 conversion and native handles. The
initial candidates are `FlushFileBuffers` for file data and native same-volume replacement
operations with replacement/write-through behavior. The exact use of `ReplaceFileW` versus
`MoveFileExW` for existing and absent destinations is finalized only after a focused native
prototype proves:

- destination replacement behavior;
- same-volume requirements;
- metadata effects;
- sharing/access failure behavior;
- whether the destination changed on every failure path;
- the strongest honest durability claim available on supported Windows/filesystems.

No API is labeled atomic or durable merely from its name. The native prototype and fault
tests define the claim.

### D5 — Metadata policy

- Native scene replacement preserves the existing destination's applicable permission/
  read-only intent as its current contract requires. A newly created scene retains a safe
  user-owned default.
- Windows does not invent POSIX mode bits. It preserves applicable destination attributes
  through the selected native replacement path and explicitly tests read-only/access cases.
- Other documents retain their current metadata behavior during initial migration.
- A later cross-document durability-quality increment may improve them, but W2 does not
  silently change every public result enum or save policy.

### D6 — Pre-commit, replacement, and post-commit behavior

Preserve current native-scene semantics:

- temporary create/write/flush/file-sync/close/mode failure: destination, document path,
  name, saved state, and dirty state remain unchanged; temporary is removed;
- replacement failure: destination and document identity remain unchanged, and the complete
  temporary is retained when the current recovery contract promises it;
- post-replacement durability failure: Save is committed, path/name/saved state update, and
  a durability warning is returned.

For current asset/UI documents that expose only one I/O failure, migration preserves their
existing public mapping. Their platform call still reports commit state internally so they
cannot incorrectly leave a successfully replaced document dirty. If a newly reachable
post-commit warning cannot be represented safely, that owner receives a separate result/
diagnostic increment rather than treating committed data as uncommitted.

`ui_preferences` continues to keep the selected scale active when persistence fails.

### D7 — Sprite directory transactions stay separate

Do not pass sprite-folder Save through a single-file replacement API. It remains a document-
owned directory transaction:

- write and sync every candidate frame and manifest;
- close all files before publication;
- move the old target to an explicit backup when present;
- publish the candidate directory;
- restore the old target on failed publication where possible;
- remove backup only after successful publication;
- report cleanup/restore failure without claiming a clean rollback.

W2 may provide file sync and directory-operation primitives used by this workflow. Final
Windows directory publication semantics require their own focused W2-Sprite increment and
native tests. Do not assume single-file atomicity applies to directory trees.

### D8 — Managed directory creation

Provide `platform_fs_ensure_directory` with typed outcomes. It must:

- create exactly one requested directory, not recurse or create missing parents;
- treat an existing ordinary directory as success/no-change;
- reject an existing regular file;
- reject symbolic links and Windows reparse points for project-managed asset directories;
- use the supplied POSIX mode only when creating on POSIX;
- use safe Windows defaults without pretending to implement POSIX mode bits;
- preserve `errno`/native error long enough to return bounded context;
- never infer or repair a different path.

Migrate `ui_menu_workspace` and `unified_editor` separately. Their domain-visible failure
results remain unchanged unless a focused requirements update explicitly improves them.

### D9 — Direct-child catalog contract

Provide a platform enumeration adapter that yields borrowed entry name plus typed metadata
for one opened directory. The `MapCatalog` owner continues to filter extensions, allocate
owned names/paths, sort, and transactionally replace its live snapshot.

Required platform metadata distinguishes:

- ordinary regular file;
- directory;
- symbolic link/reparse point;
- unsupported/other object;
- per-entry metadata failure;
- enumeration failure.

Windows enumeration uses wide APIs and rejects every entry carrying
`FILE_ATTRIBUTE_REPARSE_POINT`, whether it is a symbolic link, junction, cloud placeholder,
or another reparse type. `FILE_ATTRIBUTE_DIRECTORY` distinguishes directories. Only direct
ordinary files are eligible. Names convert strictly to UTF-8; invalid conversion fails the
candidate refresh rather than substituting characters.

Do not use `stat` after string concatenation in a way that follows a reparse point between
enumeration and metadata classification. Native Windows tests must include a regular file,
directory, file symlink when privileges permit, directory junction/reparse point, malformed
or unconvertible path boundary, disappearing entry, and read/enumeration failure.

### D10 — Locale-independent numeric service

Move scene numeric conversion behind `platform_number` without changing scene grammar or
serialized bytes.

Required operations:

- parse one already grammar-validated ASCII decimal token into finite `double` under C
  numeric rules;
- append/format a finite `double` canonically with the current `%.17g` semantics;
- normalize negative zero to `0`;
- never mutate process-global or thread-global locale state visible to callers;
- use no shared mutable locale object without explicit lifetime and thread-safety proof;
- preserve output on failure.

POSIX may retain `newlocale`/`strtod_l` and a locale-scoped formatting implementation behind
the adapter. Windows uses `_create_locale(LC_NUMERIC, "C")`, `_strtod_l`, locale-specific
formatting functions, and `_free_locale`, subject to a native parity prototype.

Before replacing `scene_format`, run a fixed corpus on Linux and Windows covering:

- zero and negative zero;
- minimum/maximum accepted finite values;
- ordinary fractions and integers;
- positive/negative exponents near formatting thresholds;
- values requiring 17 significant digits;
- overflow, underflow, NaN/Inf text, comma decimal text, trailing data, and invalid grammar;
- exact canonical byte equality and binary round trip;
- process locale unchanged before/after success and failure.

If UCRT locale-specific `%.17g` differs from the accepted canonical bytes, stop W4 and
choose or implement one deterministic first-party formatter through a separate reviewed
decision. Do not normalize platform differences after serialization or weaken exact tests.

### D11 — UTF-8/native path boundary

`platform_path` defines paths passed to platform adapters as validated UTF-8, independent of
the current narrower authored identifier rules.

On Windows:

- convert with `MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ...)` or an equivalent
  strict native operation;
- use a sizing pass, checked allocation, then conversion;
- reject invalid UTF-8 and embedded NUL rather than replacement/substitution;
- call wide filesystem APIs only;
- convert enumerated names back to strict UTF-8 with checked sizing;
- never use `CP_ACP`, `CP_OEMCP`, or implicit narrow Win32 path APIs.

On POSIX, validate UTF-8 where the API promises a project path, then pass bytes unchanged.
W2-W4 may initially invoke this seam only for migrated operations. Full authored Unicode
path policy remains V1-4/V1-7 work; W1 does not broaden technical identifier or filename
syntax.

### D12 — Fault injection and diagnostics

Each platform capability has deterministic test substitution through a test-only header or
explicit operations object. Production public headers do not expose global mutable failure
flags.

Tests can force at least:

- path conversion/allocation failure;
- temporary creation, flush, file sync, close, metadata, replace, and post-commit durability
  failure;
- directory create/already-exists/wrong-type/reparse failure;
- catalog open, per-entry metadata, conversion, read, and close failure;
- locale creation, parse, and format failure.

The platform layer returns typed results. The highest domain/application boundary that knows
the consequence owns the `TSG-*` diagnostic and logs exactly once. Existing native-scene
diagnostic IDs and meanings remain stable unless an exact new detection case requires a new
catalog entry.

## Rejected approaches

1. **Scatter `_WIN32` around current callers.** Rejected because it duplicates semantics,
   makes failure parity unauditable, and obstructs UTF-8 work.
2. **Use macros mapping `fsync`, `mkdir`, or locale calls.** Rejected because signatures and
   guarantees differ; macros cannot expose commit state or native error meaning.
3. **Remove file sync or mode handling on Windows.** Rejected because compiling by weakening
   persistence violates the roadmap.
4. **Use process-global `setlocale`.** Rejected because it is externally visible, unsafe
   across concurrent work, and violates existing tests.
5. **Use Windows ANSI/OEM filesystem APIs.** Rejected because host code pages are
   nondeterministic and incompatible with the v1 UTF-8 contract.
6. **Treat all Windows reparse points as regular files after following them.** Rejected
   because it breaks direct-child identity and can escape the selected asset root.
7. **Build one generic document-save framework now.** Rejected because existing document
   ownership and post-commit semantics differ; capability reuse is sufficient.
8. **Treat a sprite folder as one atomic file.** Rejected because directory publication,
   backup, restoration, and partial cleanup have different failure states.
9. **Patch pinned SMC during profile image construction.** Rejected; dependency corrections
   require a reviewed revision and updated pin/hash.
10. **Reduce or externally override platform time bounds.** Rejected on this low-end host;
    repository-owned phase limits remain authoritative.

## Implementation increments and rollback points

### W2-A — Platform path and file primitive skeleton — complete 2026-09-14

- Add narrow headers/implementations and focused tests.
- Implement strict UTF-8/native conversion, file sync, replacement prototype, metadata,
  native error context, and test substitution.
- Do not migrate a document yet.

Implemented and locally verified in
[`reviews/2026-09-14-v1-0-w2a-platform-foundation.md`](reviews/2026-09-14-v1-0-w2a-platform-foundation.md).
Native Windows strict compilation includes both platform modules without diagnostics, but
the full profile stops in known unmigrated callers before the focused executable can run.
The W2-B native replacement-semantics prototype remains required before document migration.

**Rollback:** New isolated modules and runner only.

### W2-B — Native scene vertical slice

**W2-B1 prototype gate (2026-09-14): complete.** Native PE/import inspection and 9/9
Windows runtime tests prove the same-directory replacement boundary needed by scene save;
cross-volume is unavailable and outside that transaction. Existing-destination Windows
publication remains conservatively committed-with-durability-warning. See
[`reviews/2026-09-14-v1-0-w2b1-native-replacement-prototype.md`](reviews/2026-09-14-v1-0-w2b1-native-replacement-prototype.md).

**W2-B2 scene migration: next.**

- Migrate native scene file sync, mode/attribute preservation, replacement, and parent
  durability through the platform layer.
- Preserve every existing `SceneSaveResult`, diagnostic, temp-retention rule, identity, and
  dirty-state assertion.
- Add native Windows file/attribute/share/failure acceptance.

**Rollback:** Restore direct POSIX calls in one owner; platform primitives remain unused or
available for the next attempt.

### W2-C — Remaining single-file documents

Migrate one owner per increment in this order:

1. `ui_preferences` because its active-not-saved contract is small and explicit;
2. `material_document`;
3. `object_document`;
4. `flow_document`;
5. `ui_document`;
6. `decal_document`.

Each increment adds owner-specific pre/post-commit failure tests before migration. Do not
change all result enums in one pass.

### W2-D — Managed directories and sprite-folder transaction

- Migrate `ui_menu_workspace` and `unified_editor` to safe one-level directory creation.
- Prototype and implement Windows sprite candidate/backup/publication/restoration using
  file/directory primitives without pretending it is a single-file transaction.

**Rollback:** Directory creation and sprite publication are separate commits.

### W3 — Direct-child catalog adapter

- Add platform enumeration metadata and tests.
- Migrate `MapCatalog` while retaining candidate ownership, filtering, sorting, and exact
  live-catalog preservation on failure.
- Extend native Windows fixtures for reparse points and UTF-8 names.

**Rollback:** Existing POSIX `MapCatalog` implementation remains recoverable as one slice.

### W4 — C-locale numeric conversion

- Add corpus tests before changing `scene_format`.
- Implement POSIX and UCRT adapters.
- Prove exact parse/format parity and no locale mutation.
- Migrate parsing first, formatting second, then remove direct locale APIs from
  `scene_format`.

**Rollback:** Each parse/format migration is independent; authored grammar never changes.

### W5 — Native Windows functional profile

Run the existing profile without an external timeout through strict app, complete runner
build, `make test`, `standards-core`, and native binary inspection. Stop and classify the
first real failure; do not combine an unrelated correction into the same increment.

## Acceptance matrix

| Capability | Normal | Boundary | Failure | Preserved behavior | Native Windows |
|---|---|---|---|---|---|
| UTF-8 path | ASCII and multilingual path | maximum checked length, NUL | invalid UTF-8/allocation | POSIX bytes unchanged | strict wide API round trip |
| File sync | temporary bytes flushed/synced | empty and existing file | invalid handle/device/full/access | domain state unchanged before commit | `FlushFileBuffers` result mapped |
| Replace | absent/existing destination | same volume, attributes, sharing | before/after commit distinguished | atomic destination and temp policy | selected native primitive proven |
| Directory durability | replacement persisted where supported | filesystem capability | warning after commit | scene marked saved | strongest honest guarantee recorded |
| Ensure directory | create/existing directory | file/reparse/parent absent | access/path/native failure | no recursive repair | wide API, reparse rejected |
| Catalog | sorted regular children | empty, long/Unicode names | metadata/read/conversion/OOM | live catalog preserved | reparse/junction excluded |
| Number parse | valid C decimal | exponents/extremes/-0 | invalid/range/locale allocation | output and locale unchanged | `_strtod_l` parity |
| Number format | canonical round trip | 17-digit/exponent thresholds | allocation/format failure | exact serialized bytes | Linux/UCRT corpus identical |
| Sprite folder | publish candidate | existing target/backup | restore/cleanup/partial commit | staged ownership/dirty semantics | native directory transaction proven |

## Required verification

For each source-changing increment:

1. focused new platform-capability runner;
2. affected document/catalog/scene-format owner tests;
3. strict GCC and Clang application builds;
4. `make standards-core`;
5. complete `make test` through the staged build procedure needed on this host;
6. applicable ASan/UBSan/leak checks when ownership or allocation changes;
7. native Windows profile without an external timeout;
8. required Linux profiles where shared POSIX behavior changed;
9. exact VM lifecycle cleanup evidence.

`make benchmark-headless` remains independently blocked on P1 and is not waived by W1.
SMC source, mode selection, runner scope, and performance remain unchanged by W2-W4.

## W1 exit assessment

W1 is complete because:

- every current native compile blocker has an owning capability and migration increment;
- pre/post-commit semantics, metadata, catalog safety, locale behavior, and UTF-8 paths are
  explicit;
- document and sprite-folder ownership remain separate;
- normal, boundary, failure, rollback, Linux, and native Windows tests are defined;
- rejected shortcuts and stop conditions are recorded;
- no production behavior was changed before the decisions were resolved.

## Next safe action

Proceed to **W2-B2 native scene migration**. Preserve every scene save result, diagnostic,
temporary-retention, identity, and dirty-state contract, and keep existing-destination
Windows publication conservative as a committed durability warning. W2-B1 evidence is in
[`reviews/2026-09-14-v1-0-w2b1-native-replacement-prototype.md`](reviews/2026-09-14-v1-0-w2b1-native-replacement-prototype.md).

In parallel, the pinned-SMC C2 dependency update and P1 performance reproduction remain
independent. Do not externally timeout platform profiles.
