import { useState } from "react";
import { useKernelStore } from "@/store/kernelStore";
import { useWindowStore } from "@/store/windowStore";
import type { AgentTask } from "@/api/types";

const STAGES = ["QUEUED", "PLANNING", "THINKING", "ACTING", "REPORTING", "DONE"];

function stageColor(stage: string): string {
  if (stage === "DONE") return "#34d399";
  if (stage === "FAILED" || stage === "CANCELLED") return "#f87171";
  if (stage === "ACTING") return "#c084fc";
  if (stage === "REPORTING") return "#fbbf24";
  if (stage === "THINKING") return "#60a5fa";
  if (stage === "PLANNING") return "#22d3ee";
  return "#94a3b8";
}

const field = "rounded border border-[#1e2a4a] bg-[#0d1225] px-2 py-1.5 text-xs text-white/90 outline-none focus:border-cyan-400/60";

export default function AgentConsole({ pid }: { pid: number }) {
  const snap = useKernelStore((s) => s.snapshot);
  const command = useKernelStore((s) => s.command);
  const close = useWindowStore((s) => s.close);
  const [title, setTitle] = useState("");
  const [input, setInput] = useState("");
  const [msg, setMsg] = useState<string | null>(null);
  const [selected, setSelected] = useState<number | null>(null);

  if (!snap) return <div className="p-6 mono text-xs text-white/40">Waiting for kernel snapshot...</div>;

  const inst = snap.agents.instances.find((i) => i.pid === pid);
  const def = inst ? snap.agents.definitions.find((d) => d.id === inst.def_id) : undefined;
  const tasks = snap.agents.tasks.filter((t) => t.pid === pid);
  const active = tasks.find((t) => t.id === selected) ?? tasks.find((t) => t.id === inst?.current_task) ?? tasks[tasks.length - 1];
  const color = def?.color ?? "#22d3ee";

  if (!inst) {
    return (
      <div className="flex h-full flex-col items-center justify-center gap-2 bg-[#060a14] p-6 text-center">
        <div className="mono text-xs tracking-widest text-white/50">AGENT PROCESS {pid} IS NOT RUNNING</div>
        <div className="mono text-[10px] text-white/30">
          The console stays attached to the pid. Relaunch from Agent Hub to get a new console.
        </div>
        <button
          onClick={() => {
            const id = `agent-${pid}` as const;
            close(id);
          }}
          className="mt-2 rounded bg-white/5 px-3 py-1 mono text-[10px] tracking-widest text-white/60 hover:bg-white/10"
        >
          CLOSE
        </button>
      </div>
    );
  }

  const submit = async () => {
    if (!title.trim()) {
      setMsg("task title is required");
      return;
    }
    const res = (await command({ action: "agent_task", pid, title: title.trim(), input: input.trim() })) as {
      ok: boolean;
      task_id?: number;
      error?: string;
    };
    if (res.ok) {
      setTitle("");
      setInput("");
      setMsg(`task ${res.task_id ?? ""} queued`);
    } else {
      setMsg(`dispatch failed: ${res.error ?? "unknown error"}`);
    }
  };

  const cancel = async (task: AgentTask) => {
    const res = (await command({ action: "agent_cancel", pid, task_id: task.id })) as {
      ok: boolean;
      error?: string;
    };
    setMsg(res.ok ? `task ${task.id} cancelled` : `cancel failed: ${res.error}`);
  };

  const tick = async (count: number) => {
    await command({ action: "tick", count });
  };

  const kill = async () => {
    const res = (await command({ action: "agent_kill", pid })) as { ok: boolean; error?: string };
    setMsg(res.ok ? `pid ${pid} terminated` : `kill failed: ${res.error}`);
  };

  return (
    <div className="flex h-full flex-col bg-[#060a14]">
      <div className="flex items-center gap-2 border-b border-[#1e2a4a] bg-[#0d1225] px-3 py-2">
        <span
          className="grid h-8 w-8 place-items-center rounded-lg border mono text-[10px] font-bold"
          style={{ color, borderColor: `${color}66`, background: `${color}14` }}
        >
          {def?.icon.slice(0, 3).toUpperCase() ?? "AI"}
        </span>
        <div>
          <div className="mono text-xs font-semibold text-white/90">{inst.name}</div>
          <div className="mono text-[9px] tracking-widest text-white/35">
            PID {pid} / {inst.state} / {inst.activity} / PRIO {inst.priority}
          </div>
        </div>
        <div className="ml-auto flex items-center gap-1.5">
          <button onClick={() => void tick(1)} className="rounded bg-violet-500/15 px-2 py-1 mono text-[10px] text-violet-200 hover:bg-violet-500/25">
            +1 TICK
          </button>
          <button onClick={() => void tick(10)} className="rounded bg-violet-500/15 px-2 py-1 mono text-[10px] text-violet-200 hover:bg-violet-500/25">
            +10
          </button>
          <button onClick={() => void kill()} className="rounded bg-red-500/15 px-2 py-1 mono text-[10px] text-red-200 hover:bg-red-500/25">
            TERMINATE
          </button>
        </div>
      </div>

      <div className="grid grid-cols-2 gap-1.5 border-b border-[#1e2a4a] px-3 py-1.5 md:grid-cols-4">
        {[
          ["CPU TICKS", String(inst.cpu_ticks)],
          ["MEMORY", `${inst.mem_pages} pages`],
          ["GROWTH", `${inst.memory_growth}/2`],
          ["TASKS", `${inst.tasks_done} done / ${inst.tasks_queued} queued`],
        ].map(([k, v]) => (
          <div key={k} className="rounded border border-[#1e2a4a] bg-[#0d1225] px-1.5 py-1 mono text-[10px]">
            <span className="text-white/35">{k} </span>
            <span className="text-white/80">{v}</span>
          </div>
        ))}
      </div>

      <div className="flex-1 overflow-auto p-2">
        <div className="rounded border border-[#1e2a4a] bg-[#0d1225] p-2">
          <div className="mono text-[10px] tracking-widest text-white/35">DISPATCH TASK</div>
          <div className="mt-1 flex flex-col gap-1.5 md:flex-row">
            <input
              value={title}
              onChange={(e) => setTitle(e.target.value)}
              maxLength={48}
              placeholder="Task title"
              className={`${field} md:w-56`}
            />
            <input
              value={input}
              onChange={(e) => setInput(e.target.value)}
              maxLength={192}
              onKeyDown={(e) => {
                if (e.key === "Enter") void submit();
              }}
              placeholder="Input / instructions"
              className={`${field} flex-1`}
            />
            <button
              onClick={() => void submit()}
              disabled={inst.active_tasks >= snap.agents.config.max_tasks}
              className="rounded bg-emerald-500/20 px-3 py-1.5 mono text-[10px] tracking-widest text-emerald-200 hover:bg-emerald-500/30 disabled:opacity-40"
            >
              QUEUE
            </button>
          </div>
          {msg && <div className="mt-1 mono text-[10px] text-amber-200">{msg}</div>}
        </div>

        <div className="mt-2 rounded border border-[#1e2a4a] bg-[#0d1225] p-2">
          <div className="flex items-center gap-2">
            <span className="mono text-[10px] tracking-widest text-white/35">PIPELINE</span>
            <span className="ml-auto mono text-[10px] text-white/40">
              current: {inst.current?.title ?? "idle"}
            </span>
          </div>
          <div className="mt-1.5 flex items-center gap-1">
            {STAGES.map((stage, i) => {
              const current = inst.current?.stage === stage || (!inst.current && stage === "QUEUED");
              return (
                <div key={stage} className="flex flex-1 items-center gap-1">
                  <div
                    className="flex-1 rounded px-1 py-1 text-center mono text-[9px] tracking-wider"
                    style={{
                      color: current ? stageColor(stage) : "#64748b",
                      background: current ? `${stageColor(stage)}18` : "#0a1020",
                      border: `1px solid ${current ? stageColor(stage) + "55" : "#1e2a4a"}`,
                    }}
                  >
                    {stage}
                  </div>
                  {i < STAGES.length - 1 && <span className="mono text-[9px] text-white/20">&gt;</span>}
                </div>
              );
            })}
          </div>
          <div className="mt-1.5 h-1.5 overflow-hidden rounded bg-black/50">
            <div
              className="h-full transition-all"
              style={{ width: `${inst.current?.progress ?? 0}%`, background: stageColor(inst.current?.stage ?? "QUEUED") }}
            />
          </div>
        </div>

        <div className="mt-2 grid grid-cols-1 gap-2 xl:grid-cols-5">
          <div className="rounded border border-[#1e2a4a] bg-[#0d1225] p-2 xl:col-span-2">
            <div className="mono text-[10px] tracking-widest text-white/35">QUEUE / HISTORY</div>
            {tasks.length === 0 ? (
              <div className="mt-2 mono text-[10px] text-white/30">No tasks dispatched to this agent.</div>
            ) : (
              <div className="mt-1 space-y-1">
                {tasks.map((task) => (
                  <button
                    key={task.id}
                    onClick={() => setSelected(task.id)}
                    className={`w-full rounded border px-1.5 py-1 text-left ${
                      active?.id === task.id ? "border-cyan-400/50 bg-cyan-500/10" : "border-[#1e2a4a] hover:bg-white/5"
                    }`}
                  >
                    <div className="flex items-center gap-1.5 mono text-[10px]">
                      <span className="text-white/35">#{task.id}</span>
                      <span className="min-w-0 flex-1 truncate text-white/75">{task.title}</span>
                      <span style={{ color: stageColor(task.stage) }}>{task.stage}</span>
                    </div>
                    <div className="mt-0.5 flex items-center gap-1.5 mono text-[9px] text-white/35">
                      <span>{task.tool}</span>
                      <span className="ml-auto">{task.progress}%</span>
                      {task.stage !== "DONE" && task.stage !== "FAILED" && task.stage !== "CANCELLED" && (
                        <span
                          role="button"
                          onClick={(e) => {
                            e.stopPropagation();
                            void cancel(task);
                          }}
                          className="rounded bg-red-500/10 px-1 text-red-200 hover:bg-red-500/20"
                        >
                          CANCEL
                        </span>
                      )}
                    </div>
                  </button>
                ))}
              </div>
            )}
          </div>

          <div className="rounded border border-[#1e2a4a] bg-[#0d1225] p-2 xl:col-span-3">
            {!active ? (
              <div className="mono text-[10px] text-white/30">Select a task to inspect the agent's reasoning trace.</div>
            ) : (
              <>
                <div className="flex items-center gap-2">
                  <span className="mono text-[10px] tracking-widest text-white/35">
                    TASK #{active.id} / {active.tool}
                  </span>
                  <span className="ml-auto mono text-[10px]" style={{ color: stageColor(active.stage) }}>
                    {active.stage} {active.progress}%
                  </span>
                </div>
                <div className="mt-1 space-y-1 mono text-[10px] leading-snug">
                  <div><span className="text-cyan-300">PLAN </span><span className="text-white/65">{active.plan || "-"}</span></div>
                  <div><span className="text-blue-300">THINK </span><span className="text-white/65">{active.thought || "-"}</span></div>
                  <div><span className="text-violet-300">ACT </span><span className="text-white/65">{active.action || "-"}</span></div>
                  <div><span className="text-amber-300">RESULT </span><span className="text-white/65">{active.result || "-"}</span></div>
                  <div className="text-white/40">
                    input: {active.input || "-"} / disk writes {active.writes} / reads {active.reads}
                  </div>
                </div>
                <div className="mt-1.5 rounded border border-[#1e2a4a] bg-black/30 p-1.5">
                  <div className="mono text-[9px] tracking-widest text-white/30">REPORT</div>
                  <pre className="mt-0.5 max-h-40 overflow-auto whitespace-pre-wrap break-words mono text-[10px] text-white/70">
                    {active.report || "not generated yet - pipeline is still running"}
                  </pre>
                </div>
              </>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
