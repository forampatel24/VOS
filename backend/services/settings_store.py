"""
backend/services/settings_store.py

Durable system settings for the bridge.

SQLite owns persistence; the kernel owns runtime state. Settings that the
kernel can change live (scheduler algorithm, allocator, replacement policy)
are applied through ``jvk_command`` immediately and also written to the
boot config on the next start. Theme settings are persisted for the UI.
"""

from __future__ import annotations

import copy
import json
import os
import sqlite3
from typing import Any

from pydantic import ValidationError

from backend.schemas.system import SettingsDocument
from backend.services import agent_store
from backend.services.config_loader import load_config
from backend.services.kernel_loader import jvk_command

SETTINGS_KEY = "system"
_SCHEDULER_ALIASES = {
    "round-robin": "round_robin",
    "roundrobin": "round_robin",
    "rr": "round_robin",
    "round_robin": "round_robin",
    "fcfs": "fcfs",
    "sjf": "sjf",
    "priority": "priority",
}


class SettingsError(RuntimeError):
    """Raised when the kernel rejects a persisted setting."""


def _connect() -> sqlite3.Connection:
    path = agent_store.db_path()
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    conn = sqlite3.connect(path)
    conn.row_factory = sqlite3.Row
    return conn


def init_db() -> None:
    with _connect() as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS settings (
                key TEXT PRIMARY KEY,
                value TEXT NOT NULL,
                updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
            )
            """
        )


def _theme_defaults(config: dict[str, Any]) -> dict[str, Any]:
    theme = config.get("theme", {})
    return {
        "name": theme.get("name", "default"),
        "mode": theme.get("themeMode", theme.get("mode", "system")),
        "primary_color": theme.get("primaryColor", "#00ffff"),
        "secondary_color": theme.get("secondaryColor", "#0088ff"),
        "accent_color": theme.get("accentColor", "#ff00ff"),
        "font_family": theme.get("fontFamily", "Roboto, sans-serif"),
        "animations": theme.get("animations", "smooth"),
        "wallpaper": theme.get("wallpaper", "default.jpg"),
    }


def defaults() -> SettingsDocument:
    config = load_config()
    scheduler = config.get("scheduler", {})
    memory = config.get("memory", {})
    clock = config.get("clock", {})
    algo = str(scheduler.get("algorithm", "round_robin")).lower()
    algo = _SCHEDULER_ALIASES.get(algo, "round_robin")
    return SettingsDocument(
        scheduler={
            "algorithm": algo,
            "quantum": scheduler.get(
                "quantum", clock.get("quantum", 10)
            ),
            "context_switch_overhead": scheduler.get(
                "contextSwitchOverhead", 1
            ),
            "max_agents": scheduler.get("maxAgents", 64),
        },
        memory={
            "allocator": memory.get("allocator", "first_fit"),
            "replacement": memory.get("replacement", "clock"),
            "agent_budget_pages": memory.get("agentBudgetPages", 64),
            "referenced_bit": memory.get("referencedBit", True),
        },
        theme=_theme_defaults(config),
    )


def _merge(base: dict, extra: dict) -> dict:
    for key, value in extra.items():
        if isinstance(value, dict) and isinstance(base.get(key), dict):
            _merge(base[key], value)
        else:
            base[key] = value
    return base


def _read_raw() -> dict[str, Any]:
    init_db()
    with _connect() as conn:
        row = conn.execute(
            "SELECT value FROM settings WHERE key = ?", (SETTINGS_KEY,)
        ).fetchone()
    if row is None:
        return {}
    try:
        value = json.loads(row["value"])
    except (TypeError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def get_settings() -> tuple[SettingsDocument, bool]:
    raw = _read_raw()
    merged = _merge(defaults().model_dump(), copy.deepcopy(raw))
    try:
        return SettingsDocument(**merged), bool(raw)
    except ValidationError:
        return defaults(), False


def save_settings(document: SettingsDocument) -> None:
    init_db()
    payload = json.dumps(document.model_dump())
    with _connect() as conn:
        conn.execute(
            """
            INSERT INTO settings (key, value, updated_at)
            VALUES (?, ?, CURRENT_TIMESTAMP)
            ON CONFLICT(key) DO UPDATE SET
                value = excluded.value,
                updated_at = CURRENT_TIMESTAMP
            """,
            (SETTINGS_KEY, payload),
        )


def apply_boot_config(config: dict[str, Any]) -> dict[str, Any]:
    """Overlay persisted runtime settings on the config passed to jvk_init."""
    raw = _read_raw()
    if not raw:
        return config
    boot = copy.deepcopy(config)
    scheduler = raw.get("scheduler", {})
    if isinstance(scheduler, dict):
        target = boot.setdefault("scheduler", {})
        if "algorithm" in scheduler:
            target["algorithm"] = scheduler["algorithm"]
        if "quantum" in scheduler:
            boot.setdefault("clock", {})["quantum"] = scheduler["quantum"]
        if "context_switch_overhead" in scheduler:
            target["contextSwitchOverhead"] = scheduler[
                "context_switch_overhead"
            ]
        if "max_agents" in scheduler:
            target["maxAgents"] = scheduler["max_agents"]
    memory = raw.get("memory", {})
    if isinstance(memory, dict):
        target = boot.setdefault("memory", {})
        if "allocator" in memory:
            target["allocator"] = memory["allocator"]
        if "replacement" in memory:
            target["replacement"] = memory["replacement"]
        if "agent_budget_pages" in memory:
            target["agentBudgetPages"] = memory["agent_budget_pages"]
        if "referenced_bit" in memory:
            target["referencedBit"] = memory["referenced_bit"]
    return boot


def apply_scheduler(document: SettingsDocument) -> bool:
    result = jvk_command(
        {"action": "scheduler_config", "algo": document.scheduler.algorithm}
    )
    if not result.get("ok"):
        raise SettingsError(
            result.get("error", "kernel rejected scheduler settings")
        )
    return True


def apply_memory(document: SettingsDocument) -> bool:
    memory = document.memory
    result = jvk_command(
        {
            "action": "mem_config",
            "allocator": memory.allocator,
            "replacement": memory.replacement,
            "agentBudgetPages": memory.agent_budget_pages,
            "referencedBit": memory.referenced_bit,
        }
    )
    if not result.get("ok"):
        raise SettingsError(
            result.get("error", "kernel rejected memory settings")
        )
    return True


def apply_runtime(document: SettingsDocument) -> list[str]:
    """Apply the runtime-changeable sections and return their names."""
    apply_memory(document)
    apply_scheduler(document)
    return ["memory", "scheduler"]

