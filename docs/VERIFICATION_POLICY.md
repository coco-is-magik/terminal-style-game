# Verification Policy

This document defines the current project-wide verification outcomes and the
required response when a verification tool cannot produce trustworthy evidence.
It applies to local development, audits, closeouts, and automated environments.

## Canonical gates

- `make test` runs the complete functional runner inventory, including SMC tests.
  Partial aggregate suites must not be used as project-wide regression evidence.
- `make test-ui-standards` runs the focused owners of currently accepted UI rules.
- `make standards` runs required static analysis, the handwritten-production
  unsafe-call/project-structure/test-inventory guards, and repository-owned
  legacy/current API guards. Cppcheck defect diagnostics retain error exit status;
  only named tool-version analysis-scope information may be suppressed, and the
  repository-owned policy guard rejects broad severity/category suppression.
  `make standards-core` runs the dependency-free subset.
- `make check` builds the application and runs the complete functional and standards
  gates.
- `make asan`, `make ubsan`, and `make leak` provide separate runtime-safety evidence.
  `make leak` is the canonical, pinned Ubuntu 24.04 container gate. It compiles and
  directly executes `test-decal-io` and `test-core` before running Memcheck. The
  optional `make leak-native` is diagnostic evidence and does not replace the
  canonical gate.
- `make coverage` runs the complete suite with coverage instrumentation and produces
  gcov reports.
- `make benchmark-headless` runs current deterministic performance workloads.
  `make stability-fast` and `make stability-headless` run increasing deterministic
  stability tiers; display-backed `make stability` remains separate.
- `make platform-survey` attempts every configured Linux profile and records all
  typed outcomes without treating expected incompatibility as pass. `make
  platform-check` fails when any required profile is not compatible. Neither target
  is part of `make check` until platform-gating policy is separately approved.

Benchmark records must include `make verification-environment` output or equivalent
platform, compiler, flags, processor-count, and dependency-artifact context. Timing
results from unlike environments are separate evidence, not one interchangeable
baseline.

Required gates fail when their required tool is absent or cannot run. They must not
report a missing or broken tool as a successful skip.

The canonical leak container mounts the source at `/source` read-only, copies the
required project tree to writable tmpfs `/work`, and writes logs to the host
`build/valgrind-container/` directory. Verification execution has no network,
runs as the invoking UID/GID with the source directory's supplementary group,
drops capabilities, and uses `no-new-privileges`. Compiler, Make, Valgrind, glibc,
SDL, SDL_mixer, cmocka, and ENet are image-owned; host `build/`, `vendor/dist`, and
host-built SDL are excluded from the copy. Image construction is a separate,
networked trust step and verifies pinned source archive hashes.
Docker host/kernel prerequisites, exact operational commands, security implications,
and troubleshooting are maintained in
[`DOCKER_VALGRIND_GATE.md`](DOCKER_VALGRIND_GATE.md); this policy does not duplicate
that changing platform detail.

## Outcome taxonomy

Use these outcome names in command output and verification records:

- `PASS`: the named gate completed and produced the expected evidence.
- `FAIL-PRODUCT`: the tool ran sufficiently to identify a test, build, standards,
  memory-safety, stability, or other product defect.
- `FAIL-MISSING-TOOL`: a required executable or dependency was not available. No
  evidence from that tool was produced.
- `FAIL-TOOL`: the tool started but crashed, rejected the environment, failed during
  its own initialization, or otherwise did not produce trustworthy product evidence.
- `FAIL-TIMEOUT`: the harness or command exceeded its time budget or was interrupted.
  The result is incomplete evidence, not a product pass or product failure.

An unsupported platform/tool combination is reported as `FAIL-TOOL` with the
unsupported combination identified. Any temporary exception requires an explicit,
documented decision; it is not a pass.

## Tool-failure standard operating procedure

For `FAIL-MISSING-TOOL`, `FAIL-TOOL`, or `FAIL-TIMEOUT`:

1. Stop before changing product code based on the failed tool invocation.
2. Confirm the failure class. Re-run the narrowest command needed to distinguish a
   product defect from missing tooling, tool initialization failure, or interruption.
3. Check common causes and capture relevant development-environment information:
   - exact command, exit status, signal, and complete relevant diagnostic;
   - tool name, path, and version;
   - operating system, distribution/release, kernel, and architecture;
   - compiler, linker, C library, and dynamic-loader versions;
   - executable build mode and relevant compile/link flags;
   - dependency/library versions and loader search paths;
   - available disk, memory, permissions, and container/virtualization constraints
     when relevant;
   - whether a minimal program works under the tool;
   - whether the project runner works without the tool and whether failure occurs
     before entering project code;
   - results from related but non-equivalent tools, such as ASan/LeakSanitizer and
     UBSan when Valgrind fails.
4. Report the classification and captured evidence for discussion before committing
   to corrective action. Do not silently upgrade, downgrade, pin, replace, or exempt
   a tool, and do not modify product code, until the likely cause and tradeoffs have
   been reviewed.
5. Record the agreed action and later verification. Possible actions include fixing
   the development environment, changing a tool version, running the gate on a
   supported platform, or documenting a narrowly scoped temporary exception.

Sanitizers and alternative analyzers provide complementary evidence. They may be run
while a tool failure is investigated, but they do not silently convert the failed
gate into `PASS`.

## Verification records

A review or handoff must state the command, outcome class, and evidence actually
obtained. In particular:

- do not describe an absent or broken tool as status 0 verification;
- do not describe a tool crash as application instability;
- do not describe an interrupted aggregate build as a failed test if no test ran;
- preserve logs or the relevant diagnostics for every non-product failure;
- identify all unrun or incomplete gates as unverified.

The 2026-09-11 Valgrind event is the motivating example: Valgrind was installed, but
terminated with `SIGILL` in dynamic-loader startup before it produced leak evidence.
Its classification is `FAIL-TOOL`, not `FAIL-PRODUCT` and not `PASS`.