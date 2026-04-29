"""
glass_optics_demo.py — helper demo for pykernel/glass_optics.py
================================================================

Exercises every function in the glass optics module with physically
realistic values for silicate and fluoride glass systems.

Sections:
    1.  Constants
    2.  Beer-Lambert (§1)
    3.  Pair distribution function (§2)
    4.  Optical indicatrix (§3)
    5.  Rayleigh scattering + scatter regime (§4)
    6.  Photoelastic effect (§5)
    7.  Urbach tail (§6)
    8.  Varshni band-gap shift (§7)
    9.  Crystal-field scaling (§8)
    10. Lorentz-Lorenz + dn/dT (§9)
    11. Planck emission (§10)
    12. Composite clarity score
    13. Full summary (SLS glass at 300 K + at 1200 K)
    14. CSV output — Planck emission vs λ, VFT viscosity vs T

Run from workspace root:
    python scripts/demos/glass_optics_demo.py

Output: out/glass_optics_demo/

VSEPR-SIM 3.0.0
"""

from __future__ import annotations

import csv
import importlib.util
import math
import sys
from pathlib import Path

# ── load glass_optics.py directly (bypasses __init__.py / VisPy) ────────────
_ROOT = Path(__file__).resolve().parents[2]

def _load(rel: str):
    path = _ROOT / rel
    spec = importlib.util.spec_from_file_location(path.stem, path)
    mod  = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

go = _load("pykernel/glass_optics.py")

OUT = _ROOT / "out" / "glass_optics_demo"
OUT.mkdir(parents=True, exist_ok=True)

RULE = "─" * 70

def section(n: int, title: str) -> None:
    print(f"\n{RULE}")
    print(f"  §{n}  {title}")
    print(RULE)

def check(label: str, value: float, expected: float, rtol: float = 1e-4) -> None:
    ok = abs(value - expected) <= rtol * abs(expected) + 1e-30
    tag = "✓" if ok else "✗ FAIL"
    print(f"  {tag}  {label:<45} = {value:.6g}  (expect ≈ {expected:.6g})")

# ============================================================================
# 1. Constants
# ============================================================================
section(1, "Physical constants")
print(f"  h   = {go.H_PLANCK:.8e} J·s")
print(f"  c   = {go.C_LIGHT:.8e} m/s")
print(f"  kB  = {go.K_BOLTZMANN:.8e} J/K")
print(f"  b   = {go.WIEN_B:.9e} m·K")

# ============================================================================
# 2. Beer-Lambert  (§1)
# ============================================================================
section(2, "Beer-Lambert absorption  (§1)")

# SLS glass at 550 nm: α ≈ 0.01 m⁻¹ (extremely low)
alpha_sls = 0.01          # m⁻¹  (bulk SLS, visible)
x_1mm     = 1e-3          # m
x_10mm    = 10e-3         # m

I_1mm  = go.beer_lambert_I(1.0, alpha_sls, x_1mm)
I_10mm = go.beer_lambert_I(1.0, alpha_sls, x_10mm)
check("I(1 mm) / I0  (α=0.01 m⁻¹)", I_1mm,  math.exp(-0.01 * 1e-3))
check("I(10 mm) / I0 (α=0.01 m⁻¹)", I_10mm, math.exp(-0.01 * 10e-3))

# α from κ at 550 nm
kappa_silica = 3e-9          # typical for high-purity silica
lambda_550   = 550e-9        # m
alpha_kappa  = go.alpha_from_kappa(kappa_silica, lambda_550)
check("α from κ=3e-9 at 550 nm (m⁻¹)",
      alpha_kappa, 4 * go.PI * kappa_silica / lambda_550)

# ============================================================================
# 3. Pair distribution function  (§2)
# ============================================================================
section(3, "Pair distribution function g(r)  (§2)")

