#!/usr/bin/env python3
"""Prepare and verify a Windows survey workspace through QEMU Guest Agent."""

from __future__ import annotations

import hashlib
import json
import os
import platform
import re
import secrets
import sys
import time
import zipfile
from pathlib import Path
from typing import Iterable

from windows_qga import QgaClient, QgaError
from windows_result import SurveyFailure, write_primary


INCLUDED_DIRECTORIES = ("assets", "scripts", "src", "tests", "tools")
INCLUDED_FILES = (
    "Makefile",
    "config.ini",
    "default_user.ini",
    "user.ini",
    "README.md",
    "LICENSE",
    "SMC_INTEGRATION_REPORT.md",
    "vendor.sh",
)
ZIP_TIMESTAMP = (2000, 1, 1, 0, 0, 0)


def source_files(root: Path) -> list[Path]:
    files: list[Path] = []
    for directory in INCLUDED_DIRECTORIES:
        base = root / directory
        if base.is_dir():
            files.extend(
                path
                for path in base.rglob("*")
                if path.is_file()
                and "__pycache__" not in path.parts
                and path.suffix != ".pyc"
            )
    files.extend(root / name for name in INCLUDED_FILES if (root / name).is_file())
    return sorted(set(files), key=lambda path: path.relative_to(root).as_posix())


def manifest_bytes(root: Path, files: Iterable[Path]) -> bytes:
    lines = []
    for path in files:
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        relative = path.relative_to(root).as_posix()
        lines.append(f"{digest}  {relative}\n")
    return "".join(lines).encode("utf-8")


def create_payload(root: Path, files: Iterable[Path], destination: Path) -> None:
    with zipfile.ZipFile(
        destination, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6
    ) as archive:
        for path in files:
            relative = path.relative_to(root).as_posix()
            info = zipfile.ZipInfo(relative, ZIP_TIMESTAMP)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, path.read_bytes())


def parse_manifest(content: bytes) -> dict[str, str]:
    try:
        lines = content.decode("utf-8-sig").splitlines()
    except UnicodeDecodeError as error:
        raise ValueError("manifest is not UTF-8") from error
    entries: dict[str, str] = {}
    for line in lines:
        if "  " not in line:
            raise ValueError("manifest entry has no separator")
        digest, relative = line.split("  ", 1)
        if not re.fullmatch(r"[0-9a-f]{64}", digest) or not relative:
            raise ValueError("manifest entry is malformed")
        if relative in entries:
            raise ValueError("manifest contains a duplicate path")
        entries[relative] = digest
    return entries


