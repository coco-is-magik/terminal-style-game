# Docker and Canonical Valgrind Gate

This is the operational authority for the project's Docker-based Valgrind gate.
It records the verified Gentoo host behavior, kernel and cgroup prerequisites,
image identity, security boundary, commands, troubleshooting, and lessons learned
on 2026-09-12. Verification policy remains authoritative in
[`VERIFICATION_POLICY.md`](VERIFICATION_POLICY.md).

## Scope

`make leak` is the canonical focused leak gate. It builds and runs
`test-decal-io` and `test-core` inside a pinned Ubuntu 24.04 amd64 image, executes
each runner directly, and then executes each under Memcheck. `make leak-native`
is an optional host diagnostic and does not replace the canonical gate.

The image is headless Linux evidence only. It does not verify native display,
input, Windows, macOS, SteamOS/Gamescope, or non-amd64 behavior.

## Quick start

Docker Engine must already be installed, the daemon must be running, and the
invoking user must be authorized to access its socket.

```sh
docker version
docker info
docker run --rm \
  ubuntu@sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254 \
  /bin/true

make leak-image
make leak-image-self-test
make leak
```

The digest-pinned `/bin/true` command is the binding readiness probe. Package
presence and a responsive daemon are insufficient if runc cannot start a process.

Other targets:

```sh
make test-leak-classifier
make leak-native
```

## Verified host and image

The successful Gentoo host observation was:

```text
kernel:          7.2.2-gentoo #3, x86_64
Docker Engine:   29.5.2
containerd:      2.3.2
runc:            1.4.3
cgroup version:  2
cgroup driver:   cgroupfs
storage driver:  overlayfs (containerd snapshotter)
security:        seccomp builtin profile, cgroup namespaces
```

`/sys/fs/cgroup/cgroup.controllers` exposed:

```text
cpuset cpu io memory hugetlb pids rdma misc dmem
```

The minimal digest-pinned container exited 0, the image self-test passed, and the
canonical gate passed. These runtime observations are stronger readiness evidence
than inspecting a configuration file alone.

The image is based on:

```text
ubuntu@sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254
```

The first final verified local image was amd64 and used:

```text
GCC:       13.3.0
glibc:     2.39-0ubuntu8.9
Valgrind:  3.22.0
cmocka:    1.1.7-3
ENet:      1.3.17+ds-2build1
```

Exact Ubuntu package versions and SDL source pins are in
[`../tools/valgrind-container/dependencies.env`](../tools/valgrind-container/dependencies.env).
The Dockerfile duplicates these values as build arguments because classic
`docker build` does not consume this env file automatically.

## Kernel requirements

Docker uses the **host kernel**. The Ubuntu image does not supply or replace it.
Requirements vary with kernel version, Docker/runc version, cgroup mode, storage
driver, network features, security features, and Gentoo USE flags. Review the
installed Docker ebuild's `CONFIG_CHECK`, package post-install messages, and the
current Moby `contrib/check-config.sh` when changing any of those variables.

### Baseline for this project

For a modern cgroup-v2, bridge-networked, overlayfs Docker host, enable these in
the kernel (`=y` is simplest; some Moby checks also accept `=m`):

```text
CONFIG_NAMESPACES
CONFIG_NET_NS
CONFIG_PID_NS
CONFIG_IPC_NS
CONFIG_UTS_NS

CONFIG_CGROUPS
CONFIG_CGROUP_CPUACCT
CONFIG_CGROUP_DEVICE
CONFIG_CGROUP_FREEZER
CONFIG_CGROUP_SCHED
CONFIG_CPUSETS
CONFIG_MEMCG
CONFIG_CGROUP_BPF
CONFIG_BPF
CONFIG_BPF_SYSCALL

CONFIG_KEYS
CONFIG_POSIX_MQUEUE

CONFIG_VETH
CONFIG_BRIDGE
CONFIG_BRIDGE_NETFILTER

CONFIG_IP_NF_FILTER
CONFIG_IP_NF_MANGLE
CONFIG_IP_NF_TARGET_MASQUERADE
CONFIG_IP_NF_RAW
CONFIG_IP_NF_NAT
CONFIG_NF_NAT
CONFIG_IP6_NF_FILTER
CONFIG_IP6_NF_MANGLE
CONFIG_IP6_NF_TARGET_MASQUERADE
CONFIG_IP6_NF_RAW
CONFIG_IP6_NF_NAT
CONFIG_NETFILTER_XT_MATCH_ADDRTYPE
CONFIG_NETFILTER_XT_MATCH_CONNTRACK
CONFIG_NETFILTER_XT_MATCH_IPVS
CONFIG_NETFILTER_XT_MARK

CONFIG_OVERLAY_FS
```

