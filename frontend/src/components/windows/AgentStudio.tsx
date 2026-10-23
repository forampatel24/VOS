import { useEffect, useState } from "react";
import { useKernelStore } from "@/store/kernelStore";
import { useWindowStore } from "@/store/windowStore";
import {
  createAgentProfile,
  deleteAgentProfile,
  fetchAgentProfiles,
  updateAgentProfile,
} from "@/api/client";
import type { AgentProfile, AgentProfileInput } from "@/api/types";

const TOOL_CHOICES = [
  "query_budget", "compute_total", "read_spec", "generate_code", "run_tests",
  "search_files", "read_file", "summarize", "write_draft", "rewrite",
  "onboarding_checklist", "policy_lookup", "extract_clauses",
  "compliance_check", "brainstorm", "campaign_report", "plan_itinerary",
  "booking_check", "wellness_checklist", "reminder",
];

const EMPTY: AgentProfileInput = {
  name: "",
  role: "",
  personality: "Balanced",
  color: "#22d3ee",
  icon: "bot",
  tools: ["read_file", "summarize"],
  priority: 4,
  mem_pages: 3,
};

function openConsole(pid: number, name: string) {
  useWindowStore.getState().open(`agent-${pid}`, `${name} Console`);
}

export default function AgentStudio() {
  const command = useKernelStore((s) => s.command);
  const refreshSnapshot = useKernelStore((s) => s.refreshSnapshot);
  const [profiles, setProfiles] = useState<AgentProfile[]>([]);
  const [form, setForm] = useState<AgentProfileInput>(EMPTY);
  const [editing, setEditing] = useState<number | null>(null);
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState<string | null>(null);

  const reload = async () => {
    try {
      setProfiles(await fetchAgentProfiles());
    } catch (e) {
      setMessage(`profile store offline: ${String(e)}`);
    }
  };

  useEffect(() => {
    void reload();
  }, []);

  const set = <K extends keyof AgentProfileInput>(key: K, value: AgentProfileInput[K]) =>
    setForm((prev) => ({ ...prev, [key]: value }));

  const toggleTool = (tool: string) => {
    setForm((prev) => {
      if (prev.tools.includes(tool)) {
        return { ...prev, tools: prev.tools.filter((t) => t !== tool) };
      }
      if (prev.tools.length >= 4) return prev;
      return { ...prev, tools: [...prev.tools, tool] };
    });
  };

  const save = async (launchAfter: boolean) => {
    if (!form.name.trim()) {
      setMessage("name is required");
      return;
    }
    setBusy(true);
    setMessage(null);
    try {
      const saved = editing
        ? await updateAgentProfile(editing, form)
        : await createAgentProfile(form);
      setEditing(saved.id);
      setMessage(`${saved.name} saved as ${saved.key}`);
      await reload();
      await refreshSnapshot();
      if (launchAfter) {
        const res = (await command({ action: "agent_launch", key: saved.key })) as {
          ok: boolean;
          pid?: number;
          error?: string;
        };
        if (res.ok && res.pid) {
          openConsole(res.pid, saved.name);
        } else {
          setMessage(`saved, launch failed: ${res.error ?? "unknown error"}`);
        }
      }
    } catch (e) {
      const err = e as { response?: { data?: { detail?: string } }; message?: string };
      setMessage(err.response?.data?.detail ?? err.message ?? String(e));
    } finally {
      setBusy(false);
    }
  };

  const load = (profile: AgentProfile) => {
    setEditing(profile.id);
    setForm({
      name: profile.name,
      role: profile.role,
      personality: profile.personality,
      color: profile.color,
      icon: profile.icon,
      tools: profile.tools,
      priority: profile.priority,
      mem_pages: profile.mem_pages,
    });
    setMessage(`editing ${profile.name}`);
  };

  const remove = async (profile: AgentProfile) => {
    setBusy(true);
    try {
      await deleteAgentProfile(profile.id);
      if (editing === profile.id) {
        setEditing(null);
        setForm(EMPTY);
      }
      setMessage(`${profile.name} removed`);
      await reload();
      await refreshSnapshot();
    } catch (e) {
      const err = e as { response?: { data?: { detail?: string } } };
      setMessage(err.response?.data?.detail ?? "delete failed");
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="flex h-full flex-col bg-[#060a14]">
      <div className="flex items-center gap-2 border-b border-[#1e2a4a] bg-[#0d1225] px-3 py-2">
        <span className="mono text-xs font-semibold tracking-widest text-cyan-200">
          {editing ? "EDIT AGENT" : "CREATE AGENT"}
        </span>
        <span className="mono text-[10px] text-white/35">
          profile stored in SQLite, registered in the kernel registry
        </span>
        {message && <span className="ml-auto mono text-[10px] text-amber-200">{message}</span>}
      </div>

      <div className="flex-1 overflow-auto p-3">
        <div className="grid grid-cols-1 gap-3 lg:grid-cols-2">
          <label className="mono text-[10px] text-white/50">
            NAME
            <input
              value={form.name}
              onChange={(e) => set("name", e.target.value)}
              maxLength={32}
              placeholder="Research Analyst"
              className="mt-1 w-full rounded border border-[#1e2a4a] bg-[#0d1225] px-2 py-1.5 text-xs text-white/90 outline-none focus:border-cyan-400/60"
            />
          </label>
          <label className="mono text-[10px] text-white/50">
            ICON LABEL
            <input
              value={form.icon}
              onChange={(e) => set("icon", e.target.value)}
              maxLength={8}
              className="mt-1 w-full rounded border border-[#1e2a4a] bg-[#0d1225] px-2 py-1.5 text-xs text-white/90 outline-none focus:border-cyan-400/60"
            />
          </label>
        </div>

        <label className="mt-3 block mono text-[10px] text-white/50">
          ROLE / SYSTEM PROMPT
          <textarea
            value={form.role}
            onChange={(e) => set("role", e.target.value)}
            maxLength={96}
            rows={2}
            placeholder="What this agent is responsible for"
            className="mt-1 w-full resize-none rounded border border-[#1e2a4a] bg-[#0d1225] px-2 py-1.5 text-xs text-white/90 outline-none focus:border-cyan-400/60"
          />
        </label>

        <div className="mt-3 grid grid-cols-1 gap-3 md:grid-cols-3">
          <label className="mono text-[10px] text-white/50">
            PERSONALITY
            <input
              value={form.personality}
              onChange={(e) => set("personality", e.target.value)}
              maxLength={48}
              className="mt-1 w-full rounded border border-[#1e2a4a] bg-[#0d1225] px-2 py-1.5 text-xs text-white/90 outline-none focus:border-cyan-400/60"
            />
          </label>
          <label className="mono text-[10px] text-white/50">
            COLOUR
            <div className="mt-1 flex items-center gap-2">
              <input
                type="color"
                value={form.color}
                onChange={(e) => set("color", e.target.value)}
                className="h-8 w-10 cursor-pointer rounded border border-[#1e2a4a] bg-transparent"
              />
              <input
                value={form.color}
                onChange={(e) => set("color", e.target.value)}
                maxLength={16}
                className="w-full rounded border border-[#1e2a4a] bg-[#0d1225] px-2 py-1.5 mono text-xs text-white/90 outline-none focus:border-cyan-400/60"
              />
            </div>
          </label>
          <label className="mono text-[10px] text-white/50">
            PRIORITY / MEMORY PAGES
            <div className="mt-1 flex gap-2">
              <input
                type="number"
                min={0}
                max={10}
                value={form.priority}
                onChange={(e) => set("priority", Number(e.target.value))}
                className="w-full rounded border border-[#1e2a4a] bg-[#0d1225] px-2 py-1.5 mono text-xs text-white/90 outline-none focus:border-cyan-400/60"
              />
              <input
                type="number"
                min={1}
                max={32}
                value={form.mem_pages}
                onChange={(e) => set("mem_pages", Number(e.target.value))}
                className="w-full rounded border border-[#1e2a4a] bg-[#0d1225] px-2 py-1.5 mono text-xs text-white/90 outline-none focus:border-cyan-400/60"
              />
            </div>
          </label>
        </div>

        <div className="mt-3">
          <div className="flex items-center justify-between mono text-[10px] text-white/50">
            <span>SIMULATED TOOLS</span>
            <span className={form.tools.length >= 4 ? "text-amber-300" : "text-white/30"}>
              {form.tools.length}/4
            </span>
          </div>
          <div className="mt-1 flex flex-wrap gap-1">
            {TOOL_CHOICES.map((tool) => {
              const on = form.tools.includes(tool);
              return (
                <button
                  key={tool}
                  onClick={() => toggleTool(tool)}
                  className={`rounded px-1.5 py-0.5 mono text-[9px] transition ${
                    on
                      ? "bg-cyan-500/25 text-cyan-100"
                      : "bg-white/5 text-white/45 hover:bg-white/10"
                  }`}
                >
                  {tool}
                </button>
              );
            })}
          </div>
        </div>

        <div className="mt-4 flex flex-wrap gap-2">
          <button
            onClick={() => void save(false)}
            disabled={busy}
            className="rounded bg-cyan-500/20 px-3 py-1.5 mono text-[10px] tracking-widest text-cyan-200 hover:bg-cyan-500/30 disabled:opacity-40"
          >
            {editing ? "SAVE CHANGES" : "SAVE AGENT"}
          </button>
          <button
            onClick={() => void save(true)}
            disabled={busy}
            className="rounded bg-emerald-500/20 px-3 py-1.5 mono text-[10px] tracking-widest text-emerald-200 hover:bg-emerald-500/30 disabled:opacity-40"
          >
            SAVE & LAUNCH
          </button>
          <button
            onClick={() => {
              setEditing(null);
              setForm(EMPTY);
              setMessage(null);
            }}
            className="rounded bg-white/5 px-3 py-1.5 mono text-[10px] tracking-widest text-white/60 hover:bg-white/10"
          >
            NEW
          </button>
        </div>

        <div className="mt-5 border-t border-[#1e2a4a] pt-2">
          <div className="mono text-[10px] tracking-widest text-white/35">CUSTOM PROFILES</div>
          {profiles.length === 0 ? (
            <div className="mt-2 mono text-[10px] text-white/30">No custom profiles saved yet.</div>
          ) : (
            <div className="mt-1 space-y-1">
              {profiles.map((profile) => (
                <div key={profile.id} className="flex items-center gap-2 rounded border border-[#1e2a4a] bg-[#0d1225] px-2 py-1.5">
                  <span className="h-2 w-2 rounded-full" style={{ background: profile.color }} />
                  <span className="mono text-[11px] text-white/85">{profile.name}</span>
                  <span className="truncate mono text-[9px] text-white/35">{profile.key}</span>
                  <span className="ml-auto mono text-[9px] text-white/35">
                    {profile.tools.join(", ") || "no tools"}
                  </span>
                  <button
                    onClick={() => load(profile)}
                    className="rounded bg-white/5 px-1.5 py-0.5 mono text-[9px] text-cyan-200 hover:bg-white/10"
                  >
                    EDIT
                  </button>
                  <button
                    onClick={() => void remove(profile)}
                    disabled={busy}
                    className="rounded bg-red-500/10 px-1.5 py-0.5 mono text-[9px] text-red-200 hover:bg-red-500/20 disabled:opacity-40"
                  >
                    DELETE
                  </button>
                </div>
              ))}
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
