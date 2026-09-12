# Platform Verification Profiles

This document defines what constitutes platform evidence. A profile is not marked
verified until its commands run on the named native environment or an explicitly
appropriate container.

## Common required evidence

Every supported profile must record:

- OS/distribution and release, kernel where applicable, architecture, compiler,
  linker, C library/runtime, SDL, SDL_mixer, enet, and cmocka versions;
- strict C11 application build with `-Wall -Wextra -Wpedantic -Werror`;
- complete `make test`, including SMC runners;
- `make standards`, or a typed tool failure under `VERIFICATION_POLICY.md`;
- `make smoke`;
- ASan/LeakSanitizer and UBSan where the compiler/runtime support them;
- `make benchmark-headless` and `make stability-headless`, with results treated as
  platform-specific rather than compared blindly across unlike hardware;
- native display startup, presentation, resize, keyboard input, and pointer checks
  where a graphical session is available.

Reproducible Linux survey mechanics and current compiler/libc results are defined in
[`PLATFORM_TESTING.md`](PLATFORM_TESTING.md). Survey completion means all configured
profiles were attempted and classified; it is not itself a support claim.

## Target profiles

| Profile | Appropriate execution | Additional evidence | Current state |
|---|---|---|---|
| Gentoo Linux | Native developer host | glibc/loader/tool compatibility and real display | Headless functional/benchmark/stability evidence available; native Valgrind remains diagnostic |
| Ubuntu Linux | Container for headless gates; native/VM for display | packaged GCC/Clang and SDL runtime | GCC complete headless profile passes; Clang strict app build is `FAIL-PRODUCT`; focused canonical Valgrind passes; display evidence deferred |
| Fedora Linux | Container for headless gates; native/VM for display | current GCC/glibc behavior | Fedora 43 GCC complete headless profile passes; display evidence deferred |
| Steam Deck / SteamOS | Native device or representative self-hosted SteamOS environment | Gamescope display, controller, touch/pointer, suspend/resume, constrained stability | Deferred: no target environment is available |
| Windows | Native Windows runner | Windows loader/filesystem/path behavior and native SDL presentation/input | Deferred: no native runner or verified dependency build is available |
| macOS | Native macOS runner on supported architecture | Apple toolchain/runtime and native SDL presentation/input | Deferred: no native runner or verified dependency build is available |

## Container boundary

Linux containers are valid evidence for headless distro/toolchain behavior. They are
not evidence for native Windows, macOS, Steam Deck Gamescope/controller behavior, or
host display/input behavior.

The canonical focused Valgrind image is defined under
`tools/valgrind-container/`. It pins Ubuntu by digest, Ubuntu package versions,
and exact SDL/SDL_mixer commits and archive hashes. Image construction owns the
dependency build and is separate from verification execution. `make leak` mounts
the project read-only, excludes host build/dependency artifacts, builds in tmpfs,
disables networking, and persists only logs/manifest output. This is valid focused
Ubuntu amd64 headless memory-safety evidence, not native display/input evidence or
a complete Ubuntu platform profile. Multi-architecture publication remains deferred
because Docker Buildx is not available on the current host.
See [`DOCKER_VALGRIND_GATE.md`](DOCKER_VALGRIND_GATE.md) for the complete verified
Gentoo host profile, kernel/cgroup requirements, image identity, trust boundary,
commands, and troubleshooting record.
The broader Linux compiler/libc survey uses the same security boundary and is
documented in [`PLATFORM_TESTING.md`](PLATFORM_TESTING.md).

## Display-backed boundary

Headless cell, layout, interaction, smoke, benchmark, and stability checks do not
prove native window presentation or application-edge pointer coordinate conversion.
Display-backed checks must identify the video backend, logical/window dimensions,
scale factor, input device, and whether the session is physical, virtual, or remote.
Reliable authored Button pointer activation remains explicitly deferred; keyboard
activation remains required.

## Failure reporting

Use the outcome taxonomy and investigation procedure in
[`VERIFICATION_POLICY.md`](VERIFICATION_POLICY.md). Missing native runners,
containers, display servers, controllers, or supported analysis tools are incomplete
platform evidence. They cannot be recorded as pass and do not imply product failure.