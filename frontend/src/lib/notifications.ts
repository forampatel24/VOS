import type { FixedWindowId } from "@/store/windowStore";
import type { NoticeLevel } from "@/store/shellStore";

const ERROR_WORDS = [
  "panic",
  "error",
  "denied",
  "deadlock",
  "fault",
  "failed",
  "segfault",
  "full",
  "unknown",
];
const WARN_WORDS = [
  "swap",
  "budget",
  "suspend",
  "kill",
  "dropped",
  "timeout",
  "blocked",
  "oom",
];
const SUCCESS_WORDS = [
  "task completed",
  "task done",
  "report ready",
  "completed",
  "launched",
  "saved",
  "created",
  "ready",
];

const TOPICS: { test: RegExp; topic: string; window: FixedWindowId }[] = [
  { test: /agent|task/, topic: "Agents", window: "agent-hub" },
  { test: /process|pcb|scheduler full/, topic: "Processes", window: "processes" },
  { test: /memory|frame|page|swap|oom|paging/, topic: "Memory", window: "memory" },
  { test: /file|inode|disk/, topic: "Filesystem", window: "files" },
  { test: /device|driver|spool|buffer/, topic: "Devices", window: "devices" },
  { test: /interrupt|irq|panic|error manager/, topic: "Interrupts", window: "interrupts" },
  { test: /ipc|mutex|semaphore|deadlock|pipe|queue/, topic: "IPC", window: "ipc" },
  { test: /scheduler|context_switch|quantum|cpu/, topic: "Scheduler", window: "cpu" },
  { test: /shell|command/, topic: "Shell", window: "terminal" },
  { test: /clock/, topic: "Clock", window: "cpu" },
  { test: /machine|post|bios|chipset/, topic: "Hardware", window: "system-info" },
];

export interface LogNotice {
  level: NoticeLevel;
  topic: string;
  window: FixedWindowId;
}

export function levelFor(message: string): NoticeLevel {
  const lowered = message.toLowerCase();
  if (ERROR_WORDS.some((word) => lowered.includes(word))) return "error";
  if (WARN_WORDS.some((word) => lowered.includes(word))) return "warning";
  if (SUCCESS_WORDS.some((word) => lowered.includes(word))) return "success";
  return "info";
}

export function topicFor(message: string): { topic: string; window: FixedWindowId } {
  const lowered = message.toLowerCase();
  const match = TOPICS.find((entry) => entry.test.test(lowered));
  return match
    ? { topic: match.topic, window: match.window }
    : { topic: "Kernel", window: "eventlog" };
}

export function describeLog(message: string): LogNotice {
  const level = levelFor(message);
  const { topic, window } = topicFor(message);
  return { level, topic, window };
}

export function shouldToast(level: NoticeLevel): boolean {
  return level === "warning" || level === "error" || level === "success";
}

export function levelStyles(level: NoticeLevel) {
  switch (level) {
    case "error":
      return {
        text: "text-red-200",
        border: "border-red-400/30",
        bg: "bg-red-500/10",
        dot: "bg-red-400",
      };
    case "warning":
      return {
        text: "text-amber-200",
        border: "border-amber-400/30",
        bg: "bg-amber-500/10",
        dot: "bg-amber-400",
      };
    case "success":
      return {
        text: "text-emerald-200",
        border: "border-emerald-400/30",
        bg: "bg-emerald-500/10",
        dot: "bg-emerald-400",
      };
    default:
      return {
        text: "text-cyan-200",
        border: "border-cyan-400/20",
        bg: "bg-cyan-500/5",
        dot: "bg-cyan-400",
      };
  }
}
