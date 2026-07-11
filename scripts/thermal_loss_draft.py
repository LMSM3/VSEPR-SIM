"""
thermal_loss_draft.py — Draft analysis companion for thermal_loss_explorer.cpp
==============================================================================

Reads the CSV/JSON output from the C++ explorer and produces:
  1. dt-sensitivity analysis (how dt choice affects convergence and Q_loss)
  2. Tc/Th/T_ambient decay curves grouped by material
  3. Energy loss accumulation comparison across trials
  4. Best-trial identification for follow-up high-res runs

Can also run its own quick Python-side exploration with wider nets
and feed those back for comparison.

VSEPR-SIM 4.0.4
"""

from __future__ import annotations
import sys, pathlib, json, csv, math, random, time
from dataclasses import dataclass, asdict
from typing import List, Dict, Optional, Tuple

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

CPP_OUT = ROOT / "out" / "reports" / "thermal_loss_explorer"
PY_OUT  = ROOT / "out" / "reports" / "thermal_loss_draft"
PY_OUT.mkdir(parents=True, exist_ok=True)


# ═══════════════════════════════════════════════════════════════════════════
# 1. Read C++ outputs
# ═══════════════════════════════════════════════════════════════════════════

def load_cpp_summary() -> Optional[list]:
    path = CPP_OUT / "summary.json"
    if not path.exists():
        print(f"  [WARN] C++ summary not found at {path}")
        print(f"         Run thermal_loss_explorer first, or use --py-only")
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def load_trial_csv(trial_id: int) -> Optional[List[dict]]:
    """Find and load a trial CSV from C++ output."""
    for p in CPP_OUT.glob(f"trial_{trial_id:04d}_*.csv"):
        rows = []
        with open(p, newline="", encoding="utf-8") as f:
            for row in csv.DictReader(f):
                rows.append({k: float(v) for k, v in row.items()})
        return rows
    return None


# ═══════════════════════════════════════════════════════════════════════════
# 2. Python-side mini solver (matches C++ physics for cross-validation)
# ═══════════════════════════════════════════════════════════════════════════

MATERIALS = {
    "FLiBe":           {"k": 1.00, "rho": 1940, "Cp": 2386},
    "FLiNaK":          {"k": 0.92, "rho": 2090, "Cp": 1886},
    "NaCl_UCl3":       {"k": 0.55, "rho": 3200, "Cp": 1050},
    "Cl_ThUPu":        {"k": 0.42, "rho": 3800, "Cp":  950},
    "MSBR_ref":        {"k": 1.00, "rho": 2290, "Cp": 1357},
    "NaCl_MgCl2_UCl3": {"k": 0.50, "rho": 2100, "Cp": 1100},
    "Carbon_Steel":    {"k": 50.0, "rho": 7850, "Cp":  490},
    "Alloy_Steel":     {"k": 40.0, "rho": 7900, "Cp":  500},
}
MAT_NAMES = list(MATERIALS.keys())
PI = math.pi


@dataclass
class PyTrial:
    trial_id: int
    material: str
    Tc_init: float
    Th_init: float
    T_ambient: float
    r_inner: float
    r_outer: float
    length: float
    h_outer: float
    dt: float
    n_frames: int
    N_radial: int
    Th_decays: bool


@dataclass
class PyResult:
    trial: PyTrial
    final_Tc: float
    final_Th: float
    total_Q_loss_J: float
    rms_final: float
    converged: bool
    elapsed_s: float
    # per-frame snapshots (sampled)
    frame_Tc: List[float]
    frame_Th: List[float]
    frame_Q_cumul: List[float]


