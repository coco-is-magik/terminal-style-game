# Windows 10 Platform Survey Plan — 2026-09-12

## Status

**Planned, not implemented.** This document is the accepted plan for the first
native Windows compatibility survey. No Windows survey infrastructure, VM
provisioning, dependency bootstrap, or product portability remediation has been
implemented as part of this planning increment.

Execution is currently blocked because the running Gentoo kernel does not provide
KVM. Do not proceed beyond planning until a kernel with the required KVM support is
booted and `/dev/kvm` is usable.

## Objective

Add a reproducible native Windows compatibility survey that runs the unchanged
project inside an externally maintained Windows 10 x64 virtual machine and
preserves the first trustworthy result as typed evidence.

The purpose is to discover whether the current project works in the selected
Windows environment. It is not to make the project compatible with Windows. A
valid survey result may therefore be:

- `PASS`;
- `FAIL-PRODUCT`;
- `FAIL-MISSING-TOOL`;
- `FAIL-TOOL`;
- `FAIL-TIMEOUT`.

A product failure is successful compatibility evidence collection, not a harness
failure. It remains nonzero and must not be converted into a pass.

## Accepted requirements and invariants

### Required behavior

- Use a genuine native Windows environment, not Wine or Linux execution.
- Begin with a normal Windows 10 x64 desktop VM.
- Run the unchanged project and unchanged complete 62-runner inventory.
- Preserve strict C11 warnings-as-errors.
- Record the exact observed OS, compiler, CRT, toolchain, and dependency identity
  on every run.
- Copy the current source into a fresh guest-local workspace and verify its
  integrity before building.
- Keep dependency bootstrap separate from routine survey execution.
- Preserve the first product failure independently from VM cleanup failures.
- Continue the broader platform survey after any classified Windows result.
- Introduce Windows as an informational profile.
- Check the VM state before acting: start it if stopped, use it if already running.
- Stop the VM only if the harness successfully started it.
- Leave an initially running VM running on every exit path.
- Use graceful shutdown only; do not automatically force power off.

### Forbidden behavior

- Do not modify `src/` or existing test sources.
- Do not omit, disable, or replace test runners.
- Do not weaken `-Werror` or suppress portability diagnostics.
- Do not add Windows product `#ifdef` branches or compatibility shims.
- Do not patch the Makefile or dependencies to force a pass.
- Do not use Linux-built dependencies in Windows.
- Do not label Wine, a cross-compile alone, or MSYS-linked programs as native
  Windows evidence.
- Do not install, update, activate, license, or provision Windows from the survey.
- Do not acquire or verify Windows installation media from the survey.
- Do not automatically reset, clone, replace, snapshot, or revert the VM.
- Do not expose the live repository writable to Windows.
- Do not commit VM disks, media, product keys, activation state, credentials, or
  environment-specific secrets.
- Do not shut down a VM that was already running.
- Do not restart a VM that stops externally during a survey.
- Do not force-stop a VM after a graceful shutdown timeout.
- Do not claim Windows 11 compatibility from Windows 10 evidence.
- Do not promote the profile to required without separate approval.

## Initial Windows target

The first profile represents a normal desktop Windows 10 x64 environment:

```text
Profile: windows-10-x64-gcc
Role: informational
OS family: Windows
Required major version: Windows 10
Architecture: AMD64/x86_64
Compiler: MinGW-w64 GCC
CRT: UCRT
Build shell: MSYS2
Build system: unchanged GNU Makefile
Provider: persistent local QEMU/libvirt VM
```

The VM should ordinarily be Windows 10 Pro 22H2 x64. Pro is a common normal-user
edition and is simpler to administer in a VM than Home. The survey must not depend
on Pro-only product functionality.

The profile name deliberately does not encode an exact Windows patch revision.
The VM may be updated externally, and each survey records the exact observed
edition, display version, build, and update revision.

### Windows 11 interpretation

