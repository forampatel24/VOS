"""
backend/schemas/common.py

Shared Pydantic contracts for the bridge API.

Requests are strict (unknown fields are rejected so typos surface early).
Kernel replies keep their full JSON payload while guaranteeing the two
fields every ABI result carries: ``ok`` and an optional ``error``.
"""

from __future__ import annotations

from typing import Any

from pydantic import BaseModel, ConfigDict, Field


class ApiModel(BaseModel):
    model_config = ConfigDict(extra="forbid")


class KernelReply(BaseModel):
    model_config = ConfigDict(extra="allow")

    ok: bool
    error: str | None = None


class KernelSnapshot(BaseModel):
    model_config = ConfigDict(extra="allow")

    booted: bool
    shutdown: bool
    uptime_ticks: int
    processes: int
    memory: dict[str, Any]
    interrupts: dict[str, Any]
    error_manager: dict[str, Any]
    filesystem: dict[str, Any]
    devices: dict[str, Any]
    machine: dict[str, Any]
    ipc: dict[str, Any]
    shell: dict[str, Any]
    agents: dict[str, Any]
    scheduler: dict[str, Any]
    process_list: list[dict[str, Any]]
    queues: dict[str, list[int]]
    cpu: dict[str, Any]
    clock: dict[str, Any]


class TickRequest(ApiModel):
    count: int = Field(default=1, ge=1, le=10000)


class TickResponse(ApiModel):
    ok: bool
    snapshot: KernelSnapshot


class LogEntry(BaseModel):
    model_config = ConfigDict(extra="allow")

    index: int
    message: str


class LogsResponse(ApiModel):
    logs: list[LogEntry] = Field(default_factory=list)
