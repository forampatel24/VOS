"""
backend/routers/agents.py

REST surface for Agent Studio profiles. Profiles are stored in SQLite and
pushed into the kernel registry; runtime instances still live only in the C
kernel and are read from the snapshot by the desktop.
"""

from __future__ import annotations

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel, Field

from backend.services import agent_store
from backend.services.kernel_loader import jvk_command, jvk_snapshot

router = APIRouter()


class AgentProfileIn(BaseModel):
    name: str = Field(min_length=1, max_length=32)
    role: str = ""
    personality: str = "Balanced"
    color: str = "#22d3ee"
    icon: str = "bot"
    tools: list[str] = Field(default_factory=list)
    priority: int = 4
    mem_pages: int = 3


def _clean(payload: AgentProfileIn) -> dict:
    tools = [t.strip()[:23] for t in payload.tools if t.strip()][:4]
    return {
        "name": payload.name.strip()[:32],
        "role": payload.role.strip()[:96],
        "personality": payload.personality.strip()[:48],
        "color": payload.color.strip()[:16] or "#22d3ee",
        "icon": payload.icon.strip()[:8] or "bot",
        "tools": tools,
        "priority": max(0, min(10, int(payload.priority))),
        "mem_pages": max(1, min(32, int(payload.mem_pages))),
    }


def _register(profile: dict) -> dict:
    definition = agent_store.profile_to_definition(profile)
    result = jvk_command({"action": "agent_define", **definition})
    if not result.get("ok"):
        raise HTTPException(
            status_code=400,
            detail=result.get("error", "kernel rejected agent profile"),
        )
    return result


def register_stored_agents() -> int:
    registered = 0
    for profile in agent_store.list_profiles():
        try:
            _register(profile)
            registered += 1
        except HTTPException:
            continue
    return registered


@router.get("")
def list_agents() -> dict:
    return {"ok": True, "profiles": agent_store.list_profiles()}


@router.post("")
def create_agent(payload: AgentProfileIn) -> dict:
    cleaned = _clean(payload)
    try:
        profile = agent_store.create_profile(cleaned)
    except Exception as exc:  # sqlite integrity or IO failure
        raise HTTPException(status_code=409, detail=str(exc)) from exc
    try:
        _register(profile)
    except HTTPException:
        agent_store.delete_profile(profile["id"])
        raise
    return {"ok": True, "profile": profile}


@router.put("/{profile_id}")
def update_agent(profile_id: int, payload: AgentProfileIn) -> dict:
    current = agent_store.get_profile(profile_id)
    if current is None:
        raise HTTPException(status_code=404, detail="profile not found")
    cleaned = _clean(payload)
    updated = agent_store.update_profile(profile_id, cleaned)
    try:
        _register(updated)
    except HTTPException:
        agent_store.update_profile(profile_id, current)
        raise
    return {"ok": True, "profile": updated}


@router.delete("/{profile_id}")
def delete_agent(profile_id: int) -> dict:
    profile = agent_store.get_profile(profile_id)
    if profile is None:
        raise HTTPException(status_code=404, detail="profile not found")
    snapshot = jvk_snapshot()
    instances = snapshot.get("agents", {}).get("instances", [])
    if any(inst.get("key") == profile["key"] for inst in instances):
        raise HTTPException(
            status_code=409, detail="stop the running agent first"
        )
    jvk_command({"action": "agent_remove", "key": profile["key"]})
    agent_store.delete_profile(profile_id)
    return {"ok": True, "id": profile_id}
