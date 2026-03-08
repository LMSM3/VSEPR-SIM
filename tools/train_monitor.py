#!/usr/bin/env python3
"""
train_monitor.py — Alpha Method D Continual Training TUI
=========================================================
Run from project root in WSL:

    python3 tools/train_monitor.py [--target 1.2] [--max-runs 200]

Builds fit_alpha_model, runs it in a warm-start loop, displays live
dashboard.  Stops when best RMS ≤ target, max runs exceeded, or Ctrl+C.
Full fitter output logged to build-wsl/train.log.
"""

import argparse, curses, os, re, subprocess, sys, time
from dataclasses import dataclass, field
from typing import List

# ═══════════════════════════════════════════════════════════════════════
# State
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class Iter:
    outer:   int   = 0
    sweeps:  int   = 0
    loss:    float = 0.0
    delta:   float = 0.0
    rms:     float = 0.0
    log_rms: float = 0.0
    w_rms:   float = 0.0
    max_pct: float = 0.0
    mono:    int   = 0
    flips:   int   = 0
    blk:     List[float] = field(default_factory=lambda: [0.0]*4)
    per:     List[float] = field(default_factory=lambda: [0.0]*7)
    noble:   float = 0.0
    halogen: float = 0.0
    alkali:  float = 0.0
    n_sat:   int   = 0
    n_tot:   int   = 118
    top5:    str   = ""

@dataclass
class St:
    run:        int   = 0
    target:     float = 1.2
    max_runs:   int   = 200
    t0:         float = 0.0
    best_rms:   float = 999.0
    best_run:   int   = 0
    best_outer: int   = 0
    init_rms:   float = 0.0
    status:     str   = "init"
    done:       bool  = False
    total_iter: int   = 0
    hist:       List[float] = field(default_factory=list)
    # parameters
    kp:      List[float] = field(default_factory=lambda: [0.0]*7)
    cb:      List[float] = field(default_factory=lambda: [0.0]*4)
    c_rel:   float = 0.0
    beta_f:  float = 0.0
    a_f1:    float = 0.0
    a_f2:    float = 0.0
    a_f3:    float = 0.0
    sf1:     float = 3.0
    sf2:     float = 2.0
    b_bind:  float = 0.0
    q_drude: float = 1.0
    blob_f_lin:  float = 0.0
    blob_f_half: float = 0.0
    blob_f_full: float = 0.0
    it:      Iter  = field(default_factory=Iter)
    # stagnation detection
    stall_iters: int = 0
    reseeds:     int = 0
    # per-element worst offenders (parsed from fitter output)
    worst: List[tuple] = field(default_factory=list)  # [(sym, ref, pred, pct), ...]
    # reference data loaded from CSV
    ref_data: dict = field(default_factory=dict)  # {Z: (sym, alpha_ref)}

# ═══════════════════════════════════════════════════════════════════════
# Output parser
# ═══════════════════════════════════════════════════════════════════════

