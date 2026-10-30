"""
backend/schemas/auth.py

Session contracts for the single-user JARVIS OS login.
"""

from __future__ import annotations

from pydantic import Field

from backend.schemas.common import ApiModel


class LoginRequest(ApiModel):
    username: str = Field(min_length=1, max_length=32)
    password: str = Field(min_length=1, max_length=128)


class SessionUser(ApiModel):
    id: int
    username: str
    display_name: str
    role: str


class LoginResponse(ApiModel):
    ok: bool = True
    user: SessionUser
