import { useEffect, useState } from "react";
import { APP_MAP } from "@/lib/apps";
import { useKernelStore } from "@/store/kernelStore";
import { useShellStore } from "@/store/shellStore";
import { useWindowStore, type WindowId } from "@/store/windowStore";
import LauncherMenu from "./LauncherMenu";
import TrayMenu from "@/components/shell/TrayMenu";

export default function Taskbar() {
  const windows = useWindowStore((state) => state.windows);
  const open = useWindowStore((state) => state.open);
  const focus = useWindowStore((state) => state.focus);
  const snapshot = useKernelStore((state) => state.snapshot);
  const connected = useKernelStore((state) => state.connected);
  const logs = useKernelStore((state) => state.logs);
  const doTick = useKernelStore((state) => state.doTick);
  const launcherOpen = useShellStore((state) => state.launcherOpen);
  const toggleLauncher = useShellStore((state) => state.toggleLauncher);
  const searchOpen = useShellStore((state) => state.searchOpen);
  const openSearch = useShellStore((state) => state.openSearch);
  const notificationsOpen = useShellStore((state) => state.notificationsOpen);
  const toggleNotifications = useShellStore((state) => state.toggleNotifications);
  const trayOpen = useShellStore((state) => state.trayOpen);
  const toggleTray = useShellStore((state) => state.toggleTray);
  const lastRead = useShellStore((state) => state.lastReadIndex);
  const [time, setTime] = useState(() => new Date());

  useEffect(() => {
    const timer = setInterval(() => setTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  const activeId = Object.values(windows)
    .filter((win) => win.open && !win.minimized)
    .sort((a, b) => b.z - a.z)[0]?.id;

  const running = Object.values(windows)
    .filter((win) => win.open)
    .sort((a, b) => b.z - a.z);

  const unread = logs.reduce((count, entry) => (entry.index > lastRead ? count + 1 : count), 0);

  const activate = (id: WindowId) => {
    const win = windows[id];
    if (!win?.open || win.minimized) open(id);
    else focus(id);
  };

  return (
    <>
      <div className="relative z-40 flex h-11 shrink-0 items-center gap-2 border-t border-[#1e2a4a] bg-[#0d1225]/95 px-2.5 backdrop-blur-xl">
        <button
          onClick={toggleLauncher}
          className={`flex items-center gap-2 rounded-lg px-2 py-1.5 transition ${
            launcherOpen ? "bg-cyan-500/20" : "hover:bg-white/5"
          }`}
          title="Applications"
        >
          <span
            className="grid h-7 w-7 place-items-center rounded-lg mono text-xs font-bold text-black"
            style={{
              backgroundImage:
                "linear-gradient(135deg, var(--jarvis-primary), var(--jarvis-secondary))",
            }}
          >
            J
          </span>
          <span className="hidden mono text-xs font-semibold tracking-[0.2em] text-cyan-100 sm:inline">
            JARVIS
          </span>
        </button>

        <span className="h-6 w-px bg-[#1e2a4a]" />

        <div className="flex min-w-0 flex-1 items-center gap-1 overflow-x-auto">
          {running.length === 0 ? (
            <span className="mono text-[10px] tracking-widest text-white/20">NO OPEN WINDOWS</span>
          ) : (
            running.map((win) => {
              const app = APP_MAP[win.id];
              const isAgent = win.id.startsWith("agent-");
              const label = app?.label ?? win.title;
              const icon = app?.icon ?? "AI";
              return (
                <button
                  key={win.id}
                  onClick={() => activate(win.id)}
                  className={`flex shrink-0 items-center gap-1.5 rounded-lg px-2 py-1.5 mono text-[10px] tracking-wide transition ${
                    activeId === win.id
                      ? "bg-cyan-500/15 text-cyan-100"
                      : win.minimized
                        ? "text-white/35 hover:bg-white/5"
                        : "text-white/60 hover:bg-white/5 hover:text-white/85"
                  }`}
                  title={win.title}
                >
                  <span className={`h-1.5 w-1.5 rounded-full ${isAgent ? "bg-emerald-300" : app?.dot ?? "bg-cyan-400"}`} />
                  <span className="hidden xl:inline">{label}</span>
                  <span className="xl:hidden">{icon}</span>
                </button>
              );
            })
          )}
        </div>

        <div className="hidden items-center gap-2.5 mono text-[10px] lg:flex">
          <span className={`flex items-center gap-1.5 ${connected ? "text-emerald-300" : "text-amber-300"}`}>
            <span className={`h-1.5 w-1.5 rounded-full ${connected ? "animate-pulse bg-emerald-400" : "bg-amber-400"}`} />
            {connected ? "KERNEL LIVE" : "RECONNECTING"}
          </span>
          {snapshot && (
            <>
              <span className="text-white/15">·</span>
              <span className="text-cyan-300/80">{snapshot.processes} procs</span>
              <span className="text-violet-300/80">{snapshot.memory.stats.frames_used}/{snapshot.memory.config.total_frames} fr</span>
              <span className="text-emerald-300/80">{snapshot.agents.stats.live} agents</span>
            </>
          )}
          <button
            onClick={() => void doTick()}
            className="rounded bg-cyan-500/15 px-1.5 py-0.5 tracking-widest text-cyan-300/80 hover:bg-cyan-500/25"
            title="Advance virtual clock one tick"
          >
            TICK
          </button>
        </div>

        <div className="flex items-center gap-1">
          <button
            onClick={openSearch}
            className={`grid h-8 w-8 place-items-center rounded-lg mono text-sm transition ${
              searchOpen ? "bg-cyan-500/20 text-cyan-200" : "text-white/50 hover:bg-white/5 hover:text-white/85"
            }`}
            title="Global search (Ctrl+K)"
          >
            ⌕
          </button>
          <button
            onClick={toggleNotifications}
            className={`relative grid h-8 w-8 place-items-center rounded-lg transition ${
              notificationsOpen ? "bg-cyan-500/20 text-cyan-200" : "text-white/50 hover:bg-white/5 hover:text-white/85"
            }`}
            title="Notifications"
          >
            <span className="mono text-xs">◔</span>
            {unread > 0 && (
              <span className="absolute -right-0.5 -top-0.5 grid min-w-[15px] place-items-center rounded-full bg-red-500 px-1 mono text-[8px] font-bold text-white">
                {unread > 99 ? "99+" : unread}
              </span>
            )}
          </button>
          <button
            onClick={toggleTray}
            className={`grid h-8 w-8 place-items-center rounded-lg mono text-[10px] transition ${
              trayOpen ? "bg-cyan-500/20 text-cyan-200" : "text-white/50 hover:bg-white/5 hover:text-white/85"
            }`}
            title="Quick settings"
          >
            ◡
          </button>
        </div>

        <div className="ml-1 border-l border-[#1e2a4a] pl-2 text-right mono text-[10px] leading-tight">
          <div className="text-white/70">
            {time.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" })}
          </div>
          <div className="hidden text-white/25 sm:block">{time.toLocaleDateString()}</div>
        </div>

        <div className="ml-1 hidden items-center gap-0.5 lg:flex">
          <button
            onClick={() => window.jarvis?.minimize()}
            className="grid h-7 w-7 place-items-center rounded text-xs text-white/40 hover:bg-white/10 hover:text-white/70"
            title="Minimize JARVIS OS"
          >
            ─
          </button>
          <button
            onClick={() => window.jarvis?.maximize()}
            className="grid h-7 w-7 place-items-center rounded text-[10px] text-white/40 hover:bg-white/10 hover:text-white/70"
            title="Toggle fullscreen"
          >
            □
          </button>
          <button
            onClick={() => window.jarvis?.close()}
            className="grid h-7 w-7 place-items-center rounded text-xs text-white/40 hover:bg-red-500/20 hover:text-red-300"
            title="Close JARVIS OS"
          >
            ✕
          </button>
        </div>
      </div>

      <LauncherMenu />
      <TrayMenu />
    </>
  );
}
