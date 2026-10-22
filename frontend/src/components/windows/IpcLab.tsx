import { useState } from "react";
import { useKernelStore } from "@/store/kernelStore";
import type { IpcQueue, IpcSegment, IpcPipe } from "@/api/types";
import IpcOps from "./IpcOps";
import type { IpcKind } from "./IpcOps";

const pids = (list: number[]) => (list.length === 0 ? "—" : list.join(", "));

export default function IpcLab() {
  const snap = useKernelStore((s) => s.snapshot);
  const command = useKernelStore((s) => s.command);
  const [selection, setSelection] = useState<{ kind: IpcKind; name: string } | null>(null);
  const [msg, setMsg] = useState<string | null>(null);

  if (!snap) return <div className="p-6 mono text-xs text-white/40">Waiting for kernel snapshot…</div>;

  const ipc = snap.ipc;
  const st = ipc.stats;
  const graph = ipc.wait_graph;

  const pick = (kind: IpcKind, name: string) => setSelection({ kind, name });

  const detect = async () => {
    const res = (await command({ action: "ipc_deadlock" })) as {
      ok: boolean;
      deadlock?: boolean;
      cycle_count?: number;
    };
    setMsg(res.deadlock ? `deadlock detected — ${res.cycle_count} cycle(s)` : "no deadlock: wait-for graph is acyclic");
  };

  const recover = async (pid: number) => {
    const res = (await command({ action: "ipc_recover", pid })) as {
      ok: boolean;
      error?: string;
      recovered?: number;
    };
    setMsg(res.ok ? `recovered — killed pid ${res.recovered}` : `error: ${res.error}`);
  };

  const rowBase = "mb-1 cursor-pointer rounded border p-1.5 transition";

  const queueRow = (q: IpcQueue) => (
    <div
      key={`q-${q.name}`}
      onClick={() => pick("queue", q.name)}
      className={`${rowBase} ${
        selection?.kind === "queue" && selection.name === q.name
          ? "border-cyan-400/40 bg-cyan-500/10"
          : "border-[#1e2a4a] bg-[#0d1225] hover:bg-white/5"
      }`}
    >
      <div className="flex items-center gap-2 mono text-xs">
        <span className="text-cyan-300">▤</span>
        <span className="text-white/85 font-semibold">{q.name}</span>
        <span className="ml-auto text-white/40">
          {q.count}/{q.capacity}
        </span>
      </div>
      <div className="mt-0.5 flex flex-wrap gap-2 mono text-[10px] text-white/35">
        <span>owner {q.owner}</span>
        <span>sent {q.sent}</span>
        <span>recv {q.received}</span>
        <span className={q.senders.length ? "text-amber-300" : ""}>senders {pids(q.senders)}</span>
        <span className={q.receivers.length ? "text-amber-300" : ""}>receivers {pids(q.receivers)}</span>
      </div>
    </div>
  );

  const segmentRow = (s: IpcSegment) => (
    <div
      key={`s-${s.name}`}
      onClick={() => pick("shm", s.name)}
      className={`${rowBase} ${
        selection?.kind === "shm" && selection.name === s.name
          ? "border-violet-400/40 bg-violet-500/10"
          : "border-[#1e2a4a] bg-[#0d1225] hover:bg-white/5"
      }`}
    >
      <div className="flex items-center gap-2 mono text-xs">
        <span className="text-violet-300">⬢</span>
        <span className="text-white/85 font-semibold">{s.name}</span>
        <span className="ml-auto text-white/40">
          {s.length}/{s.size} B
        </span>
      </div>
      <div className="mt-0.5 truncate mono text-[10px] text-cyan-200/60">{s.data || "(empty)"}</div>
      <div className="mt-0.5 flex gap-2 mono text-[10px] text-white/35">
        <span>owner {s.owner}</span>
        <span>maps {s.maps}</span>
        <span>r/w {s.reads}/{s.writes}</span>
        <span>attached {pids(s.attached)}</span>
      </div>
    </div>
  );

  const pipeRow = (p: IpcPipe) => (
    <div
      key={`p-${p.name}`}
      onClick={() => pick("pipe", p.name)}
      className={`${rowBase} ${
        selection?.kind === "pipe" && selection.name === p.name
          ? "border-emerald-400/40 bg-emerald-500/10"
          : "border-[#1e2a4a] bg-[#0d1225] hover:bg-white/5"
      }`}
    >
      <div className="flex items-center gap-2 mono text-xs">
        <span className="text-emerald-300">⇄</span>
        <span className="text-white/85 font-semibold">{p.name}</span>
        <span className="ml-auto text-white/40">
          {p.count}/{p.capacity} B
        </span>
      </div>
      <div className="mt-0.5 flex flex-wrap gap-2 mono text-[10px] text-white/35">
        <span>readers {pids(p.readers)}</span>
        <span>writers {pids(p.writers)}</span>
        <span>r/w {p.reads}/{p.writes}</span>
        <span className={p.read_waiters.length ? "text-amber-300" : ""}>r-wait {pids(p.read_waiters)}</span>
        <span className={p.write_waiters.length ? "text-amber-300" : ""}>w-wait {pids(p.write_waiters)}</span>
      </div>
    </div>
  );

  return (
    <div className="flex h-full flex-col">
      <div className="grid grid-cols-3 gap-1.5 p-2 border-b border-[#1e2a4a] bg-[#0d1225] lg:grid-cols-6">
        {[
          ["MSG", `${st.messages_sent}/${st.messages_received}`],
          ["SHM MAPS", String(st.shm_maps)],
          ["PIPE B", String(st.pipe_bytes)],
          ["BLOCKED", String(st.blocked)],
          ["WOKEN", String(st.woken)],
          ["DEADLOCKS", String(st.deadlocks)],
        ].map(([label, value]) => (
          <div key={label} className="rounded bg-[#141b34] border border-[#1e2a4a] p-1.5 mono text-xs">
            <div className="text-white/40 tracking-widest text-[9px]">{label}</div>
            <div className={label === "DEADLOCKS" && st.deadlocks > 0 ? "text-red-300 font-bold" : "text-white"}>
              {value}
            </div>
          </div>
        ))}
      </div>

      <div className="flex min-h-0 flex-1">
        <div className="w-1/2 overflow-auto border-r border-[#1e2a4a] p-1.5 space-y-1.5">
          <div>
            <div className="mono text-[10px] tracking-widest text-white/40 mb-1">QUEUES · cap {ipc.config.queue_capacity}</div>
            {ipc.queues.length === 0 ? (
              <div className="mono text-[11px] text-white/25 p-1">none</div>
            ) : (
              ipc.queues.map(queueRow)
            )}
          </div>
          <div>
            <div className="mono text-[10px] tracking-widest text-white/40 mb-1">SHARED MEMORY · {ipc.config.shm_bytes} B</div>
            {ipc.segments.length === 0 ? (
              <div className="mono text-[11px] text-white/25 p-1">none</div>
            ) : (
              ipc.segments.map(segmentRow)
            )}
          </div>
          <div>
            <div className="mono text-[10px] tracking-widest text-white/40 mb-1">PIPES · cap {ipc.config.pipe_capacity} B</div>
            {ipc.pipes.length === 0 ? (
              <div className="mono text-[11px] text-white/25 p-1">none</div>
            ) : (
              ipc.pipes.map(pipeRow)
            )}
          </div>
        </div>

        <div className="w-1/2 overflow-auto p-1.5 space-y-1.5">
          <div>
            <div className="mono text-[10px] tracking-widest text-white/40 mb-1">MUTEXES</div>
            {ipc.mutexes.map((m) => (
              <div
                key={m.name}
                onClick={() => pick("mutex", m.name)}
                className={`${rowBase} ${
                  selection?.kind === "mutex" && selection.name === m.name
                    ? "border-amber-400/40 bg-amber-500/10"
                    : "border-[#1e2a4a] bg-[#0d1225] hover:bg-white/5"
                }`}
              >
                <div className="flex items-center gap-2 mono text-xs">
                  <span className="text-amber-300">◫</span>
                  <span className="text-white/85 font-semibold">{m.name}</span>
                  <span className="ml-auto text-white/40">
                    {m.owner === 0 ? "free" : `held by ${m.owner}`}
                    {m.recursion > 1 ? ` ×${m.recursion}` : ""}
                  </span>
                </div>
                <div className="mt-0.5 flex gap-2 mono text-[10px] text-white/35">
                  <span>locks {m.locks}</span>
                  <span>contend {m.contentions}</span>
                  <span className={m.waiters.length ? "text-amber-300" : ""}>wait {pids(m.waiters)}</span>
                </div>
              </div>
            ))}
          </div>

          <div>
            <div className="mono text-[10px] tracking-widest text-white/40 mb-1">SEMAPHORES</div>
            {ipc.semaphores.map((s) => (
              <div
                key={s.name}
                onClick={() => pick("semaphore", s.name)}
                className={`${rowBase} ${
                  selection?.kind === "semaphore" && selection.name === s.name
                    ? "border-pink-400/40 bg-pink-500/10"
                    : "border-[#1e2a4a] bg-[#0d1225] hover:bg-white/5"
                }`}
              >
                <div className="flex items-center gap-2 mono text-xs">
                  <span className="text-pink-300">◍</span>
                  <span className="text-white/85 font-semibold">{s.name}</span>
                  <span className="ml-auto text-white/40">
                    {s.value}/{s.max}
                  </span>
                </div>
                <div className="mt-0.5 flex gap-2 mono text-[10px] text-white/35">
                  <span>holders {pids(s.holders)}</span>
                  <span className={s.waiters.length ? "text-amber-300" : ""}>wait {pids(s.waiters)}</span>
                </div>
              </div>
            ))}
          </div>

          <div className="rounded border border-[#1e2a4a] bg-[#0d1225] p-2">
            <div className="flex items-center gap-2">
              <span className="mono text-[10px] tracking-widest text-white/40">WAIT-FOR GRAPH</span>
              <span className={`ml-auto rounded border px-1.5 py-0.5 mono text-[9px] ${graph.active ? "border-red-400/40 bg-red-500/15 text-red-200" : "border-emerald-400/30 bg-emerald-500/10 text-emerald-300"}`}>
                {graph.active ? "DEADLOCK" : "ACYCLIC"}
              </span>
              <button onClick={detect} className="rounded bg-white/10 px-2 py-0.5 mono text-[10px] text-white/80 hover:bg-white/15">
                DETECT
              </button>
            </div>
            <div className="mt-1 mono text-[10px] text-white/50 space-y-0.5">
              {graph.edges.length === 0 ? (
                <div className="text-white/25">no blocked waiters</div>
              ) : (
                graph.edges.map((e, i) => (
                  <div key={i}>
                    <span className="text-amber-200">{e.pid}</span> → <span className="text-cyan-200">{e.target}</span>{" "}
                    <span className="text-white/30">({e.kind} {e.resource})</span>
                  </div>
                ))
              )}
            </div>
            {graph.cycles.length > 0 && (
              <div className="mt-1.5 space-y-1">
                {graph.cycles.map((cycle, i) => (
                  <div key={i} className="flex items-center gap-1.5 rounded border border-red-400/30 bg-red-500/10 px-1.5 py-1 mono text-[10px]">
                    <span className="text-red-200">cycle {i + 1}: {cycle.join(" → ")}</span>
                    <button
                      onClick={() => recover(cycle[0])}
                      className="ml-auto rounded bg-red-500/30 px-1.5 py-0.5 text-red-100 hover:bg-red-500/40"
                    >
                      RECOVER
                    </button>
                  </div>
                ))}
              </div>
            )}
            {ipc.deliveries.length > 0 && (
              <div className="mt-1.5 mono text-[10px] text-white/40">
                pending deliveries:{" "}
                {ipc.deliveries.map((d) => (
                  <span key={d.pid} className="mr-2 text-violet-200">
                    pid {d.pid} ({d.bytes} B)
                  </span>
                ))}
              </div>
            )}
            {msg && <div className="mt-1.5 mono text-[10px] text-cyan-200">{msg}</div>}
          </div>
        </div>
      </div>

      <IpcOps selection={selection} />
    </div>
  );
}
