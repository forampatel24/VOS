"""
backend/schemas/devices.py

Device manager and driver command contracts.
"""

from __future__ import annotations

from typing import Literal

from pydantic import Field

from backend.schemas.common import ApiModel, KernelReply

DeviceState = Literal["connected", "busy", "waiting", "fault", "disabled"]


class DeviceWrite(ApiModel):
    data: str
    pid: int = Field(default=0, ge=0)


class DeviceRead(ApiModel):
    max: int | None = Field(default=None, ge=1, le=1_000_000)
    pid: int = Field(default=0, ge=0)


class DeviceSet(ApiModel):
    power: bool | None = None
    state: DeviceState | None = None


class DeviceFault(ApiModel):
    reason: str = Field(default="simulated fault", max_length=96)


class DeviceReply(KernelReply):
    device: str | None = None
    data: str | None = None
    bytes: int | None = None
    state: str | None = None
    irq: str | None = None
    flushed: int | None = None
