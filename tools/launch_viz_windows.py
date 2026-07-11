"""
VSEPR-SIM Stream-Aware Chrome Window Launcher
==============================================
Queries the viz_web.py /api/streams registry and opens Chrome windows
ONLY for ports that have an active generator producing data.

Contract: a window is opened IFF the stream is enabled AND active.
Ports with no generator get skipped with a log message.

Usage:
    python tools/launch_viz_windows.py [--host HOST] [--http-port PORT] [--no-restart]

Flags:
    --host HOST         Server host (default: localhost)
    --http-port PORT    HTTP port of viz_web.py (default: 8899)
    --no-restart        Do not auto-restart viz_web.py if not found
    --no-watchdog       Do not reopen closed windows
    --chrome PATH       Path to chrome.exe (auto-detected if omitted)
    --force-all         Bypass stream check, open all ports (debug mode)
"""

import sys
import os
import time
import subprocess
import threading
import socket
import json
import urllib.request

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

CHROME_CANDIDATES = [
    r"C:\Program Files\Google\Chrome\Application\chrome.exe",
    r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
    r"C:\Program Files\Chromium\Application\chromium.exe",
    r"C:\Program Files (x86)\Chromium\Application\chromium.exe",
]

VIZ_WEB_SCRIPT = os.path.join(os.path.dirname(__file__), "viz_web.py")
WORKSPACE      = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

HTTP_PORT   = 8899
BIND_HOST   = "localhost"
WATCHDOG_INTERVAL = 60    # seconds between window health checks
TEMP_DIR = os.path.join(os.environ.get("TEMP", "C:\\Temp"), "vsepr_chrome_profiles")
SERVER_POLL_INTERVAL = 0.5
SERVER_TIMEOUT = 30      # max seconds to wait for server to come up
STREAM_WAIT_TIMEOUT = 20 # max seconds to wait for generators to become active
STREAM_POLL_INTERVAL = 2 # seconds between /api/streams polls
WINDOW_OPEN_RETRIES = 3
WINDOW_RETRY_DELAY  = 1.5

# Fallback port list — used only with --force-all
PORT_WINDOWS = [
    (9990, "Crystal"),
    (9991, "Coarse-Grain"),
    (9992, "EHD Field"),
    (9993, "Thermal Pipe"),
    (9994, "Peptide 2D"),
    (9995, "Peptide 3D"),
    (9996, "Phase Diagram"),
    (9997, "Formation Timeline"),
    (9998, "Analysis Deep"),
    (9999, "Atomic Live"),
]

# Screen grid layout (3 cols x 4 rows, ~1920x1080 assumed)
GRID_COLS = 3
WIN_W = 620
WIN_H = 520
MARGIN = 4
MAX_GRID_SLOTS = 12  # 3x4

def _make_grid_positions(count):
    positions = []
    for row in range(4):
        for col in range(GRID_COLS):
            if len(positions) >= count:
                return positions
            x = col * (WIN_W + MARGIN) + MARGIN
            y = row * (WIN_H + MARGIN) + MARGIN
            positions.append((x, y))
    return positions

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def log(msg):
    ts = time.strftime("%H:%M:%S")
    print(f"  [{ts}]  {msg}", flush=True)

def find_chrome():
    for path in CHROME_CANDIDATES:
        if os.path.isfile(path):
            return path
    # Try PATH
    for name in ("chrome", "chromium", "chromium-browser"):
        try:
            result = subprocess.run(["where", name], capture_output=True, text=True)
            if result.returncode == 0:
                line = result.stdout.strip().splitlines()[0].strip()
                if os.path.isfile(line):
                    return line
        except Exception:
            pass
    return None

def server_is_alive(host, port, timeout=2.0):
    try:
        url = f"http://{host}:{port}/api/stats"
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return r.status == 200
    except Exception:
        return False

def query_streams(host, http_port, timeout=3.0):
    """Query /api/streams and return the stream list.
    Returns [] on failure (server too old, unreachable, etc)."""
    try:
        url = f"http://{host}:{http_port}/api/streams"
        with urllib.request.urlopen(url, timeout=timeout) as r:
            data = json.loads(r.read())
            return data.get("streams", [])
    except Exception as e:
        log(f"WARN: /api/streams query failed: {e}")
        return []

