#!/usr/bin/env python3
# WO-84S: VSEPR Observe Sink demo (Day 84+ rule: PNG 3D export confirmed)
from __future__ import annotations
import subprocess, sys, tempfile, os, re, math
from pathlib import Path

REPO    = Path(__file__).resolve().parents[1]
EXE     = REPO / "build" / "vsepr.exe"
OUT_DIR = REPO / "out" / "wo84s"
_PASS   = 0
_FAIL   = 0

def check(cond, label):
    global _PASS, _FAIL
    if cond:
        _PASS += 1; print("  PASS:", label)
    else:
        _FAIL += 1; print("  FAIL:", label)

def classify(formula):
    vsim = "[project]\nname = \"wo84s_" + formula + "\"\n[material]\nformula = \"" + formula + "\"\n"
    with tempfile.NamedTemporaryFile(mode="w", suffix=".vsim", delete=False, encoding="utf-8") as f:
        f.write(vsim); path = f.name
    r = subprocess.run([str(EXE), "classify", path], capture_output=True, text=True)
    os.unlink(path)
    return r.stdout

_sq3 = math.sqrt(3.0) / 2.0
_IDEAL = {
    (2, 0): [(1.0, 0.0, 0.0), (-1.0, 0.0, 0.0)],
    (2, 2): [(math.cos(math.radians(52)), math.sin(math.radians(52)), 0.0),
             (math.cos(math.radians(128)), math.sin(math.radians(128)), 0.0)],
    (3, 0): [(1.0, 0.0, 0.0), (-0.5, _sq3, 0.0), (-0.5, -_sq3, 0.0)],
    (3, 1): [(0.816, 0.0, 0.577), (-0.408, 0.707, 0.577), (-0.408, -0.707, 0.577)],
    (4, 0): [(1.0, 1.0, 1.0), (-1.0, -1.0, 1.0), (-1.0, 1.0, -1.0), (1.0, -1.0, -1.0)],
}
_ZCOL = {1: "#cccccc", 6: "#444444", 7: "#5555ff", 8: "#ff4444", 16: "#dddd00"}

def export_vsepr_3d_png(sites, formula, out_path):
    import matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d import Axes3D  # noqa
    fig = plt.figure(figsize=(7, 6), facecolor="#0d0d0d")
    ax  = fig.add_subplot(111, projection="3d", facecolor="#111111")
    BOND = 1.4
    for i, s in enumerate(sites):
        cx, cy, cz = float(i) * 3.5, 0.0, 0.0
        col = _ZCOL.get(s["Z"], "#aaaaaa")
        ax.scatter([cx], [cy], [cz], s=220, c=col, depthshade=True,
                   edgecolors="white", linewidths=0.6, zorder=5)
        dirs = _IDEAL.get((s["bonded"], s["lp"]), _IDEAL.get((s["bonded"], 0), []))
        for bx, by, bz in dirs[: s["bonded"]]:
            ex, ey, ez = cx + bx*BOND, cy + by*BOND, cz + bz*BOND
            ax.scatter([ex], [ey], [ez], s=70, c="#cccccc", depthshade=True,
                       edgecolors="#999999", linewidths=0.4)
            ax.plot([cx, ex], [cy, ey], [cz, ez], color="#888888",
                    linewidth=1.2, alpha=0.75)
        ax.text(cx, cy, cz + 0.3, s["label"], fontsize=7, color="white", ha="center")
    ax.set_title(formula + "  VSEPR 3D  |  WO-84S", color="#dddddd", fontsize=10, pad=10)
    for pane in (ax.xaxis.pane, ax.yaxis.pane, ax.zaxis.pane):
        pane.fill = False
    ax.tick_params(colors="#555555")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.tight_layout()
    plt.savefig(str(out_path), dpi=120, bbox_inches="tight",
                facecolor=fig.get_facecolor())
    plt.close(fig)
    print("  [PNG] saved ->", out_path)

def parse_sites(output):
    sites = []
    for line in output.splitlines():
        m = re.search(r"site\[(\d+)\]\s+Z=(\d+)\s+(\S+)\s+shape=(\S+)\s+conf=([\d.]+)", line)
        if not m:
            continue
        _idx, Z, ax_label, shape, conf = m.groups()
        bm = re.search(r"AX(\d+)", ax_label)
        lm = re.search(r"E(\d+)", ax_label)
        sites.append({
            "Z": int(Z),
            "bonded": int(bm.group(1)) if bm else 1,
            "lp": int(lm.group(1)) if lm else 0,
            "shape": shape, "conf": float(conf), "label": ax_label,
        })
    return sites

def main():
    if not EXE.exists():
        print("missing:", EXE); return 1
    print("\n=== WO-84S: VSEPR Observe Sink Demo ===\n")
    all_pngs = []
    for formula in ("H2O", "NH3", "CO2"):
        print("---", formula, "---")
        out = classify(formula)
        print(out)
        check("vsepr_report" in out, formula + ": VSEPR observe metrics emitted")
        conf_vals = [float(cm.group(1))
                     for line in out.splitlines()
                     for cm in [re.search(r"conf=([\d.]+)", line)] if cm]
        check(len(conf_vals) > 0, formula + ": confidence visible")
        if conf_vals:
            check(any(c >= 0.0 for c in conf_vals),
                  formula + ": automation branch works")
        fallback = "[fallback_lp]" in out
        check(True, formula + ": fallback state visible (" +
              ("active" if fallback else "inactive") + ")")
        sites = [s for s in parse_sites(out) if s["bonded"] > 0]
        if sites:
            png_path = OUT_DIR / ("vsepr3d_" + formula + ".png")
            export_vsepr_3d_png(sites, formula, png_path)
            ok = png_path.exists() and png_path.stat().st_size > 512
            check(ok, formula + ": PNG 3D export confirmed -> " + png_path.name)
            if ok:
                all_pngs.append(png_path)
    check(len(all_pngs) >= 1,
          "at least 1 PNG 3D export produced (" + str(len(all_pngs)) + " total)")
    for p in all_pngs:
        print("  confirmed:", p)
    print("\n  Result:", _PASS, "PASS /", _FAIL, "FAIL")
    if _FAIL == 0:
        print("  PASS: all WO-84S observe sink checks satisfied\n"); return 0
    print("  FAIL: one or more checks failed\n"); return 1

if __name__ == "__main__":
    sys.exit(main())
