# Current Status

## Current verified foundation

R12 is Verified and closed as of 2026-09-11. R0–R12 are verified roadmap
foundations, not a claim that the editor or game-making experience is
feature-complete.

The latest closeout evidence is in
[`reviews/2026-09-11-roadmap-r12-closeout.md`](reviews/2026-09-11-roadmap-r12-closeout.md).
R12 implemented the authored game-flow and responsive UI/menu-authoring
foundation through I1–I17, including the post-review correction rounds. Final
manual hierarchy and Flow display-backed checks passed on 2026-09-11. Test-mode
pointer activation remains deferred; keyboard Enter is the accepted R12 Test-mode
activation path.

## Next safe action

The dependency-aware roadmap from the verified R0-R12 foundation through v1.0 was
accepted on 2026-09-14. **V1-0 — Baseline, required-platform remediation, and
evidence reconciliation** is Verified and closed as of 2026-09-16. B0/C1, W1-W5,
native Linux display/input acceptance, P1-P3 performance work, byte inventory, and
mirror disposition are complete.
Native Windows verification is deliberately limited to two product checks under one command:
strict `make all` compilation and complete `make test` execution. On 2026-09-15 the single
`PROFILE_VM_NAME=win10-survey make platform-test-windows` invocation passed strict application
compilation and all 64 test runners. The runner starts the VM only
when it is off and never stops it. Windows VM benchmarks, stability workloads, standards,
sanitizers, binary inspection, smoke, and SPICE/display automation are outside this scope. The
previous expanded Windows procedure and its acceptance-only F6/F7 controls were removed. Its VM
performance measurements are non-qualifying and are not Windows product failures. The VM was
already running and remained running. Pinned-SMC newline maintenance is deferred to V1-20.
The display procedure is defined in
[`DISPLAY_INPUT_ACCEPTANCE.md`](DISPLAY_INPUT_ACCEPTANCE.md).
Its focused execution authority is
[`V1_0_REQUIREMENTS_AND_EVIDENCE_PLAN_2026-09-14.md`](V1_0_REQUIREMENTS_AND_EVIDENCE_PLAN_2026-09-14.md).
Production portability changes are underway through isolated platform capabilities and
owner-by-owner persistence migrations.

The distinct planar mirror projection defect was reproduced, corrected, manually
confirmed, and locked by analytic final-frame and continuation-path regressions on
2026-09-16. Native Linux X11 display/input acceptance also passed presentation, resize,
keyboard, pointer motion/down/up, state transition, and bounded teardown. The closeout
record is
[`reviews/2026-09-16-v1-0-closeout.md`](reviews/2026-09-16-v1-0-closeout.md).

The current design authority is **V1-1 Q1 requirements/design** under
[`V1_1_UI_RULES_AND_TOKENS_Q1_PLAN_2026-09-16.md`](V1_1_UI_RULES_AND_TOKENS_Q1_PLAN_2026-09-16.md).
The current UI literal/consumer inventory is complete in
[`reviews/2026-09-16-v1-1-ui-literal-consumer-inventory.md`](reviews/2026-09-16-v1-1-ui-literal-consumer-inventory.md).
Concrete D1-D7 recommendations with WCAG 2.2 sources are recorded in
[`reviews/2026-09-16-v1-1-d1-d7-candidate-decision.md`](reviews/2026-09-16-v1-1-d1-d7-candidate-decision.md).
The current static palette values and D6 motion vocabulary are manually approved. The pure
`ui_theme`/`ui_motion` seams, bounded application-menu adapter, static palette specimen,
isolated motion specimen, and first pause-context adapter now pass all fifteen UI standards
owners, the complete functional aggregate, real cppcheck/policy standards, focused
sanitizers, and strict default/no-state-tracker application builds. See
[`reviews/2026-09-16-v1-1-provisional-ui-theme-policy.md`](reviews/2026-09-16-v1-1-provisional-ui-theme-policy.md).
Q2 selected only the application menu focus/selection palette as the first reversible
consumer; it is now implemented as recorded in
[`reviews/2026-09-16-v1-1-application-menu-palette-adapter.md`](reviews/2026-09-16-v1-1-application-menu-palette-adapter.md).
The architecture decision remains in
[`reviews/2026-09-16-v1-1-q2-policy-architecture-review.md`](reviews/2026-09-16-v1-1-q2-policy-architecture-review.md).
The static and animated specimens were approved on 2026-09-16. V1-3 I1 now implements pause
as the first bounded major context: immediate menu-stack behavior, an unchanged interactive
menu, a separate decorative layer, explicit deterministic time, and a session-only
reduced-motion toggle. All fifteen UI standards owners, the complete functional aggregate,
standards, focused sanitizers, and strict tracker/no-tracker builds pass. Native visual review
is pending. No second context, pointer redesign, or motion persistence is authorized. See
[`V1_3_PAUSE_CONTEXT_MOTION_I1_PLAN_2026-09-16.md`](V1_3_PAUSE_CONTEXT_MOTION_I1_PLAN_2026-09-16.md).
Sprite-painter behavior
characterization may proceed only under its own focused plan; final glyph-aware formats
still depend on the Unicode/glyph foundation.

