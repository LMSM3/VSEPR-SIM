"""
molten_salt_experiments.py -- Extended molten-salt thermal experiments
=====================================================================

Runs three experiment sets beyond the baseline report_072 run:

  Experiment 1 -- Temperature sweep (700 K to 1200 K center, 600 K wall)
                  Tests thermal-property sensitivity across operating range

  Experiment 2 -- Geometry sweep (thin annulus to thick annulus)
                  Varies r_outer from 0.02 to 0.08 m, fixed r_inner=0.01

  Experiment 3 -- High-resolution convergence (N up to 128)
                  Pushes resolution beyond the baseline 64-node cascade

All experiments use the existing molten_thermal infrastructure and
stream results through pykernel.pipe for CSV/JSON export.

VSEPR-SIM 4.0.4
"""

from __future__ import annotations
import sys, pathlib, json, time, math

# ── ensure venv modules available ──────────────────────────────────────────
ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

import importlib.util

def _load(name: str, path: pathlib.Path):
    if name in sys.modules:
        return sys.modules[name]
    spec = importlib.util.spec_from_file_location(name, str(path))
    mod  = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod

mt   = _load("pykernel.molten_thermal", ROOT / "pykernel" / "molten_thermal.py")
pipe = _load("pykernel.pipe",           ROOT / "pykernel" / "pipe.py")

MATERIALS   = mt.MATERIALS
MISSING     = mt.MISSING
EquationLog = mt.EquationLog

# ── output directory ───────────────────────────────────────────────────────
OUT = ROOT / "out" / "reports" / "molten_experiments"
OUT.mkdir(parents=True, exist_ok=True)

# ── salt list (same filter as run_full_report) ─────────────────────────────
SALTS = [f for f, m in MATERIALS.items()
         if m.phase == "liquid" and m.category in ("fuel", "coolant")
         and f not in ("Na_liq", "Pb_liq", "H2O_liq")]

CONV_THRESHOLD = 1e-2  # rms below this = converged

def _converged(r: dict) -> bool:
    return r["rms_final"] < CONV_THRESHOLD


def _write_results(results: list, name: str):
    """Write results to CSV and JSON."""
    import csv as _csv
    json_path = OUT / f"{name}.json"
    json_path.write_text(json.dumps(results, indent=2), encoding="utf-8")
    csv_path = OUT / f"{name}.csv"
    if results:
        cols = list(results[0].keys())
        with open(csv_path, "w", newline="", encoding="utf-8") as fh:
            w = _csv.DictWriter(fh, fieldnames=cols)
            w.writeheader()
            w.writerows(results)
    print(f"\n  -> {json_path}")
    print(f"  -> {csv_path}")


# ══════════════════════════════════════════════════════════════════════════
# Helpers
# ══════════════════════════════════════════════════════════════════════════

def _props_at(formula: str, T: float) -> dict:
    m = MATERIALS[formula]
    k  = m.get("thermal_conductivity", T)
    rho = m.get("density", T)
    cp = m.get("Cp", T)
    mu = m.get("viscosity", T)
    if k  == MISSING: k  = 1.0
    if rho == MISSING: rho = m.density_kgm3
    if cp == MISSING: cp = 1500.0
    if mu == MISSING: mu = 0.01
    return {"k": k, "rho": rho, "Cp": cp, "mu": mu,
            "alpha": k / (rho * cp)}


# ══════════════════════════════════════════════════════════════════════════
# Experiment 1 -- Temperature Sweep
# ══════════════════════════════════════════════════════════════════════════

