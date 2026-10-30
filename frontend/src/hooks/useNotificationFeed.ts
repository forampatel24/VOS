import { useEffect, useRef } from "react";
import { useKernelStore } from "@/store/kernelStore";
import { useShellStore } from "@/store/shellStore";
import { describeLog, shouldToast } from "@/lib/notifications";

/**
 * Turns newly arrived kernel log lines into desktop toasts. The first render
 * seeds the cursor with the current log head so the boot sequence does not
 * explode into a toast storm; only events from this session forward notify.
 */
export function useNotificationFeed() {
  const logs = useKernelStore((state) => state.logs);
  const pushToast = useShellStore((state) => state.pushToast);
  const cursor = useRef<number | null>(null);

  useEffect(() => {
    if (logs.length === 0) return;
    const head = logs[logs.length - 1].index;
    if (cursor.current === null) {
      cursor.current = head;
      return;
    }
    const fresh = logs.filter((entry) => entry.index > (cursor.current ?? 0));
    cursor.current = Math.max(cursor.current, head);
    if (fresh.length === 0) return;

    for (const entry of fresh.slice(-3)) {
      const notice = describeLog(entry.message);
      if (!shouldToast(notice.level)) continue;
      pushToast({
        level: notice.level,
        topic: notice.topic,
        message: entry.message,
      });
    }
  }, [logs, pushToast]);
}
