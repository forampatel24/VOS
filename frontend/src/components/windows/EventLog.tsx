import { useEffect, useRef, useState } from "react";
import { useKernelStore } from "@/store/kernelStore";

const FILTERS = ["all", "PROCESS", "MEMORY", "PAGE_FAULT", "SWAP", "SCHEDULE", "TIMER", "CPU"] as const;

export default function EventLog() {
  const logs = useKernelStore((s) => s.logs);
  const [filter, setFilter] = useState<(typeof FILTERS)[number]>("all");
  const [search, setSearch] = useState("");
  const endRef = useRef<HTMLDivElement>(null);
  const [autoScroll, setAutoScroll] = useState(true);

  useEffect(() => {
    if (autoScroll) endRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [logs, autoScroll]);

  const filtered = logs.filter((e) => {
    if (filter !== "all" && !e.message.toUpperCase().includes(filter)) return false;
    if (search && !e.message.toLowerCase().includes(search.toLowerCase())) return false;
    return true;
  });

  const shown = filtered.slice(-300);

  return (
    <div className="flex h-full flex-col">
      <div className="flex flex-wrap items-center gap-1.5 border-b border-[#1e2a4a] bg-[#0d1225] p-2">
        <div className="flex gap-1">
          {FILTERS.map((f) => (
            <button
              key={f}
              onClick={() => setFilter(f)}
              className={`mono rounded px-2 py-1 text-[11px] tracking-widest border ${filter === f ? "bg-cyan-500/20 border-cyan-400/30 text-cyan-200" : "bg-[#141b34] border-[#1e2a4a] text-white/50 hover:text-white/80"}`}
            >
              {f}
            </button>
          ))}
        </div>
        <input
          value={search}
          onChange={(e) => setSearch(e.target.value)}
          placeholder="filter…"
          className="ml-auto mono h-7 w-28 rounded border border-[#1e2a4a] bg-[#060a14] px-2 text-xs text-white placeholder:text-white/30 focus:border-cyan-400/40 focus:outline-none"
        />
        <label className="flex items-center gap-1 mono text-xs text-white/50 cursor-pointer">
          <input type="checkbox" checked={autoScroll} onChange={(e) => setAutoScroll(e.target.checked)} className="accent-cyan-400" />
          follow
        </label>
        <span className="mono text-xs text-white/30">
          {filtered.length} / {logs.length}
        </span>
      </div>

      <div className="flex-1 overflow-auto p-2 font-mono text-xs leading-relaxed" onScroll={(e) => {
        const el = e.currentTarget;
        const nearBottom = el.scrollHeight - el.scrollTop - el.clientHeight < 40;
        if (!nearBottom && autoScroll) setAutoScroll(false);
        if (nearBottom && !autoScroll) setAutoScroll(true);
      }}>
        {shown.length === 0 ? (
          <div className="text-white/30 py-8 text-center">No matching events — kernel logs stream here live via WebSocket.</div>
        ) : (
          shown.map((e) => {
            const isError = e.message.includes("SEGV") || e.message.includes("rejected") || e.message.includes("failed");
            const isFault = e.message.includes("PAGE_FAULT") || e.message.includes("SWAP");
            const isProc = e.message.includes("PROCESS_");
            return (
              <div
                key={e.index}
                className={`flex gap-2 py-0.5 px-1 rounded hover:bg-white/[0.04] ${isError ? "text-red-300" : isFault ? "text-amber-200" : isProc ? "text-emerald-200" : "text-white/60"}`}
              >
                <span className="shrink-0 text-white/20 tabular-nums">#{String(e.index).padStart(3, "0")}</span>
                <span className="break-all">{e.message}</span>
              </div>
            );
          })
        )}
        <div ref={endRef} />
      </div>
    </div>
  );
}