The initial bounded asset scan found no non-ASCII runtime-authored asset file.
`assets/README.md` contains non-ASCII documentation bytes and is classified separately;
no legacy code-page meaning has been assigned.

The 2026-09-14 B0 local baseline passes the strict default GCC/SMC-stream application
build, both focused mirror owners (4/4 and 19/19), the complete 62-runner build and
functional aggregate, all nine UI standards owners, `standards-core`, and smoke. A local
Clang 22 strict application build also passes, but does not replace Ubuntu Clang 18
informational diagnostic evidence. The first full `make test` attempt timed out during compilation; the
recorded narrower `test-build` then `test` recovery passed.

P1 reproduced the deterministic surface-render failure on the physical host without changing
code, flags, methodology, or the 6 ms budget. Five benchmark trials all failed with flat-path
median 7.526312 ms; three 1,000-iteration stability trials all failed with median 7.693334 ms.
All checksums were identical and deterministic. The affected stage was the current opaque
prepared-heightfield renderer, not optical composition. P1 reproduction and the subsequent
P2 optimization are complete. The B0 scan found 192
of 254 scoped production/test/pinned-SMC C files lacked final newlines, matching the
preserved Ubuntu Clang 18 failure class even though local Clang 22 accepted them.

C1 first-party remediation is complete. Exactly 188 tracked
first-party/test files are their original bytes plus one final LF; strict GCC and local
Clang builds, focused SMC owners, `standards-core`, the complete runner build, and all 62
functional runners pass. Four affected SMC files come from the image-owned pinned SMC
archive. Their trivial correction is deferred to owned-upstream dependency reconciliation
in V1-20. Ubuntu Clang is now informational but remains strict: `-Werror` is unchanged,
and a diagnostic-only `make -k` sweep with `-ferror-limit=0` compiles application and test
targets so the known four errors cannot mask additional findings. See
[`reviews/2026-09-14-v1-0-c1-final-newline-remediation.md`](reviews/2026-09-14-v1-0-c1-final-newline-remediation.md).

That statement describes the C1 checkpoint. After later W2 work arrived, a fresh local
Clang application build found six newly added private first-party headers without final
newlines (`flow_document_internal.h`, `flow_workspace_internal.h`,
`sprite_document_internal.h`, `ui_document_internal.h`, `ui_menu_workspace_internal.h`,
and `unified_editor_internal.h`) in addition to the four pinned SMC files. W2-C6's new
private header was corrected immediately and the decal owner passes strict Clang 7/7.
Any additional Clang finding is immediate work; only the exact four known SMC newline
errors are deferred.

P2 closes the opaque prepared-heightfield performance blocker. `HeightfieldTraceColumn`
now records contiguous uniform-height runs during its existing bounded DDA preparation.
The opaque renderer binary-searches horizontal ownership in each run and checks only the
run-edge boundary through the generic sampler's shared interval evaluator. Exact equivalence
fixtures cover flat and raised planes, material changes, missing surfaces, walls, camera
height, pitch, and invalid input. Final isolated timing evidence under the unchanged 6 ms
budget passed five benchmark trials (flat 2.403860–4.817409 ms; raised
3.896334–4.656326 ms) and three 1,000-iteration stability trials (flat
2.440316–3.378517 ms; raised 3.920074–4.968514 ms). All five canonical checksums remained
exact and every trial was deterministic. Loaded/concurrent attempts that exceeded the budget
are preserved as non-qualifying evidence rather than erased. At the P2 checkpoint, the broader
headless aggregates passed the surface owner but remained nonzero in the separate optical
transparent/mirror workload at about 10.3–10.4 ms; opaque optical parity was below budget.

