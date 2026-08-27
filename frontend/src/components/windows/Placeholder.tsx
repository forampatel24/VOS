export default function Placeholder({ title, milestone }: { title: string; milestone: string }) {
  return (
    <div className="flex h-full flex-col items-center justify-center gap-4 p-8 text-center">
      <div className="h-12 w-12 rounded-xl border border-dashed border-white/15 grid place-items-center">
        <span className="text-xl opacity-30">◌</span>
      </div>
      <div>
        <h3 className="mono text-sm font-semibold tracking-widest text-white/80">{title}</h3>
        <p className="mt-2 mono text-xs text-white/40 max-w-sm">
          Subsystem offline — arrives in <span className="text-cyan-300">{milestone}</span>.
          <br />
          The kernel will expose this subsystem&apos;s state over the same JSON ABI; the UI will render it the moment it ships.
        </p>
      </div>
      <div className="mono text-[10px] tracking-widest text-white/20 border border-white/10 rounded px-2 py-1">
        Honest placeholder — no fake data
      </div>
    </div>
  );
}
