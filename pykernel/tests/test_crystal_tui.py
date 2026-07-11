"""Tests for crystal_tui — wind particle + TUI renderer.

VSEPR-SIM 3.0.0
"""

import math
import re
import pytest
from pykernel.crystal_tui import (
    Colour,
    Vec3,
    WindParticle,
    WindState,
    LatticeInfo,
    TUISnapshot,
    TUIConfig,
    CrystalTUI,
    FrameBuffer,
    force_colour,
    colour_for_Z,
)


# ═══════════════════════════════════════════════════════════════════════
# Helpers
# ═══════════════════════════════════════════════════════════════════════

def _strip_ansi(s: str) -> str:
    """Remove all ANSI escape sequences for plain-text assertion."""
    return re.sub(r'\033\[[^m]*m', '', s)


def _make_positions(n: int, spacing: float = 2.0) -> list[Vec3]:
    side = math.ceil(n ** (1 / 3))
    pos = []
    for ix in range(side):
        for iy in range(side):
            for iz in range(side):
                if len(pos) >= n:
                    break
                pos.append(Vec3(ix * spacing, iy * spacing, iz * spacing))
    return pos[:n]


def _make_snapshot(n: int = 8, wind: WindParticle | None = None) -> TUISnapshot:
    positions = _make_positions(n)
    types = [6] * n
    forces = [Vec3(0, 0, 0) for _ in range(n)]

    ws = None
    if wind:
        wind.apply(positions, forces)
        ws = wind.to_state()

    fmags = [f.norm() for f in forces]
    frms = math.sqrt(sum(m ** 2 for m in fmags) / max(n, 1))
    fmax = max(fmags) if fmags else 0.0

    return TUISnapshot(
        positions=positions,
        types=types,
        forces=forces,
        Frms=frms,
        Fmax=fmax,
        U_total=-100.0,
        U_vdw=-80.0,
        U_coul=-20.0,
        KE=5.0,
        T=298.0,
        step=42,
        dt=0.001,
        dt_eff=wind.effective_dt(0.001) if wind else 0.001,
        N_atoms=n,
        lattice=LatticeInfo(a=6.0, b=6.0, c=6.0, volume=216.0),
        wind=ws,
    )


# ═══════════════════════════════════════════════════════════════════════
# Colour tests
# ═══════════════════════════════════════════════════════════════════════

class TestColour:
    def test_fg_ansi(self):
        c = Colour(255, 0, 128)
        assert c.fg() == "\033[38;2;255;0;128m"

    def test_lerp_zero(self):
        a = Colour(0, 0, 0)
        b = Colour(100, 200, 50)
        assert Colour.lerp(a, b, 0.0) == a

    def test_lerp_one(self):
        a = Colour(0, 0, 0)
        b = Colour(100, 200, 50)
        assert Colour.lerp(a, b, 1.0) == b

    def test_lerp_clamp(self):
        a = Colour(0, 0, 0)
        b = Colour(100, 200, 50)
        assert Colour.lerp(a, b, 2.0) == b
        assert Colour.lerp(a, b, -1.0) == a


class TestColourHelpers:
    def test_colour_for_Z_hydrogen(self):
        c = colour_for_Z(1)
        assert c.r == 255 and c.g == 255 and c.b == 255

    def test_colour_for_Z_carbon(self):
        c = colour_for_Z(6)
        assert c.r == 100  # cyan

    def test_force_colour_cold(self):
        c = force_colour(0.0, 1.0)
        assert c.r < 100  # blue-ish

    def test_force_colour_hot(self):
        c = force_colour(1.0, 1.0)
        assert c.r > 200  # red-ish


# ═══════════════════════════════════════════════════════════════════════
# Vec3 tests
# ═══════════════════════════════════════════════════════════════════════

class TestVec3:
    def test_norm_zero(self):
        assert Vec3(0, 0, 0).norm() == 0.0

    def test_norm_unit(self):
        assert abs(Vec3(1, 0, 0).norm() - 1.0) < 1e-15

    def test_norm_diagonal(self):
        assert abs(Vec3(1, 1, 1).norm() - math.sqrt(3)) < 1e-12


# ═══════════════════════════════════════════════════════════════════════
# WindParticle tests
# ═══════════════════════════════════════════════════════════════════════

