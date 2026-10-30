import { AnimatePresence } from "framer-motion";
import { useCallback, useEffect, useState } from "react";
import { useKernelStore } from "@/store/kernelStore";
import { useSessionStore } from "@/store/sessionStore";
import { useThemeStore } from "@/store/themeStore";
import BootScreen from "@/components/BootScreen";
import LoginScreen from "@/components/LoginScreen";
import Desktop from "@/components/desktop/Desktop";
import Taskbar from "@/components/desktop/Taskbar";
import NotificationCenter from "@/components/shell/NotificationCenter";
import PowerOverlay from "@/components/shell/PowerOverlay";
import SearchOverlay from "@/components/shell/SearchOverlay";
import Toasts from "@/components/shell/Toasts";
import { useNotificationFeed } from "@/hooks/useNotificationFeed";

function DesktopSession() {
  useNotificationFeed();
  return (
    <>
      <div className="relative min-h-0 flex-1 overflow-hidden">
        <Desktop />
      </div>
      <Taskbar />
      <SearchOverlay />
      <NotificationCenter />
      <Toasts />
      <PowerOverlay />
    </>
  );
}

export default function App() {
  const init = useKernelStore((s) => s.init);
  const destroy = useKernelStore((s) => s.destroy);
  const snapshot = useKernelStore((s) => s.snapshot);
  const error = useKernelStore((s) => s.error);
  const user = useSessionStore((s) => s.user);
  const loadTheme = useThemeStore((s) => s.load);
  const [bootDone, setBootDone] = useState(false);
  const [showBoot, setShowBoot] = useState(true);

  useEffect(() => {
    init();
    return () => destroy();
  }, [init, destroy]);

  useEffect(() => {
    void loadTheme();
  }, [loadTheme, user]);

  const handleBootDone = useCallback(() => setBootDone(true), []);

  useEffect(() => {
    if (bootDone) {
      const timer = setTimeout(() => setShowBoot(false), 280);
      return () => clearTimeout(timer);
    }
  }, [bootDone]);

  // Failsafe: never keep boot longer than 3.5s even if timers glitch.
  useEffect(() => {
    const timer = setTimeout(() => setShowBoot(false), 3500);
    return () => clearTimeout(timer);
  }, []);

  return (
    <div className="flex h-screen w-screen flex-col overflow-hidden bg-[#060a14] text-white">
      <AnimatePresence>
        {showBoot && <BootScreen onDone={handleBootDone} />}
      </AnimatePresence>

      {/* offline banner — honest, not fake */}
      {error && !snapshot && (
        <div className="absolute inset-x-0 top-0 z-[130] border-b border-amber-400/30 bg-amber-500/15 px-4 py-2 text-center mono text-xs text-amber-200">
          Backend offline — start it with{" "}
          <span className="text-white">uvicorn backend.main:app --port 8000</span> · {error}
        </div>
      )}

      {!showBoot && user && <DesktopSession />}
      {!showBoot && !user && (
        <div className="relative min-h-0 flex-1">
          <LoginScreen />
        </div>
      )}
    </div>
  );
}