def wait_for_active_streams(host, http_port, total=STREAM_WAIT_TIMEOUT):
    """Poll /api/streams until at least one stream is enabled+active.
    Generators may take a few seconds to produce their first frame.
    Returns the final stream list."""
    log(f"Waiting for active streams (up to {total}s) ...")
    deadline = time.time() + total
    best = []
    while time.time() < deadline:
        streams = query_streams(host, http_port)
        active = [s for s in streams if s.get("enabled") and s.get("active")]
        if len(active) > len(best):
            best = active
            log(f"  {len(active)} active streams found so far")
        # If we have most of the registered streams, good enough
        registered = [s for s in streams if s.get("enabled")]
        if registered and len(active) >= len(registered) * 0.8:
            log(f"  {len(active)}/{len(registered)} streams active -- proceeding")
            return streams
        time.sleep(STREAM_POLL_INTERVAL)
    # Return whatever we have
    final = query_streams(host, http_port)
    if not final:
        log("WARN: no stream data from server")
    return final

def wait_for_server(host, port, total=SERVER_TIMEOUT):
    log(f"Waiting for viz_web.py on http://{host}:{port} ...")
    deadline = time.time() + total
    dots = 0
    while time.time() < deadline:
        if server_is_alive(host, port):
            log(f"Server alive after {int(deadline - time.time() - total + SERVER_TIMEOUT)}s  OK")
            return True
        time.sleep(SERVER_POLL_INTERVAL)
        dots += 1
        if dots % 10 == 0:
            sys.stdout.write(".")
            sys.stdout.flush()
    return False

def start_viz_server(http_port):
    log("Starting viz_web.py (detached, persistent) ...")
    # Use pythonw on Windows so the server outlives this launcher process
    py = sys.executable
    pythonw = py.replace("python.exe", "pythonw.exe")
    if not os.path.isfile(pythonw):
        pythonw = py  # fallback to python.exe
    cmd = [pythonw, VIZ_WEB_SCRIPT, "--http-port", str(http_port)]
    flags = 0
    if sys.platform == "win32":
        flags = subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.DETACHED_PROCESS
    proc = subprocess.Popen(
        cmd,
        cwd=WORKSPACE,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        stdin=subprocess.DEVNULL,
        creationflags=flags,
    )
    log(f"viz_web.py PID={proc.pid}  (detached)")
    return proc

def open_chrome_window(chrome, url, x, y, w, h, port_name, port_id, retries=WINDOW_OPEN_RETRIES):
    """Open a single Chrome --app window at (x,y) with size (w,h)."""
    # Isolated profile dir so Chrome can't collapse this into an existing session
    profile_dir = os.path.join(TEMP_DIR, f"port_{port_id}")
    os.makedirs(profile_dir, exist_ok=True)
    for attempt in range(1, retries + 1):
        try:
            args = [
                chrome,
                f"--app={url}",
                f"--window-size={w},{h}",
                f"--window-position={x},{y}",
                f"--user-data-dir={profile_dir}",
                "--disable-extensions",
                "--disable-background-networking",
                "--no-first-run",
                "--no-default-browser-check",
                "--disable-sync",
                "--disable-translate",
                "--disable-features=TranslateUI",
                "--disable-popup-blocking",
                "--autoplay-policy=no-user-gesture-required",
                "--allow-insecure-localhost",
            ]
            proc = subprocess.Popen(args,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL)
            log(f"  Opened [{port_name}]  {url}  pos=({x},{y})  pid={proc.pid}")
            return proc
        except Exception as e:
            log(f"  WARN: attempt {attempt}/{retries} failed for {port_name}: {e}")
            time.sleep(WINDOW_RETRY_DELAY)
    log(f"  ERROR: could not open window for {port_name}")
    return None