class TestWindParticle:
    def test_additive_force(self):
        wp = WindParticle(direction=Vec3(0, 1, 0), strength=0.3, ramp_steps=0)
        positions = [Vec3(0, 0, 0), Vec3(1, 0, 0)]
        forces = [Vec3(1, 0, 0), Vec3(1, 0, 0)]  # pre-existing x forces
        wp.apply(positions, forces)
        for f in forces:
            assert abs(f.x - 1.0) < 1e-10  # original preserved
            assert abs(f.y - 0.3) < 1e-10  # wind added

    def test_force_clamp(self):
        wp = WindParticle(direction=Vec3(1, 0, 0), strength=10.0, F_max=2.0, ramp_steps=0)
        positions = [Vec3(0, 0, 0)]
        forces = [Vec3(0, 0, 0)]
        wp.apply(positions, forces)
        assert forces[0].norm() <= 2.0 + 1e-10

    def test_ramp_schedule(self):
        wp = WindParticle(strength=1.0, ramp_steps=100)
        assert wp.ramp_fraction() < 1e-10
        positions = [Vec3(0, 0, 0)]
        for _ in range(50):
            forces = [Vec3(0, 0, 0)]
            wp.apply(positions, forces)
        assert abs(wp.ramp_fraction() - 0.5) < 0.02
        for _ in range(50):
            forces = [Vec3(0, 0, 0)]
            wp.apply(positions, forces)
        assert abs(wp.ramp_fraction() - 1.0) < 0.01

    def test_effective_dt(self):
        wp = WindParticle(dt_factor=1.5)
        assert abs(wp.effective_dt(1.0) - 1.5) < 1e-10
        wp2 = WindParticle(dt_factor=2.0)
        assert abs(wp2.effective_dt(0.001) - 0.002) < 1e-14

    def test_gaussian_taper(self):
        wp = WindParticle(
            direction=Vec3(0, 1, 0),
            strength=1.0, F_max=5.0, sigma=5.0,
            origin=Vec3(0, 0, 0), ramp_steps=0,
        )
        positions = [Vec3(0, 0, 0), Vec3(100, 0, 0)]
        forces = [Vec3(0, 0, 0), Vec3(0, 0, 0)]
        wp.apply(positions, forces)
        assert forces[0].norm() > 0.9
        assert forces[1].norm() < 1e-6

    def test_headroom_ratio(self):
        wp = WindParticle(strength=1.0, F_max=3.0)
        assert abs(wp.headroom_ratio() - 3.0) < 1e-10

    def test_reset(self):
        wp = WindParticle(ramp_steps=100)
        positions = [Vec3(0, 0, 0)]
        for _ in range(50):
            wp.apply(positions, [Vec3(0, 0, 0)])
        assert wp.ramp_fraction() > 0.4
        wp.reset()
        assert wp.ramp_fraction() < 1e-10

    def test_to_state(self):
        wp = WindParticle(strength=0.8, dt_factor=1.5, ramp_steps=0)
        ws = wp.to_state()
        assert isinstance(ws, WindState)
        assert abs(ws.strength - 0.8) < 1e-10
        assert abs(ws.dt_factor - 1.5) < 1e-10


# ═══════════════════════════════════════════════════════════════════════
# FrameBuffer tests
# ═══════════════════════════════════════════════════════════════════════

class TestFrameBuffer:
    def test_dimensions(self):
        fb = FrameBuffer(80, 24)
        assert fb.W == 80
        assert fb.H == 24
        assert len(fb.cells) == 80 * 24

    def test_put_and_read(self):
        fb = FrameBuffer(10, 10)
        fb.put(5, 5, "X", Colour(255, 0, 0))
        assert fb.at(5, 5).ch == "X"
        assert fb.at(5, 5).fg.r == 255

    def test_out_of_bounds(self):
        fb = FrameBuffer(10, 10)
        fb.put(-1, -1, "X", Colour())  # should not crash
        assert fb.at(-1, -1).ch == " "  # returns default

    def test_put_string(self):
        fb = FrameBuffer(20, 5)
        fb.put_string(0, 0, "Hello", Colour(0, 255, 0))
        assert fb.at(0, 0).ch == "H"
        assert fb.at(4, 0).ch == "o"

    def test_box(self):
        fb = FrameBuffer(20, 10)
        fb.box(0, 0, 10, 5, Colour(100, 100, 100))
        assert fb.at(0, 0).ch == "+"
        assert fb.at(9, 0).ch == "+"
        assert fb.at(0, 4).ch == "+"
        assert fb.at(9, 4).ch == "+"
        assert fb.at(5, 0).ch == "-"
        assert fb.at(0, 2).ch == "|"

    def test_render_has_ansi(self):
        fb = FrameBuffer(10, 3)
        fb.put(0, 0, "A", Colour(255, 0, 0))
        out = fb.render()
        assert "\033[" in out
        assert "A" in out

    def test_clear(self):
        fb = FrameBuffer(10, 5)
        fb.put(5, 2, "X", Colour(255, 0, 0))
        fb.clear()
        assert fb.at(5, 2).ch == " "


