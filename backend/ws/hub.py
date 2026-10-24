"""
backend/ws/hub.py

Single background live-update hub for every WebSocket channel.

One task advances the kernel and broadcasts frames, so connecting more
clients never accelerates the virtual clock. Each client keeps its own log
cursor and receives only what is new since its previous frame.
"""

from __future__ import annotations

import asyncio
import json
from dataclasses import dataclass, field
from typing import Any

from fastapi import WebSocket, WebSocketDisconnect

from backend.schemas.ws import WsCommand
from backend.services.kernel_loader import (
    jvk_logs,
    jvk_snapshot,
    jvk_tick,
    kernel_lock,
)
from backend.ws.channels import (
    CHANNELS,
    build_update,
    voice_status,
)

TICK_INTERVAL = 2.0


@dataclass(eq=False)
class Client:
    ws: WebSocket
    channel: str
    cursor: int = 0
    lock: asyncio.Lock = field(default_factory=asyncio.Lock)


class LiveHub:
    def __init__(self) -> None:
        self.clients: list[Client] = []
        self._task: asyncio.Task | None = None

    async def start(self) -> None:
        if self._task is None or self._task.done():
            self._task = asyncio.create_task(self._run(), name="jarvis-ws-hub")

    async def stop(self) -> None:
        if self._task is None:
            return
        self._task.cancel()
        try:
            await self._task
        except asyncio.CancelledError:
            pass
        self._task = None
        self.clients.clear()

    async def _run(self) -> None:
        while True:
            try:
                await asyncio.sleep(TICK_INTERVAL)
                if self.clients:
                    self.push_frame(tick=True)
            except asyncio.CancelledError:
                break
            except Exception:
                await asyncio.sleep(0.5)

    def _frame(
        self, channel: str, cursor: int
    ) -> tuple[dict[str, Any], int]:
        with kernel_lock():
            snapshot = jvk_snapshot()
            log_data = jvk_logs(cursor)
        entries = log_data.get("logs", [])
        next_cursor = cursor
        if entries:
            next_cursor = max(int(e.get("index", 0)) for e in entries) + 1
        return build_update(channel, snapshot, entries), next_cursor

    async def _send(self, client: Client, payload: dict[str, Any]) -> None:
        async with client.lock:
            await client.ws.send_text(json.dumps(payload))

    async def subscribe(self, ws: WebSocket, channel: str) -> Client:
        await self.start()
        client = Client(ws=ws, channel=channel)
        payload, cursor = self._frame(channel, 0)
        client.cursor = cursor
        self.clients.append(client)
        try:
            await self._send(client, payload)
        except Exception:
            self.unsubscribe(client)
            raise
        return client

    def unsubscribe(self, client: Client) -> None:
        if client in self.clients:
            self.clients.remove(client)

    def push_frame(self, tick: bool) -> None:
        """Advance (optionally) and broadcast one frame synchronously."""
        if not self.clients:
            return
        frames: list[tuple[Client, dict[str, Any]]] = []
        with kernel_lock():
            if tick:
                jvk_tick()
            for client in list(self.clients):
                try:
                    payload, cursor = self._frame(
                        client.channel, client.cursor
                    )
                except Exception:
                    continue
                client.cursor = cursor
                frames.append((client, payload))
        for client, payload in frames:
            asyncio.create_task(self._safe_send(client, payload))

    async def _safe_send(self, client: Client, payload: dict[str, Any]) -> None:
        try:
            await self._send(client, payload)
        except Exception:
            self.unsubscribe(client)

    async def send_snapshot(self, client: Client) -> None:
        payload, cursor = self._frame(client.channel, client.cursor)
        client.cursor = cursor
        await self._send(client, payload)

    async def send_command_result(
        self, client: Client, result: dict[str, Any]
    ) -> None:
        await self._send(
            client, {"type": "command_result", "channel": client.channel,
                     "result": result}
        )


hub = LiveHub()


async def handle_socket(ws: WebSocket, channel: str) -> None:
    await ws.accept()
    if channel not in CHANNELS:
        await ws.send_text(
            json.dumps(
                {
                    "type": "error",
                    "error": f"unknown channel '{channel}'",
                    "available": sorted(CHANNELS),
                }
            )
        )
        await ws.close(code=1008)
        return

    if channel == "voice":
        snapshot = jvk_snapshot()
        await ws.send_text(
            json.dumps(voice_status(int(snapshot.get("uptime_ticks", 0))))
        )
        try:
            while True:
                await ws.receive_text()
        except WebSocketDisconnect:
            pass
        return

    client = await hub.subscribe(ws, channel)
    try:
        while True:
            raw = await ws.receive_text()
            try:
                message = WsCommand.model_validate_json(raw)
            except ValueError as exc:
                await hub._send(
                    client,
                    {
                        "type": "error",
                        "channel": channel,
                        "error": str(exc),
                    },
                )
                continue
            await _dispatch(client, message)
    except WebSocketDisconnect:
        pass
    except Exception:
        try:
            await ws.close()
        except Exception:
            pass
    finally:
        hub.unsubscribe(client)


async def _dispatch(client: Client, message: WsCommand) -> None:
    if message.action == "ping":
        await hub._send(
            client, {"type": "pong", "channel": client.channel}
        )
    elif message.action == "command":
        from backend.services.kernel_loader import jvk_command

        result = jvk_command(message.payload or {})
        await hub.send_command_result(client, result)
        hub.push_frame(tick=False)
    elif message.action == "tick":
        for _ in range(message.count):
            jvk_tick()
        await hub.send_snapshot(client)
        hub.push_frame(tick=False)
    elif message.action == "snapshot":
        await hub.send_snapshot(client)
