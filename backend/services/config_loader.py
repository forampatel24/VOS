"""
backend/services/config_loader.py

Loads the virtual-hardware configuration (``config/*.json``) into the single
flat boot dictionary the C kernel's ``jvk_init()`` expects.

Responsibilities
----------------
* discover every JSON file in ``config/``
* map each file to its kernel section (``memory.json`` and ``paging.json``
  both feed the ``memory`` section and are deep-merged)
* normalise a few human-written key names into the kernel's ABI spelling
* never invent values: files that are absent simply do not contribute

Public API
----------
``load_config()`` -> dict ready to hand to ``jvk_init``.
"""

from __future__ import annotations

import json
import os
from typing import Any

# file name -> kernel config section
SECTION_FILES: dict[str, str] = {
    "clock.json": "clock",
    "memory.json": "memory",
    "paging.json": "memory",
    "scheduler.json": "scheduler",
    "fs.json": "filesystem",
    "device.json": "devices",
    "ipc.json": "ipc",
    "agents.json": "agents",
    "machine.json": "machine",
    "theme.json": "theme",
    "voice.json": "voice",
}

KNOWN_SECTIONS = set(SECTION_FILES.values())

# human-written key -> kernel ABI key
KEY_ALIASES: dict[str, str] = {
    "replacementAlgorithm": "replacement",
    "allocationStrategy": "allocator",
    "timeSlice": "quantum",
}


def _project_root() -> str:
    """Project root = three levels up from this file (backend/services/)."""
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def config_dir() -> str:
    return os.path.join(_project_root(), "config")


def _clean(value: Any) -> Any:
    """Drop documentation keys (``_comment``) so the kernel sees data only."""
    if isinstance(value, dict):
        return {
            KEY_ALIASES.get(k, k): _clean(v)
            for k, v in value.items()
            if not k.startswith("_")
        }
    if isinstance(value, list):
        return [_clean(v) for v in value]
    return value


def _merge(base: dict, extra: dict) -> dict:
    for key, value in extra.items():
        if isinstance(value, dict) and isinstance(base.get(key), dict):
            _merge(base[key], value)
        else:
            base[key] = value
    return base


def _section_for(name: str, payload: Any) -> tuple[str, Any]:
    """A file may either wrap its section (``{"clock": {...}}``) or be the
    bare section body (``{"speed_hz": ...}``)."""
    if isinstance(payload, dict) and len(payload) == 1:
        (only_key, only_value) = next(iter(payload.items()))
        if only_key in KNOWN_SECTIONS:
            return only_key, only_value
    return SECTION_FILES[name], payload


def load_config(directory: str | None = None) -> dict:
    """Merge every known config file into one boot dictionary."""
    base_dir = directory or config_dir()
    merged: dict[str, Any] = {"boot": True}
    if not os.path.isdir(base_dir):
        return merged

    for name in sorted(os.listdir(base_dir)):
        if name not in SECTION_FILES:
            continue
        path = os.path.join(base_dir, name)
        try:
            with open(path, "r", encoding="utf-8") as handle:
                payload = json.load(handle)
        except (OSError, json.JSONDecodeError):
            continue
        # Documentation keys are stripped first so a file that wraps its
        # section ({ "clock": {...}, "_comment": ... }) is still detected.
        payload = _clean(payload)
        section, body = _section_for(name, payload)
        body = _clean(body)
        current = merged.get(section)
        if isinstance(current, dict) and isinstance(body, dict):
            _merge(current, body)
        else:
            merged[section] = body
    return merged
