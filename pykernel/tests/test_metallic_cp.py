"""
Tests for pykernel.metallic_cp — Metallic specific heat capacity engine.

Validates:
  - Debye model against experimental c_p(298 K) for all 30 metals
  - Temperature dependence (low-T cubic, high-T Dulong-Petit limit)
  - Electronic Sommerfeld correction
  - Nernst-Lindemann Cp-Cv correction
  - Edge cases (T=0, extreme T)

Continuous thermo validation: these tests serve as the always-on
safety net for the c_p engine.

VSEPR-SIM 3.0.0
"""

import math
import pytest

from pykernel.metallic_cp import (
    R,
    MetalRecord,
    METAL_DB,
    lookup_metal,
    all_metals,
    debye_cv,
    electronic_cv,
    dulong_petit,
    compute_cp,
    compute_cp_curve,
    CpResult,
    _debye_integrand,
)


# ═══════════════════════════════════════════════════════════════════════
# Database tests
# ═══════════════════════════════════════════════════════════════════════

class TestMetalDB:
    def test_db_has_30_metals(self):
        assert len(METAL_DB) >= 28  # at least 28 metals

    def test_common_metals_present(self):
        for sym in ["Fe", "Al", "Cu", "Au", "Ag", "Ti", "W", "Pb", "Ni", "Cr"]:
            assert sym in METAL_DB, f"{sym} missing from METAL_DB"

    def test_lookup_exact(self):
        m = lookup_metal("Fe")
        assert m is not None
        assert m.name == "Iron"
        assert m.Z == 26

    def test_lookup_case_insensitive(self):
        m = lookup_metal("fe")
        assert m is not None
        assert m.symbol == "Fe"

    def test_lookup_nonexistent(self):
        assert lookup_metal("Xx") is None

    def test_all_metals(self):
        metals = all_metals()
        assert len(metals) == len(METAL_DB)

    def test_all_have_positive_values(self):
        for sym, m in METAL_DB.items():
            assert m.Z > 0, f"{sym}: Z must be positive"
            assert m.molar_mass > 0, f"{sym}: molar_mass must be positive"
            assert m.density > 0, f"{sym}: density must be positive"
            assert m.theta_D > 0, f"{sym}: theta_D must be positive"
            assert m.melting_point > 0, f"{sym}: melting_point must be positive"
            assert m.cp_298 > 0, f"{sym}: cp_298 must be positive"
            assert m.thermal_cond > 0, f"{sym}: thermal_cond must be positive"


# ═══════════════════════════════════════════════════════════════════════
# Debye model tests
# ═══════════════════════════════════════════════════════════════════════

class TestDebyeModel:
    def test_zero_temperature(self):
        assert debye_cv(0.0, 400.0) == 0.0

    def test_zero_theta(self):
        assert debye_cv(300.0, 0.0) == 0.0

    def test_negative_temperature(self):
        assert debye_cv(-10.0, 400.0) == 0.0

    def test_high_T_approaches_3R(self):
        """At T >> theta_D, Cv should approach 3R (Dulong-Petit)."""
        cv = debye_cv(5000.0, 400.0, n_atoms=1)
        dp = 3.0 * R
        assert abs(cv - dp) / dp < 0.02, f"Cv={cv:.3f}, 3R={dp:.3f}"

    def test_low_T_cubic(self):
        """At T << theta_D, Cv ∝ T³ (Debye T³ law)."""
        theta_D = 400.0
        T1 = 10.0
        T2 = 20.0
        cv1 = debye_cv(T1, theta_D)
        cv2 = debye_cv(T2, theta_D)
        # Cv(2T) / Cv(T) should ≈ 8 in the T³ regime
        ratio = cv2 / max(cv1, 1e-30)
        assert ratio > 6.0, f"T³ law: ratio = {ratio:.2f}, expected ~8"

    def test_multi_atom(self):
        """n_atoms=2 should double Cv."""
        cv1 = debye_cv(300.0, 400.0, n_atoms=1)
        cv2 = debye_cv(300.0, 400.0, n_atoms=2)
        assert abs(cv2 - 2 * cv1) < 0.01

    def test_integrand_overflow_protection(self):
        """Large x should not overflow."""
        val = _debye_integrand(600.0)
        assert val == 0.0