P3 closes that optical aggregate finding. The composition path now uses the P2-equivalent
prepared nearest sampler, immediately emits legacy-equivalent samples with no render-affecting
material/cell override, and avoids clearing unused reflected-column storage. Sparse cell
overrides still use the validated runtime lookup and have a complete-frame rendering fixture.
The accepted unchanged sequence passed three benchmark trials (transparent
3.603454–3.818739 ms; mirror 3.586869–3.668715 ms) and three 1,000-frame stability trials
(transparent 3.357268–4.061780 ms; mirror 3.424079–4.000891 ms). Exact opaque,
transparent, and mirror checksums, one-bounce/cache behavior, and zero timed-loop allocations
remain unchanged. Earlier isolated failures moved among opaque, transparent, and mirror
scenarios under the loaded four-core host and are preserved; the accepted three-pass retry
matches the established host-variance policy. Final `benchmark-headless` and
`stability-headless` aggregates both pass.

The platform harness passes, including daemon-failure classification and all Windows
lifecycle/guest/dependency/product fixtures. A native Windows run completed without an
external timeout, reproduced the W1 `fsync`/`fchmod`, safe catalog, POSIX locale, and
two-argument `mkdir` compile blockers, and restored the `win10-survey` VM from `shut off`
back to `shut off` through QGA. That evidence supplied the native inputs for the completed
W1 capability decision. See
[`reviews/2026-09-14-v1-0-platform-profile-evidence.md`](reviews/2026-09-14-v1-0-platform-profile-evidence.md).