def experiment_temperature_sweep():
    """Sweep center temperature from 700 K to 1200 K in 100 K steps,
    wall fixed at 600 K. Run each salt at N=32 for speed."""
    print()
    print("=" * 72)
    print("  EXPERIMENT 1 -- Temperature Sweep (T_center 700..1200 K)")
    print("=" * 72)

    T_wall = 600.0
    centers = [700.0, 800.0, 900.0, 1000.0, 1100.0, 1200.0]
    N = 32
    n_steps = 2000

    results = []
    for formula in SALTS:
        print(f"\n  Salt: {formula}")
        for T_c in centers:
            props = _props_at(formula, (T_c + T_wall) / 2.0)
            r = mt.thermal_iterate(formula, N, n_steps=n_steps,
                                   T_wall=T_wall, T_center=T_c)
            row = {
                "experiment": "temp_sweep",
                "formula": formula,
                "T_center_K": T_c,
                "T_wall_K": T_wall,
                "N": N,
                "rms_final": r["rms_final"],
                "converged": _converged(r),
                "k_W_mK": props["k"],
                "rho_kgm3": props["rho"],
                "Cp_JkgK": props["Cp"],
                "alpha_m2s": props["alpha"],
                "T_profile_mid": r["T_profile"][N // 2] if r["T_profile"] else None,
            }
            results.append(row)
            tag = "OK" if _converged(r) else "!!"
            print(f"    T_c={T_c:.0f}K  rms={r['rms_final']:.4e}  "
                  f"T_mid={row['T_profile_mid']:.1f}K  [{tag}]")

    _write_results(results, "exp1_temp_sweep")
    return results


# ══════════════════════════════════════════════════════════════════════════
# Experiment 2 -- Geometry Sweep
# ══════════════════════════════════════════════════════════════════════════

def experiment_geometry_sweep():
    """Vary annulus outer radius from 0.02 to 0.08 m (inner fixed at 0.01).
    Tests how channel thickness affects convergence and temperature profile."""
    print()
    print("=" * 72)
    print("  EXPERIMENT 2 -- Geometry Sweep (r_outer 0.02..0.08 m)")
    print("=" * 72)

    r_inner = 0.01
    r_outers = [0.02, 0.03, 0.04, 0.05, 0.06, 0.08]
    T_wall = 873.0
    T_center = 1073.0
    N = 32
    n_steps = 2000

    results = []
    for formula in SALTS:
        print(f"\n  Salt: {formula}")
        for r_o in r_outers:
            thickness_mm = (r_o - r_inner) * 1000.0
            r = mt.thermal_iterate(formula, N, n_steps=n_steps,
                                   T_wall=T_wall, T_center=T_center,
                                   r_inner=r_inner, r_outer=r_o)
            row = {
                "experiment": "geom_sweep",
                "formula": formula,
                "r_inner_m": r_inner,
                "r_outer_m": r_o,
                "thickness_mm": thickness_mm,
                "N": N,
                "rms_final": r["rms_final"],
                "converged": _converged(r),
                "T_profile_mid": r["T_profile"][N // 2] if r["T_profile"] else None,
                "T_profile_inner": r["T_profile"][0] if r["T_profile"] else None,
                "T_profile_outer": r["T_profile"][-1] if r["T_profile"] else None,
            }
            results.append(row)
            tag = "OK" if _converged(r) else "!!"
            print(f"    r_o={r_o:.3f}m ({thickness_mm:.0f}mm)  rms={r['rms_final']:.4e}  "
                  f"T_mid={row['T_profile_mid']:.1f}K  [{tag}]")

    _write_results(results, "exp2_geom_sweep")
    return results


# ══════════════════════════════════════════════════════════════════════════
# Experiment 3 -- High-Resolution Convergence
# ══════════════════════════════════════════════════════════════════════════

def experiment_high_resolution():
    """Push grid resolution to 128 nodes and compare RMS convergence
    across resolutions 8, 16, 32, 64, 96, 128 for a subset of salts."""
    print()
    print("=" * 72)
    print("  EXPERIMENT 3 -- High-Resolution Convergence (up to N=128)")
    print("=" * 72)

    resolutions = [8, 16, 32, 64, 96, 128]
    # Use a representative subset to keep runtime reasonable
    subset = ["FLiBe", "FLiNaK", "NaCl_UCl3", "Cl_ThUPu", "MSBR_ref"]
    subset = [s for s in subset if s in SALTS]

    T_wall = 873.0
    T_center = 1073.0

    results = []
    for formula in subset:
        print(f"\n  Salt: {formula}")
        for N in resolutions:
            n_steps = 1500 + N * 40
            eqlog = None
            if N == 128:
                eqlog = EquationLog()

            r = mt.thermal_iterate(formula, N, n_steps=n_steps,
                                   T_wall=T_wall, T_center=T_center)

            # eigenvalue analysis
            mat_obj = MATERIALS[formula]
            T_avg = (T_wall + T_center) / 2.0
            k_v = mat_obj.get("thermal_conductivity", T_avg)
            if k_v == MISSING: k_v = 1.0
            dr_e = (0.03 - 0.01) / (N - 1) if N > 1 else 0.02

            # Build Jacobi iteration matrix
            J = [[0.0] * N for _ in range(N)]
            for ii in range(1, N - 1):
                r_i = 0.01 + ii * dr_e
                c_m = 1.0 - dr_e / (2.0 * r_i)
                c_p = 1.0 + dr_e / (2.0 * r_i)
                denom = c_m + c_p
                J[ii][ii - 1] = c_m / denom
                J[ii][ii + 1] = c_p / denom

            eigvals = mt.qr_eigenvalues(J, max_iter=80)
            spec_radius = max(abs(e) for e in eigvals) if eigvals else 0.0

            if eqlog:
                eqlog.close()

            row = {
                "experiment": "high_res",
                "formula": formula,
                "N": N,
                "n_steps": n_steps,
                "rms_final": r["rms_final"],
                "converged": _converged(r),
                "spectral_radius": round(spec_radius, 8),
                "eigenvalue_top": round(eigvals[0], 8) if eigvals else 0.0,
                "T_profile_mid": r["T_profile"][N // 2] if r["T_profile"] else None,
            }
            results.append(row)
            tag = "OK" if _converged(r) else "!!"
            print(f"    N={N:4d}  steps={n_steps:5d}  rms={r['rms_final']:.6e}  "
                  f"rho_spec={spec_radius:.6f}  [{tag}]")

    _write_results(results, "exp3_high_res")
    return results


# ══════════════════════════════════════════════════════════════════════════
# Summary
# ══════════════════════════════════════════════════════════════════════════

def run_all():
    """Run all three experiments and write a combined summary."""
    t0 = time.perf_counter()

    r1 = experiment_temperature_sweep()
    r2 = experiment_geometry_sweep()
    r3 = experiment_high_resolution()

    elapsed = time.perf_counter() - t0

    summary = {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "elapsed_s": round(elapsed, 2),
        "experiments": {
            "temp_sweep":  {"count": len(r1), "salts": len(SALTS), "points_per_salt": 6},
            "geom_sweep":  {"count": len(r2), "salts": len(SALTS), "points_per_salt": 6},
            "high_res":    {"count": len(r3), "salts": 5, "resolutions": [8,16,32,64,96,128]},
        },
        "total_data_points": len(r1) + len(r2) + len(r3),
    }

    (OUT / "experiments_summary.json").write_text(
        json.dumps(summary, indent=2), encoding="utf-8")

    print()
    print("=" * 72)
    print("  ALL EXPERIMENTS COMPLETE")
    print(f"  Total data points: {summary['total_data_points']}")
    print(f"  Elapsed: {elapsed:.1f} s")
    print(f"  Output:  {OUT}")
    print("=" * 72)

    return summary


if __name__ == "__main__":
    run_all()