A Windows 10 x64 pass is a useful forward-compatibility indicator for ordinary
Win32, UCRT, filesystem, loader, socket, and mainstream SDL behavior. It is not
native Windows 11 evidence. Documentation must state:

```text
Windows 10 x64: verified for the recorded environment
Windows 11 x64: likely compatible, not tested
```

Windows 11-specific display, DPI, graphics, input, security-policy, and SDL code
paths remain unverified.

### Deferred Windows targets

- Windows 7 SP1 x64 runtime compatibility;
- Windows 7 build-host compatibility, if later required;
- native Windows 11 execution;
- Windows ARM64;
- 32-bit Windows.

Windows 10 success must not be used as evidence for Windows 7.

## Provider decision

Use an ordinary persistent local VM managed through QEMU/libvirt. Prefer a per-user
connection:

```text
qemu:///session
```

This avoids requiring access to the system-wide libvirt connection. The provider
must be narrow enough that the guest command channel can change later without
changing survey semantics.

The Windows survey will not use:

- Dockur;
- hosted Windows CI;
- Wine;
- Docker wrapped around QEMU;
- per-run Windows installation;
- generated ISO download links;
- disposable VM overlays for every run;
- public RDP, SSH, WinRM, or noVNC services as a survey requirement;
- Samba access to the live repository.

QEMU Guest Agent is the planned initial command and file-transfer boundary. If it
proves inadequate with the real VM, another externally configured control channel
may replace it without altering the guest phase contract.

## Responsibility boundary

### Operator-owned VM administration

The operator creates the VM once, maintains it outside project automation, and
updates it when necessary. The project does not manage:

- Windows installation media or ISO downloads;
- license acceptance, product keys, or activation;
- Windows installation or edition selection;
- Windows Update;
- VM snapshots or backups;
- VM disk replacement;
- operating-system upgrades;
- routine toolchain updates;
- recovery of a damaged VM.

No proprietary Windows artifact or private VM state may enter Git or survey
artifacts.

### Project-owned survey behavior

Repository automation will own only:

- locating the configured VM;
- observing its initial power state;
- starting it when necessary;
- waiting for guest readiness;
- verifying the guest OS and architecture;
- verifying the expected compiler and dependency environment;
- transferring a fresh copy of the unchanged project;
- verifying source integrity;
- running ordered build and test phases;
- collecting logs and typed results;
- restoring the initial power state when the harness started the VM;
- preserving evidence on every failure path.

## Meaning of reproducibility

For this profile, reproducibility means:

> Given the same prepared VM and project revision, the harness performs the same
> ordered survey procedure and records sufficient observed identity to attribute
> and compare its result.

It does not require reconstructing Windows from scratch, producing a byte-identical
VM, hashing the complete VM disk before each run, pinning every cumulative update,
reverting to a snapshot before each survey, or reinstalling the toolchain and
dependencies on each invocation.

This is equivalent to maintaining a physical compatibility-test machine. If the
operator updates Windows, MSYS2, GCC, QEMU, or another relevant component, the next
survey records the new identity. Older logs remain evidence for the previous
environment.

## Host prerequisites and current blocker

The host-side runner will verify:

- `virsh` is installed;
- the configured libvirt URI is reachable;
- the configured domain exists;
- QEMU/KVM support is available;
- `/dev/kvm` exists and is usable;
- the host has sufficient bounded storage for logs and transfer staging;
- QEMU Guest Agent is configured for the domain;
- required local archive and hashing tools are available;
- output remains under the ignored project build directory.

The current host has an AMD CPU with virtualization capability and installed QEMU
and libvirt userspace, but the running Gentoo kernel reports:

```text
CONFIG_VIRTUALIZATION=y
# CONFIG_KVM is not set
```

and does not expose `/dev/kvm`. Practical Windows execution is blocked until a
kernel providing at least the following is booted:

```text
CONFIG_VIRTUALIZATION=y
CONFIG_KVM=y
CONFIG_KVM_AMD=y
```

