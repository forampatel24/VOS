import { AnimatePresence, motion } from "framer-motion";
import { useEffect, useState } from "react";
import { describeLog, levelStyles } from "@/lib/notifications";
import { useKernelStore } from "@/store/kernelStore";
import { useShellStore } from "@/store/shellStore";
import { useWindowStore } from "@/store/windowStore";

type Filter = "all" | "alerts";

export default function NotificationCenter() {
  const open = useShellStore((state) => state.notificationsOpen);
  const close = useShellStore((state) => state.closeNotifications);
  const markRead = useShellStore((state) => state.markNotificationsRead);
  const logs = useKernelStore((state) => state.logs);
  const [filter, setFilter] = useState<Filter>("all");

  useEffect(() => {
    if (!open) return;
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Escape") close();
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [open, close]);

  useEffect(() => {
    if (!open || logs.length === 0) return;
    markRead(logs[logs.length - 1].index);
  }, [open, logs, markRead]);

  const rows = [...logs].reverse().map((entry) => ({
    ...entry,
    ...describeLog(entry.message),
  }));
  const visible = filter === "alerts" ? rows.filter((row) => row.level === "warning" || row.level === "error") : rows;
  const alerts = rows.filter((row) => row.level === "warning" || row.level === "error").length;

  return (
    <AnimatePresence>
      {open && (
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          onPointerDown={close}
          className="fixed inset-0 z-[70] bg-black/25"
        >
          <motion.div
            initial={{ opacity: 0, x: 24, scale: 0.98 }}
            animate={{ opacity: 1, x: 0, scale: 1 }}
            exit={{ opacity: 0, x: 16, scale: 0.98 }}
            transition={{ duration: 0.16, ease: "easeOut" }}
            onPointerDown={(event) => event.stopPropagation()}
            className="absolute right-3 top-3 flex max-h-[74vh] w-[min(380px,calc(100vw-24px))] flex-col overflow-hidden rounded-2xl border border-[#1e2a4a] bg-[#0d1225]/97 shadow-[0_24px_80px_rgba(0,0,0,0.7)] backdrop-blur-xl"
          >
            <div className="flex items-center justify-between border-b border-[#1e2a4a] px-3 py-2.5">
              <div>
                <div className="mono text-xs font-semibold tracking-widest text-cyan-200">NOTIFICATIONS</div>
                <div className="mono text-[9px] tracking-widest text-white/30">
                  KERNEL EVENT FEED · {alerts} ALERT{alerts === 1 ? "" : "S"}
                </div>
              </div>
              <button
                onClick={() => {
                  const head = useKernelStore.getState().logs;
                  if (head.length > 0) markRead(head[head.length - 1].index);
                }}
                className="rounded border border-[#1e2a4a] px-2 py-1 mono text-[9px] tracking-widest text-white/45 hover:bg-white/5 hover:text-white/80"
              >
                MARK READ
              </button>
            </div>

            <div className="flex gap-1 border-b border-[#1e2a4a] px-2 py-1.5">
              {(["all", "alerts"] as const).map((value) => (
                <button
                  key={value}
                  onClick={() => setFilter(value)}
                  className={`rounded px-2.5 py-1 mono text-[9px] tracking-widest transition ${
                    filter === value
                      ? "bg-cyan-500/20 text-cyan-200"
                      : "text-white/35 hover:bg-white/5 hover:text-white/65"
                  }`}
                >
                  {value.toUpperCase()}
                </button>
              ))}
              <span className="ml-auto self-center mono text-[9px] text-white/25">
                {visible.length} shown
              </span>
            </div>

            <div className="min-h-0 flex-1 overflow-y-auto p-1.5">
              {visible.length === 0 ? (
                <div className="px-4 py-10 text-center mono text-xs text-white/30">
                  No notifications in this view.
                </div>
              ) : (
                visible.map((row) => {
                  const styles = levelStyles(row.level);
                  return (
                    <button
                      key={row.index}
                      onClick={() => {
                        useWindowStore.getState().open(row.window);
                        close();
                      }}
                      className={`mb-1 flex w-full items-start gap-2.5 rounded-lg border px-2.5 py-2 text-left transition hover:bg-white/5 ${styles.border} ${styles.bg}`}
                    >
                      <span className={`mt-1.5 h-1.5 w-1.5 shrink-0 rounded-full ${styles.dot}`} />
                      <span className="min-w-0 flex-1">
                        <span className="flex items-center gap-2">
                          <span className={`mono text-[9px] tracking-widest ${styles.text}`}>
                            {row.topic.toUpperCase()}
                          </span>
                          <span className="mono ml-auto text-[9px] text-white/20">#{row.index}</span>
                        </span>
                        <span className="mt-0.5 block break-words mono text-[11px] leading-relaxed text-white/70">
                          {row.message}
                        </span>
                      </span>
                    </button>
                  );
                })
              )}
            </div>

            <div className="border-t border-[#1e2a4a] px-3 py-2 mono text-[9px] tracking-wide text-white/25">
              Every entry is a real kernel log line. Click one to open its subsystem.
            </div>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
}
