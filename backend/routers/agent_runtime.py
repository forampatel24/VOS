"""
backend/routers/agent_runtime.py

Live agent runtime endpoints: launch, task dispatch/cancel, instance
inspection and termination. Runtime state stays in the kernel; this router
only forwards actions and reads the authoritative snapshot.
"""

from __future__ import annotations

from fastapi import APIRouter, Query

from backend.routers.common import kernel_action, with_ok
from backend.schemas.agents import (
    AgentCancelBody,
    AgentLaunch,
    AgentReply,
    AgentTaskCreate,
    AgentTaskInput,
)
from backend.services.kernel_loader import jvk_snapshot

router = APIRouter()


@router.get("/runtime")
def runtime() -> dict:
    agents = jvk_snapshot().get("agents", {})
    return with_ok(dict(agents))


@router.get("/definitions")
def definitions() -> dict:
    agents = jvk_snapshot().get("agents", {})
    return with_ok(
        {
            "config": agents.get("config", {}),
            "stats": agents.get("stats", {}),
            "definitions": agents.get("definitions", []),
        }
    )


@router.get("/instances")
def instances() -> dict:
    agents = jvk_snapshot().get("agents", {})
    return with_ok(
        {
            "config": agents.get("config", {}),
            "stats": agents.get("stats", {}),
            "instances": agents.get("instances", []),
        }
    )


@router.get("/tasks")
def tasks(pid: int = Query(default=0, ge=0)) -> dict:
    return kernel_action("agent_tasks", pid=pid)


@router.post("/launch", response_model=AgentReply)
def launch(payload: AgentLaunch) -> dict:
    return kernel_action("agent_launch", payload)


@router.post("/tasks", response_model=AgentReply)
def create_task(payload: AgentTaskCreate) -> dict:
    return kernel_action("agent_task", payload)


@router.post("/tasks/{task_id}/cancel", response_model=AgentReply)
def cancel_task(
    task_id: int,
    pid: int | None = Query(default=None, ge=1),
) -> dict:
    return kernel_action("agent_cancel", task=task_id, pid=pid)


@router.post("/instances/{pid}/tasks", response_model=AgentReply)
def queue_instance_task(pid: int, payload: AgentTaskInput) -> dict:
    return kernel_action("agent_task", payload, pid=pid)


@router.post("/instances/{pid}/cancel", response_model=AgentReply)
def cancel_instance_task(pid: int, payload: AgentCancelBody) -> dict:
    return kernel_action("agent_cancel", payload, pid=pid)


@router.post("/instances/{pid}/suspend", response_model=AgentReply)
def suspend_instance(pid: int) -> dict:
    return kernel_action("suspend_process", pid=pid)


@router.post("/instances/{pid}/resume", response_model=AgentReply)
def resume_instance(pid: int) -> dict:
    return kernel_action("resume_process", pid=pid)


@router.delete("/instances/{pid}", response_model=AgentReply)
def kill_instance(pid: int) -> dict:
    return kernel_action("agent_kill", pid=pid)