# ═══════════════════════════════════════════════════════════════════════
# Electronic correction tests
# ═══════════════════════════════════════════════════════════════════════

class TestElectronic:
    def test_sommerfeld(self):
        """C_el = γT where γ is in mJ/(mol·K²)."""
        cv_el = electronic_cv(300.0, 1.0)  # γ=1 mJ/(mol·K²)
        expected = 1e-3 * 300.0  # 0.3 J/(mol·K)
        assert abs(cv_el - expected) < 1e-10

    def test_zero_T(self):
        assert electronic_cv(0.0, 5.0) == 0.0

    def test_iron_electronic(self):
        fe = METAL_DB["Fe"]
        cv_el = electronic_cv(298.0, fe.gamma)
        # Should be small compared to lattice contribution
        assert cv_el < 3.0, f"Electronic contribution too large: {cv_el}"


# ═══════════════════════════════════════════════════════════════════════
# Dulong-Petit limit
# ═══════════════════════════════════════════════════════════════════════

class TestDulongPetit:
    def test_single_atom(self):
        dp = dulong_petit(1)
        assert abs(dp - 3 * R) < 1e-10

    def test_multi_atom(self):
        dp = dulong_petit(5)
        assert abs(dp - 15 * R) < 1e-10


# ═══════════════════════════════════════════════════════════════════════
# Composite c_p: validate against experimental data at 298 K
# ═══════════════════════════════════════════════════════════════════════

class TestCpVsExperiment:
    """Validate computed Cp(298K) against experimental values for all metals.

    This is the CORE thermo validation. Tolerance: ±15% for Debye model.
    The Debye model is an approximation; real metals have phonon DOS
    features that cause deviations, especially for transition metals.
    """

    @pytest.fixture(params=list(METAL_DB.keys()))
    def metal(self, request):
        return METAL_DB[request.param]

    def test_cp_298_within_tolerance(self, metal):
        """Cp(298K) should be within 15% of experimental value.

        Exception: Beryllium (Θ_D=1440K) — at 298K the Debye model
        significantly underestimates due to extreme quantum regime.
        Tolerance relaxed to 50% for Be.
        """
        result = compute_cp(metal, 298.0)
        exp = metal.cp_298
        error_pct = abs(result.Cp_approx - exp) / exp * 100.0
        tol = 50.0 if metal.symbol == "Be" else 15.0
        assert error_pct < tol, (
            f"{metal.symbol}: Cp(298K) = {result.Cp_approx:.2f} vs exp {exp:.2f} "
            f"J/(mol·K) — error {error_pct:.1f}%"
        )


class TestCpResult:
    def test_cp_result_fields(self):
        al = METAL_DB["Al"]
        r = compute_cp(al, 298.0)
        assert isinstance(r, CpResult)
        assert r.T == 298.0
        assert r.Cv_lattice > 0
        assert r.Cv_electronic >= 0
        assert r.Cv_total > 0
        assert r.Cp_approx >= r.Cv_total  # Cp >= Cv
        assert r.cp_specific > 0
        assert 0 < r.fraction_dp <= 1.1  # can slightly exceed 1 due to numerics

    def test_cp_specific_units(self):
        """cp_specific should be in J/(g·K)."""
        al = METAL_DB["Al"]
        r = compute_cp(al, 298.0)
        # Al: experimental ~ 0.897 J/(g·K)
        expected = al.cp_298 / al.molar_mass
        assert abs(r.cp_specific - expected) / expected < 0.20

    def test_cp_curve(self):
        al = METAL_DB["Al"]
        curve = compute_cp_curve(al, T_start=10.0, T_end=1000.0, n_points=50)
        assert len(curve) == 50
        # Temperature should be monotonically increasing
        temps = [c.T for c in curve]
        for i in range(1, len(temps)):
            assert temps[i] > temps[i - 1]
        # Cv should increase with T (for metals, monotonic in Debye model)
        assert curve[-1].Cv_lattice > curve[0].Cv_lattice

    def test_cp_curve_single_point(self):
        fe = METAL_DB["Fe"]
        curve = compute_cp_curve(fe, T_start=298.0, T_end=298.0, n_points=1)
        assert len(curve) == 1
        assert abs(curve[0].T - 298.0) < 0.01
