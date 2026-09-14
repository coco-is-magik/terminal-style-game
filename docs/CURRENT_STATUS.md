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
evidence reconciliation** is Active at its documentation/evidence-only B0 increment.
Its focused execution authority is
[`V1_0_REQUIREMENTS_AND_EVIDENCE_PLAN_2026-09-14.md`](V1_0_REQUIREMENTS_AND_EVIDENCE_PLAN_2026-09-14.md).
Production portability changes have not begun.

V1-0 must also reconcile any current mirror report with the planar reflected-curvature
defect already reproduced, corrected, and guarded on 2026-09-12. Mirror source changes
remain evidence-blocked unless a distinct failing scene is captured. In parallel,
documentation-only Q1 preparation may inventory measurable UI rules and Unicode/font
research. Sprite-painter behavior characterization may proceed under a focused plan, but
final glyph-aware workbench formats depend on the Unicode/glyph foundation.

The initial bounded asset scan found no non-ASCII runtime-authored asset file.
`assets/README.md` contains non-ASCII documentation bytes and is classified separately;
no legacy code-page meaning has been assigned.

The 2026-09-14 B0 local baseline passes the strict default GCC/SMC-stream application
build, both focused mirror owners (4/4 and 19/19), the complete 62-runner build and
functional aggregate, all nine UI standards owners, `standards-core`, and smoke. A local
Clang 22 strict application build also passes, but does not replace required Ubuntu Clang
18 profile evidence. The first full `make test` attempt timed out during compilation; the
recorded narrower `test-build` then `test` recovery passed.

`make benchmark-headless` is currently `FAIL-PRODUCT`: the deterministic surface-render
workload measured `flat_height_avg_ms=6.188057` against its 6 ms budget. Later benchmark
workloads and `stability-headless` remain unrun behind that blocker. The B0 scan found 192
of 254 scoped production/test/pinned-SMC C files lacked final newlines, matching the
preserved Ubuntu Clang 18 failure class even though local Clang 22 accepted them. P1
performance reproduction remains independent; Windows adapter work remains at the W1
design gate.

C1 is partially implemented and locally verified as of 2026-09-14. Exactly 188 tracked
first-party/test files are their original bytes plus one final LF; strict GCC and local
Clang builds, focused SMC owners, `standards-core`, the complete runner build, and all 62
functional runners pass. Four affected SMC files come from the image-owned pinned SMC
archive, so local ignored-vendor corrections were restored and a reviewed pinned revision
is still required. The required Ubuntu Clang profile did not run because
`/var/run/docker.sock` is unavailable. Its wrapper output incorrectly called Docker status
1 a contained `FAIL-PRODUCT`; policy-correct classification is `FAIL-TOOL`. See
[`reviews/2026-09-14-v1-0-c1-final-newline-remediation.md`](reviews/2026-09-14-v1-0-c1-final-newline-remediation.md).

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
standards guards passed. That historical run found cppcheck product findings in
`src/scene_format.c`; the fresh 2026-09-14 B0 environment has no `cppcheck` on its
command path, so current `make standards` evidence is unavailable and is not claimed.
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