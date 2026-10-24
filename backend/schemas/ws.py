"""
backend/schemas/ws.py

WebSocket channel and client-message contracts.
"""

from __future__ import annotations

from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field, model_validator

Channel = Literal[
    "all",
    "processes",
    "memory",
    "cpu",
    "logs",
    "devices",
    "interrupts",
    "filesystem",
    "ipc",
    "agents",
    "notifications",
    "voice",
]


class WsCommand(BaseModel):
    model_config = ConfigDict(extra="allow")

    action: Literal["tick", "command", "ping", "snapshot"]
    payload: dict[str, Any] | None = None
    count: int = Field(default=1, ge=1, le=10000)

    @model_validator(mode="after")
    def _payload(self) -> "WsCommand":
        if self.action == "command" and not isinstance(self.payload, dict):
            raise ValueError("payload must be an object for command")
        return self


class WsUpdate(BaseModel):
    model_config = ConfigDict(extra="allow")

    type: Literal["update"] = "update"
    channel: str = "all"
    uptime_ticks: int
    data: dict[str, Any] | list[Any] | None = None
    logs: list[dict[str, Any]] = Field(default_factory=list)


class WsStatus(BaseModel):
    type: Literal["status"] = "status"
    channel: str
    available: bool
    reason: str | None = None
    uptime_ticks: int
