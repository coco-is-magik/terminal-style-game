# V1-0 Platform Profile Evidence — 2026-09-14

## Status

**Platform harness verified; Ubuntu and Windows product blockers reproduced under
their repository-owned bounds.**

This increment resumed V1-0 platform evidence after Docker and the native Windows
VM path became operational. No external timeout wrapped a platform profile. The
Docker and Windows harnesses retained their own established build, execution,
readiness, command, evidence-retrieval, and graceful-shutdown bounds.

No product source, warning policy, runner inventory, dependency pin, or VM lifecycle
contract was changed in this increment.

## Bound policy

- `platform-image-ubuntu-clang` used its internal 1800-second image-build bound.
- `platform-test-ubuntu-clang` used the Docker runner's internal 3600-second bound and
  in-container 900/1800/1800/120-second phase bounds.
- `platform-test-windows` used the existing libvirt/QGA command, readiness, guest
  phase, retrieval, and shutdown bounds.
- No external `timeout` command was placed around any of those Make targets.
- Long execution on this low-end host was not treated as a stall or failure.

## Platform harness

`make test-platform-harness` passed:

- profile classifier;
- required-profile check;
- provider dispatch;
- libvirt lifecycle;
- five QGA client tests;
- twelve Windows guest tests;
- twelve Windows dependency tests;
- ten Windows product tests;
- survey continuation tests.

The current classifier already contains the regression discovered during C1: Docker
status 1 accompanied by a daemon-connect diagnostic is `FAIL-TOOL`, while status 1
without that host-runtime diagnostic remains a contained `FAIL-PRODUCT`. The parent
worktree was clean at inspection, so this increment verified existing behavior and
does not claim to have authored that correction.

## Docker and Ubuntu Clang

`docker info` passed and reported Docker server 29.7.2, Linux x86_64.

### Missing-image result

The first no-wrapper `make platform-test-ubuntu-clang` attempt correctly produced:

```text
PHASE=container-start
OUTCOME=FAIL-TOOL
REASON=docker-daemon-or-runtime
STATUS=125
```

The log showed that the trusted local profile image did not exist and Docker refused
an unavailable registry pull. Docker itself remained healthy. This result was not
treated as product evidence and was not retried unchanged.

### Image preparation

`make platform-image-ubuntu-clang` completed within its internal bound:

```text
OUTCOME=PASS
REASON=image-built
STATUS=0
image_id=sha256:41ec8d5da69902a22385b3994d476597e525aae2881d65e7162faa87da14bac2
os=linux
architecture=amd64
```

The image used the pinned Ubuntu digest and dependency commit/archive hashes in
`tools/platform-profiles/dependencies.env`. Image construction downloaded and
verified the configured source archives before use.

### Required Ubuntu Clang result

With that prerequisite changed, rerunning `make platform-test-ubuntu-clang` produced
trustworthy product evidence:

```text
PROFILE_ID=ubuntu-24.04-clang
PROFILE_ROLE=required
PHASE=strict-app-build
OUTCOME=FAIL-PRODUCT
REASON=application-compile-failure
STATUS=2
```

Environment:

```text
Ubuntu Clang 18.1.3
GNU ld 2.42
glibc 2.39
GNU Make 4.3
-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
SMC commit 3783ae976b13bf3e1d6bfa437d2927509871ef30
```

All first-party newline diagnostics corrected by C1 are absent. The complete
66-line compiler log contains only final-newline errors from these four image-owned
pinned SMC files:

- `vendor/src/smc/include/smc.h`;
- `vendor/src/smc/src/c/smc_artifact.h`;
- `vendor/src/smc/src/c/smc_runtime_stub.c`;
- `vendor/src/smc/src/c/smc_artifact.c`.

`smc.h` appears repeatedly because several translation units include it; this does
not represent additional affected files. Strict test build, complete test execution,
and `standards-core` correctly did not run after the application prerequisite failed.

**Disposition:** C1's 188 parent-repository newline corrections are confirmed effective
in the required profile. C2 remains blocked only on obtaining a reviewed SMC revision
with equivalent final-LF corrections and updating the pinned commit/archive hash through
the dependency review process. Patching fetched sources during image construction or
using the ignored local clone remains forbidden.

