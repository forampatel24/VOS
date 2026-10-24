"""
backend/schemas/system.py

System, settings and logging contracts.
"""

from __future__ import annotations

from typing import Any, Literal

from pydantic import Field, model_validator

from backend.schemas.common import ApiModel, KernelReply, KernelSnapshot

SchedulerAlgo = Literal["round_robin", "fcfs", "sjf", "priority"]
Allocator = Literal["first_fit", "best_fit", "worst_fit"]
Replacement = Literal["fifo", "lru", "clock"]
ThemeMode = Literal["system", "light", "dark"]


class SchedulerSettings(ApiModel):
    algorithm: SchedulerAlgo = "round_robin"
    quantum: int = Field(default=10, ge=1, le=10000)
    context_switch_overhead: int = Field(default=1, ge=0, le=1000)
    max_agents: int = Field(default=64, ge=1, le=1024)


class MemorySettings(ApiModel):
    allocator: Allocator = "first_fit"
    replacement: Replacement = "clock"
    agent_budget_pages: int = Field(default=64, ge=0, le=4096)
    referenced_bit: bool = True


class ThemeSettings(ApiModel):
    name: str = Field(default="default", min_length=1, max_length=32)
    mode: ThemeMode = "system"
    primary_color: str = Field(default="#00ffff", max_length=24)
    secondary_color: str = Field(default="#0088ff", max_length=24)
    accent_color: str = Field(default="#ff00ff", max_length=24)
    font_family: str = Field(default="Roboto, sans-serif", max_length=96)
    animations: str = Field(default="smooth", max_length=32)
    wallpaper: str = Field(default="default.jpg", max_length=128)


class SettingsDocument(ApiModel):
    scheduler: SchedulerSettings
    memory: MemorySettings
    theme: ThemeSettings


class SchedulerPatch(ApiModel):
    algorithm: SchedulerAlgo | None = None
    quantum: int | None = Field(default=None, ge=1, le=10000)
    context_switch_overhead: int | None = Field(default=None, ge=0, le=1000)
    max_agents: int | None = Field(default=None, ge=1, le=1024)


class MemoryPatch(ApiModel):
    allocator: Allocator | None = None
    replacement: Replacement | None = None
    agent_budget_pages: int | None = Field(default=None, ge=0, le=4096)
    referenced_bit: bool | None = None


class ThemePatch(ApiModel):
    name: str | None = Field(default=None, min_length=1, max_length=32)
    mode: ThemeMode | None = None
    primary_color: str | None = Field(default=None, max_length=24)
    secondary_color: str | None = Field(default=None, max_length=24)
    accent_color: str | None = Field(default=None, max_length=24)
    font_family: str | None = Field(default=None, max_length=96)
    animations: str | None = Field(default=None, max_length=32)
    wallpaper: str | None = Field(default=None, max_length=128)


class SettingsPatch(ApiModel):
    scheduler: SchedulerPatch | None = None
    memory: MemoryPatch | None = None
    theme: ThemePatch | None = None

    @model_validator(mode="after")
    def _has_changes(self) -> "SettingsPatch":
        for section in (self.scheduler, self.memory, self.theme):
            if section is not None and section.model_dump(
                exclude_none=True
            ):
                return self
        raise ValueError("settings patch cannot be empty")


class SettingsResponse(ApiModel):
    ok: bool = True
    settings: SettingsDocument
    persisted: bool


class SettingsUpdateResponse(SettingsResponse):
    applied: list[str] = Field(default_factory=list)
    restart_required: list[str] = Field(default_factory=list)


class SystemInfo(ApiModel):
    model_config = {"extra": "allow"}

    ok: bool = True
    kernel: str = "c-native"
    booted: bool
    shutdown: bool
    uptime_ticks: int
    machine: dict[str, Any]
    clock: dict[str, Any]
    scheduler: dict[str, Any]
    config: dict[str, Any]
    counts: dict[str, int]


class SystemHealth(KernelReply):
    kernel: KernelSnapshot


class ConfigResponse(KernelReply):
    model_config = {"extra": "allow"}


class LogsQuery(ApiModel):
    since: int = Field(default=0, ge=0)