def parse(line: str, st: St):
    it = st.it

    # Outer iteration header
    m = re.search(
        r'Outer\s+(\d+)\s*\|.*?RMS=([\d.]+)%.*?logRMS=([\d.]+)%'
        r'.*?wRMS=([\d.]+)%.*?Max=([\d.]+)%'
        r'.*?mono_breaks=(\d+).*?sign_flips=(\d+)', line)
    if m:
        it.outer   = int(m[1])
        it.rms     = float(m[2])
        it.log_rms = float(m[3])
        it.w_rms   = float(m[4])
        it.max_pct = float(m[5])
        it.mono    = int(m[6])
        it.flips   = int(m[7])
        st.hist.append(it.rms)
        st.total_iter += 1
        if st.init_rms < 0.01:
            st.init_rms = it.rms
        return

    # Inner CD
    m = re.search(r'Inner CD:\s*(\d+)\s*sweeps\s+loss=([\d.]+)\s+delta=([\d.e+-]+)', line)
    if m:
        it.sweeps = int(m[1])
        it.loss   = float(m[2])
        it.delta  = float(m[3])
        return

    # Block RMS
    if 'Block:' in line:
        for nm, i in [('s',0),('p',1),('d',2),('f',3)]:
            m2 = re.search(rf'\b{nm}=([\d.]+)%', line)
            if m2: it.blk[i] = float(m2[1])
        return

    # Period RMS
    if 'Period:' in line:
        for i in range(1, 8):
            m2 = re.search(rf'P{i}=([\d.]+)%', line)
            if m2: it.per[i-1] = float(m2[1])
        return

    # Group RMS
    m = re.search(r'noble=([\d.]+)%.*?halogen=([\d.]+)%.*?alkali=([\d.]+)%', line)
    if m:
        it.noble   = float(m[1])
        it.halogen = float(m[2])
        it.alkali  = float(m[3])
        return

    # Best checkpoint (per-outer)
    m = re.search(r'New best unweighted RMS:\s*([\d.]+)%\s*at outer\s*(\d+)', line)
    if m:
        v = float(m[1])
        if v < st.best_rms:
            st.best_rms   = v
            st.best_run   = st.run
            st.best_outer = int(m[2])
        return

    # Weights
    m = re.search(r'Weights:\s*(\d+)/(\d+)\s*saturated.*?Top-5:\s*(.*)', line)
    if m:
        it.n_sat = int(m[1])
        it.n_tot = int(m[2])
        it.top5  = m[3].strip()
        return

    # Final best checkpoint
    m = re.search(r'\[Best checkpoint\].*?RMS=([\d.]+)%', line)
    if m:
        v = float(m[1])
        if v < st.best_rms:
            st.best_rms = v
            st.best_run = st.run
        return

    # Parameters
    m = re.search(r'k_period\[(\d+)\]\s*=\s*([\d.e+-]+)', line)
    if m:
        idx = int(m[1]) - 1
        if 0 <= idx < 7: st.kp[idx] = float(m[2])
        return

    m = re.search(r'c_block\[([spdf])\]\s*=\s*([\d.e+-]+)', line)
    if m:
        st.cb[{'s':0,'p':1,'d':2,'f':3}[m[1]]] = float(m[2])
        return

    pmap = [('c_rel','c_rel'), ('beta_f','beta_f'),
            ('a_f1','a_f1'), ('a_f2','a_f2'),
            ('a_f3','a_f3'), ('sf1','sigma_f1'), ('sf2','sigma_f2'),
            ('b_bind','b_bind'), ('q_drude','q_drude'),
            ('blob_f_lin','blob_f_lin'), ('blob_f_half','blob_f_half'),
            ('blob_f_full','blob_f_full')]
    for attr, pat in pmap:
        m2 = re.search(rf'{pat}\s*=\s*([\d.e+-]+)', line)
        if m2:
            setattr(st, attr, float(m2[1]))
            return

    # Per-element worst offender from final report
    m = re.match(r'\s*Z=\s*(\d+)\s+(\S+)\s+ref=([\d.]+)\s+pred=([\d.]+)\s+([+\-][\d.]+)%', line)
    if m:
        sym, ref, pred, pct = m[2], float(m[3]), float(m[4]), float(m[5])
        if len(st.worst) < 10:
            st.worst.append((sym, ref, pred, pct))
        return

# ═══════════════════════════════════════════════════════════════════════
# Sparkline
# ═══════════════════════════════════════════════════════════════════════

_SP = " ▁▂▃▄▅▆▇█"

def spark(vals, w=40):
    if len(vals) < 2: return ""
    seg = vals[-w:]
    lo, hi = min(seg), max(seg)
    if hi - lo < 0.01: return _SP[4] * len(seg)
    return "".join(_SP[min(8, int((v - lo) / (hi - lo) * 8))] for v in seg)

# ═══════════════════════════════════════════════════════════════════════
# TUI renderer
# ═══════════════════════════════════════════════════════════════════════

C_HEAD = C_GOOD = C_WARN = C_DIM = 0  # color pair ids, set in init

def init_colors():
    global C_HEAD, C_GOOD, C_WARN, C_DIM
    if not curses.has_colors():
        return
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(1, curses.COLOR_CYAN,  -1)
    curses.init_pair(2, curses.COLOR_GREEN, -1)
    curses.init_pair(3, curses.COLOR_YELLOW,-1)
    curses.init_pair(4, curses.COLOR_WHITE, -1)
    C_HEAD = curses.color_pair(1) | curses.A_BOLD
    C_GOOD = curses.color_pair(2) | curses.A_BOLD
    C_WARN = curses.color_pair(3)
    C_DIM  = curses.A_DIM