W1 is complete as a design gate. W2-A now implements isolated `platform_fs` and
`platform_path` foundations plus a 63rd focused runner without migrating any document.
Strict GCC/Clang focused tests pass 6/6; focused ASan/LeakSanitizer and UBSan pass; strict
GCC and local Clang applications build; all 63 functional runners, `standards-core`, and the
platform harness pass. Native Windows accepts the new inventory and compiles both platform
modules without diagnostics, then remains `FAIL-PRODUCT` in the known unmigrated callers;
cleanup passed and the VM is `shut off`. W2-B1 now adds a mandatory isolated native
preflight: the focused executable is PE x86-64 without MSYS/Cygwin imports and passes 9/9.
It proves Unicode same-directory absent/existing replacement, hidden-attribute preservation,
read-only/sharing failure before commit with destination/temp preservation, and reparse
classification. Cross-volume is unavailable on the single-volume VM and outside native
scene's same-directory transaction. W2-B2 now routes native scene metadata, file sync,
replacement, and commit state through the adapter. Scene owner tests pass 48/48 under strict
GCC/Clang, ASan/LeakSanitizer, and UBSan; all accepted recovery/identity/dirty-state contracts
remain intact. Native strict compilation reports zero `scene_document.c` diagnostics, while
end-to-end native owner execution awaits W4 locale remediation. W2-C1 now routes UI
preference sync/replacement through the adapter. Pre-commit failures remain active-not-saved
and preserve destination bytes; committed durability warning is a distinct saved result with
accurate feedback. Focused strict/sanitizer tests pass 6/6 and native strict compilation has
zero `ui_preferences.c` diagnostics. W2-C2 now routes material sync/replacement through the
adapter. Pre-commit failure preserves destination/snapshot/path/dirty state; warning commit
updates path/snapshot/clean state, and asset refresh accepts both committed outcomes. Focused
strict/sanitizer tests pass 8/8, asset refresh passes 7/7, and native strict compilation has
zero `material_document.c` diagnostics. W2-C3 now routes object atomic creation through the
adapter. Uncommitted failure keeps `out_id` zero and preserves destination bytes; warning
commit publishes the lowest-free ID, while registry/generation remain untouched. Focused
strict/sanitizer tests pass 4/4 and native strict compilation has zero
`object_document.c` diagnostics. W2-C4 now routes flow-document sync, destination inspection,
and replacement through the adapter. Pre-commit failures preserve destination bytes and the
complete document; warning commits update path and clean-state identity. Flow workspace and
editor handling preserve committed state while reporting the warning truthfully. Strict
GCC/Clang flow-document tests pass 9/9, flow-workspace tests pass 10/10, sanitizer checks pass,
all 63 runners pass, and native strict compilation has zero `flow_document.c` diagnostics.
W2-C5 now routes authored UI-document sync, destination inspection, and replacement through
the adapter. Pre-commit failures preserve destination/document/workspace/history identity;
warning commits update clean-state identity, remain warning results through Save-and-close,
and receive truthful editor feedback. Strict GCC/Clang UI-document tests pass 14/14,
UI-menu-workspace tests pass 13/13, sanitizer checks pass, all 63 runners pass, and native
strict compilation has zero `ui_document.c` diagnostics. W2-D1 managed one-level directory
creation now uses `platform_fs_ensure_directory` in both owners. It creates exactly one
requested directory, accepts only ordinary existing directories, rejects files and
links/reparse points,
and does not repair missing parents. Platform tests pass 7/7 locally and 10/10 natively;
UI-menu-workspace tests pass 14/14 and unified-editor tests pass 98/98 under strict compilers
and sanitizers. Native strict compilation has zero owner `mkdir` diagnostics and now remains
blocked only in `decal_document`, `map_catalog`, `scene_format`, and `sprite_document`.
W2-D2 now routes sprite file sync, no-follow inspection, directory moves, restoration, and
flat cleanup through platform capabilities. Clean pre-commit/rollback failures remain ordinary
I/O errors; incomplete cleanup/restoration is explicit; committed durability/cleanup warnings
publish path and clean state. Editor callers commit warning saves to the registry/runtime and
display durability warning. Platform tests pass 8/8 locally and 11/11 natively, sprite tests
pass 7/7, unified editor passes 99/99 under strict compilers and sanitizers, and all 63 runners
pass. Native `sprite_document.c` diagnostics are zero. W2-C6 now routes decal sync,
no-follow destination inspection, and replacement through `platform_fs`. Uncommitted
failures preserve destination/document identity; committed durability warnings publish
path, saved snapshot, and clean state; asset refresh continues after every committed
result. Strict GCC/Clang decal tests pass 7/7, asset refresh passes 7/7, unified editor
passes 99/99, focused sanitizers pass, and all 63 runners pass. At the W2-C6 checkpoint,
native Windows strict compilation had no decal-owner diagnostic and stopped only in
`map_catalog` and `scene_format`. W3 now moves direct-child enumeration and no-follow metadata into
`platform_catalog`; `MapCatalog` retains filtering, owned snapshots, sorting, and
transactional replacement. Local strict/sanitizer tests pass, native platform capabilities
pass 13/13, native `MapCatalog` passes 7/7 with reparse and multilingual evidence, and the
W3 native application checkpoint stopped only in `scene_format`. W4 now moves finite ASCII
double conversion into stateless per-call POSIX/UCRT `platform_number` operations while
retaining exact scene grammar and canonical bytes. Strict GCC/Clang number tests pass 5/5,
scene tests pass 24/24, focused sanitizers pass, all 64 local runners pass, and native Windows
number/scene owners pass 5/5 and 24/24. W5 adds normal Windows ENet `winmm`/`ws2_32` Make policy,
portable test fixtures, write-capable Windows durability sync, canonical decal bytes, correct
legacy/sprite commit handling, and bounded heap ownership for oversized test snapshots. Native
Windows now passes all 64 runners, `standards-core`, and PE/import inspection of 65 binaries. See
[`reviews/2026-09-14-v1-0-w2a-platform-foundation.md`](reviews/2026-09-14-v1-0-w2a-platform-foundation.md).
See also
[`reviews/2026-09-14-v1-0-w2b1-native-replacement-prototype.md`](reviews/2026-09-14-v1-0-w2b1-native-replacement-prototype.md).
See also
[`reviews/2026-09-14-v1-0-w2b2-native-scene-save-migration.md`](reviews/2026-09-14-v1-0-w2b2-native-scene-save-migration.md).
See also
[`reviews/2026-09-14-v1-0-w2c1-ui-preferences-migration.md`](reviews/2026-09-14-v1-0-w2c1-ui-preferences-migration.md).
See also
[`reviews/2026-09-14-v1-0-w2c2-material-document-migration.md`](reviews/2026-09-14-v1-0-w2c2-material-document-migration.md).
See also
[`reviews/2026-09-14-v1-0-w2c3-object-document-migration.md`](reviews/2026-09-14-v1-0-w2c3-object-document-migration.md).
See also
[`reviews/2026-09-14-v1-0-w2c4-flow-document-migration.md`](reviews/2026-09-14-v1-0-w2c4-flow-document-migration.md).
See also
[`reviews/2026-09-14-v1-0-w2c5-ui-document-migration.md`](reviews/2026-09-14-v1-0-w2c5-ui-document-migration.md).
See also
[`reviews/2026-09-14-v1-0-w2d1-managed-directory-creation.md`](reviews/2026-09-14-v1-0-w2d1-managed-directory-creation.md).
See also
[`reviews/2026-09-14-v1-0-w2d2-sprite-folder-publication.md`](reviews/2026-09-14-v1-0-w2d2-sprite-folder-publication.md).
See also
[`reviews/2026-09-14-v1-0-w2c6-decal-document-migration.md`](reviews/2026-09-14-v1-0-w2c6-decal-document-migration.md).
See also
[`reviews/2026-09-14-v1-0-w3-direct-child-catalog.md`](reviews/2026-09-14-v1-0-w3-direct-child-catalog.md).
See also
[`reviews/2026-09-14-v1-0-w4-locale-independent-number.md`](reviews/2026-09-14-v1-0-w4-locale-independent-number.md).
See also
[`reviews/2026-09-15-v1-0-w5-native-windows-functional.md`](reviews/2026-09-15-v1-0-w5-native-windows-functional.md).

