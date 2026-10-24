import { useEffect, useState } from "react";
import { fetchSettings, updateSettings } from "@/api/client";
import type { SystemSettings } from "@/api/types";
import { useKernelStore } from "@/store/kernelStore";

const ALGORITHMS: SystemSettings["scheduler"]["algorithm"][] = [
  "round_robin",
  "fcfs",
  "sjf",
  "priority",
];
const ALLOCATORS: SystemSettings["memory"]["allocator"][] = [
  "first_fit",
  "best_fit",
  "worst_fit",
];
const REPLACEMENTS: SystemSettings["memory"]["replacement"][] = [
  "fifo",
  "lru",
  "clock",
];

function errorText(error: unknown): string {
  const value = error as {
    response?: { data?: { detail?: unknown } };
    message?: string;
  };
  const detail = value.response?.data?.detail;
  if (typeof detail === "string") return detail;
  if (Array.isArray(detail)) return detail.map(String).join(", ");
  return value.message ?? "settings request failed";
}

function Field({ label, hint, children }: { label: string; hint?: string; children: React.ReactNode }) {
  return (
    <label className="flex items-center justify-between gap-3 rounded-lg border border-[#1e2a4a] bg-[#141b34] px-3 py-2">
      <span>
        <span className="mono block text-[11px] tracking-wide text-white/70">{label}</span>
        {hint && <span className="mono block text-[10px] text-white/30">{hint}</span>}
      </span>
      {children}
    </label>
  );
}

function NumberInput({ value, min, max, onChange }: { value: number; min: number; max: number; onChange: (value: number) => void }) {
  return (
    <input
      type="number"
      min={min}
      max={max}
      value={value}
      onChange={(event) => onChange(Number(event.target.value))}
      className="w-24 rounded border border-[#1e2a4a] bg-[#060a14] px-2 py-1 text-right mono text-xs text-cyan-200 outline-none focus:border-cyan-400/60"
    />
  );
}

function Choice<T extends string>({ value, options, onChange }: { value: T; options: readonly T[]; onChange: (value: T) => void }) {
  return (
    <div className="flex flex-wrap justify-end gap-1">
      {options.map((option) => (
        <button
          key={option}
          type="button"
          onClick={() => onChange(option)}
          className={`rounded border px-2 py-1 mono text-[10px] tracking-wide transition ${
            value === option
              ? "border-cyan-400/40 bg-cyan-500/20 text-cyan-200"
              : "border-[#1e2a4a] bg-[#0d1225] text-white/50 hover:text-white/80"
          }`}
        >
          {option.replace("_", " ").toUpperCase()}
        </button>
      ))}
    </div>
  );
}

