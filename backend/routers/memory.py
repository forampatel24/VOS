"""
backend/routers/memory.py

Virtual memory, paging and policy endpoints.
"""

from __future__ import annotations

from fastapi import APIRouter

from backend.routers.common import kernel_action, with_ok
from backend.schemas.memory import (
    MemoryAccess,
    MemoryAccessReply,
    MemoryAlloc,
    MemoryAllocReply,
    MemoryConfig,
    MemoryConfigReply,
    MemoryFree,
    MemoryFreeReply,
    MemoryWrite,
)
from backend.services.kernel_loader import jvk_snapshot

router = APIRouter()


@router.get("/memory")
def memory_state() -> dict:
    return with_ok({"memory": jvk_snapshot().get("memory", {})})


@router.post("/memory/alloc", response_model=MemoryAllocReply)
def allocate(payload: MemoryAlloc) -> dict:
    return kernel_action("mem_alloc", payload)


@router.post("/memory/free", response_model=MemoryFreeReply)
def free(payload: MemoryFree) -> dict:
    return kernel_action("mem_free", payload)


@router.post("/memory/read", response_model=MemoryAccessReply)
def read(payload: MemoryAccess) -> dict:
    return kernel_action("mem_read", payload)


@router.post("/memory/write", response_model=MemoryAccessReply)
def write(payload: MemoryWrite) -> dict:
    return kernel_action("mem_write", payload)


@router.patch("/memory/config", response_model=MemoryConfigReply)
@router.post("/memory/config", response_model=MemoryConfigReply)
def configure(payload: MemoryConfig) -> dict:
    return kernel_action("mem_config", payload)
