import { AnimatePresence } from "framer-motion";
import { useWindowStore, type FixedWindowId, type WindowId } from "@/store/windowStore";
import { useShellStore } from "@/store/shellStore";
import { APP_CATALOG } from "@/lib/apps";
import Window from "./Window";
import Dock from "./Dock";
import SystemMonitor from "@/components/windows/SystemMonitor";
import ProcessManager from "@/components/windows/ProcessManager";
import MemoryViewer from "@/components/windows/MemoryViewer";
import CpuScheduler from "@/components/windows/CpuScheduler";
import EventLog from "@/components/windows/EventLog";
import InterruptCenter from "@/components/windows/InterruptCenter";
import FileExplorer from "@/components/windows/FileExplorer";
import DeviceManager from "@/components/windows/DeviceManager";
import IpcLab from "@/components/windows/IpcLab";
import Terminal from "@/components/windows/Terminal";
import AgentHub from "@/components/windows/AgentHub";
import AgentStudio from "@/components/windows/AgentStudio";
import AgentConsole from "@/components/windows/AgentConsole";
import Settings from "@/components/windows/Settings";
import SystemInfo from "@/components/windows/SystemInfo";

const FIXED: { id: FixedWindowId; title: string; render: () => React.ReactNode }[] = [
  { id: "system-monitor", title: "System Monitor", render: () => <SystemMonitor /> },
  { id: "processes", title: "Process Manager", render: () => <ProcessManager /> },
  { id: "memory", title: "Memory Viewer", render: () => <MemoryViewer /> },
  { id: "cpu", title: "CPU & Scheduler", render: () => <CpuScheduler /> },
  { id: "eventlog", title: "Event Log", render: () => <EventLog /> },
  { id: "files", title: "File Explorer", render: () => <FileExplorer /> },
  { id: "devices", title: "Device Manager", render: () => <DeviceManager /> },
  { id: "interrupts", title: "Interrupt Center", render: () => <InterruptCenter /> },
  { id: "ipc", title: "IPC & Sync Lab", render: () => <IpcLab /> },
  { id: "terminal", title: "Terminal", render: () => <Terminal /> },
  { id: "agent-hub", title: "Agent Hub", render: () => <AgentHub /> },
  { id: "agent-studio", title: "Agent Studio", render: () => <AgentStudio /> },
  { id: "settings", title: "Settings", render: () => <Settings /> },
  { id: "system-info", title: "System Information", render: () => <SystemInfo /> },
];

export default function Desktop() {
  const windows = useWindowStore((s) => s.windows);
  const close = useWindowStore((s) => s.close);

  const consoles = Object.values(windows).filter(
    (w) => w.open && !w.minimized && /^agent-\d+$/.test(w.id)
  );

  return (
    <div className="absolute inset-0 overflow-hidden bg-[#060a14]">
      <div
        className="absolute inset-0"
        style={{
          background:
            "radial-gradient(ellipse at top, color-mix(in srgb, var(--jarvis-primary) 10%, transparent), transparent 60%), radial-gradient(ellipse at bottom right, color-mix(in srgb, var(--jarvis-accent) 10%, transparent), transparent 60%)",
        }}
      />
      <div className="absolute inset-0 opacity-[0.03] bg-[linear-gradient(to_right,rgba(255,255,255,0.6)_1px,transparent_1px),linear-gradient(to_bottom,rgba(255,255,255,0.6)_1px,transparent_1px)] bg-[size:40px_40px]" />

      <div className="absolute left-3 top-3 z-0 grid grid-flow-col grid-rows-7 gap-1.5">
        {APP_CATALOG.map((app) => (
          <button
            key={app.id}
            onDoubleClick={() => useWindowStore.getState().open(app.id)}
            onContextMenu={(event) => {
              event.preventDefault();
              useShellStore.getState().togglePin(app.id);
            }}
            className="flex w-[68px] flex-col items-center gap-0.5 rounded-lg p-1 transition hover:bg-white/5"
            title={`${app.label} — double-click to open · right-click to pin`}
          >
            <div className="grid h-9 w-9 place-items-center rounded-xl border border-[#1e2a4a] bg-[#141b34] mono text-[10px] font-bold text-cyan-300 shadow-[0_4px_20px_rgba(0,0,0,0.4)]">
              {app.icon}
            </div>
            <span className="whitespace-pre text-center mono text-[9px] leading-tight tracking-wide text-white/60">
              {app.short}
            </span>
          </button>
        ))}
      </div>

      <AnimatePresence>
        {FIXED.map(({ id, title, render }) => {
          const w = windows[id];
          if (!w?.open || w.minimized) return null;
          return (
            <Window key={id} id={id} title={title} onClose={() => close(id)}>
              {render()}
            </Window>
          );
        })}
        {consoles.map((w) => {
          const pid = Number(w.id.slice("agent-".length));
          const id = w.id as WindowId;
          return (
            <Window key={id} id={id} title={w.title} onClose={() => close(id)}>
              <AgentConsole pid={pid} />
            </Window>
          );
        })}
      </AnimatePresence>

      <Dock />
    </div>
  );
}
