"""
backend/schemas/agents.py

Agent runtime command contracts (profiles live in routers/agents.py).
"""

from __future__ import annotations

from pydantic import Field, model_validator

from backend.schemas.common import ApiModel, KernelReply


class AgentTarget(ApiModel):
    agent: str | None = Field(default=None, max_length=23)
    key: str | None = Field(default=None, max_length=23)
    pid: int | None = Field(default=None, ge=1)

    @model_validator(mode="after")
    def _target(self) -> "AgentTarget":
        if not self.agent and not self.key and self.pid is None:
            raise ValueError("agent, key or pid is required")
        return self


class AgentLaunch(AgentTarget):
    pass


class AgentTaskCreate(AgentTarget):
    title: str = Field(default="Task", min_length=1, max_length=96)
    input: str = Field(default="", max_length=512)


class AgentTaskInput(ApiModel):
    title: str = Field(default="Task", min_length=1, max_length=96)
    input: str = Field(default="", max_length=512)


class AgentCancelBody(ApiModel):
    task: int | None = Field(default=None, ge=1)
    task_id: int | None = Field(default=None, ge=1)

    @model_validator(mode="after")
    def _task(self) -> "AgentCancelBody":
        if self.task is None and self.task_id is None:
            raise ValueError("task or task_id is required")
        return self


class AgentCancel(ApiModel):
    pid: int | None = Field(default=None, ge=1)
    task: int | None = Field(default=None, ge=1)
    task_id: int | None = Field(default=None, ge=1)

    @model_validator(mode="after")
    def _task(self) -> "AgentCancel":
        if self.task is None and self.task_id is None:
            raise ValueError("task or task_id is required")
        return self


class AgentKill(ApiModel):
    pid: int = Field(ge=1)


class AgentReply(KernelReply):
    pid: int | None = None
    task_id: int | None = None
    agent: str | None = None
    key: str | None = None
