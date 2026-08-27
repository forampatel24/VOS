/* Kernel snapshot types — mirrors jvk_snapshot() JSON exactly.
   Every field here must exist in the C kernel; no invented data. */

export interface CpuSnapshot {
  pc: number;
  sp: number;
  ir: number;
  halted: boolean;
  program_size: number;
  registers: Record<string, number>; // R0..R7
  flags: { Z: boolean; N: boolean; C: boolean };
}

export interface ClockSnapshot {
  speed_hz: number;
  quantum: number;
  ticks: number;
}

export interface ProcessEntry {
  pid: number;
  name: string;
  state: string; // READY | RUNNING | WAITING | SUSPENDED | TERMINATED
  priority: number;
  created_ticks: number;
  cpu_used: number;
}

export interface Queues {
  ready: number[];
  waiting: number[];
  suspended: number[];
  terminated: number[];
}

export interface MemoryStats {
  frames_used: number;
  frames_free: number;
  pages_mapped: number;
  allocs: number;
  frees: number;
  page_faults: number;
  swap_ins: number;
  swap_outs: number;
  segfaults: number;
  fragmented_allocs: number;
}

export interface MemoryConfig {
  total_frames: number;
  page_words: number;
  allocator: string;
  replacement: string;
  swap_enabled: boolean;
  swap_slots: number;
}

export interface PageEntry {
  vpage: number;
  frame: number;
  present: boolean;
  referenced: boolean;
  dirty: boolean;
}

export interface PageTable {
  pid: number;
  pages: PageEntry[];
}

export interface MemorySnapshot {
  config: MemoryConfig;
  stats: MemoryStats;
  swap_used: number;
  frame_map: number[]; // index=frame, value=owner pid or 0
  page_tables: PageTable[];
}

export interface KernelSnapshot {
  booted: boolean;
  shutdown: boolean;
  uptime_ticks: number;
  processes: number;
  memory: MemorySnapshot;
  process_list: ProcessEntry[];
  queues: Queues;
  cpu: CpuSnapshot;
  clock: ClockSnapshot;
}

export interface LogEntry {
  index: number;
  message: string;
}

export interface CommandResult {
  ok: boolean;
  error?: string;
  [key: string]: unknown;
}
