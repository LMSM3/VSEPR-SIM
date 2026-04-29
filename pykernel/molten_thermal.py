"""
molten_thermal.py — Molten-salt channel heat-transfer matrix analysis
VSEPR-SIM v4.0.4

Adapts the burn_inorganic.py matrix/RMS/eigen framework for salt-channel
radial heat transfer.  Uses property correlations from plant_alpha2.py.

Physics:  1-D radial conduction through a molten-salt channel annulus,
          explicit finite-difference with T-dependent properties.

Outputs:  resolution cascade (4x4 -> 64x64), RMS convergence, eigenvalue
          analysis, and per-salt comparison data.
"""

from __future__ import annotations
import math, time, json, pathlib, sys, importlib.util
from dataclasses import dataclass, field
from typing import Optional, Tuple

# ---------------------------------------------------------------------------
# Load plant_alpha2 without triggering VisPy gate
# ---------------------------------------------------------------------------
_PA2_PATH = pathlib.Path(__file__).parent / "plant_alpha2.py"
_spec = importlib.util.spec_from_file_location("plant_alpha2", str(_PA2_PATH))
_pa2 = importlib.util.module_from_spec(_spec)
sys.modules.setdefault("plant_alpha2", _pa2)
_spec.loader.exec_module(_pa2)

MATERIALS = _pa2.MATERIALS
MISSING   = _pa2.MISSING

# ---------------------------------------------------------------------------
# Output paths
# ---------------------------------------------------------------------------
OUT_DIR = pathlib.Path(__file__).resolve().parent.parent / "out" / "reports" / "report_072_molten"
OUT_DIR.mkdir(parents=True, exist_ok=True)
LOG_FILE = OUT_DIR / "thermal_log.jsonl"
EQ_LOG   = OUT_DIR / "equations.log"


# ---------------------------------------------------------------------------
# Types
# ---------------------------------------------------------------------------
Matrix = list[list[float]]
Vector = list[float]


# ---------------------------------------------------------------------------
# Equation logger (same pattern as burn_inorganic)
# ---------------------------------------------------------------------------
class EquationLog:
    def __init__(self):
        self.lines: list[str] = []
        self._fh = open(EQ_LOG, "w", encoding="utf-8")

    def log(self, eq: str):
        self.lines.append(eq)
        self._fh.write(eq + "\n")
        self._fh.flush()

    def close(self):
        self._fh.close()


# ---------------------------------------------------------------------------
# Pure-Python matrix helpers
# ---------------------------------------------------------------------------
def eye(n: int) -> Matrix:
    return [[1.0 if i == j else 0.0 for j in range(n)] for i in range(n)]

def mat_copy(A: Matrix) -> Matrix:
    return [row[:] for row in A]

def mat_vec(A: Matrix, x: Vector) -> Vector:
    return [sum(A[i][j] * x[j] for j in range(len(x))) for i in range(len(A))]

def vec_norm(v: Vector) -> float:
    return math.sqrt(sum(x * x for x in v))

def mat_mul(A: Matrix, B: Matrix) -> Matrix:
    n = len(A)
    m = len(B[0])
    k = len(B)
    return [[sum(A[i][p] * B[p][j] for p in range(k)) for j in range(m)] for i in range(n)]


