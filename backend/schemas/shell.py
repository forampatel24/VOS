"""
backend/schemas/shell.py

Virtual shell command contracts.
"""

from __future__ import annotations

from pydantic import Field

from backend.schemas.common import ApiModel, KernelReply


class ShellExec(ApiModel):
    line: str = Field(min_length=1, max_length=512)


class ShellReply(KernelReply):
    output: str | None = None
    cwd: str | None = None
    prompt: str | None = None
    exit: bool | None = None
