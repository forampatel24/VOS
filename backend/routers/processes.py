"""
backend/routers/processes.py

REST surface for the process manager. Every route forwards one kernel ABI
action; PID lookup for the detail route reads the authoritative snapshot.
"""

from __future__ import annotations

from fastapi import APIRouter, HTTPException

from backend.routers.common import kernel_action, with_ok
from backend.schemas.common import KernelReply
from backend.schemas.processes import (
    ProcessCreate,
    ProcessList,
    ProcessReply,
)
from backend.services.kernel_loader import jvk_snapshot

router = APIRouter()


@router.get("", response_model=ProcessList)
def list_processes() -> dict:
    return kernel_action("list_processes")


@router.post("", response_model=ProcessReply)
def create_process(payload: ProcessCreate) -> dict:
    return kernel_action("create_process", payload)


@router.get("/{pid}")
def get_process(pid: int) -> dict:
    snapshot = jvk_snapshot()
    for process in snapshot.get("process_list", []):
        if process.get("pid") == pid:
            return with_ok(
                {
                    "process": process,
                    "queues": snapshot.get("queues", {}),
                }
            )
    raise HTTPException(status_code=404, detail="process not found")


@router.post("/{pid}/suspend", response_model=KernelReply)
def suspend_process(pid: int) -> dict:
    return kernel_action("suspend_process", pid=pid)


@router.post("/{pid}/resume", response_model=KernelReply)
def resume_process(pid: int) -> dict:
    return kernel_action("resume_process", pid=pid)


@router.delete("/{pid}", response_model=ProcessReply)
def kill_process(pid: int) -> dict:
    return kernel_action("kill_process", pid=pid)