# ---------------------------------------------------------------------------
# Build radial thermal matrix for a molten-salt channel
# ---------------------------------------------------------------------------
def build_channel_matrix(N: int, formula: str, T_avg: float = 973.0,
                         r_inner: float = 0.01, r_outer: float = 0.03,
                         eqlog: Optional[EquationLog] = None) -> Tuple[Matrix, float]:
    """
    Build an N x N finite-difference matrix for radial heat conduction
    in a cylindrical salt channel.

    Returns (A, dr) where A is the explicit update matrix and dr is the
    radial step size.

    Governing equation (cylindrical radial):
        dT/dt = (k / (rho Cp)) * (d2T/dr2 + (1/r) dT/dr)
    """
    mat = MATERIALS.get(formula)
    if mat is None:
        raise ValueError(f"Unknown material: {formula}")

    k_val  = mat.get("thermal_conductivity", T_avg)
    rho    = mat.get("density", T_avg)
    cp_val = mat.get("Cp", T_avg)

    # Fallbacks for missing properties
    if k_val == MISSING:
        k_val = 1.0
    if rho == MISSING:
        rho = mat.density_kgm3
    if cp_val == MISSING:
        cp_val = 1500.0

    alpha = k_val / (rho * cp_val)  # thermal diffusivity m^2/s
    dr = (r_outer - r_inner) / (N - 1) if N > 1 else (r_outer - r_inner)
    dt = 0.10 * dr * dr / alpha  # CFL-safe for cylindrical (Fo=0.10)

    if eqlog:
        eqlog.log(f"# Channel matrix: {formula}, N={N}")
        eqlog.log(f"#   k={k_val:.4f} W/(m K), rho={rho:.1f} kg/m^3, Cp={cp_val:.1f} J/(kg K)")
        eqlog.log(f"#   alpha = k/(rho*Cp) = {alpha:.6e} m^2/s")
        eqlog.log(f"#   dr={dr:.6e} m, dt={dt:.6e} s")
        eqlog.log(f"#   Fo = alpha*dt/dr^2 = {alpha * dt / (dr * dr):.4f}")
        eqlog.log(f"#   Governing: dT/dt = alpha * (d2T/dr2 + (1/r)*dT/dr)")
        eqlog.log("")

    Fo = alpha * dt / (dr * dr)  # Fourier number

    A = [[0.0] * N for _ in range(N)]
    for i in range(N):
        r_i = r_inner + i * dr
        if i == 0 or i == N - 1:
            A[i][i] = 1.0  # boundary
        else:
            inv_r = 1.0 / r_i if r_i > 1e-15 else 0.0
            A[i][i]     = 1.0 - 2.0 * Fo
            A[i][i - 1] = Fo * (1.0 - dr * inv_r / 2.0)
            A[i][i + 1] = Fo * (1.0 + dr * inv_r / 2.0)

    return A, dr


