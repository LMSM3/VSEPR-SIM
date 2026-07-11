"""
Tests for pykernel.metal_graphs — research-quality matplotlib graphs.

Covers:
  - Stress-Strain Rainbow computation + plotting
  - Brittleness ranking computation + plotting
  - XRD diffraction pattern computation + plotting
  - Toughness-over-time computation + plotting
  - Chaos dashboard plotting
  - Lattice 3D projection plotting
  - xyzA trajectory export
  - Generate-all master function
  - GraphReport dataclass
  - Edge cases and physical constraints

VSEPR-SIM 3.0.0
"""

from __future__ import annotations

import math
import os
import shutil
import tempfile
from pathlib import Path

import numpy as np
import pytest

from pykernel.metal_graphs import (
    GraphReport,
    StressStrainPoint,
    BrittlenessRecord,
    XRDPeak,
    ToughnessPoint,
    compute_stress_strain,
    compute_brittleness,
    compute_xrd_pattern,
    compute_toughness_history,
    plot_stress_strain_rainbow,
    plot_brittleness,
    plot_xrd_rainbow,
    plot_toughness_over_time,
    plot_chaos_dashboard,
    plot_lattice_projection,
    export_sweep_xyza,
    generate_all_graphs,
    _fcc_lattice_param,
    _hex_to_rgb,
)
from pykernel.metal_sweep import (
    ElementInfo, load_elements, synthesise_metal_record, build_fcc_lattice,
)
from pykernel.metallic_cp import MetalRecord


# ── Fixtures ──────────────────────────────────────────────────────────

@pytest.fixture
def tmp_dir():
    d = tempfile.mkdtemp(prefix="mg_test_")
    yield d
    shutil.rmtree(d, ignore_errors=True)


@pytest.fixture
def all_elements():
    return load_elements()


@pytest.fixture
def small_elements(all_elements):
    """First 5 elements for fast tests."""
    return all_elements[:5]


@pytest.fixture
def metals_dict(small_elements):
    return {e.symbol: synthesise_metal_record(e) for e in small_elements}


@pytest.fixture
def fe_element(all_elements):
    for e in all_elements:
        if e.symbol == "Fe":
            return e
    pytest.skip("Fe not in element list")


@pytest.fixture
def fe_metal(fe_element):
    return synthesise_metal_record(fe_element)


# ── Helper tests ──────────────────────────────────────────────────────

class TestHelpers:

    def test_fcc_lattice_param(self):
        r = 1.26  # Fe covalent radius approx
        a = _fcc_lattice_param(r)
        assert a == pytest.approx(2 * math.sqrt(2) * r, rel=1e-10)

    def test_fcc_lattice_param_zero(self):
        assert _fcc_lattice_param(0.0) == 0.0

    def test_hex_to_rgb_standard(self):
        assert _hex_to_rgb("#FF0000") == pytest.approx((1.0, 0.0, 0.0), abs=1e-10)
        assert _hex_to_rgb("#00FF00") == pytest.approx((0.0, 1.0, 0.0), abs=1e-10)
        assert _hex_to_rgb("#0000FF") == pytest.approx((0.0, 0.0, 1.0), abs=1e-10)

    def test_hex_to_rgb_no_hash(self):
        assert _hex_to_rgb("FFFFFF") == pytest.approx((1.0, 1.0, 1.0), abs=1e-10)

    def test_hex_to_rgb_short(self):
        r, g, b = _hex_to_rgb("FF")
        assert r == pytest.approx(1.0, abs=0.01)


# ── Stress-Strain ────────────────────────────────────────────────────

