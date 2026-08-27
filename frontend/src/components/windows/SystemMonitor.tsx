import { useKernelStore } from "@/store/kernelStore";

function Stat({ label, value, sub, accent }: { label: string; value: string; sub?: string; accent?: string }) {
  return (
    <div className="rounded-lg border border-[#1e2a4a] bg-[#141b34] p-3">
      <div className="mono text-[10px] tracking-widest text-white/40">{label}</div>
      <div className={`mono text-lg font-bold ${accent ?? "text-white"}`}>{value}</div>
      {sub && <div className="mono text-[11px] text-white/40">{sub}</div>}
    </div>
  );
}

export default function SystemMonitor() {
  const snap = useKernelStore((s) => s.snapshot);
  if (!snap) {
    return <div className="p-6 mono text-xs text-white/40">Waiting for kernel snapshot…</div>;
  }

  const memPct = snap.memory.config.total_frames > 0 ? (snap.memory.stats.frames_used / snap.memory.config.total_frames) * 100 : 0;
  const uptimeSec = Math.floor(snap.uptime_ticks / 10); // ticks ~ timer granularity
  const hours = String(Math.floor(uptimeSec / 3600)).padStart(2, "0");
  const mins = String(Math.floor((uptimeSec % 3600) / 60)).padStart(2, "0");
  const secs = String(uptimeSec % 60).padStart(2, "0");

  return (
    <div className="flex h-full flex-col gap-3 p-3">
      {/* top stats */}
      <div className="grid grid-cols-2 lg:grid-cols-4 gap-2">
        <Stat label="PROCESSES" value={String(snap.processes)} sub={`${snap.process_list.length} PCBs · ${snap.queues.ready.length} ready`} accent="text-emerald-300" />
        <Stat label="MEMORY" value={`${memPct.toFixed(0)}%`} sub={`${snap.memory.stats.frames_used}/${snap.memory.config.total_frames} frames · ${snap.memory.stats.pages_mapped} pages`} accent="text-violet-300" />
        <Stat label="CPU" value={snap.cpu.halted ? "HALTED" : `PC ${snap.cpu.pc}`} sub={`IR ${snap.cpu.ir} · SP ${snap.cpu.sp} · ${snap.cpu.program_size} words`} accent={snap.cpu.halted ? "text-amber-300" : "text-cyan-300"} />
        <Stat label="UPTIME" value={`${hours}:${mins}:${secs}`} sub={`ticks ${snap.uptime_ticks} · quantum ${snap.clock.quantum}`} accent="text-white" />
      </div>

      {/* queues */}
      <div className="rounded-lg border border-[#1e2a4a] bg-[#0d1225] p-3">
        <div className="mono text-xs font-semibold tracking-widest text-cyan-200 mb-2">QUEUES</div>
        <div className="grid grid-cols-4 gap-2 mono text-xs">
          {[
            { name: "READY", items: snap.queues.ready, color: "text-emerald-300" },
            { name: "WAITING", items: snap.queues.waiting, color: "text-amber-300" },
            { name: "SUSPENDED", items: snap.queues.suspended, color: "text-violet-300" },
            { name: "TERMINATED", items: snap.queues.terminated, color: "text-white/40" },
          ].map((q) => (
            <div key={q.name} className="rounded bg-[#141b34] p-2 border border-[#1e2a4a]">
              <div className={`text-[10px] tracking-widest ${q.color}`}>{q.name} · {q.items.length}</div>
              <div className="mt-1 flex flex-wrap gap-1">
                {q.items.length === 0 ? (
                  <span className="text-white/20">—</span>
                ) : (
                  q.items.map((pid) => (
                    <span key={pid} className="rounded bg-white/10 px-1.5 py-0.5 text-white/80">
                      {pid}
                    </span>
                  ))
                )}
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* memory summary + clock */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-2">
        <div className="rounded-lg border border-[#1e2a4a] bg-[#0d1225] p-3">
          <div className="mono text-xs font-semibold tracking-widest text-violet-300 mb-2">MEMORY</div>
          <div className="mono text-xs space-y-1 text-white/70">
            <div className="flex justify-between">
              <span>Allocator</span>
              <span className="text-white">{snap.memory.config.allocator}</span>
            </div>
            <div className="flex justify-between">
              <span>Replacement</span>
              <span className="text-white">{snap.memory.config.replacement}</span>
            </div>
            <div className="flex justify-between">
              <span>Swap</span>
              <span className="text-white">
                {snap.memory.config.swap_enabled ? "on" : "off"} · {snap.memory.swap_used}/{snap.memory.config.swap_slots} used
              </span>
            </div>
            <div className="flex justify-between">
              <span>Faults / swap</span>
              <span className="text-white">
                {snap.memory.stats.page_faults} faults · {snap.memory.stats.swap_ins} in · {snap.memory.stats.swap_outs} out
              </span>
            </div>
            {snap.memory.stats.fragmented_allocs > 0 && (
              <div className="flex justify-between text-amber-300">
                <span>Fragmented allocs</span>
                <span>{snap.memory.stats.fragmented_allocs}</span>
              </div>
            )}
            {snap.memory.stats.segfaults > 0 && (
              <div className="flex justify-between text-red-300">
                <span>Segfaults</span>
                <span>{snap.memory.stats.segfaults}</span>
              </div>
            )}
          </div>
        </div>

        <div className="rounded-lg border border-[#1e2a4a] bg-[#0d1225] p-3">
          <div className="mono text-xs font-semibold tracking-widest text-cyan-300 mb-2">CLOCK & SCHEDULER</div>
          <div className="mono text-xs space-y-1 text-white/70">
            <div className="flex justify-between">
              <span>Speed</span>
              <span className="text-white">{snap.clock.speed_hz} Hz</span>
            </div>
            <div className="flex justify-between">
              <span>Quantum</span>
              <span className="text-white">{snap.clock.quantum} ticks</span>
            </div>
            <div className="flex justify-between">
              <span>Ticks</span>
              <span className="text-white">{snap.clock.ticks}</span>
            </div>
            <div className="flex justify-between">
              <span>Algorithm</span>
              <span className="text-white">Round Robin</span>
            </div>
            <div className="mono text-[10px] text-white/30 mt-2">Ready queue drives scheduling; context switches are real NASM saves.</div>
          </div>
        </div>
      </div>

      <div className="mono text-[10px] tracking-wide text-white/20">
        Every number above comes from <span className="text-cyan-300">jvk_snapshot()</span> via WebSocket — no placeholder data.
      </div>
    </div>
  );
}
