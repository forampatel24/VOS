"""
backend/schemas/interrupts.py

Interrupt controller and error manager contracts.
"""

from __future__ import annotations

from typing import Literal

from pydantic import Field

from backend.schemas.common import ApiModel, KernelReply

IrqName = Literal[
    "shutdown",
    "page_fault",
    "disk",
    "keyboard",
    "timer",
    "software",
    "context_switch",
]


class InterruptTrigger(ApiModel):
    irq: IrqName
    source: str = Field(default="", max_length=64)
    detail: str = Field(default="", max_length=128)


class PanicRequest(ApiModel):
    reason: str = Field(default="manual panic", min_length=1, max_length=128)


class InterruptReply(KernelReply):
    irq: str | None = None
    pending: int | None = None
    panic: str | None = None
