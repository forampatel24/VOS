import { useState } from "react";
import { useKernelStore } from "@/store/kernelStore";

export default function InterruptCenter() {
  const snap = useKernelStore((s) => s.snapshot);
  const command = useKernelStore((s) => s.command);
  const [msg, setMsg] = useState<string | null>(null);

  if (!snap) return <div className="p-6 mono text-xs text-white/40">Waiting for kernel snapshot…</div>;

  const ic = snap.interrupts;
  const em = snap.error_manager;

  const trigger = async (irq: string) => {
    setMsg(null);
    const res = (await command({ action: "trigger_interrupt", irq, source: irq, detail: `manual ${irq}` })) as {
      ok: boolean;
      error?: string;
      pending?: number;
    };
    setMsg(res.ok ? `enqueued ${irq} — pending ${res.pending}` : `error: ${res.error}`);
  };

  const doPanic = async () => {
    const res = (await command({ action: "panic", reason: "manual panic from UI" })) as { ok: boolean };
    setMsg(res.ok ? "panic raised — check banner" : "panic failed");
  };

  const clearPanic = async () => {
    await command({ action: "clear_panic" });
    setMsg("panic cleared");
  };

  return (
    <div className="flex h-full flex-col">
      {ic.panic && (
        <div className="bg-red-500/20 border-b border-red-400/40 px-3 py-2 mono text-xs text-red-200 flex items-center justify-between">
          <span>⚠ KERNEL PANIC: {ic.panic_reason}</span>
          <button onClick={clearPanic} className="rounded bg-red-500/30 px-2 py-1 text-red-100 hover:bg-red-500/40">
            CLEAR
          </button>
        </div>
      )}

      {/* stats */}
      <div className="grid grid-cols-4 gap-2 p-2 border-b border-[#1e2a4a] bg-[#0d1225]">
        <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 mono text-xs text-center">
          <div className="text-white/40 text-[10px] tracking-widest">PENDING</div>
          <div className="text-amber-300 font-bold text-lg">{ic.pending}</div>
        </div>
        <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 mono text-xs text-center">
          <div className="text-white/40 text-[10px] tracking-widest">HANDLED</div>
          <div className="text-emerald-300 font-bold text-lg">{ic.handled}</div>
        </div>
        <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 mono text-xs text-center">
          <div className="text-white/40 text-[10px] tracking-widest">DROPPED</div>
          <div className={`font-bold text-lg ${ic.dropped > 0 ? "text-red-300" : "text-white/60"}`}>{ic.dropped}</div>
        </div>
        <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 mono text-xs text-center">
          <div className="text-white/40 text-[10px] tracking-widest">PANIC</div>
          <div className={`font-bold ${ic.panic ? "text-red-300" : "text-white/40"}`}>{ic.panic ? "YES" : "NO"}</div>
        </div>
      </div>

      {/* queue */}
      <div className="flex-1 overflow-auto">
        <div className="mono text-[10px] tracking-widest text-white/40 px-3 py-1 border-b border-[#1e2a4a]">PENDING QUEUE — priority order (0=highest: shutdown → page_fault → disk → keyboard → timer → software)</div>
        {ic.queue.length === 0 ? (
          <div className="mono text-xs text-white/30 p-6 text-center">No pending interrupts — trigger one below or cause a page fault via Memory Viewer.</div>
        ) : (
          <table className="w-full mono text-xs">
            <thead className="sticky top-0 bg-[#141b34] text-white/50">
              <tr>
                <th className="text-left font-normal tracking-widest p-2">IRQ</th>
                <th className="text-left font-normal tracking-widest p-2">PRIO</th>
                <th className="text-left font-normal tracking-widest p-2">SOURCE</th>
                <th className="text-left font-normal tracking-widest p-2">DETAIL</th>
                <th className="text-left font-normal tracking-widest p-2">TICKS</th>
                <th className="text-left font-normal tracking-widest p-2">STATUS</th>
              </tr>
            </thead>
            <tbody>
              {ic.queue.map((e, idx) => (
                <tr key={idx} className="border-t border-[#1e2a4a] hover:bg-white/[0.03]">
                  <td className="p-2">
                    <span className={`rounded px-1.5 py-0.5 text-[11px] ${e.irq === "shutdown" ? "bg-red-500/20 text-red-300" : e.irq === "page_fault" ? "bg-amber-500/20 text-amber-200" : "bg-cyan-500/15 text-cyan-200"}`}>{e.irq}</span>
                  </td>
                  <td className="p-2 text-white/70">{e.priority}</td>
                  <td className="p-2 text-white/80">{e.source}</td>
                  <td className="p-2 text-white/60 truncate max-w-[160px]">{e.detail || "—"}</td>
                  <td className="p-2 text-white/40">{e.enqueued_ticks}</td>
                  <td className="p-2 text-white/50">{e.status}</td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>

      {/* controls */}
      <div className="border-t border-[#1e2a4a] bg-[#0d1225] p-2 space-y-2">
        <div className="mono text-[10px] tracking-widest text-white/40">TRIGGER — enqueues an interrupt (serviced on next tick, one at a time)</div>
        <div className="flex flex-wrap gap-1.5">
          {["page_fault", "disk", "keyboard", "timer", "software", "shutdown"].map((irq) => (
            <button
              key={irq}
              onClick={() => trigger(irq)}
              className={`mono rounded px-2.5 py-1 text-xs tracking-wide border ${irq === "shutdown" ? "bg-red-500/20 border-red-400/30 text-red-200 hover:bg-red-500/30" : irq === "page_fault" ? "bg-amber-500/15 border-amber-400/30 text-amber-200 hover:bg-amber-500/25" : "bg-white/10 border-[#1e2a4a] text-white/70 hover:bg-white/15"}`}
            >
              {irq}
            </button>
          ))}
          <button onClick={doPanic} className="mono rounded bg-red-500/20 border border-red-400/30 px-2.5 py-1 text-xs tracking-wide text-red-200">
            PANIC
          </button>
        </div>
        {msg && <div className="mono text-xs text-cyan-200 bg-cyan-500/10 border border-cyan-400/20 rounded px-2 py-1">{msg}</div>}

        <div className="mono text-xs text-white/40 border-t border-[#1e2a4a] pt-2">
          <div className="text-white/60 tracking-widest text-[10px]">ERROR MANAGER — last: {em.last_category} — {em.last_message || "none"}</div>
          <div className="flex flex-wrap gap-2 mt-1">
            {Object.entries(em.counts).map(([k, v]) => (
              <span key={k} className="text-white/30">
                <span className="text-white/60">{k}</span> {String(v)}
              </span>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}
