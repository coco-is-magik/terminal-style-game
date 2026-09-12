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

The 2026-09-11 non-SMC code-standards audit and corrective-action pass are
manually approved and closed. No post-closeout implementation bucket is selected.
Before further work, choose one deferred structural, data-authority, or roadmap
item and write a focused requirements and regression plan.

The current future-work inventory is [`TODO.md`](TODO.md). Roadmap sequencing and
phase status belong in [`FEATURE_ROADMAP.md`](FEATURE_ROADMAP.md).

The 2026-09-12 verification-policy increment removed the temporary partial aggregate
suite, made `make test` the only complete functional aggregate, added strict
project-wide standards gates, and established typed tool-failure handling. The
implementation and current Gentoo Valgrind `FAIL-TOOL` evidence are recorded in
[`reviews/2026-09-12-verification-policy-and-tool-failure.md`](reviews/2026-09-12-verification-policy-and-tool-failure.md).
At that pre-expansion checkpoint, the complete 60-runner suite and repository-owned
standards guards passed. cppcheck is now installed and working; its current findings
in `src/scene_format.c` are `FAIL-PRODUCT`, not a tool failure, and are outside this
Valgrind increment. Native Valgrind remains incompatible with the host loader, while
the canonical pinned Ubuntu 24.04 amd64 container gate now provides focused Memcheck
evidence for `test-decal-io` and `test-core`.
The complete Docker host/kernel requirements and operational handoff are in
[`DOCKER_VALGRIND_GATE.md`](DOCKER_VALGRIND_GATE.md).

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