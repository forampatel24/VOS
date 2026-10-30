import { AnimatePresence, motion } from "framer-motion";
import { useEffect, useState } from "react";
import { restartSystem, shutdownSystem } from "@/api/client";
import { useShellStore } from "@/store/shellStore";

export default function PowerOverlay() {
  const dialog = useShellStore((state) => state.powerDialog);
  const close = useShellStore((state) => state.closePower);
  const requestPower = useShellStore((state) => state.requestPower);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    if (!dialog) {
      setBusy(false);
      setError(null);
    }
  }, [dialog]);

  useEffect(() => {
    if (!dialog || dialog === "halted") return;
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Escape" && !busy) close();
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [dialog, busy, close]);

  if (!dialog) return null;

  const restart = async () => {
    setBusy(true);
    setError(null);
    try {
      await restartSystem();
      window.location.reload();
    } catch (err) {
      setError(String(err));
      setBusy(false);
    }
  };

  const shutdown = async () => {
    setBusy(true);
    setError(null);
    try {
      const result = (await shutdownSystem()) as { ok?: boolean; error?: string };
      if (result.ok === false) throw new Error(result.error ?? "kernel rejected shutdown");
      setBusy(false);
      requestPower("halted");
    } catch (err) {
      setError(String(err));
      setBusy(false);
    }
  };

  return (
    <AnimatePresence>
      <motion.div
        initial={{ opacity: 0 }}
        animate={{ opacity: 1 }}
        exit={{ opacity: 0 }}
        className={`fixed inset-0 z-[120] flex flex-col items-center justify-center text-white ${
          dialog === "halted" ? "bg-black" : "bg-[#060a14]/90 backdrop-blur-md"
        }`}
      >
        {dialog === "halted" ? (
          <motion.div
            initial={{ opacity: 0, y: 10 }}
            animate={{ opacity: 1, y: 0 }}
            className="flex flex-col items-center gap-5 px-6 text-center"
          >
            <div className="grid h-16 w-16 place-items-center rounded-2xl border border-white/10 bg-white/5 mono text-xl text-white/40">
              J
            </div>
            <div>
              <h2 className="mono text-xl font-semibold tracking-[0.18em]">JARVIS OS HAS SHUT DOWN</h2>
              <p className="mt-2 mono text-[11px] tracking-widest text-white/35">
                The C kernel is halted. It is now safe to power off.
              </p>
            </div>
            <div className="mt-3 flex gap-2">
              <button
                onClick={() => void restart()}
                disabled={busy}
                className="rounded-lg border border-cyan-400/30 bg-cyan-500/10 px-4 py-2 mono text-[10px] tracking-[0.2em] text-cyan-200 hover:bg-cyan-500/20 disabled:opacity-40"
              >
                {busy ? "BOOTING…" : "RESTART"}
              </button>
              <button
                onClick={() => {
                  if (window.jarvis) void window.jarvis.close();
                  else window.close();
                }}
                className="rounded-lg border border-white/10 bg-white/5 px-4 py-2 mono text-[10px] tracking-[0.2em] text-white/55 hover:bg-white/10"
              >
                POWER OFF
              </button>
            </div>
            {error && <div className="mono text-[10px] text-red-300">{error}</div>}
          </motion.div>
        ) : (
          <motion.div
            initial={{ opacity: 0, y: 12, scale: 0.98 }}
            animate={{ opacity: 1, y: 0, scale: 1 }}
            className="w-[min(380px,calc(100vw-32px))] rounded-2xl border border-[#1e2a4a] bg-[#0d1225] p-5 shadow-[0_30px_100px_rgba(0,0,0,0.7)]"
          >
            <div className="mono text-[9px] tracking-[0.3em] text-cyan-300/60">POWER CONTROL</div>
            <h2 className="mt-1 mono text-lg font-semibold tracking-wide text-white/90">
              {dialog === "restart" ? "Restart JARVIS OS?" : "Shut down JARVIS OS?"}
            </h2>
            <p className="mt-2 text-[11px] leading-relaxed text-white/40">
              {dialog === "restart"
                ? "The bridge will re-init the C kernel with persisted boot settings, re-register Agent Studio profiles, then reload the desktop."
                : "The kernel shutdown sequence will halt every subsystem. Running agent tasks and unsaved kernel state are lost."}
            </p>
            {error && (
              <div className="mt-3 rounded-lg border border-red-400/30 bg-red-500/10 px-3 py-2 mono text-[10px] text-red-200">
                {error}
              </div>
            )}
            <div className="mt-4 flex justify-end gap-2">
              <button
                onClick={close}
                disabled={busy}
                className="rounded-lg border border-[#1e2a4a] bg-[#141b34] px-3 py-1.5 mono text-[10px] tracking-widest text-white/55 hover:text-white/85 disabled:opacity-40"
              >
                CANCEL
              </button>
              <button
                onClick={() => void (dialog === "restart" ? restart() : shutdown())}
                disabled={busy}
                className={`rounded-lg border px-3 py-1.5 mono text-[10px] tracking-widest disabled:opacity-40 ${
                  dialog === "restart"
                    ? "border-amber-400/30 bg-amber-500/15 text-amber-200 hover:bg-amber-500/25"
                    : "border-red-400/30 bg-red-500/15 text-red-200 hover:bg-red-500/25"
                }`}
              >
                {busy ? "WORKING…" : dialog === "restart" ? "RESTART" : "SHUT DOWN"}
              </button>
            </div>
          </motion.div>
        )}
      </motion.div>
    </AnimatePresence>
  );
}
