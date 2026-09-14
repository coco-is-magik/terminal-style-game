# Platform Testing

This document is the operational authority for reproducible platform surveys.
It defines what the Docker profiles test, how outcomes are classified, and how to
run and interpret the current matrix. Native platform support requirements remain
defined by [`PLATFORM_VERIFICATION_PROFILES.md`](PLATFORM_VERIFICATION_PROFILES.md).

## Purpose and scope

The immediate platform-testing objective is to run the unchanged project in
controlled environments and preserve incompatibilities as evidence. A survey does
not modify product code, add portability shims, weaken strict warnings, omit test
runners, or convert an anticipated failure into a pass.

Docker profiles provide Linux userspace, compiler, libc, and dependency-build
evidence while sharing the Gentoo host kernel. They do not prove native Windows,
macOS, Steam Deck/Gamescope, display, input, lifecycle, packaging, or non-amd64
behavior.

## Current profiles

| Profile | Role | Environment | Current result |
|---|---|---|---|
| `ubuntu-24.04-gcc` | Required | Ubuntu 24.04, glibc 2.39, GCC 13.3.0 | `PASS` through complete profile |
| `ubuntu-24.04-clang` | Required | Ubuntu 24.04, glibc 2.39, Clang 18.1.3 | `FAIL-PRODUCT` at strict application build |
| `fedora-gcc` | Required | Fedora 43, glibc 2.42, GCC 15.3.1 | `PASS` through complete profile |
| `alpine-musl-gcc` | Informational | Alpine 3.22, musl 1.2.5, GCC 14.2.0 | `PASS` through complete profile |
| `windows-10-x64-gcc` | Informational | Windows 10 Pro 22H2 x64, UCRT64 GCC 16.2.0 | `FAIL-PRODUCT` at strict application compilation |

Alpine is a portability probe, not a supported-platform promise. Its result does
not determine `platform-check` unless its profile role is deliberately promoted.
The v1 product roadmap requires native Linux x64 and Windows x64 release evidence;
the current informational `windows-10-x64-gcc` survey remains evidence of a known
`FAIL-PRODUCT`, not yet a passing required release profile. Steam Deck / SteamOS is
informational for v1, and macOS is post-v1.

## Profile providers

Each profile declares `PROFILE_PROVIDER`. The current Linux profiles use `docker`;
profiles without this field continue to default to Docker for compatibility with
older local fixtures. Provider-neutral dispatch separates preparation from profile
execution:

```text
docker:
  prepare = build and classify the profile image
  run = execute the existing isolated Docker profile

libvirt-windows:
  prepare = validate KVM, libvirt, domain identity, and the QGA channel
  run = enforce VM ownership, readiness, guest-command, and cleanup lifecycle
```

The `libvirt-windows` host lifecycle is implemented. The machine-local VM name must
be supplied through ignored local configuration or `PROFILE_VM_NAME`; it is not
stored in a tracked profile. The host requires Python 3 for bounded QGA JSON,
process, and file transport; only Python's standard library is used.

The lifecycle runner records the initial state before acting. It starts only a
`shut off` VM and stops only a VM whose start request succeeded. An initially
running or concurrently externally started VM remains running. Paused, suspended,
blocked, crashed, and unknown states are rejected without mutation. Shutdown uses
QGA followed by an ACPI fallback; forced power-off is never used.

Primary compatibility and cleanup evidence are separate in `result.env`:

```text
PRIMARY_OUTCOME  PRIMARY_PHASE  PRIMARY_REASON  PRIMARY_STATUS
VM_INITIAL_STATE  VM_STARTED_BY_HARNESS  VM_FINAL_STATE
CLEANUP_OUTCOME  CLEANUP_REASON  CLEANUP_STATUS
OUTCOME  PHASE  REASON  STATUS
```

A cleanup failure controls the aggregate outcome but does not overwrite the
primary result.

### Windows guest workspace

The implemented guest runner records and validates Windows 10 x64 identity, then
verifies the MSYS2 UCRT64 command boundary. It records GCC target/version, linker,
GNU Make, CMake, and pkgconf versions. It does not install or update any guest tool.

Each run creates a unique workspace under:

```text
C:\platform-survey\runs\<run-id>
```

The source payload uses an explicit build-relevant allowlist: `src/`, `tests/`,
`assets/`, `scripts/`, `tools/`, the unchanged `Makefile`, runtime configuration,
top-level project/license reports, and `vendor.sh`. It excludes `.git/`, `.cache/`,
`build/`, `vendor/`, Python bytecode, and non-build documentation/reference/example
trees. In particular, Linux dependency artifacts and the writable live repository
never enter the guest.