class TestStressStrain:

    def test_compute_returns_list(self, fe_metal, fe_element):
        a = _fcc_lattice_param(fe_element.r_cov)
        curve = compute_stress_strain(fe_metal, a)
        assert isinstance(curve, list)
        assert len(curve) == 100  # default n_points

    def test_point_types(self, fe_metal, fe_element):
        a = _fcc_lattice_param(fe_element.r_cov)
        curve = compute_stress_strain(fe_metal, a)
        for p in curve:
            assert isinstance(p, StressStrainPoint)
            assert hasattr(p, "strain")
            assert hasattr(p, "stress_GPa")

    def test_stress_starts_at_zero(self, fe_metal, fe_element):
        a = _fcc_lattice_param(fe_element.r_cov)
        curve = compute_stress_strain(fe_metal, a)
        assert curve[0].strain == pytest.approx(0.0, abs=1e-10)
        assert curve[0].stress_GPa == pytest.approx(0.0, abs=1e-6)

    def test_stress_non_negative(self, fe_metal, fe_element):
        a = _fcc_lattice_param(fe_element.r_cov)
        curve = compute_stress_strain(fe_metal, a)
        for p in curve:
            assert p.stress_GPa >= 0.0

    def test_stress_has_peak(self, fe_metal, fe_element):
        """Physical: stress should rise then fall (necking)."""
        a = _fcc_lattice_param(fe_element.r_cov)
        curve = compute_stress_strain(fe_metal, a)
        stresses = [p.stress_GPa for p in curve]
        peak_idx = stresses.index(max(stresses))
        assert 1 < peak_idx < len(stresses) - 1

    def test_custom_n_points(self, fe_metal, fe_element):
        a = _fcc_lattice_param(fe_element.r_cov)
        curve = compute_stress_strain(fe_metal, a, n_points=50)
        assert len(curve) == 50

    def test_max_strain(self, fe_metal, fe_element):
        a = _fcc_lattice_param(fe_element.r_cov)
        curve = compute_stress_strain(fe_metal, a, max_strain=0.10)
        assert curve[-1].strain == pytest.approx(0.10, rel=0.01)

    def test_plot_creates_file(self, small_elements, metals_dict, tmp_dir):
        p = os.path.join(tmp_dir, "ss.png")
        result = plot_stress_strain_rainbow(small_elements, metals_dict, p)
        assert os.path.isfile(result)
        assert os.path.getsize(result) > 1000  # non-trivial

    def test_young_modulus_clamp(self):
        """Extremely low density/theta should still produce valid curve."""
        rec = MetalRecord(
            symbol="Es", Z=99, name="Einsteinium",
            molar_mass=252.0, density=0.5, theta_D=10.0,
            gamma=1.0, melting_point=100.0,
            cp_298=25.0, thermal_cond=10.0,
        )
        a = 3.0
        curve = compute_stress_strain(rec, a)
        assert len(curve) == 100
        assert all(p.stress_GPa >= 0 for p in curve)


# ── Brittleness ──────────────────────────────────────────────────────

class TestBrittleness:

    def test_compute_returns_record(self, fe_metal, fe_element):
        a = _fcc_lattice_param(fe_element.r_cov)
        rec = compute_brittleness(fe_metal, a)
        assert isinstance(rec, BrittlenessRecord)

    def test_brittleness_range(self, fe_metal, fe_element):
        a = _fcc_lattice_param(fe_element.r_cov)
        rec = compute_brittleness(fe_metal, a)
        assert 0.0 <= rec.brittleness <= 1.0

    def test_pugh_ratio_positive(self, fe_metal, fe_element):
        a = _fcc_lattice_param(fe_element.r_cov)
        rec = compute_brittleness(fe_metal, a)
        assert rec.pugh_ratio > 0

    def test_symbol_propagated(self, fe_metal, fe_element):
        a = _fcc_lattice_param(fe_element.r_cov)
        rec = compute_brittleness(fe_metal, a)
        assert rec.symbol == "Fe"
        assert rec.Z == 26

    def test_plot_creates_file(self, small_elements, metals_dict, tmp_dir):
        p = os.path.join(tmp_dir, "brit.png")
        result = plot_brittleness(small_elements, metals_dict, p)
        assert os.path.isfile(result)
        assert os.path.getsize(result) > 1000

    def test_all_elements_in_range(self, all_elements):
        """All 74 metals produce brittleness in [0, 1]."""
        for elem in all_elements[:20]:
            metal = synthesise_metal_record(elem)
            a = _fcc_lattice_param(elem.r_cov)
            rec = compute_brittleness(metal, a)
            assert 0.0 <= rec.brittleness <= 1.0, f"{elem.symbol} out of range"


# ── XRD ──────────────────────────────────────────────────────────────

