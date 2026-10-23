import { AnimatePresence } from "framer-motion";
import { useWindowStore, type FixedWindowId, type WindowId } from "@/store/windowStore";
import Window from "./Window";
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
import Placeholder from "@/components/windows/Placeholder";

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
  { id: "settings", title: "Settings", render: () => <Placeholder title="Settings" milestone="M10b - Backend API Full" /> },
];

const ICONS: { id: FixedWindowId; label: string; icon: string }[] = [
  { id: "system-monitor", label: "System", icon: "SM" },
  { id: "processes", label: "Processes", icon: "PS" },
  { id: "memory", label: "Memory", icon: "MM" },
  { id: "cpu", label: "CPU", icon: "CPU" },
  { id: "eventlog", label: "Logs", icon: "LG" },
  { id: "files", label: "Files", icon: "FS" },
  { id: "devices", label: "Devices", icon: "DV" },
  { id: "interrupts", label: "IRQs", icon: "IRQ" },
  { id: "ipc", label: "IPC", icon: "IPC" },
  { id: "terminal", label: "Terminal", icon: ">_" },
  { id: "agent-hub", label: "Agent Hub", icon: "AI" },
  { id: "agent-studio", label: "Studio", icon: "AS" },
];

export default function Desktop() {
  const windows = useWindowStore((s) => s.windows);
  const close = useWindowStore((s) => s.close);

  const consoles = Object.values(windows).filter(
    (w) => w.open && !w.minimized && /^agent-\d+$/.test(w.id)
  );

  return (
    <div className="absolute inset-0 overflow-hidden bg-[#060a14]">
      <div className="absolute inset-0 bg-[radial-gradient(ellipse_at_top,_rgba(0,255,255,0.08),transparent_60%),radial-gradient(ellipse_at_bottom_right,_rgba(124,58,237,0.08),transparent_60%)]" />
      <div className="absolute inset-0 opacity-[0.03] bg-[linear-gradient(to_right,rgba(255,255,255,0.6)_1px,transparent_1px),linear-gradient(to_bottom,rgba(255,255,255,0.6)_1px,transparent_1px)] bg-[size:40px_40px]" />

      <div className="absolute left-3 top-3 z-0 grid grid-flow-col grid-rows-7 gap-1.5">
        {ICONS.map((item) => (
          <button
            key={item.id}
            onDoubleClick={() => useWindowStore.getState().open(item.id)}
            className="flex w-[68px] flex-col items-center gap-0.5 rounded-lg p-1 transition hover:bg-white/5"
            title="Double-click to open"
          >
            <div className="grid h-9 w-9 place-items-center rounded-xl border border-[#1e2a4a] bg-[#141b34] mono text-[10px] font-bold text-cyan-300 shadow-[0_4px_20px_rgba(0,0,0,0.4)]">
              {item.icon}
            </div>
            <span className="whitespace-pre text-center mono text-[9px] leading-tight tracking-wide text-white/60">
              {item.label}
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
    </div>
  );
}
