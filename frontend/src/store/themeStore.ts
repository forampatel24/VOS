import { create } from "zustand";
import { fetchSettings } from "@/api/client";
import type { ThemeSettings } from "@/api/types";

const DEFAULT_THEME: ThemeSettings = {
  name: "default",
  mode: "system",
  primary_color: "#00ffff",
  secondary_color: "#0088ff",
  accent_color: "#ff00ff",
  font_family: "Inter, system-ui, sans-serif",
  animations: "smooth",
  wallpaper: "default.jpg",
};

export function applyTheme(theme: ThemeSettings) {
  const root = document.documentElement;
  root.style.setProperty("--jarvis-primary", theme.primary_color);
  root.style.setProperty("--jarvis-secondary", theme.secondary_color);
  root.style.setProperty("--jarvis-accent", theme.accent_color);
  root.style.setProperty("--jarvis-font", theme.font_family);
  root.dataset.themeMode = theme.mode;
}

interface ThemeState {
  theme: ThemeSettings;
  load: () => Promise<void>;
  apply: (theme: ThemeSettings) => void;
}

export const useThemeStore = create<ThemeState>((set) => ({
  theme: DEFAULT_THEME,
  load: async () => {
    try {
      const response = await fetchSettings();
      applyTheme(response.settings.theme);
      set({ theme: response.settings.theme });
    } catch {
      applyTheme(DEFAULT_THEME);
    }
  },
  apply: (theme) => {
    applyTheme(theme);
    set({ theme });
  },
}));