class GuestSurvey:
    def __init__(self, root: Path, output: Path, client: QgaClient) -> None:
        self.root = root
        self.output = output
        self.client = client
        self.phase = "guest-identity"
        self.run_id = self._new_run_id()
        self.workspace = rf"C:\platform-survey\runs\{self.run_id}"
        self.source = self.workspace + r"\source"
        self.workspace_created = False

    @staticmethod
    def _new_run_id() -> str:
        return time.strftime("%Y%m%dT%H%M%SZ", time.gmtime()) + "-" + secrets.token_hex(8)

    def run(self, payload_limit: int) -> None:
        self.output.mkdir(parents=True, exist_ok=True)
        (self.output / "guest-runner.log").write_text(
            f"run_id={self.run_id}\nworkspace={self.workspace}\n",
            encoding="utf-8",
        )
        self.collect_identity()
        self.check_toolchain()
        self.check_dependencies()
        self.transfer_source(payload_limit)
        self.run_product_survey()

    def collect_identity(self) -> None:
        self.phase = "guest-identity"
        os_info = self.client.call("guest-get-osinfo")
        if not isinstance(os_info, dict):
            raise SurveyFailure("FAIL-TOOL", self.phase, "windows-profile-mismatch", 3)
        if (
            os_info.get("id") != "mswindows"
            or os_info.get("version-id") != "10"
            or os_info.get("machine") != "x86_64"
        ):
            raise SurveyFailure("FAIL-TOOL", self.phase, "windows-profile-mismatch", 3)
        script = (
            "$o=Get-CimInstance Win32_OperatingSystem;"
            "$c=Get-CimInstance Win32_ComputerSystem;"
            "$v=Get-ItemProperty 'HKLM:\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion';"
            "[ordered]@{Caption=$o.Caption;Version=$o.Version;BuildNumber=$o.BuildNumber;"
            "UBR=$v.UBR;DisplayVersion=$v.DisplayVersion;OSArchitecture=$o.OSArchitecture;"
            "ComputerName=$c.Name;InstallationType=$v.InstallationType;"
            "PowerShell=$PSVersionTable.PSVersion.ToString()}|ConvertTo-Json -Compress"
        )
        code, stdout, stderr = self.client.exec(
            r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe",
            ["-NoLogo", "-NoProfile", "-NonInteractive", "-Command", script],
        )
        if code != 0:
            self._write_command_error("guest-identity.log", stderr)
            raise SurveyFailure("FAIL-TOOL", self.phase, "windows-identity-command-failure", 3)
        try:
            identity = json.loads(stdout.decode("utf-8-sig"))
        except (UnicodeDecodeError, json.JSONDecodeError) as error:
            raise SurveyFailure("FAIL-TOOL", self.phase, "windows-profile-mismatch", 3) from error
        if (
            not isinstance(identity, dict)
            or not str(identity.get("Version", "")).startswith("10.0.")
            or identity.get("OSArchitecture") != "64-bit"
            or not identity.get("BuildNumber")
        ):
            raise SurveyFailure("FAIL-TOOL", self.phase, "windows-profile-mismatch", 3)
        record = {"qga": os_info, "windows": identity}
        (self.output / "guest-identity.log").write_text(
            json.dumps(record, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        environment = {
            "host_architecture": platform.machine(),
            "profile_provider": "libvirt-windows",
            "run_id": self.run_id,
            "workspace": self.workspace,
        }
        (self.output / "host-environment.log").write_text(
            json.dumps(environment, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )

    def check_toolchain(self) -> None:
        self.phase = "toolchain-check"
        bash = r"C:\msys64\usr\bin\bash.exe"
        command = (
            "export MSYSTEM=UCRT64; "
            "export PATH=/ucrt64/bin:/usr/bin; "
            "printf 'MSYSTEM=%s\\n' \"$MSYSTEM\"; "
            "printf 'GCC_TARGET=%s\\n' \"$(gcc -dumpmachine)\"; "
            "printf 'GCC_VERSION=%s\\n' \"$(gcc --version | head -n 1)\"; "
            "printf 'LINKER_VERSION=%s\\n' \"$(ld --version | head -n 1)\"; "
            "printf 'MAKE_VERSION=%s\\n' \"$(make --version | head -n 1)\"; "
            "printf 'CMAKE_VERSION=%s\\n' \"$(cmake --version | head -n 1)\"; "
            "printf 'PKGCONF_VERSION=%s\\n' \"$(pkgconf --version)\""
        )
        code, stdout, stderr = self.client.exec(bash, ["-lc", command])
        toolchain = stdout.decode("utf-8", "replace")
        (self.output / "toolchain.log").write_text(
            toolchain + stderr.decode("utf-8", "replace"), encoding="utf-8"
        )
        if code != 0:
            raise SurveyFailure("FAIL-MISSING-TOOL", self.phase, "windows-toolchain-not-provisioned", 2)
        fields = dict(
            line.split("=", 1) for line in toolchain.splitlines() if "=" in line
        )
        target = fields.get("GCC_TARGET", "")
        if fields.get("MSYSTEM") != "UCRT64" or "x86_64-w64-mingw32" not in target:
            raise SurveyFailure("FAIL-TOOL", self.phase, "windows-toolchain-profile-mismatch", 3)
        required_fields = (
            "GCC_VERSION",
            "LINKER_VERSION",
            "MAKE_VERSION",
            "CMAKE_VERSION",
            "PKGCONF_VERSION",
        )
        if "gcc" not in fields.get("GCC_VERSION", "").lower() or any(
            not fields.get(name) for name in required_fields
        ):
            raise SurveyFailure("FAIL-MISSING-TOOL", self.phase, "windows-toolchain-not-provisioned", 2)

    def transfer_source(self, payload_limit: int) -> None:
        self.phase = "source-transfer"
        files = source_files(self.root)
        if not files or self.root / "Makefile" not in files:
            raise SurveyFailure("FAIL-TOOL", self.phase, "source-payload-invalid", 3)
        host_manifest = manifest_bytes(self.root, files)
        host_manifest_path = self.output / "source-manifest-host.txt"
        host_manifest_path.write_bytes(host_manifest)
        payload = self.output / "source-payload.zip"
        create_payload(self.root, files, payload)
        if payload.stat().st_size > payload_limit:
            raise SurveyFailure("FAIL-TOOL", self.phase, "source-payload-too-large", 3)
        setup = (
            f"$w='{self.workspace}';"
            f"$expected='{self.workspace}';if($w -cne $expected){{exit 41}};"
            "New-Item -ItemType Directory -Path $w -ErrorAction Stop|Out-Null;"
            "New-Item -ItemType Directory -Path ($w+'\\source') -ErrorAction Stop|Out-Null"
        )
        self._powershell(setup, "guest-workspace-create-failure")
        self.workspace_created = True
        guest_payload = self.workspace + r"\source-payload.zip"
        guest_expected = self.workspace + r"\source-manifest-host.txt"
        run_marker = self.output / "run-id.txt"
        run_marker.write_text(self.run_id + "\n", encoding="ascii")
        guest_run_marker = self.workspace + r"\run-id.txt"
        try:
            self.client.put_file(payload, guest_payload)
            self.client.put_file(host_manifest_path, guest_expected)
            self.client.put_file(run_marker, guest_run_marker)
        except QgaError as error:
            self._append_transfer_log(str(error))
            raise SurveyFailure("FAIL-TOOL", self.phase, "guest-source-transfer-failure", 3) from error
        extract = (
            f"Expand-Archive -LiteralPath '{guest_payload}' -DestinationPath '{self.source}' "
            "-Force -ErrorAction Stop"
        )
        self._powershell(extract, "guest-workspace-extract-failure")
        self.phase = "source-integrity"
        guest_manifest = self.workspace + r"\source-manifest-guest.txt"
        manifest_script = (
            f"$root='{self.source}';"
            "$lines=Get-ChildItem -LiteralPath $root -File -Recurse|ForEach-Object{"
            "$rel=$_.FullName.Substring($root.Length+1).Replace('\\','/');"
            "$hash=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLower();"
            "$hash+'  '+$rel}|Sort-Object;"
            f"[IO.File]::WriteAllLines('{guest_manifest}',$lines,[Text.UTF8Encoding]::new($false))"
        )
        self._powershell(manifest_script, "guest-manifest-generation-failure", 300)
        try:
            guest_bytes = self.client.get_file(guest_manifest)
            returned_run_id = self.client.get_file(guest_run_marker).decode("ascii").strip()
        except QgaError as error:
            raise SurveyFailure("FAIL-TOOL", self.phase, "guest-result-missing", 3) from error
        except UnicodeDecodeError as error:
            raise SurveyFailure("FAIL-TOOL", self.phase, "stale-guest-result", 3) from error
        if returned_run_id != self.run_id:
            raise SurveyFailure("FAIL-TOOL", self.phase, "stale-guest-result", 3)
        (self.output / "source-manifest-guest.txt").write_bytes(guest_bytes)
        try:
            manifests_match = parse_manifest(guest_bytes) == parse_manifest(host_manifest)
        except ValueError as error:
            raise SurveyFailure("FAIL-TOOL", self.phase, "source-manifest-mismatch", 3) from error
        if not manifests_match:
            raise SurveyFailure("FAIL-TOOL", self.phase, "source-manifest-mismatch", 3)
        self._append_transfer_log(
            f"files={len(files)} payload_bytes={payload.stat().st_size} run_id={self.run_id}"
        )

    def check_dependencies(self) -> None:
        self.phase = "dependency-check"
        from windows_dependencies import load_dependencies, validate_dependency_set

        validate_dependency_set(
            self.client, load_dependencies(self.root), self.output, required=True
        )

    def run_product_survey(self) -> None:
        from windows_product import WindowsProductSurvey

        WindowsProductSurvey(self.client, self.root, self.output, self.source).run()
        self.phase = "complete-test-run"

    def cleanup_workspace(self) -> None:
        if not self.workspace_created:
            return
        script = (
            f"$w='{self.workspace}';"
            f"$expected='{self.workspace}';if($w -cne $expected){{exit 41}};"
            "if(Test-Path -LiteralPath $w){Remove-Item -LiteralPath $w -Recurse -Force "
            "-ErrorAction Stop}"
        )
        self._powershell(script, "guest-workspace-cleanup-failure")
        self.workspace_created = False

    def _powershell(self, script: str, reason: str, timeout: int = 120) -> None:
        code, _, stderr = self.client.exec(
            r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe",
            ["-NoLogo", "-NoProfile", "-NonInteractive", "-Command", script],
            timeout,
        )
        if code != 0:
            self._append_transfer_log(stderr.decode("utf-8", "replace"))
            raise SurveyFailure("FAIL-TOOL", self.phase, reason, 3)

    def _append_transfer_log(self, message: str) -> None:
        with (self.output / "source-transfer.log").open("a", encoding="utf-8") as log:
            log.write(message + "\n")

    def _write_command_error(self, name: str, content: bytes) -> None:
        (self.output / name).write_bytes(content)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: windows_guest.py PROFILE", file=sys.stderr)
        return 3
    required = ("PLATFORM_PRIMARY_RESULT", "PROFILE_VM_NAME")
    if any(not os.environ.get(name) for name in required):
        print("missing lifecycle guest-runner environment", file=sys.stderr)
        return 3
    root = Path(__file__).resolve().parents[2]
    profile_id = os.environ.get("PROFILE_ID", "windows-10-x64-gcc")
    output_root = Path(os.environ.get("PLATFORM_OUTPUT", root / "build/platform-profiles"))
    output = output_root / profile_id
    primary = Path(os.environ["PLATFORM_PRIMARY_RESULT"])
    try:
        payload_limit = int(os.environ.get("PLATFORM_WINDOWS_PAYLOAD_LIMIT", str(256 * 1024 * 1024)))
        if payload_limit <= 0:
            raise ValueError
    except ValueError:
        write_primary(primary, SurveyFailure("FAIL-TOOL", "host-preflight", "invalid-payload-limit", 3))
        return 3
    client = QgaClient(
        os.environ.get("PROFILE_VM_URI", "qemu:///session"),
        os.environ["PROFILE_VM_NAME"],
        os.environ.get("PLATFORM_VIRSH", "virsh"),
        int(os.environ.get("PLATFORM_VM_COMMAND_TIMEOUT", "30")),
        float(os.environ.get("PLATFORM_QGA_POLL_SECONDS", "1")),
    )
    survey = GuestSurvey(root, output, client)
    failure: SurveyFailure | None = None
    try:
        survey.run(payload_limit)
    except SurveyFailure as error:
        failure = error
    except (OSError, QgaError, ValueError) as error:
        output.mkdir(parents=True, exist_ok=True)
        (output / "guest-runner.log").write_text(str(error) + "\n", encoding="utf-8")
        failure = SurveyFailure("FAIL-TOOL", survey.phase, "guest-command-failure", 3)
    try:
        survey.cleanup_workspace()
    except (OSError, QgaError, SurveyFailure) as error:
        with (output / "guest-runner.log").open("a", encoding="utf-8") as log:
            log.write(f"workspace cleanup failed: {error}\n")
        if failure is None:
            failure = SurveyFailure(
                "FAIL-TOOL", "source-transfer", "guest-workspace-cleanup-failure", 3
            )
    if failure is not None:
        write_primary(primary, failure)
        return failure.status
    write_primary(primary, SurveyFailure("PASS", "complete-test-run", "profile-compatible", 0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())