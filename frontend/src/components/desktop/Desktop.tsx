import { AnimatePresence } from "framer-motion";
import { useWindowStore } from "@/store/windowStore";
import Window from "./Window";
import SystemMonitor from "@/components/windows/SystemMonitor";
import ProcessManager from "@/components/windows/ProcessManager";
import MemoryViewer from "@/components/windows/MemoryViewer";
import CpuScheduler from "@/components/windows/CpuScheduler";
import EventLog from "@/components/windows/EventLog";
import Placeholder from "@/components/windows/Placeholder";

export default function Desktop() {
  const { windows, close } = useWindowStore();

  return (
    <div className="absolute inset-0 overflow-hidden bg-[#060a14]">
      {/* wallpaper — subtle grid + radial glow */}
      <div className="absolute inset-0 bg-[radial-gradient(ellipse_at_top,_rgba(0,255,255,0.08),transparent_60%),radial-gradient(ellipse_at_bottom_right,_rgba(124,58,237,0.08),transparent_60%)]" />
      <div className="absolute inset-0 opacity-[0.03] bg-[linear-gradient(to_right,rgba(255,255,255,0.6)_1px,transparent_1px),linear-gradient(to_bottom,rgba(255,255,255,0.6)_1px,transparent_1px)] bg-[size:40px_40px]" />

      {/* desktop icons — quick launch */}
      <div className="absolute left-4 top-4 flex flex-col gap-3 z-0">
        {[
          { id: "system-monitor", label: "System\nMonitor", icon: "◈" },
          { id: "processes", label: "Processes", icon: "▦" },
          { id: "memory", label: "Memory", icon: "⬢" },
          { id: "cpu", label: "CPU", icon: "⬣" },
          { id: "eventlog", label: "Event Log", icon: "≡" },
        ].map((item) => (
          <button
            key={item.id}
            onDoubleClick={() => useWindowStore.getState().open(item.id as never)}
            className="flex flex-col items-center gap-1 rounded-lg p-2 hover:bg-white/5 transition w-20"
            title="Double-click to open"
          >
            <div className="h-10 w-10 rounded-xl bg-[#141b34] border border-[#1e2a4a] grid place-items-center text-cyan-300 mono text-sm shadow-[0_4px_20px_rgba(0,0,0,0.4)]">
              {item.icon}
            </div>
            <span className="mono text-[10px] tracking-wide text-white/60 text-center whitespace-pre leading-tight">{item.label}</span>
          </button>
        ))}
      </div>

      {/* windows */}
      <AnimatePresence>
        {windows["system-monitor"].open && !windows["system-monitor"].minimized && (
          <Window id="system-monitor" title="System Monitor" onClose={() => close("system-monitor")}>
            <SystemMonitor />
          </Window>
        )}
        {windows.processes.open && !windows.processes.minimized && (
          <Window id="processes" title="Process Manager" onClose={() => close("processes")}>
            <ProcessManager />
          </Window>
        )}
        {windows.memory.open && !windows.memory.minimized && (
          <Window id="memory" title="Memory Viewer" onClose={() => close("memory")}>
            <MemoryViewer />
          </Window>
        )}
        {windows.cpu.open && !windows.cpu.minimized && (
          <Window id="cpu" title="CPU & Scheduler" onClose={() => close("cpu")}>
            <CpuScheduler />
          </Window>
        )}
        {windows.eventlog.open && !windows.eventlog.minimized && (
          <Window id="eventlog" title="Event Log" onClose={() => close("eventlog")}>
            <EventLog />
          </Window>
        )}
        {windows.files.open && !windows.files.minimized && (
          <Window id="files" title="File Explorer" onClose={() => close("files")}>
            <Placeholder title="File Explorer" milestone="M5 — File System & Disk" />
          </Window>
        )}
        {windows.devices.open && !windows.devices.minimized && (
          <Window id="devices" title="Device Manager" onClose={() => close("devices")}>
            <Placeholder title="Device Manager" milestone="M6 — Devices & Drivers" />
          </Window>
        )}
        {windows.interrupts.open && !windows.interrupts.minimized && (
          <Window id="interrupts" title="Interrupt Center" onClose={() => close("interrupts")}>
            <Placeholder title="Interrupt Center" milestone="M4 — Interrupt Controller" />
          </Window>
        )}
        {windows.terminal.open && !windows.terminal.minimized && (
          <Window id="terminal" title="Terminal" onClose={() => close("terminal")}>
            <Placeholder title="Terminal" milestone="M8 — Shell & CLI" />
          </Window>
        )}
        {windows.settings.open && !windows.settings.minimized && (
          <Window id="settings" title="Settings" onClose={() => close("settings")}>
            <Placeholder title="Settings" milestone="M10b — Backend API Full" />
          </Window>
        )}
      </AnimatePresence>
    </div>
  );
}
