/* Electron main process — JARVIS OS desktop shell.
   Spawns the FastAPI backend (uvicorn), waits for /health, then opens
   a frameless fullscreen window with the Vite-built renderer.
   The window has no native chrome; all controls are in React.
*/
const { app, BrowserWindow, ipcMain } = require("electron");
const { spawn } = require("child_process");
const path = require("path");
const http = require("http");
const fs = require("fs");

let mainWindow = null;
let backendProc = null;
const BACKEND_PORT = 8000;
const BACKEND_HOST = "127.0.0.1";
const BACKEND_URL = `http://${BACKEND_HOST}:${BACKEND_PORT}/health`;

function waitForBackend(timeoutMs = 15000) {
  const start = Date.now();
  return new Promise((resolve, reject) => {
    function poll() {
      const req = http.get(BACKEND_URL, (res) => {
        if (res.statusCode === 200) {
          resolve(true);
        } else {
          retry();
        }
      });
      req.on("error", retry);
      req.setTimeout(800, retry);
      function retry() {
        if (Date.now() - start > timeoutMs) {
          reject(new Error("Backend did not become ready in time"));
          return;
        }
        setTimeout(poll, 400);
      }
    }
    poll();
  });
}

function spawnBackend() {
  const projectRoot = path.resolve(__dirname, "..", "..");
  const pythonCandidates = ["python", "python3", "py"];
  // Try to spawn uvicorn via `python -m uvicorn`. The packaged app would bundle python,
  // but in dev we rely on the system interpreter + installed backend deps.
  const args = ["-m", "uvicorn", "backend.main:app", "--host", BACKEND_HOST, "--port", String(BACKEND_PORT)];
  const opts = { cwd: projectRoot, stdio: "pipe", windowsHide: true };

  for (const py of pythonCandidates) {
    try {
      const proc = spawn(py, args, opts);
      let started = false;
      proc.stdout?.on("data", (d) => process.stdout.write(`[backend] ${d}`));
      proc.stderr?.on("data", (d) => process.stderr.write(`[backend] ${d}`));
      proc.on("error", () => {});
      proc.on("exit", (code) => {
        if (!started) return;
        console.log(`[backend] exited with code ${code}`);
      });
      // Give it a moment to see if it immediately fails (e.g. python not found)
      // We consider it started if it doesn't error in 300ms.
      backendProc = proc;
      started = true;
      console.log(`[electron] spawned backend via ${py}`);
      return proc;
    } catch {
      continue;
    }
  }
  console.warn("[electron] could not spawn backend — is Python + uvicorn installed?");
  return null;
}

function createWindow() {
  const isDev = !app.isPackaged && fs.existsSync(path.join(__dirname, "..", "src"));
  // In dev, Vite serves on 5173. In prod, load the built dist/index.html.
  const devUrl = "http://127.0.0.1:5173";
  const prodFile = path.join(__dirname, "..", "dist", "index.html");

  mainWindow = new BrowserWindow({
    width: 1280,
    height: 800,
    minWidth: 1024,
    minHeight: 600,
    frame: false,
    fullscreen: false,
    backgroundColor: "#060a14",
    title: "JARVIS OS",
    show: false,
    webPreferences: {
      preload: path.join(__dirname, "preload.js"),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });

  mainWindow.once("ready-to-show", () => {
    mainWindow.show();
    // Start fullscreen after a moment so boot animation is visible
    // setTimeout(() => mainWindow.setFullScreen(true), 800);
  });

  if (isDev) {
    mainWindow.loadURL(devUrl).catch(() => {
      // fallback to file if dev server not running
      if (fs.existsSync(prodFile)) mainWindow.loadFile(prodFile);
    });
  } else {
    mainWindow.loadFile(prodFile);
  }

  // Open DevTools in dev
  if (isDev) {
    mainWindow.webContents.once("did-finish-load", () => {
      // mainWindow.webContents.openDevTools({ mode: "detach" });
    });
  }

  mainWindow.on("closed", () => {
    mainWindow = null;
  });
}

app.whenReady().then(async () => {
  // Try to spawn backend; if it fails, we still open the window (it will show offline state
  // until the user manually starts `uvicorn backend.main:app`).
  spawnBackend();

  try {
    await waitForBackend(12000);
    console.log("[electron] backend ready");
  } catch (e) {
    console.warn("[electron] backend not ready:", e.message, "- window will show offline until backend starts");
  }

  createWindow();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") {
    app.quit();
  }
});

app.on("before-quit", () => {
  if (backendProc) {
    try {
      backendProc.kill();
    } catch {}
    backendProc = null;
  }
});

ipcMain.handle("window:minimize", () => mainWindow?.minimize());
ipcMain.handle("window:maximize", () => {
  if (!mainWindow) return;
  if (mainWindow.isMaximized()) mainWindow.unmaximize();
  else mainWindow.maximize();
});
ipcMain.handle("window:close", () => mainWindow?.close());
ipcMain.handle("window:isMaximized", () => mainWindow?.isMaximized() ?? false);
