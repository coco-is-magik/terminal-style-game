#!/usr/bin/env python3
"""Typed Windows platform-survey result shared by guest operations."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class SurveyFailure(Exception):
    outcome: str
    phase: str
    reason: str
    status: int


def write_primary(path: Path, result: SurveyFailure) -> None:
    path.write_text(
        f"PRIMARY_OUTCOME={result.outcome}\nPRIMARY_PHASE={result.phase}\n"
        f"PRIMARY_REASON={result.reason}\nPRIMARY_STATUS={result.status}\n",
        encoding="utf-8",
    )