This list follows the current Moby “Generally Necessary” set, selecting
overlayfs as the storage provider. `CONFIG_POSIX_MQUEUE` is needed for Docker's
`/dev/mqueue` bind mount. On old kernels, Moby additionally checks:

- `CONFIG_DEVPTS_MULTIPLE_INSTANCES` before 4.8;
- `CONFIG_NF_NAT_IPV4` through 5.1;
- `CONFIG_NF_NAT_NEEDED` through 5.2.

runc documents kernel 4.15 as the cgroup-v2 minimum and 5.2 or newer as
recommended. Kernels before 4.15 lack cgroup-v2 device-permission control unless
containers use user namespaces. This project was verified on kernel 7.2.2.

### cgroup v2 and BPF device filtering

On cgroup v2 there is no v1 `devices` controller file interface. OCI runtimes can
implement device allow/deny rules with cgroup-attached eBPF programs using the
`BPF_CGROUP_DEVICE` attachment type. The relevant modern configuration chain is:

```text
CONFIG_BPF=y
CONFIG_BPF_SYSCALL=y
CONFIG_CGROUP_BPF=y
CONFIG_CGROUP_DEVICE=y
```

`CONFIG_CGROUP_BPF` depends on `CONFIG_BPF_SYSCALL`. Moby's current checker treats
`CONFIG_CGROUP_BPF` as generally necessary on kernels 4.15 and newer. These are
therefore the recommended compatibility settings for a newly configured Gentoo
cgroup-v2 Docker kernel, even when a particular runtime path can operate without
exercising them.

The original host failure occurred before any container command ran:

```text
docker status: 125
OCI runtime create failed
error setting cgroup config for procHooks process
bpf_prog_query(BPF_CGROUP_DEVICE) failed: function not implemented
```

The then-running kernel configuration showed `CONFIG_BPF=y` and
`CONFIG_CGROUP_DEVICE=y`, but `CONFIG_BPF_SYSCALL` was unset and consequently
`CONFIG_CGROUP_BPF` was unavailable. The likely failure boundary was runc's
cgroup-v2 eBPF device query returning `ENOSYS`.

After the user rebuilt/rebooted the kernel, digest-pinned containers worked.
However, `/proc/config.gz` was unavailable and `/boot/config-7.2.2-gentoo` still
reported `CONFIG_BPF_SYSCALL` unset. That file therefore did not conclusively
describe the effective capability used by the successful runtime, or runc selected
a path that did not require the failed query. Do not erase this discrepancy:

- use `CONFIG_BPF_SYSCALL=y` and `CONFIG_CGROUP_BPF=y` for the robust modern
  cgroup-v2 configuration;
- verify which configuration actually produced the booted kernel;
- enable `CONFIG_IKCONFIG=y` and `CONFIG_IKCONFIG_PROC=y` if `/proc/config.gz`
  provenance is desired;
- always finish with a real digest-pinned `docker run`, not config inspection alone.

`CONFIG_BPF_JIT` is optional for this gate. It can improve BPF performance but was
not established as necessary for container startup.

### Optional and conditional kernel features

Enable these when their corresponding functionality or security policy is used:

| Feature | Kernel options | Notes |
|---|---|---|
| Seccomp filtering | `CONFIG_SECCOMP`, `CONFIG_SECCOMP_FILTER` | Optional in Moby's checker, strongly recommended; required to use Docker's seccomp profile. The verified host had both. Build Gentoo Docker/runc/containerd with their `seccomp` USE support as applicable. |
| User namespaces/rootless | `CONFIG_USER_NS` | Optional for rootful Docker; required by user-namespace/rootless designs. This project currently uses a rootful daemon and a non-root container process, not rootless Docker. |
| PID limits | `CONFIG_CGROUP_PIDS` | Recommended; the working cgroup-v2 hierarchy exposed `pids`. |
| Swap accounting | Modern kernels include cgroup-v2 swap accounting; old kernels may need `CONFIG_MEMCG_SWAP`, `CONFIG_MEMCG_SWAP_ENABLED`, and boot parameter `swapaccount=1` | Moby marks this optional and version-dependent. |
| CPU limits | `CONFIG_FAIR_GROUP_SCHED`, `CONFIG_CFS_BANDWIDTH`, optionally `CONFIG_RT_GROUP_SCHED` | Needed for corresponding CPU scheduling/limit controls, not for this gate's current command line. |
| I/O limits | `CONFIG_BLK_CGROUP`, `CONFIG_BLK_DEV_THROTTLING` | Needed for corresponding block-I/O controls. |
| Huge pages/perf | `CONFIG_CGROUP_HUGETLB`, `CONFIG_CGROUP_PERF` | Needed only for those controllers. |
| Cgroup network classifiers | `CONFIG_NET_CLS_CGROUP`, `CONFIG_CGROUP_NET_PRIO` | Optional legacy/network-policy functionality. |
| AppArmor | `CONFIG_SECURITY_APPARMOR` plus userspace parser and Docker support | Required only when AppArmor policy is enabled. |
| SELinux | `CONFIG_SECURITY_SELINUX` plus a coherent SELinux profile/userspace | Required only when SELinux is used. |
| nftables features | `CONFIG_NF_TABLES`, `CONFIG_NFT_CT`, `CONFIG_NFT_FIB`, `CONFIG_NFT_FIB_IPV4`, `CONFIG_NFT_FIB_IPV6`, `CONFIG_NFT_MASQ`, `CONFIG_NFT_NAT` | Conditional on firewall backend and desired networking. The verified Docker daemon reported an iptables backend. |
| Overlay networks | `CONFIG_VXLAN`, `CONFIG_BRIDGE_VLAN_FILTERING` | Docker swarm/overlay networking, not ordinary local bridge networking. Encrypted overlays additionally need the relevant crypto/XFRM/ESP options. |
| IPvlan/macvlan | `CONFIG_IPVLAN`; `CONFIG_MACVLAN`, `CONFIG_DUMMY` | Only for those network drivers. |
| FTP/TFTP NAT helpers | `CONFIG_NF_NAT_FTP`, `CONFIG_NF_CONNTRACK_FTP`, `CONFIG_NF_NAT_TFTP`, `CONFIG_NF_CONNTRACK_TFTP` | Only when those protocols must traverse container NAT. |
| btrfs storage | `CONFIG_BTRFS_FS`, `CONFIG_BTRFS_FS_POSIX_ACL` | Alternative to overlayfs, not additionally required. |
| ZFS storage | Working ZFS kernel/userspace stack | Alternative to overlayfs, not additionally required. |
| ext4 backing filesystem | `CONFIG_EXT4_FS`, `CONFIG_EXT4_FS_POSIX_ACL`, `CONFIG_EXT4_FS_SECURITY` | Conditional on the Docker backing filesystem. Equivalent ext3 checks apply if ext3 is used. |

Moby only requires at least one supported storage driver. This project observed
overlayfs, so `CONFIG_OVERLAY_FS` is the relevant choice; btrfs and ZFS are not
simultaneous requirements.

### Kernel command-line parameters

Do not add boot parameters merely because they appear in historical Docker
troubleshooting. The preferred modern configuration is a current kernel with
unified cgroup v2. Relevant parameters are:

| Parameter | When relevant | Status for this project |
|---|---|---|
| `systemd.unified_cgroup_hierarchy=1` | Enables unified cgroup v2 on systemd hosts that do not use it by default | Not needed on the verified host; cgroup v2 was already active. This Gentoo host uses OpenRC/elogind rather than systemd as its service authority. |
| `systemd.unified_cgroup_hierarchy=0` | Historical workaround forcing cgroup v1 for old Docker/systemd combinations | Not recommended for this gate; cgroup v1 is deprecated and modern runc supports v2. |
| `SYSTEMD_CGROUP_ENABLE_LEGACY_FORCE=1` | Historical systemd v256+ force-enable for legacy cgroup v1, used with the previous setting | Legacy only; systemd v258 deprecates/removes this route. |
| `systemd.legacy_systemd_cgroup_controller=yes` | Historical hybrid/legacy systemd-controller workaround | Legacy only and not applicable to the verified OpenRC host. |
| `swapaccount=1` | Older kernels where configured memory-cgroup swap accounting is not enabled by default | Not needed for the verified kernel 7.2.2 gate. |
| `vsyscall=emulate` or `vsyscall=native` | Compatibility with very old containers using eglibc ≤2.13 when `CONFIG_LEGACY_VSYSCALL_NONE` is selected | Not needed by Ubuntu 24.04; `native` weakens ASLR and should not be enabled casually. |

Kernel configuration symbols (`CONFIG_*`) are build-time settings, not kernel
command-line parameters. Changing either requires the normal administrator-owned
kernel/boot workflow and, for build-time settings, rebuilding and booting the new
kernel. Project verification never performs those system changes.

## Checking a Gentoo host

### Static inspection

Locate the configuration corresponding to the **running** kernel:

```sh
uname -r

if test -r /proc/config.gz; then
    zgrep -E 'CONFIG_(NAMESPACES|CGROUPS|CGROUP_DEVICE|CGROUP_BPF|BPF_SYSCALL|SECCOMP|SECCOMP_FILTER|OVERLAY_FS)=' /proc/config.gz
else
    grep -E 'CONFIG_(NAMESPACES|CGROUPS|CGROUP_DEVICE|CGROUP_BPF|BPF_SYSCALL|SECCOMP|SECCOMP_FILTER|OVERLAY_FS)=' "/boot/config-$(uname -r)"
fi
```

If `/boot/config-$(uname -r)` is copied manually or kernel release names are
reused, verify its provenance; matching filenames alone do not prove it generated
the running image.

The upstream Moby checker is the broadest current check. Review it before running
downloaded code, then invoke it against the known config file. Gentoo also advises
checking the installed ebuild's `CONFIG_CHECK` and emerge post-install messages.

### Runtime inspection

```sh
docker version
docker info
mount | grep cgroup
cat /sys/fs/cgroup/cgroup.controllers
docker run --rm \
  ubuntu@sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254 \
  /bin/true
```

For this project, success requires the final command to return status 0. A pulled
image, working client, reachable daemon, and plausible kernel config are only
partial evidence.

## Gentoo userspace requirements

The verified providers were:

```text
app-containers/docker-29.5.2
app-containers/docker-cli-29.5.2
app-containers/containerd-2.3.2-r1
app-containers/runc-1.4.3
```

Observed Gentoo features included Docker `overlay2`, `seccomp`, and
`container-init`, plus seccomp support in runc and containerd. Package names and
USE requirements can change; inspect the current Gentoo repository metadata and
installed package state rather than copying these historical versions blindly.

Docker's daemon must be running before the project targets can use it. This
project does not start, restart, enable, or reconfigure the daemon. Follow the
host's init-system policy; do not apply systemd commands to an OpenRC host.

Access to `/run/docker.sock` is privileged. Official Docker documentation warns
that membership in the `docker` group grants root-level control through the
daemon. The project runs the **container process** as the caller's UID/GID and
drops capabilities, but that does not make possession of the Docker socket
unprivileged. Rootless Docker is a separate host architecture and was not verified.

## Image construction and reproducibility

`make leak-image` uses ordinary `docker build` because Buildx is unavailable on
the verified host. It builds `terminal-style-game-valgrind:ubuntu24.04-amd64`.
The Dockerfile pins:

- Ubuntu by immutable digest;
- direct Ubuntu package versions;
- exact SDL and SDL_mixer commits;
- SHA-256 for both downloaded source archives.

Image construction requires network access for Ubuntu package indexes/packages and
the two HTTPS source archives. It verifies archive hashes before extraction. The
result still depends on the continued availability and integrity of the configured
Ubuntu repository metadata and package files. The local image tag is mutable; use
`docker image inspect` and the generated image manifest when recording evidence.

The verified SDL build is intentionally headless and uses
`SDL_UNIX_CONSOLE_BUILD=ON`. SDL_mixer's unneeded optional codecs, examples, and
tests are disabled. The image provides a dependency prefix at
`/opt/valgrind-deps` and records its resolved toolchain manifest there.

Multi-architecture image construction/publication is unverified and deferred.
The current image is amd64 only.

## Verification execution boundary

`tools/valgrind-container/docker-gate.sh` runs the image with:

- `--network none`;
- `--cap-drop ALL`;
- `--security-opt no-new-privileges`;
- the invoking UID and GID;
- the source directory's group as a supplementary group;
- a read-only root filesystem;
- noexec `/tmp` tmpfs;
- executable `/work` tmpfs;
- the project mounted read-only at `/source`;
- only `build/valgrind-container/` mounted writable at `/output`.

The supplementary group is necessary on hosts where the project directory is
group-traversable but not world-traversable. Without it, the observed failure was:

```text
/source/Makefile: Permission denied
FAIL-TOOL: reason=container-mount-contract
```

The entrypoint copies tracked/required project content to `/work` while excluding:

```text
.git
.cache
build
vendor/dist
vendor/src/SDL
```

It retains `vendor/src/smc`, which the Makefile needs. It then links only the
image-owned dependency prefix and SDL source headers into the workspace. This is
why `test-decal-io`, which writes under `tests/`, can run while `/source` remains
read-only.

## Gate sequence and Memcheck policy

For each runner, the container:

1. Builds it from the copied source with the project's strict C11 flags.
2. Runs it directly to distinguish ordinary test failure from Valgrind failure.
3. Runs Memcheck with:

```text
--error-exitcode=100
--leak-check=full
--show-leak-kinds=all
--errors-for-leak-kinds=definite,indirect,possible
```

Definite, indirect, and possible leaks fail the product gate. Reachable blocks are
reported but are not independently selected as errors by this command.

The container has a 1,800-second outer timeout. Native Memcheck gives each runner
900 seconds. Timeout statuses are typed as incomplete evidence, not product pass or
failure.

## Outcome classification

Stable reasons include:

```text
FAIL-MISSING-TOOL: reason=docker-not-found
FAIL-MISSING-TOOL: reason=valgrind-not-found
FAIL-TOOL: reason=docker-daemon-or-runtime status=125
FAIL-TOOL: reason=container-command-not-executable status=126
FAIL-TOOL: reason=container-command-not-found status=127
FAIL-TOOL: reason=unsupported-client-instruction
FAIL-TOOL: reason=valgrind-crash
FAIL-PRODUCT: reason=direct-runner-failure
FAIL-PRODUCT: reason=memcheck-findings
FAIL-TIMEOUT: reason=container-timeout
FAIL-TIMEOUT: reason=memcheck-timeout
```

Contained typed statuses 1–4 are preserved at the Docker boundary. GNU Make
normally exits 2 when a recipe fails, so automation should preserve the printed
typed diagnostic and artifact log rather than expecting Make's process status to
equal the internal classifier status.

## Artifacts

Canonical output is under `build/valgrind-container/`:

```text
build.log
docker.log
direct-test-decal-io.log
direct-test-core.log
valgrind-test-decal-io.log
valgrind-test-core.log
image-manifest.txt
```

`build/` is ignored and cleanable. Durable conclusions belong in documentation;
logs are supporting local artifacts, not permanent authority.

## Problems encountered and resolutions

### runc could not configure cgroup-v2 devices

