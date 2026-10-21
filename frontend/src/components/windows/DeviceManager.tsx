import { useEffect, useState } from "react";
import { useKernelStore } from "@/store/kernelStore";
import type { DeviceEntry } from "@/api/types";

const STATE_STYLE: Record<string, string> = {
  CONNECTED: "bg-emerald-500/15 text-emerald-300 border-emerald-400/30",
  DISCONNECTED: "bg-slate-500/15 text-slate-300 border-slate-400/30",
  BUSY: "bg-amber-500/15 text-amber-200 border-amber-400/30",
  WAITING: "bg-violet-500/15 text-violet-200 border-violet-400/30",
  FAULT: "bg-red-500/15 text-red-200 border-red-400/30",
  DISABLED: "bg-white/5 text-white/40 border-white/10",
};

const str = (value: unknown, fallback = "") =>
  typeof value === "string" ? value : fallback;
const num = (value: unknown) =>
  typeof value === "number" ? value : 0;

export default function DeviceManager() {
  const snap = useKernelStore((s) => s.snapshot);
  const command = useKernelStore((s) => s.command);

  const [selectedKey, setSelectedKey] = useState("keyboard");
  const [input, setInput] = useState("");
  const [coords, setCoords] = useState("640,480");
  const [msg, setMsg] = useState<string | null>(null);
  const [readout, setReadout] = useState<string | null>(null);

  useEffect(() => {
    setInput("");
    setReadout(null);
    setMsg(null);
  }, [selectedKey]);

  const devices = snap?.devices;
  if (!snap || !devices) {
    return <div className="p-6 mono text-xs text-white/40">Waiting for kernel snapshot…</div>;
  }

  const list = devices.list;
  const device = list.find((d) => d.key === selectedKey) ?? list[0];
  const info = device.info ?? {};

  const run = async (payload: Record<string, unknown>, okText: string) => {
    setMsg(null);
    try {
      const res = (await command(payload)) as { ok: boolean; error?: string; data?: string };
      setMsg(res.ok ? okText : `error: ${res.error}`);
      return res;
    } catch (e) {
      setMsg(`error: ${String(e)}`);
      return { ok: false } as { ok: boolean; data?: string };
    }
  };

  const write = (data: string) =>
    run({ action: "device_write", device: device.key, data }, `wrote ${data.length} B to ${device.name}`);

  const read = async () => {
    const res = await run({ action: "device_read", device: device.key }, `read from ${device.name}`);
    if (res.ok) setReadout(res.data ?? "");
  };

  const interrupt = () =>
    run({ action: "device_interrupt", device: device.key }, `${device.name} raised ${device.irq} IRQ`);

  const setPower = (power: boolean) =>
    run({ action: "device_set", device: device.key, power }, `${device.name} power ${power ? "on" : "off"}`);

  const fault = () =>
    run(
      { action: "device_fault", device: device.key, reason: "simulated hardware fault" },
      `${device.name} reported a fault`
    );

  const repair = () => run({ action: "device_repair", device: device.key }, `${device.name} repaired`);

  const lines = Array.isArray(info.lines) ? (info.lines as unknown[]).map((l) => String(l)) : [];
  const queue = Array.isArray(info.queue) ? (info.queue as unknown[]).map((l) => String(l)) : [];

  const inputPanel = (entry: DeviceEntry) => {
    switch (entry.key) {
      case "keyboard":
        return (
          <div className="space-y-2">
            <div className="flex gap-1.5">
              <input
                value={input}
                onChange={(e) => setInput(e.target.value)}
                onKeyDown={(e) => e.key === "Enter" && input && write(input)}
                placeholder="type keys to inject…"
                className="flex-1 rounded border border-[#1e2a4a] bg-[#060a14] px-2 py-1 mono text-xs text-white placeholder:text-white/25"
              />
              <button
                onClick={() => input && write(input)}
                className="rounded bg-cyan-500 px-2 py-1 mono text-xs font-semibold tracking-widest text-black hover:bg-cyan-400"
              >
                INJECT
              </button>
              <button onClick={read} className="rounded bg-white/10 px-2 py-1 mono text-xs text-white/80 hover:bg-white/15">
                READ
              </button>
            </div>
            <div className="rounded border border-[#1e2a4a] bg-[#060a14] p-2 mono text-[11px] text-white/70">
              buffer ({num(info.buffer_len)} B): <span className="text-cyan-200">{str(info.buffer) || "—"}</span>
            </div>
          </div>
        );
      case "mouse":
        return (
          <div className="flex gap-1.5">
            <input
              value={coords}
              onChange={(e) => setCoords(e.target.value)}
              className="w-28 rounded border border-[#1e2a4a] bg-[#060a14] px-2 py-1 mono text-xs text-white"
            />
            <button
              onClick={() => write(coords)}
              className="rounded bg-cyan-500 px-2 py-1 mono text-xs font-semibold tracking-widest text-black hover:bg-cyan-400"
            >
              MOVE
            </button>
            <span className="ml-auto mono text-xs text-white/50">
              x {num(info.x)} · y {num(info.y)}
            </span>
          </div>
        );
      case "display":
        return (
          <div className="space-y-2">
            <textarea
              value={input}
              onChange={(e) => setInput(e.target.value)}
              placeholder="lines to draw on the virtual display…"
              className="h-16 w-full resize-none rounded border border-[#1e2a4a] bg-[#060a14] p-2 mono text-xs text-white placeholder:text-white/25"
            />
            <div className="flex gap-1.5">
              <button
                onClick={() => input && write(input)}
                className="rounded bg-cyan-500 px-2 py-1 mono text-xs font-semibold tracking-widest text-black hover:bg-cyan-400"
              >
                WRITE
              </button>
              <button onClick={read} className="rounded bg-white/10 px-2 py-1 mono text-xs text-white/80 hover:bg-white/15">
                READ
              </button>
              <span className="ml-auto mono text-[11px] text-white/40">{lines.length} line(s)</span>
            </div>
            <div className="max-h-28 overflow-auto rounded border border-[#1e2a4a] bg-[#060a14] p-2 mono text-[11px] text-cyan-100/80">
              {lines.length === 0 ? "display buffer empty" : lines.map((line, i) => <div key={i}>{line}</div>)}
            </div>
          </div>
        );
      case "printer":
        return (
          <div className="space-y-2">
            <div className="flex gap-1.5">
              <input
                value={input}
                onChange={(e) => setInput(e.target.value)}
                placeholder="spool a print job…"
                className="flex-1 rounded border border-[#1e2a4a] bg-[#060a14] px-2 py-1 mono text-xs text-white placeholder:text-white/25"
              />
              <button
                onClick={() => input && write(input)}
                className="rounded bg-cyan-500 px-2 py-1 mono text-xs font-semibold tracking-widest text-black hover:bg-cyan-400"
              >
                QUEUE
              </button>
              <button
                onClick={interrupt}
                className="rounded bg-violet-500/30 px-2 py-1 mono text-xs text-violet-200 hover:bg-violet-500/40"
              >
                PRINT ONE
              </button>
              <button onClick={read} className="rounded bg-white/10 px-2 py-1 mono text-xs text-white/80 hover:bg-white/15">
                READ
              </button>
            </div>
            <div className="rounded border border-[#1e2a4a] bg-[#060a14] p-2 mono text-[11px] text-white/70">
              <div className="text-white/40">
                queued {num(info.queued)} · printed {num(info.printed)}
              </div>
              {queue.length === 0 ? (
                <div className="text-white/30">spool empty</div>
              ) : (
                queue.map((job, i) => (
                  <div key={i} className="truncate text-violet-200">
                    [{i}] {job}
                  </div>
                ))
              )}
            </div>
          </div>
        );
      case "clock":
        return (
          <div className="flex items-center gap-2">
            <button
              onClick={interrupt}
              className="rounded bg-violet-500/30 px-2 py-1 mono text-xs text-violet-200 hover:bg-violet-500/40"
            >
              RAISE TIMER IRQ
            </button>
            <span className="mono text-xs text-white/50">
              hardware ticks {num(info.ticks)} · mode {str(info.mode, "periodic")}
            </span>
          </div>
        );
      case "disk":
        return (
          <div className="flex items-center gap-2">
            <button onClick={read} className="rounded bg-white/10 px-2 py-1 mono text-xs text-white/80 hover:bg-white/15">
              READ STATUS
            </button>
            <span className="mono text-xs text-white/50">
              transfers {num(info.transfers)} · backend {str(info.backend, "virtual-blocks")}
            </span>
          </div>
        );
      default:
        return (
          <div className="mono text-xs text-white/40">
            Core device — driven by the kernel, no user I/O path.
          </div>
        );
    }
  };

  return (
    <div className="flex h-full flex-col">
      {/* stats */}
      <div className="grid grid-cols-4 gap-2 p-2 border-b border-[#1e2a4a] bg-[#0d1225]">
        <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 mono text-xs">
          <div className="text-white/40 tracking-widest text-[10px]">DEVICES</div>
          <div className="text-white font-bold">{devices.count}</div>
        </div>
        <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 mono text-xs">
          <div className="text-white/40 tracking-widest text-[10px]">INTERRUPTS</div>
          <div className="text-white">{devices.stats.interrupts}</div>
        </div>
        <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 mono text-xs">
          <div className="text-white/40 tracking-widest text-[10px]">I/O OPS</div>
          <div className="text-white">r {devices.stats.reads} · w {devices.stats.writes}</div>
        </div>
        <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 mono text-xs">
          <div className="text-white/40 tracking-widest text-[10px]">ERRORS</div>
          <div className={devices.stats.errors > 0 ? "text-red-300" : "text-white"}>{devices.stats.errors}</div>
        </div>
      </div>

      <div className="flex min-h-0 flex-1">
        {/* device list */}
        <div className="w-2/5 overflow-auto border-r border-[#1e2a4a] p-1.5">
          {list.map((dev) => (
            <div
              key={dev.key}
              onClick={() => {
                setSelectedKey(dev.key);
                setReadout(null);
                setMsg(null);
              }}
              className={`mb-1 cursor-pointer rounded border p-1.5 transition ${
                dev.key === device.key
                  ? "border-cyan-400/40 bg-cyan-500/10"
                  : "border-[#1e2a4a] bg-[#0d1225] hover:bg-white/5"
              }`}
            >
              <div className="flex items-center gap-2 mono text-xs">
                <span className={`h-2 w-2 rounded-full ${dev.power ? "bg-emerald-400" : "bg-white/20"}`} />
                <span className="text-white/85 font-semibold">{dev.name}</span>
                <span className={`ml-auto rounded border px-1.5 py-0.5 text-[9px] ${STATE_STYLE[dev.state] ?? ""}`}>
                  {dev.state}
                </span>
              </div>
              <div className="mt-0.5 flex items-center gap-2 mono text-[10px] text-white/35">
                <span>{dev.driver}</span>
                <span>irq {dev.irq}</span>
                <span className="ml-auto">{dev.power ? "on" : "off"}</span>
              </div>
            </div>
          ))}
        </div>

        {/* detail */}
        <div className="w-3/5 overflow-auto p-2 space-y-2">
          <div className="rounded border border-[#1e2a4a] bg-[#0d1225] p-2">
            <div className="flex items-center gap-2">
              <span className="mono text-sm font-semibold text-cyan-200">{device.name}</span>
              <span className={`rounded border px-1.5 py-0.5 mono text-[10px] ${STATE_STYLE[device.state] ?? ""}`}>
                {device.state}
              </span>
              <span className="ml-auto mono text-[11px] text-white/40">IRQ {device.irq}</span>
            </div>
            <div className="mt-1 grid grid-cols-3 gap-1 mono text-[11px] text-white/50">
              <span>reads {device.reads}</span>
              <span>writes {device.writes}</span>
              <span>irqs {device.irq_count}</span>
              <span>in {device.bytes_in} B</span>
              <span>out {device.bytes_out} B</span>
              <span>errors {device.errors}</span>
            </div>
            <div className="mt-2 flex flex-wrap gap-1.5 mono text-xs">
              <button
                onClick={() => setPower(!device.power)}
                className="rounded bg-white/10 px-2 py-1 text-white/80 hover:bg-white/15"
              >
                power {device.power ? "off" : "on"}
              </button>
              <button onClick={fault} className="rounded bg-red-500/20 px-2 py-1 text-red-200 hover:bg-red-500/30">
                inject fault
              </button>
              <button onClick={repair} className="rounded bg-emerald-500/20 px-2 py-1 text-emerald-200 hover:bg-emerald-500/30">
                repair
              </button>
            </div>
          </div>

          <div className="rounded border border-[#1e2a4a] bg-[#0d1225] p-2">
            <div className="mono text-[10px] tracking-widest text-white/40 mb-1.5">DRIVER I/O</div>
            {inputPanel(device)}
          </div>

          {readout !== null && (
            <div className="rounded border border-[#1e2a4a] bg-[#060a14] p-2 mono text-[11px] text-cyan-200 whitespace-pre-wrap break-all">
              <span className="text-white/35">read → </span>
              {readout || "(empty)"}
            </div>
          )}
        </div>
      </div>

      {msg && <div className="border-t border-[#1e2a4a] bg-amber-500/10 px-3 py-1 mono text-xs text-amber-200">{msg}</div>}
    </div>
  );
}