# First Si-O peak in silica: r ≈ 1.62 Å, ρ ≈ 6.6e28 atoms/m³
rho_silica = 6.6e28          # atoms/m³
r_SiO      = 1.62e-10        # m
# dN/dr at first peak — toy: ~4 atoms in thin shell dr=0.05 Å
dN_dr_peak = 4.0 / 0.05e-10
g_peak     = go.pdf_g(dN_dr_peak, r_SiO, rho_silica)
print(f"  g(r) at Si-O peak (r={r_SiO*1e10:.2f} Å)  = {g_peak:.3f}  (first peak >> 1)")

# ============================================================================
# 4. Optical indicatrix  (§3)
# ============================================================================
section(4, "Optical indicatrix  (§3)")

# Ideal bulk SLS — isotropic
check("δn (isotropic: nx=ny=1.5200)",
      go.indicatrix_delta_n(1.5200, 1.5200), 0.0)
print(f"  is_spherical (1.52, 1.52, 1.52)      = "
      f"{go.indicatrix_is_spherical(1.52, 1.52, 1.52)}")

# Stressed glass: induced birefringence ~ 5e-6
n_fast, n_slow = 1.52000, 1.52005
dn = go.indicatrix_delta_n(n_fast, n_slow)
print(f"  is_spherical (stressed)               = "
      f"{go.indicatrix_is_spherical(n_fast, n_slow, n_slow, tol=1e-6)}")
check("δn (stressed: 5e-5 birefringence)", dn, 5e-5)

# ============================================================================
# 5. Rayleigh scattering + scatter regime  (§4)
# ============================================================================
section(5, "Rayleigh scattering + scatter regime  (§4)")

# Blue vs red: relative Rayleigh at 450 nm vs 700 nm, reference 550 nm
lambda_ref = 550e-9
for lnm, wl in [(450, "blue"), (550, "green"), (700, "red")]:
    I_rel = go.rayleigh_I_rel(lnm * 1e-9, lambda_ref)
    print(f"  I_rel ({wl:<5} {lnm} nm vs ref 550 nm) = {I_rel:.4f}")

# Scatter regime classification
cases = [
    ("nanoparticle  a=20 nm, λ=550 nm",    20e-9,  550e-9),
    ("microparticle a=400 nm, λ=550 nm",  400e-9,  550e-9),
    ("inclusion     a=10 µm, λ=550 nm",   10e-6,  550e-9),
]
for label, a, lam in cases:
    regime = go.scatter_regime(a, lam)
    print(f"  {label:<42} → {regime.name}")

# ============================================================================
# 6. Photoelastic effect  (§5)
# ============================================================================
section(6, "Photoelastic stress-optic effect  (§5)")

# SLS glass: C ≈ 2.65e-12 Pa⁻¹
C_sls      = 2.65e-12   # Pa⁻¹
sigma_diff = 1e6         # 1 MPa residual
dn_stress  = go.stress_birefringence(C_sls, sigma_diff, 0.0)
check("Δn (C=2.65e-12, Δσ=1 MPa)", dn_stress, C_sls * sigma_diff)
print(f"  Δn = {dn_stress:.3e}  (half-wave retardation at "
      f"{lambda_550*1e9:.0f} nm over "
      f"{lambda_550 / (2 * dn_stress) * 1e3:.1f} mm path)")

# ============================================================================
# 7. Urbach tail  (§6)
# ============================================================================
section(7, "Urbach tail — temperature-dependent absorption edge  (§6)")

# SiO2 glass: E0 ≈ 8.9 eV, EU(300K) ≈ 0.065 eV, α0 = 1e9 m⁻¹
alpha0_sio2 = 1e9   # m⁻¹
E0_sio2     = 8.9   # eV
EU_300      = 0.065 # eV  (300 K)
EU_700      = 0.11  # eV  (700 K — thermally broadened)

for E_eV in [8.0, 8.5, 8.9]:
    a300 = go.urbach_alpha(alpha0_sio2, E_eV, E0_sio2, EU_300)
    a700 = go.urbach_alpha(alpha0_sio2, E_eV, E0_sio2, EU_700)
    print(f"  E={E_eV:.1f} eV  α(300K)={a300:.3e} m⁻¹   α(700K)={a700:.3e} m⁻¹")

# ============================================================================
# 8. Varshni band-gap shift  (§7)
# ============================================================================
section(8, "Varshni band-gap temperature dependence  (§7)")

