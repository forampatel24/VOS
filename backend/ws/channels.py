"""
backend/ws/channels.py

Projection of one kernel snapshot/log frame onto a WebSocket channel.

The bridge never invents state. A channel either forwards a slice of the
authoritative snapshot or, for ``notifications``, re-labels authenticated
kernel log lines with a severity derived from their existing text.
"""

from __future__ import annotations

from typing import Any

from backend.schemas.ws import WsUpdate

CHANNELS = {
    "all",
    "processes",
    "memory",
    "cpu",
    "logs",
    "devices",
    "interrupts",
    "filesystem",
    "ipc",
    "agents",
    "notifications",
    "voice",
}

_ERROR_WORDS = (
    "panic",
    "error",
    "denied",
    "deadlock",
    "fault",
    "failed",
    "full",
    "unknown",
    "segfault",
)
_WARN_WORDS = (
    "swap",
    "budget",
    "suspend",
    "kill",
    "dropped",
    "timeout",
    "blocked",
)


def notification_level(message: str) -> str:
    lowered = message.lower()
    if any(word in lowered for word in _ERROR_WORDS):
        return "error"
    if any(word in lowered for word in _WARN_WORDS):
        return "warning"
    return "info"


def _notifications(logs: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return [
        {
            "index": entry.get("index", 0),
            "message": entry.get("message", ""),
            "level": notification_level(str(entry.get("message", ""))),
        }
        for entry in logs
    ]


def _data(channel: str, snapshot: dict[str, Any], logs: list[dict]) -> Any:
    if channel == "all":
        return None
    if channel == "processes":
        return {
            "processes": snapshot.get("processes", 0),
            "process_list": snapshot.get("process_list", []),
            "queues": snapshot.get("queues", {}),
            "scheduler": snapshot.get("scheduler", {}),
        }
    if channel == "memory":
        return snapshot.get("memory", {})
    if channel == "cpu":
        return {
            "cpu": snapshot.get("cpu", {}),
            "clock": snapshot.get("clock", {}),
            "scheduler": snapshot.get("scheduler", {}),
            "machine": snapshot.get("machine", {}),
        }
    if channel == "logs":
        return None
    if channel == "devices":
        return {
            "devices": snapshot.get("devices", {}),
            "interrupts": snapshot.get("interrupts", {}),
        }
    if channel == "interrupts":
        return {
            "interrupts": snapshot.get("interrupts", {}),
            "error_manager": snapshot.get("error_manager", {}),
        }
    if channel == "filesystem":
        return snapshot.get("filesystem", {})
    if channel == "ipc":
        return snapshot.get("ipc", {})
    if channel == "agents":
        return snapshot.get("agents", {})
    if channel == "notifications":
        return {"notifications": _notifications(logs)}
    return None


def build_update(
    channel: str,
    snapshot: dict[str, Any],
    logs: list[dict[str, Any]],
) -> dict[str, Any]:
    if channel == "all":
        return {
            "type": "update",
            "channel": "all",
            "uptime_ticks": int(snapshot.get("uptime_ticks", 0)),
            "snapshot": snapshot,
            "logs": logs,
        }
    payload: dict[str, Any] = {
        "type": "update",
        "channel": channel,
        "uptime_ticks": int(snapshot.get("uptime_ticks", 0)),
        "logs": [] if channel == "notifications" else logs,
    }
    data = _data(channel, snapshot, logs)
    if data is not None:
        payload["data"] = data
    if channel == "notifications":
        payload["data"] = _data(channel, snapshot, logs)
    return WsUpdate(**payload).model_dump(exclude_none=True)


def voice_status(now_ticks: int) -> dict[str, Any]:
    return {
        "type": "status",
        "channel": "voice",
        "available": False,
        "reason": "voice subsystem is not implemented in this build",
        "uptime_ticks": now_ticks,
    }
