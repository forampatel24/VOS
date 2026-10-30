"""
backend/routers/auth.py

Login endpoint for the single Admin session.
"""

from __future__ import annotations

from fastapi import APIRouter, HTTPException

from backend.schemas.auth import LoginRequest, LoginResponse
from backend.services import auth_store

router = APIRouter()


@router.post("/login", response_model=LoginResponse)
def login(payload: LoginRequest) -> dict:
    user = auth_store.verify(payload.username, payload.password)
    if user is None:
        raise HTTPException(status_code=401, detail="invalid credentials")
    return {"ok": True, "user": user}