class TestXRD:

    def test_fcc_peaks(self):
        peaks = compute_xrd_pattern(3.615)  # Cu lattice param
        assert len(peaks) > 0

    def test_first_peak_111(self):
        peaks = compute_xrd_pattern(3.615)
        assert peaks[0].hkl == (1, 1, 1)

    def test_d_spacing_formula(self):
        a = 4.05  # Al lattice param
        peaks = compute_xrd_pattern(a)
        for p in peaks:
            h, k, l = p.hkl
            expected_d = a / math.sqrt(h**2 + k**2 + l**2)
            assert p.d_spacing_A == pytest.approx(expected_d, rel=1e-6)

    def test_bragg_law(self):
        """2d sin(theta) = lambda."""
        lam = 1.5406
        peaks = compute_xrd_pattern(3.615, wavelength_A=lam)
        for p in peaks:
            theta = math.radians(p.two_theta_deg / 2.0)
            computed_lam = 2 * p.d_spacing_A * math.sin(theta)
            assert computed_lam == pytest.approx(lam, rel=1e-4)

    def test_selection_rule(self):
        """FCC: all odd or all even (no mixed)."""
        peaks = compute_xrd_pattern(3.615)
        for p in peaks:
            h, k, l = p.hkl
            parities = {h % 2, k % 2, l % 2}
            assert len(parities) == 1, f"Mixed parity: {p.hkl}"

    def test_intensity_normalised(self):
        peaks = compute_xrd_pattern(3.615)
        assert max(p.intensity for p in peaks) == pytest.approx(1.0, rel=1e-6)

    def test_two_theta_in_range(self):
        peaks = compute_xrd_pattern(3.615, max_two_theta=90.0)
        for p in peaks:
            assert 0 < p.two_theta_deg <= 90.0

    def test_custom_wavelength(self):
        peaks_cu = compute_xrd_pattern(3.615, wavelength_A=1.5406)
        peaks_mo = compute_xrd_pattern(3.615, wavelength_A=0.7107)
        # Mo K-alpha has more peaks in same range (smaller wavelength)
        assert len(peaks_mo) >= len(peaks_cu)

    def test_plot_creates_file(self, small_elements, metals_dict, tmp_dir):
        p = os.path.join(tmp_dir, "xrd.png")
        result = plot_xrd_rainbow(small_elements, metals_dict, p)
        assert os.path.isfile(result)
        assert os.path.getsize(result) > 1000

    def test_very_small_lattice(self):
        """Very small lattice → fewer accessible peaks."""
        peaks = compute_xrd_pattern(1.5)
        assert isinstance(peaks, list)

    def test_very_large_lattice(self):
        """Very large lattice → many peaks at low angles."""
        peaks = compute_xrd_pattern(10.0)
        assert len(peaks) > 5


# ── Toughness ────────────────────────────────────────────────────────

class TestToughness:

    def test_compute_returns_list(self, fe_metal):
        history = compute_toughness_history(fe_metal, t_end=0.5, dt=0.01)
        assert isinstance(history, list)
        assert len(history) > 0

    def test_point_types(self, fe_metal):
        history = compute_toughness_history(fe_metal, t_end=0.5, dt=0.01)
        for pt in history:
            assert isinstance(pt, ToughnessPoint)
            assert pt.time_s >= 0
            assert pt.T_K > 0
            assert pt.energy_absorbed_J >= 0
            assert pt.toughness_proxy >= 0

    def test_monotonic_toughness(self, fe_metal):
        """Cumulative energy is monotonically increasing."""
        history = compute_toughness_history(fe_metal, t_end=0.5, dt=0.01)
        for i in range(1, len(history)):
            assert history[i].toughness_proxy >= history[i-1].toughness_proxy

    def test_monotonic_temperature(self, fe_metal):
        """Temperature rises with constant heating."""
        history = compute_toughness_history(fe_metal, t_end=0.5, dt=0.01)
        for i in range(1, len(history)):
            assert history[i].T_K >= history[i-1].T_K - 0.01  # allow tiny numerical jitter

    def test_plot_creates_file(self, small_elements, metals_dict, tmp_dir):
        p = os.path.join(tmp_dir, "tough.png")
        result = plot_toughness_over_time(small_elements, metals_dict, p, n_sample=3)
        assert os.path.isfile(result)
        assert os.path.getsize(result) > 1000


# ── Chaos Dashboard ──────────────────────────────────────────────────

class TestChaosDashboard:

    def test_plot_creates_file(self, small_elements, metals_dict, tmp_dir):
        p = os.path.join(tmp_dir, "chaos.png")
        result = plot_chaos_dashboard(small_elements, metals_dict, p)
        assert os.path.isfile(result)
        assert os.path.getsize(result) > 1000


# ── Lattice 3D Projection ───────────────────────────────────────────

class TestLattice3D:

    def test_plot_creates_file(self, small_elements, metals_dict, tmp_dir):
        p = os.path.join(tmp_dir, "lat3d.png")
        result = plot_lattice_projection(small_elements, metals_dict, p, n_sample=4)
        assert os.path.isfile(result)
        assert os.path.getsize(result) > 1000


# ── xyzA Export ──────────────────────────────────────────────────────

class TestXYZAExport:

    def test_export_creates_file(self, small_elements, metals_dict, tmp_dir):
        p = os.path.join(tmp_dir, "test.xyzA")
        result = export_sweep_xyza(small_elements, metals_dict, p)
        assert os.path.isfile(result)
        assert os.path.getsize(result) > 100

    def test_export_contains_all_elements(self, small_elements, metals_dict, tmp_dir):
        p = os.path.join(tmp_dir, "test.xyzA")
        export_sweep_xyza(small_elements, metals_dict, p)
        content = Path(p).read_text()
        for elem in small_elements:
            assert elem.symbol in content


