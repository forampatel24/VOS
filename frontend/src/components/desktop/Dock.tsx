import { appFor } from "@/lib/apps";
import { useKernelStore } from "@/store/kernelStore";
import { useShellStore } from "@/store/shellStore";
import { useWindowStore, type FixedWindowId, type WindowId } from "@/store/windowStore";

export default function Dock() {
  const windows = useWindowStore((state) => state.windows);
  const open = useWindowStore((state) => state.open);
  const focus = useWindowStore((state) => state.focus);
  const toggleMinimize = useWindowStore((state) => state.toggleMinimize);
  const pinned = useShellStore((state) => state.pinned);
  const togglePin = useShellStore((state) => state.togglePin);
  const toggleLauncher = useShellStore((state) => state.toggleLauncher);
  const instances = useKernelStore((state) => state.snapshot?.agents.instances ?? []);
  const definitions = useKernelStore((state) => state.snapshot?.agents.definitions ?? []);

  const activeId = Object.values(windows)
    .filter((win) => win.open && !win.minimized)
    .sort((a, b) => b.z - a.z)[0]?.id;

  const activate = (id: WindowId) => {
    const win = windows[id];
    if (!win?.open) open(id);
    else if (win.minimized) open(id);
    else if (activeId === id) toggleMinimize(id);
    else focus(id);
  };

  const pin = (id: FixedWindowId) => {
    togglePin(id);
  };

  return (
    <div className="absolute bottom-3 left-1/2 z-30 -translate-x-1/2">
      <div className="flex items-end gap-1.5 rounded-2xl border border-[#1e2a4a] bg-[#0d1225]/82 px-2 py-1.5 shadow-[0_14px_44px_rgba(0,0,0,0.55)] backdrop-blur-xl">
        <button
          onClick={toggleLauncher}
          className="group relative grid h-10 w-10 place-items-center rounded-xl border text-cyan-200 transition hover:-translate-y-1"
          style={{
            borderColor: "color-mix(in srgb, var(--jarvis-primary) 25%, transparent)",
            background: "color-mix(in srgb, var(--jarvis-primary) 10%, transparent)",
          }}
          title="All applications"
        >
          <span className="mono text-sm">▦</span>
          <span className="pointer-events-none absolute -top-8 hidden whitespace-nowrap rounded border border-[#1e2a4a] bg-[#060a14] px-2 py-1 mono text-[9px] tracking-widest text-white/70 group-hover:block">
            ALL APPS
          </span>
        </button>
        <span className="mx-0.5 h-7 w-px self-center bg-[#1e2a4a]" />

        {pinned.map((id) => {
          const app = appFor(id);
          if (!app) return null;
          const win = windows[id];
          const isActive = activeId === id;
          return (
            <button
              key={id}
              onClick={() => activate(id)}
              onContextMenu={(event) => {
                event.preventDefault();
                pin(id);
              }}
              className={`group relative grid h-10 w-10 place-items-center rounded-xl border transition hover:-translate-y-1 ${
                isActive
                  ? ""
                  : "border-[#1e2a4a] bg-[#141b34] hover:bg-[#1a2444]"
              }`}
              style={
                isActive
                  ? {
                      borderColor: "color-mix(in srgb, var(--jarvis-primary) 45%, transparent)",
                      background: "color-mix(in srgb, var(--jarvis-primary) 20%, transparent)",
                    }
                  : undefined
              }
              title={`${app.label} · right-click to ${pinned.includes(id) ? "unpin" : "pin"}`}
            >
              <span className="mono text-[9px] font-bold text-cyan-200">{app.icon}</span>
              <span className={`absolute -bottom-0.5 h-1 w-1 rounded-full ${win?.open ? app.dot : "bg-transparent"}`} />
              <span className="pointer-events-none absolute -top-8 hidden whitespace-nowrap rounded border border-[#1e2a4a] bg-[#060a14] px-2 py-1 mono text-[9px] tracking-wide text-white/70 group-hover:block">
                {app.label}
              </span>
            </button>
          );
        })}

        {instances.length > 0 && <span className="mx-0.5 h-7 w-px self-center bg-[#1e2a4a]" />}

        {instances.slice(0, 6).map((instance) => {
          const id = `agent-${instance.pid}` as WindowId;
          const win = windows[id];
          const definition = definitions.find((entry) => entry.key === instance.key);
          const color = definition?.color ?? "#22d3ee";
          return (
            <button
              key={id}
              onClick={() => activate(id)}
              className={`group relative grid h-10 w-10 place-items-center rounded-xl border transition hover:-translate-y-1 ${
                activeId === id ? "ring-1 ring-emerald-300/50" : ""
              }`}
              style={{ background: `${color}1f`, borderColor: `${color}55` }}
              title={`${instance.name} · PID ${instance.pid}`}
            >
              <span className="mono text-[9px] font-bold" style={{ color }}>
                {instance.name.slice(0, 2).toUpperCase()}
              </span>
              <span
                className={`absolute -bottom-0.5 h-1 w-1 rounded-full ${
                  win?.open ? (instance.state === "RUNNING" ? "bg-emerald-300" : "bg-amber-300") : "bg-transparent"
                }`}
              />
              <span className="pointer-events-none absolute -top-8 hidden whitespace-nowrap rounded border border-[#1e2a4a] bg-[#060a14] px-2 py-1 mono text-[9px] tracking-wide text-white/70 group-hover:block">
                {instance.name} · PID {instance.pid}
              </span>
            </button>
          );
        })}
      </div>
    </div>
  );
}
