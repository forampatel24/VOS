"""
backend/services/auth_store.py

SQLite-backed single-user session store.

JARVIS OS is a local educational machine, so authentication is deliberately
small: one Admin account whose password hash lives beside the other durable
settings. The password can be overridden at first boot with
``JARVIS_ADMIN_PASSWORD``. No session tokens are fabricated — the renderer
keeps the returned user until the desktop is locked or reloaded.
"""

from __future__ import annotations

import hashlib
import hmac
import os
import secrets
import sqlite3
from typing import Any

from backend.services import agent_store

DEFAULT_USERNAME = "admin"
DEFAULT_DISPLAY_NAME = "Admin"
DEFAULT_PASSWORD = "jarvis"
PASSWORD_ENV = "JARVIS_ADMIN_PASSWORD"
_ITERATIONS = 180_000


def _connect() -> sqlite3.Connection:
    path = agent_store.db_path()
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    conn = sqlite3.connect(path)
    conn.row_factory = sqlite3.Row
    return conn


def _hash(password: str, salt: str) -> str:
    digest = hashlib.pbkdf2_hmac(
        "sha256",
        password.encode("utf-8"),
        salt.encode("utf-8"),
        _ITERATIONS,
    )
    return digest.hex()


def _default_password() -> str:
    return os.environ.get(PASSWORD_ENV) or DEFAULT_PASSWORD


def init_db() -> None:
    with _connect() as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT NOT NULL UNIQUE,
                display_name TEXT NOT NULL,
                role TEXT NOT NULL DEFAULT 'admin',
                salt TEXT NOT NULL,
                password_hash TEXT NOT NULL,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
            )
            """
        )
        row = conn.execute("SELECT COUNT(*) AS count FROM users").fetchone()
        if int(row["count"]) == 0:
            salt = secrets.token_hex(16)
            conn.execute(
                """
                INSERT INTO users
                    (username, display_name, role, salt, password_hash)
                VALUES (?, ?, 'admin', ?, ?)
                """,
                (
                    DEFAULT_USERNAME,
                    DEFAULT_DISPLAY_NAME,
                    salt,
                    _hash(_default_password(), salt),
                ),
            )


def _public(row: sqlite3.Row) -> dict[str, Any]:
    return {
        "id": row["id"],
        "username": row["username"],
        "display_name": row["display_name"],
        "role": row["role"],
    }


def verify(username: str, password: str) -> dict[str, Any] | None:
    """Return the public user record when the credentials match."""
    init_db()
    with _connect() as conn:
        row = conn.execute(
            "SELECT * FROM users WHERE username = ?",
            (username.strip().lower(),),
        ).fetchone()
    if row is None:
        return None
    candidate = _hash(password, row["salt"])
    if not hmac.compare_digest(candidate, row["password_hash"]):
        return None
    return _public(row)
