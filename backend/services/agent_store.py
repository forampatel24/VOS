"""
backend/services/agent_store.py

SQLite persistence for Agent Studio profiles.

The kernel owns runtime state; this module owns only the durable profile
(what the user typed in Agent Studio). Every custom agent is re-registered
with the kernel at boot, so the kernel registry remains the runtime source
of truth.
"""

from __future__ import annotations

import json
import os
import sqlite3
import uuid
from typing import Any

DB_ENV = "JARVIS_AGENT_DB"


def _project_root() -> str:
    return os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    )


def db_path() -> str:
    override = os.environ.get(DB_ENV)
    if override:
        return override
    return os.path.join(_project_root(), "data", "jarvis_agents.db")


def _connect() -> sqlite3.Connection:
    path = db_path()
    parent = os.path.dirname(os.path.abspath(path))
    os.makedirs(parent, exist_ok=True)
    conn = sqlite3.connect(path)
    conn.row_factory = sqlite3.Row
    return conn


def init_db() -> None:
    with _connect() as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS agent_profiles (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                key TEXT NOT NULL UNIQUE,
                name TEXT NOT NULL,
                role TEXT NOT NULL DEFAULT '',
                personality TEXT NOT NULL DEFAULT '',
                color TEXT NOT NULL DEFAULT '#22d3ee',
                icon TEXT NOT NULL DEFAULT 'bot',
                tools TEXT NOT NULL DEFAULT '[]',
                priority INTEGER NOT NULL DEFAULT 4,
                mem_pages INTEGER NOT NULL DEFAULT 3,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
            )
            """
        )


def _row_to_profile(row: sqlite3.Row) -> dict[str, Any]:
    try:
        tools = json.loads(row["tools"])
    except (TypeError, json.JSONDecodeError):
        tools = []
    return {
        "id": row["id"],
        "key": row["key"],
        "name": row["name"],
        "role": row["role"],
        "personality": row["personality"],
        "color": row["color"],
        "icon": row["icon"],
        "tools": tools if isinstance(tools, list) else [],
        "priority": row["priority"],
        "mem_pages": row["mem_pages"],
        "builtin": False,
    }


def list_profiles() -> list[dict[str, Any]]:
    init_db()
    with _connect() as conn:
        rows = conn.execute(
            "SELECT * FROM agent_profiles ORDER BY id"
        ).fetchall()
    return [_row_to_profile(row) for row in rows]


def get_profile(profile_id: int) -> dict[str, Any] | None:
    with _connect() as conn:
        row = conn.execute(
            "SELECT * FROM agent_profiles WHERE id = ?", (profile_id,)
        ).fetchone()
    return _row_to_profile(row) if row else None


def create_profile(data: dict[str, Any]) -> dict[str, Any]:
    init_db()
    key = data.get("key") or f"custom-{uuid.uuid4().hex[:8]}"
    with _connect() as conn:
        cursor = conn.execute(
            """
            INSERT INTO agent_profiles
                (key, name, role, personality, color, icon, tools,
                 priority, mem_pages)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                key,
                data.get("name", "Custom Agent"),
                data.get("role", ""),
                data.get("personality", "Balanced"),
                data.get("color", "#22d3ee"),
                data.get("icon", "bot"),
                json.dumps(data.get("tools", [])),
                int(data.get("priority", 4)),
                int(data.get("mem_pages", 3)),
            ),
        )
        row = conn.execute(
            "SELECT * FROM agent_profiles WHERE id = ?",
            (cursor.lastrowid,),
        ).fetchone()
    return _row_to_profile(row)


def update_profile(profile_id: int, data: dict[str, Any]) -> dict[str, Any] | None:
    current = get_profile(profile_id)
    if current is None:
        return None
    with _connect() as conn:
        conn.execute(
            """
            UPDATE agent_profiles SET
                name = ?, role = ?, personality = ?, color = ?, icon = ?,
                tools = ?, priority = ?, mem_pages = ?,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
            """,
            (
                data.get("name", current["name"]),
                data.get("role", current["role"]),
                data.get("personality", current["personality"]),
                data.get("color", current["color"]),
                data.get("icon", current["icon"]),
                json.dumps(data.get("tools", current["tools"])),
                int(data.get("priority", current["priority"])),
                int(data.get("mem_pages", current["mem_pages"])),
                profile_id,
            ),
        )
    return get_profile(profile_id)


def delete_profile(profile_id: int) -> dict[str, Any] | None:
    profile = get_profile(profile_id)
    if profile is None:
        return None
    with _connect() as conn:
        conn.execute(
            "DELETE FROM agent_profiles WHERE id = ?", (profile_id,)
        )
    return profile


def profile_to_definition(profile: dict[str, Any]) -> dict[str, Any]:
    return {
        "key": profile["key"],
        "name": profile["name"],
        "role": profile["role"],
        "personality": profile["personality"],
        "color": profile["color"],
        "icon": profile["icon"],
        "tools": profile["tools"],
        "priority": profile["priority"],
        "mem_pages": profile["mem_pages"],
        "builtin": False,
    }
