"""
backend/routers/devices.py

Device manager and virtual driver endpoints.
"""

from __future__ import annotations

from fastapi import APIRouter, HTTPException

from backend.routers.common import kernel_action, with_ok
from backend.schemas.devices import (
    DeviceFault,
    DeviceRead,
    DeviceReply,
    DeviceSet,
    DeviceWrite,
)
from backend.services.kernel_loader import jvk_snapshot

router = APIRouter()


@router.get("/devices")
def list_devices() -> dict:
    return with_ok({"devices": jvk_snapshot().get("devices", {})})


@router.get("/devices/{key}")
def get_device(key: str) -> dict:
    devices = jvk_snapshot().get("devices", {})
    for device in devices.get("list", []):
        if device.get("key") == key:
            return with_ok({"device": device})
    raise HTTPException(status_code=404, detail="device not found")


@router.post("/devices/{key}/write", response_model=DeviceReply)
def write_device(key: str, payload: DeviceWrite) -> dict:
    return kernel_action("device_write", payload, device=key)


@router.post("/devices/{key}/read", response_model=DeviceReply)
def read_device(key: str, payload: DeviceRead) -> dict:
    return kernel_action("device_read", payload, device=key)


@router.patch("/devices/{key}", response_model=DeviceReply)
def set_device(key: str, payload: DeviceSet) -> dict:
    return kernel_action("device_set", payload, device=key)


@router.post("/devices/{key}/flush", response_model=DeviceReply)
def flush_device(key: str) -> dict:
    return kernel_action("device_flush", device=key)


@router.post("/devices/{key}/interrupt", response_model=DeviceReply)
def interrupt_device(key: str) -> dict:
    return kernel_action("device_interrupt", device=key)


@router.post("/devices/{key}/fault", response_model=DeviceReply)
def fault_device(key: str, payload: DeviceFault) -> dict:
    return kernel_action("device_fault", payload, device=key)


@router.post("/devices/{key}/repair", response_model=DeviceReply)
def repair_device(key: str) -> dict:
    return kernel_action("device_repair", device=key)
