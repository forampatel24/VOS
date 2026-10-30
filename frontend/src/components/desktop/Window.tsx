import { motion } from "framer-motion";
import { useRef, useState } from "react";
import { useWindowStore, type WindowId } from "@/store/windowStore";

const TASKBAR_HEIGHT = 44;

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
  const { windows, focus, move, resize, toggleMinimize, toggleMaximize, snapWindow, restoreWindow } =
    useWindowStore();
  const w = windows[id];
  const ref = useRef<HTMLDivElement>(null);
  const [dragging, setDragging] = useState(false);
  const [resizing, setResizing] = useState(false);

  const onHeaderPointerDown = (e: React.PointerEvent) => {
    if (e.button !== 0) return;
    if ((e.target as HTMLElement).closest("button")) return;
    focus(id);

    const base = w.snap && w.restore ? w.restore : w;
    const offsetX = Math.min(Math.max(e.clientX - base.x, 20), base.w - 20);
    const offsetY = Math.min(Math.max(e.clientY - base.y, 0), 34);
    let didRestore = !w.snap;
    setDragging(true);

    const onMove = (ev: PointerEvent) => {
      if (!didRestore) {
        didRestore = true;
        restoreWindow(id);
      }
      const maxX = Math.max(0, window.innerWidth - 120);
      const maxY = Math.max(0, window.innerHeight - TASKBAR_HEIGHT - 40);
      const x = Math.min(Math.max(ev.clientX - offsetX, 120 - base.w), maxX);
      const y = Math.min(Math.max(ev.clientY - offsetY, 0), maxY);
      move(id, x, y);
    };
    const onUp = (ev: PointerEvent) => {
      setDragging(false);
      window.removeEventListener("pointermove", onMove);
      window.removeEventListener("pointerup", onUp);
      if (ev.clientX <= 8) snapWindow(id, "left");
      else if (ev.clientX >= window.innerWidth - 8) snapWindow(id, "right");
      else if (ev.clientY <= 8) snapWindow(id, "top");
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

  const maximized = w.snap === "maximized";

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
        dragging || resizing ? "select-none" : "transition-[left,top,width,height] duration-150"
      }`}
    >
      <div
        onPointerDown={onHeaderPointerDown}
        onDoubleClick={(e) => {
          if ((e.target as HTMLElement).closest("button")) return;
          toggleMaximize(id);
        }}
        className="flex h-9 shrink-0 items-center justify-between border-b border-[#1e2a4a] bg-[#141b34] px-3 cursor-grab active:cursor-grabbing"
      >
        <div className="flex min-w-0 items-center gap-2">
          <span
            className="h-2.5 w-2.5 rounded-full"
            style={{
              background: "var(--jarvis-primary)",
              boxShadow:
                "0 0 8px color-mix(in srgb, var(--jarvis-primary) 65%, transparent)",
            }}
          />
          <span className="truncate mono text-xs font-semibold tracking-widest text-cyan-100">
            {title}
          </span>
        </div>
        <div className="flex items-center gap-1">
          <button
            onClick={(e) => {
              e.stopPropagation();
              toggleMinimize(id);
            }}
            className="grid h-6 w-6 place-items-center rounded text-xs text-white/60 hover:bg-white/10 hover:text-white"
            title="Minimize"
          >
            ─
          </button>
          <button
            onClick={(e) => {
              e.stopPropagation();
              toggleMaximize(id);
            }}
            className="grid h-6 w-6 place-items-center rounded text-[10px] text-white/60 hover:bg-white/10 hover:text-white"
            title={maximized ? "Restore" : "Maximize"}
          >
            {maximized ? "❐" : "□"}
          </button>
          <button
            onClick={(e) => {
              e.stopPropagation();
              onClose();
            }}
            className="grid h-6 w-6 place-items-center rounded text-xs text-white/60 hover:bg-red-500/20 hover:text-red-300"
            title="Close"
          >
            ✕
          </button>
        </div>
      </div>

      <div className="flex-1 overflow-auto bg-[#060a14] p-0 text-sm select-text">{children}</div>

      {!w.snap && (
        <div
          onPointerDown={onResizePointerDown}
          className="absolute bottom-0 right-0 grid h-5 w-5 cursor-nwse-resize place-items-center opacity-40 hover:opacity-80"
        >
          <div className="h-3 w-3 rounded-br border-b-2 border-r-2 border-white/30" />
        </div>
      )}
    </motion.div>
  );
}
