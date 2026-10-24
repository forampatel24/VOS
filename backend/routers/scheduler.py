"""
backend/routers/scheduler.py

CPU, clock and scheduler strategy endpoints.
"""

from __future__ import annotations

from fastapi import APIRouter

from backend.routers.common import kernel_action, with_ok
from backend.schemas.common import KernelReply
from backend.schemas.processes import (
    CpuProgram,
    CpuStepReply,
    SchedulerConfig,
    SchedulerReply,
)
from backend.services.kernel_loader import jvk_snapshot

router = APIRouter()


@router.get("/scheduler", response_model=SchedulerReply)
def scheduler_state() -> dict:
    reply = kernel_action("list_scheduler")
    snapshot = jvk_snapshot().get("scheduler", {})
    reply.setdefault("next", snapshot.get("next", 0))
    return reply


@router.put("/scheduler", response_model=SchedulerReply)
def set_scheduler(payload: SchedulerConfig) -> dict:
    return kernel_action("scheduler_config", payload)


@router.get("/cpu")
def cpu_state() -> dict:
    snapshot = jvk_snapshot()
    return with_ok(
        {
            "cpu": snapshot.get("cpu", {}),
            "clock": snapshot.get("clock", {}),
            "scheduler": snapshot.get("scheduler", {}),
            "machine": snapshot.get("machine", {}),
        }
    )


@router.post("/cpu/program", response_model=KernelReply)
def load_cpu_program(payload: CpuProgram) -> dict:
    return kernel_action("cpu_load_program", payload)


@router.post("/cpu/step", response_model=CpuStepReply)
def step_cpu() -> dict:
    return kernel_action("cpu_step")


@router.post("/cpu/reset", response_model=KernelReply)
def reset_cpu() -> dict:
    return kernel_action("cpu_reset")
