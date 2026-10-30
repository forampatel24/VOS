import { AnimatePresence, motion } from "framer-motion";
import { useEffect } from "react";
import { APP_CATALOG } from "@/lib/apps";
import { useShellStore } from "@/store/shellStore";
import { useWindowStore } from "@/store/windowStore";

export default function LauncherMenu() {
  const open = useShellStore((state) => state.launcherOpen);
  const close = useShellStore((state) => state.closeLauncher);
  const openSearch = useShellStore((state) => state.openSearch);
  const pinned = useShellStore((state) => state.pinned);
  const togglePin = useShellStore((state) => state.togglePin);
  const windows = useWindowStore((state) => state.windows);

  useEffect(() => {
    if (!open) return;
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Escape") close();
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [open, close]);

  return (
    <AnimatePresence>
      {open && (
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          onPointerDown={close}
          className="fixed inset-0 z-[73] bg-black/10"
        >
          <motion.div
            initial={{ opacity: 0, y: 14, scale: 0.98 }}
            animate={{ opacity: 1, y: 0, scale: 1 }}
            exit={{ opacity: 0, y: 10, scale: 0.98 }}
            transition={{ duration: 0.16, ease: "easeOut" }}
            onPointerDown={(event) => event.stopPropagation()}
            className="absolute bottom-12 left-3 w-[min(520px,calc(100vw-24px))] overflow-hidden rounded-2xl border border-[#1e2a4a] bg-[#0d1225]/97 shadow-[0_24px_80px_rgba(0,0,0,0.7)] backdrop-blur-xl"
          >
            <div className="flex items-center justify-between border-b border-[#1e2a4a] px-3 py-2.5">
              <div>
                <div className="mono text-xs font-semibold tracking-widest text-cyan-200">APPLICATIONS</div>
                <div className="mono text-[9px] tracking-widest text-white/30">JARVIS OS · ALL SUBSYSTEM WINDOWS</div>
              </div>
              <button
                onClick={openSearch}
                className="rounded border border-cyan-400/25 bg-cyan-500/10 px-2 py-1 mono text-[9px] tracking-widest text-cyan-200 hover:bg-cyan-500/20"
              >
                SEARCH
              </button>
            </div>

            <div className="grid grid-cols-2 gap-1.5 p-2.5 sm:grid-cols-3">
              {APP_CATALOG.map((app) => {
                const win = windows[app.id];
                return (
                  <button
                    key={app.id}
                    onClick={() => {
                      useWindowStore.getState().open(app.id);
                      close();
                    }}
                    onContextMenu={(event) => {
                      event.preventDefault();
                      togglePin(app.id);
                    }}
                    className="group flex items-start gap-2 rounded-lg border border-[#1e2a4a] bg-[#141b34] px-2 py-2 text-left transition hover:border-cyan-400/25 hover:bg-[#1a2444]"
                    title={`${app.label} — right-click to ${pinned.includes(app.id) ? "unpin" : "pin"}`}
                  >
                    <span className="relative grid h-8 w-8 shrink-0 place-items-center rounded-lg border border-[#1e2a4a] bg-[#060a14] mono text-[9px] font-bold text-cyan-200">
                      {app.icon}
                      {win?.open && <span className={`absolute -right-0.5 -top-0.5 h-1.5 w-1.5 rounded-full ${app.dot}`} />}
                    </span>
                    <span className="min-w-0">
                      <span className="flex items-center gap-1.5">
                        <span className="truncate mono text-[11px] text-white/75 group-hover:text-white">
                          {app.label}
                        </span>
                        {pinned.includes(app.id) && (
                          <span className="h-1 w-1 shrink-0 rounded-full bg-cyan-300" title="Pinned to dock" />
                        )}
                      </span>
                      <span className="mt-0.5 block truncate mono text-[9px] text-white/30">
                        {app.description}
                      </span>
                    </span>
                  </button>
                );
              })}
            </div>

            <div className="flex items-center gap-3 border-t border-[#1e2a4a] px-3 py-2 mono text-[9px] tracking-widest text-white/25">
              <span>RIGHT-CLICK DOCK ICONS TO PIN</span>
              <span className="ml-auto">CTRL K</span>
            </div>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
}
