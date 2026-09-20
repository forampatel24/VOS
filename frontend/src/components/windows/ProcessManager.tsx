import { useState } from "react";
import { useKernelStore } from "@/store/kernelStore";

export default function ProcessManager() {
  const snap = useKernelStore((s) => s.snapshot);
  const command = useKernelStore((s) => s.command);
  const [name, setName] = useState("agent_" + Math.floor(Math.random() * 900 + 100));
  const [prio, setPrio] = useState("0");
  const [burst, setBurst] = useState("5");
  const [busy, setBusy] = useState<number | null>(null);
  const [error, setError] = useState<string | null>(null);

  if (!snap) return <div className="p-6 mono text-xs text-white/40">Waiting for kernel snapshot…</div>;

  const doCreate = async () => {
    setError(null);
    const payload: Record<string, unknown> = { action: "create_process", name };
    const p = parseInt(prio, 10);
    const b = parseInt(burst, 10);
    if (!isNaN(p)) payload.priority = p;
    if (!isNaN(b) && b > 0) payload.burst_time = b;
    const res = (await command(payload)) as { ok: boolean; error?: string };
    if (!res.ok) setError(res.error ?? "create failed");
  };

  const act = async (action: string, pid: number) => {
    setBusy(pid);
    setError(null);
    const res = (await command({ action, pid })) as { ok: boolean; error?: string };
    if (!res.ok) setError(res.error ?? `${action} failed`);
    setBusy(null);
  };

  return (
    <div className="flex h-full flex-col">
      {/* toolbar */}
      <div className="flex flex-wrap items-center gap-2 border-b border-[#1e2a4a] bg-[#0d1225] p-2">
        <input
          value={name}
          onChange={(e) => setName(e.target.value)}
          placeholder="process name"
          className="mono h-7 w-28 rounded border border-[#1e2a4a] bg-[#060a14] px-2 text-xs text-white placeholder:text-white/30 focus:border-cyan-400/50 focus:outline-none"
        />
        <label className="flex items-center gap-1 mono text-xs text-white/60">
          PRIO
          <input value={prio} onChange={(e) => setPrio(e.target.value)} className="w-12 h-7 rounded border border-[#1e2a4a] bg-[#060a14] px-1 text-xs text-white" />
        </label>
        <label className="flex items-center gap-1 mono text-xs text-white/60">
          BT
          <input value={burst} onChange={(e) => setBurst(e.target.value)} className="w-12 h-7 rounded border border-[#1e2a4a] bg-[#060a14] px-1 text-xs text-white" />
        </label>
        <button onClick={doCreate} className="mono h-7 rounded bg-cyan-500 px-3 text-xs font-semibold tracking-widest text-black hover:bg-cyan-400 transition">
          CREATE
        </button>
        <span className="mono text-xs text-white/30">
          {snap.processes} live · {snap.process_list.length} PCBs
        </span>
        {error && <span className="mono text-xs text-red-300">{error}</span>}
      </div>

      {/* table */}
      <div className="flex-1 overflow-auto">
        <table className="w-full mono text-xs">
          <thead className="sticky top-0 bg-[#141b34] text-white/50">
            <tr>
              <th className="text-left font-normal tracking-widest p-2">PID</th>
              <th className="text-left font-normal tracking-widest p-2">NAME</th>
              <th className="text-left font-normal tracking-widest p-2">STATE</th>
              <th className="text-left font-normal tracking-widest p-2">PRIO</th>
              <th className="text-left font-normal tracking-widest p-2">BT</th>
              <th className="text-left font-normal tracking-widest p-2">AT</th>
              <th className="text-right font-normal tracking-widest p-2">ACTIONS</th>
            </tr>
          </thead>
          <tbody>
            {snap.process_list.length === 0 ? (
              <tr>
                <td colSpan={7} className="p-8 text-center text-white/30">
                  No processes — create one to see the lifecycle (READY → SUSPENDED → TERMINATED)
                </td>
              </tr>
            ) : (
              snap.process_list.map((p) => (
                <tr key={p.pid} className="border-t border-[#1e2a4a] hover:bg-white/[0.03]">
                  <td className="p-2 text-cyan-300">{p.pid}</td>
                  <td className="p-2 text-white truncate max-w-[160px]">{p.name}</td>
                  <td className="p-2">
                    <span
                      className={`rounded px-1.5 py-0.5 text-[11px] tracking-wide ${
                        p.state === "READY"
                          ? "bg-emerald-500/20 text-emerald-300"
                          : p.state === "SUSPENDED"
                          ? "bg-violet-500/20 text-violet-300"
                          : p.state === "WAITING"
                          ? "bg-amber-500/20 text-amber-300"
                          : "bg-white/10 text-white/60"
                      }`}
                    >
                      {p.state}
                    </span>
                  </td>
                  <td className="p-2 text-white/70">{p.priority}</td>
                  <td className="p-2 text-white/60">{p.burst_time || "—"}</td>
                  <td className="p-2 text-white/40">{p.created_ticks}</td>
                  <td className="p-2">
                    <div className="flex justify-end gap-1">
                      <button
                        disabled={busy === p.pid || p.state !== "READY"}
                        onClick={() => act("suspend_process", p.pid)}
                        className="rounded bg-white/10 px-2 py-1 text-[11px] tracking-wide text-white/80 hover:bg-white/15 disabled:opacity-30"
                      >
                        PAUSE
                      </button>
                      <button
                        disabled={busy === p.pid || p.state !== "SUSPENDED"}
                        onClick={() => act("resume_process", p.pid)}
                        className="rounded bg-white/10 px-2 py-1 text-[11px] tracking-wide text-white/80 hover:bg-white/15 disabled:opacity-30"
                      >
                        RESUME
                      </button>
                      <button
                        disabled={busy === p.pid}
                        onClick={() => act("kill_process", p.pid)}
                        className="rounded bg-red-500/20 px-2 py-1 text-[11px] tracking-wide text-red-300 hover:bg-red-500/30 disabled:opacity-30"
                      >
                        KILL
                      </button>
                    </div>
                  </td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>

      {/* queues footer */}
      <div className="border-t border-[#1e2a4a] bg-[#0d1225] p-2 mono text-xs">
        <div className="flex flex-wrap gap-2">
          {(["ready", "waiting", "suspended", "terminated"] as const).map((q) => (
            <span key={q} className="text-white/40">
              <span className="text-white/60">{q}</span> [{snap.queues[q].join(", ") || "—"}]
            </span>
          ))}
        </div>
      </div>
    </div>
  );
}