def open_windows_for_active_streams(chrome, host, http_port, streams, force_all=False):
    """Open Chrome windows only for streams that are enabled AND active.
    With --force-all, opens all enabled streams regardless of activity.
    """
    if not streams:
        log("No stream data -- falling back to PORT_WINDOWS")
        streams = [{"port": p, "name": n, "enabled": True, "active": True}
                   for p, n in PORT_WINDOWS]

    # Filter: only open if enabled+active (or force_all)
    to_open = []
    for s in sorted(streams, key=lambda x: x.get("port", 0)):
        port = s.get("port")
        name = s.get("name", f"Port {port}")
        enabled = s.get("enabled", False)
        active = s.get("active", False)

        if not enabled:
            log(f"[skip] ::{port} {name} -- disabled")
            continue
        if not active and not force_all:
            log(f"[skip] ::{port} {name} -- no generator / no data")
            continue
        to_open.append((port, name))

    if not to_open:
        log("No active streams to open")
        return {}

    grid = _make_grid_positions(len(to_open))
    procs = {}
    log(f"Opening {len(to_open)} Chrome windows (of {len(streams)} registered) ...")
    for i, (port, name) in enumerate(to_open):
        url = f"http://{host}:{http_port}/?port={port}"
        x, y = grid[i] if i < len(grid) else (i * 40, i * 40)
        proc = open_chrome_window(chrome, url, x, y, WIN_W, WIN_H,
                                  f"::{port} {name}", port)
        if proc:
            procs[port] = {"proc": proc, "url": url, "name": name,
                           "x": x, "y": y, "i": i}
        time.sleep(0.3)  # stagger to avoid Chrome startup race
    return procs

# Windows that form the "secondary group" — if any one is closed, close all others in the group
SECONDARY_GROUP = {9992, 9993, 9994, 9995, 9996, 9997, 9998}
# Primary windows (9999, 9990, 9991) are independent — always reopen
PRIMARY_PORTS   = {9999, 9990, 9991}

def _kill_group(procs, group_ports, exclude_port=None):
    """Terminate all windows in a port set (used for group-close cascade)."""
    for port in group_ports:
        if port == exclude_port:
            continue
        info = procs.get(port)
        if info and info["proc"] and info["proc"].poll() is None:
            log(f"  Group-close: terminating ::{port} {info['name']}")
            try:
                info["proc"].terminate()
            except Exception:
                pass
            info["proc"] = None