The deterministic ZIP has a default 256 MiB ceiling and is uploaded in bounded QGA
chunks. The host manifest and run marker are transferred separately. Windows
extracts to guest-local NTFS, generates its own SHA-256 manifest, and the host
rejects malformed, mismatched, duplicate, or stale evidence. After evidence
retrieval, cleanup validates and removes only the exact current run workspace.
Routine execution validates the published dependency set, creates only guest-local
`vendor/` and `build/` adaptations, confirms the unchanged 62-runner inventory, and
runs the normal Make targets in order. Only `CC=gcc` is supplied; warning flags,
libraries, runtime paths, targets, and first-party files are not changed. Logs are
retrieved after every reachable phase, and the run stops at the first failure.

If all Make phases pass, exactly 63 outputs (`ascii-fps.exe` and 62 test executables)
must identify as PE x86-64 and must not import `msys-2.0.dll` or `cygwin1.dll`.

### Windows dependency bootstrap

Windows dependencies are provisioned by an explicit operator command and are never
built by `platform-survey` or ordinary guest preparation. The local profile path is
machine-specific and ignored:

```sh
make WINDOWS_PROFILE=/path/to/windows.local.env \
  WINDOWS_DEPENDENCY_STEP=<step> \
  platform-bootstrap-windows-dependencies
```

Run the bounded steps in order:

```text
prepare
transfer-SDL
transfer-SDL_mixer
transfer-enet
transfer-cmocka
transfer-smc
extract
build-SDL
build-SDL_mixer
build-enet
build-cmocka
sources
manifest
publish
cleanup
validate
```

Each step runs through the ownership-aware lifecycle and restores a harness-started
VM to `shut off`. A clean SDL build may require rerunning the same incremental
`build-SDL` step if the outer operator time bound interrupts it; no detached guest
process remains after lifecycle cleanup.

The acquisition step downloads only the five URLs derived from
`dependencies.env`, limits each archive to 128 MiB, and verifies SHA-256 before
cache publication and every guest transfer. Archives and their host manifest stay
under ignored `build/platform-profiles/<profile>/dependency-archives/`.

The guest extracts the unchanged archives under an exact staging path. It excludes
only `*/Xcode/*`: the accepted SDL_mixer archive contains four macOS framework
symlinks there which Windows cannot create, and no other accepted archive contains
symlinks. SDL disables only the optional OpenGL ES backend because the prepared
UCRT64 environment does not include external `EGL/egl.h`; desktop OpenGL, D3D,
D3D11, D3D12, Vulkan, and normal Windows subsystems remain enabled.

Successful publication atomically moves the staged install to:

```text
C:\platform-survey\dependencies\<dependency-set-id>
```

The ID covers manifest schema, accepted source commit/hash pairs, and build
options. Routine validation checks the schema, UCRT target, build options, exact
tool versions, required project artifacts, and every file in
`artifact-sha256.txt`. Missing sets are `FAIL-MISSING-TOOL`; existing incomplete,
tampered, or mismatched sets are `FAIL-TOOL`. Bootstrap never replaces an existing
set in place.

## Commands

Build and run one profile:

```sh
make platform-image-ubuntu-gcc
make platform-test-ubuntu-gcc

make platform-image-ubuntu-clang
make platform-test-ubuntu-clang

make platform-image-fedora-gcc
make platform-test-fedora-gcc

make platform-image-alpine-gcc
make platform-test-alpine-gcc

PROFILE_VM_NAME=<local-domain-name> make platform-prepare-windows
PROFILE_VM_NAME=<local-domain-name> make platform-test-windows
```

The tracked Windows profile deliberately contains no VM name. Supply it through the
environment or ignored machine-local configuration. Preparation is non-mutating;
profile execution uses the ownership-aware lifecycle and restores a VM it started.

Build all profile images:

```sh
make platform-images
```

Run every profile and continue after incompatibilities:

```sh
make platform-survey
```

Require all profiles whose role is `required` to pass:

```sh
make platform-check
```

Validate the harness independently of current product results:

```sh
make test-platform-harness
```

`platform-survey` returning success means every configured profile was attempted
and classified. It does **not** mean every profile passed. `platform-check` is the
compatibility gate and currently fails because Ubuntu Clang is incompatible under
the unchanged strict build.

## Ordered profile phases

Each profile stops at its first failed prerequisite:

1. Validate source/output mounts and required tools.
2. Copy the source into the writable tmpfs workspace.
3. Record the environment and installed-package manifests.
4. Validate image-owned dependency artifacts.
5. Build the application with strict C11 warnings-as-errors.
6. Build the complete 62-runner inventory without execution.
7. Execute the complete `make test` aggregate.
8. Run `make standards-core`.

`make test-build` is an orchestration-only target whose prerequisites are exactly
the existing `TEST_RUNNERS`. It does not modify test registration or behavior.

Later phases do not run after a prerequisite fails. The survey host proceeds to
the next profile so one incompatibility does not hide the rest of the matrix.

## Strict build contract

Every profile uses the existing flags:

```text
-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
```

The profile compiler is passed as `CC=...`; product files and warning flags remain
unchanged. Ordinary diagnostics are `FAIL-PRODUCT`. Compiler internal errors,
signals, invalid images, Docker runtime failures, and harness failures are
`FAIL-TOOL` or `FAIL-TIMEOUT` as applicable.

## First native Windows result

The first unchanged Windows execution on 2026-09-13 validated Windows 10 x64,
UCRT64 GCC, all 2,464 dependency artifact hashes, all 397 transferred source
path/hash entries, and the exact 62-runner inventory. The strict application build
then failed under the existing warnings-as-errors policy:

```text
FAIL-PRODUCT phase=strict-app-build reason=application-compile-failure status=1
```

The diagnostics are native API incompatibilities, including unavailable POSIX
`fsync`, `dirfd`, `fstatat`, `AT_SYMLINK_NOFOLLOW`, POSIX two-argument `mkdir`, and
POSIX locale APIs. No compatibility remediation was applied. Test building, test
execution, standards, and native-binary inspection were correctly not attempted.
The VM began `shut off`, was started by the harness, and was restored to `shut off`
through QGA. See
[`reviews/2026-09-13-windows-10-first-result.md`](reviews/2026-09-13-windows-10-first-result.md).

## Dependency identity

Every profile builds the same pinned dependency sources inside its image:

```text
SDL       f48525aa703e0e11134347b38571661d6ca829fd
SDL_mixer 3075d3eda55ce295c6919d330edb2554ff4edb5b
ENet      8be2368a8001f28db44e81d5939de5e613025023
cmocka    7f736f65dc7e21499702d531a1de84cc5308cb7a
SMC       3783ae976b13bf3e1d6bfa437d2927509871ef30
```

The exact archive SHA-256 values are in
[`../tools/platform-profiles/dependencies.env`](../tools/platform-profiles/dependencies.env).
All archives are verified before extraction. SDL uses the upstream-supported
headless `SDL_UNIX_CONSOLE_BUILD=ON` configuration.

Base images are pinned by digest:

```text
Ubuntu 24.04 sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254
Fedora 43    sha256:a651ddf48ea28a06ed4e1e6519f51c9f47e7a5a138722ade87369b8fbb7e5b42
Alpine 3.22  sha256:14358309a308569c32bdc37e2e0e9694be33a9d99e68afb0f5ff33cc1f695dce
```

Distribution package versions are recorded from each built image in
`packages.txt`. Package repository snapshots are not pinned, so rebuilding later
can resolve different package revisions even with the same base digest. This is a
known reproducibility limit, not hidden evidence.

## Execution boundary

Profile execution follows the canonical Valgrind container boundary:

- source mounted read-only at `/source`;
- build workspace in executable `/work` tmpfs;
- host `.git`, `.cache`, `build`, and `vendor` excluded from the copy;
- only image-owned `vendor/dist`, SDL headers, and SMC source connected;
- network disabled;
- invoking UID/GID plus source-directory supplementary group;
- all capabilities dropped;
- `no-new-privileges`;
- read-only container root;
- output limited to `build/platform-profiles/<profile>/`.

Image construction is a separate networked trust step. It retrieves distribution
packages and checksum-verified dependency archives.

## Outcome taxonomy

Examples of stable classifications:

```text
PASS: reason=profile-compatible

FAIL-PRODUCT: reason=dependency-build-failure
FAIL-PRODUCT: reason=application-compile-failure
FAIL-PRODUCT: reason=test-compile-failure
FAIL-PRODUCT: reason=test-runner-failure
FAIL-PRODUCT: reason=standards-core-failure

FAIL-MISSING-TOOL: reason=docker-not-found
FAIL-MISSING-TOOL: reason=compiler-not-found
FAIL-MISSING-TOOL: reason=make-not-found

FAIL-TOOL: reason=image-build-failure
FAIL-TOOL: reason=docker-daemon-or-runtime
FAIL-TOOL: reason=compiler-crash
FAIL-TOOL: reason=container-mount-contract

FAIL-TIMEOUT: reason=image-build-timeout
FAIL-TIMEOUT: reason=build-timeout
FAIL-TIMEOUT: reason=execution-timeout
```

