"""
backend/routers/ipc.py

Message queues, shared memory, pipes, mutexes, semaphores and deadlock
recovery. Object names are path segments; every operation is one kernel
``ipc_*`` action so blocking/wakeup semantics stay in the kernel.
"""

from __future__ import annotations

from fastapi import APIRouter, Path, Query

from backend.routers.common import kernel_action, with_ok
from backend.schemas.ipc import (
    DeadlockRecover,
    IpcActor,
    IpcReply,
    MutexCreate,
    MutexLock,
    PipeCreate,
    PipeOpen,
    PipeRead,
    PipeWrite,
    QueueCreate,
    QueueMessage,
    QueueReceive,
    SegmentCreate,
    SegmentRead,
    SegmentWrite,
    SemaphoreCreate,
    SemaphoreWait,
)
from backend.services.kernel_loader import jvk_snapshot

router = APIRouter()

ObjectName = Path(min_length=1, max_length=23)
PidQuery = Query(default=0, ge=0)


@router.get("/ipc")
def ipc_state() -> dict:
    return with_ok({"ipc": jvk_snapshot().get("ipc", {})})


@router.get("/ipc/deadlock", response_model=IpcReply)
@router.post("/ipc/deadlock/detect", response_model=IpcReply)
def detect_deadlock() -> dict:
    return kernel_action("ipc_deadlock")


@router.post("/ipc/deadlock/recover", response_model=IpcReply)
def recover_deadlock(payload: DeadlockRecover) -> dict:
    return kernel_action("ipc_recover", payload)


@router.post("/ipc/queues", response_model=IpcReply)
def create_queue(payload: QueueCreate) -> dict:
    return kernel_action("ipc_queue_create", payload)


@router.post("/ipc/queues/{name}/send", response_model=IpcReply)
def send_message(name: str = ObjectName, payload: QueueMessage | None = None) -> dict:
    return kernel_action("ipc_queue_send", payload, queue=name)


@router.post("/ipc/queues/{name}/recv", response_model=IpcReply)
def receive_message(
    name: str = ObjectName, payload: QueueReceive | None = None
) -> dict:
    return kernel_action("ipc_queue_recv", payload, queue=name)


@router.delete("/ipc/queues/{name}", response_model=IpcReply)
def destroy_queue(name: str = ObjectName, pid: int = PidQuery) -> dict:
    return kernel_action("ipc_queue_destroy", queue=name, pid=pid)


@router.post("/ipc/segments", response_model=IpcReply)
def create_segment(payload: SegmentCreate) -> dict:
    return kernel_action("ipc_shm_create", payload)


@router.post("/ipc/segments/{name}/attach", response_model=IpcReply)
def attach_segment(name: str = ObjectName, payload: IpcActor | None = None) -> dict:
    return kernel_action("ipc_shm_attach", payload, segment=name)


@router.post("/ipc/segments/{name}/detach", response_model=IpcReply)
def detach_segment(name: str = ObjectName, payload: IpcActor | None = None) -> dict:
    return kernel_action("ipc_shm_detach", payload, segment=name)


@router.post("/ipc/segments/{name}/write", response_model=IpcReply)
def write_segment(name: str = ObjectName, payload: SegmentWrite | None = None) -> dict:
    return kernel_action("ipc_shm_write", payload, segment=name)


@router.post("/ipc/segments/{name}/read", response_model=IpcReply)
def read_segment(name: str = ObjectName, payload: SegmentRead | None = None) -> dict:
    return kernel_action("ipc_shm_read", payload, segment=name)


@router.delete("/ipc/segments/{name}", response_model=IpcReply)
def destroy_segment(name: str = ObjectName, pid: int = PidQuery) -> dict:
    return kernel_action("ipc_shm_destroy", segment=name, pid=pid)


@router.post("/ipc/pipes", response_model=IpcReply)
def create_pipe(payload: PipeCreate) -> dict:
    return kernel_action("ipc_pipe_create", payload)


@router.post("/ipc/pipes/{name}/open", response_model=IpcReply)
def open_pipe(name: str = ObjectName, payload: PipeOpen | None = None) -> dict:
    return kernel_action("ipc_pipe_open", payload, pipe=name)


@router.post("/ipc/pipes/{name}/close", response_model=IpcReply)
def close_pipe(name: str = ObjectName, payload: IpcActor | None = None) -> dict:
    return kernel_action("ipc_pipe_close", payload, pipe=name)


@router.post("/ipc/pipes/{name}/write", response_model=IpcReply)
def write_pipe(name: str = ObjectName, payload: PipeWrite | None = None) -> dict:
    return kernel_action("ipc_pipe_write", payload, pipe=name)


@router.post("/ipc/pipes/{name}/read", response_model=IpcReply)
def read_pipe(name: str = ObjectName, payload: PipeRead | None = None) -> dict:
    return kernel_action("ipc_pipe_read", payload, pipe=name)


@router.delete("/ipc/pipes/{name}", response_model=IpcReply)
def destroy_pipe(name: str = ObjectName, pid: int = PidQuery) -> dict:
    return kernel_action("ipc_pipe_destroy", pipe=name, pid=pid)


@router.post("/ipc/mutexes", response_model=IpcReply)
def create_mutex(payload: MutexCreate) -> dict:
    return kernel_action("ipc_mutex_create", payload)


@router.post("/ipc/mutexes/{name}/lock", response_model=IpcReply)
def lock_mutex(name: str = ObjectName, payload: MutexLock | None = None) -> dict:
    return kernel_action("ipc_mutex_lock", payload, mutex=name)


@router.post("/ipc/mutexes/{name}/unlock", response_model=IpcReply)
def unlock_mutex(name: str = ObjectName, payload: IpcActor | None = None) -> dict:
    return kernel_action("ipc_mutex_unlock", payload, mutex=name)


@router.delete("/ipc/mutexes/{name}", response_model=IpcReply)
def destroy_mutex(name: str = ObjectName, pid: int = PidQuery) -> dict:
    return kernel_action("ipc_mutex_destroy", mutex=name, pid=pid)


@router.post("/ipc/semaphores", response_model=IpcReply)
def create_semaphore(payload: SemaphoreCreate) -> dict:
    return kernel_action("ipc_sem_create", payload)


@router.post("/ipc/semaphores/{name}/wait", response_model=IpcReply)
def wait_semaphore(name: str = ObjectName, payload: SemaphoreWait | None = None) -> dict:
    return kernel_action("ipc_sem_wait", payload, semaphore=name)


@router.post("/ipc/semaphores/{name}/post", response_model=IpcReply)
def post_semaphore(name: str = ObjectName, payload: IpcActor | None = None) -> dict:
    return kernel_action("ipc_sem_post", payload, semaphore=name)


@router.delete("/ipc/semaphores/{name}", response_model=IpcReply)
def destroy_semaphore(name: str = ObjectName, pid: int = PidQuery) -> dict:
    return kernel_action("ipc_sem_destroy", semaphore=name, pid=pid)

