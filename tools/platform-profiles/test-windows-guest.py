#!/usr/bin/env python3
"""Deterministic tests for Windows guest workspace preparation."""

from __future__ import annotations

import io
import json
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock

from windows_guest import (
    GuestSurvey,
    SurveyFailure,
    create_payload,
    manifest_bytes,
    parse_manifest,
    source_files,
)
from windows_qga import QgaError


class FakeQga:
    def __init__(self) -> None:
        self.os_info = {
            "id": "mswindows",
            "version-id": "10",
            "machine": "x86_64",
            "pretty-name": "Windows 10 Pro",
        }
        self.tool_exit = 0
        self.tool_output = (
            b"MSYSTEM=UCRT64\nGCC_TARGET=x86_64-w64-mingw32\n"
            b"GCC_VERSION=gcc.exe (Rev1) 14.2.0\n"
            b"LINKER_VERSION=GNU ld 2.44\nMAKE_VERSION=GNU Make 4.4.1\n"
            b"CMAKE_VERSION=cmake version 4.0\nPKGCONF_VERSION=2.4.3\n"
        )
        self.files: dict[str, bytes] = {}
        self.commands: list[str] = []
        self.fail_upload = False
        self.mismatch_manifest = False
        self.stale_run_id = False
        self.fail_cleanup = False
        self.dependencies_missing = False

    def call(self, execute: str, arguments: dict | None = None) -> object:
        self.commands.append(execute)
        if execute == "guest-get-osinfo":
            return self.os_info
        raise AssertionError(f"unexpected QGA call: {execute} {arguments}")

    def exec(
        self, path: str, arguments: list[str], execution_timeout: int = 120
    ) -> tuple[int, bytes, bytes]:
        del execution_timeout
        command = arguments[-1]
        self.commands.append(f"exec:{path}:{command}")
        if "Get-CimInstance" in command:
            identity = {
                "Caption": "Microsoft Windows 10 Pro",
                "Version": "10.0.19045",
                "BuildNumber": "19045",
                "UBR": 4529,
                "DisplayVersion": "22H2",
                "OSArchitecture": "64-bit",
                "ComputerName": "FIXTURE",
                "InstallationType": "Client",
                "PowerShell": "5.1",
            }
            return 0, json.dumps(identity).encode(), b""
        if "artifact-sha256.txt" in command:
            if self.dependencies_missing:
                return 44, b"", b""
            from windows_dependencies import BUILD_OPTIONS, MANIFEST_SCHEMA, dependency_set_id, load_dependencies

            root = Path(self.root_path)
            dependencies = load_dependencies(root)
            manifest = {
                "schema": MANIFEST_SCHEMA,
                "dependency_set_id": dependency_set_id(dependencies),
                "crt": "ucrt",
                "compiler_target": "x86_64-w64-mingw32",
                "dependencies": {
                    item.name: {"commit": item.commit, "sha256": item.sha256}
                    for item in dependencies
                },
                "build_options": BUILD_OPTIONS,
                "toolchain": {
                    "gcc": "gcc fixture",
                    "ld": "ld fixture",
                    "make": "make fixture",
                    "cmake": "cmake fixture",
                    "pkgconf": "pkgconf fixture",
                },
            }
            return 0, b"checks passed\n" + json.dumps(manifest).encode() + b"\n", b""
        if 'test ! -e "$source/vendor"' in command:
            return 0, b"", b""
        if "runner_count=" in command:
            return 0, b"runner_count=64\n", b""
        if "platform-capability-preflight.log" in command:
            self.files[next(key.rsplit("\\", 1)[0] + "\\platform-capability-preflight.log"
                            for key in self.files if key.endswith("source-payload.zip"))] = \
                b"[  PASSED  ] 9 test(s).\n"
            return 0, b"", b""
        if "make CC=gcc" in command:
            return 0, b"0\n", b""
        if "file format pei-x86-64" in command:
            return 0, b"native binaries\n", b""
        if "MSYSTEM=UCRT64" in command:
            return self.tool_exit, self.tool_output, b"tool failure"
        if "Expand-Archive" in command:
            return 0, b"", b""
        if "WriteAllLines" in command:
            payload = next(value for key, value in self.files.items() if key.endswith("source-payload.zip"))
            lines = []
            import hashlib

            with zipfile.ZipFile(io.BytesIO(payload)) as archive:
                for name in archive.namelist():
                    digest = hashlib.sha256(archive.read(name)).hexdigest()
                    lines.append(f"{digest}  {name}\n")
            if self.mismatch_manifest:
                lines[0] = "0" * 64 + lines[0][64:]
            manifest_path = next(
                part.split("'", 1)[0]
                for part in command.split("'")
                if part.endswith("source-manifest-guest.txt")
            )
            self.files[manifest_path] = "".join(reversed(lines)).encode()
            return 0, b"", b""
        if "Remove-Item" in command:
            if self.fail_cleanup:
                return 1, b"", b"controlled cleanup failure"
            return 0, b"", b""
        if "New-Item" in command:
            return 0, b"", b""
        raise AssertionError(f"unexpected guest command: {command}")

    def put_file(self, local_path: Path, guest_path: str, chunk_size: int = 49152) -> None:
        del chunk_size
        if self.fail_upload:
            raise QgaError("controlled upload failure")
        self.files[guest_path] = local_path.read_bytes()

    def get_file(self, guest_path: str, chunk_size: int = 49152) -> bytes:
        del chunk_size
        if guest_path.endswith(r"\artifact-sha256.txt"):
            return b"abc123  ./fixture.dll\n"
        for phase in (
            "strict-app-build",
            "strict-test-build",
            "complete-test-run",
            "standards-core",
        ):
            if guest_path.endswith(f"{phase}.log"):
                return b"PASS\n"
        if self.stale_run_id and guest_path.endswith("run-id.txt"):
            return b"stale-run-id\n"
        return self.files[guest_path]


class WindowsGuestTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name) / "root"
        self.output = Path(self.temp.name) / "output"
        for directory in ("src", "tests", "assets", "tools", "docs", "vendor", "build"):
            (self.root / directory).mkdir(parents=True, exist_ok=True)
        (self.root / "Makefile").write_text("all:\n\t@true\n", encoding="utf-8")
        (self.root / "src" / "app.c").write_text("int main(void){return 0;}\n", encoding="utf-8")
        (self.root / "tests" / "test_app.c").write_text("test\n", encoding="utf-8")
        (self.root / "assets" / "map.txt").write_text("1\n", encoding="utf-8")
        (self.root / "tools" / "runner.sh").write_text("#!/bin/sh\n", encoding="utf-8")
        (self.root / "docs" / "large.gif").write_bytes(b"x" * 1000)
        (self.root / "vendor" / "linux.so").write_bytes(b"linux")
        (self.root / "build" / "generated.o").write_bytes(b"generated")
        cache = self.root / "tools" / "__pycache__"
        cache.mkdir()
        (cache / "generated.pyc").write_bytes(b"bytecode")
        self.client = FakeQga()
        self.client.root_path = str(self.root)
        dependency_env = self.root / "tools/platform-profiles/dependencies.env"
        dependency_env.parent.mkdir(parents=True, exist_ok=True)
        dependency_env.write_text(
            "SDL_COMMIT=a\nSDL_SHA256=" + "1" * 64 +
            "\nSDL_MIXER_COMMIT=b\nSDL_MIXER_SHA256=" + "2" * 64 +
            "\nENET_COMMIT=c\nENET_SHA256=" + "3" * 64 +
            "\nCMOCKA_COMMIT=d\nCMOCKA_SHA256=" + "4" * 64 +
            "\nSMC_COMMIT=e\nSMC_SHA256=" + "5" * 64 + "\n",
            encoding="utf-8",
        )

    def tearDown(self) -> None:
        self.temp.cleanup()

    def survey(self) -> GuestSurvey:
        with mock.patch.object(GuestSurvey, "_new_run_id", return_value="20260913T120000Z-0123456789abcdef"):
            return GuestSurvey(self.root, self.output, self.client)  # type: ignore[arg-type]

    def test_source_payload_is_deterministic_and_bounded(self) -> None:
        files = source_files(self.root)
        relative = [path.relative_to(self.root).as_posix() for path in files]
        self.assertEqual(
            relative,
            [
                "Makefile",
                "assets/map.txt",
                "src/app.c",
                "tests/test_app.c",
                "tools/platform-profiles/dependencies.env",
                "tools/runner.sh",
            ],
        )
        first = Path(self.temp.name) / "first.zip"
        second = Path(self.temp.name) / "second.zip"
        create_payload(self.root, files, first)
        create_payload(self.root, files, second)
        self.assertEqual(first.read_bytes(), second.read_bytes())

    def test_complete_workspace_round_trip(self) -> None:
        survey = self.survey()
        survey.run(1024 * 1024)
        survey.cleanup_workspace()
        self.assertEqual(
            parse_manifest((self.output / "source-manifest-host.txt").read_bytes()),
            parse_manifest((self.output / "source-manifest-guest.txt").read_bytes()),
        )
        self.assertIn("run_id=20260913T120000Z-0123456789abcdef", (self.output / "guest-runner.log").read_text())
        mutation_commands = [command for command in self.client.commands if "New-Item" in command or "Remove-Item" in command]
        self.assertTrue(mutation_commands)
        self.assertTrue(all(r"C:\platform-survey\runs\20260913T120000Z-0123456789abcdef" in command for command in mutation_commands))

    def test_wrong_guest_identity_is_rejected_before_mutation(self) -> None:
        self.client.os_info["version-id"] = "11"
        with self.assertRaisesRegex(SurveyFailure, "windows-profile-mismatch"):
            self.survey().run(1024 * 1024)
        self.assertFalse(any("New-Item" in command for command in self.client.commands))

    def test_mismatched_powershell_identity_is_rejected(self) -> None:
        original_exec = self.client.exec

        def wrong_identity(path: str, arguments: list[str], execution_timeout: int = 120):
            result = original_exec(path, arguments, execution_timeout)
            if "Get-CimInstance" in arguments[-1]:
                identity = json.loads(result[1])
                identity["OSArchitecture"] = "32-bit"
                return result[0], json.dumps(identity).encode(), result[2]
            return result

        self.client.exec = wrong_identity  # type: ignore[method-assign]
        with self.assertRaisesRegex(SurveyFailure, "windows-profile-mismatch"):
            self.survey().run(1024 * 1024)

    def test_missing_toolchain_is_classified(self) -> None:
        self.client.tool_exit = 127
        with self.assertRaisesRegex(SurveyFailure, "windows-toolchain-not-provisioned"):
            self.survey().run(1024 * 1024)

    def test_missing_dependencies_stop_before_workspace_mutation(self) -> None:
        self.client.dependencies_missing = True
        with self.assertRaisesRegex(SurveyFailure, "windows-dependencies-not-provisioned"):
            self.survey().run(1024 * 1024)
        self.assertFalse(any("New-Item" in command for command in self.client.commands))

    def test_wrong_toolchain_is_rejected(self) -> None:
        self.client.tool_output = self.client.tool_output.replace(b"UCRT64", b"MSYS")
        with self.assertRaisesRegex(SurveyFailure, "windows-toolchain-profile-mismatch"):
            self.survey().run(1024 * 1024)

    def test_payload_limit_is_enforced_before_guest_mutation(self) -> None:
        with self.assertRaisesRegex(SurveyFailure, "source-payload-too-large"):
            self.survey().run(1)
        self.assertFalse(any("New-Item" in command for command in self.client.commands))

    def test_upload_failure_is_classified(self) -> None:
        self.client.fail_upload = True
        with self.assertRaisesRegex(SurveyFailure, "guest-source-transfer-failure"):
            self.survey().run(1024 * 1024)

    def test_manifest_mismatch_is_rejected(self) -> None:
        self.client.mismatch_manifest = True
        with self.assertRaisesRegex(SurveyFailure, "source-manifest-mismatch"):
            self.survey().run(1024 * 1024)

    def test_stale_run_marker_is_rejected(self) -> None:
        self.client.stale_run_id = True
        with self.assertRaisesRegex(SurveyFailure, "stale-guest-result"):
            self.survey().run(1024 * 1024)

    def test_workspace_cleanup_failure_is_classified(self) -> None:
        survey = self.survey()
        survey.run(1024 * 1024)
        self.client.fail_cleanup = True
        with self.assertRaisesRegex(SurveyFailure, "guest-workspace-cleanup-failure"):
            survey.cleanup_workspace()


if __name__ == "__main__":
    unittest.main(verbosity=2)