Docker statuses 125, 126, and 127 remain distinct. GNU Make normally exits 2 for a
failed recipe; preserve the printed typed result and `result.env` rather than
assuming Make's outer status equals the internal classification status.

## Artifacts

Each profile writes under `build/platform-profiles/<profile>/`:

```text
image-build.log
image-identity.txt
image-result.env
docker.log
environment.log
packages.txt
dependency-check.log
strict-app-build.log
strict-test-build.log
complete-test-run.log
standards-core.log
result.env
```

Phases not reached have no log. The aggregate survey is:

```text
build/platform-profiles/survey-summary.tsv
```

The summary columns are:

```text
profile  role  provider_prepare  phase  outcome  reason  status
```

`provider_prepare` is `PASS` or `FAIL`. Provider-specific artifacts remain intact;
Docker therefore continues to write `image-build.log`, `image-identity.txt`, and
`image-result.env` in addition to the provider-neutral `prepare-result.env`. A
preparation failure preserves its provider-specific terminal phase; Docker image
failures therefore remain `image-build` results.

`build/` is ignored and cleanable. Durable conclusions are recorded in dated
reviews, not only in local logs.

## Current compatibility finding

Ubuntu Clang reaches the strict application build and rejects many first-party and
pinned SMC files because they lack a final newline:

```text
error: no newline at end of file [-Werror,-Wnewline-eof]
```

This is `FAIL-PRODUCT`, not a Clang crash or missing tool. No file was changed to
remediate it during the survey. The complete file-level diagnostics are preserved
in `build/platform-profiles/ubuntu-24.04-clang/strict-app-build.log`.

Ubuntu GCC, Fedora GCC, and Alpine musl GCC built the unchanged application and all
62 runners, ran the complete suite, and passed `standards-core`. This establishes
cross-Linux headless compatibility for those exact profiles while sharing the
Gentoo host kernel. It does not establish native display or release support.

## Harness regression coverage

`make test-platform-harness` deterministically covers:

- phase success and ordinary compile/test/standards failures;
- dependency-build versus image-tool failure;
- compiler crash recognition;
- missing compiler and Make;
- Docker 125/126/127 and timeout classification;
- contained typed statuses;
- required versus informational strict-check behavior;
- missing survey results;
- default and explicit Docker provider dispatch;
- injected Windows provider dispatch without a real VM;
- unsupported and not-yet-implemented provider failures;
- survey continuation after profile and provider-preparation failures;
- libvirt/KVM/domain/QGA-channel preflight classification;
- race-safe VM start ownership and initially-running preservation;
- readiness and graceful shutdown timeouts;
- QGA shutdown with ACPI fallback and no forced power-off;
- independent primary and cleanup outcomes;
- signal cleanup for a harness-owned start;
- bounded QGA command, process, upload, and download behavior;
- Windows 10/x64 and PowerShell identity validation;
- UCRT64 GCC/Make/linker/CMake/pkgconf validation;
- deterministic source allowlisting and payload-size enforcement;
- host/guest manifest comparison and stale run-ID rejection;
- exact-path guest workspace cleanup and cleanup-failure classification;
- pinned archive acquisition, size bounds, cache reuse, and checksum rejection;
- stable dependency-set identity and canonical Windows/MSYS paths;
- phased dependency build ordering and interruption-safe resume;
- missing versus corrupt dependency-set classification;
- atomic publication and newly-published-set rollback;
- concrete required-artifact and complete artifact-hash validation;
- mixed-result summary generation.

These tests use controlled fixtures rather than relying on the current Clang
incompatibility, so they remain valid after later remediation.

## Explicitly deferred

This Docker survey does not implement or claim:

- portability fixes found by a profile;
- Windows or macOS native compilation;
- Steam Deck/Gamescope/controller/touch testing;
- native X11 or Wayland presentation/input testing;
- Linux arm64 testing;
- installers, app bundles, signing, or clean-machine packaging;
- sanitizer or Valgrind execution in every profile;
- performance comparison across unlike environments.

Any remediation must be separately planned, must preserve current behavior, and
must rerun the profile that exposed the finding plus the existing broad gates.