def run_py_trial(t: PyTrial) -> PyResult:
    """Python-side explicit FD solver matching the C++ version."""
    mat = MATERIALS[t.material]
    k, rho, Cp = mat["k"], mat["rho"], mat["Cp"]
    alpha = k / (rho * Cp)
    dr = (t.r_outer - t.r_inner) / max(t.N_radial - 1, 1)

    # stability sub-stepping
    dt_stable = dr * dr / (2.0 * alpha)
    sub_steps = max(1, math.ceil(t.dt / dt_stable))
    dt_sub = t.dt / sub_steps

    # initial profile
    T = [t.Tc_init + (t.Th_init - t.Tc_init) * i / max(t.N_radial - 1, 1)
         for i in range(t.N_radial)]

    Tc = t.Tc_init
    Th = t.Th_init
    Q_cumul = 0.0

    area_outer = 2 * PI * t.r_outer * t.length

    frame_Tc = []
    frame_Th = []
    frame_Q = []

    t0 = time.perf_counter()

    rms = 0.0
    for frame in range(t.n_frames):
        for _ in range(sub_steps):
            T_new = T[:]
            T_new[0] = Tc
            T_new[t.N_radial - 1] = Th
            for i in range(1, t.N_radial - 1):
                r_i = t.r_inner + i * dr
                d2T = (T[i + 1] - 2.0 * T[i] + T[i - 1]) / (dr * dr)
                dT = (T[i + 1] - T[i - 1]) / (2.0 * dr)
                T_new[i] = T[i] + alpha * dt_sub * (d2T + dT / r_i)
            T = T_new

        T_surface = T[t.N_radial - 1]
        Q_loss = t.h_outer * area_outer * max(T_surface - t.T_ambient, 0.0)
        Q_cumul += Q_loss * t.dt

        if t.Th_decays:
            dTh = -Q_loss / (rho * Cp * area_outer * dr)
            Th += dTh * t.dt
            Th = max(Th, t.T_ambient)

        Tc = T[0]
        T[t.N_radial - 1] = Th

        # RMS residual
        sum_sq = 0.0
        for i in range(1, t.N_radial - 1):
            r_i = t.r_inner + i * dr
            d2T = (T[i + 1] - 2.0 * T[i] + T[i - 1]) / (dr * dr)
            dT = (T[i + 1] - T[i - 1]) / (2.0 * dr)
            sum_sq += (d2T + dT / r_i) ** 2
        rms = math.sqrt(sum_sq / max(t.N_radial - 2, 1))

        # sample every 10th frame for storage
        if frame % 10 == 0 or frame == t.n_frames - 1:
            frame_Tc.append(round(Tc, 3))
            frame_Th.append(round(Th, 3))
            frame_Q.append(round(Q_cumul, 3))

    elapsed = time.perf_counter() - t0

    return PyResult(
        trial=t,
        final_Tc=Tc, final_Th=Th,
        total_Q_loss_J=Q_cumul, rms_final=rms,
        converged=(rms < 1e4),
        elapsed_s=round(elapsed, 4),
        frame_Tc=frame_Tc, frame_Th=frame_Th, frame_Q_cumul=frame_Q,
    )


# ═══════════════════════════════════════════════════════════════════════════
# 3. Random net caster (Python side, wider net than C++)
# ═══════════════════════════════════════════════════════════════════════════

def cast_py_net(n_trials: int = 60, seed: int = 7777) -> List[PyTrial]:
    """Cast a wider net than the C++ explorer."""
    rng = random.Random(seed)
    dt_choices = [0.00005, 0.0001, 0.0005, 0.001, 0.005, 0.01, 0.05, 0.1, 0.5]
    trials = []
    for i in range(n_trials):
        mat = rng.choice(MAT_NAMES)
        Tc = 400 + rng.random() * 1000       # 400-1400 K (wider)
        Th = Tc + 30 + rng.random() * 500     # Th > Tc by 30-530 K
        Ta = 200 + rng.random() * 200         # 200-400 K
        ri = 0.003 + rng.random() * 0.02      # 3-23 mm
        ro = ri + 0.008 + rng.random() * 0.08 # 8-88 mm gap (wider)
        length = 0.3 + rng.random() * 3.0     # 0.3-3.3 m
        h = 3.0 + rng.random() * 80.0         # 3-83 W/(m^2 K) (wider)
        dt = rng.choice(dt_choices)
        N = 12 + int(rng.random() * 52)       # 12-64 nodes
        N += N % 2
        Th_decays = rng.random() > 0.4

        trials.append(PyTrial(
            trial_id=i, material=mat,
            Tc_init=round(Tc, 1), Th_init=round(Th, 1), T_ambient=round(Ta, 1),
            r_inner=round(ri, 5), r_outer=round(ro, 5), length=round(length, 3),
            h_outer=round(h, 2), dt=dt, n_frames=200,
            N_radial=N, Th_decays=Th_decays,
        ))
    return trials


# ═══════════════════════════════════════════════════════════════════════════
# 4. Analysis
# ═══════════════════════════════════════════════════════════════════════════

def analyze_dt_sensitivity(results: List[PyResult]) -> Dict[float, dict]:
    """Group results by dt, compute avg RMS and Q_loss."""
    from collections import defaultdict
    groups: Dict[float, list] = defaultdict(list)
    for r in results:
        groups[r.trial.dt].append(r)

    summary = {}
    for dt in sorted(groups.keys()):
        rs = groups[dt]
        avg_rms = sum(r.rms_final for r in rs) / len(rs)
        avg_Q = sum(r.total_Q_loss_J for r in rs) / len(rs)
        n_conv = sum(1 for r in rs if r.converged)
        summary[dt] = {
            "dt": dt, "n_trials": len(rs),
            "n_converged": n_conv,
            "avg_rms": avg_rms, "avg_Q_loss_J": avg_Q,
        }
    return summary


def analyze_material_ranking(results: List[PyResult]) -> List[dict]:
    """Rank materials by average convergence quality."""
    from collections import defaultdict
    groups: Dict[str, list] = defaultdict(list)
    for r in results:
        groups[r.trial.material].append(r)

    ranking = []
    for mat, rs in groups.items():
        avg_rms = sum(r.rms_final for r in rs) / len(rs)
        avg_Q = sum(r.total_Q_loss_J for r in rs) / len(rs)
        ranking.append({
            "material": mat, "n_trials": len(rs),
            "avg_rms": avg_rms, "avg_Q_loss_J": avg_Q,
            "n_converged": sum(1 for r in rs if r.converged),
        })
    ranking.sort(key=lambda x: x["avg_rms"])
    return ranking


