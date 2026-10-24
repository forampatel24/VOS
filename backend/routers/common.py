"""
backend/routers/common.py

Small helpers shared by the subsystem routers. They only translate a
typed request into a flat kernel action and back; no state is kept here.
"""

from __future__ import annotations

from typing import Any

from pydantic import BaseModel

from backend.services.kernel_loader import jvk_command


def action_payload(
    action: str,
    model: BaseModel | None = None,
    **overrides: Any,
) -> dict[str, Any]:
    payload: dict[str, Any] = {"action": action}
    if model is not None:
        payload.update(model.model_dump(exclude_none=True))
    payload.update({key: value for key, value in overrides.items()
                    if value is not None})
    return payload


def kernel_action(
    action: str,
    model: BaseModel | None = None,
    **overrides: Any,
) -> dict[str, Any]:
    return jvk_command(action_payload(action, model, **overrides))


def with_ok(payload: dict[str, Any]) -> dict[str, Any]:
    """Kernel snapshots do not carry ok; API envelopes do."""
    return {"ok": True, **payload}
