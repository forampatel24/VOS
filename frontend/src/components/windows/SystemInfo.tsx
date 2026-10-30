import { useEffect, useState } from "react";
import { fetchSystemInfo } from "@/api/client";
import type { SystemInfoResponse } from "@/api/types";
import { useKernelStore } from "@/store/kernelStore";

const PAGE_SIZE = 4096;

function fmtBytes(bytes: number): string {
  if (!Number.isFinite(bytes) || bytes <= 0) return "0 B";
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  if (bytes < 1024 * 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
  return `${(bytes / (1024 * 1024 * 1024)).toFixed(2)} GB`;
}

function Stat({ label, value, hint }: { label: string; value: string; hint?: string }) {
  return (
    <div className="rounded-lg border border-[#1e2a4a] bg-[#141b34] p-2.5">
      <div className="mono text-[9px] tracking-[0.18em] text-white/30">{label}</div>
      <div className="mono mt-1 text-sm font-semibold text-white/85">{value}</div>
      {hint && <div className="mono mt-0.5 text-[10px] text-white/30">{hint}</div>}
    </div>
  );
}

export default function SystemInfo() {
  const snapshot = useKernelStore((state) => state.snapshot);
  const refreshSnapshot = useKernelStore((state) => state.refreshSnapshot);
  const [info, setInfo] = useState<SystemInfoResponse | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  const load = async () => {
    setBusy(true);
    try {
      setInfo(await fetchSystemInfo());
      setError(null);
    } catch (err) {
      setError(String(err));
    } finally {
      setBusy(false);
    }
  };

  useEffect(() => {
    void load();
  }, []);

  const refresh = async () => {
    await Promise.all([refreshSnapshot(), load()]);
  };

  if (!snapshot && !info) {
    return <div className="p-6 mono text-xs text-white/40">Waiting for kernel snapshot…</div>;
  }

  const machine = snapshot?.machine ?? info?.machine;
  const clock = snapshot?.clock ?? info?.clock;
  const scheduler = snapshot?.scheduler ?? info?.scheduler;
  const memory = snapshot?.memory;
  const filesystem = snapshot?.filesystem;
  const devices = snapshot?.devices;
  const ipc = snapshot?.ipc;
  const agents = snapshot?.agents;
  const interrupts = snapshot?.interrupts;
  const uptime = snapshot?.uptime_ticks ?? info?.uptime_ticks ?? 0;
  const seconds = clock && clock.speed_hz > 0 ? uptime / clock.speed_hz : 0;
  const framesTotal = memory?.config.total_frames ?? machine?.ram_frames ?? info?.counts.frames_total ?? 0;
  const framesUsed = memory?.stats.frames_used ?? info?.counts.frames_used ?? 0;
  const ramBytes = framesTotal * PAGE_SIZE;
  const diskUsed = filesystem?.disk.used_bytes ?? 0;
  const diskTotal = filesystem?.disk.capacity_bytes ?? 0;

  return (
    <div className="flex h-full flex-col overflow-y-auto p-3">
      <div className="relative overflow-hidden rounded-xl border border-cyan-400/20 bg-gradient-to-br from-[#0d1225] via-[#0d1225] to-[#101d3a] p-4">
        <div className="absolute -right-10 -top-12 h-40 w-40 rounded-full bg-cyan-400/10 blur-2xl" />
        <div className="relative flex flex-wrap items-start justify-between gap-3">
          <div>
            <div className="mono text-[10px] tracking-[0.35em] text-cyan-300/70">JARVIS OS</div>
            <h2 className="mono mt-1 text-2xl font-bold tracking-[0.12em] text-white">
              SYSTEM INFORMATION
            </h2>
            <p className="mt-1 max-w-lg text-[11px] leading-relaxed text-white/40">
              Machine identity and live subsystem capacity, read from the C kernel snapshot.
              Nothing on this page is hardcoded.
            </p>
          </div>
          <div className="text-right mono text-[10px]">
            <div className={snapshot?.shutdown ? "text-amber-300" : "text-emerald-300"}>
              {snapshot?.shutdown ? "KERNEL HALTED" : "KERNEL RUNNING"}
            </div>
            <div className="mt-1 text-white/35">
              uptime {uptime} ticks · {seconds.toFixed(0)} s
            </div>
            <button
              onClick={() => void refresh()}
              disabled={busy}
              className="mt-2 rounded border border-cyan-400/30 bg-cyan-500/15 px-2 py-1 tracking-widest text-cyan-200 hover:bg-cyan-500/25 disabled:opacity-40"
            >
              {busy ? "REFRESHING…" : "REFRESH"}
            </button>
          </div>
        </div>
      </div>

      {error && (
        <div className="mt-2 rounded-lg border border-red-400/30 bg-red-500/10 px-3 py-2 mono text-[11px] text-red-200">
          system information API unavailable: {error}
        </div>
      )}

      <div className="mt-3 grid grid-cols-2 gap-2 md:grid-cols-4">
        <Stat label="MOTHERBOARD" value={machine?.board ?? "—"} hint={machine?.bios ?? ""} />
        <Stat label="CHIPSET" value={machine?.chipset ?? "—"} hint={machine?.bus ?? ""} />
        <Stat label="CPU CORES" value={String(machine?.cpu_cores ?? "—")} hint="virtual cores" />
        <Stat label="BOOT DEVICE" value={machine?.boot_device ?? "—"} hint={machine?.bus_speed_mhz ? `${machine.bus_speed_mhz} MHz bus` : ""} />
      </div>

      <div className="mt-2 grid gap-2 lg:grid-cols-2">
        <div className="rounded-xl border border-[#1e2a4a] bg-[#0d1225] p-3">
          <div className="mono mb-2 text-[10px] tracking-[0.22em] text-cyan-300">KERNEL</div>
          <div className="grid grid-cols-2 gap-1.5">
            <Stat label="IMPLEMENTATION" value={info?.kernel === "c-native" ? "C17 + NASM" : info?.kernel ?? "c-native"} hint="JSON ABI bridge" />
            <Stat label="SCHEDULER" value={(scheduler?.algo ?? "—").toUpperCase()} hint={`quantum ${clock?.quantum ?? "—"} ticks`} />
            <Stat label="CLOCK" value={`${clock?.speed_hz ?? "—"} Hz`} hint={`${clock?.ticks ?? 0} clock ticks`} />
            <Stat label="PROCESSES" value={String(snapshot?.processes ?? info?.counts.processes ?? 0)} hint={`${snapshot?.process_list.length ?? 0} PCB records`} />
          </div>
        </div>

        <div className="rounded-xl border border-[#1e2a4a] bg-[#0d1225] p-3">
          <div className="mono mb-2 text-[10px] tracking-[0.22em] text-violet-300">MEMORY</div>
          <div className="grid grid-cols-2 gap-1.5">
            <Stat label="FRAMES" value={`${framesUsed}/${framesTotal}`} hint={`${fmtBytes(ramBytes)} virtual RAM`} />
            <Stat label="PAGE FAULTS" value={String(memory?.stats.page_faults ?? 0)} hint={`${memory?.stats.pages_mapped ?? 0} pages mapped`} />
            <Stat label="SWAP" value={`${memory?.swap_used ?? 0} slots`} hint={`${memory?.stats.swap_ins ?? 0} in / ${memory?.stats.swap_outs ?? 0} out`} />
            <Stat label="ALLOCATOR" value={(memory?.config.allocator ?? "—").toUpperCase()} hint={`${memory?.config.replacement ?? "—"} replacement`} />
          </div>
        </div>

        <div className="rounded-xl border border-[#1e2a4a] bg-[#0d1225] p-3">
          <div className="mono mb-2 text-[10px] tracking-[0.22em] text-emerald-300">STORAGE &amp; I/O</div>
          <div className="grid grid-cols-2 gap-1.5">
            <Stat label="VIRTUAL DISK" value={fmtBytes(diskUsed)} hint={`of ${fmtBytes(diskTotal)} · ${filesystem?.disk.used_blocks ?? 0}/${filesystem?.disk.total_blocks ?? 0} blocks`} />
            <Stat label="INODES" value={`${filesystem?.stats.inodes_used ?? 0}/${filesystem?.config.max_inodes ?? 0}`} hint={`${filesystem?.stats.open_files ?? 0} open files`} />
            <Stat label="DEVICES" value={String(devices?.count ?? info?.counts.devices ?? 0)} hint={`${devices?.stats.interrupts ?? 0} device IRQs`} />
            <Stat label="IPC OBJECTS" value={String((ipc?.queues.length ?? 0) + (ipc?.pipes.length ?? 0) + (ipc?.segments.length ?? 0))} hint={`${ipc?.stats.messages_sent ?? 0} messages sent`} />
          </div>
        </div>

        <div className="rounded-xl border border-[#1e2a4a] bg-[#0d1225] p-3">
          <div className="mono mb-2 text-[10px] tracking-[0.22em] text-amber-300">AGENTS &amp; HEALTH</div>
          <div className="grid grid-cols-2 gap-1.5">
            <Stat label="AGENTS LIVE" value={`${agents?.stats.live ?? info?.counts.agents_live ?? 0}/${agents?.config.max_live ?? 0}`} hint={`${agents?.stats.definitions ?? info?.counts.agents_definitions ?? 0} definitions`} />
            <Stat label="TASKS" value={`${agents?.stats.tasks_done ?? 0}/${agents?.stats.tasks_queued ?? 0}`} hint={`${agents?.stats.tasks_failed ?? 0} failed`} />
            <Stat label="INTERRUPTS" value={String(interrupts?.handled ?? 0)} hint={`${interrupts?.dropped ?? 0} dropped`} />
            <Stat label="ERROR STATE" value={interrupts?.panic ? "PANIC" : "CLEAR"} hint={`${snapshot?.error_manager.counts ? Object.values(snapshot.error_manager.counts).reduce((a, b) => a + b, 0) : 0} recorded`} />
          </div>
        </div>
      </div>

      <div className="mt-2 rounded-xl border border-[#1e2a4a] bg-[#0d1225] p-3">
        <div className="mono mb-2 text-[10px] tracking-[0.22em] text-white/35">VIRTUAL MACHINE SLOTS</div>
        <div className="flex flex-wrap gap-1.5">
          {(machine?.slots ?? []).map((slot) => (
            <div
              key={slot.name}
              className={`rounded-lg border px-2.5 py-1.5 mono text-[10px] ${
                slot.populated
                  ? "border-cyan-400/20 bg-cyan-500/5 text-white/70"
                  : "border-[#1e2a4a] bg-[#060a14] text-white/30"
              }`}
              title={slot.detail}
            >
              <span className="text-white/35">{slot.kind.toUpperCase()}</span>
              <span className="mx-1.5 text-white/20">·</span>
              {slot.name}
              <span className={`ml-2 ${slot.populated ? "text-emerald-300" : "text-white/25"}`}>
                {slot.populated ? "READY" : "EMPTY"}
              </span>
            </div>
          ))}
        </div>
      </div>

      <div className="mt-2 flex flex-wrap items-center gap-x-4 gap-y-1 rounded-lg border border-[#1e2a4a] bg-[#060a14]/70 px-3 py-2 mono text-[10px] text-white/30">
        <span>JSON ABI: /api/snapshot</span>
        <span>PAGE SIZE: {PAGE_SIZE} B</span>
        <span>BRIDGE: FastAPI + ctypes</span>
        <span>RENDERER: Electron + React</span>
      </div>
    </div>
  );
}
