"""
backend/schemas/filesystem.py

Virtual filesystem command contracts.
"""

from __future__ import annotations

from pydantic import Field, model_validator

from backend.schemas.common import ApiModel, KernelReply


class FsPath(ApiModel):
    path: str = Field(min_length=1, max_length=256)
    pid: int = Field(default=0, ge=0)


class FsCreate(FsPath):
    content: str = ""
    mode: int | str = 0o644
    hidden: bool = False
    readonly: bool = False


class FsMkdir(FsPath):
    mode: int | str = 0o755


class FsRead(ApiModel):
    path: str | None = Field(default=None, max_length=256)
    fd: int | None = Field(default=None, ge=0)
    pid: int = Field(default=0, ge=0)

    @model_validator(mode="after")
    def _target(self) -> "FsRead":
        if self.path is None and self.fd is None:
            raise ValueError("path or fd is required")
        return self


class FsWrite(ApiModel):
    path: str | None = Field(default=None, max_length=256)
    fd: int | None = Field(default=None, ge=0)
    pid: int = Field(default=0, ge=0)
    content: str
    append: bool = False

    @model_validator(mode="after")
    def _target(self) -> "FsWrite":
        if self.path is None and self.fd is None:
            raise ValueError("path or fd is required")
        return self


class FsRename(ApiModel):
    path: str = Field(min_length=1, max_length=256)
    new_name: str = Field(min_length=1, max_length=64)
    pid: int = Field(default=0, ge=0)


class FsTransfer(ApiModel):
    src: str = Field(min_length=1, max_length=256)
    dst: str = Field(min_length=1, max_length=256)
    pid: int = Field(default=0, ge=0)


class FsSearch(ApiModel):
    pattern: str = Field(min_length=1, max_length=64)
    pid: int = Field(default=0, ge=0)


class FsOpen(ApiModel):
    path: str = Field(min_length=1, max_length=256)
    mode: str = Field(default="r", pattern="^(r|w|rw|a)$")
    pid: int = Field(default=0, ge=0)


class FsClose(ApiModel):
    fd: int = Field(ge=0)
    pid: int = Field(default=0, ge=0)


class FsChmod(ApiModel):
    path: str = Field(min_length=1, max_length=256)
    mode: int | str | None = None
    hidden: bool | None = None
    readonly: bool | None = None
    pid: int = Field(default=0, ge=0)

    @model_validator(mode="after")
    def _change(self) -> "FsChmod":
        if self.mode is None and self.hidden is None and self.readonly is None:
            raise ValueError("mode, hidden or readonly is required")
        return self


class FsEntries(KernelReply):
    path: str | None = None
    inode_id: int | None = None
    entries: list[dict] = Field(default_factory=list)
    count: int | None = None


class FsStat(KernelReply):
    entry: dict | None = None


class FsReadReply(KernelReply):
    content: str | None = None
    bytes: int | None = None
    cursor: int | None = None


class FsOpenReply(KernelReply):
    fd: int | None = None
    path: str | None = None
