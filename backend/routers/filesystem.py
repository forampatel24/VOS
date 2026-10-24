"""
backend/routers/filesystem.py

Virtual filesystem CRUD, permissions, open-file table and disk stats.
Nothing here touches a host path; every operation is a kernel ``fs_*``
action against the in-memory virtual disk.
"""

from __future__ import annotations

from fastapi import APIRouter, Query

from backend.routers.common import kernel_action, with_ok
from backend.schemas.common import KernelReply
from backend.schemas.filesystem import (
    FsChmod,
    FsClose,
    FsCreate,
    FsEntries,
    FsMkdir,
    FsOpen,
    FsOpenReply,
    FsPath,
    FsRead,
    FsReadReply,
    FsRename,
    FsSearch,
    FsStat,
    FsTransfer,
    FsWrite,
)
from backend.services.kernel_loader import jvk_snapshot

router = APIRouter()


@router.get("/filesystem")
def filesystem_state() -> dict:
    return with_ok({"filesystem": jvk_snapshot().get("filesystem", {})})


@router.get("/filesystem/entries", response_model=FsEntries)
def list_entries(
    path: str = Query(default="/", min_length=1, max_length=256),
) -> dict:
    return kernel_action("fs_list", path=path)


@router.get("/filesystem/stat", response_model=FsStat)
def stat_entry(
    path: str = Query(min_length=1, max_length=256),
) -> dict:
    return kernel_action("fs_stat", path=path)


@router.post("/filesystem/entries", response_model=KernelReply)
def create_entry(payload: FsCreate) -> dict:
    return kernel_action("fs_create", payload)


@router.delete("/filesystem/entries", response_model=KernelReply)
def delete_entry(
    path: str = Query(min_length=1, max_length=256),
    pid: int = Query(default=0, ge=0),
) -> dict:
    return kernel_action("fs_delete", path=path, pid=pid)


@router.post("/filesystem/directories", response_model=KernelReply)
def create_directory(payload: FsMkdir) -> dict:
    return kernel_action("fs_mkdir", payload)


@router.post("/filesystem/read", response_model=FsReadReply)
def read_file(payload: FsRead) -> dict:
    return kernel_action("fs_read", payload)


@router.post("/filesystem/write", response_model=KernelReply)
def write_file(payload: FsWrite) -> dict:
    return kernel_action("fs_write", payload)


@router.post("/filesystem/rename", response_model=KernelReply)
def rename_entry(payload: FsRename) -> dict:
    return kernel_action("fs_rename", payload)


@router.post("/filesystem/copy", response_model=KernelReply)
def copy_entry(payload: FsTransfer) -> dict:
    return kernel_action("fs_copy", payload)


@router.post("/filesystem/move", response_model=KernelReply)
def move_entry(payload: FsTransfer) -> dict:
    return kernel_action("fs_move", payload)


@router.post("/filesystem/search", response_model=FsEntries)
def search_entries(payload: FsSearch) -> dict:
    return kernel_action("fs_search", payload)


@router.post("/filesystem/open", response_model=FsOpenReply)
def open_file(payload: FsOpen) -> dict:
    return kernel_action("fs_open", payload)


@router.post("/filesystem/close", response_model=KernelReply)
def close_file(payload: FsClose) -> dict:
    return kernel_action("fs_close", payload)


@router.post("/filesystem/chmod", response_model=KernelReply)
def chmod_entry(payload: FsChmod) -> dict:
    return kernel_action("fs_chmod", payload)


@router.post("/filesystem/format", response_model=KernelReply)
def format_disk() -> dict:
    return kernel_action("fs_format")


@router.get("/filesystem/disk")
def disk_stats() -> dict:
    filesystem = jvk_snapshot().get("filesystem", {})
    return with_ok(
        {
            "config": filesystem.get("config", {}),
            "stats": filesystem.get("stats", {}),
            "disk": filesystem.get("disk", {}),
            "open_files": filesystem.get("open_files", []),
        }
    )