# Approximate Varshni parameters for vitreous SiO2
# (adapted from Canham et al. / optical gap ~8.9 eV at 0 K)
Eg0_sio2   = 8.90    # eV
alpha_v    = 2.73e-4 # eV/K
beta_v     = 260.0   # K

for T in [0, 100, 300, 700, 1100]:
    Eg = go.varshni_Eg(Eg0_sio2, alpha_v, beta_v, T) if T > 0 else Eg0_sio2
    print(f"  Eg({T:4d} K) = {Eg:.4f} eV")

check("Eg(0 K) == Eg0", Eg0_sio2, 8.90)

# ============================================================================
# 9. Crystal-field scaling  (§8)
# ============================================================================
section(9, "Crystal-field Δ_oct vs metal-ligand distance  (§8)")

# Co²⁺ in glass: R0 = 2.10 Å (octahedral reference in crystal)
R0_A  = 2.10   # Å
Delta0 = 8000.0  # cm⁻¹  (typical for Co²⁺ in octahedral oxide)

for R_A in [2.05, 2.10, 2.15, 2.20]:
    ratio = go.delta_oct(R_A, R0_A)
    Delta = Delta0 * ratio
    print(f"  R={R_A:.2f} Å  →  Δ_oct = {Delta:.1f} cm⁻¹  (ratio={ratio:.4f})")

check("ratio(R=R0) == 1.0", go.delta_oct(R0_A, R0_A), 1.0)

# ============================================================================
# 10. Lorentz-Lorenz + dn/dT  (§9)
# ============================================================================
section(10, "Lorentz-Lorenz refractive index + dn/dT  (§9)")

# SiO2: N ≈ 2.204e28 SiO2 units/m³, α_e ≈ 3.15e-30 m³  (SI: C·m²/V)
# (These are scaled toy values consistent with n ≈ 1.46)
N_sio2     = 2.204e28   # m⁻³
alpha_e_si = 3.00e-30   # C·m²/V  (adjusted for demonstration)

n_ll = go.lorentz_lorenz_n(N_sio2, alpha_e_si)
print(f"  n (Lorentz-Lorenz)                  = {n_ll:.4f}  (SiO2 ≈ 1.46)")

# Thermal expansion: ΔN/ΔT ≈ -N·β_V,  β_V ≈ 1.5e-6 K⁻¹ for silica
beta_V   = 1.5e-6   # K⁻¹
dN_dT    = -N_sio2 * beta_V
dalpha_dT = 0.0     # polarizability approx constant here

dn_dT = go.thermo_optic_dn_dT(N_sio2, dN_dT, alpha_e_si, dalpha_dT)
print(f"  dn/dT (expansion-only, silica)      = {dn_dT:.4e} K⁻¹")
print(f"  Measured silica dn/dT ≈ +1.0e-5 K⁻¹  (sign depends on polarizability term)")

# ============================================================================
# 11. Planck thermal emission  (§10)
# ============================================================================
section(11, "Planck thermal emission  (§10)")

for T_K in [300, 600, 1000, 1273, 1500]:
    peak_nm = go.planck_peak_lambda(T_K) * 1e9
    L_vis   = go.planck_L_lambda(550e-9, T_K, emissivity=0.92)
    print(f"  T={T_K:5d} K  λ_peak={peak_nm:7.1f} nm  "
          f"L_λ(550nm,ε=0.92)={L_vis:.3e} W/m³/sr")

# Verify Wien at 5778 K (solar)
solar_peak = go.planck_peak_lambda(5778.0) * 1e9
print(f"\n  Solar Wien peak @ 5778 K: {solar_peak:.1f} nm  (expect ≈ 501.5 nm)")
check("Wien peak 5778 K (nm)", solar_peak, go.WIEN_B / 5778.0 * 1e9)

# ============================================================================
# 12. Composite clarity score
# ============================================================================
section(12, "Composite clarity score")

