import { motion } from "framer-motion";
import { useEffect, useState } from "react";

export default function BootScreen({ onDone }: { onDone?: () => void }) {
  const [phase, setPhase] = useState(0);

  useEffect(() => {
    const t1 = setTimeout(() => setPhase(1), 400);
    const t2 = setTimeout(() => setPhase(2), 900);
    const t3 = setTimeout(() => setPhase(3), 1500);
    const t4 = setTimeout(() => onDone?.(), 2200);
    return () => {
      clearTimeout(t1);
      clearTimeout(t2);
      clearTimeout(t3);
      clearTimeout(t4);
    };
  }, [onDone]);

  return (
    <motion.div
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
      exit={{ opacity: 0 }}
      className="fixed inset-0 z-[100] flex flex-col items-center justify-center bg-[#060a14] text-white"
    >
      {/* scanline overlay */}
      <div className="pointer-events-none absolute inset-0 opacity-[0.04] bg-[repeating-linear-gradient(0deg,transparent,transparent_2px,rgba(0,255,255,0.5)_2px,rgba(0,255,255,0.5)_3px)]" />

      <motion.div
        initial={{ scale: 0.9, opacity: 0 }}
        animate={{ scale: 1, opacity: 1 }}
        transition={{ duration: 0.6, ease: "easeOut" }}
        className="flex flex-col items-center gap-6 cursor-pointer"
        onClick={() => onDone?.()}
        title="Click to skip"
      >
        <div className="relative">
          <div className="absolute -inset-6 rounded-full border border-cyan-400/20 animate-pulse" />
          <div className="absolute -inset-10 rounded-full border border-cyan-400/10 animate-pulse [animation-delay:0.3s]" />
          <div className="h-20 w-20 rounded-2xl bg-gradient-to-br from-cyan-400 to-blue-600 flex items-center justify-center shadow-[0_0_30px_rgba(0,255,255,0.4)]">
            <span className="text-2xl font-bold tracking-widest mono">J</span>
          </div>
        </div>

        <div className="text-center">
          <h1 className="text-3xl font-bold tracking-[0.3em] mono">JARVIS OS</h1>
          <p className="mt-2 text-sm tracking-widest text-cyan-300/70 mono">KERNEL v0.1 • C-NATIVE</p>
        </div>

        <div className="mt-4 flex flex-col items-center gap-2 mono text-xs text-cyan-200/60">
          <span className={phase >= 1 ? "text-cyan-300" : "opacity-30"}>
            {phase >= 1 ? "✓" : "○"} kernel booted
          </span>
          <span className={phase >= 2 ? "text-cyan-300" : "opacity-30"}>
            {phase >= 2 ? "✓" : "○"} memory manager ready
          </span>
          <span className={phase >= 3 ? "text-cyan-300" : "opacity-30"}>
            {phase >= 3 ? "✓" : "○"} desktop shell initialized
          </span>
        </div>

        <div className="mt-6 h-1 w-48 overflow-hidden rounded-full bg-white/10">
          <motion.div
            className="h-full bg-gradient-to-r from-cyan-400 to-blue-500"
            initial={{ width: "0%" }}
            animate={{ width: phase === 0 ? "25%" : phase === 1 ? "55%" : phase === 2 ? "85%" : "100%" }}
            transition={{ duration: 0.5, ease: "easeOut" }}
          />
        </div>
        <p className="mono text-[10px] tracking-widest text-white/30">click to skip →</p>
      </motion.div>

      <p className="absolute bottom-8 mono text-[10px] tracking-widest text-white/20">JARVIS OS — Educational Operating System Simulator</p>
    </motion.div>
  );
}
