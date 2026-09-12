# Verification Policy and Tool-Failure Gate Implementation — 2026-09-12

## Scope and requirements

This increment removed the temporary aggregate suite that excluded SMC and made
project-wide verification outcomes explicit. It did not change product C code,
renderer behavior, SMC behavior, file formats, or accepted UI behavior.

Required outcomes:

- `make test` remains the complete functional suite and includes both SMC runners;
- required tools no longer produce status-0 skips when missing or broken;
- product defects remain distinguishable from tool/environment failures;
- a tool failure triggers investigation and discussion before corrective action;
- the historical 2026-09-11 audit remains an accurate scoped record.

## Implementation

- Removed `NON_SMC_TEST_RUNNERS`, `test-non-smc`, and its phony declaration.
- Added current authority [`../VERIFICATION_POLICY.md`](../VERIFICATION_POLICY.md).
- Added typed Make output for `PASS`, `FAIL-PRODUCT`, `FAIL-MISSING-TOOL`, and
  `FAIL-TOOL`; the policy also defines `FAIL-TIMEOUT` for harness interruption.
- Changed `make leak`, `make style`, and `make coverage` to fail when their required
  tools are missing instead of reporting a successful skip.
- Added `make standards`, containing required cppcheck analysis plus repository-owned
  unsafe-call, deprecated-legacy-call, and current-renderer/current-lighting guards.
- Changed `make check` to run the complete application build, test suite, and
  standards aggregate.

The Make targets use tool-specific diagnostic status 100 for Valgrind/cppcheck
findings. Other nonzero tool statuses are classified as `FAIL-TOOL`; missing tools
are `FAIL-MISSING-TOOL`. Valgrind runners are first executed without Valgrind so an
ordinary runner failure is reported as `FAIL-PRODUCT` rather than confused with tool
startup failure.

## Verification evidence

- `make all`: passed; strict flags remain C11 `-Wall -Wextra -Wpedantic -Werror`.
- `make test`: passed with the complete 60-runner inventory, including
  `test-smc-state-tracker` and `test-smc-indexed-state-tracker`. The command exceeded
  the tool's initial 300-second observation window during compilation but remained
  alive and later completed with exit status 0; this was not a test failure.
- `make check-unsafe-calls check-legacy-unused check-current-renderer`: passed.
- `make standards CPPCHECK=true`: passed as a control-flow test of the aggregate and
  all repository-owned guards. This is not cppcheck analysis evidence.
- `make check CPPCHECK=true`: passed as a final orchestration check of the strict
  application build, complete suite, and standards dependencies. The `true`
  override means this is not real cppcheck or complete strict-gate evidence.
- Real `make style`: `FAIL-MISSING-TOOL`; cppcheck is not installed. The target
  returned nonzero instead of silently succeeding.
- Missing-tool control checks for cppcheck, Valgrind, and gcov each returned nonzero
  and printed `FAIL-MISSING-TOOL`.
- A failing cppcheck-command control returned nonzero and printed `FAIL-TOOL`.
- A controlled cppcheck diagnostic status of 100 returned nonzero and printed
  `FAIL-PRODUCT`.

No C source changed, so this workflow/documentation increment did not require new C
unit tests or a sanitizer rebuild. Existing application and functional behavior was
protected by the strict build and complete suite.

## Valgrind `FAIL-TOOL` investigation

`make leak` first ran `build/test-decal-io` normally: all 16 tests passed. Valgrind
then terminated before project code could run:

```text
Valgrind 3.27.1
status 132 / SIGILL
unhandled bytes: 62 F1 7F 48 7F 84 24 30 00 00 00
location: _dl_start in /lib64/ld-linux-x86-64.so.2
heap usage before termination: 0 allocations
```

The same failure, instruction bytes, `_dl_start` location, status 132, and zero
allocations reproduced with a separately compiled minimal C program whose `main`
only calls `puts("ok")`. This isolates the observed failure from project behavior.

Captured environment:

- Gentoo Linux 2.18, kernel `7.2.2-gentoo`, x86_64;
- AMD Ryzen 9 7950X with AVX-512 features available;
- GCC 15.3.0, GNU ld 2.46.1, glibc/dynamic loader 2.43-r2;
- Valgrind 3.27.1.

Classification: `FAIL-TOOL`. The likely boundary is Valgrind/LibVEX support for an
instruction used by this Gentoo dynamic loader. This is an investigation result,
not an approved corrective action. Per policy, no tool version, loader, compiler
flags, or product code was changed. A corrective action should be discussed before
choosing among tool upgrade/patching, loader/toolchain adjustment, or running the
Valgrind gate on another supported environment.

The generated diagnostic is preserved at
`build/valgrind-test-decal-io.log` in the current working tree, but `build/` is a
cleanable output directory and is not durable project documentation. The relevant
diagnostic is therefore also recorded above.

## Remaining work

- Install/approve cppcheck before claiming real `make standards` or `make check`
  completion under the new strict policy.
- Discuss the Valgrind environment evidence before committing to corrective action.
- Expand Valgrind beyond the two historical focused runners in a separate planned
  increment; the current target does not provide project-wide leak coverage.
- Continue the regression-coverage audit with the known reflected-image curved-edge
  fixture, performance/stability modernization, UI standards, and platform matrix as
  separately testable increments.