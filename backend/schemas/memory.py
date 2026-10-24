"""
backend/schemas/memory.py

Virtual memory command contracts.
"""

from __future__ import annotations

from typing import Literal

from pydantic import Field

from backend.schemas.common import ApiModel, KernelReply

Allocator = Literal["first_fit", "best_fit", "worst_fit"]
Replacement = Literal["fifo", "lru", "clock"]


class MemoryAlloc(ApiModel):
    pid: int = Field(ge=1)
    pages: int = Field(ge=1, le=4096)


class MemoryAllocReply(KernelReply):
    base_vpage: int | None = None
    contiguous: bool | None = None
    frames: list[int] = Field(default_factory=list)


class MemoryFree(ApiModel):
    pid: int = Field(ge=1)


class MemoryFreeReply(KernelReply):
    pid: int | None = None
    pages_freed: int | None = None


class MemoryAccess(ApiModel):
    pid: int = Field(ge=1)
    addr: int = Field(ge=0, le=10_000_000)


class MemoryWrite(MemoryAccess):
    value: int


class MemoryAccessReply(KernelReply):
    pid: int | None = None
    addr: int | None = None
    vpage: int | None = None
    offset: int | None = None
    value: int | None = None


class MemoryConfig(ApiModel):
    allocator: Allocator | None = None
    replacement: Replacement | None = None
    agent_budget_pages: int | None = Field(default=None, ge=0, le=4096)
    referenced_bit: bool | None = None


class MemoryConfigReply(KernelReply):
    allocator: str | None = None
    replacement: str | None = None
    total_frames: int | None = None
    agent_budget_pages: int | None = None
