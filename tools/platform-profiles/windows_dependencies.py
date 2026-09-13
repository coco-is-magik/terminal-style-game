#!/usr/bin/env python3
"""Acquire, build, and validate the pinned Windows dependency set."""

from __future__ import annotations

import hashlib
import json
import os
import sys
import time
import urllib.request
from dataclasses import dataclass
from pathlib import Path

from windows_qga import QgaClient, QgaError
from windows_result import SurveyFailure, write_primary


MAX_ARCHIVE_BYTES = 128 * 1024 * 1024
MANIFEST_SCHEMA = 2
BUILD_OPTIONS = {
    "SDL": [
        "SDL3_MAINPROJECT=ON",
        "SDL_INSTALL=ON",
        "SDL_TESTS=OFF",
        "SDL_EXAMPLES=OFF",
        "SDL_SHARED=ON",
        "SDL_STATIC=OFF",
        "SDL_OPENGLES=OFF",
    ],
    "SDL_mixer": [
        "SDLMIXER_VENDORED=OFF",
        "SDLMIXER_EXAMPLES=OFF",
        "SDLMIXER_TESTS=OFF",
        "SDLMIXER_FLAC=OFF",
        "SDLMIXER_GME=OFF",
        "SDLMIXER_MOD=OFF",
        "SDLMIXER_MP3=OFF",
        "SDLMIXER_MIDI=OFF",
        "SDLMIXER_OPUS=OFF",
        "SDLMIXER_WAVPACK=OFF",
    ],
    "enet": ["ENET_BUILD_TESTS=OFF"],
    "cmocka": ["WITH_EXAMPLES=OFF", "UNIT_TESTING=OFF"],
    "smc": ["SOURCE_ONLY"],
}
REQUIRED_ARTIFACTS = (
    "include/SDL3/SDL.h",
    "include/SDL3_mixer/SDL_mixer.h",
    "include/enet/enet.h",
    "include/cmocka.h",
    "lib64/libSDL3.dll.a",
    "lib64/libSDL3_mixer.dll.a",
    "lib64/libenet.a",
    "lib64/libcmocka.dll.a",
    "bin/SDL3.dll",
    "bin/SDL3_mixer.dll",
    "bin/cmocka.dll",
    "source/SDL/include/SDL3/SDL.h",
    "source/smc/Makefile",
)


@dataclass(frozen=True)
class Dependency:
    name: str
    commit: str
    sha256: str
    url: str

    @property
    def archive_name(self) -> str:
        return f"{self.name}.tar.gz"


def load_dependencies(root: Path) -> list[Dependency]:
    values: dict[str, str] = {}
    path = root / "tools/platform-profiles/dependencies.env"
    for line in path.read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            key, value = line.split("=", 1)
            values[key] = value
    return [
        Dependency("SDL", values["SDL_COMMIT"], values["SDL_SHA256"],
                   f"https://github.com/libsdl-org/SDL/archive/{values['SDL_COMMIT']}.tar.gz"),
        Dependency("SDL_mixer", values["SDL_MIXER_COMMIT"], values["SDL_MIXER_SHA256"],
                   f"https://github.com/libsdl-org/SDL_mixer/archive/{values['SDL_MIXER_COMMIT']}.tar.gz"),
        Dependency("enet", values["ENET_COMMIT"], values["ENET_SHA256"],
                   f"https://github.com/lsalzman/enet/archive/{values['ENET_COMMIT']}.tar.gz"),
        Dependency("cmocka", values["CMOCKA_COMMIT"], values["CMOCKA_SHA256"],
                   f"https://gitlab.com/cmocka/cmocka/-/archive/{values['CMOCKA_COMMIT']}/cmocka-{values['CMOCKA_COMMIT']}.tar.gz"),
        Dependency("smc", values["SMC_COMMIT"], values["SMC_SHA256"],
                   f"https://github.com/coco-is-magik/self-modifying-calculator/archive/{values['SMC_COMMIT']}.tar.gz"),
    ]