def write_results(results: List[PyResult], tag: str):
    """Write summary CSV and JSON for a result set."""
    rows = []
    for r in results:
        rows.append({
            "trial_id": r.trial.trial_id,
            "material": r.trial.material,
            "Tc_init": r.trial.Tc_init,
            "Th_init": r.trial.Th_init,
            "T_ambient": r.trial.T_ambient,
            "dt": r.trial.dt,
            "N_radial": r.trial.N_radial,
            "r_inner": r.trial.r_inner,
            "r_outer": r.trial.r_outer,
            "h_outer": r.trial.h_outer,
            "Th_decays": r.trial.Th_decays,
            "final_Tc": round(r.final_Tc, 3),
            "final_Th": round(r.final_Th, 3),
            "total_Q_loss_J": round(r.total_Q_loss_J, 2),
            "rms_final": r.rms_final,
            "converged": r.converged,
            "elapsed_s": r.elapsed_s,
        })

    json_path = PY_OUT / f"{tag}.json"
    json_path.write_text(json.dumps(rows, indent=2), encoding="utf-8")

    csv_path = PY_OUT / f"{tag}.csv"
    if rows:
        cols = list(rows[0].keys())
        with open(csv_path, "w", newline="", encoding="utf-8") as fh:
            w = csv.DictWriter(fh, fieldnames=cols)
            w.writeheader()
            w.writerows(rows)

    print(f"  -> {json_path}")
    print(f"  -> {csv_path}")


# ═══════════════════════════════════════════════════════════════════════════
# 5. Main
# ═══════════════════════════════════════════════════════════════════════════

def main():
    print()
    print("=" * 72)
    print("  THERMAL LOSS DRAFT -- Python companion analysis")
    print("=" * 72)

    # --- attempt to load C++ results ---
    cpp_summary = load_cpp_summary()
    if cpp_summary:
        print(f"\n  Loaded {len(cpp_summary)} C++ trial summaries")
        n_conv = sum(1 for t in cpp_summary if t.get("converged", False))
        print(f"  C++ converged: {n_conv}/{len(cpp_summary)}")

    # --- run Python exploration with wider net ---
    print("\n  Casting wider Python net (60 trials, 9 dt values)...")
    trials = cast_py_net(n_trials=60, seed=7777)

    t0 = time.perf_counter()
    results: List[PyResult] = []
    for t in trials:
        r = run_py_trial(t)
        results.append(r)
        tag = "OK" if r.converged else "!!"
        print(f"    #{t.trial_id:3d}  {t.material:20s}  Tc={t.Tc_init:7.1f}  Th={t.Th_init:7.1f}  "
              f"dt={t.dt:.0e}  Q={r.total_Q_loss_J:12.1f} J  rms={r.rms_final:.3e}  [{tag}]")
    elapsed = time.perf_counter() - t0

    # --- write all results ---
    print()
    write_results(results, "py_all_trials")

    # --- dt sensitivity ---
    dt_sens = analyze_dt_sensitivity(results)
    print("\n  dt SENSITIVITY:")
    for dt, s in dt_sens.items():
        print(f"    dt={dt:.0e}  avg_rms={s['avg_rms']:.3e}  "
              f"avg_Q={s['avg_Q_loss_J']:.1f} J  conv={s['n_converged']}/{s['n_trials']}")

    (PY_OUT / "dt_sensitivity.json").write_text(
        json.dumps(list(dt_sens.values()), indent=2), encoding="utf-8")

    # --- material ranking ---
    mat_rank = analyze_material_ranking(results)
    print("\n  MATERIAL RANKING (by avg RMS):")
    for m in mat_rank:
        print(f"    {m['material']:20s}  avg_rms={m['avg_rms']:.3e}  "
              f"avg_Q={m['avg_Q_loss_J']:.1f} J  conv={m['n_converged']}/{m['n_trials']}")

    (PY_OUT / "material_ranking.json").write_text(
        json.dumps(mat_rank, indent=2), encoding="utf-8")

    # --- top 5 for follow-up ---
    results.sort(key=lambda r: r.rms_final)
    top5 = results[:5]
    print("\n  TOP 5 TRIALS (for high-res follow-up):")
    for r in top5:
        print(f"    #{r.trial.trial_id:3d}  {r.trial.material:20s}  "
              f"Tc={r.trial.Tc_init:.0f}  Th={r.trial.Th_init:.0f}  "
              f"dt={r.trial.dt:.0e}  rms={r.rms_final:.3e}")

    print(f"\n  Total elapsed: {elapsed:.1f} s")
    print(f"  Output: {PY_OUT}")
    print("=" * 72)


if __name__ == "__main__":
    main()
