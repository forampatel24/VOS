"""
backend/routers/interrupts.py

Interrupt controller and error-manager endpoints.
"""

from __future__ import annotations

from fastapi import APIRouter

from backend.routers.common import kernel_action, with_ok
from backend.schemas.common import KernelReply
from backend.schemas.interrupts import (
    InterruptReply,
    InterruptTrigger,
    PanicRequest,
)
from backend.services.kernel_loader import jvk_snapshot

router = APIRouter()


@router.get("/interrupts")
def interrupt_state() -> dict:
    return kernel_action("list_interrupts")


@router.post("/interrupts", response_model=InterruptReply)
def trigger_interrupt(payload: InterruptTrigger) -> dict:
    return kernel_action("trigger_interrupt", payload)


@router.post("/interrupts/panic", response_model=InterruptReply)
def panic(payload: PanicRequest) -> dict:
    return kernel_action("panic", payload)


@router.post("/interrupts/clear-panic", response_model=KernelReply)
def clear_panic() -> dict:
    return kernel_action("clear_panic")


@router.get("/errors")
def error_state() -> dict:
    snapshot = jvk_snapshot()
    return with_ok(
        {
            "error_manager": snapshot.get("error_manager", {}),
            "interrupts": snapshot.get("interrupts", {}),
        }
    )
