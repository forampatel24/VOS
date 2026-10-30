import { AnimatePresence, motion } from "framer-motion";
import { useEffect, useRef, useState } from "react";
import { searchFiles } from "@/api/client";
import type { FsEntry } from "@/api/types";
import { APP_CATALOG } from "@/lib/apps";
import { useKernelStore } from "@/store/kernelStore";
import { useShellStore } from "@/store/shellStore";
import { useWindowStore } from "@/store/windowStore";

type ResultKind = "app" | "agent" | "file";

interface SearchResult {
  key: string;
  kind: ResultKind;
  title: string;
  subtitle: string;
  icon: string;
  color: string;
  action: () => void;
}

function searchable(value: string): string {
  return value.toLowerCase();
}

export default function SearchOverlay() {
  const open = useShellStore((state) => state.searchOpen);
  const closeSearch = useShellStore((state) => state.closeSearch);
  const toggleSearch = useShellStore((state) => state.toggleSearch);
  const setFileTarget = useShellStore((state) => state.setFileTarget);
  const snapshot = useKernelStore((state) => state.snapshot);
  const [query, setQuery] = useState("");
  const [active, setActive] = useState(0);
  const [files, setFiles] = useState<FsEntry[]>([]);
  const [loading, setLoading] = useState(false);
  const inputRef = useRef<HTMLInputElement>(null);
  const requestSeq = useRef(0);

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "k") {
        event.preventDefault();
        toggleSearch();
        return;
      }
      if (event.key === "Escape" && useShellStore.getState().searchOpen) {
        closeSearch();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [toggleSearch, closeSearch]);

  useEffect(() => {
    if (!open) return;
    setQuery("");
    setActive(0);
    setFiles([]);
    const timer = setTimeout(() => inputRef.current?.focus(), 40);
    return () => clearTimeout(timer);
  }, [open]);

  const trimmed = query.trim();

  useEffect(() => {
    const sequence = ++requestSeq.current;
    if (!open) return;
    if (trimmed.length < 2) {
      setFiles([]);
      setLoading(false);
      return;
    }
    setLoading(true);
    const timer = setTimeout(() => {
      searchFiles(trimmed.slice(0, 64))
        .then((entries) => {
          if (requestSeq.current === sequence) setFiles(entries.slice(0, 8));
        })
        .catch(() => {
          if (requestSeq.current === sequence) setFiles([]);
        })
        .finally(() => {
          if (requestSeq.current === sequence) setLoading(false);
        });
    }, 220);
    return () => clearTimeout(timer);
  }, [trimmed, open]);

  const q = searchable(trimmed);
  const appResults: SearchResult[] = APP_CATALOG.filter((app) => {
    if (!q) return true;
    return searchable(`${app.label} ${app.description} ${app.keywords.join(" ")}`).includes(q);
  }).map((app) => ({
    key: `app-${app.id}`,
    kind: "app",
    title: app.label,
    subtitle: app.description,
    icon: app.icon,
    color: app.dot,
    action: () => {
      useWindowStore.getState().open(app.id);
      closeSearch();
    },
  }));

  const agentResults: SearchResult[] = q
    ? (snapshot?.agents.definitions ?? [])
        .filter((agent) =>
          searchable(`${agent.name} ${agent.role} ${agent.key}`).includes(q)
        )
        .slice(0, 6)
        .map((agent) => ({
          key: `agent-def-${agent.id}`,
          kind: "agent" as const,
          title: agent.name,
          subtitle: `${agent.role || "agent"} · ${agent.running_pid ? `PID ${agent.running_pid}` : "idle"}${agent.builtin ? " · built-in" : ""}`,
          icon: "AI",
          color: "bg-emerald-400",
          action: () => {
            if (agent.running_pid > 0) {
              useWindowStore
                .getState()
                .open(`agent-${agent.running_pid}`, `${agent.name} Console`);
            } else {
              useWindowStore.getState().open("agent-hub");
            }
            closeSearch();
          },
        }))
    : [];

  const fileResults: SearchResult[] = files.map((entry) => ({
    key: `file-${entry.id}`,
    kind: "file",
    title: entry.name,
    subtitle: entry.path,
    icon: entry.type === "dir" ? "DIR" : "TXT",
    color: entry.type === "dir" ? "bg-amber-400" : "bg-cyan-400",
    action: () => {
      setFileTarget(entry.path);
      useWindowStore.getState().open("files");
      closeSearch();
    },
  }));

  const results = [...appResults, ...agentResults, ...fileResults];

  useEffect(() => {
    setActive((index) => Math.min(index, Math.max(0, results.length - 1)));
  }, [results.length]);

  const choose = (result: SearchResult | undefined) => {
    result?.action();
  };

  const onInputKeyDown = (event: React.KeyboardEvent<HTMLInputElement>) => {
    if (event.key === "ArrowDown") {
      event.preventDefault();
      setActive((index) => (results.length === 0 ? 0 : (index + 1) % results.length));
    } else if (event.key === "ArrowUp") {
      event.preventDefault();
      setActive((index) => (results.length === 0 ? 0 : (index - 1 + results.length) % results.length));
    } else if (event.key === "Enter") {
      event.preventDefault();
      choose(results[active]);
    }
  };

  return (
    <AnimatePresence>
      {open && (
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          onPointerDown={closeSearch}
          className="fixed inset-0 z-[80] flex items-start justify-center bg-black/55 px-4 pt-[12vh] backdrop-blur-sm"
        >
          <motion.div
            initial={{ opacity: 0, y: -12, scale: 0.98 }}
            animate={{ opacity: 1, y: 0, scale: 1 }}
            exit={{ opacity: 0, y: -8, scale: 0.98 }}
            transition={{ duration: 0.16, ease: "easeOut" }}
            onPointerDown={(event) => event.stopPropagation()}
            className="w-full max-w-2xl overflow-hidden rounded-2xl border border-cyan-400/20 bg-[#0d1225]/95 shadow-[0_30px_100px_rgba(0,0,0,0.75)]"
          >
            <div className="flex items-center gap-3 border-b border-[#1e2a4a] px-4 py-3">
              <span className="mono text-lg text-cyan-300">⌕</span>
              <input
                ref={inputRef}
                value={query}
                onChange={(event) => {
                  setQuery(event.target.value);
                  setActive(0);
                }}
                onKeyDown={onInputKeyDown}
                placeholder="Search apps, agents and virtual files…"
                spellCheck={false}
                className="min-w-0 flex-1 bg-transparent mono text-sm text-white/90 outline-none placeholder:text-white/25"
              />
              {loading && <span className="mono text-[10px] text-cyan-300/60">SEARCHING…</span>}
              <kbd className="rounded border border-[#1e2a4a] bg-[#060a14] px-1.5 py-0.5 mono text-[9px] text-white/35">
                ESC
              </kbd>
            </div>

            <div className="max-h-[52vh] overflow-y-auto p-1.5">
              {results.length === 0 ? (
                <div className="px-4 py-10 text-center mono text-xs text-white/30">
                  {loading ? "Querying the virtual filesystem…" : `No matches for “${trimmed}”.`}
                </div>
              ) : (
                results.map((result, index) => (
                  <button
                    key={result.key}
                    onPointerEnter={() => setActive(index)}
                    onClick={() => choose(result)}
                    className={`flex w-full items-center gap-3 rounded-lg px-3 py-2 text-left transition ${
                      active === index ? "" : "hover:bg-white/5"
                    }`}
                    style={
                      active === index
                        ? {
                            background:
                              "color-mix(in srgb, var(--jarvis-primary) 15%, transparent)",
                          }
                        : undefined
                    }
                  >
                    <span className="grid h-8 w-8 shrink-0 place-items-center rounded-lg border border-[#1e2a4a] bg-[#141b34] mono text-[9px] font-bold text-cyan-200">
                      {result.icon}
                    </span>
                    <span className="min-w-0 flex-1">
                      <span className="flex items-center gap-2">
                        <span className="truncate mono text-xs font-semibold text-white/85">
                          {result.title}
                        </span>
                        <span className={`h-1.5 w-1.5 shrink-0 rounded-full ${result.color}`} />
                      </span>
                      <span className="mt-0.5 block truncate mono text-[10px] text-white/35">
                        {result.subtitle}
                      </span>
                    </span>
                    <span className="mono text-[9px] uppercase tracking-widest text-white/25">
                      {result.kind}
                    </span>
                  </button>
                ))
              )}
            </div>

            <div className="flex items-center gap-4 border-t border-[#1e2a4a] px-4 py-2 mono text-[9px] tracking-widest text-white/25">
              <span>↑↓ NAVIGATE</span>
              <span>ENTER OPEN</span>
              <span className="ml-auto">{results.length} RESULTS</span>
            </div>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
}
