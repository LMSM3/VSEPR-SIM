"""
Tests for metal_sweep — Z=5..101 metal sweep with 3D TUI rendering.

VSEPR-SIM 3.0.0
"""

import math
import pytest
import numpy as np

from pykernel.metal_sweep import (
    ElementInfo,
    SweepFrame,
    load_elements,
    synthesise_metal_record,
    build_fcc_lattice,
    render_metal,
    hex_to_colour,
    MetalSweepRunner,
    _is_metal_by_Z,
    _estimate_theta_D,
    _estimate_gamma,
)
from pykernel.metallic_cp import METAL_DB, MetalRecord, compute_cp
from pykernel.crystal_tui import TUIConfig, Colour


# ═══════════════════════════════════════════════════════════════════════
# Element loading
# ═══════════════════════════════════════════════════════════════════════


class TestLoadElements:
    def test_returns_list(self):
        elems = load_elements()
        assert isinstance(elems, list)
        assert len(elems) > 0

    def test_all_metals_in_range(self):
        elems = load_elements()
        for e in elems:
            assert 5 <= e.Z <= 101

    def test_common_metals_present(self):
        elems = load_elements()
        symbols = {e.symbol for e in elems}
        for s in ["Fe", "Cu", "Al", "Au", "Ag", "Ti", "Na", "K"]:
            assert s in symbols, f"{s} missing from metal list"

    def test_element_info_fields(self):
        elems = load_elements()
        e = elems[0]
        assert isinstance(e, ElementInfo)
        assert isinstance(e.Z, int)
        assert isinstance(e.symbol, str)
        assert isinstance(e.name, str)
        assert e.weight > 0
        assert e.r_cov > 0
        assert e.r_vdw > 0
        assert e.cpk_hex.startswith("#")

    def test_no_duplicates(self):
        elems = load_elements()
        zs = [e.Z for e in elems]
        assert len(zs) == len(set(zs))


class TestIsMetalByZ:
    def test_iron(self):
        assert _is_metal_by_Z(26) is True

    def test_sodium(self):
        assert _is_metal_by_Z(11) is True

    def test_hydrogen(self):
        assert _is_metal_by_Z(1) is False

    def test_carbon(self):
        assert _is_metal_by_Z(6) is False


# ═══════════════════════════════════════════════════════════════════════
# Metal record synthesis
# ═══════════════════════════════════════════════════════════════════════


class TestSynthesiseMetalRecord:
    def test_known_metal_returns_db_record(self):
        elem = ElementInfo(Z=26, symbol="Fe", name="Iron",
                           weight=55.845, r_cov=1.32, r_vdw=2.0,
                           cpk_hex="#E06633", category="Transition metal")
        rec = synthesise_metal_record(elem)
        assert rec is METAL_DB["Fe"]

    def test_unknown_metal_synthesises(self):
        elem = ElementInfo(Z=58, symbol="Ce", name="Cerium",
                           weight=140.12, r_cov=1.81, r_vdw=2.0,
                           cpk_hex="#FFFFC7", category="Lanthanide")
        rec = synthesise_metal_record(elem)
        assert isinstance(rec, MetalRecord)
        assert rec.symbol == "Ce"
        assert rec.Z == 58
        assert rec.theta_D > 0
        assert rec.gamma > 0

    def test_theta_D_estimate(self):
        t = _estimate_theta_D(28.0)
        assert abs(t - 300.0) < 1e-6

    def test_gamma_estimate_transition(self):
        g = _estimate_gamma(26)  # Fe
        assert g == 4.0

    def test_gamma_estimate_lanthanide(self):
        g = _estimate_gamma(60)  # Nd
        assert g == 8.0


# ═══════════════════════════════════════════════════════════════════════
# FCC lattice builder
# ═══════════════════════════════════════════════════════════════════════