The project harness will detect and classify this condition only. It must not
install, rebuild, reconfigure, or select a kernel. The offline QGA-based survey does
not inherently require TUN or `NET_ADMIN`.

## Profile configuration

Version-controlled profile metadata will identify the provider and compatibility
expectations without containing credentials or a private VM name:

```text
PROFILE_ID=windows-10-x64-gcc
PROFILE_ROLE=informational
PROFILE_PROVIDER=libvirt-windows
PROFILE_VM_URI=qemu:///session
PROFILE_OS_MAJOR=10
PROFILE_ARCHITECTURE=x86_64
PROFILE_COMPILER=gcc
PROFILE_CRT=ucrt
```

The actual VM name and other machine-local values should come from ignored local
configuration or environment variables. An optional operator-defined generation
label may be recorded, for example:

```text
PROFILE_VM_BASELINE=win10-main-2026-09
```

The label aids comparison but does not replace observed guest identity.

## VM power-state lifecycle

The initial VM power state is a hard invariant. The harness queries and records it
before taking any start or shutdown action.

| Initial state | Harness behavior |
|---|---|
| `running` | Use it and leave it running |
| `shut off` | Start it and stop it afterward |
| `in shutdown` | Wait for bounded completion, then reassess |
| `paused` | Do not resume; report a tool failure |
| `pmsuspended` | Do not wake; report a tool failure |
| `blocked` | Do not alter; report a tool failure |
| `crashed` | Do not restart; report a tool failure |
| unknown/unparseable | Do not alter; report a tool failure |

### Initially running VM

Record:

```text
VM_INITIAL_STATE=running
VM_STARTED_BY_HARNESS=false
```

The harness must not restart, shut down, suspend, resume, snapshot, or otherwise
change its final power state. This remains true after pass, product failure, tool
failure, timeout, or interruption.

### Initially stopped VM

After a successful harness-owned start, record:

```text
VM_INITIAL_STATE=shut_off
VM_STARTED_BY_HARNESS=true
```

The harness then:

1. Registers cleanup immediately.
2. Waits for guest readiness.
3. Runs the survey.
4. Requests graceful shutdown on every normal or trapped exit path.
5. Polls until the domain is confirmed shut off.
6. Records final state and cleanup outcome.

Cleanup applies after success, product failure, tool failure, guest timeout,
`SIGINT`, `SIGTERM`, and `SIGHUP`. It cannot guarantee cleanup after `SIGKILL`, host
power loss, kernel failure, or libvirt failure; those limitations must remain
documented.

### Race-safe ownership

Ownership is established only when the harness's `virsh start` request succeeds.
If the domain appeared shut off but another actor starts it before the harness:

1. The harness start request fails.
2. The harness queries state again.
3. If the VM is now running, it is treated as externally started.
4. `VM_STARTED_BY_HARNESS` remains false.
5. The VM is left running afterward.

This prevents the harness from stopping a VM started by someone else.

### Graceful shutdown

For a harness-started VM:

1. Request shutdown through QEMU Guest Agent.
2. Poll for `shut off` with a bounded timeout.
3. If the agent request fails, request ACPI shutdown.
4. Poll again with a bounded timeout.
5. If the VM remains active, report cleanup failure and require operator action.

The canonical automation must not call `virsh destroy` as an automatic fallback.
Forced power-off risks filesystem and VM-disk corruption.

### Stale prior runs

A stale prior-run record must never cause automatic shutdown of a VM found running
on a later invocation. The operator may now be using it. The new run treats the VM
as externally owned and records the stale lifecycle evidence.

If a VM found running stops externally during a survey, the harness records a VM
runtime failure and does not restart it.

## Primary and cleanup results

Lifecycle cleanup must not overwrite the compatibility finding. The result format
will preserve primary evidence:

```text
PRIMARY_OUTCOME
PRIMARY_PHASE
PRIMARY_REASON
PRIMARY_STATUS
```

and separate cleanup evidence:

```text
VM_INITIAL_STATE
VM_STARTED_BY_HARNESS
VM_FINAL_STATE
CLEANUP_OUTCOME
CLEANUP_REASON
CLEANUP_STATUS
```

An aggregate `OUTCOME`, `REASON`, and `STATUS` remain available for the survey and
check orchestration.

For example, a compilation failure followed by shutdown timeout preserves both:

```text
PRIMARY_OUTCOME=FAIL-PRODUCT
PRIMARY_PHASE=strict-app-build
PRIMARY_REASON=application-compile-failure
PRIMARY_STATUS=1

CLEANUP_OUTCOME=FAIL-TOOL
CLEANUP_REASON=vm-shutdown-timeout
CLEANUP_STATUS=3

OUTCOME=FAIL-TOOL
```

The aggregate is a tool failure because lifecycle cleanup did not complete, but
the product incompatibility remains recorded as the primary finding.

For an initially running VM:

```text
VM_INITIAL_STATE=running
VM_STARTED_BY_HARNESS=false
VM_FINAL_STATE=running
CLEANUP_OUTCOME=PASS
CLEANUP_REASON=vm-left-running-as-found
CLEANUP_STATUS=0
```

## Guest readiness

A running libvirt domain is not sufficient evidence that Windows is ready.
Readiness requires:

1. The domain remains active.
2. QEMU Guest Agent responds.
3. Guest command execution works.
4. Windows reports the expected OS family and architecture.
5. The survey workspace can be created.
6. The expected MSYS2/UCRT64 tools execute.

Readiness is bounded by a timeout. If an externally running VM stops during the
survey, the harness must not restart it.

## Environment identity

Every run records observed identity rather than assuming the persistent VM has not
changed.

### Host and provider identity

- profile ID, role, and provider;
- libvirt URI;
- domain name and UUID;
- optional operator baseline label;
- libvirt and QEMU versions;
- host architecture;
- initial and final domain state;
- whether the harness started the VM;
- unique run ID;
- start and completion timestamps.

### Windows identity

- product caption and edition;
- display version;
- complete OS version;
- build number and update build revision when available;
- architecture;
- computer name;
- installation type;
- system-drive filesystem;
- PowerShell version.

The profile rejects a Windows 11 guest, non-Windows guest, non-x64 guest, or
unidentifiable environment. Normal Windows 10 cumulative-update drift is recorded,
not automatically rejected.

### Toolchain identity

- MSYS2 runtime version;
- active `MSYSTEM`;
- GCC version and target triplet;
- linker/binutils version;
- GNU Make version;
- CMake version used for dependencies;
- pkgconf/pkg-config version;
- relevant installed package versions;
- effective `PATH` and C flags;
- dependency-set identity.

The profile must prove that compilation uses UCRT64 MinGW GCC rather than the MSYS
POSIX compiler.

## Toolchain and native output

The expected compiler environment is MSYS2 UCRT64 with MinGW-w64 GCC, GNU Make,
CMake for external dependency builds only, and pkgconf/pkg-config. The project
continues to use Make; CMake is not introduced as a project build system.

MSYS2 Bash may be the command shell, but application and test executables must be
native Windows programs. Representative output will be inspected for:

- PE32+ x86-64 format;
- native Windows/UCRT imports;
- expected dependency DLLs;
- absence of `msys-2.0.dll`;
- absence of Cygwin runtime DLLs;
- absence of accidental ELF or Linux artifacts.

Failure to establish native output invalidates the claimed profile and is a tool
failure, not a Windows product result.

## Dependency model

Windows-owned dependencies use the same accepted revisions and source archive
hashes as the Linux profiles:

- SDL;
- SDL_mixer;
- ENet;
- cmocka;
- SMC.

Linux `vendor/dist` artifacts must never enter the Windows workspace.

### One-time dependency bootstrap

Dependency construction is separate from routine surveys. A dedicated explicit
bootstrap operation may:

1. Verify the approved dependency source hashes.
2. Transfer dependency source to Windows.
3. Build dependencies with UCRT64.
4. Install them under a dedicated VM-owned survey path.
5. Write a dependency manifest.
6. Record complete build logs and a typed outcome.

Example location:

```text
C:\platform-survey\dependencies\<dependency-set-id>
```

The manifest records source revisions and hashes, compiler and target identity,
CRT, build options, produced files and hashes, build timestamp, and bootstrap
result. Routine surveys validate and consume the existing set; they do not rebuild
dependencies on every invocation.

Classifications include:

```text
FAIL-MISSING-TOOL phase=dependency-check reason=windows-dependencies-not-provisioned
FAIL-PRODUCT phase=dependency-build reason=dependency-build-failure
FAIL-TOOL phase=dependency-check reason=windows-dependency-manifest-mismatch
```

Infrastructure may arrange dependency artifacts to match the unchanged project's
declared `vendor/dist` layout. It must not patch product or linker behavior to make
the build pass.

## Source transfer and workspace

The live repository must not be exposed writable to Windows. Every survey:

1. Generates a unique run ID.
2. Removes stale host results for that run target.
3. Builds a fresh deterministic source payload.
4. Records a host source manifest.
5. Transfers the payload through the bounded VM channel.
6. Creates a fresh guest-local run directory.
7. Extracts the payload to guest-local NTFS.
8. Produces or verifies the guest source manifest.
9. Refuses to build if the manifests differ.

Exclude host-generated or incompatible content:

```text
.git/
.cache/
build/
vendor/
```

Include first-party source, all tests, the unchanged Makefile, required assets, the
guest survey runner, source manifest, expected dependency-set identity, and run ID.

Use a bounded workspace such as:

```text
C:\platform-survey\runs\<run-id>\source
```

The runner must validate the expected parent before deletion. Results from another
run ID are stale and must be rejected.

Successful runs may remove their guest workspace after evidence export. Failed
runs may retain it temporarily for diagnosis and record its path. Retention must be
bounded to avoid silently exhausting the VM disk. The persistent VM itself is never
reverted or deleted.

## Ordered survey phases

The profile stops at the first failed prerequisite or product phase while still
performing lifecycle cleanup when it owns the VM start:

1. `host-preflight`
2. `vm-locate`
3. `vm-state-check`
4. `vm-start-or-connect`
5. `guest-readiness`
6. `guest-identity`
7. `toolchain-check`
8. `dependency-check`
9. `source-transfer`
10. `source-integrity`
11. `strict-app-build`
12. `strict-test-build`
13. `complete-test-run`
14. `standards-core`
15. `native-binary-check`
16. `evidence-export`
17. `vm-power-state-restore`

The strict application build uses the unchanged project Makefile with C11,
`-Wall`, `-Wextra`, `-Wpedantic`, and `-Werror`.

If the application builds, `strict-test-build` builds exactly the existing
62-runner inventory without execution. If that succeeds, `complete-test-run` runs
the complete existing `make test`. No runner may be removed because it appears
POSIX-specific or is expected to fail. `standards-core` runs when its profile
prerequisites are present. Produced executables are then inspected for native
Windows identity.

## Expected first compatibility result

The unchanged Makefile contains Linux/ELF assumptions, including `lib64` layout and
an ELF runtime path using `$ORIGIN`. The first Windows attempt may fail during
application compilation or linking. If the compiler, dependency set, source
integrity, and guest identity have already been validated, this is a legitimate
product finding:

```text
FAIL-PRODUCT
phase=strict-app-build
reason=application-compile-failure
```

If reliable classification can distinguish the linker failure, the reason may be
`application-link-failure`. No remediation belongs in this increment.

## Windows outcome taxonomy

Representative missing-capability results:

