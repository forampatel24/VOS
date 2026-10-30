import { create } from "zustand";
import type { SessionUser } from "@/api/types";

interface SessionState {
  user: SessionUser | null;
  login: (user: SessionUser) => void;
  logout: () => void;
}

export const useSessionStore = create<SessionState>((set) => ({
  user: null,
  login: (user) => set({ user }),
  logout: () => set({ user: null }),
}));
