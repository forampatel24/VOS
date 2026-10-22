import { useEffect, useRef, useState } from "react";
import type { KeyboardEvent as ReactKeyboardEvent } from "react";
import { useKernelStore } from "@/store/kernelStore";
import type { ShellExecResult } from "@/api/types";

interface Line {
  kind: "cmd" | "out" | "err" | "sys";
  text: string;
}

const WELCOME: Line[] = [
  { kind: "sys", text: "JARVIS shell — type 'help' for commands. Every command is executed by the kernel." },
];

export default function Terminal() {
  const snap = useKernelStore((s) => s.snapshot);
  const command = useKernelStore((s) => s.command);
  const [lines, setLines] = useState<Line[]>(WELCOME);
  const [input, setInput] = useState("");
  const [history, setHistory] = useState<string[]>([]);
  const [cursor, setCursor] = useState(-1);
  const [busy, setBusy] = useState(false);
  const endRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);

  const prompt = snap?.shell.prompt ?? "jarvis@vos:/$ ";
  const cwd = snap?.shell.cwd ?? "/";

  useEffect(() => {
    endRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [lines]);

  const push = (next: Line[]) => setLines((prev) => [...prev, ...next]);

  const submit = async () => {
    const line = input.trim();
    if (!line || busy) return;
    setInput("");
    setHistory((h) => [...h.slice(-49), line]);
    setCursor(-1);
    push([{ kind: "cmd", text: `${prompt}${line}` }]);

    setBusy(true);
    try {
      const res = (await command({ action: "shell_exec", line })) as ShellExecResult;
      if (res.clear) {
        setLines(res.output ? [{ kind: "out", text: res.output }] : []);
        return;
      }
      const out: Line[] = [];
      if (res.output) {
        out.push({ kind: res.ok ? "out" : "err", text: res.output.replace(/\n$/, "") });
      }
      if (!res.ok && res.error && !res.output) out.push({ kind: "err", text: res.error });
      if (res.exit) out.push({ kind: "sys", text: "shell session ended" });
      push(out);
    } catch (e) {
      push([{ kind: "err", text: String(e) }]);
    } finally {
      setBusy(false);
      inputRef.current?.focus();
    }
  };

  const onKeyDown = (e: ReactKeyboardEvent<HTMLInputElement>) => {
    if (e.key === "Enter") {
      e.preventDefault();
      void submit();
    } else if (e.key === "ArrowUp") {
      e.preventDefault();
      if (history.length === 0) return;
      const next = cursor < 0 ? history.length - 1 : Math.max(0, cursor - 1);
      setCursor(next);
      setInput(history[next]);
    } else if (e.key === "ArrowDown") {
      e.preventDefault();
      if (cursor < 0) return;
      const next = cursor + 1;
      if (next >= history.length) {
        setCursor(-1);
        setInput("");
      } else {
        setCursor(next);
        setInput(history[next]);
      }
    } else if (e.key === "l" && e.ctrlKey) {
      e.preventDefault();
      setLines([]);
    }
  };

  const quick = async (line: string) => {
    setInput(line);
    inputRef.current?.focus();
  };

  const color: Record<Line["kind"], string> = {
    cmd: "text-cyan-300",
    out: "text-white/80",
    err: "text-red-300",
    sys: "text-white/40 italic",
  };

  return (
    <div className="flex h-full flex-col bg-[#050810]">
      <div className="flex items-center gap-2 border-b border-[#1e2a4a] bg-[#0d1225] px-2 py-1">
        <span className="mono text-[10px] tracking-widest text-white/40">SHELL · M8</span>
        <span className="mono text-[10px] text-white/30">cwd {cwd}</span>
        <div className="ml-auto flex gap-1 mono text-[10px]">
          {["help", "ps", "mem", "ipc", "devices", "files"].map((c) => (
            <button
              key={c}
              onClick={() => quick(c)}
              className="rounded border border-[#1e2a4a] bg-[#141b34] px-1.5 py-0.5 text-white/60 hover:bg-white/10 hover:text-white/90"
            >
              {c}
            </button>
          ))}
          <button
            onClick={() => setLines([])}
            className="rounded border border-[#1e2a4a] bg-[#141b34] px-1.5 py-0.5 text-white/60 hover:bg-white/10 hover:text-white/90"
          >
            clear
          </button>
        </div>
      </div>

      <div
        className="min-h-0 flex-1 overflow-auto p-2 mono text-xs leading-relaxed"
        onClick={() => inputRef.current?.focus()}
      >
        {lines.map((line, i) => (
          <div key={i} className={`whitespace-pre-wrap break-all ${color[line.kind]}`}>
            {line.text}
          </div>
        ))}
        <div className="flex items-center gap-1">
          <span className="text-cyan-300">{prompt}</span>
          <input
            ref={inputRef}
            value={input}
            onChange={(e) => setInput(e.target.value)}
            onKeyDown={onKeyDown}
            disabled={busy}
            autoFocus
            spellCheck={false}
            className="flex-1 bg-transparent text-white outline-none placeholder:text-white/20 disabled:opacity-50"
            placeholder={busy ? "executing…" : "type a command"}
          />
        </div>
        <div ref={endRef} />
      </div>

      <div className="border-t border-[#1e2a4a] bg-[#0d1225] px-2 py-1 mono text-[10px] text-white/35 flex flex-wrap gap-3">
        <span>exit {snap?.shell.last_status ?? 0}</span>
        <span>↑/↓ history</span>
        <span>ctrl+l clear</span>
        <span className="ml-auto">{snap?.shell.last_output ? snap.shell.last_output.slice(0, 80) : "ready"}</span>
      </div>
    </div>
  );
}
