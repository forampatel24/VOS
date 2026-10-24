"""
backend/routers/shell.py

Virtual shell endpoint. The kernel re-enters its own command dispatcher,
so shell commands obey exactly the same ABI and logs as direct calls.
"""

from __future__ import annotations

from fastapi import APIRouter

from backend.routers.common import kernel_action, with_ok
from backend.schemas.shell import ShellExec, ShellReply
from backend.services.kernel_loader import jvk_snapshot

router = APIRouter()


@router.get("/shell")
def shell_state() -> dict:
    return with_ok({"shell": jvk_snapshot().get("shell", {})})


@router.post("/shell/exec", response_model=ShellReply)
def execute(payload: ShellExec) -> dict:
    return kernel_action("shell_exec", payload)