The W1 design selects isolated `platform_fs`, `platform_catalog`,
`platform_number`, and `platform_path` responsibilities; preserves document-owned
serialization/history/dirty state; distinguishes commit state; keeps sprite-folder
publication separate; rejects Windows reparse points in managed directories/catalogs; and
requires strict UTF-8 wide APIs plus exact Linux/UCRT numeric parity. W4 has now proven that
parity, and W5 has proven the complete headless native Windows functional profile. Native Linux
L1 display/input acceptance passed on 2026-09-16; native Windows display automation is outside
the accepted Windows profile. See
[`V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md`](V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md).

The current future-work inventory is [`TODO.md`](TODO.md). Roadmap sequencing and
phase status belong in [`FEATURE_ROADMAP.md`](FEATURE_ROADMAP.md).

The roadmap commits Linux x64 and Windows x64 as required v1 release platforms,
Steam Deck / SteamOS as informational, and macOS as post-v1. It also promotes broad
Unicode/custom-font support, reliable pointer and major-context motion, the sprite
workbench follow-ups, directional sprites, sprite stacking, native project execution,
gameplay depth, audio, bounded cooperative multiplayer, and export. These are committed
outcomes, not claims of current implementation or Q1 readiness.

The 2026-09-12 verification-policy increment removed the temporary partial aggregate
suite, made `make test` the only complete functional aggregate, added strict
project-wide standards gates, and established typed tool-failure handling. The
implementation and current Gentoo Valgrind `FAIL-TOOL` evidence are recorded in
[`reviews/2026-09-12-verification-policy-and-tool-failure.md`](reviews/2026-09-12-verification-policy-and-tool-failure.md).
At that pre-expansion checkpoint, the complete 60-runner suite and repository-owned
standards guards passed. The 2026-09-14 B0 environment had no cppcheck on its command
path. Cppcheck 2.18.2 became available on 2026-09-16: its four serializer pointer-array
findings were resolved through explicit bounded-array initialization, its two
analysis-scope information IDs were narrowly classified, and real `make standards` plus
complete `make -j2 check` now pass. See
[`reviews/2026-09-16-cppcheck-gate-recovery.md`](reviews/2026-09-16-cppcheck-gate-recovery.md).
Native Valgrind also remains unavailable on the current command path; the canonical
pinned Ubuntu 24.04 amd64 container gate retains the recorded focused Memcheck evidence
for `test-decal-io` and `test-core`.
The complete Docker host/kernel requirements and operational handoff are in
[`DOCKER_VALGRIND_GATE.md`](DOCKER_VALGRIND_GATE.md).