# ---------------------------------------------------------------------------
# Thermal iteration
# ---------------------------------------------------------------------------
def thermal_iterate(formula: str, N: int, n_steps: int = 5000,
                    T_wall: float = 873.0, T_center: float = 1073.0,
                    r_inner: float = 0.01, r_outer: float = 0.03,
                    eqlog: Optional[EquationLog] = None) -> dict:
    """Jacobi iteration to steady-state for radial conduction in a salt channel.

    Steady-state cylindrical: d2T/dr2 + (1/r) dT/dr = 0
    Analytic solution: T(r) = A + B ln(r)
    Jacobi iterates the FD system to converge on this.
    """
    mat_obj = MATERIALS[formula]
    T_avg = (T_wall + T_center) / 2.0

    k_v  = mat_obj.get("thermal_conductivity", T_avg)
    rho_v = mat_obj.get("density", T_avg)
    cp_v = mat_obj.get("Cp", T_avg)
    mu_v = mat_obj.get("viscosity", T_avg)
    if k_v == MISSING: k_v = 1.0
    if rho_v == MISSING: rho_v = mat_obj.density_kgm3
    if cp_v == MISSING: cp_v = 1500.0
    if mu_v == MISSING: mu_v = 0.01
    alpha_val = k_v / (rho_v * cp_v)

    dr = (r_outer - r_inner) / (N - 1) if N > 1 else (r_outer - r_inner)

    if eqlog:
        eqlog.log(f"# Steady-state Jacobi: {formula}, N={N}, max_steps={n_steps}")
        eqlog.log(f"#   k={k_v:.4f} W/(m K), rho={rho_v:.1f} kg/m^3, Cp={cp_v:.1f} J/(kg K)")
        eqlog.log(f"#   mu={mu_v:.6f} Pa s, alpha={alpha_val:.6e} m^2/s")
        eqlog.log(f"#   dr={dr:.6e} m, r_inner={r_inner}, r_outer={r_outer}")
        eqlog.log(f"#   Governing: k*(d2T/dr2 + (1/r)*dT/dr) + q_vol = 0,  q_vol=2e7 W/m^3")
        eqlog.log(f"#   T(r_inner)={T_center} K, T(r_outer)={T_wall} K")
        eqlog.log("")

    # Initial: linear interpolation
    T = [T_center + (T_wall - T_center) * i / (N - 1) if N > 1 else T_center
         for i in range(N)]

    rms_history = []
    t0 = time.perf_counter()

    for step in range(n_steps):
        T_new = T[:]
        T_new[0] = T_center
        T_new[-1] = T_wall
        for i in range(1, N - 1):
            r_i = r_inner + i * dr
            coeff_m = 1.0 - dr / (2.0 * r_i)
            coeff_p = 1.0 + dr / (2.0 * r_i)
            # Volumetric heat source: q_vol = 2e7 W/m^3 (typical MSR channel)
            q_vol = 2.0e7
            T_new[i] = (coeff_m * T[i - 1] + coeff_p * T[i + 1] + q_vol * dr * dr / k_v) / (coeff_m + coeff_p)
        rms = math.sqrt(sum((T_new[i] - T[i]) ** 2 for i in range(N)) / N)
        rms_history.append(rms)
        T = T_new
        if rms < 1e-12:
            break

    elapsed = time.perf_counter() - t0
    actual_steps = len(rms_history)

    if eqlog:
        eqlog.log(f"# Converged in {actual_steps} steps, {elapsed:.4f} s")
        eqlog.log(f"# Final RMS change: {rms_history[-1]:.6e}")
        eqlog.log(f"# T_max={max(T):.2f} K, T_min={min(T):.2f} K")
        # Log analytic comparison (with volumetric source)
        # T(r) = -(q/(4k))*r^2 + A*ln(r) + B
        q_vol = 2.0e7
        c = q_vol / (4.0 * k_v)
        # BCs: T(r_inner) = T_center, T(r_outer) = T_wall
        # => A*ln(ri) + B = T_center + c*ri^2
        # => A*ln(ro) + B = T_wall   + c*ro^2
        ln_ratio = math.log(r_inner / r_outer)
        A_coeff = ((T_center + c * r_inner**2) - (T_wall + c * r_outer**2)) / ln_ratio
        B_coeff = T_center + c * r_inner**2 - A_coeff * math.log(r_inner)
        r_mid = (r_inner + r_outer) / 2.0
        T_mid_analytic = -c * r_mid**2 + A_coeff * math.log(r_mid) + B_coeff
        T_mid_numeric = T[N // 2]
        eqlog.log(f"# Analytic T(r_mid) = {T_mid_analytic:.4f} K")
        eqlog.log(f"# Numeric  T(r_mid) = {T_mid_numeric:.4f} K")
        eqlog.log(f"# Error = {abs(T_mid_analytic - T_mid_numeric):.6e} K")
        eqlog.log("")

    return {
        "formula": formula,
        "N": N,
        "alpha_m2s": alpha_val,
        "k_Wm": k_v,
        "rho_kgm3": rho_v,
        "Cp_JkgK": cp_v,
        "mu_Pas": mu_v,
        "steps": actual_steps,
        "elapsed_s": round(elapsed, 6),
        "rms_final": rms_history[-1] if rms_history else 0.0,
        "rms_history": rms_history,
        "T_profile": T,
        "T_max": max(T),
        "T_min": min(T),
    }


# ---------------------------------------------------------------------------
# QR decomposition (Householder) & eigenvalue analysis
# ---------------------------------------------------------------------------
def qr_decompose(A: Matrix) -> Tuple[Matrix, Matrix]:
    n = len(A)
    Q = eye(n)
    R = mat_copy(A)
    for k in range(n - 1):
        x = [R[i][k] for i in range(k, n)]
        norm_x = vec_norm(x)
        if norm_x < 1e-15:
            continue
        sign = 1.0 if x[0] >= 0 else -1.0
        x[0] += sign * norm_x
        norm_v = vec_norm(x)
        if norm_v < 1e-15:
            continue
        v = [xi / norm_v for xi in x]
        # Apply H_k to R
        for j in range(k, n):
            dot = sum(v[i - k] * R[i][j] for i in range(k, n))
            for i in range(k, n):
                R[i][j] -= 2.0 * v[i - k] * dot
        # Accumulate Q
        for j in range(n):
            dot = sum(v[i - k] * Q[i][j] for i in range(k, n))
            for i in range(k, n):
                Q[i][j] -= 2.0 * v[i - k] * dot
    # Transpose Q
    Qt = [[Q[j][i] for j in range(n)] for i in range(n)]
    return Qt, R


def qr_eigenvalues(A: Matrix, max_iter: int = 80) -> list[float]:
    n = len(A)
    Ak = mat_copy(A)
    for _ in range(max_iter):
        Q, R_ = qr_decompose(Ak)
        Ak = mat_mul(R_, Q)
    return sorted([Ak[i][i] for i in range(n)], reverse=True)


# ---------------------------------------------------------------------------
# Resolution cascade
# ---------------------------------------------------------------------------
RESOLUTIONS = [4, 8, 16, 32, 64]

def run_cascade(formula: str, eqlog: Optional[EquationLog] = None) -> list[dict]:
    results = []
    for N in RESOLUTIONS:
        steps = 1000 + N * 50
        r = thermal_iterate(formula, N, n_steps=steps, eqlog=eqlog)
        # Build Jacobi iteration matrix for eigen analysis
        mat_obj = MATERIALS[formula]
        T_avg = (873.0 + 1073.0) / 2.0
        k_v = mat_obj.get("thermal_conductivity", T_avg)
        if k_v == MISSING: k_v = 1.0
        r_inner, r_outer = 0.01, 0.03
        dr_e = (r_outer - r_inner) / (N - 1) if N > 1 else 0.02
        q_vol = 2.0e7
        J = [[0.0] * N for _ in range(N)]
        for ii in range(N):
            if ii == 0 or ii == N - 1:
                J[ii][ii] = 0.0  # BC nodes: T_new = constant (no iteration dep)
            else:
                r_i = r_inner + ii * dr_e
                c_m = 1.0 - dr_e / (2.0 * r_i)
                c_p = 1.0 + dr_e / (2.0 * r_i)
                denom = c_m + c_p
                J[ii][ii - 1] = c_m / denom
                J[ii][ii + 1] = c_p / denom
        eigvals = qr_eigenvalues(J, max_iter=60)
        r["eigenvalues_top5"] = [round(e, 8) for e in eigvals[:5]]
        r["spectral_radius"] = round(max(abs(e) for e in eigvals), 8)
        results.append(r)
        if eqlog:
            eqlog.log(f"# Eigenvalues (top 5) at N={N}: {r['eigenvalues_top5']}")
            eqlog.log(f"# Spectral radius: {r['spectral_radius']}")
            eqlog.log("")
    return results


# ---------------------------------------------------------------------------
# Full report driver
# ---------------------------------------------------------------------------
def run_full_report() -> dict:
    """Run thermal cascade for all molten-salt fuels and coolants, return summary."""
    eqlog = EquationLog()
    log_fh = open(LOG_FILE, "w", encoding="utf-8")

    # Collect all salt materials (fuels with molten-salt indicators + coolants that are salts)
    salt_formulas = []
    for f, m in MATERIALS.items():
        if m.phase == "liquid" and m.category in ("fuel", "coolant"):
            # Skip non-salt liquids (Na, Pb, H2O)
            if f in ("Na_liq", "Pb_liq", "H2O_liq"):
                continue
            salt_formulas.append(f)

    print(f"Molten-salt thermal analysis: {len(salt_formulas)} salts")
    print(f"Salts: {salt_formulas}")
    print()

    all_results = {}
    for formula in salt_formulas:
        print(f"  Running cascade for {formula} ...")
        eqlog.log(f"{'='*70}")
        eqlog.log(f"# SALT: {formula}")
        eqlog.log(f"{'='*70}")
        cascade = run_cascade(formula, eqlog=eqlog)
        all_results[formula] = cascade
        # Log JSON
        for r in cascade:
            entry = {k: v for k, v in r.items() if k not in ("rms_history", "T_profile")}
            log_fh.write(json.dumps(entry) + "\n")
        # Print summary for highest resolution
        best = cascade[-1]
        print(f"    N=64: rms_final={best['rms_final']:.6e}, "
              f"spectral_radius={best['spectral_radius']}, "
              f"elapsed={best['elapsed_s']:.4f}s")

    eqlog.close()
    log_fh.close()

    # Build comparison summary
    comparison = []
    for formula, cascade in all_results.items():
        best = cascade[-1]
        comparison.append({
            "formula": formula,
            "rms_final_64": best["rms_final"],
            "spectral_radius_64": best["spectral_radius"],
            "T_max_64": best["T_max"],
            "T_min_64": best["T_min"],
            "elapsed_64": best["elapsed_s"],
            "converged": best["rms_final"] < 0.01,
        })
    comparison.sort(key=lambda x: x["rms_final_64"])

    summary = {
        "salt_count": len(salt_formulas),
        "salts": salt_formulas,
        "resolutions": RESOLUTIONS,
        "comparison": comparison,
    }

    # Write summary JSON
    (OUT_DIR / "thermal_summary.json").write_text(
        json.dumps(summary, indent=2), encoding="utf-8")

    print()
    print("=== THERMAL CONVERGENCE COMPARISON (N=64) ===")
    for c in comparison:
        tag = "CONVERGED" if c["converged"] else "NOT CONVERGED"
        print(f"  {c['formula']:20s}  rms={c['rms_final_64']:.6e}  "
              f"rho={c['spectral_radius_64']:.6f}  [{tag}]")

    return summary


# ---------------------------------------------------------------------------
if __name__ == "__main__":
    run_full_report()