export default function Settings() {
  const snapshot = useKernelStore((state) => state.snapshot);
  const refreshSnapshot = useKernelStore((state) => state.refreshSnapshot);
  const [draft, setDraft] = useState<SystemSettings | null>(null);
  const [persisted, setPersisted] = useState(false);
  const [saving, setSaving] = useState(false);
  const [status, setStatus] = useState<{ tone: "ok" | "warn" | "error"; text: string } | null>(null);
  const [restartRequired, setRestartRequired] = useState<string[]>([]);

  const load = async () => {
    try {
      const response = await fetchSettings();
      setDraft(response.settings);
      setPersisted(response.persisted);
      setRestartRequired([]);
      setStatus(null);
    } catch (error) {
      setStatus({ tone: "error", text: errorText(error) });
    }
  };

  useEffect(() => {
    void load();
  }, []);

  const save = async () => {
    if (!draft) return;
    setSaving(true);
    try {
      const response = await updateSettings({
        scheduler: draft.scheduler,
        memory: draft.memory,
      });
      setDraft(response.settings);
      setPersisted(response.persisted);
      setRestartRequired(response.restart_required ?? []);
      const applied = response.applied && response.applied.length > 0 ? response.applied.join(", ") : "no live change";
      setStatus({ tone: "ok", text: `Saved. Applied: ${applied}.` });
      await refreshSnapshot();
    } catch (error) {
      setStatus({ tone: "error", text: errorText(error) });
    } finally {
      setSaving(false);
    }
  };

  if (!draft) {
    return (
      <div className="p-6 mono text-xs text-white/40">
        {status?.tone === "error" ? `Settings offline — ${status.text}` : "Loading settings…"}
      </div>
    );
  }

  const updateScheduler = (patch: Partial<SystemSettings["scheduler"]>) =>
    setDraft((current) => current && { ...current, scheduler: { ...current.scheduler, ...patch } });
  const updateMemory = (patch: Partial<SystemSettings["memory"]>) =>
    setDraft((current) => current && { ...current, memory: { ...current.memory, ...patch } });

  return (
    <div className="flex h-full flex-col gap-2 overflow-y-auto p-3">
      <div className="rounded-lg border border-[#1e2a4a] bg-[#0d1225] p-3">
        <div className="flex items-start justify-between gap-3">
          <div>
            <div className="mono text-xs font-semibold tracking-widest text-cyan-300">KERNEL SETTINGS</div>
            <div className="mono mt-1 text-[11px] text-white/40">
              Changes are validated by the bridge, applied through the kernel ABI and persisted in SQLite.
            </div>
          </div>
          <div className="text-right mono text-[10px]">
            <div className={persisted ? "text-emerald-300" : "text-white/40"}>
              {persisted ? "persisted" : "defaults"}
            </div>
            <div className="text-white/30">
              live {snapshot?.scheduler.algo ?? "—"} · {snapshot?.memory.config.allocator ?? "—"}
            </div>
          </div>
        </div>
      </div>

      <div className="rounded-lg border border-[#1e2a4a] bg-[#0d1225] p-3">
        <div className="mono mb-2 text-xs font-semibold tracking-widest text-emerald-300">SCHEDULER</div>
        <div className="grid gap-1.5">
          <Field label="Algorithm" hint="applied immediately">
            <Choice value={draft.scheduler.algorithm} options={ALGORITHMS} onChange={(algorithm) => updateScheduler({ algorithm })} />
          </Field>
          <Field label="Time quantum" hint="ticks · restart required">
            <NumberInput value={draft.scheduler.quantum} min={1} max={10000} onChange={(quantum) => updateScheduler({ quantum })} />
          </Field>
        </div>
      </div>

      <div className="rounded-lg border border-[#1e2a4a] bg-[#0d1225] p-3">
        <div className="mono mb-2 text-xs font-semibold tracking-widest text-violet-300">MEMORY</div>
        <div className="grid gap-1.5">
          <Field label="Frame allocator" hint="applied immediately">
            <Choice value={draft.memory.allocator} options={ALLOCATORS} onChange={(allocator) => updateMemory({ allocator })} />
          </Field>
          <Field label="Page replacement" hint="applied immediately">
            <Choice value={draft.memory.replacement} options={REPLACEMENTS} onChange={(replacement) => updateMemory({ replacement })} />
          </Field>
          <Field label="Agent memory budget" hint="pages per agent">
            <NumberInput
              value={draft.memory.agent_budget_pages}
              min={0}
              max={4096}
              onChange={(agent_budget_pages) => updateMemory({ agent_budget_pages })}
            />
          </Field>
          <Field label="Referenced bit" hint="clock replacement support">
            <button
              type="button"
              onClick={() => updateMemory({ referenced_bit: !draft.memory.referenced_bit })}
              className={`rounded border px-2 py-1 mono text-[10px] tracking-wide ${
                draft.memory.referenced_bit
                  ? "border-emerald-400/40 bg-emerald-500/20 text-emerald-200"
                  : "border-[#1e2a4a] bg-[#0d1225] text-white/40"
              }`}
            >
              {draft.memory.referenced_bit ? "ENABLED" : "DISABLED"}
            </button>
          </Field>
        </div>
      </div>

      <div className="rounded-lg border border-[#1e2a4a] bg-[#0d1225] p-3">
        <div className="mono mb-2 text-xs font-semibold tracking-widest text-amber-300">APPEARANCE (STORED)</div>
        <div className="grid grid-cols-2 gap-2 mono text-[11px] text-white/60">
          <div className="rounded border border-[#1e2a4a] bg-[#141b34] px-3 py-2">
            <div className="text-[10px] tracking-widest text-white/30">THEME</div>
            <div className="text-white/80">{draft.theme.name} · {draft.theme.mode}</div>
          </div>
          <div className="rounded border border-[#1e2a4a] bg-[#141b34] px-3 py-2">
            <div className="text-[10px] tracking-widest text-white/30">ACCENTS</div>
            <div className="flex items-center gap-1.5">
              {[draft.theme.primary_color, draft.theme.secondary_color, draft.theme.accent_color].map((color) => (
                <span key={color} className="h-3 w-3 rounded-full border border-white/20" style={{ background: color }} />
              ))}
              <span className="text-white/60">{draft.theme.primary_color}</span>
            </div>
          </div>
        </div>
        <div className="mono mt-2 text-[10px] text-white/25">
          Theme values persist through the same table; the desktop theme engine consumes them in the shell layer.
        </div>
      </div>

      {restartRequired.length > 0 && (
        <div className="rounded-lg border border-amber-400/30 bg-amber-500/10 px-3 py-2 mono text-[11px] text-amber-200">
          Restart required for: {restartRequired.join(", ")}
        </div>
      )}
      {status && (
        <div
          className={`rounded-lg border px-3 py-2 mono text-[11px] ${
            status.tone === "error"
              ? "border-red-400/30 bg-red-500/10 text-red-200"
              : "border-emerald-400/30 bg-emerald-500/10 text-emerald-200"
          }`}
        >
          {status.text}
        </div>
      )}

      <div className="mt-auto flex items-center gap-2 pt-1">
        <button
          type="button"
          onClick={save}
          disabled={saving}
          className="mono rounded border border-cyan-400/40 bg-cyan-500/20 px-3 py-1.5 text-xs tracking-widest text-cyan-200 transition hover:bg-cyan-500/30 disabled:opacity-40"
        >
          {saving ? "SAVING…" : "SAVE SETTINGS"}
        </button>
        <button
          type="button"
          onClick={() => void load()}
          disabled={saving}
          className="mono rounded border border-[#1e2a4a] bg-[#141b34] px-3 py-1.5 text-xs tracking-widest text-white/60 transition hover:text-white/90 disabled:opacity-40"
        >
          RELOAD
        </button>
        <span className="mono ml-auto text-[10px] text-white/25">
          PATCH /api/settings
        </span>
      </div>
    </div>
  );
}
