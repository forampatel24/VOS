import { useKernelStore } from "@/store/kernelStore";

export default function CpuScheduler() {
  const snap = useKernelStore((s) => s.snapshot);
  const command = useKernelStore((s) => s.command);
  if (!snap) return <div className="p-6 mono text-xs text-white/40">Waiting for kernel snapshot…</div>;

  const regs = snap.cpu.registers;

  return (
    <div className="flex h-full flex-col gap-2 p-2">
      {/* CPU state */}
      <div className="rounded-lg border border-[#1e2a4a] bg-[#0d1225] p-3">
        <div className="mono text-xs font-semibold tracking-widest text-amber-300 mb-2">CPU STATE</div>
        <div className="grid grid-cols-4 gap-2 mono text-xs">
          <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 text-center">
            <div className="text-white/40 text-[10px] tracking-widest">PC</div>
            <div className="text-white font-bold">{snap.cpu.pc}</div>
          </div>
          <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 text-center">
            <div className="text-white/40 text-[10px] tracking-widest">SP</div>
            <div className="text-white font-bold">{snap.cpu.sp}</div>
          </div>
          <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 text-center">
            <div className="text-white/40 text-[10px] tracking-widest">IR</div>
            <div className="text-white font-bold">{snap.cpu.ir}</div>
          </div>
          <div className={`rounded border p-2 text-center ${snap.cpu.halted ? "bg-amber-500/15 border-amber-400/30" : "bg-emerald-500/15 border-emerald-400/30"}`}>
            <div className="text-[10px] tracking-widest opacity-60">HALTED</div>
            <div className={`font-bold ${snap.cpu.halted ? "text-amber-300" : "text-emerald-300"}`}>{snap.cpu.halted ? "YES" : "NO"}</div>
          </div>
        </div>

        <div className="mt-3 grid grid-cols-4 gap-1.5">
          {Array.from({ length: 8 }, (_, i) => (
            <div key={i} className="rounded bg-[#060a14] border border-[#1e2a4a] px-2 py-1.5 text-center mono text-xs">
              <span className="text-white/40">R{i}</span> <span className="text-cyan-200 font-bold">{regs[`R${i}`] ?? 0}</span>
            </div>
          ))}
        </div>

        <div className="mt-2 flex gap-2 mono text-xs">
          {Object.entries(snap.cpu.flags).map(([k, v]) => (
            <span key={k} className={`rounded px-2 py-0.5 border ${v ? "bg-cyan-500/20 border-cyan-400/30 text-cyan-200" : "bg-[#141b34] border-[#1e2a4a] text-white/30"}`}>
              {k} {v ? "1" : "0"}
            </span>
          ))}
          <span className="ml-auto text-white/30">program: {snap.cpu.program_size} words</span>
        </div>

        <div className="mt-3 flex gap-2">
          <button
            onClick={() => command({ action: "cpu_step" })}
            className="mono rounded bg-white/10 px-3 py-1.5 text-xs tracking-widest text-white/80 hover:bg-white/15"
          >
            STEP
          </button>
          <button
            onClick={() => command({ action: "cpu_reset" })}
            className="mono rounded bg-white/10 px-3 py-1.5 text-xs tracking-widest text-white/80 hover:bg-white/15"
          >
            RESET
          </button>
        </div>
      </div>

      {/* scheduler */}
      <div className="rounded-lg border border-[#1e2a4a] bg-[#0d1225] p-3">
        <div className="mono text-xs font-semibold tracking-widest text-emerald-300 mb-2">SCHEDULER — ROUND ROBIN</div>
        <div className="mono text-xs space-y-2">
          <div className="flex justify-between text-white/60">
            <span>Quantum</span>
            <span className="text-white">{snap.clock.quantum} ticks</span>
          </div>
          <div>
            <div className="text-white/40 text-[10px] tracking-widest mb-1">READY QUEUE — drives next SCHEDULE</div>
            <div className="flex flex-wrap gap-1">
              {snap.queues.ready.length === 0 ? (
                <span className="text-white/20">empty — no runnable process</span>
              ) : (
                snap.queues.ready.map((pid, idx) => (
                  <span
                    key={pid}
                    className={`rounded px-2 py-1 border text-xs ${idx === 0 ? "bg-emerald-500/20 border-emerald-400/40 text-emerald-200" : "bg-[#141b34] border-[#1e2a4a] text-white/60"}`}
                  >
                    {pid}
                    {idx === 0 && " ← next"}
                  </span>
                ))
              )}
            </div>
          </div>
          <div className="mono text-[10px] text-white/30">
            Scheduling is real round-robin over the ready queue; algorithm switching arrives with the scheduler-strategy milestone.
          </div>
        </div>
      </div>

      {/* clock */}
      <div className="rounded-lg border border-[#1e2a4a] bg-[#0d1225] p-3 mono text-xs">
        <div className="text-cyan-300 font-semibold tracking-widest mb-1">CLOCK</div>
        <div className="flex justify-between text-white/60">
          <span>Speed</span>
          <span className="text-white">{snap.clock.speed_hz} Hz</span>
        </div>
        <div className="flex justify-between text-white/60">
          <span>Ticks</span>
          <span className="text-white">{snap.clock.ticks}</span>
        </div>
        <div className="flex justify-between text-white/60">
          <span>Uptime ticks</span>
          <span className="text-white">{snap.uptime_ticks}</span>
        </div>
      </div>
    </div>
  );
}