def draw(scr, st: St):
    scr.erase()
    H, W = scr.getmaxyx()
    MW = min(W, 76)         # main panel width
    SB = 78 if W >= 115 else 0  # sidebar column (0 = disabled)

    def p(y, x, s, a=0):
        if 0 <= y < H - 1 and 0 <= x < W:
            try: scr.addnstr(y, x, s, max(0, W - x), a)
            except curses.error: pass

    it = st.it
    el = time.time() - st.t0
    hh, mm, ss = int(el // 3600), int(el % 3600 // 60), int(el % 60)
    B = curses.A_BOLD

    # ── header ──
    p(0, 0, "\u2550" * MW, C_DIM)
    p(1, 1, "ALPHA METHOD D \u2014 CONTINUAL TRAINING (23 params)", C_HEAD)
    p(2, 1, f"Target: \u2264{st.target:.2f}%  \u2502  Run: {st.run}/{st.max_runs}"
            f"  \u2502  {hh:02}:{mm:02}:{ss:02}  \u2502  Iters: {st.total_iter}"
            f"  \u2502  Reseeds: {st.reseeds}")
    p(3, 0, "\u2500" * MW, C_DIM)

    # ── iteration ──
    p(4, 1, f"Outer: {it.outer}/50  \u2502  Sweeps: {it.sweeps}"
            f"  \u2502  Loss: {it.loss:.4f}  \u2502  \u0394: {it.delta:+.2e}"
            f"  \u2502  Stall: {st.stall_iters}")

    # progress bar
    iR = st.init_rms if st.init_rms > 0.01 else 100.0
    denom = max(iR - st.target, 0.01)
    pct = max(0.0, min(1.0, (iR - st.best_rms) / denom))
    bw = 34
    filled = int(pct * bw)
    bar = "\u2588" * filled + "\u2591" * (bw - filled)
    p(5, 1, f"RMS {it.rms:6.2f}%  {bar} \u25ba{st.target:.1f}%")
    p(6, 1, f"logRMS {it.log_rms:5.1f}%  \u2502  wRMS {it.w_rms:5.1f}%"
            f"  \u2502  Max {it.max_pct:5.1f}%")

    # sparkline history
    sp = spark(st.hist, bw)
    if sp:
        p(7, 1, f"trend   {sp}  ({len(st.hist)} pts)")

    p(8, 0, "\u2500" * MW, C_DIM)

    # ── block / period / group ──
    p(9, 1, "BLOCK", B)
    p(9, 14, "PERIOD", B)
    p(9, 46, "GROUP", B)
    bn = "spdf"
    for i in range(4):
        p(10 + i, 1, f"{bn[i]}  {it.blk[i]:5.1f}%")
    p(10, 14, f"P1={it.per[0]:4.1f}%  P2={it.per[1]:4.1f}%  P3={it.per[2]:4.1f}%")
    p(11, 14, f"P4={it.per[3]:4.1f}%  P5={it.per[4]:4.1f}%  P6={it.per[5]:4.1f}%")
    p(12, 14, f"P7={it.per[6]:4.1f}%")
    p(10, 46, f"noble   {it.noble:5.1f}%")
    p(11, 46, f"halogen {it.halogen:5.1f}%")
    p(12, 46, f"alkali  {it.alkali:5.1f}%")

    p(14, 0, "\u2500" * MW, C_DIM)

    # ── parameters (22) ──
    y = 15
    p(y, 1, "PARAMETERS (23)", B); y += 1
    kps = " ".join(f"{v:6.3f}" for v in st.kp)
    p(y, 1, f"k_per \u2502 {kps}"); y += 1
    cbs = " ".join(f"{v:5.3f}" for v in st.cb)
    p(y, 1, f"c_blk \u2502 {cbs}"); y += 1
    p(y, 1, f"c_rel \u2502 {st.c_rel:+.9f}  \u03b2_f={st.beta_f:.6f}"); y += 1
    p(y, 1, f"f-mul \u2502 a1={st.a_f1:+.4f}  a2={st.a_f2:+.4f}"
            f"  a3={st.a_f3:+.4f}"); y += 1
    p(y, 1, f"      \u2502 s1={st.sf1:.3f}   s2={st.sf2:.3f}"); y += 1
    p(y, 1, f"blob  \u2502 lin={st.blob_f_lin:+.4f}  half={st.blob_f_half:+.3f}"
            f"  full={st.blob_f_full:+.3f}"); y += 1
    p(y, 1, f"bind  \u2502 b={st.b_bind:.6f}  q_D={st.q_drude:.4f}"); y += 1
    p(y, 0, "\u2500" * MW, C_DIM); y += 1

    # ── best ──
    attr = C_GOOD if st.best_rms <= st.target else B
    p(y, 1, f"\u2605 BEST  RMS: {st.best_rms:.2f}%  \u2502"
            f"  Run {st.best_run} / Outer {st.best_outer}", attr)
    y += 1
    p(y, 0, "\u2500" * MW, C_DIM); y += 1

    # ── status ──
    p(y, 1, f"{st.status}  \u2502  Saturated: {it.n_sat}/{it.n_tot}",
      C_GOOD if st.done else C_WARN if st.run > 0 else 0)
    y += 1
    if it.top5:
        p(y, 1, f"Top-5: {it.top5[:MW - 8]}")
        y += 1
    p(y, 0, "\u2550" * MW, C_DIM)

    # ══════════════════════════════════════════════════════════════════
    # SIDEBAR — reference vs model for worst offenders
    # ══════════════════════════════════════════════════════════════════
    if SB > 0:
        p(0, SB - 1, "\u2551", C_DIM)
        p(1, SB, "REF vs MODEL", B)
        p(2, SB, "Sym    Ref    Pred   Err", C_DIM)
        p(3, SB - 1, "\u2551", C_DIM)
        # Draw vertical separator
        for vy in range(4, min(H - 1, 28)):
            p(vy, SB - 1, "\u2502", C_DIM)
        # Show worst offenders (from latest fitter report)
        if st.worst:
            for i, (sym, ref, pred, pct_e) in enumerate(st.worst[:min(10, H - 6)]):
                attr_e = C_WARN if abs(pct_e) > 15 else C_DIM
                p(4 + i, SB, f"{sym:<4s} {ref:6.2f} {pred:6.2f} {pct_e:+5.1f}%", attr_e)
        else:
            p(4, SB, "(awaiting first run)", C_DIM)
        # Show notable elements from ref data
        notable = [1, 2, 6, 63, 64, 70, 80]  # H He C Eu Gd Yb Hg
        sy = 16
        p(sy, SB, "REFERENCE (notable)", B); sy += 1
        for z in notable:
            if z in st.ref_data and sy < H - 1:
                sym, aref = st.ref_data[z]
                p(sy, SB, f"{sym:<3s} Z={z:3d}  {aref:6.2f} \u00c5\u00b3", C_DIM)
                sy += 1

    scr.refresh()

# ═══════════════════════════════════════════════════════════════════════
# Build & run
# ═══════════════════════════════════════════════════════════════════════

def find_root():
    d = os.path.dirname(os.path.abspath(__file__))
    for _ in range(5):
        if os.path.isfile(os.path.join(d, "CMakeLists.txt")):
            return d
        d = os.path.dirname(d)
    return os.getcwd()

def build_fitter(root):
    bdir = os.path.join(root, "build-wsl")
    os.makedirs(bdir, exist_ok=True)
    out = os.path.join(bdir, "fit_alpha_model")
    src = os.path.join(root, "tools", "fit_alpha_model.cpp")
    r = subprocess.run(
        ["g++", "-std=c++17", "-O2", "-DNDEBUG", f"-I{root}", src, "-o", out],
        capture_output=True, text=True, cwd=root)
    if r.returncode != 0:
        raise RuntimeError(r.stderr[:600])
    return out

def run_once(binary, root, st, scr, log):
    st.run += 1
    st.status = f"Run {st.run}: training..."
    st.it = Iter()
    st.worst = []  # reset per-element worst list for this run
    draw(scr, st)

    # Unique seed per run (time-based + run counter)
    run_seed = int(time.time() * 1000000 + st.run) % (2**32)

    # Try stdbuf for line-buffered pipe; fall back to direct exec
    cmd = [binary]
    try:
        subprocess.run(["stdbuf", "--version"],
                       capture_output=True, timeout=2)
        cmd = ["stdbuf", "-oL", binary]
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass

    env = os.environ.copy()
    env['FIT_SEED'] = str(run_seed)

    proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, bufsize=1, cwd=root, env=env)

    prev_best = st.best_rms
    try:
        for line in proc.stdout:
            line = line.rstrip("\n")
            if log:
                log.write(line + "\n")
                log.flush()
            parse(line, st)
            draw(scr, st)
            if st.best_rms <= st.target:
                st.done = True
                st.status = (f"\u2713 TARGET: {st.best_rms:.2f}%"
                             f" \u2264 {st.target:.2f}%")
                proc.terminate()
                break
    except KeyboardInterrupt:
        proc.terminate()
        raise
    finally:
        proc.wait()

    # Stagnation tracking: did this run improve best_rms?
    if st.best_rms < prev_best - 0.01:
        st.stall_iters = 0  # improvement — reset stall counter
    else:
        st.stall_iters += st.it.outer if st.it.outer > 0 else 1

