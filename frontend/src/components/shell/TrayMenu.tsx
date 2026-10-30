import { AnimatePresence, motion } from "framer-motion";
import { useEffect } from "react";
import { useKernelStore } from "@/store/kernelStore";
import { useSessionStore } from "@/store/sessionStore";
import { useShellStore } from "@/store/shellStore";
import { useWindowStore } from "@/store/windowStore";

function pct(value: number): string {
  return `${Math.max(0, Math.min(100, value)).toFixed(0)}%`;
}

export default function TrayMenu() {
  const open = useShellStore((state) => state.trayOpen);
  const close = useShellStore((state) => state.closeTray);
  const openSearch = useShellStore((state) => state.openSearch);
  const toggleNotifications = useShellStore((state) => state.toggleNotifications);
  const requestPower = useShellStore((state) => state.requestPower);
  const logout = useSessionStore((state) => state.logout);
  const user = useSessionStore((state) => state.user);
  const snapshot = useKernelStore((state) => state.snapshot);
  const connected = useKernelStore((state) => state.connected);

  useEffect(() => {
    if (!open) return;
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Escape") close();
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [open, close]);

  const memory = snapshot?.memory;
  const framesUsed = memory?.stats.frames_used ?? 0;
  const framesTotal = memory?.config.total_frames ?? 0;
  const memoryPct = framesTotal > 0 ? (framesUsed / framesTotal) * 100 : 0;
  const cpuPct = (snapshot?.scheduler.cpu_utilization ?? 0) * 100;

  const openWindow = (id: "settings" | "system-info") => {
    useWindowStore.getState().open(id);
    close();
  };

  return (
    <AnimatePresence>
      {open && (
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          onPointerDown={close}
          className="fixed inset-0 z-[72] bg-black/10"
        >
          <motion.div
            initial={{ opacity: 0, y: 12, scale: 0.98 }}
            animate={{ opacity: 1, y: 0, scale: 1 }}
            exit={{ opacity: 0, y: 8, scale: 0.98 }}
            transition={{ duration: 0.16, ease: "easeOut" }}
            onPointerDown={(event) => event.stopPropagation()}
            className="absolute bottom-12 right-3 w-[min(340px,calc(100vw-24px))] overflow-hidden rounded-2xl border border-[#1e2a4a] bg-[#0d1225]/97 shadow-[0_24px_80px_rgba(0,0,0,0.7)] backdrop-blur-xl"
          >
            <div className="border-b border-[#1e2a4a] px-3 py-2.5">
              <div className="flex items-center justify-between">
                <div>
                  <div className="mono text-xs font-semibold tracking-widest text-cyan-200">QUICK SETTINGS</div>
                  <div className="mono text-[9px] tracking-widest text-white/30">
                    {user?.display_name ?? "Admin"} · LOCAL SESSION
                  </div>
                </div>
                <span className={`flex items-center gap-1.5 mono text-[9px] ${connected ? "text-emerald-300" : "text-amber-300"}`}>
                  <span className={`h-1.5 w-1.5 rounded-full ${connected ? "bg-emerald-400 animate-pulse" : "bg-amber-400"}`} />
                  {connected ? "LIVE" : "RETRY"}
                </span>
              </div>
            </div>

            <div className="grid grid-cols-3 gap-1.5 p-2.5">
              {[
                { label: "CPU", value: pct(cpuPct), tone: "text-amber-200" },
                { label: "MEMORY", value: pct(memoryPct), tone: "text-violet-200" },
                { label: "AGENTS", value: String(snapshot?.agents.stats.live ?? 0), tone: "text-emerald-200" },
              ].map((stat) => (
                <div key={stat.label} className="rounded-lg border border-[#1e2a4a] bg-[#141b34] px-2 py-1.5">
                  <div className="mono text-[8px] tracking-[0.18em] text-white/30">{stat.label}</div>
                  <div className={`mono mt-0.5 text-sm font-semibold ${stat.tone}`}>{stat.value}</div>
                </div>
              ))}
            </div>

            <div className="grid grid-cols-2 gap-1.5 px-2.5 pb-2.5">
              <button onClick={openSearch} className="rounded-lg border border-[#1e2a4a] bg-[#141b34] px-2 py-1.5 text-left mono text-[10px] text-white/65 hover:bg-[#1a2444]">
                SEARCH <span className="ml-1 text-white/25">Ctrl K</span>
              </button>
              <button onClick={toggleNotifications} className="rounded-lg border border-[#1e2a4a] bg-[#141b34] px-2 py-1.5 text-left mono text-[10px] text-white/65 hover:bg-[#1a2444]">
                NOTIFICATIONS
              </button>
              <button onClick={() => openWindow("settings")} className="rounded-lg border border-[#1e2a4a] bg-[#141b34] px-2 py-1.5 text-left mono text-[10px] text-white/65 hover:bg-[#1a2444]">
                SETTINGS
              </button>
              <button onClick={() => openWindow("system-info")} className="rounded-lg border border-[#1e2a4a] bg-[#141b34] px-2 py-1.5 text-left mono text-[10px] text-white/65 hover:bg-[#1a2444]">
                SYSTEM INFO
              </button>
            </div>

            <div className="mx-2.5 mb-2.5 rounded-lg border border-[#1e2a4a] bg-[#060a14]/70 px-2.5 py-2">
              <div className="flex items-center justify-between mono text-[9px]">
                <span className="text-white/30">VOICE INPUT</span>
                <span className="text-amber-300/70">POST-MVP</span>
              </div>
              <div className="mt-1 flex items-center justify-between mono text-[9px]">
                <span className="text-white/30">KERNEL ABI</span>
                <span className="text-cyan-200/70">C17 · JSON</span>
              </div>
            </div>

            <div className="flex gap-1.5 border-t border-[#1e2a4a] p-2.5">
              <button
                onClick={() => requestPower("restart")}
                className="flex-1 rounded-lg border border-amber-400/25 bg-amber-500/10 px-2 py-1.5 mono text-[10px] tracking-widest text-amber-200 hover:bg-amber-500/20"
              >
                RESTART
              </button>
              <button
                onClick={() => requestPower("shutdown")}
                className="flex-1 rounded-lg border border-red-400/25 bg-red-500/10 px-2 py-1.5 mono text-[10px] tracking-widest text-red-200 hover:bg-red-500/20"
              >
                SHUT DOWN
              </button>
              <button
                onClick={() => {
                  close();
                  logout();
                }}
                className="rounded-lg border border-[#1e2a4a] bg-[#141b34] px-2.5 py-1.5 mono text-[10px] tracking-widest text-white/55 hover:text-white/85"
              >
                LOCK
              </button>
            </div>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
}