class TestBuildFccLattice:
    def test_shape(self):
        pos = build_fcc_lattice(3.6, 2, 2, 2)
        assert pos.shape == (32, 3)

    def test_single_cell(self):
        pos = build_fcc_lattice(4.0, 1, 1, 1)
        assert pos.shape == (4, 3)

    def test_positions_non_negative(self):
        pos = build_fcc_lattice(3.6, 2, 2, 2)
        assert np.all(pos >= -1e-10)

    def test_min_distance(self):
        a = 3.6
        pos = build_fcc_lattice(a, 2, 2, 2)
        # Nearest-neighbour distance in FCC = a / sqrt(2)
        expected_nn = a / math.sqrt(2.0)
        dists = []
        for i in range(len(pos)):
            for j in range(i + 1, len(pos)):
                d = np.linalg.norm(pos[i] - pos[j])
                dists.append(d)
        assert min(dists) == pytest.approx(expected_nn, rel=1e-6)


# ═══════════════════════════════════════════════════════════════════════
# Colour conversion
# ═══════════════════════════════════════════════════════════════════════


class TestHexToColour:
    def test_white(self):
        c = hex_to_colour("#FFFFFF")
        assert c == Colour(255, 255, 255)

    def test_black(self):
        c = hex_to_colour("#000000")
        assert c == Colour(0, 0, 0)

    def test_red(self):
        c = hex_to_colour("#FF0000")
        assert c == Colour(255, 0, 0)

    def test_no_hash(self):
        c = hex_to_colour("C0C0C0")
        assert c == Colour(192, 192, 192)


# ═══════════════════════════════════════════════════════════════════════
# Per-element rendering
# ═══════════════════════════════════════════════════════════════════════


class TestRenderMetal:
    @pytest.fixture
    def iron_elem(self):
        return ElementInfo(Z=26, symbol="Fe", name="Iron",
                           weight=55.845, r_cov=1.32, r_vdw=2.0,
                           cpk_hex="#E06633", category="Transition metal")

    @pytest.fixture
    def iron_metal(self, iron_elem):
        return synthesise_metal_record(iron_elem)

    @pytest.fixture
    def config(self):
        return TUIConfig(width=80, height=36)

    def test_returns_sweep_frame(self, iron_elem, iron_metal, config):
        sf = render_metal(iron_elem, iron_metal, 1, config)
        assert isinstance(sf, SweepFrame)

    def test_frame_fields(self, iron_elem, iron_metal, config):
        sf = render_metal(iron_elem, iron_metal, 1, config)
        assert sf.Z == 26
        assert sf.symbol == "Fe"
        assert sf.name == "Iron"
        assert sf.n_atoms == 32
        assert sf.T_final > 0
        assert sf.cp_final > 0
        assert sf.elapsed_ms > 0
        assert len(sf.tui_frame) > 0

    def test_tui_frame_is_string(self, iron_elem, iron_metal, config):
        sf = render_metal(iron_elem, iron_metal, 1, config)
        assert isinstance(sf.tui_frame, str)

    def test_temperature_increased(self, iron_elem, iron_metal, config):
        sf = render_metal(iron_elem, iron_metal, 1, config)
        assert sf.T_final > 298.0  # Started at 298 K, should heat up

    def test_synthesised_metal_renders(self, config):
        elem = ElementInfo(Z=58, symbol="Ce", name="Cerium",
                           weight=140.12, r_cov=1.81, r_vdw=2.0,
                           cpk_hex="#FFFFC7", category="Lanthanide")
        metal = synthesise_metal_record(elem)
        sf = render_metal(elem, metal, 1, config)
        assert sf.Z == 58
        assert sf.n_atoms == 32
        assert len(sf.tui_frame) > 0


# ═══════════════════════════════════════════════════════════════════════
# Sweep runner
# ═══════════════════════════════════════════════════════════════════════


class TestMetalSweepRunner:
    def test_element_count(self):
        r = MetalSweepRunner(render_delay=0.0)
        assert r.element_count >= 50

    def test_run_once_returns_frames(self, capsys):
        r = MetalSweepRunner(render_delay=0.0)
        frames = r.run_once()
        assert len(frames) == r.element_count
        assert all(isinstance(f, SweepFrame) for f in frames)

    def test_all_metals_render(self, capsys):
        r = MetalSweepRunner(render_delay=0.0)
        frames = r.run_once()
        zs = {f.Z for f in frames}
        # Verify we got unique Z values
        assert len(zs) == len(frames)
        # All should have rendered TUI output
        for f in frames:
            assert len(f.tui_frame) > 100, f"Empty frame for Z={f.Z} {f.symbol}"