```text
FAIL-MISSING-TOOL phase=host-preflight reason=kvm-unavailable
FAIL-MISSING-TOOL phase=vm-locate reason=windows-vm-not-configured
FAIL-MISSING-TOOL phase=toolchain-check reason=windows-toolchain-not-provisioned
FAIL-MISSING-TOOL phase=dependency-check reason=windows-dependencies-not-provisioned
```

Representative infrastructure results:

```text
FAIL-TOOL phase=host-preflight reason=libvirt-unavailable
FAIL-TOOL phase=vm-state-check reason=unsupported-vm-state
FAIL-TOOL phase=vm-start-or-connect reason=windows-vm-start-failure
FAIL-TOOL phase=guest-identity reason=windows-profile-mismatch
FAIL-TOOL phase=dependency-check reason=windows-dependency-manifest-mismatch
FAIL-TOOL phase=source-transfer reason=guest-source-transfer-failure
FAIL-TOOL phase=source-integrity reason=source-manifest-mismatch
FAIL-TOOL phase=native-binary-check reason=non-native-windows-binary
FAIL-TOOL phase=evidence-export reason=guest-result-missing
FAIL-TOOL phase=vm-power-state-restore reason=vm-shutdown-timeout
```

Representative product results:

```text
FAIL-PRODUCT phase=dependency-build reason=dependency-build-failure
FAIL-PRODUCT phase=strict-app-build reason=application-compile-failure
FAIL-PRODUCT phase=strict-test-build reason=test-compile-failure
FAIL-PRODUCT phase=complete-test-run reason=test-runner-failure
FAIL-PRODUCT phase=standards-core reason=standards-core-failure
```

Representative timeout results:

```text
FAIL-TIMEOUT phase=guest-readiness reason=windows-guest-readiness-timeout
FAIL-TIMEOUT phase=strict-app-build reason=build-timeout
FAIL-TIMEOUT phase=complete-test-run reason=execution-timeout
```

A shutdown timeout is cleanup `FAIL-TOOL` evidence even if its immediate cause was
a bounded wait. It must not erase the primary result.

## Integration with existing platform orchestration

The current survey assumes every profile is Docker-backed. Generalize this
surgically through provider dispatch while preserving existing behavior.

Existing Linux profiles gain or default to:

```text
PROFILE_PROVIDER=docker
```

The Windows profile uses:

```text
PROFILE_PROVIDER=libvirt-windows
```

Provider operations remain separate:

```text
Docker:
  prepare = existing Docker image build/check
  run = existing Docker profile runner

Windows:
  prepare = host, VM, guest, toolchain, and dependency preflight
  run = persistent VM survey runner
```

Generalize the summary's `image_build` concept to a provider-neutral preparation
result such as `provider_prepare`. Preserve profile, role, terminal phase, outcome,
reason, and status. Additional primary and cleanup columns may be appended without
losing existing fields.

`platform-survey` continues after every classified Windows or Linux result. Survey
success still means every configured profile was attempted and classified; it does
not mean compatibility. The Windows profile starts informational and therefore does
not affect `platform-check`.

## Planned artifacts

Use the existing ignored profile output root:

```text
build/platform-profiles/windows-10-x64-gcc/
```

Expected artifacts include:

```text
result.env
environment.log
host-environment.log
vm-lifecycle.log
guest-identity.log
toolchain.log
packages.txt
dependency-check.log
source-manifest-host.txt
source-manifest-guest.txt
source-transfer.log
strict-app-build.log
strict-test-build.log
complete-test-run.log
standards-core.log
native-binary-check.log
guest-runner.log
```

Phases not reached have no phase log. Durable compatibility conclusions belong in
a dated review, not only in ignored build artifacts.

## Deterministic harness regression tests

Tests will use controlled fake `virsh` and QGA boundaries and will not require a
real Windows VM.

### Provider and survey integration

- Existing Docker profiles retain their behavior.
- Windows does not call the Docker image builder.
- Survey continues after Windows failure.
- Informational Windows failure does not fail `platform-check`.
- Mixed Docker and Windows results produce a complete summary.
- Missing Windows results are incomplete evidence.

