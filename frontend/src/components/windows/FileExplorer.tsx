import { useEffect, useState } from "react";
import { useKernelStore } from "@/store/kernelStore";
import { useShellStore } from "@/store/shellStore";
import type { FsEntry } from "@/api/types";

const ROOT_ID = 1;

const joinPath = (base: string, name: string) =>
  base === "/" ? `/${name}` : `${base}/${name}`;

const fmtBytes = (bytes: number) => {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
};

export default function FileExplorer() {
  const snap = useKernelStore((s) => s.snapshot);
  const command = useKernelStore((s) => s.command);
  const fileTarget = useShellStore((s) => s.fileTarget);
  const setFileTarget = useShellStore((s) => s.setFileTarget);

  const [currentId, setCurrentId] = useState(ROOT_ID);
  const [selectedId, setSelectedId] = useState<number | null>(null);
  const [pathInput, setPathInput] = useState("/");
  const [search, setSearch] = useState("");
  const [content, setContent] = useState("");
  const [dirty, setDirty] = useState(false);
  const [msg, setMsg] = useState<string | null>(null);

  const fs = snap?.filesystem;
  const entries: FsEntry[] = fs?.entries ?? [];
  const current = entries.find((e) => e.id === currentId) ?? entries.find((e) => e.id === ROOT_ID);
  const selected = entries.find((e) => e.id === selectedId) ?? null;

  const children = entries
    .filter((e) => e.parent === currentId)
    .sort((a, b) => (a.type === b.type ? a.name.localeCompare(b.name) : a.type === "dir" ? -1 : 1));

  const pattern = search.trim().toLowerCase();
  const results = pattern
    ? entries.filter((e) => e.name.toLowerCase().includes(pattern)).slice(0, 24)
    : [];

  useEffect(() => {
    if (current) setPathInput(current.path);
  }, [current?.path]);

  // fs_format replaces the tree; fall back to root if the current inode vanished.
  useEffect(() => {
    if (entries.length > 0 && !entries.some((e) => e.id === currentId)) {
      setCurrentId(ROOT_ID);
      setSelectedId(null);
    }
  }, [entries, currentId]);

  // Global search can hand a path to the explorer; navigate/select it once.
  useEffect(() => {
    if (!fileTarget || entries.length === 0) return;
    const target = entries.find((entry) => entry.path === fileTarget);
    setFileTarget(null);
    if (!target) {
      setMsg(`error: search result vanished: ${fileTarget}`);
      return;
    }
    setSearch("");
    if (target.type === "dir") {
      setCurrentId(target.id);
      setSelectedId(null);
      setMsg(`opened ${target.path}`);
    } else {
      setCurrentId(target.parent || ROOT_ID);
      setSelectedId(target.id);
      setMsg(`selected ${target.path}`);
    }
  }, [fileTarget, entries, setFileTarget]);

  // Load the editor content when a different file is selected.
  useEffect(() => {
    setDirty(false);
    if (!selected || selected.type !== "file") {
      setContent("");
      return;
    }
    let live = true;
    command({ action: "fs_read", path: selected.path })
      .then((res) => {
        const r = res as { ok: boolean; content?: string; error?: string };
        if (!live) return;
        setContent(r.ok ? r.content ?? "" : "");
        if (!r.ok) setMsg(`error: ${r.error}`);
      })
      .catch((e) => live && setMsg(`error: ${String(e)}`));
    return () => {
      live = false;
    };
  }, [selected?.path]);

  if (!snap || !fs) {
    return <div className="p-6 mono text-xs text-white/40">Waiting for kernel snapshot…</div>;
  }

  const disk = fs.disk;
  const usedPct = disk.total_blocks > 0 ? (disk.used_blocks / disk.total_blocks) * 100 : 0;

  const run = async (payload: Record<string, unknown>, okText: string) => {
    setMsg(null);
    try {
      const res = (await command(payload)) as { ok: boolean; error?: string };
      setMsg(res.ok ? okText : `error: ${res.error}`);
      return res.ok;
    } catch (e) {
      setMsg(`error: ${String(e)}`);
      return false;
    }
  };

  const navigate = (entry: FsEntry) => {
    if (entry.type === "dir") {
      setCurrentId(entry.id);
      setSelectedId(null);
      setSearch("");
    } else {
      setSelectedId(entry.id);
    }
  };

  const goUp = () => {
    if (!current || current.id === ROOT_ID) return;
    setCurrentId(current.parent || ROOT_ID);
    setSelectedId(null);
    setSearch("");
  };

  const openPath = () => {
    const target = entries.find((e) => e.path === pathInput && e.type === "dir");
    if (!target) {
      setMsg(`error: no such directory ${pathInput}`);
      return;
    }
    setCurrentId(target.id);
    setSelectedId(null);
    setSearch("");
  };

  const newFolder = async () => {
    const name = window.prompt("New folder name");
    if (!name) return;
    if (await run({ action: "fs_mkdir", path: joinPath(current?.path ?? "/", name) }, `created folder ${name}`)) {
      setSelectedId(null);
    }
  };

  const newFile = async () => {
    const name = window.prompt("New file name");
    if (!name) return;
    await run(
      { action: "fs_create", path: joinPath(current?.path ?? "/", name), content: "" },
      `created file ${name}`
    );
  };

  const renameSelected = async () => {
    if (!selected) return;
    const name = window.prompt("Rename to", selected.name);
    if (!name || name === selected.name) return;
    await run({ action: "fs_rename", path: selected.path, new_name: name }, `renamed to ${name}`);
  };

  const deleteSelected = async () => {
    if (!selected) return;
    if (!window.confirm(`Delete ${selected.path}?`)) return;
    const ok = await run({ action: "fs_delete", path: selected.path }, `deleted ${selected.path}`);
    if (ok) setSelectedId(null);
  };

  const saveFile = async () => {
    if (!selected || selected.type !== "file") return;
    const ok = await run(
      { action: "fs_write", path: selected.path, content },
      `saved ${selected.path} (${content.length} bytes)`
    );
    if (ok) setDirty(false);
  };

  const toggleReadonly = async () => {
    if (!selected) return;
    await run(
      { action: "fs_chmod", path: selected.path, readonly: !selected.readonly },
      `${selected.path} is now ${selected.readonly ? "writable" : "read-only"}`
    );
  };

  return (
    <div className="flex h-full flex-col">
      {/* disk stats */}
      <div className="grid grid-cols-4 gap-2 p-2 border-b border-[#1e2a4a] bg-[#0d1225]">
        <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 mono text-xs">
          <div className="text-white/40 tracking-widest text-[10px]">DISK</div>
          <div className="text-white font-bold">
            {disk.used_blocks}/{disk.total_blocks}
            <span className="text-white/40 font-normal"> blocks</span>
          </div>
          <div className="text-white/30 text-[11px]">{usedPct.toFixed(0)}% used · {fmtBytes(disk.used_bytes)}</div>
        </div>
        <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 mono text-xs">
          <div className="text-white/40 tracking-widest text-[10px]">INODES</div>
          <div className="text-white">{fs.stats.inodes_used}/{fs.config.max_inodes}</div>
          <div className="text-white/30 text-[11px]">{fs.config.max_blocks_per_file} blocks/file max</div>
        </div>
        <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 mono text-xs">
          <div className="text-white/40 tracking-widest text-[10px]">OPEN FILES</div>
          <div className="text-white">{fs.stats.open_files}/{fs.config.max_open_files}</div>
          <div className="text-cyan-300 text-[11px]">{fs.config.block_size} B blocks</div>
        </div>
        <div className="rounded bg-[#141b34] border border-[#1e2a4a] p-2 mono text-xs">
          <div className="text-white/40 tracking-widest text-[10px]">OPERATIONS</div>
          <div className="text-white">r {fs.stats.reads} · w {fs.stats.writes}</div>
          <div className="text-white/30 text-[11px]">denied {fs.stats.denied} · search {fs.stats.searches}</div>
        </div>
      </div>

      {/* path + tools */}
      <div className="flex flex-wrap items-center gap-1.5 p-2 border-b border-[#1e2a4a] mono text-xs">
        <button onClick={goUp} className="rounded bg-white/10 px-2 py-1 text-white/80 hover:bg-white/15" title="Parent directory">
          ↑
        </button>
        <input
          value={pathInput}
          onChange={(e) => setPathInput(e.target.value)}
          onKeyDown={(e) => e.key === "Enter" && openPath()}
          className="min-w-[160px] flex-1 rounded border border-[#1e2a4a] bg-[#060a14] px-2 py-1 text-cyan-200"
          spellCheck={false}
        />
        <button onClick={openPath} className="rounded bg-cyan-500/20 px-2 py-1 text-cyan-200 hover:bg-cyan-500/30">
          GO
        </button>
        <button onClick={newFolder} className="rounded bg-white/10 px-2 py-1 text-white/80 hover:bg-white/15">
          + folder
        </button>
        <button onClick={newFile} className="rounded bg-white/10 px-2 py-1 text-white/80 hover:bg-white/15">
          + file
        </button>
        <input
          value={search}
          onChange={(e) => setSearch(e.target.value)}
          placeholder="search names"
          className="w-32 rounded border border-[#1e2a4a] bg-[#060a14] px-2 py-1 text-white placeholder:text-white/25"
        />
      </div>

      <div className="flex min-h-0 flex-1">
        {/* listing */}
        <div className="w-1/2 overflow-auto border-r border-[#1e2a4a] p-2">
          <div className="mono text-[10px] tracking-widest text-white/40 mb-1">
            {pattern ? `SEARCH RESULTS — "${search}"` : current?.path ?? "/"}
          </div>
          {pattern ? (
            results.length === 0 ? (
              <div className="mono text-xs text-white/30 p-3">No match.</div>
            ) : (
              results.map((entry) => (
                <button
                  key={entry.id}
                  onClick={() => (entry.type === "dir" ? navigate(entry) : setSelectedId(entry.id))}
                  className="flex w-full items-center gap-2 rounded px-2 py-1 text-left mono text-xs hover:bg-white/5"
                >
                  <span className="text-cyan-300">{entry.type === "dir" ? "▸" : "·"}</span>
                  <span className="text-white/80 truncate">{entry.name}</span>
                  <span className="ml-auto text-[10px] text-white/25 truncate">{entry.path}</span>
                </button>
              ))
            )
          ) : children.length === 0 ? (
            <div className="mono text-xs text-white/30 p-3">Empty directory.</div>
          ) : (
            children.map((entry) => (
              <button
                key={entry.id}
                onClick={() => setSelectedId(entry.id)}
                onDoubleClick={() => navigate(entry)}
                className={`flex w-full items-center gap-2 rounded px-2 py-1 text-left mono text-xs transition ${
                  selectedId === entry.id ? "bg-cyan-500/15 text-cyan-100" : "hover:bg-white/5 text-white/75"
                }`}
              >
                <span className={entry.type === "dir" ? "text-amber-300" : "text-cyan-300"}>
                  {entry.type === "dir" ? "▸" : "·"}
                </span>
                <span className="truncate">{entry.name}</span>
                {entry.hidden && <span className="text-[10px] text-white/30">hidden</span>}
                {entry.readonly && <span className="text-[10px] text-amber-300/70">ro</span>}
                <span className="ml-auto text-[10px] text-white/25">
                  {entry.type === "dir" ? `${entries.filter((e) => e.parent === entry.id).length} items` : fmtBytes(entry.size)}
                </span>
              </button>
            ))
          )}
        </div>

        {/* details / editor */}
        <div className="w-1/2 overflow-auto p-2">
          {!selected ? (
            <div className="mono text-xs text-white/30 p-3">
              Select a file to inspect it. Double-click a folder to open it.
            </div>
          ) : (
            <div className="space-y-2">
              <div className="rounded border border-[#1e2a4a] bg-[#0d1225] p-2 mono text-xs">
                <div className="flex items-center gap-2">
                  <span className="text-cyan-200 font-semibold truncate">{selected.name}</span>
                  <span className="ml-auto rounded bg-white/5 px-1.5 py-0.5 text-[10px] text-white/50">
                    {selected.type}
                  </span>
                </div>
                <div className="mt-1 text-white/40 break-all">{selected.path}</div>
                <div className="mt-1 flex flex-wrap gap-x-3 gap-y-1 text-[11px] text-white/50">
                  <span>size {selected.size} B</span>
                  <span>blocks {selected.block_count} [{selected.blocks.join(", ")}]</span>
                  <span>owner {selected.owner}</span>
                  <span>mode {selected.mode}</span>
                </div>
              </div>

              <div className="flex flex-wrap gap-1.5 mono text-xs">
                <button onClick={renameSelected} className="rounded bg-white/10 px-2 py-1 text-white/80 hover:bg-white/15">
                  rename
                </button>
                <button onClick={toggleReadonly} className="rounded bg-amber-500/20 px-2 py-1 text-amber-200 hover:bg-amber-500/30">
                  {selected.readonly ? "make writable" : "make read-only"}
                </button>
                <button onClick={deleteSelected} className="rounded bg-red-500/20 px-2 py-1 text-red-200 hover:bg-red-500/30">
                  delete
                </button>
              </div>

              {selected.type === "file" ? (
                <>
                  <textarea
                    value={content}
                    onChange={(e) => {
                      setContent(e.target.value);
                      setDirty(true);
                    }}
                    spellCheck={false}
                    className="h-40 w-full resize-none rounded border border-[#1e2a4a] bg-[#060a14] p-2 mono text-xs text-white/85 outline-none focus:border-cyan-400/40"
                  />
                  <div className="flex items-center gap-2 mono text-xs">
                    <button
                      onClick={saveFile}
                      disabled={!dirty || selected.readonly}
                      className="rounded bg-cyan-500 px-3 py-1 font-semibold tracking-widest text-black hover:bg-cyan-400 disabled:cursor-not-allowed disabled:opacity-30"
                    >
                      SAVE
                    </button>
                    <span className="text-white/35">
                      {selected.readonly ? "read-only file" : dirty ? "unsaved changes" : "saved"}
                    </span>
                    <span className="ml-auto text-white/25">{content.length} chars</span>
                  </div>
                </>
              ) : (
                <div className="mono text-xs text-white/40 p-2">
                  Directory metadata only. Use the toolbar to create entries here.
                </div>
              )}
            </div>
          )}
        </div>
      </div>

      {msg && <div className="border-t border-[#1e2a4a] bg-amber-500/10 px-3 py-1 mono text-xs text-amber-200">{msg}</div>}
    </div>
  );
}