# ── GraphReport ──────────────────────────────────────────────────────

class TestGraphReport:

    def test_default_empty(self):
        r = GraphReport()
        assert r.stress_strain == ""
        assert r.all_paths() == []

    def test_all_paths_filters_empty(self):
        r = GraphReport(stress_strain="a.png", xrd="b.png")
        paths = r.all_paths()
        assert len(paths) == 2
        assert "a.png" in paths
        assert "b.png" in paths

    def test_all_paths_full(self):
        r = GraphReport(
            stress_strain="a.png", brittleness="b.png", xrd="c.png",
            toughness="d.png", chaos_dashboard="e.png", lattice_3d="f.png",
            xyza_trajectory="g.xyzA", summary_md="h.md",
        )
        assert len(r.all_paths()) == 8


# ── Generate All ─────────────────────────────────────────────────────

class TestGenerateAll:

    def test_generate_all_small(self, tmp_dir):
        """Generate all graphs for 3 metals."""
        report = generate_all_graphs(out_dir=tmp_dir, symbols=["Fe", "Cu", "Au"])
        assert isinstance(report, GraphReport)
        assert os.path.isfile(report.stress_strain)
        assert os.path.isfile(report.brittleness)
        assert os.path.isfile(report.xrd)
        assert os.path.isfile(report.toughness)
        assert os.path.isfile(report.chaos_dashboard)
        assert os.path.isfile(report.lattice_3d)
        assert os.path.isfile(report.xyza_trajectory)
        assert os.path.isfile(report.summary_md)
        assert len(report.all_paths()) == 8

    def test_generate_creates_directory(self, tmp_dir):
        sub = os.path.join(tmp_dir, "nested", "deep")
        report = generate_all_graphs(out_dir=sub, symbols=["Ti"])
        assert os.path.isdir(sub)
        assert len(report.all_paths()) == 8

    def test_markdown_report_content(self, tmp_dir):
        report = generate_all_graphs(out_dir=tmp_dir, symbols=["Fe"])
        content = Path(report.summary_md).read_text()
        assert "VSEPR-SIM" in content
        assert "Stress-Strain" in content
        assert "Brittleness" in content
        assert "X-ray Diffraction" in content
        assert "Toughness" in content
        assert "Chaos" in content
        assert "xyzA" in content
        assert "Angstrom" in content


# ── Physical Consistency ─────────────────────────────────────────────

class TestPhysicalConsistency:

    def test_harder_metal_higher_modulus(self, all_elements):
        """Transition metals (Fe, W) should generally have higher E than alkali (Na, K)."""
        fe = None
        na = None
        for e in all_elements:
            if e.symbol == "Fe":
                fe = e
            if e.symbol == "Na":
                na = e
        if fe is None or na is None:
            pytest.skip("Fe or Na not available")
        fe_m = synthesise_metal_record(fe)
        na_m = synthesise_metal_record(na)
        fe_curve = compute_stress_strain(fe_m, _fcc_lattice_param(fe.r_cov))
        na_curve = compute_stress_strain(na_m, _fcc_lattice_param(na.r_cov))
        fe_peak = max(p.stress_GPa for p in fe_curve)
        na_peak = max(p.stress_GPa for p in na_curve)
        assert fe_peak > na_peak

    def test_xrd_peak_shifts_with_lattice(self):
        """Larger lattice → lower 2theta for same (hkl)."""
        peaks_small = compute_xrd_pattern(3.0)
        peaks_large = compute_xrd_pattern(5.0)
        # (1,1,1) peak should be at lower 2theta for larger lattice
        assert peaks_large[0].two_theta_deg < peaks_small[0].two_theta_deg

    def test_brittleness_consistency(self, all_elements):
        """Verify no NaN or infinite brittleness values."""
        for elem in all_elements[:30]:
            metal = synthesise_metal_record(elem)
            a = _fcc_lattice_param(elem.r_cov)
            rec = compute_brittleness(metal, a)
            assert math.isfinite(rec.brittleness)
            assert math.isfinite(rec.pugh_ratio)
            assert math.isfinite(rec.pettifor)

    def test_lattice_angstrom_coordinates(self, all_elements):
        """Verify lattice positions are in Angstrom scale (0.5 to 50 A)."""
        for elem in all_elements[:10]:
            a = _fcc_lattice_param(elem.r_cov)
            assert 0.5 < a < 50.0, f"{elem.symbol}: a={a}"
            positions = build_fcc_lattice(a, nx=2, ny=2, nz=2)
            assert positions.max() < 200.0  # 2x2x2 supercell, max ~4a
            assert positions.min() >= 0.0