### Host and VM discovery

- Missing KVM and missing `virsh`.
- Unreachable libvirt connection.
- Missing or mismatched domain.
- Unsupported or unparseable initial state.

### Lifecycle ownership

- Initially running VM is never started or stopped.
- Initially stopped VM is started and stopped after pass.
- Initially stopped VM is stopped after product failure, tool failure, and timeout.
- Supported interruption signals trigger cleanup only for harness-owned starts.
- Start failure does not establish ownership.
- A concurrent external start does not give the harness ownership.
- A VM already stopped when cleanup begins is accepted.
- Paused, suspended, blocked, or crashed VMs are not altered.
- An externally running VM that stops is not restarted.
- Stale metadata never triggers automatic shutdown.
- Cleanup is idempotent.
- QGA shutdown succeeds in the normal case.
- QGA shutdown failure may fall back to ACPI.
- Shutdown timeout never invokes forced power-off.
- Shutdown failure preserves the primary compatibility result.

### Guest and toolchain validation

- QGA unavailable and guest readiness timeout.
- Wrong Windows major version or architecture.
- Missing UCRT64 GCC or Make.
- Accidental MSYS GCC selection.
- Wrong CRT.
- Missing dependency set or mismatched dependency manifest.

### Source and result integrity

- Fresh source transfer.
- Source transfer failure.
- Host/guest manifest mismatch.
- Stale result run ID rejection.
- Missing guest result.
- Evidence export failure preserves available logs.
- Cleanup targets only the bounded survey workspace.

### Product phases

- Application compilation failure and compiler crash.
- Application build timeout.
- Test-build failure.
- Test-runner failure and execution timeout.
- Standards-core failure.
- Non-native or MSYS-linked output rejection.
- Complete successful profile.

Existing classifier, survey, and check tests must be expanded rather than weakened
or replaced.

## Implementation sequence

### Increment 0: planning record

This document is Increment 0. It records the accepted plan before implementation.
No infrastructure or product behavior is changed. Stop here while KVM is
unavailable.

### Increment 1: provider-neutral orchestration

1. Add `PROFILE_PROVIDER` dispatch.
2. Preserve current Docker preparation and execution.
3. Generalize the survey summary carefully.
4. Update deterministic survey and check tests.

### Increment 2: libvirt lifecycle runner

1. Add host prerequisite checks.
2. Locate and validate the configured domain.
3. Implement state detection and race-safe startup ownership.
4. Implement readiness polling.
5. Implement graceful ownership-aware cleanup.
6. Preserve primary and cleanup outcomes independently.
7. Add fake-libvirt lifecycle tests.

### Increment 3: Windows guest runner

1. Add unique run IDs.
2. Collect guest identity.
3. Validate UCRT64 tools.
4. Transfer deterministic source payloads.
5. Manage bounded guest workspaces.
6. Verify source manifests.
7. Export typed guest results.
8. Add fake-QGA tests.

### Increment 4: Windows dependency bootstrap

1. Reuse accepted dependency revisions and hashes.
2. Build dependencies inside Windows with UCRT64.
3. Produce a persistent VM-owned dependency set.
4. Record artifacts, hashes, and build identity.
5. Preserve dependency build failures.
6. Keep bootstrap separate from routine surveys.

### Increment 5: product survey execution

1. Attempt the unchanged strict application build.
2. If successful, build all 62 runners.
3. If successful, run the complete test aggregate.
4. Run `standards-core`.
5. Inspect native binaries.
6. Preserve the earliest product incompatibility.
7. Restore VM power state according to ownership.

### Increment 6: integration and documentation

1. Add `windows-10-x64-gcc` as informational.
2. Add focused Make targets for preflight, dependency bootstrap, one-profile survey,
   and bounded workspace cleanup.
3. Include Windows in the broader survey only after deterministic harness paths
   pass.
4. Update operational platform documentation.
5. Add a dated review after the first actual Windows run.
6. Update stable README and status documentation last.

