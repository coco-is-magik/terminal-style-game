#!/usr/bin/env python3
"""Bounded QEMU Guest Agent command, process, and file transport."""

from __future__ import annotations

import base64
import json
import subprocess
import time
from pathlib import Path
from typing import Any


class QgaError(RuntimeError):
    """Raised when a bounded QGA operation fails."""


class QgaClient:
    SUBPROCESS_TIMEOUT_GRACE = 5

    def __init__(
        self,
        uri: str,
        domain: str,
        virsh: str = "virsh",
        command_timeout: int = 30,
        poll_seconds: float = 1.0,
    ) -> None:
        self.uri = uri
        self.domain = domain
        self.virsh = virsh
        self.command_timeout = command_timeout
        self.poll_seconds = poll_seconds

    def call(self, execute: str, arguments: dict[str, Any] | None = None) -> Any:
        request: dict[str, Any] = {"execute": execute}
        if arguments is not None:
            request["arguments"] = arguments
        command = [
            self.virsh,
            "-c",
            self.uri,
            "qemu-agent-command",
            self.domain,
            "--timeout",
            str(self.command_timeout),
            json.dumps(request, separators=(",", ":")),
        ]
        try:
            result = subprocess.run(
                command,
                check=False,
                capture_output=True,
                text=True,
                timeout=self.command_timeout + self.SUBPROCESS_TIMEOUT_GRACE,
            )
        except (OSError, subprocess.TimeoutExpired) as error:
            raise QgaError(f"QGA command failed: {execute}: {error}") from error
        if result.returncode != 0:
            detail = result.stderr.strip() or result.stdout.strip() or "no diagnostic"
            raise QgaError(f"QGA command failed: {execute}: {detail}")
        try:
            response = json.loads(result.stdout)
        except json.JSONDecodeError as error:
            raise QgaError(f"QGA returned invalid JSON for {execute}") from error
        if "error" in response:
            raise QgaError(f"QGA returned an error for {execute}: {response['error']}")
        if "return" not in response:
            raise QgaError(f"QGA omitted return data for {execute}")
        return response["return"]

    def exec(
        self,
        path: str,
        arguments: list[str],
        execution_timeout: int = 120,
    ) -> tuple[int, bytes, bytes]:
        response = self.call(
            "guest-exec",
            {"path": path, "arg": arguments, "capture-output": True},
        )
        if not isinstance(response, dict) or not isinstance(response.get("pid"), int):
            raise QgaError("QGA guest-exec omitted a valid process ID")
        pid = response["pid"]
        deadline = time.monotonic() + execution_timeout
        while time.monotonic() < deadline:
            status = self.call("guest-exec-status", {"pid": pid})
            if not isinstance(status, dict):
                raise QgaError("QGA guest-exec-status returned invalid data")
            if status.get("exited"):
                exit_code = status.get("exitcode")
                if not isinstance(exit_code, int):
                    raise QgaError("QGA guest process omitted an exit code")
                return (
                    exit_code,
                    self._decode_output(status.get("out-data"), "stdout"),
                    self._decode_output(status.get("err-data"), "stderr"),
                )
            time.sleep(self.poll_seconds)
        raise QgaError(f"guest process timed out: {path}")

    def put_file(self, local_path: Path, guest_path: str, chunk_size: int = 49152) -> None:
        handle = self.call("guest-file-open", {"path": guest_path, "mode": "wb"})
        if not isinstance(handle, int):
            raise QgaError("QGA guest-file-open omitted a valid handle")
        operation_error: Exception | None = None
        try:
            with local_path.open("rb") as source:
                while True:
                    chunk = source.read(chunk_size)
                    if not chunk:
                        break
                    response = self.call(
                        "guest-file-write",
                        {
                            "handle": handle,
                            "buf-b64": base64.b64encode(chunk).decode("ascii"),
                        },
                    )
                    if not isinstance(response, dict) or response.get("count") != len(chunk):
                        raise QgaError("QGA guest-file-write reported a short write")
            self.call("guest-file-flush", {"handle": handle})
        except Exception as error:
            operation_error = error
        try:
            self.call("guest-file-close", {"handle": handle})
        except Exception as error:
            if operation_error is None:
                operation_error = error
        if operation_error is not None:
            raise QgaError(f"guest file upload failed: {guest_path}: {operation_error}")

    def get_file(
        self, guest_path: str, chunk_size: int = 49152, max_bytes: int = 16 * 1024 * 1024
    ) -> bytes:
        handle = self.call("guest-file-open", {"path": guest_path, "mode": "rb"})
        if not isinstance(handle, int):
            raise QgaError("QGA guest-file-open omitted a valid handle")
        content = bytearray()
        operation_error: Exception | None = None
        try:
            while True:
                response = self.call(
                    "guest-file-read", {"handle": handle, "count": chunk_size}
                )
                if not isinstance(response, dict):
                    raise QgaError("QGA guest-file-read returned invalid data")
                encoded = response.get("buf-b64", "")
                count = response.get("count")
                if not isinstance(encoded, str) or not isinstance(count, int):
                    raise QgaError("QGA guest-file-read omitted required fields")
                try:
                    chunk = base64.b64decode(encoded, validate=True)
                except ValueError as error:
                    raise QgaError("QGA guest-file-read returned invalid base64") from error
                if len(chunk) != count:
                    raise QgaError("QGA guest-file-read returned an invalid byte count")
                if len(content) + len(chunk) > max_bytes:
                    raise QgaError("QGA guest file exceeds the download limit")
                content.extend(chunk)
                if response.get("eof") or count == 0:
                    break
        except Exception as error:
            operation_error = error
        try:
            self.call("guest-file-close", {"handle": handle})
        except Exception as error:
            if operation_error is None:
                operation_error = error
        if operation_error is not None:
            raise QgaError(f"guest file download failed: {guest_path}: {operation_error}")
        return bytes(content)

    @staticmethod
    def _decode_output(value: Any, stream: str) -> bytes:
        if value is None:
            return b""
        if not isinstance(value, str):
            raise QgaError(f"QGA guest process returned invalid {stream}")
        try:
            return base64.b64decode(value, validate=True)
        except ValueError as error:
            raise QgaError(f"QGA guest process returned invalid base64 {stream}") from error