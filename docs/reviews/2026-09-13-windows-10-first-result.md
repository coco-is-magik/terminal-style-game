# Windows 10 x64 UCRT64 First Compatibility Result — 2026-09-13

## Scope

This review records the first native execution of the unchanged project build/test
pipeline in the persistent Windows 10 UCRT64 VM. It is compatibility evidence, not
a portability-remediation review.

## Result

```text
PROFILE_ID=windows-10-x64-gcc
PROFILE_ROLE=informational
PRIMARY_OUTCOME=FAIL-PRODUCT
PRIMARY_PHASE=strict-app-build
PRIMARY_REASON=application-compile-failure
PRIMARY_STATUS=1
```

The provider wrapper's outer Make process returned 2 because the profile recipe
failed. The typed product status in `result.env` is 1 and is the compatibility
result.

## Validated prerequisites

- Windows 10 Pro 22H2, build 19045.4529, x86-64.
- PowerShell 5.1.19041.4522.
- MSYS2 UCRT64 with target `x86_64-w64-mingw32`.
- GCC 16.2.0, GNU ld 2.47.20260726, GNU Make 4.4.1, CMake 4.4.3,
  and pkgconf 3.0.7.
- Dependency set `win10-ucrt64-01cf4fc84d14ca0c`.
- All 2,464 dependency artifact hashes.
- All 397 transferred source path/SHA-256 entries.
- Exactly 62 test runners expanded from the unchanged Makefile.

The host and guest source manifest files have different raw hashes because the guest
uses Windows line endings. Parsing both manifests yields the same 397 path/digest
entries; this semantic comparison is the source-integrity contract.

## First product failure

The unchanged command retained the project's strict C11 warning policy:

```text
gcc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror ...
```

Compilation failed before linking. Representative incompatibilities were:

- implicit declarations of `fsync`, `dirfd`, and `fstatat`;
- unavailable `AT_SYMLINK_NOFOLLOW`;
- POSIX `mkdir(path, mode)` calls conflicting with UCRT's one-argument `mkdir`;
- unavailable POSIX locale types and operations including `locale_t`, `newlocale`,
  `uselocale`, `freelocale`, and `LC_NUMERIC_MASK`.

These are first-party portability findings and were correctly classified as
`FAIL-PRODUCT`. No flags, sources, tests, dependencies, Make targets, libraries, or
runtime paths were changed to avoid them.

Because strict application compilation failed, `test-build`, `test`,
`standards-core`, and native-binary inspection did not run. Their absence is correct
stop-on-first-failure behavior, not missing evidence.

## Lifecycle result

```text
VM_INITIAL_STATE=shut off
VM_STARTED_BY_HARNESS=true
VM_FINAL_STATE=shut off
CLEANUP_OUTCOME=PASS
CLEANUP_REASON=vm-shut-down-by-qga
CLEANUP_STATUS=0
```

No forced power-off was used. The guest run workspace was removed by the normal
bounded cleanup path.

## Preserved evidence hashes

```text
db6c478f375b774854d2871e4c56bbd904ff0b592499226527c759a310a5435a  result.env
78975c3ed28a1e669e5af2aa76d526c0d78b4404c573036c892f5456ace0e1a8  primary-result.env
d9d75a87e92eaba95927900c14981d85511ae3bc6ad32ec06402e395ef114a0e  strict-app-build.log
5ddef54b86b0b977ad95a30efe5ab1937541e31f29a2e2ec64815613ed531120  source-manifest-host.txt
57ae6d5b327942ee411f0d55e9440b4d216f7a4de52a4fa103b5bbae633e46cd  source-manifest-guest.txt
```

The ignored evidence remains under
`build/platform-profiles/windows-10-x64-gcc/` on the survey host.

## Disposition

Increment 5 is complete. The first classified result is preserved as observed.
Portability remediation requires a separate requirements and design increment; it
must not be folded into this survey result.