## Planned verification

After implementation and only when prerequisites are ready:

1. Run narrow Windows classifier tests.
2. Run fake-libvirt lifecycle tests.
3. Run fake-QGA guest-runner tests.
4. Run the complete platform-harness suite.
5. Re-run existing Linux harness tests.
6. Verify script syntax and documentation links.
7. Confirm no `src/` or test-source changes.
8. Re-run Linux profiles to detect provider-dispatch regressions.
9. Run Windows host preflight.
10. Run the explicit Windows dependency bootstrap.
11. Run the Windows 10 profile.
12. Run `make platform-survey`.
13. Run `make platform-check`.
14. Preserve every pass and failure without reinterpretation.

No C source changes are planned. If later work changes C, it must independently
satisfy strict `-Werror` compilation, regression tests, and sanitizer/leak checks.

## Deferred scope

This increment does not include:

- source portability remediation;
- Windows packaging, installers, or signing;
- Windows 7 provisioning or execution;
- Windows 11 execution;
- Windows ARM64 or 32-bit Windows;
- native display, keyboard, pointer, controller, touch, DPI, or multi-monitor
  acceptance;
- suspend/resume testing;
- cross-platform performance comparison;
- per-profile sanitizers;
- hosted CI or Dockur integration;
- VM image publication;
- fully automated VM provisioning.

## Risks and limitations

1. **KVM is unavailable in the running kernel.** Actual implementation and Windows
   execution remain blocked until this is corrected externally.
2. **Persistent VM drift is intentional.** Every run must record observed identity.
3. **QGA command and transfer behavior is unverified with the eventual VM.** Keep
   the provider boundary replaceable.
4. **The existing Makefile is likely Linux-specific.** An early Windows build or
   link failure is expected evidence, not a reason to patch during this increment.
5. **Shutdown cannot be guaranteed after untrappable host/process failure.** Stale
   metadata must never grant shutdown ownership.
6. **An initially running VM may be in operator use.** Never change its power state.
7. **Windows 10 maintenance is external.** The survey records but does not control
   update state.
8. **Windows 10 is not Windows 11 verification.** Forward compatibility remains an
   explicitly labeled inference.
9. **Windows 7 is materially different.** Its runtime and build-host requirements
   need a separate plan.

## Readiness and next safe action

Planning is complete and requirements are aligned. Implementation is **not ready to
start on the current host** because KVM is absent.

The next safe action is external to the repository:

1. Boot a Gentoo kernel with `CONFIG_KVM=y` and `CONFIG_KVM_AMD=y`.
2. Confirm `/dev/kvm` exists and is usable by the intended QEMU/libvirt session.
3. Create or identify the externally maintained Windows 10 x64 VM.
4. Confirm the VM is visible through the selected libvirt URI.
5. Confirm QEMU Guest Agent is installed and responsive.
6. Return to this plan before implementing Increment 1.

Do not begin provider, guest-runner, dependency, Makefile, source, or test changes
until those prerequisites are ready and implementation is separately authorized.

## Acceptance criteria

The eventual Windows survey infrastructure is complete when:

- an externally maintained Windows 10 x64 VM can be located;
- its initial power state is recorded;
- it is started only when initially stopped;
- it is stopped only when the harness started it;
- graceful shutdown failure is preserved without forced power-off;
- exact observed Windows and toolchain identity is recorded;
- a fresh unchanged source copy is verified inside the guest;
- Windows-owned pinned dependencies are validated;
- the existing application and all reachable test phases run in order;
- all 62 test runners remain in scope;
- native Windows output is distinguished from MSYS/Cygwin-linked output;
- the first product failure remains visible independently of cleanup failure;
- `platform-survey` continues after the Windows result;
- informational Windows evidence does not affect `platform-check`;
- existing Linux profiles and classifications remain unchanged;
- no product source or test source is modified;
- documentation distinguishes verified Windows 10 evidence from inferred Windows
  11 compatibility.