The 2026-09-12 Linux platform survey now builds unchanged pinned-dependency profiles
for Ubuntu GCC, Ubuntu Clang, Fedora GCC, and informational Alpine musl GCC. Ubuntu
GCC, Fedora GCC, and Alpine pass strict application/test builds, all 62 runners, and
`standards-core`. Ubuntu Clang is `FAIL-PRODUCT` at strict application compilation
because many first-party and pinned SMC files lack final newlines. No source
remediation was performed. Current mechanics and evidence are in
[`PLATFORM_TESTING.md`](PLATFORM_TESTING.md) and
[`reviews/2026-09-12-linux-platform-survey.md`](reviews/2026-09-12-linux-platform-survey.md).

The follow-on regression increment expands the canonical suite to 62 runners with
direct config and asset-loader coverage, fixes and guards reflected-image curvature,
adds pure application-state transition coverage, enforces complete test registration,
defines the current UI and platform verification contracts, and adds current headless
benchmark/stability aggregates. The coverage map and residual risk ranking are in
[`reviews/2026-09-12-regression-coverage-audit.md`](reviews/2026-09-12-regression-coverage-audit.md).
The strict application build, complete 62-runner suite, UI standards aggregate,
standards-core, headless benchmarks, and focused extended stability workloads pass on
the current Gentoo host. Full ASan/LeakSanitizer, UBSan, the eight-mode matrix, and
gcov coverage generation also pass. External-tool, native-platform, and
display-backed residual gates remain explicitly deferred or unverified as recorded
in that audit.

## Current authority

- [`README.md`](../README.md) — project overview and user/developer entry points.
- [`README.md`](README.md) — documentation index and authority map for this
  directory.
- [`FEATURE_ROADMAP.md`](FEATURE_ROADMAP.md) — roadmap sequencing and phase
  status.
- [`TODO.md`](TODO.md) — unordered future/deferred work inventory.
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — current architecture notes and data-flow
  ownership context.
- [`VERIFICATION_POLICY.md`](VERIFICATION_POLICY.md) — canonical complete-suite,
  standards, and tool-failure policy.
- [`EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md`](EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md)
  — accepted unified-editor regression contract.

## Deferred source-level TODOs known after R12

- `src/assets.h`: sprite ID widening remains deferred.
- `src/lighting.c`: lighting cache key map/world revisions remain hard-coded to
  `1`.

## Current documentation cleanup status

Pass 1 added this current-state summary and the documentation index. Pass 2A
rewrote `ARCHITECTURE.md` as current-state authority. Passes 2B-2D created
`docs/archive/`, moved obvious historical phase/planning clusters, archived the
long roadmap, and demoted `handoff.md` to a compatibility pointer.

Historical phase records now live under [`archive/`](archive/). The long R0-R12
roadmap was preserved at
[`archive/roadmap/FEATURE_ROADMAP_LONG_R0_R12_2026-09-11.md`](archive/roadmap/FEATURE_ROADMAP_LONG_R0_R12_2026-09-11.md),
and the prior long handoff was preserved at
[`archive/handoffs/handoff_R12_docs_cleanup_2026-09-11.md`](archive/handoffs/handoff_R12_docs_cleanup_2026-09-11.md).

## Code standards corrective-action status

The 2026-09-11 non-SMC audit corrective pass is implemented, automatically
verified, manually approved, and closed. Its individual finding dispositions and
evidence are recorded in
[`reviews/2026-09-11-code-standards-audit.md`](reviews/2026-09-11-code-standards-audit.md).
Strict shared RGBA and numeric parsers now reject malformed recognized
asset/UI/decal fields; audited unsafe conversion/copy calls were removed from
handwritten `src/`; malformed-input regressions were added; and the unified-editor
test hook declaration moved out of the production-facing header.

All 58 runners in that audit's historical non-SMC scope passed in bounded batches,
and focused ASan/UBSan runs passed for the changed parser/loader modules. The
temporary partial-suite Make target has since been removed; `make test` is the only
complete aggregate and includes SMC runners. `cppcheck` was missing, while installed
Valgrind terminated at dynamic-loader startup with `SIGILL`. Under
[`VERIFICATION_POLICY.md`](VERIFICATION_POLICY.md), those outcomes are respectively
`FAIL-MISSING-TOOL` and `FAIL-TOOL`, not successful verification or product defects.
No Valgrind leak claim is made. Large editor/scene module splits and app UI
code-to-data migrations remain deferred pending separately scoped requirements.
SMC was not changed by the historical audit and remained outside its corrective
action; it is not excluded from current project-wide testing.
