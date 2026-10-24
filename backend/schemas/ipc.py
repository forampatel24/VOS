"""
backend/schemas/ipc.py

IPC and synchronization command contracts. ``pid`` 0 means the system
caller, exactly as the kernel ABI defines it. The object name lives in the
URL for operations on an existing object and in the body for creation.
"""

from __future__ import annotations

from typing import Annotated, Literal

from pydantic import Field

from backend.schemas.common import ApiModel, KernelReply

IpcName = Annotated[str, Field(min_length=1, max_length=23)]
IpcData = Annotated[str, Field(max_length=511)]
IpcMessage = Annotated[str, Field(max_length=95)]


class IpcActor(ApiModel):
    pid: int = Field(default=0, ge=0)


class QueueCreate(IpcActor):
    queue: IpcName
    capacity: int = Field(default=8, ge=1, le=16)


class QueueMessage(IpcActor):
    data: IpcMessage
    nonblock: bool = False


class QueueReceive(IpcActor):
    nonblock: bool = False


class SegmentCreate(IpcActor):
    segment: IpcName
    size: int = Field(default=128, ge=1, le=512)


class SegmentWrite(IpcActor):
    data: IpcData
    offset: int = Field(default=0, ge=0, le=511)


class SegmentRead(IpcActor):
    offset: int = Field(default=0, ge=0, le=511)
    max: int = Field(default=512, ge=1, le=512)


class PipeCreate(IpcActor):
    pipe: IpcName
    capacity: int = Field(default=256, ge=1, le=512)


class PipeOpen(IpcActor):
    mode: Literal["r", "w", "rw"] = "rw"


class PipeWrite(IpcActor):
    data: IpcData
    nonblock: bool = False


class PipeRead(IpcActor):
    max: int = Field(default=128, ge=1, le=512)
    nonblock: bool = False


class MutexCreate(IpcActor):
    mutex: IpcName


class MutexLock(IpcActor):
    nonblock: bool = False


class SemaphoreCreate(IpcActor):
    semaphore: IpcName
    value: int = Field(default=2, ge=0, le=1024)
    max: int = Field(default=4, ge=1, le=1024)


class SemaphoreWait(IpcActor):
    nonblock: bool = False


class DeadlockRecover(IpcActor):
    pid: int = Field(ge=1)


class IpcReply(KernelReply):
    queue: str | None = None
    segment: str | None = None
    pipe: str | None = None
    mutex: str | None = None
    semaphore: str | None = None
    data: str | None = None
    bytes: int | None = None
    blocked: bool | None = None
    state: str | None = None
    waiting_on: str | None = None
    deadlock: bool | None = None
    active: bool | None = None
    cycle_count: int | None = None
    cycles: list[list[int]] = Field(default_factory=list)
    edges: list[dict] = Field(default_factory=list)
    recovered: int | None = None
