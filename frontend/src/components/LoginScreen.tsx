import { motion } from "framer-motion";
import { useEffect, useRef, useState } from "react";
import { login as loginRequest } from "@/api/client";
import { useKernelStore } from "@/store/kernelStore";
import { useSessionStore } from "@/store/sessionStore";

function loginError(error: unknown): string {
  const value = error as {
    response?: { status?: number; data?: { detail?: unknown } };
    message?: string;
  };
  if (value.response?.status === 401) return "Incorrect passcode.";
  const detail = value.response?.data?.detail;
  if (typeof detail === "string") return detail;
  return "Bridge offline — start uvicorn backend.main:app --port 8000";
}

export default function LoginScreen() {
  const signIn = useSessionStore((state) => state.login);
  const connected = useKernelStore((state) => state.connected);
  const [password, setPassword] = useState("");
  const [show, setShow] = useState(false);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [now, setNow] = useState(() => new Date());
  const inputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    const timer = setInterval(() => setNow(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  useEffect(() => {
    inputRef.current?.focus();
  }, []);

  const submit = async (event: React.FormEvent) => {
    event.preventDefault();
    if (busy || password.length === 0) return;
    setBusy(true);
    setError(null);
    try {
      const user = await loginRequest("admin", password);
      signIn(user);
    } catch (err) {
      setError(loginError(err));
      setPassword("");
      inputRef.current?.focus();
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="relative flex h-full w-full items-center justify-center overflow-hidden bg-[#060a14] px-5 text-white">
      <div
        className="absolute inset-0"
        style={{
          background:
            "radial-gradient(circle at 25% 30%, color-mix(in srgb, var(--jarvis-primary) 10%, transparent), transparent 45%), radial-gradient(circle at 80% 75%, color-mix(in srgb, var(--jarvis-accent) 12%, transparent), transparent 50%)",
        }}
      />
      <div className="absolute inset-0 opacity-[0.035] bg-[linear-gradient(to_right,rgba(255,255,255,0.7)_1px,transparent_1px),linear-gradient(to_bottom,rgba(255,255,255,0.7)_1px,transparent_1px)] bg-[size:48px_48px]" />
      <motion.div
        className="absolute left-1/2 top-0 h-[420px] w-[420px] -translate-x-1/2 rounded-full blur-3xl"
        style={{ background: "color-mix(in srgb, var(--jarvis-primary) 10%, transparent)" }}
        animate={{ scale: [1, 1.15, 1], opacity: [0.5, 0.8, 0.5] }}
        transition={{ duration: 8, repeat: Infinity, ease: "easeInOut" }}
      />

      <div className="absolute inset-x-0 top-0 flex items-center justify-between px-6 py-4 mono text-[10px] tracking-[0.25em] text-white/35">
        <span>JARVIS OS · SINGLE USER SESSION</span>
        <span className={connected ? "text-emerald-300/80" : "text-amber-300/80"}>
          {connected ? "BRIDGE ONLINE" : "BRIDGE RECONNECTING"}
        </span>
      </div>

      <motion.div
        initial={{ opacity: 0, y: 18, scale: 0.98 }}
        animate={{ opacity: 1, y: 0, scale: 1 }}
        transition={{ duration: 0.45, ease: "easeOut" }}
        className="relative z-10 grid w-full max-w-4xl gap-8 lg:grid-cols-[1fr_360px]"
      >
        <div className="hidden flex-col justify-between py-6 lg:flex">
          <div>
            <div className="mono text-[10px] tracking-[0.5em] text-cyan-300/70">
              EDUCATIONAL OPERATING SYSTEM
            </div>
            <h1 className="mt-4 mono text-5xl font-bold tracking-[0.16em]">
              JARVIS
              <span className="ml-3 text-cyan-300">OS</span>
            </h1>
            <p className="mt-4 max-w-md text-sm leading-relaxed text-white/45">
              A kernel-driven desktop where AI agents run as scheduled processes.
              Authenticate to attach the shell to the local bridge.
            </p>
          </div>
          <div className="grid max-w-md grid-cols-3 gap-2 mono text-[10px]">
            {[
              ["KERNEL", "C17 + NASM"],
              ["BRIDGE", "FastAPI"],
              ["SHELL", "React"],
            ].map(([label, value]) => (
              <div key={label} className="rounded-lg border border-[#1e2a4a] bg-[#0d1225]/70 p-3">
                <div className="tracking-[0.2em] text-white/30">{label}</div>
                <div className="mt-1 text-cyan-200/80">{value}</div>
              </div>
            ))}
          </div>
        </div>

        <div className="rounded-2xl border border-[#1e2a4a] bg-[#0d1225]/85 p-6 shadow-[0_30px_90px_rgba(0,0,0,0.65)] backdrop-blur-xl">
          <div className="mb-6 text-center">
            <div className="mono text-right text-3xl font-semibold tracking-wider text-white/90">
              {now.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" })}
            </div>
            <div className="mono text-right text-[10px] tracking-[0.2em] text-white/35">
              {now.toLocaleDateString([], {
                weekday: "long",
                month: "long",
                day: "numeric",
              })}
            </div>
          </div>

          <div className="flex flex-col items-center">
            <motion.div
              animate={{
                boxShadow: [
                  "0 0 0 color-mix(in srgb, var(--jarvis-primary) 15%, transparent)",
                  "0 0 30px color-mix(in srgb, var(--jarvis-primary) 28%, transparent)",
                  "0 0 0 color-mix(in srgb, var(--jarvis-primary) 15%, transparent)",
                ],
              }}
              transition={{ duration: 4, repeat: Infinity }}
              className="grid h-20 w-20 place-items-center rounded-2xl border"
              style={{
                borderColor:
                  "color-mix(in srgb, var(--jarvis-primary) 30%, transparent)",
                backgroundImage:
                  "linear-gradient(135deg, color-mix(in srgb, var(--jarvis-primary) 25%, transparent), color-mix(in srgb, var(--jarvis-secondary) 25%, transparent))",
              }}
            >
              <span className="mono text-xl font-bold tracking-[0.2em] text-cyan-200">AD</span>
            </motion.div>
            <div className="mt-3 mono text-sm font-semibold tracking-[0.2em] text-white/90">ADMIN</div>
            <div className="mono text-[10px] tracking-[0.18em] text-white/30">LOCAL ADMINISTRATOR</div>
          </div>

          <form onSubmit={submit} className="mt-6 space-y-3">
            <label className="block">
              <span className="mb-1 block mono text-[10px] tracking-[0.2em] text-white/35">
                PASSCODE
              </span>
              <div className="flex items-center gap-2 rounded-lg border border-[#1e2a4a] bg-[#060a14] px-3 py-2 focus-within:border-cyan-400/50">
                <input
                  ref={inputRef}
                  type={show ? "text" : "password"}
                  value={password}
                  onChange={(event) => setPassword(event.target.value)}
                  autoComplete="current-password"
                  spellCheck={false}
                  className="min-w-0 flex-1 bg-transparent mono text-sm text-cyan-100 outline-none placeholder:text-white/20"
                  placeholder="Enter passcode"
                />
                <button
                  type="button"
                  onClick={() => setShow((value) => !value)}
                  className="mono text-[9px] tracking-widest text-white/35 hover:text-white/70"
                >
                  {show ? "HIDE" : "SHOW"}
                </button>
              </div>
            </label>

            {error && (
              <motion.div
                initial={{ opacity: 0, x: -6 }}
                animate={{ opacity: 1, x: 0 }}
                className="rounded-lg border border-red-400/30 bg-red-500/10 px-3 py-2 mono text-[11px] text-red-200"
              >
                {error}
              </motion.div>
            )}

            <button
              type="submit"
              disabled={busy || password.length === 0}
              className="w-full rounded-lg border py-2.5 mono text-xs font-semibold tracking-[0.25em] text-cyan-100 transition hover:brightness-125 disabled:cursor-not-allowed disabled:opacity-35"
              style={{
                borderColor:
                  "color-mix(in srgb, var(--jarvis-primary) 40%, transparent)",
                background:
                  "color-mix(in srgb, var(--jarvis-primary) 20%, transparent)",
              }}
            >
              {busy ? "UNLOCKING…" : "UNLOCK DESKTOP"}
            </button>
          </form>

          <div className="mt-5 rounded-lg border border-[#1e2a4a] bg-[#060a14]/70 px-3 py-2 mono text-[10px] leading-relaxed text-white/30">
            Default passcode <span className="text-cyan-200/80">jarvis</span>. Set
            <span className="text-white/50"> JARVIS_ADMIN_PASSWORD </span>
            before first boot to change it.
          </div>
          <div className="mt-3 text-center mono text-[9px] tracking-[0.18em] text-white/20">
            ONE LOCAL ACCOUNT · NO CLOUD SESSION
          </div>
        </div>
      </motion.div>

      <div className="absolute inset-x-0 bottom-0 flex items-center justify-between px-6 py-4 mono text-[10px] tracking-[0.2em] text-white/20">
        <span>{connected ? "KERNEL LINK ATTACHED" : "WAITING FOR KERNEL LINK"}</span>
        <span>PRESS ENTER TO AUTHENTICATE</span>
      </div>
    </div>
  );
}