def load_ref_data(root):
    """Load reference CSV into {Z: (symbol, alpha_ref)} dict."""
    ref = {}
    csv_path = os.path.join(root, "data", "polarizability_ref.csv")
    if not os.path.isfile(csv_path):
        return ref
    with open(csv_path) as f:
        for line in f:
            line = line.strip()
            if not line or line[0] in ('#', 'Z'):
                continue
            parts = line.split(',')
            if len(parts) >= 3:
                try:
                    z = int(parts[0])
                    sym = parts[1].strip()
                    alpha = float(parts[2])
                    ref[z] = (sym, alpha)
                except (ValueError, IndexError):
                    pass
    return ref

def archive_and_reseed(root, st, scr, log):
    """Archive current JSON and reset to cold-start for a fresh basin."""
    import shutil
    json_path = os.path.join(root, "config", "alpha_model_params.json")
    archive_dir = os.path.join(root, "build-wsl", "archive")
    os.makedirs(archive_dir, exist_ok=True)

    # Archive current params
    ts = time.strftime("%Y%m%d_%H%M%S")
    archive_name = f"params_run{st.run}_rms{st.best_rms:.1f}_{ts}.json"
    archive_path = os.path.join(archive_dir, archive_name)
    if os.path.isfile(json_path):
        shutil.copy2(json_path, archive_path)
        os.remove(json_path)

    st.reseeds += 1
    st.stall_iters = 0
    st.status = f"RESEED #{st.reseeds}: archived \u2192 {archive_name}"

    if log:
        log.write(f"\n=== RESEED #{st.reseeds} at run {st.run} ===\n")
        log.write(f"Archived: {archive_path}\n")
        log.write(f"Best RMS was: {st.best_rms:.2f}%\n\n")
        log.flush()

    draw(scr, st)
    time.sleep(1.0)