def watchdog_loop(chrome, host, http_port, procs, interval, stop_event):
    """Periodically check that all windows are still alive.

    Rules:
    - Primary ports (9999, 9990, 9991): always reopen if closed
    - Secondary group (9992-9998): if ANY one is closed, close ALL others in
      the group (group-close), then reopen all of them together
    - Only manages ports that were actually opened (in procs dict)
    - Re-checks /api/streams: if a port lost its generator, skip reopen
    """
    log(f"Watchdog active -- checking every {interval}s")
    # Determine primary/secondary from what was actually opened
    opened_primary = sorted(p for p in procs if p in PRIMARY_PORTS)
    opened_secondary = sorted(p for p in procs if p in SECONDARY_GROUP)
    log(f"  Primary (always reopen): {opened_primary}")
    log(f"  Secondary group (close-all-on-one-close): {opened_secondary}")
    while not stop_event.is_set():
        time.sleep(interval)
        if not server_is_alive(host, http_port):
            log("WARN: viz_web.py not responding -- windows will reconnect automatically")
            continue

        # Re-query streams to check which are still active
        streams = query_streams(host, http_port)
        active_ports = set()
        for s in streams:
            if s.get("enabled") and s.get("active"):
                active_ports.add(s.get("port"))

        # Check secondary group first (only ports we actually opened)
        closed_secondary = [p for p in opened_secondary
                            if p in procs and procs[p]["proc"] is not None
                            and procs[p]["proc"].poll() is not None]

        if closed_secondary:
            log(f"Group-close triggered by: {closed_secondary} -- closing all secondary windows")
            _kill_group(procs, set(opened_secondary))
            time.sleep(1.5)
            for port in opened_secondary:
                if port not in procs:
                    continue
                if port not in active_ports:
                    log(f"  [skip reopen] ::{port} -- stream no longer active")
                    continue
                info = procs[port]
                new_proc = open_chrome_window(
                    chrome, info["url"], info["x"], info["y"],
                    WIN_W, WIN_H, f"::{port} {info['name']}", port)
                if new_proc:
                    procs[port]["proc"] = new_proc
                time.sleep(0.25)
        else:
            # Normal watchdog: reopen any individually closed primary window
            for port in opened_primary:
                if port not in procs:
                    continue
                info = procs[port]
                if info["proc"] and info["proc"].poll() is not None:
                    if port not in active_ports:
                        log(f"  [skip reopen] ::{port} -- stream no longer active")
                        continue
                    log(f"Primary window ::{port} {info['name']} closed -- reopening ...")
                    new_proc = open_chrome_window(
                        chrome, info["url"], info["x"], info["y"],
                        WIN_W, WIN_H, f"::{port} {info['name']}", port)
                    if new_proc:
                        procs[port]["proc"] = new_proc

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    args = sys.argv[1:]
    host = BIND_HOST
    http_port = HTTP_PORT
    auto_start = True
    watchdog = True
    chrome_override = None
    force_all = False

    i = 0
    while i < len(args):
        a = args[i]
        if a == "--host" and i + 1 < len(args):
            host = args[i + 1]; i += 2
        elif a == "--http-port" and i + 1 < len(args):
            http_port = int(args[i + 1]); i += 2
        elif a == "--no-restart":
            auto_start = False; i += 1
        elif a == "--no-watchdog":
            watchdog = False; i += 1
        elif a == "--force-all":
            force_all = True; i += 1
        elif a == "--chrome" and i + 1 < len(args):
            chrome_override = args[i + 1]; i += 2
        elif a in ("--help", "-h"):
            print(__doc__); sys.exit(0)
        else:
            i += 1

    print()
    print("+============================================================+")
    print("|  VSEPR-SIM  Stream-Aware Chrome Window Launcher            |")
    print("+============================================================+")
    print()

    # --- Find Chrome ---
    chrome = chrome_override or find_chrome()
    if not chrome:
        print("ERROR: Chrome/Chromium not found. Use --chrome /path/to/chrome.exe")
        sys.exit(1)
    log(f"Chrome: {chrome}")

    # --- Ensure server is up ---
    server_proc = None
    if not server_is_alive(host, http_port):
        if auto_start:
            server_proc = start_viz_server(http_port)
            if not wait_for_server(host, http_port):
                log("ERROR: viz_web.py did not come up in time")
                sys.exit(1)
        else:
            log(f"ERROR: No server at http://{host}:{http_port} and --no-restart set")
            sys.exit(1)
    else:
        log(f"Server already alive at http://{host}:{http_port}")

    # --- Query stream registry: wait for generators to produce data ---
    if force_all:
        log("--force-all: bypassing stream check, opening all ports")
        streams = [{"port": p, "name": n, "enabled": True, "active": True}
                   for p, n in PORT_WINDOWS]
    else:
        streams = wait_for_active_streams(host, http_port)
        active_count = sum(1 for s in streams
                          if s.get("enabled") and s.get("active"))
        total_count = len(streams)
        log(f"Stream registry: {active_count} active / {total_count} registered")
        for s in streams:
            port = s.get("port", "?")
            name = s.get("name", "?")
            act = "ACTIVE" if s.get("active") else "------"
            ena = "enabled" if s.get("enabled") else "disabled"
            fc = s.get("frame_count", 0)
            log(f"  ::{port}  {name:<20s}  {act}  {ena}  frames={fc}")

    # --- Open windows for active streams only ---
    procs = open_windows_for_active_streams(chrome, host, http_port, streams,
                                            force_all=force_all)
    active_count = sum(1 for s in streams
                       if s.get("enabled") and s.get("active"))
    log(f"Launched {len(procs)}/{active_count} windows")

    if not procs:
        log("No windows opened -- no active streams. Exiting.")
        log("  (Use --force-all to open all ports regardless)")
        sys.exit(1)

    # --- Watchdog ---
    stop_event = threading.Event()
    if watchdog:
        t = threading.Thread(
            target=watchdog_loop,
            args=(chrome, host, http_port, procs, WATCHDOG_INTERVAL, stop_event),
            daemon=True)
        t.start()

    print()
    log(f"{len(procs)} windows open. Press Ctrl+C to close.")
    print()

    try:
        while True:
            time.sleep(5)
            try:
                url = f"http://{host}:{http_port}/api/stats"
                with urllib.request.urlopen(url, timeout=2) as r:
                    data = json.loads(r.read())
                    total = data.get("total_frames", "?")
                    up = int(data.get("uptime", 0))
                    ports_active = len(data.get("ports", {}))
                    sys.stdout.write(
                        f"\r  Uptime={up}s  Frames={total}  ActivePorts={ports_active}"
                        f"  Windows={len(procs)}  Watchdog={'ON' if watchdog else 'OFF'}  ")
                    sys.stdout.flush()
            except Exception:
                pass
    except KeyboardInterrupt:
        pass

    print()
    log("Shutting down launcher...")
    stop_event.set()

    if server_proc:
        log("Stopping viz_web.py...")
        try:
            server_proc.terminate()
            server_proc.wait(timeout=5)
        except Exception:
            server_proc.kill()

    log("Done.")


if __name__ == "__main__":
    main()