def dependency_set_id(dependencies: list[Dependency]) -> str:
    identity = f"schema:{MANIFEST_SCHEMA}\n" + "\n".join(
        f"{item.name}:{item.commit}:{item.sha256}" for item in dependencies
    ) + "\n" + json.dumps(BUILD_OPTIONS, sort_keys=True, separators=(",", ":"))
    identity = identity.encode("ascii")
    return "win10-ucrt64-" + hashlib.sha256(identity).hexdigest()[:16]


def windows_to_msys(path: str) -> str:
    if not path.startswith("C:\\"):
        raise ValueError("only canonical C drive paths are supported")
    return "/c/" + path[3:].replace("\\", "/")


def acquire_archives(dependencies: list[Dependency], destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    for dependency in dependencies:
        archive = destination / dependency.archive_name
        if archive.is_file() and file_sha256(archive) == dependency.sha256:
            continue
        archive.unlink(missing_ok=True)
        temporary = archive.with_suffix(archive.suffix + ".partial")
        temporary.unlink(missing_ok=True)
        request = urllib.request.Request(
            dependency.url, headers={"User-Agent": "terminal-style-game-platform-survey/1"}
        )
        try:
            with urllib.request.urlopen(request, timeout=30) as response, temporary.open("wb") as output:
                total = 0
                while True:
                    chunk = response.read(1024 * 1024)
                    if not chunk:
                        break
                    total += len(chunk)
                    if total > MAX_ARCHIVE_BYTES:
                        raise ValueError(f"archive exceeds size limit: {dependency.name}")
                    output.write(chunk)
        except Exception:
            temporary.unlink(missing_ok=True)
            raise
        if file_sha256(temporary) != dependency.sha256:
            temporary.unlink(missing_ok=True)
            raise ValueError(f"archive checksum mismatch: {dependency.name}")
        temporary.replace(archive)
    manifest = {
        "dependency_set_id": dependency_set_id(dependencies),
        "archives": [
            {
                "name": item.name,
                "commit": item.commit,
                "sha256": item.sha256,
                "url": item.url,
                "size": (destination / item.archive_name).stat().st_size,
            }
            for item in dependencies
        ],
    }
    (destination / "archive-manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


class DependencyBootstrap:
    def __init__(
        self,
        client: QgaClient,
        dependencies: list[Dependency],
        archives: Path,
        output: Path,
    ) -> None:
        self.client = client
        self.dependencies = dependencies
        self.archives = archives
        self.output = output
        self.set_id = dependency_set_id(dependencies)
        self.run_id = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
        self.staging = rf"C:\platform-survey\dependency-builds\{self.set_id}-staging"
        self.final = rf"C:\platform-survey\dependencies\{self.set_id}"
        self.staging_created = False
        self.published_by_run = False

    def run(self) -> None:
        for dependency in self.dependencies:
            archive = self.archives / dependency.archive_name
            if not archive.is_file():
                raise SurveyFailure("FAIL-MISSING-TOOL", "dependency-build", "dependency-archive-missing", 2)
            if file_sha256(archive) != dependency.sha256:
                raise SurveyFailure("FAIL-TOOL", "dependency-build", "dependency-archive-checksum-mismatch", 3)
        if validate_dependency_set(self.client, self.dependencies, self.output, required=False):
            return
        self._create_staging()
        failure: Exception | None = None
        try:
            self._transfer_archives()
            self._build()
            self._publish()
            validate_dependency_set(self.client, self.dependencies, self.output, required=True)
        except Exception as error:
            failure = error
            if self.published_by_run:
                try:
                    self._remove_published_set()
                except Exception:
                    pass
        finally:
            try:
                self._cleanup_staging()
            except Exception as error:
                if failure is None:
                    failure = error
        if failure is not None:
            raise failure

    def _create_staging(self) -> None:
        script = (
            f"$p='{self.staging}';$e='{self.staging}';if($p -cne $e){{exit 41}};"
            "New-Item -ItemType Directory -Path ($p+'\\archives') -Force -ErrorAction Stop|Out-Null"
        )
        self._powershell(script, "dependency-staging-create-failure")
        self.staging_created = True

    def _transfer_archives(self) -> None:
        for dependency in self.dependencies:
            self.transfer_archive(dependency)

    def transfer_archive(self, dependency: Dependency) -> None:
        archive = self.archives / dependency.archive_name
        if not archive.is_file():
            raise SurveyFailure("FAIL-MISSING-TOOL", "dependency-build", "dependency-archive-missing", 2)
        if file_sha256(archive) != dependency.sha256:
            raise SurveyFailure("FAIL-TOOL", "dependency-build", "dependency-archive-checksum-mismatch", 3)
        try:
            self.client.put_file(
                archive,
                self.staging + rf"\archives\{dependency.archive_name}",
            )
        except QgaError as error:
            raise SurveyFailure("FAIL-TOOL", "dependency-build", "dependency-transfer-failure", 3) from error

    def prepare_staging(self) -> None:
        script = (
            f"$p='{self.staging}';$e='{self.staging}';if($p -cne $e){{exit 41}};"
            "if(Test-Path -LiteralPath $p){Remove-Item -LiteralPath $p -Recurse -Force -ErrorAction Stop};"
            "New-Item -ItemType Directory -Path ($p+'\\archives') -Force -ErrorAction Stop|Out-Null"
        )
        self._powershell(script, "dependency-staging-create-failure")

    def extract_archives(self) -> None:
        posix_stage = windows_to_msys(self.staging)
        script = f"""set -eu
stage='{posix_stage}'
rm -rf "$stage/src" "$stage/build" "$stage/install"
mkdir -p "$stage/src" "$stage/build" "$stage/install/source"
extract() {{ mkdir "$stage/src/$1"; tar -xzf "$stage/archives/$1.tar.gz" --exclude='*/Xcode/*' --strip-components=1 -C "$stage/src/$1"; }}
for dep in SDL SDL_mixer enet cmocka smc; do extract "$dep"; done
"""
        self._run_bash_step("extract", script, 300)

    def build_dependency(self, name: str) -> None:
        posix_stage = windows_to_msys(self.staging)
        common = f"""set -eu
export MSYSTEM=UCRT64
export PATH=/ucrt64/bin:/usr/bin
stage='{posix_stage}'
prefix="$stage/install"
"""
        scripts = {
            "SDL": """cmake -S "$stage/src/SDL" -B "$stage/build/SDL" -G 'MSYS Makefiles' -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib64 -DSDL3_MAINPROJECT=ON -DSDL_INSTALL=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_OPENGLES=OFF
cmake --build "$stage/build/SDL" --parallel 2
cmake --install "$stage/build/SDL"
""",
            "SDL_mixer": """cmake -S "$stage/src/SDL_mixer" -B "$stage/build/SDL_mixer" -G 'MSYS Makefiles' -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib64 -DSDL3_DIR="$prefix/lib64/cmake/SDL3" -DSDLMIXER_VENDORED=OFF -DSDLMIXER_EXAMPLES=OFF -DSDLMIXER_TESTS=OFF -DSDLMIXER_FLAC=OFF -DSDLMIXER_GME=OFF -DSDLMIXER_MOD=OFF -DSDLMIXER_MP3=OFF -DSDLMIXER_MIDI=OFF -DSDLMIXER_OPUS=OFF -DSDLMIXER_WAVPACK=OFF
cmake --build "$stage/build/SDL_mixer" --parallel 2
cmake --install "$stage/build/SDL_mixer"
""",
            "enet": """cmake -S "$stage/src/enet" -B "$stage/build/enet" -G 'MSYS Makefiles' -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib64 -DENET_BUILD_TESTS=OFF
cmake --build "$stage/build/enet" --parallel 2
cmake --install "$stage/build/enet"
""",
            "cmocka": """cmake -S "$stage/src/cmocka" -B "$stage/build/cmocka" -G 'MSYS Makefiles' -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib64 -DWITH_EXAMPLES=OFF -DUNIT_TESTING=OFF
cmake --build "$stage/build/cmocka" --parallel 2
cmake --install "$stage/build/cmocka"
""",
        }
        if name not in scripts:
            raise ValueError(f"unsupported dependency build step: {name}")
        self._run_bash_step(name, common + scripts[name], 900)

    def copy_sources(self) -> None:
        posix_stage = windows_to_msys(self.staging)
        script = f"""set -eu
stage='{posix_stage}'
rm -rf "$stage/install/source/SDL" "$stage/install/source/smc"
cp -R "$stage/src/SDL" "$stage/install/source/SDL"
cp -R "$stage/src/smc" "$stage/install/source/smc"
test -f "$stage/install/include/SDL3/SDL.h"
test -f "$stage/install/include/cmocka.h"
test -f "$stage/install/include/enet/enet.h"
test -d "$stage/install/source/smc"
"""
        self._run_bash_step("sources", script, 300)

    def write_manifest(self) -> None:
        metadata = {item.name: {"commit": item.commit, "sha256": item.sha256} for item in self.dependencies}
        self._write_manifest(metadata, windows_to_msys(self.final))

    def publish(self) -> None:
        self._publish()
        try:
            validate_dependency_set(
                self.client, self.dependencies, self.output, required=True
            )
        except Exception:
            if self.published_by_run:
                self._remove_published_set()
            raise

    def cleanup_staging(self) -> None:
        self.staging_created = True
        self._cleanup_staging()

    def _run_bash_step(self, name: str, script: str, timeout: int) -> None:
        code, stdout, stderr = self.client.exec(
            r"C:\msys64\usr\bin\bash.exe", ["-lc", script], timeout
        )
        (self.output / f"dependency-{name}.log").write_bytes(stdout + stderr)
        if code != 0:
            raise SurveyFailure("FAIL-PRODUCT", "dependency-build", "dependency-build-failure", 1)

    def _build(self) -> None:
        bash = r"C:\msys64\usr\bin\bash.exe"
        posix_stage = windows_to_msys(self.staging)
        posix_final = windows_to_msys(self.final)
        metadata = {item.name: {"commit": item.commit, "sha256": item.sha256} for item in self.dependencies}
        script = f"""set -eu
export MSYSTEM=UCRT64
export PATH=/ucrt64/bin:/usr/bin
stage='{posix_stage}'
prefix="$stage/install"
mkdir -p "$stage/src" "$stage/build" "$prefix/source"
extract() {{ mkdir "$stage/src/$1"; tar -xzf "$stage/archives/$1.tar.gz" --exclude='*/Xcode/*' --strip-components=1 -C "$stage/src/$1"; }}
for dep in SDL SDL_mixer enet cmocka smc; do extract "$dep"; done
cmake -S "$stage/src/SDL" -B "$stage/build/SDL" -G 'MSYS Makefiles' -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib64 -DSDL3_MAINPROJECT=ON -DSDL_INSTALL=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_SHARED=ON -DSDL_STATIC=OFF
cmake --build "$stage/build/SDL" --parallel 2
cmake --install "$stage/build/SDL"
cmake -S "$stage/src/SDL_mixer" -B "$stage/build/SDL_mixer" -G 'MSYS Makefiles' -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib64 -DSDL3_DIR="$prefix/lib64/cmake/SDL3" -DSDLMIXER_VENDORED=OFF -DSDLMIXER_EXAMPLES=OFF -DSDLMIXER_TESTS=OFF -DSDLMIXER_FLAC=OFF -DSDLMIXER_GME=OFF -DSDLMIXER_MOD=OFF -DSDLMIXER_MP3=OFF -DSDLMIXER_MIDI=OFF -DSDLMIXER_OPUS=OFF -DSDLMIXER_WAVPACK=OFF
cmake --build "$stage/build/SDL_mixer" --parallel 2
cmake --install "$stage/build/SDL_mixer"
cmake -S "$stage/src/enet" -B "$stage/build/enet" -G 'MSYS Makefiles' -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib64 -DENET_BUILD_TESTS=OFF
cmake --build "$stage/build/enet" --parallel 2
cmake --install "$stage/build/enet"
cmake -S "$stage/src/cmocka" -B "$stage/build/cmocka" -G 'MSYS Makefiles' -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib64 -DWITH_EXAMPLES=OFF -DUNIT_TESTING=OFF
cmake --build "$stage/build/cmocka" --parallel 2
cmake --install "$stage/build/cmocka"
cp -R "$stage/src/SDL" "$prefix/source/SDL"
cp -R "$stage/src/smc" "$prefix/source/smc"
test -f "$prefix/include/SDL3/SDL.h"
test -f "$prefix/include/cmocka.h"
test -f "$prefix/include/enet/enet.h"
test -d "$prefix/source/smc"
"""
        code, stdout, stderr = self.client.exec(bash, ["-lc", script], 3600)
        (self.output / "dependency-build.log").write_bytes(stdout + stderr)
        if code != 0:
            raise SurveyFailure("FAIL-PRODUCT", "dependency-build", "dependency-build-failure", 1)
        self._write_manifest(metadata, posix_final)

    def _write_manifest(self, metadata: dict[str, dict[str, str]], posix_final: str) -> None:
        bash = r"C:\msys64\usr\bin\bash.exe"
        posix_stage = windows_to_msys(self.staging)
        version_script = """export PATH=/ucrt64/bin:/usr/bin
printf 'gcc=%s\nld=%s\nmake=%s\ncmake=%s\npkgconf=%s\n' "$(gcc --version | head -n1)" "$(ld --version | head -n1)" "$(make --version | head -n1)" "$(cmake --version | head -n1)" "$(pkgconf --version)"
"""
        code, stdout, stderr = self.client.exec(bash, ["-lc", version_script], 120)
        if code != 0:
            (self.output / "dependency-manifest-build.log").write_bytes(stdout + stderr)
            raise SurveyFailure("FAIL-TOOL", "dependency-build", "dependency-manifest-generation-failure", 3)
        toolchain = dict(
            line.split("=", 1)
            for line in stdout.decode("utf-8", "replace").splitlines()
            if "=" in line
        )
        if set(toolchain) != {"gcc", "ld", "make", "cmake", "pkgconf"}:
            raise SurveyFailure("FAIL-TOOL", "dependency-build", "dependency-manifest-generation-failure", 3)
        manifest = {
            "schema": MANIFEST_SCHEMA,
            "dependency_set_id": self.set_id,
            "crt": "ucrt",
            "compiler_target": "x86_64-w64-mingw32",
            "dependencies": metadata,
            "cmake_generator": "MSYS Makefiles",
            "build_options": BUILD_OPTIONS,
            "install_path": posix_final,
            "toolchain": toolchain,
        }
        encoded = json.dumps(manifest, sort_keys=True, separators=(",", ":"))
        required_checks = "\n".join(
            f"test -f \"$prefix/{path}\"" for path in REQUIRED_ARTIFACTS
        )
        script = f"""set -eu
export PATH=/ucrt64/bin:/usr/bin
prefix='{posix_stage}/install'
cd "$prefix"
{required_checks}
printf '%s' {shell_quote(encoded)} > dependency-manifest.json
printf '%s' {shell_quote(json.dumps(toolchain, sort_keys=True, separators=(",", ":")))} > toolchain.json
find . -type f ! -name artifact-sha256.txt -print0 | sort -z | xargs -0 sha256sum > artifact-sha256.txt
"""
        code, stdout, stderr = self.client.exec(bash, ["-lc", script], 600)
        (self.output / "dependency-manifest-build.log").write_bytes(stdout + stderr)
        if code != 0:
            raise SurveyFailure("FAIL-TOOL", "dependency-build", "dependency-manifest-generation-failure", 3)

    def _publish(self) -> None:
        script = (
            f"$s='{self.staging}\\install';$d='{self.final}';"
            "if(Test-Path -LiteralPath $d){exit 42};"
            "New-Item -ItemType Directory -Path (Split-Path -Parent $d) -Force|Out-Null;"
            "Move-Item -LiteralPath $s -Destination $d -ErrorAction Stop"
        )
        self._powershell(script, "dependency-publish-failure")
        self.published_by_run = True

    def _remove_published_set(self) -> None:
        script = (
            f"$p='{self.final}';$e='{self.final}';if($p -cne $e){{exit 41}};"
            "if(Test-Path -LiteralPath $p){Remove-Item -LiteralPath $p -Recurse -Force -ErrorAction Stop}"
        )
        self._powershell(script, "dependency-published-set-cleanup-failure")
        self.published_by_run = False

    def _cleanup_staging(self) -> None:
        if not self.staging_created:
            return
        script = (
            f"$p='{self.staging}';$e='{self.staging}';if($p -cne $e){{exit 41}};"
            "if(Test-Path -LiteralPath $p){Remove-Item -LiteralPath $p -Recurse -Force -ErrorAction Stop}"
        )
        self._powershell(script, "dependency-staging-cleanup-failure")
        self.staging_created = False

    def _powershell(self, script: str, reason: str) -> None:
        code, _, _ = self.client.exec(
            r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe",
            ["-NoLogo", "-NoProfile", "-NonInteractive", "-Command", script],
            300,
        )
        if code != 0:
            raise SurveyFailure("FAIL-TOOL", "dependency-build", reason, 3)


def shell_quote(value: str) -> str:
    return "'" + value.replace("'", "'\\''") + "'"


def validate_dependency_set(
    client: QgaClient,
    dependencies: list[Dependency],
    output: Path,
    required: bool = True,
) -> bool:
    set_id = dependency_set_id(dependencies)
    final = rf"C:\platform-survey\dependencies\{set_id}"
    bash = r"C:\msys64\usr\bin\bash.exe"
    posix_final = windows_to_msys(final)
    required_checks = "\n".join(
        f"test -f \"$prefix/{path}\"" for path in REQUIRED_ARTIFACTS
    )
    script = f"""set -eu
export PATH=/ucrt64/bin:/usr/bin
prefix='{posix_final}'
if ! test -d "$prefix"; then exit 44; fi
if ! test -f "$prefix/dependency-manifest.json" || ! test -f "$prefix/artifact-sha256.txt"; then exit 45; fi
{required_checks}
cd "$prefix"
sha256sum -c artifact-sha256.txt >/dev/null
cat dependency-manifest.json
"""
    code, stdout, stderr = client.exec(bash, ["-lc", script], 900)
    (output / "dependency-check.log").write_bytes(stdout + stderr)
    if code == 44:
        if required:
            raise SurveyFailure("FAIL-MISSING-TOOL", "dependency-check", "windows-dependencies-not-provisioned", 2)
        return False
    if code != 0:
        raise SurveyFailure("FAIL-TOOL", "dependency-check", "windows-dependency-manifest-mismatch", 3)
    try:
        manifest_line = stdout.decode("utf-8", "replace").splitlines()[-1]
        manifest = json.loads(manifest_line)
    except (IndexError, json.JSONDecodeError) as error:
        raise SurveyFailure("FAIL-TOOL", "dependency-check", "windows-dependency-manifest-mismatch", 3) from error
    expected = {item.name: {"commit": item.commit, "sha256": item.sha256} for item in dependencies}
    if (
        manifest.get("dependency_set_id") != set_id
        or manifest.get("schema") != MANIFEST_SCHEMA
        or manifest.get("crt") != "ucrt"
        or manifest.get("compiler_target") != "x86_64-w64-mingw32"
        or manifest.get("dependencies") != expected
        or manifest.get("build_options") != BUILD_OPTIONS
        or set(manifest.get("toolchain", {})) != {"gcc", "ld", "make", "cmake", "pkgconf"}
    ):
        raise SurveyFailure("FAIL-TOOL", "dependency-check", "windows-dependency-manifest-mismatch", 3)
    (output / "dependency-manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    artifact_hashes = client.get_file(final + r"\artifact-sha256.txt")
    (output / "dependency-artifact-sha256.txt").write_bytes(artifact_hashes)
    return True


def bootstrap_main(step: str) -> int:
    root = Path(__file__).resolve().parents[2]
    output = Path(os.environ["PLATFORM_OUTPUT"]) / os.environ["PROFILE_ID"]
    output.mkdir(parents=True, exist_ok=True)
    step_log = output / f"dependency-{step}.log"
    step_log.unlink(missing_ok=True)
    primary = Path(os.environ["PLATFORM_PRIMARY_RESULT"])
    dependencies = load_dependencies(root)
    archives = Path(os.environ["PLATFORM_WINDOWS_DEPENDENCY_ARCHIVES"])
    client = QgaClient(
        os.environ.get("PROFILE_VM_URI", "qemu:///session"),
        os.environ["PROFILE_VM_NAME"],
        os.environ.get("PLATFORM_VIRSH", "virsh"),
        int(os.environ.get("PLATFORM_VM_COMMAND_TIMEOUT", "30")),
    )
    try:
        bootstrap = DependencyBootstrap(client, dependencies, archives, output)
        if step == "prepare":
            bootstrap.prepare_staging()
        elif step.startswith("transfer-"):
            name = step.removeprefix("transfer-")
            dependency = next((item for item in dependencies if item.name == name), None)
            if dependency is None:
                raise ValueError(f"unknown transfer step: {step}")
            bootstrap.transfer_archive(dependency)
        elif step == "extract":
            bootstrap.extract_archives()
        elif step.startswith("build-"):
            bootstrap.build_dependency(step.removeprefix("build-"))
        elif step == "sources":
            bootstrap.copy_sources()
        elif step == "manifest":
            bootstrap.write_manifest()
        elif step == "publish":
            bootstrap.publish()
        elif step == "validate":
            validate_dependency_set(client, dependencies, output, required=True)
        elif step == "cleanup":
            bootstrap.cleanup_staging()
        else:
            raise ValueError(f"unknown dependency bootstrap step: {step}")
    except SurveyFailure as failure:
        step_log.write_text(
            f"{failure.outcome}: phase={failure.phase} reason={failure.reason}"
            f" detail={failure.__cause__ or failure}\n",
            encoding="utf-8",
        )
        write_primary(primary, failure)
        return failure.status
    except (OSError, QgaError, ValueError) as error:
        step_log.write_text(str(error) + "\n", encoding="utf-8")
        failure = SurveyFailure("FAIL-TOOL", "dependency-build", "dependency-bootstrap-failure", 3)
        write_primary(primary, failure)
        return 3
    if not step_log.exists():
        step_log.write_text(f"PASS: dependency step {step} completed\n", encoding="utf-8")
    write_primary(primary, SurveyFailure("PASS", "dependency-build", f"dependency-step-{step}-complete", 0))
    return 0


def acquire_main(destination: str) -> int:
    root = Path(__file__).resolve().parents[2]
    dependencies = load_dependencies(root)
    try:
        acquire_archives(dependencies, Path(destination))
    except (OSError, ValueError) as error:
        print(f"dependency archive acquisition failed: {error}", file=sys.stderr)
        return 3
    print(dependency_set_id(dependencies))
    return 0


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "acquire":
        raise SystemExit(acquire_main(sys.argv[2]))
    if len(sys.argv) == 3 and sys.argv[1] == "bootstrap":
        raise SystemExit(bootstrap_main(sys.argv[2]))
    print("usage: windows_dependencies.py acquire DIRECTORY | bootstrap STEP", file=sys.stderr)
    raise SystemExit(3)