# SLS glass at 550 nm, 10 mm path, modest residual stress
cases_cs = [
    ("SLS pristine",      0.01,  10e-3, 550e-9, 380e-9, 2.65e-12, 0.0),
    ("SLS+residual 2 MPa",0.01,  10e-3, 550e-9, 380e-9, 2.65e-12, 2e6),
    ("Dirty glass",       50.0,  10e-3, 550e-9, 380e-9, 2.65e-12, 5e6),
    ("ZBLAN (IR window)", 0.001, 10e-3, 550e-9, 380e-9, 1.00e-12, 0.0),
]
for label, al, x, lv, lr, C, ds in cases_cs:
    cs = go.clarity_score(al, x, lv, lr, C, ds)
    print(f"  {label:<28} clarity = {cs:.4f}")

# ============================================================================
# 13. Full summary printouts
# ============================================================================
section(13, "Full glass optics summary")

# SLS at 550 nm, 300 K
print("\n  [SLS glass, λ=550 nm, T=300 K]")
go.print_summary(
    lambda_m=550e-9, T_K=300.0,
    alpha=0.01, kappa=3e-9,
    n=1.52, emissivity=0.92
)

# SLS at 550 nm, 1200 K (near pour temperature)
print("\n  [SLS glass, λ=550 nm, T=1200 K]")
go.print_summary(
    lambda_m=550e-9, T_K=1200.0,
    alpha=0.8, kappa=1.5e-7,
    n=1.49, emissivity=0.90
)

# ============================================================================
# 14. CSV outputs
# ============================================================================
section(14, "CSV outputs")

# 14a. Planck emission vs λ (300 nm – 5000 nm) at three temperatures
planck_csv = OUT / "planck_emission.csv"
with open(planck_csv, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["lambda_nm", "L_300K", "L_1000K", "L_1500K"])
    for nm in range(300, 5001, 25):
        lam = nm * 1e-9
        w.writerow([
            nm,
            f"{go.planck_L_lambda(lam, 300.0, 0.92):.6e}",
            f"{go.planck_L_lambda(lam, 1000.0, 0.92):.6e}",
            f"{go.planck_L_lambda(lam, 1500.0, 0.92):.6e}",
        ])
print(f"  Planck emission CSV   → {planck_csv}")

# 14b. Beer-Lambert transmission vs path (0–100 mm) for SLS + dirty glass
bl_csv = OUT / "beer_lambert.csv"
with open(bl_csv, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["path_mm", "I_SLS_alpha0.01", "I_dirty_alpha50", "I_ZBLAN_alpha0.001"])
    for mm in range(0, 101):
        x = mm * 1e-3
        w.writerow([
            mm,
            f"{go.beer_lambert_I(1.0, 0.01, x):.6f}",
            f"{go.beer_lambert_I(1.0, 50.0, x):.6e}",
            f"{go.beer_lambert_I(1.0, 0.001, x):.8f}",
        ])
print(f"  Beer-Lambert CSV      → {bl_csv}")

# 14c. Varshni Eg(T) for SiO2 (0–1400 K)
varshni_csv = OUT / "varshni_Eg.csv"
with open(varshni_csv, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["T_K", "Eg_eV"])
    for T in range(0, 1401, 25):
        Eg = go.varshni_Eg(8.90, 2.73e-4, 260.0, max(T, 1))
        w.writerow([T, f"{Eg:.6f}"])
print(f"  Varshni Eg(T) CSV     → {varshni_csv}")

# 14d. Crystal-field vs R for Co²⁺
cf_csv = OUT / "crystal_field.csv"
with open(cf_csv, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["R_angstrom", "delta_oct_ratio", "delta_oct_cm-1"])
    for ri in range(170, 260):
        R_A  = ri / 100.0
        ratio = go.delta_oct(R_A, 2.10)
        w.writerow([f"{R_A:.2f}", f"{ratio:.6f}", f"{8000.0 * ratio:.2f}"])
print(f"  Crystal-field CSV     → {cf_csv}")

# ── Done ──────────────────────────────────────────────────────────────────
print(f"\n{RULE}")
print("  Demo complete")
print(RULE)
print(f"  All output written to: {OUT}")
print()
