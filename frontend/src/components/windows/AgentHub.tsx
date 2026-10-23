import { useState } from "react";
import { useKernelStore } from "@/store/kernelStore";
import { useWindowStore } from "@/store/windowStore";
import type { AgentDefinition, AgentInstance } from "@/api/types";

const STAGES = ["QUEUED", "PLANNING", "THINKING", "ACTING", "REPORTING", "DONE"];

function initials(name: string): string {
  const parts = name.trim().split(/\s+/);
  return ((parts[0]?.[0] ?? "A") + (parts[1]?.[0] ?? "")).toUpperCase();
}

function openConsole(pid: number, name: string) {
  useWindowStore.getState().open(`agent-${pid}`, `${name} Console`);
}

export default function AgentHub() {
  const snap = useKernelStore((s) => s.snapshot);
  const command = useKernelStore((s) => s.command);
  const open = useWindowStore((s) => s.open);
  const [busy, setBusy] = useState<string | null>(null);
  const [message, setMessage] = useState<string | null>(null);

  if (!snap) return <div className="p-6 mono text-xs text-white/40">Waiting for kernel snapshot...</div>;

  const agents = snap.agents;
  const st = agents.stats;

  const launch = async (def: AgentDefinition) => {
    setBusy(`launch-${def.key}`);
    try {
      const res = (await command({ action: "agent_launch", key: def.key })) as {
        ok: boolean;
        pid?: number;
        error?: string;
      };
      if (res.ok && res.pid) {
        openConsole(res.pid, def.name);
        setMessage(`${def.name} launched as pid ${res.pid}`);
      } else {
        setMessage(`launch failed: ${res.error ?? "unknown error"}`);
      }
    } finally {
      setBusy(null);
    }
  };

  const setRunState = async (inst: AgentInstance, action: "suspend_process" | "resume_process") => {
    setBusy(`${action}-${inst.pid}`);
    try {
      const res = (await command({ action, pid: inst.pid })) as { ok: boolean; error?: string };
      setMessage(res.ok ? `pid ${inst.pid} ${action === "suspend_process" ? "paused" : "resumed"}` : `failed: ${res.error}`);
    } finally {
      setBusy(null);
    }
  };

  const stop = async (inst: AgentInstance) => {
    setBusy(`kill-${inst.pid}`);
    try {
      const res = (await command({ action: "agent_kill", pid: inst.pid })) as {
        ok: boolean;
        error?: string;
      };
      setMessage(res.ok ? `pid ${inst.pid} terminated` : `kill failed: ${res.error}`);
    } finally {
      setBusy(null);
    }
  };

  const tick = async () => {
    await command({ action: "tick", count: 10 });
  };

  return (
    <div className="flex h-full flex-col bg-[#060a14]">
      <div className="grid grid-cols-3 gap-1.5 border-b border-[#1e2a4a] bg-[#0d1225] p-2 lg:grid-cols-6">
        {[
          ["AGENTS", `${st.live}/${st.definitions}`],
          ["LAUNCHED", String(st.launched)],
          ["QUEUED", String(st.tasks_queued)],
          ["DONE", String(st.tasks_done)],
          ["FAILED", String(st.tasks_failed)],
          ["REPORTS", String(st.reports)],
        ].map(([label, value]) => (
          <div key={label} className="rounded border border-[#1e2a4a] bg-[#141b34] p-1.5 mono text-xs">
            <div className="text-[9px] tracking-widest text-white/40">{label}</div>
            <div className={label === "FAILED" && st.tasks_failed > 0 ? "font-bold text-red-300" : "text-white"}>
              {value}
            </div>
          </div>
        ))}
      </div>

      <div className="flex items-center gap-2 border-b border-[#1e2a4a] px-2 py-1.5">
        <button
          onClick={() => open("agent-studio")}
          className="rounded bg-cyan-500/20 px-2.5 py-1 mono text-[10px] tracking-widest text-cyan-200 hover:bg-cyan-500/30"
        >
          + AGENT STUDIO
        </button>
        <button
          onClick={() => void tick()}
          className="rounded bg-violet-500/20 px-2.5 py-1 mono text-[10px] tracking-widest text-violet-200 hover:bg-violet-500/30"
        >
          STEP 10 TICKS
        </button>
        <span className="ml-auto mono text-[10px] text-white/40">
          live cap {agents.config.max_live} / task cap {agents.config.max_tasks}
        </span>
        {message && <span className="mono text-[10px] text-amber-200">{message}</span>}
      </div>

      <div className="flex-1 overflow-auto p-2">
        <div className="mb-2 mono text-[10px] tracking-widest text-white/35">INSTALLED AGENTS</div>
        <div className="grid grid-cols-1 gap-2 md:grid-cols-2 xl:grid-cols-3">
          {agents.definitions.map((def) => {
            const running = agents.instances.find((i) => i.pid === def.running_pid);
            return (
              <div
                key={def.key}
                className="rounded-lg border border-[#1e2a4a] bg-[#0d1225] p-2.5"
                style={{ borderTopColor: def.color, borderTopWidth: 2 }}
              >
                <div className="flex items-center gap-2">
                  <div
                    className="grid h-9 w-9 shrink-0 place-items-center rounded-lg border mono text-xs font-bold"
                    style={{ color: def.color, borderColor: `${def.color}66`, background: `${def.color}14` }}
                  >
                    {initials(def.name)}
                  </div>
                  <div className="min-w-0">
                    <div className="truncate mono text-xs font-semibold text-white/90">{def.name}</div>
                    <div className="mono text-[9px] tracking-widest text-white/35">
                      {def.builtin ? "BUILT-IN" : "CUSTOM"} / PRIO {def.priority} / {def.mem_pages} PAGES
                    </div>
                  </div>
                  <span
                    className={`ml-auto h-2 w-2 rounded-full ${running ? "bg-emerald-400" : "bg-white/20"}`}
                    title={running ? `running as pid ${running.pid}` : "idle"}
                  />
                </div>

                <div className="mt-1.5 line-clamp-2 min-h-[28px] mono text-[10px] leading-snug text-white/50">
                  {def.role}
                </div>

                <div className="mt-1.5 flex flex-wrap gap-1">
                  {def.tools.map((tool) => (
                    <span key={tool} className="rounded bg-white/5 px-1.5 py-0.5 mono text-[9px] text-cyan-200/70">
                      {tool}
                    </span>
                  ))}
                </div>

                {running && (
                  <div className="mt-2 rounded border border-emerald-400/20 bg-emerald-500/5 p-1.5 mono text-[10px]">
                    <div className="flex items-center gap-2 text-emerald-200">
                      <span>PID {running.pid}</span>
                      <span className="text-white/40">{running.activity}</span>
                      <span className="ml-auto text-white/40">
                        {running.tasks_done} done / {running.active_tasks} active
                      </span>
                    </div>
                    <div className="mt-1 h-1 overflow-hidden rounded bg-black/40">
                      <div
                        className="h-full bg-emerald-400"
                        style={{ width: `${running.current?.progress ?? 0}%` }}
                      />
                    </div>
                    <div className="mt-1 flex justify-between text-white/40">
                      <span>{running.current?.title ?? "no active task"}</span>
                      <span>{running.current?.stage ?? "IDLE"}</span>
                    </div>
                  </div>
                )}

                <div className="mt-2 flex gap-1.5">
                  {running ? (
                    <>
                      <button
                        onClick={() => openConsole(running.pid, running.name)}
                        className="flex-1 rounded bg-cyan-500/15 px-2 py-1 mono text-[10px] tracking-widest text-cyan-200 hover:bg-cyan-500/25"
                      >
                        CONSOLE
                      </button>
                      {running.state === "SUSPENDED" ? (
                        <button
                          onClick={() => void setRunState(running, "resume_process")}
                          disabled={busy === `resume_process-${running.pid}`}
                          className="rounded bg-amber-500/15 px-2 py-1 mono text-[10px] tracking-widest text-amber-200 hover:bg-amber-500/25 disabled:opacity-40"
                        >
                          RESUME
                        </button>
                      ) : (
                        <button
                          onClick={() => void setRunState(running, "suspend_process")}
                          disabled={busy === `suspend_process-${running.pid}`}
                          className="rounded bg-amber-500/15 px-2 py-1 mono text-[10px] tracking-widest text-amber-200 hover:bg-amber-500/25 disabled:opacity-40"
                        >
                          PAUSE
                        </button>
                      )}
                      <button
                        onClick={() => void stop(running)}
                        disabled={busy === `kill-${running.pid}`}
                        className="rounded bg-red-500/15 px-2 py-1 mono text-[10px] tracking-widest text-red-200 hover:bg-red-500/25 disabled:opacity-40"
                      >
                        STOP
                      </button>
                    </>
                  ) : (
                    <button
                      onClick={() => void launch(def)}
                      disabled={busy === `launch-${def.key}` || st.live >= agents.config.max_live}
                      className="flex-1 rounded bg-emerald-500/15 px-2 py-1 mono text-[10px] tracking-widest text-emerald-200 hover:bg-emerald-500/25 disabled:opacity-40"
                    >
                      LAUNCH
                    </button>
                  )}
                </div>
              </div>
            );
          })}
        </div>

        <div className="mb-2 mt-4 mono text-[10px] tracking-widest text-white/35">TASK QUEUE / PIPELINE</div>
        {agents.tasks.length === 0 ? (
          <div className="rounded border border-dashed border-[#1e2a4a] p-4 mono text-[11px] text-white/35">
            No tasks yet. Open an agent console and dispatch a task.
          </div>
        ) : (
          <div className="space-y-1">
            {agents.tasks.map((task) => (
              <div key={task.id} className="flex items-center gap-2 rounded border border-[#1e2a4a] bg-[#0d1225] px-2 py-1.5 mono text-[10px]">
                <span className="w-10 text-white/35">#{task.id}</span>
                <span className="w-28 truncate text-white/80">{task.agent}</span>
                <span className="min-w-0 flex-1 truncate text-white/55">{task.title}</span>
                <span className="hidden w-16 text-right text-cyan-200/70 sm:inline">{task.tool}</span>
                <span className={`w-20 text-right ${task.stage === "DONE" ? "text-emerald-300" : "text-amber-300"}`}>
                  {task.stage}
                </span>
                <div className="hidden h-1 w-20 overflow-hidden rounded bg-black/40 md:block">
                  <div className="h-full bg-cyan-400" style={{ width: `${task.progress}%` }} />
                </div>
                <span className="w-8 text-right text-white/40">{task.progress}%</span>
              </div>
            ))}
          </div>
        )}
      </div>

      <div className="border-t border-[#1e2a4a] bg-[#0d1225] px-2 py-1 mono text-[9px] text-white/30">
        pipeline: {STAGES.join(" > ")} / every stage consumes scheduled CPU ticks
      </div>
    </div>
  );
}