# ═══════════════════════════════════════════════════════════════════════
# CrystalTUI tests
# ═══════════════════════════════════════════════════════════════════════

class TestCrystalTUI:
    def test_render_non_empty(self):
        snap = _make_snapshot()
        tui = CrystalTUI()
        frame = tui.render(snap)
        assert len(frame) > 0
        assert "\033[" in frame

    def test_render_contains_math(self):
        snap = _make_snapshot()
        tui = CrystalTUI()
        frame = tui.render(snap)
        plain = _strip_ansi(frame)
        assert "MATHEMATICS" in plain
        assert "Energy" in plain
        assert "Temperature" in plain

    def test_render_contains_lattice(self):
        snap = _make_snapshot()
        tui = CrystalTUI()
        frame = tui.render(snap)
        plain = _strip_ansi(frame)
        assert "Lattice" in plain
        assert "216.0" in plain  # volume

    def test_render_with_wind(self):
        wind = WindParticle(strength=0.5, dt_factor=1.5, ramp_steps=0)
        snap = _make_snapshot(wind=wind)
        tui = CrystalTUI()
        frame = tui.render(snap)
        plain = _strip_ansi(frame)
        assert "Wind Particle" in plain
        assert "headroom" in plain
        assert "dt_factor" in plain

    def test_render_without_wind(self):
        snap = _make_snapshot()
        tui = CrystalTUI()
        frame = tui.render(snap)
        plain = _strip_ansi(frame)
        assert "Wind Particle" not in plain

    def test_xz_slice(self):
        config = TUIConfig(xz_slice=True)
        snap = _make_snapshot()
        tui = CrystalTUI(config)
        frame = tui.render(snap)
        plain = _strip_ansi(frame)
        assert "c ^" in plain  # xz slice axis label

    def test_xy_slice(self):
        config = TUIConfig(xz_slice=False)
        snap = _make_snapshot()
        tui = CrystalTUI(config)
        frame = tui.render(snap)
        plain = _strip_ansi(frame)
        assert "b ^" in plain  # xy slice axis label

    def test_step_in_title(self):
        snap = _make_snapshot()
        snap.step = 999
        tui = CrystalTUI()
        frame = tui.render(snap)
        plain = _strip_ansi(frame)
        assert "step 999" in plain

    def test_custom_config(self):
        config = TUIConfig(width=80, height=25, lattice_w=40, lattice_h=18, panel_x=45, panel_w=33)
        snap = _make_snapshot(4)
        tui = CrystalTUI(config)
        frame = tui.render(snap)
        assert len(frame) > 0

    def test_forces_visible_when_enabled(self):
        """Forces should create arrow characters when forces are present."""
        snap = _make_snapshot()
        # Add strong force
        snap.forces = [Vec3(5.0, 0.0, 0.0)] * len(snap.positions)
        snap.Fmax = 5.0
        snap.Frms = 5.0
        tui = CrystalTUI()
        frame = tui.render(snap)
        plain = _strip_ansi(frame)
        # Should contain some arrow character
        assert ">" in plain or "<" in plain or "^" in plain or "v" in plain

    def test_nacl_like(self):
        """Render a NaCl-like system."""
        positions = _make_positions(27, 2.82)
        types = [11 if i % 2 == 0 else 17 for i in range(27)]
        wind = WindParticle(direction=Vec3(1, 1, 0), strength=0.8, F_max=2.0, dt_factor=1.5, ramp_steps=0)
        forces = [Vec3(0, 0, 0) for _ in range(27)]
        wind.apply(positions, forces)

        fmags = [f.norm() for f in forces]
        frms = math.sqrt(sum(m ** 2 for m in fmags) / 27)
        fmax = max(fmags)

        snap = TUISnapshot(
            positions=positions, types=types, forces=forces,
            Frms=frms, Fmax=fmax,
            U_total=-500.0, U_vdw=-400.0, U_coul=-100.0,
            T=300.0, step=100, dt=0.001,
            dt_eff=wind.effective_dt(0.001),
            N_atoms=27,
            lattice=LatticeInfo(a=8.46, b=8.46, c=8.46, alpha_deg=90.0, beta_deg=90.0, gamma_deg=90.0, volume=605.6),
            wind=wind.to_state(),
        )
        tui = CrystalTUI()
        frame = tui.render(snap)
        plain = _strip_ansi(frame)
        assert "Wind Particle" in plain
        assert "605.6" in plain