## Native Windows profile

The first invocation without a VM name returned immediately:

```text
FAIL-MISSING-TOOL
phase=vm-locate
reason=windows-vm-not-configured
status=2
```

No VM was started or changed. The tracked profile intentionally contains no
machine-local VM identity. Read-only enumeration of `qemu:///session` then found
exactly one domain, `win10-survey`. That observed domain identity was supplied only
to the subsequent invocation.

`PROFILE_VM_NAME=win10-survey make platform-test-windows` completed under the harness's
own bounds and produced:

```text
PROFILE_ID=windows-10-x64-gcc
PROFILE_ROLE=informational
PRIMARY_OUTCOME=FAIL-PRODUCT
PRIMARY_PHASE=strict-app-build
PRIMARY_REASON=application-compile-failure
PRIMARY_STATUS=1
VM_INITIAL_STATE=shut off
VM_STARTED_BY_HARNESS=true
VM_FINAL_STATE=shut off
CLEANUP_OUTCOME=PASS
CLEANUP_REASON=vm-shut-down-by-qga
CLEANUP_STATUS=0
```

The domain was independently confirmed `shut off` after the command. No platform,
QGA, Docker, or pager process remained running.

### Recorded Windows environment

- Windows 10 Pro 22H2, build 19045.4529, x86-64;
- PowerShell 5.1.19041.4522;
- MSYS2 UCRT64;
- GCC target `x86_64-w64-mingw32`, GCC 16.2.0;
- GNU ld 2.47.20260726;
- GNU Make 4.4.1;
- CMake 4.4.3;
- pkgconf 3.0.7.

### Strict compilation findings

The 165-line strict application log reproduces four required W1 capability families:

1. **Atomic-write durability and metadata**
   - unavailable `fsync` call sites across decal, flow, material, object, scene,
     sprite, authored UI, and UI-preference persistence;
   - unavailable `fchmod` in scene save.
2. **Safe direct-child catalog inspection**
   - unavailable `dirfd`, `fstatat`, and `AT_SYMLINK_NOFOLLOW` in `map_catalog`.
3. **Locale-independent numeric parsing/writing**
   - unavailable POSIX `locale_t`, `newlocale`, `strtod_l`, `uselocale`,
     `freelocale`, and `LC_NUMERIC_MASK` in `scene_format`.
4. **Directory creation**
   - POSIX two-argument `mkdir` conflicts in `ui_menu_workspace` and
     `unified_editor`.

The exact compiler output also confirms that W1 must include `fchmod` and `strtod_l`,
not only the shorter representative list from the first 2026-09-13 result.

Strict test build, complete functional tests, `standards-core`, and binary inspection
correctly did not run after strict application failure.

## Incidental command handling

A read-only `git log` inspection of the nested SMC repository opened a pager because
`--no-pager` was omitted. The exact `git` and `less` processes were identified and
terminated before starting the Windows lifecycle. No product or platform process was
affected, and no pager remained afterward.

## Verification and artifacts

Evidence is retained under ignored build output:

- `build/platform-profiles/ubuntu-24.04-clang/`;
- `build/platform-profiles/windows-10-x64-gcc/`.

Named checks:

| Check | Result |
|---|---|
| `make test-platform-harness` | `PASS` |
| Docker server probe | `PASS`, 29.7.2 Linux x86_64 |
| Ubuntu Clang image build | `PASS`, image identity recorded |
| Required Ubuntu Clang profile | `FAIL-PRODUCT`, four pinned SMC newline files only |
| Native Windows profile | `FAIL-PRODUCT`, W1 portability families reproduced |
| Windows lifecycle cleanup | `PASS`, QGA shutdown and final `shut off` |
| External timeout around platform commands | not used |

## Next safe action

1. Write and approve the W1 platform-capability decision record using the complete
   native Windows call-site evidence above before changing product code.
2. Obtain a reviewed strict-compatible SMC revision and update the pinned dependency
   commit/hash; then rebuild the Ubuntu Clang image and rerun the required profile.
3. Keep P1 surface-render performance reproduction independent.

Do not rerun Windows expecting a pass before W2-W4 portability adapters exist. Do not
use an external timeout around native platform profiles on this host.