# ═══════════════════════════════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════════════════════════════

def app(scr, args):
    curses.curs_set(0)
    init_colors()

    H, W = scr.getmaxyx()
    if H < 29 or W < 72:
        scr.addstr(0, 0, f"Terminal too small ({W}x{H}). Need >=72x29.")
        scr.getch()
        return

    root = find_root()
    st = St(target=args.target, max_runs=args.max_runs, t0=time.time())

    # Load reference data for sidebar
    st.ref_data = load_ref_data(root)

    # ── build ──
    st.status = "Building fit_alpha_model..."
    draw(scr, st)
    try:
        binary = build_fitter(root)
    except Exception as e:
        st.status = f"BUILD FAILED: {e}"
        draw(scr, st)
        scr.nodelay(False)
        scr.getch()
        return

    st.status = "Build OK. Starting training..."
    draw(scr, st)

    logpath = os.path.join(root, "build-wsl", "train.log")
    logf = open(logpath, "w")

    STALL_LIMIT = 100  # outer iterations without improvement → reseed

    # ── training loop ──
    try:
        for _ in range(args.max_runs):
            # Stagnation detection: archive and cold-restart
            if st.stall_iters >= STALL_LIMIT:
                archive_and_reseed(root, st, scr, logf)

            run_once(binary, root, st, scr, logf)
            if st.done:
                break
            st.status = (f"Run {st.run} done (best={st.best_rms:.2f}%)."
                         f" Warm-restarting...")
            draw(scr, st)
            time.sleep(0.5)
    except KeyboardInterrupt:
        st.status = "Interrupted by user."
    finally:
        logf.close()

    # ── final screen ──
    if st.done:
        st.status = (f"\u2713 TARGET REACHED: {st.best_rms:.2f}%"
                     f" — press any key")
    else:
        st.status = (f"Stopped (best={st.best_rms:.2f}%)"
                     f" — press any key  [log: {logpath}]")
    draw(scr, st)
    scr.nodelay(False)
    scr.getch()

def main():
    ap = argparse.ArgumentParser(
        description="Alpha Method D — Continual Training Monitor")
    ap.add_argument("--target", type=float, default=1.2,
                    help="Stop when best RMS ≤ this %% (default: 1.2)")
    ap.add_argument("--max-runs", type=int, default=200,
                    help="Maximum fitter invocations (default: 200)")
    args = ap.parse_args()

    print(f"Alpha Method D trainer — target ≤{args.target}%,"
          f" max {args.max_runs} runs")
    print("Starting TUI... (Ctrl+C to stop)\n")
    time.sleep(0.5)

    curses.wrapper(lambda scr: app(scr, args))

if __name__ == "__main__":
    main()
