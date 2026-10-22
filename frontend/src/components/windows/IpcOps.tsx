import { useEffect, useState } from "react";
import { useKernelStore } from "@/store/kernelStore";

export type IpcKind = "queue" | "shm" | "pipe" | "mutex" | "semaphore";

const ACTIONS: Record<IpcKind, string[]> = {
  queue: ["create", "send", "recv", "destroy"],
  shm: ["create", "attach", "detach", "write", "read", "destroy"],
  pipe: ["create", "open", "close", "write", "read", "destroy"],
  mutex: ["create", "lock", "unlock", "destroy"],
  semaphore: ["create", "wait", "post", "destroy"],
};

const LABEL: Record<IpcKind, string> = {
  queue: "Message queue",
  shm: "Shared memory",
  pipe: "Pipe",
  mutex: "Mutex",
  semaphore: "Semaphore",
};

const NAME_KEY: Record<IpcKind, string> = {
  queue: "queue",
  shm: "segment",
  pipe: "pipe",
  mutex: "mutex",
  semaphore: "semaphore",
};

const DEFAULT_OP: Record<IpcKind, string> = {
  queue: "send",
  shm: "read",
  pipe: "read",
  mutex: "lock",
  semaphore: "wait",
};

interface Props {
  selection: { kind: IpcKind; name: string } | null;
}

export default function IpcOps({ selection }: Props) {
  const snap = useKernelStore((s) => s.snapshot);
  const command = useKernelStore((s) => s.command);
  const processes = snap?.process_list ?? [];

  const [kind, setKind] = useState<IpcKind>("queue");
  const [op, setOp] = useState("create");
  const [name, setName] = useState("");
  const [pid, setPid] = useState(0);
  const [amount, setAmount] = useState(8);
  const [data, setData] = useState("hello");
  const [offset, setOffset] = useState(0);
  const [mode, setMode] = useState("r");
  const [nonblock, setNonblock] = useState(false);
  const [msg, setMsg] = useState<string | null>(null);

  useEffect(() => {
    if (!selection) return;
    setKind(selection.kind);
    setName(selection.name);
    setOp(DEFAULT_OP[selection.kind]);
    setMsg(null);
  }, [selection]);

  useEffect(() => {
    if (pid === 0 && processes.length > 0) setPid(processes[0].pid);
  }, [pid, processes]);

  const changeKind = (next: IpcKind) => {
    setKind(next);
    setOp(DEFAULT_OP[next]);
    setMsg(null);
  };

  const run = async () => {
    if (!name.trim()) {
      setMsg("error: object name is required");
      return;
    }
    const payload: Record<string, unknown> = {
      action: `ipc_${kind}_${op}`,
      [NAME_KEY[kind]]: name.trim(),
      pid,
    };
    if (op === "create") {
      if (kind === "semaphore") {
        payload.value = amount;
        payload.max = Math.max(amount, 4);
      } else if (kind === "shm") {
        payload.size = amount;
      } else {
        payload.capacity = amount;
      }
    }
    if (op === "send" || op === "write") payload.data = data;
    if (kind === "shm" && (op === "write" || op === "read")) payload.offset = offset;
    if (kind === "pipe" && op === "read") payload.max = 64;
    if (kind === "pipe" && op === "open") payload.mode = mode;
    if (nonblock) payload.nonblock = true;

    try {
      const res = (await command(payload)) as {
        ok: boolean;
        error?: string;
        data?: string;
        bytes?: number;
        blocked?: boolean;
      };
      if (!res.ok) {
        setMsg(`error: ${res.error ?? "failed"}`);
      } else if (res.blocked) {
        setMsg(`${kind} ${name}: blocked (pid ${pid} is WAITING)`);
      } else if (typeof res.data === "string") {
        setMsg(`${kind} ${name} → "${res.data}" (${res.bytes ?? res.data.length} B)`);
      } else if (typeof res.bytes === "number") {
        setMsg(`${kind} ${name} → ${res.bytes} B`);
      } else {
        setMsg(`${kind} ${name}: ${op} ok`);
      }
    } catch (e) {
      setMsg(`error: ${String(e)}`);
    }
  };

  const field =
    "rounded border border-[#1e2a4a] bg-[#060a14] px-2 py-1 mono text-xs text-white placeholder:text-white/25";
  const button =
    "rounded bg-cyan-500 px-3 py-1 mono text-xs font-semibold tracking-widest text-black hover:bg-cyan-400";

  return (
    <div className="border-t border-[#1e2a4a] bg-[#0d1225] p-2 space-y-1.5">
      <div className="mono text-[10px] tracking-widest text-white/40">OPERATIONS — every call runs in the kernel</div>
      <div className="flex flex-wrap items-center gap-1.5 mono text-xs">
        <select value={kind} onChange={(e) => changeKind(e.target.value as IpcKind)} className={field}>
          {(Object.keys(ACTIONS) as IpcKind[]).map((k) => (
            <option key={k} value={k}>
              {LABEL[k]}
            </option>
          ))}
        </select>
        <select value={op} onChange={(e) => setOp(e.target.value)} className={field}>
          {ACTIONS[kind].map((a) => (
            <option key={a} value={a}>
              {a}
            </option>
          ))}
        </select>
        <input
          value={name}
          onChange={(e) => setName(e.target.value)}
          placeholder="object name"
          className={`w-36 ${field}`}
        />
        <select value={pid} onChange={(e) => setPid(Number(e.target.value))} className={field}>
          <option value={0}>pid 0 (system)</option>
          {processes.map((p) => (
            <option key={p.pid} value={p.pid}>
              pid {p.pid} · {p.name}
            </option>
          ))}
        </select>
        {(op === "create" || (kind === "shm" && (op === "read" || op === "write"))) && (
          <input
            type="number"
            value={kind === "shm" && op !== "create" ? offset : amount}
            onChange={(e) =>
              kind === "shm" && op !== "create"
                ? setOffset(Number(e.target.value))
                : setAmount(Number(e.target.value))
            }
            title={kind === "shm" && op !== "create" ? "offset" : "capacity / size / permits"}
            className={`w-20 ${field}`}
          />
        )}
      </div>
      <div className="flex flex-wrap items-center gap-1.5 mono text-xs">
        {(op === "send" || op === "write") && (
          <input
            value={data}
            onChange={(e) => setData(e.target.value)}
            placeholder="payload"
            className={`flex-1 min-w-[140px] ${field}`}
          />
        )}
        {kind === "pipe" && op === "open" && (
          <select value={mode} onChange={(e) => setMode(e.target.value)} className={field}>
            <option value="r">read</option>
            <option value="w">write</option>
            <option value="rw">read+write</option>
          </select>
        )}
        <label className="flex items-center gap-1 text-white/50">
          <input type="checkbox" checked={nonblock} onChange={(e) => setNonblock(e.target.checked)} />
          nonblock
        </label>
        <button onClick={run} className={button}>
          RUN
        </button>
        {msg && <span className="text-cyan-200/90 text-[11px]">{msg}</span>}
      </div>
    </div>
  );
}
