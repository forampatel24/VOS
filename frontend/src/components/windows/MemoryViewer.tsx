import { useState } from "react";
import { useKernelStore } from "@/store/kernelStore";

export default function MemoryViewer() {
  const snap = useKernelStore((s) => s.snapshot);
  const command = useKernelStore((s) => s.command);
  const [pid, setPid] = useState("1");
  const [pages, setPages] = useState("2");
  const [addr, setAddr] = useState("0");
  const [val, setVal] = useState("42");
  const [msg, setMsg] = useState<string | null>(null);

  if (!snap) return <div className="p-6 mono text-xs text-white/40">Waiting for kernel snapshot…</div>;

  const { memory } = snap;
  const total = memory.config.total_frames;
  const used = memory.stats.frames_used;

  const doAlloc = async () => {
    setMsg(null);
    const res = (await command({ action: "mem_alloc", pid: Number(pid), pages: Number(pages) })) as {
      ok: boolean;
      error?: string;
      frames?: number[];
      contiguous?: boolean;
    };
    setMsg(res.ok ? `allocated ${pages} page(s) → frames [${res.frames?.join(", ")}] ${res.contiguous ? "(contiguous)" : "(fragmented)"}` : `error: ${res.error}`);
  };

  const doWrite = async () => {
    setMsg(null);
    const res = (await command({ action: "mem_write", pid: Number(pid), addr: Number(addr), value: Number(val) })) as {
      ok: boolean;
      error?: string;
    };
    setMsg(res.ok ? `wrote ${val} to pid ${pid} addr ${addr}` : `error: ${res.error}`);
  };

  const doRead = async () => {
    setMsg(null);
    const res = (await command({ action: "mem_read", pid: Number(pid), addr: Number(addr) })) as {
      ok: boolean;
      error?: string;
      value?: number;
    };
    setMsg(res.ok ? `read pid ${pid} addr ${addr} = ${res.value}` : `error: ${res.error}`);
  };

  const doFree = async () => {
    setMsg(null);
    const res = (await command({ action: "mem_free", pid: Number(pid) })) as { ok: boolean; error?: string; pages_freed?: number };
    setMsg(res.ok ? `freed ${res.pages_freed} page(s) from pid ${pid}` : `error: ${res.error}`);
  };

  return (
    <div className="flex h-full flex-col">
      {/* stats bar */}
      <div className="grid grid-cols-3 gap-2 p-2 border-b border-[#1e2a4a] bg-[#0d1225]">
        <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 mono text-xs">
          <div className="text-white/40 tracking-widest text-[10px]">FRAMES</div>
          <div className="text-white font-bold">
            {used}/{total} <span className="text-white/40 font-normal">({total > 0 ? ((used / total) * 100).toFixed(0) : 0}%)</span>
          </div>
          <div className="text-white/30 text-[11px]">
            {memory.stats.pages_mapped} pages mapped
          </div>
        </div>
        <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 mono text-xs">
          <div className="text-white/40 tracking-widest text-[10px]">POLICIES</div>
          <div className="text-white">{memory.config.allocator}</div>
          <div className="text-white/50">{memory.config.replacement}</div>
          <div className="text-cyan-300 text-[11px]">swap {memory.config.swap_enabled ? "on" : "off"} · {memory.swap_used}/{memory.config.swap_slots}</div>
        </div>
        <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 mono text-xs">
          <div className="text-white/40 tracking-widest text-[10px]">FAULTS</div>
          <div className="text-white">
            {memory.stats.page_faults} faults · {memory.stats.swap_ins} in / {memory.stats.swap_outs} out
          </div>
          <div className="text-white/30 text-[11px]">
            segfaults {memory.stats.segfaults} · frag {memory.stats.fragmented_allocs}
          </div>
        </div>
      </div>

      {/* frame map */}
      <div className="p-2 border-b border-[#1e2a4a]">
        <div className="mono text-[10px] tracking-widest text-white/40 mb-1">FRAME MAP — index is physical frame, value is owner PID (0 = free)</div>
        <div className="flex flex-wrap gap-1">
          {memory.frame_map.map((owner, idx) => (
            <div
              key={idx}
              title={`Frame ${idx}: ${owner === 0 ? "FREE" : `PID ${owner}`}`}
              className={`h-6 min-w-6 px-1 grid place-items-center rounded text-[10px] mono font-bold border ${
                owner === 0 ? "bg-[#141b34] border-[#1e2a4a] text-white/20" : "bg-cyan-500/20 border-cyan-400/30 text-cyan-200"
              }`}
            >
              {owner === 0 ? "·" : owner}
            </div>
          ))}
        </div>
      </div>

      {/* controls */}
      <div className="flex flex-wrap gap-2 p-2 border-b border-[#1e2a4a] bg-[#0d1225] mono text-xs">
        <label className="flex items-center gap-1 text-white/60">
          PID <input value={pid} onChange={(e) => setPid(e.target.value)} className="w-12 rounded border border-[#1e2a4a] bg-[#060a14] px-1 py-1 text-white" />
        </label>
        <label className="flex items-center gap-1 text-white/60">
          pages <input value={pages} onChange={(e) => setPages(e.target.value)} className="w-12 rounded border border-[#1e2a4a] bg-[#060a14] px-1 py-1 text-white" />
        </label>
        <button onClick={doAlloc} className="rounded bg-cyan-500 px-2 py-1 font-semibold tracking-widest text-black hover:bg-cyan-400">
          ALLOC
        </button>
        <button onClick={doFree} className="rounded bg-white/10 px-2 py-1 text-white/80 hover:bg-white/15">
          FREE
        </button>
        <span className="text-white/20">|</span>
        <label className="flex items-center gap-1 text-white/60">
          addr <input value={addr} onChange={(e) => setAddr(e.target.value)} className="w-14 rounded border border-[#1e2a4a] bg-[#060a14] px-1 py-1 text-white" />
        </label>
        <label className="flex items-center gap-1 text-white/60">
          val <input value={val} onChange={(e) => setVal(e.target.value)} className="w-14 rounded border border-[#1e2a4a] bg-[#060a14] px-1 py-1 text-white" />
        </label>
        <button onClick={doWrite} className="rounded bg-violet-500/30 px-2 py-1 text-violet-200 hover:bg-violet-500/40">
          WRITE
        </button>
        <button onClick={doRead} className="rounded bg-white/10 px-2 py-1 text-white/80 hover:bg-white/15">
          READ
        </button>
      </div>

      {msg && <div className="px-3 py-1 mono text-xs border-b border-[#1e2a4a] bg-amber-500/10 text-amber-200">{msg}</div>}

      {/* page tables */}
      <div className="flex-1 overflow-auto p-2 space-y-2">
        {memory.page_tables.length === 0 ? (
          <div className="mono text-xs text-white/30 p-4 text-center">No address spaces — allocate memory for a PID to see its page table.</div>
        ) : (
          memory.page_tables.map((pt) => (
            <div key={pt.pid} className="rounded border border-[#1e2a4a] bg-[#0d1225] p-2">
              <div className="mono text-xs font-semibold tracking-widest text-cyan-200 mb-1">
                PID {pt.pid} — {pt.pages.length} page(s)
              </div>
              <div className="flex flex-wrap gap-1">
                {pt.pages.map((pg) => (
                  <div
                    key={pg.vpage}
                    className={`rounded px-2 py-1 mono text-xs border ${
                      pg.present ? "bg-emerald-500/15 border-emerald-400/30 text-emerald-200" : "bg-amber-500/15 border-amber-400/30 text-amber-200"
                    }`}
                    title={`vpage ${pg.vpage} → frame ${pg.frame} · present ${pg.present} · dirty ${pg.dirty}`}
                  >
                    <span className="text-white/60">v{pg.vpage}</span> → f{pg.frame}
                    <span className="ml-1 text-[10px] opacity-60">{pg.present ? "resident" : "swapped"}</span>
                    {pg.dirty && <span className="ml-1 text-[10px] text-amber-300">dirty</span>}
                  </div>
                ))}
              </div>
            </div>
          ))
        )}
      </div>
    </div>
  );
}
