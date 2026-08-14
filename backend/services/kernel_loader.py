"""
backend/services/kernel_loader.py

ctypes bindings for the JARVIS kernel JSON ABI.

The kernel returns JSON strings from static buffers owned by the kernel.
We therefore use restype=c_void_p and copy the string with string_at --
ctypes would otherwise call free() on pointers it does not own.
"""

from __future__ import annotations

import ctypes
import json
import os

KERNEL_NAME = "jarvis_kernel.dll"

_kernel: ctypes.CDLL | None = None


def _project_root() -> str:
    """Project root = three levels up from this file (backend/services/)."""
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def kernel_path() -> str:
    candidates = (
        os.path.join(_project_root(), "kernel", KERNEL_NAME),
        os.path.join(_project_root(), KERNEL_NAME),
    )
    for path in candidates:
        if os.path.isfile(path):
            return path
    raise FileNotFoundError(
        f"{KERNEL_NAME} not found. Build it with `mingw32-make` inside kernel/."
    )


def _read_string(pointer: int) -> str:
    if not pointer:
        return ""
    raw = ctypes.string_at(pointer)
    return raw.decode("utf-8", errors="replace")


def load_kernel() -> ctypes.CDLL:
    global _kernel
    if _kernel is not None:
        return _kernel

    lib = ctypes.CDLL(kernel_path())

    lib.jvk_init.restype = ctypes.c_void_p
    lib.jvk_init.argtypes = [ctypes.c_char_p]

    lib.jvk_command.restype = ctypes.c_void_p
    lib.jvk_command.argtypes = [ctypes.c_char_p]

    lib.jvk_tick.restype = None
    lib.jvk_tick.argtypes = []

    lib.jvk_snapshot.restype = ctypes.c_void_p
    lib.jvk_snapshot.argtypes = []

    lib.jvk_logs.restype = ctypes.c_void_p
    lib.jvk_logs.argtypes = [ctypes.c_int]

    lib.jvk_shutdown.restype = None
    lib.jvk_shutdown.argtypes = []

    lib.jvk_last_error.restype = ctypes.c_int
    lib.jvk_last_error.argtypes = [ctypes.c_char_p, ctypes.c_size_t]

    _kernel = lib
    return lib


# ---- typed wrappers -----------------------------------------------------

def jvk_init(config: dict | None = None) -> str:
    lib = load_kernel()
    payload = json.dumps(config or {}).encode("utf-8")
    return _read_string(lib.jvk_init(payload))


def jvk_command(action: dict) -> dict:
    lib = load_kernel()
    payload = json.dumps(action).encode("utf-8")
    result = _read_string(lib.jvk_command(payload))
    try:
        return json.loads(result)
    except json.JSONDecodeError:
        return {"ok": False, "error": f"kernel returned invalid JSON: {result}"}


def jvk_tick() -> None:
    load_kernel().jvk_tick()


def jvk_snapshot() -> dict:
    lib = load_kernel()
    result = _read_string(lib.jvk_snapshot())
    try:
        return json.loads(result)
    except json.JSONDecodeError:
        return {"ok": False, "error": f"kernel returned invalid JSON: {result}"}


def jvk_logs(since: int = 0) -> dict:
    lib = load_kernel()
    result = _read_string(lib.jvk_logs(since))
    try:
        return json.loads(result)
    except json.JSONDecodeError:
        return {"ok": False, "error": f"kernel returned invalid JSON: {result}"}


def jvk_shutdown() -> None:
    load_kernel().jvk_shutdown()


def jvk_last_error() -> str:
    lib = load_kernel()
    buf = ctypes.create_string_buffer(256)
    lib.jvk_last_error(buf, ctypes.c_size_t(256))
    return buf.value.decode("utf-8", errors="replace")