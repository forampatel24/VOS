import { AnimatePresence, motion } from "framer-motion";
import { useEffect } from "react";
import { describeLog, levelStyles } from "@/lib/notifications";
import { useShellStore } from "@/store/shellStore";
import { useWindowStore } from "@/store/windowStore";
import type { Toast } from "@/store/shellStore";

function ToastRow({ toast }: { toast: Toast }) {
  const dismiss = useShellStore((state) => state.dismissToast);
  const styles = levelStyles(toast.level);

  useEffect(() => {
    const timer = setTimeout(() => dismiss(toast.id), 6500);
    return () => clearTimeout(timer);
  }, [toast.id, dismiss]);

  return (
    <motion.button
      layout
      initial={{ opacity: 0, x: 40, scale: 0.97 }}
      animate={{ opacity: 1, x: 0, scale: 1 }}
      exit={{ opacity: 0, x: 30, scale: 0.97 }}
      transition={{ duration: 0.18, ease: "easeOut" }}
      onClick={() => {
        const destination = describeLog(toast.message).window;
        useWindowStore.getState().open(destination);
        dismiss(toast.id);
      }}
      className={`pointer-events-auto flex w-80 items-start gap-2.5 rounded-xl border bg-[#0d1225]/95 px-3 py-2.5 text-left shadow-[0_18px_50px_rgba(0,0,0,0.6)] backdrop-blur-xl ${styles.border}`}
    >
      <span className={`mt-1.5 h-1.5 w-1.5 shrink-0 rounded-full ${styles.dot}`} />
      <span className="min-w-0 flex-1">
        <span className={`mono text-[9px] tracking-[0.18em] ${styles.text}`}>
          {toast.topic.toUpperCase()} · {toast.level.toUpperCase()}
        </span>
        <span className="mt-0.5 block break-words mono text-[11px] leading-relaxed text-white/70">
          {toast.message}
        </span>
      </span>
      <span
        onClick={(event) => {
          event.stopPropagation();
          dismiss(toast.id);
        }}
        className="mono text-[10px] text-white/25 hover:text-white/70"
      >
        ✕
      </span>
    </motion.button>
  );
}

export default function Toasts() {
  const toasts = useShellStore((state) => state.toasts);
  return (
    <div className="pointer-events-none fixed right-3 top-3 z-[90] flex flex-col gap-2">
      <AnimatePresence>
        {toasts.map((toast) => (
          <ToastRow key={toast.id} toast={toast} />
        ))}
      </AnimatePresence>
    </div>
  );
}
