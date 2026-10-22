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
  burst_time: number;
  created_ticks: number;
  cpu_used: number;
  mem_pages: number;
  mem_budget_pages: number; // 0 = kernel default
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
  agent_budget_pages: number;
  page_table_strategy: string;
  present_bit: boolean;
  dirty_bit: boolean;
  referenced_bit: boolean;
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
  used: number;
  budget: number;
  pages: PageEntry[];
}

export interface MemorySnapshot {
  config: MemoryConfig;
  stats: MemoryStats;
  swap_used: number;
  frame_map: number[]; // index=frame, value=owner pid or 0
  page_tables: PageTable[];
}

export interface InterruptEntry {
  irq: string;
  priority: number;
  source: string;
  detail: string;
  enqueued_ticks: number;
  status: string;
}

export interface InterruptSnapshot {
  pending: number;
  handled: number;
  dropped: number;
  panic: boolean;
  panic_reason: string;
  queue: InterruptEntry[];
}

export interface ErrorManagerSnapshot {
  last_category: string;
  last_message: string;
  counts: Record<string, number>;
}

export interface SchedulerSnapshot {
  algo: string;
  current: number;
  next: number;
  switches: number;
  cpu_utilization: number; // 0..1
  avg_wait: number; // ticks
  avg_turnaround: number; // ticks
  completed: number;
  context_switch_irqs: number;
}

/* ---- M2 virtual machine / motherboard --------------------------------- */

export interface MachineSlot {
  name: string;
  kind: string; // cpu | ram | device
  detail: string;
  populated: boolean;
}

export interface MachineSnapshot {
  board: string;
  chipset: string;
  bus: string;
  bios: string;
  bus_speed_mhz: number;
  boot_device: string;
  power_on: boolean;
  post_ok: boolean;
  cpu_cores: number;
  ram_frames: number;
  device_slots: number;
  slots: MachineSlot[];
}

/* ---- M5 virtual filesystem -------------------------------------------- */

export interface FsConfig {
  total_blocks: number;
  block_size: number;
  max_inodes: number;
  max_blocks_per_file: number;
  max_open_files: number;
}

export interface FsStats {
  inodes_used: number;
  open_files: number;
  creates: number;
  deletes: number;
  mkdirs: number;
  reads: number;
  writes: number;
  renames: number;
  copies: number;
  searches: number;
  denied: number;
}

export interface DiskStats {
  total_blocks: number;
  block_size: number;
  used_blocks: number;
  free_blocks: number;
  largest_free_run: number;
  capacity_bytes: number;
  used_bytes: number;
  read_ops: number;
  write_ops: number;
  alloc_calls: number;
  free_calls: number;
}

export interface FsEntry {
  id: number;
  name: string;
  type: "file" | "dir";
  parent: number;
  path: string;
  size: number;
  owner: number;
  mode: string;
  mode_bits: number;
  block_count: number;
  blocks: number[];
  created_ticks: number;
  modified_ticks: number;
  hidden: boolean;
  readonly: boolean;
}

export interface OpenFile {
  fd: number;
  inode_id: number;
  name: string;
  path: string;
  owner: number;
  cursor: number;
  mode: string;
  opened_ticks: number;
}

export interface FilesystemSnapshot {
  config: FsConfig;
  stats: FsStats;
  disk: DiskStats;
  entries: FsEntry[];
  open_files: OpenFile[];
}

/* ---- M6 devices & drivers --------------------------------------------- */

export interface DeviceEntry {
  id: number;
  key: string;
  name: string;
  driver: string;
  state: "CONNECTED" | "DISCONNECTED" | "BUSY" | "WAITING" | "FAULT" | "DISABLED";
  irq: string;
  power: boolean;
  irq_count: number;
  reads: number;
  writes: number;
  errors: number;
  bytes_in: number;
  bytes_out: number;
  info: Record<string, unknown>;
}

export interface DeviceStats {
  interrupts: number;
  errors: number;
  reads: number;
  writes: number;
}

export interface DevicesSnapshot {
  count: number;
  stats: DeviceStats;
  list: DeviceEntry[];
}

/* ---- M7 IPC & synchronization ----------------------------------------- */

export interface IpcConfig {
  queue_capacity: number;
  pipe_capacity: number;
  shm_bytes: number;
}

export interface IpcStats {
  queues_created: number;
  messages_sent: number;
  messages_received: number;
  messages_dropped: number;
  segments_created: number;
  shm_maps: number;
  shm_writes: number;
  shm_reads: number;
  pipes_created: number;
  pipe_reads: number;
  pipe_writes: number;
  pipe_bytes: number;
  mutex_locks: number;
  mutex_unlocks: number;
  mutex_contentions: number;
  sem_waits: number;
  sem_posts: number;
  sem_contentions: number;
  blocked: number;
  woken: number;
  deadlocks: number;
  recoveries: number;
}

export interface IpcQueue {
  name: string;
  owner: number;
  capacity: number;
  count: number;
  sent: number;
  received: number;
  blocked_sends: number;
  blocked_recvs: number;
  senders: number[];
  receivers: number[];
}

export interface IpcSegment {
  name: string;
  owner: number;
  size: number;
  length: number;
  data: string;
  maps: number;
  writes: number;
  reads: number;
  attached: number[];
}

export interface IpcPipe {
  name: string;
  owner: number;
  capacity: number;
  count: number;
  bytes_in: number;
  bytes_out: number;
  reads: number;
  writes: number;
  readers: number[];
  writers: number[];
  read_waiters: number[];
  write_waiters: number[];
}

export interface IpcMutex {
  name: string;
  owner: number;
  recursion: number;
  locks: number;
  unlocks: number;
  contentions: number;
  waiters: number[];
}

export interface IpcSemaphore {
  name: string;
  value: number;
  max: number;
  waits: number;
  posts: number;
  contentions: number;
  holders: number[];
  waiters: number[];
}

export interface IpcDelivery {
  pid: number;
  bytes: number;
  data: string;
}

export interface IpcEdge {
  pid: number;
  target: number;
  kind: string;
  resource: string;
}

export interface IpcSnapshot {
  config: IpcConfig;
  stats: IpcStats;
  queues: IpcQueue[];
  segments: IpcSegment[];
  pipes: IpcPipe[];
  mutexes: IpcMutex[];
  semaphores: IpcSemaphore[];
  deliveries: IpcDelivery[];
  wait_graph: {
    edges: IpcEdge[];
    cycles: number[][];
    active: boolean;
  };
}

/* ---- M8 shell ---------------------------------------------------------- */

export interface ShellSnapshot {
  cwd: string;
  prompt: string;
  last_status: number;
  last_output: string;
}

export interface ShellExecResult {
  ok: boolean;
  output: string;
  prompt: string;
  cwd: string;
  clear: boolean;
  exit: boolean;
  command: string;
  error?: string;
}

export interface KernelSnapshot {
  booted: boolean;
  shutdown: boolean;
  uptime_ticks: number;
  processes: number;
  machine: MachineSnapshot;
  memory: MemorySnapshot;
  filesystem: FilesystemSnapshot;
  devices: DevicesSnapshot;
  ipc: IpcSnapshot;
  shell: ShellSnapshot;
  interrupts: InterruptSnapshot;
  error_manager: ErrorManagerSnapshot;
  scheduler: SchedulerSnapshot;
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
