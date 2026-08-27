import { useEffect, useState } from "react";
import { useKernelStore } from "@/store/kernelStore";
import BootScreen from "@/components/BootScreen";
import Desktop from "@/components/desktop/Desktop";
import Taskbar from "@/components/desktop/Taskbar";

export default function App() {
  const init = useKernelStore((s) => s.init);
  const destroy = useKernelStore((s) => s.destroy);
  const snapshot = useKernelStore((s) => s.snapshot);
  const error = useKernelStore((s) => s.error);
  const [bootDone, setBootDone] = useState(false);
  const [showBoot, setShowBoot] = useState(true);

  useEffect(() => {
    init();
    return () => destroy();
  }, [init, destroy]);

  // Always hide boot after animation, regardless of kernel state
  useEffect(() => {
    if (bootDone) {
      const t = setTimeout(() => setShowBoot(false), 280);
      return () => clearTimeout(t);
    }
  }, [bootDone]);

  // Failsafe: never keep boot longer than 3.5s even if timers glitch
  useEffect(() => {
    const t = setTimeout(() => setShowBoot(false), 3500);
    return () => clearTimeout(t);
  }, []);

  return (
    <div className="h-screen w-screen overflow-hidden bg-[#060a14] text-white flex flex-col">
      {showBoot && <BootScreen onDone={() => setBootDone(true)} />}

      {/* offline banner — honest, not fake */}
      {error && !snapshot && (
        <div className="absolute top-0 inset-x-0 z-50 bg-amber-500/15 border-b border-amber-400/30 px-4 py-2 mono text-xs text-amber-200 text-center">
          Backend offline — start it with <span className="text-white">uvicorn backend.main:app --port 8000</span> · {error}
        </div>
      )}

      <div className="relative flex-1 overflow-hidden">
        <Desktop />
      </div>

      <Taskbar />
    </div>
  );
}
