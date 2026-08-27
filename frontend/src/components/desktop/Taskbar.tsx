import { useEffect, useState } from "react";
import { useWindowStore, WindowId } from "@/store/windowStore";
import { useKernelStore } from "@/store/kernelStore";

const MENU: { id: WindowId; label: string; dot: string }[] = [
  { id: "system-monitor", label: "System", dot: "bg-cyan-400" },
  { id: "processes", label: "Processes", dot: "bg-emerald-400" },
  { id: "memory", label: "Memory", dot: "bg-violet-400" },
  { id: "cpu", label: "CPU", dot: "bg-amber-400" },
  { id: "eventlog", label: "Logs", dot: "bg-pink-400" },
  { id: "files", label: "Files", dot: "bg-slate-400" },
  { id: "devices", label: "Devices", dot: "bg-slate-400" },
  { id: "interrupts", label: "IRQs", dot: "bg-slate-400" },
  { id: "terminal", label: "Terminal", dot: "bg-slate-400" },
];

export default function Taskbar() {
  const { windows, open, focus } = useWindowStore();
  const snapshot = useKernelStore((s) => s.snapshot);
  const connected = useKernelStore((s) => s.connected);
  const doTick = useKernelStore((s) => s.doTick);
  const [time, setTime] = useState(() => new Date());

  useEffect(() => {
    const t = setInterval(() => setTime(new Date()), 1000);
    return () => clearInterval(t);
  }, []);

  const handleClick = (id: WindowId) => {
    const w = windows[id];
    if (!w.open) open(id);
    else if (w.minimized) open(id);
    else focus(id);
  };

  return (
    <div className="absolute bottom-0 inset-x-0 z-40 flex h-11 items-center justify-between border-t border-[#1e2a4a] bg-[#0d1225]/90 backdrop-blur-xl px-3">
      {/* left: launcher */}
      <div className="flex items-center gap-1">
        <div className="mr-2 flex items-center gap-2">
          <div className="h-7 w-7 rounded-lg bg-gradient-to-br from-cyan-400 to-blue-600 grid place-items-center text-xs font-bold mono">J</div>
          <span className="mono text-xs font-semibold tracking-widest text-cyan-100 hidden sm:inline">JARVIS</span>
        </div>
        {MENU.map((m) => {
          const w = windows[m.id];
          const isOpen = w.open;
          const isOffline = ["files", "devices", "interrupts", "terminal", "ipc"].includes(m.id);
          return (
            <button
              key={m.id}
              onClick={() => handleClick(m.id)}
              className={`relative flex items-center gap-1.5 rounded-lg px-2.5 py-1.5 text-xs mono tracking-wide transition ${
                isOpen ? "bg-white/10 text-cyan-100" : "text-white/60 hover:bg-white/5 hover:text-white/90"
              }`}
              title={isOffline ? "Subsystem offline — arrives in future milestone" : m.label}
            >
              <span className={`h-1.5 w-1.5 rounded-full ${isOffline ? "bg-slate-500" : m.dot} ${isOpen ? "shadow-[0_0_6px_currentColor]" : ""}`} />
              <span className="hidden md:inline">{m.label}</span>
              {isOffline && <span className="hidden lg:inline text-[10px] opacity-40">offline</span>}
            </button>
          );
        })}
      </div>

      {/* center: kernel status */}
      <div className="hidden lg:flex items-center gap-3 mono text-xs">
        <span className={`flex items-center gap-1.5 ${connected ? "text-emerald-300" : "text-amber-300"}`}>
          <span className={`h-2 w-2 rounded-full ${connected ? "bg-emerald-400 animate-pulse" : "bg-amber-400"}`} />
          {connected ? "kernel live" : "reconnecting"}
        </span>
        {snapshot && (
          <>
            <span className="text-white/20">·</span>
            <span className="text-cyan-300">{snapshot.processes} procs</span>
            <span className="text-white/20">·</span>
            <span className="text-violet-300">{snapshot.memory.stats.frames_used}/{snapshot.memory.config.total_frames} frames</span>
          </>
        )}
        <button
          onClick={() => doTick()}
          className="ml-2 rounded bg-cyan-500/20 px-2 py-1 text-cyan-300 hover:bg-cyan-500/30 transition text-[11px] tracking-widest"
          title="Advance virtual clock by one tick (or wait — WS auto-ticks every ~0.9s)"
        >
          TICK
        </button>
      </div>

      {/* right: clock */}
      <div className="flex items-center gap-3 mono text-xs text-white/70">
        <span className="hidden sm:inline">
          {time.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" })}
        </span>
        <span className="hidden sm:inline text-white/30">{time.toLocaleDateString()}</span>
        {/* window controls (Electron) */}
        <div className="ml-2 hidden lg:flex items-center gap-1">
          <button
            onClick={() => (window as unknown as { jarvis?: { minimize: () => void } }).jarvis?.minimize()}
            className="h-7 w-7 grid place-items-center rounded hover:bg-white/10 text-white/50"
          >
            ─
          </button>
          <button
            onClick={() => (window as unknown as { jarvis?: { maximize: () => void } }).jarvis?.maximize()}
            className="h-7 w-7 grid place-items-center rounded hover:bg-white/10 text-white/50 text-[11px]"
          >
            □
          </button>
          <button
            onClick={() => (window as unknown as { jarvis?: { close: () => void } }).jarvis?.close()}
            className="h-7 w-7 grid place-items-center rounded hover:bg-red-500/20 text-white/50 hover:text-red-300"
          >
            ✕
          </button>
        </div>
      </div>
    </div>
  );
}
