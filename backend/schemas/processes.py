"""
backend/schemas/processes.py

Process manager, CPU and scheduler contracts.
"""

from __future__ import annotations

from typing import Literal

from pydantic import Field

from backend.schemas.common import ApiModel, KernelReply

SchedulerAlgo = Literal["round_robin", "fcfs", "sjf", "priority"]


class ProcessCreate(ApiModel):
    name: str = Field(default="process", min_length=1, max_length=32)
    priority: int = Field(default=0, ge=0, le=10)
    burst_time: int = Field(default=0, ge=0, le=100000)
    mem_budget: int | None = Field(default=None, ge=0, le=4096)
    mem_budget_mb: int | None = Field(default=None, ge=1, le=4096)


class ProcessReply(KernelReply):
    pid: int | None = None
    name: str | None = None
    mem_budget_pages: int | None = None


class ProcessList(KernelReply):
    processes: list[dict] = Field(default_factory=list)
    queues: dict[str, list[int]] = Field(default_factory=dict)


class SchedulerConfig(ApiModel):
    algo: SchedulerAlgo


class SchedulerReply(KernelReply):
    algo: str | None = None
    switches: int | None = None
    current: int | None = None
    next: int | None = None
    context_switch_irqs: int | None = None
    cpu_utilization: float | None = None
    avg_wait: float | None = None
    avg_turnaround: float | None = None
    completed: int | None = None


class CpuProgram(ApiModel):
    program: list[int] = Field(min_length=1, max_length=4096)


class CpuStepReply(KernelReply):
    halted: bool | None = None
    pc: int | None = None
    ir: int | None = None
