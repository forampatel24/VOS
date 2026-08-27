import { motion } from "framer-motion";
import { useRef, useState } from "react";
import { useWindowStore, WindowId } from "@/store/windowStore";

export default function Window({
  id,
  title,
  children,
  onClose,
}: {
  id: WindowId;
  title: string;
  children: React.ReactNode;
  onClose: () => void;
}) {
  const { windows, focus, move, resize } = useWindowStore();
  const w = windows[id];
  const ref = useRef<HTMLDivElement>(null);
  const [dragging, setDragging] = useState(false);
  const [resizing, setResizing] = useState(false);

  // drag handlers
  const onHeaderPointerDown = (e: React.PointerEvent) => {
    setDragging(true);
    focus(id);
    const startX = e.clientX - w.x;
    const startY = e.clientY - w.y;
    const onMove = (ev: PointerEvent) => {
      move(id, ev.clientX - startX, ev.clientY - startY);
    };
    const onUp = () => {
      setDragging(false);
      window.removeEventListener("pointermove", onMove);
      window.removeEventListener("pointerup", onUp);
    };
    window.addEventListener("pointermove", onMove);
    window.addEventListener("pointerup", onUp);
  };

  const onResizePointerDown = (e: React.PointerEvent) => {
    e.stopPropagation();
    setResizing(true);
    const startX = e.clientX;
    const startY = e.clientY;
    const startW = w.w;
    const startH = w.h;
    const onMove = (ev: PointerEvent) => {
      resize(id, startW + ev.clientX - startX, startH + ev.clientY - startY);
    };
    const onUp = () => {
      setResizing(false);
      window.removeEventListener("pointermove", onMove);
      window.removeEventListener("pointerup", onUp);
    };
    window.addEventListener("pointermove", onMove);
    window.addEventListener("pointerup", onUp);
  };

  return (
    <motion.div
      ref={ref}
      initial={{ opacity: 0, scale: 0.97, y: 8 }}
      animate={{ opacity: 1, scale: 1, y: 0 }}
      exit={{ opacity: 0, scale: 0.96 }}
      transition={{ duration: 0.18, ease: "easeOut" }}
      onPointerDown={() => focus(id)}
      style={{
        left: w.x,
        top: w.y,
        width: w.w,
        height: w.h,
        zIndex: w.z,
      }}
      className={`absolute flex flex-col overflow-hidden rounded-xl border border-[#1e2a4a] bg-[#0d1225]/95 backdrop-blur-xl shadow-[0_20px_60px_rgba(0,0,0,0.6),0_0_0_1px_rgba(0,255,255,0.08)] select-none ${
        dragging || resizing ? "select-none" : ""
      }`}
    >
      {/* titlebar */}
      <div
        onPointerDown={onHeaderPointerDown}
        className="flex h-9 shrink-0 items-center justify-between border-b border-[#1e2a4a] bg-[#141b34] px-3 cursor-grab active:cursor-grabbing"
      >
        <div className="flex items-center gap-2 min-w-0">
          <span className="h-2.5 w-2.5 rounded-full bg-cyan-400 shadow-[0_0_8px_rgba(0,255,255,0.6)]" />
          <span className="mono text-xs font-semibold tracking-widest text-cyan-100 truncate">{title}</span>
        </div>
        <div className="flex items-center gap-1">
          <button
            onClick={(e) => {
              e.stopPropagation();
              // minimize is visual only for now
            }}
            className="h-6 w-6 grid place-items-center rounded hover:bg-white/10 text-white/60 hover:text-white text-xs"
            title="Minimize"
          >
            ─
          </button>
          <button
            onClick={(e) => {
              e.stopPropagation();
              onClose();
            }}
            className="h-6 w-6 grid place-items-center rounded hover:bg-red-500/20 text-white/60 hover:text-red-300 text-xs"
            title="Close"
          >
            ✕
          </button>
        </div>
      </div>

      {/* body */}
      <div className="flex-1 overflow-auto bg-[#060a14] p-0 text-sm select-text">{children}</div>

      {/* resize handle */}
      <div
        onPointerDown={onResizePointerDown}
        className="absolute bottom-0 right-0 h-5 w-5 cursor-nwse-resize grid place-items-center opacity-40 hover:opacity-80"
      >
        <div className="h-3 w-3 border-r-2 border-b-2 border-white/30 rounded-br" />
      </div>
    </motion.div>
  );
}