Attempt: run a minimal Ubuntu container on the original Gentoo kernel.

Result: Docker status 125 before client code.

Failure mode: `bpf_prog_query(BPF_CGROUP_DEVICE)` returned “function not
implemented”.

Resolution: user rebuilt/rebooted the kernel; afterward minimal containers ran.
Recommended robust configuration includes `BPF_SYSCALL` and `CGROUP_BPF`; retain
the config-file/runtime discrepancy described above.

### SDL rejected a headless Unix build

Attempt: build SDL without X11 or Wayland development libraries.

Result: CMake rejected a Unix build unable to create normal desktop windows.

Resolution: use the upstream-supported `SDL_UNIX_CONSOLE_BUILD=ON`, appropriate
because this gate is headless.

### SDL_mixer could not discover SDL

Attempt: install SDL into a non-default `lib64` prefix and rely on prefix discovery.

Result: `SDL3Config.cmake` was initially not installed/discovered.

Resolution: make SDL's standalone/install intent explicit, assert the package file
exists, and pass its exact `SDL3_DIR` to SDL_mixer.

### Ubuntu cmocka header assumption

Attempt: copy three cmocka headers based on the host vendor layout.

Result: Ubuntu's package did not provide `cmocka_version.h` at that path.

Resolution: copy only `<cmocka.h>`, the header the project actually uses.

### Self-test appeared to fail after clean Memcheck output

Attempt: `return puts("valgrind-image-self-test");`.

Result: Valgrind reported zero errors and no leaks but propagated `puts()`'s
nonnegative, nonzero success value (25), which the wrapper initially saw as failure.

Resolution: check `puts() == EOF`, then explicitly return 0 on success. Never assume
stdio success returns zero unless the API contract says so.

### Read-only source was inaccessible to the mapped identity

Attempt: use only `--user UID:GID` on a group-restricted project directory.

Result: Docker dropped supplementary groups and `/source/Makefile` was unreadable.

Resolution: add the source directory's group with `--group-add`; do not weaken host
permissions or run the verification process as root.

### Native Valgrind cannot execute the Gentoo loader

The host glibc 2.43-r2 loader contains an AVX-512 instruction in `_dl_start` that
Valgrind 3.27.1/LibVEX does not recognize. Native Memcheck exits 132/SIGILL with
zero allocations before project code. This remains:

```text
FAIL-TOOL: reason=unsupported-client-instruction status=132
```

The Ubuntu image's glibc 2.39 and Valgrind 3.22 combination passed the minimal
dynamic-program self-test and both project runners. The container is the approved
canonical corrective action; the native result remains useful diagnostic evidence.

## Known limitations and next steps

- The leak scope remains two focused historical runners, not all project runners.
- The image and evidence are amd64 only; Buildx/multi-architecture publication is
  deferred.
- A rootful Docker daemon and access to its privileged socket remain part of the
  host trust boundary.
- Exact image byte identity can change if the image is rebuilt after repository
  package availability changes, even though direct package versions are pinned.
- `/boot` kernel-config provenance was not conclusively reconciled with the working
  cgroup-v2 runtime. Future kernel work should retain unique release identifiers or
  expose `/proc/config.gz`.
- cppcheck works and separately reports existing `src/scene_format.c` product
  findings. Those findings do not change Docker or Valgrind evidence.

## External authorities consulted

- Gentoo Wiki, Docker: kernel configuration, USE flags, OpenRC/systemd guidance,
  storage/network setup, and historical troubleshooting.
- Moby `contrib/check-config.sh`: current generally necessary, optional,
  version-conditional, storage, network, and limit checks.
- runc `docs/cgroup-v2.md`: cgroup-v2 support, kernel versions, device-permission
  boundary, systemd-driver recommendation, and rootless delegation.
- Docker Engine Linux post-install documentation: daemon socket ownership and the
  warning that the `docker` group grants root-level privileges.

These upstream sources evolve. This document records both their 2026-09-12 guidance
and the project's observed runtime evidence; re-check them during Docker, runc,
kernel, cgroup, storage-driver, firewall, or init-system upgrades.