"""
crystal_tui — Python crystal lattice TUI with wind particle overlay.

Pure-Python ANSI terminal renderer that mirrors the C++ CrystalTUI.
Reads xyzA frames (or receives data directly) and draws:
  1. Lattice site projection (XY or XZ slice)
  2. Force arrows (magnitude colour-mapped)
  3. Wind particle field overlay
  4. Live mathematics panel (energy, temperature, forces, convergence)
  5. Lattice metric sidebar

"Looking through the glass to the core of a reaction."

VSEPR-SIM 3.0.0
"""

from __future__ import annotations

import math
import sys
from dataclasses import dataclass, field
from typing import Optional, Sequence


# ═══════════════════════════════════════════════════════════════════════
# Colour helpers
# ═══════════════════════════════════════════════════════════════════════

@dataclass(frozen=True)
class Colour:
    r: int = 180
    g: int = 180
    b: int = 180

    def fg(self) -> str:
        return f"\033[38;2;{self.r};{self.g};{self.b}m"

    @staticmethod
    def lerp(a: "Colour", b: "Colour", t: float) -> "Colour":
        t = max(0.0, min(1.0, t))
        return Colour(
            int(a.r + t * (b.r - a.r)),
            int(a.g + t * (b.g - a.g)),
            int(a.b + t * (b.b - a.b)),
        )


RESET = "\033[0m"
BOLD = "\033[1m"

_BLOCK_COLOURS = {
    "s": Colour(255, 255, 255),
    "p": Colour(100, 200, 255),
    "d": Colour(255, 200, 80),
    "f": Colour(255, 120, 80),
}


def colour_for_Z(Z: int) -> Colour:
    if Z <= 2:
        return _BLOCK_COLOURS["s"]
    if Z <= 10:
        return Colour(100, 200, 255)
    if Z <= 18:
        return Colour(100, 255, 100)
    if Z <= 36:
        return _BLOCK_COLOURS["d"]
    if Z <= 54:
        return Colour(200, 100, 255)
    if Z <= 86:
        return _BLOCK_COLOURS["f"]
    return Colour(180, 180, 180)


def force_colour(mag: float, fmax: float) -> Colour:
    if fmax < 1e-20:
        return Colour(80, 80, 80)
    t = mag / fmax
    return Colour.lerp(Colour(40, 80, 200), Colour(255, 60, 30), t)


# ═══════════════════════════════════════════════════════════════════════
# Data models
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class Vec3:
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0

    def norm(self) -> float:
        return math.sqrt(self.x ** 2 + self.y ** 2 + self.z ** 2)


@dataclass
class WindState:
    direction: Vec3 = field(default_factory=Vec3)
    strength: float = 0.0
    ramp: float = 1.0
    peak_force: float = 0.0
    headroom: float = 1.5
    dt_factor: float = 1.5


@dataclass
class LatticeInfo:
    a: float = 1.0
    b: float = 1.0
    c: float = 1.0
    alpha_deg: float = 90.0
    beta_deg: float = 90.0
    gamma_deg: float = 90.0
    volume: float = 1.0


@dataclass
class TUISnapshot:
    """All data needed for one TUI frame."""
    positions: list[Vec3] = field(default_factory=list)
    types: list[int] = field(default_factory=list)
    forces: list[Vec3] = field(default_factory=list)
    Frms: float = 0.0
    Fmax: float = 0.0

    U_total: float = 0.0
    U_bond: float = 0.0
    U_vdw: float = 0.0
    U_coul: float = 0.0
    U_ext: float = 0.0
    KE: float = 0.0
    T: float = 0.0

    step: int = 0
    dt: float = 0.001
    dt_eff: float = 0.001
    N_atoms: int = 0

    lattice: LatticeInfo = field(default_factory=LatticeInfo)
    wind: Optional[WindState] = None


# ═══════════════════════════════════════════════════════════════════════
# Frame buffer
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class Cell:
    ch: str = " "
    fg: Colour = field(default_factory=Colour)
    bold: bool = False


class FrameBuffer:
    """2D character grid with per-cell colour."""

    def __init__(self, w: int, h: int):
        self.W = w
        self.H = h
        self.cells: list[Cell] = [Cell() for _ in range(w * h)]

    def clear(self) -> None:
        for c in self.cells:
            c.ch = " "
            c.fg = Colour()
            c.bold = False

    def put(self, x: int, y: int, ch: str, fg: Colour, bold: bool = False) -> None:
        if 0 <= x < self.W and 0 <= y < self.H:
            c = self.cells[y * self.W + x]
            c.ch = ch[0] if ch else " "
            c.fg = fg
            c.bold = bold

    def put_string(self, x: int, y: int, s: str, fg: Colour, bold: bool = False) -> None:
        for i, ch in enumerate(s):
            self.put(x + i, y, ch, fg, bold)

    def at(self, x: int, y: int) -> Cell:
        if 0 <= x < self.W and 0 <= y < self.H:
            return self.cells[y * self.W + x]
        return Cell()

    def box(self, x0: int, y0: int, w: int, h: int, fg: Colour) -> None:
        for x in range(x0, x0 + w):
            self.put(x, y0, "-", fg)
            self.put(x, y0 + h - 1, "-", fg)
        for y in range(y0, y0 + h):
            self.put(x0, y, "|", fg)
            self.put(x0 + w - 1, y, "|", fg)
        for cx, cy in [(x0, y0), (x0 + w - 1, y0), (x0, y0 + h - 1), (x0 + w - 1, y0 + h - 1)]:
            self.put(cx, cy, "+", fg)

    def render(self) -> str:
        lines = []
        lines.append("\033[H")  # cursor home
        for y in range(self.H):
            row = []
            for x in range(self.W):
                c = self.cells[y * self.W + x]
                pfx = BOLD if c.bold else ""
                row.append(f"{pfx}{c.fg.fg()}{c.ch}{RESET}")
            lines.append("".join(row))
        return "\n".join(lines)


# ═══════════════════════════════════════════════════════════════════════
# Wind Particle (Python-side model)
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class WindParticle:
    """Virtual directional perturbation field.

    Applies a smooth, clamped force to atom positions.
    dt_factor gives 1.5-2x timestep headroom.
    """
    direction: Vec3 = field(default_factory=lambda: Vec3(1.0, 0.0, 0.0))
    strength: float = 0.5
    F_max: float = 2.0
    origin: Vec3 = field(default_factory=Vec3)
    sigma: float = 0.0
    dt_factor: float = 1.5
    ramp_steps: int = 50
    _step: int = 0

    def apply(
        self, positions: Sequence[Vec3], forces: list[Vec3]
    ) -> float:
        """Add wind force to *forces* (in-place). Returns U_wind contribution."""
        dnorm = self.direction.norm()
        if dnorm < 1e-30:
            return 0.0
        dhat = Vec3(self.direction.x / dnorm, self.direction.y / dnorm, self.direction.z / dnorm)

        ramp = self.ramp_fraction()
        base_F = self.strength * ramp
        taper = self.sigma > 1e-10
        sig2 = self.sigma ** 2

        U_wind = 0.0
        for i, pos in enumerate(positions):
            env = 1.0
            if taper:
                dx = pos.x - self.origin.x
                dy = pos.y - self.origin.y
                dz = pos.z - self.origin.z
                r2 = dx * dx + dy * dy + dz * dz
                env = math.exp(-r2 / (2.0 * sig2))

            Fi = min(base_F * env, self.F_max)
            fx = dhat.x * Fi
            fy = dhat.y * Fi
            fz = dhat.z * Fi
            forces[i] = Vec3(forces[i].x + fx, forces[i].y + fy, forces[i].z + fz)
            U_wind -= fx * pos.x + fy * pos.y + fz * pos.z

        self._step += 1
        return U_wind

    def effective_dt(self, dt: float) -> float:
        return dt * self.dt_factor

    def ramp_fraction(self) -> float:
        if self.ramp_steps <= 0:
            return 1.0
        return min(1.0, self._step / self.ramp_steps)

    def peak_force(self) -> float:
        return min(self.strength * self.ramp_fraction(), self.F_max)

    def headroom_ratio(self) -> float:
        if self.strength < 1e-30:
            return 1e30
        return self.F_max / self.strength

    def reset(self) -> None:
        self._step = 0

    def to_state(self) -> WindState:
        return WindState(
            direction=self.direction,
            strength=self.strength,
            ramp=self.ramp_fraction(),
            peak_force=self.peak_force(),
            headroom=self.headroom_ratio(),
            dt_factor=self.dt_factor,
        )


# ═══════════════════════════════════════════════════════════════════════
# Crystal TUI Renderer
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class TUIConfig:
    width: int = 120
    height: int = 40
    lattice_w: int = 60
    lattice_h: int = 30
    panel_x: int = 65
    panel_w: int = 52
    show_forces: bool = True
    show_wind: bool = True
    xz_slice: bool = False


def _arrow_char(dx: float, dy: float) -> str:
    if abs(dx) < 1e-12 and abs(dy) < 1e-12:
        return "."
    angle = math.degrees(math.atan2(dy, dx)) % 360
    arrows = [">", "/", "^", "\\", "<", "/", "v", "\\"]
    idx = int((angle + 22.5) / 45.0) % 8
    return arrows[idx]


class CrystalTUI:
    """Terminal crystal lattice viewer with wind particle overlay."""

    def __init__(self, config: Optional[TUIConfig] = None):
        self.config = config or TUIConfig()
        self.fb = FrameBuffer(self.config.width, self.config.height)

    def _proj_x(self, cx: float, x_min: float, x_range: float) -> int:
        if x_range < 1e-12:
            return self.config.lattice_w // 2 + 2
        t = (cx - x_min) / x_range
        return 2 + int(t * (self.config.lattice_w - 4))

    def _proj_y(self, cy: float, y_min: float, y_range: float) -> int:
        if y_range < 1e-12:
            return self.config.lattice_h // 2 + 2
        t = (cy - y_min) / y_range
        return 2 + int((1.0 - t) * (self.config.lattice_h - 4))

    def render(self, snap: TUISnapshot) -> str:
        self.fb = FrameBuffer(self.config.width, self.config.height)
        self.fb.clear()

        self._draw_title(snap)
        self._draw_border()
        self._draw_lattice_axes(snap)
        self._draw_atoms(snap)
        self._draw_forces(snap)
        self._draw_wind(snap)
        self._draw_math_panel(snap)

        return self.fb.render()

    def display(self, snap: TUISnapshot) -> None:
        sys.stdout.write(self.render(snap))
        sys.stdout.flush()

    # ── sub-renderers ──

    def _draw_title(self, snap: TUISnapshot) -> None:
        title_c = Colour(80, 220, 255)
        self.fb.put_string(2, 0, " VSEPR-SIM 3.0.0 :: Crystal Lattice TUI ", title_c, bold=True)
        dim_c = Colour(100, 100, 100)
        step_str = f"step {snap.step}"
        self.fb.put_string(self.config.width - len(step_str) - 3, 0, step_str, dim_c)

    def _draw_border(self) -> None:
        c = Colour(60, 60, 80)
        self.fb.box(1, 1, self.config.lattice_w, self.config.lattice_h, c)
        self.fb.box(self.config.panel_x, 1, self.config.panel_w, self.config.lattice_h, c)

    def _draw_lattice_axes(self, snap: TUISnapshot) -> None:
        c = Colour(100, 140, 180)
        self.fb.put_string(3, self.config.lattice_h - 1, "a->", c)
        axis_label = "c ^" if self.config.xz_slice else "b ^"
        self.fb.put_string(3, self.config.lattice_h - 2, axis_label, c)
        vol_c = Colour(120, 120, 120)
        self.fb.put_string(3, self.config.lattice_h, f"V={snap.lattice.volume:.1f} A^3", vol_c)

    def _bounding_box(self, snap: TUISnapshot):
        xs = [p.x for p in snap.positions]
        ys = [(p.z if self.config.xz_slice else p.y) for p in snap.positions]
        pad = 1.0
        x_min, x_max = min(xs) - pad, max(xs) + pad
        y_min, y_max = min(ys) - pad, max(ys) + pad
        return x_min, x_max - x_min, y_min, y_max - y_min

    def _draw_atoms(self, snap: TUISnapshot) -> None:
        if not snap.positions:
            return
        x_min, x_range, y_min, y_range = self._bounding_box(snap)

        for i, pos in enumerate(snap.positions):
            px = pos.x
            py = pos.z if self.config.xz_slice else pos.y
            gx = self._proj_x(px, x_min, x_range)
            gy = self._proj_y(py, y_min, y_range)
            Z = snap.types[i] if i < len(snap.types) else 0
            col = colour_for_Z(Z)
            self.fb.put(gx, gy, "o", col, bold=True)

    def _draw_forces(self, snap: TUISnapshot) -> None:
        if not self.config.show_forces or not snap.forces or not snap.positions:
            return
        x_min, x_range, y_min, y_range = self._bounding_box(snap)

        for i, fvec in enumerate(snap.forces):
            if i >= len(snap.positions):
                break
            fmag = fvec.norm()
            if fmag < 1e-6:
                continue
            px = snap.positions[i].x
            py = snap.positions[i].z if self.config.xz_slice else snap.positions[i].y
            gx = self._proj_x(px, x_min, x_range)
            gy = self._proj_y(py, y_min, y_range)
            fx = fvec.x
            fy = fvec.z if self.config.xz_slice else fvec.y
            ax = gx + (1 if fx > 0.3 else (-1 if fx < -0.3 else 0))
            ay = gy + (-1 if fy > 0.3 else (1 if fy < -0.3 else 0))
            fc = force_colour(fmag, snap.Fmax)
            self.fb.put(ax, ay, _arrow_char(fx, fy), fc)

    def _draw_wind(self, snap: TUISnapshot) -> None:
        if not self.config.show_wind or snap.wind is None:
            return
        wind_c = Colour(60, 180, 130)
        wx = snap.wind.direction.x
        wy = snap.wind.direction.z if self.config.xz_slice else snap.wind.direction.y
        arrow = _arrow_char(wx, wy)
        for y in range(4, self.config.lattice_h - 2, 3):
            for x in range(4, self.config.lattice_w - 2, 6):
                if self.fb.at(x, y).ch == " ":
                    self.fb.put(x, y, arrow, wind_c)
        label_c = Colour(40, 150, 110)
        wlabel = f"wind {snap.wind.strength:.2f} ramp={snap.wind.ramp * 100:.0f}%"
        self.fb.put_string(self.config.lattice_w - len(wlabel) - 2, self.config.lattice_h, wlabel, label_c)

    def _draw_math_panel(self, snap: TUISnapshot) -> None:
        x0 = self.config.panel_x + 2
        y = 2
        hdr = Colour(80, 220, 255)
        val = Colour(200, 200, 200)
        dim = Colour(120, 120, 120)
        hot = Colour(255, 100, 60)
        cool = Colour(60, 180, 255)
        grn = Colour(80, 230, 80)

        self.fb.put_string(x0, y, "=== MATHEMATICS ===", hdr, bold=True); y += 2
        self.fb.put_string(x0, y, "Energy (kcal/mol)", hdr); y += 1
        self.fb.put_string(x0, y, f"  U_total  = {snap.U_total:.4f}", val); y += 1
        self.fb.put_string(x0, y, f"  U_bond   = {snap.U_bond:.4f}", dim); y += 1
        self.fb.put_string(x0, y, f"  U_vdW    = {snap.U_vdw:.4f}", dim); y += 1
        self.fb.put_string(x0, y, f"  U_Coul   = {snap.U_coul:.4f}", dim); y += 1
        self.fb.put_string(x0, y, f"  U_ext    = {snap.U_ext:.4f}", hot if snap.U_ext != 0 else dim); y += 1
        self.fb.put_string(x0, y, f"  KE       = {snap.KE:.4f}", dim); y += 2

        self.fb.put_string(x0, y, "Temperature", hdr); y += 1
        t_col = hot if snap.T > 500 else (cool if snap.T < 100 else val)
        self.fb.put_string(x0, y, f"  T = {snap.T:.1f} K", t_col); y += 2

        self.fb.put_string(x0, y, "Forces (kcal/mol/A)", hdr); y += 1
        self.fb.put_string(x0, y, f"  F_rms = {snap.Frms:.2e}", grn if snap.Frms < 1e-4 else val); y += 1
        self.fb.put_string(x0, y, f"  F_max = {snap.Fmax:.2e}", val); y += 2

        self.fb.put_string(x0, y, "Integration", hdr); y += 1
        self.fb.put_string(x0, y, f"  dt     = {snap.dt:.2e} fs", dim); y += 1
        self.fb.put_string(x0, y, f"  dt_eff = {snap.dt_eff:.2e} fs", grn if snap.dt_eff > snap.dt else dim); y += 1
        self.fb.put_string(x0, y, f"  N_atom = {snap.N_atoms}", dim); y += 2

        if snap.wind is not None:
            self.fb.put_string(x0, y, "Wind Particle", hdr); y += 1
            self.fb.put_string(x0, y, f"  strength = {snap.wind.strength:.3f}", val); y += 1
            self.fb.put_string(x0, y, f"  ramp     = {snap.wind.ramp * 100:.0f} %",
                               cool if snap.wind.ramp < 1.0 else grn); y += 1
            self.fb.put_string(x0, y, f"  F_peak   = {snap.wind.peak_force:.4f}", val); y += 1
            self.fb.put_string(x0, y, f"  headroom = {snap.wind.headroom:.1f}x",
                               grn if snap.wind.headroom >= 1.5 else hot); y += 1
            self.fb.put_string(x0, y, f"  dt_factor= {snap.wind.dt_factor:.1f}x", grn); y += 2

        # Lattice info (flows after wind section to avoid overlap)
        lat = snap.lattice
        self.fb.put_string(x0, y, "Lattice", hdr); y += 1
        self.fb.put_string(x0, y, f"  a={lat.a:.2f} b={lat.b:.2f} c={lat.c:.2f}", val); y += 1
        self.fb.put_string(x0, y, f"  alpha={lat.alpha_deg:.1f} beta={lat.beta_deg:.1f}", val); y += 1
        self.fb.put_string(x0, y, f"  gamma={lat.gamma_deg:.1f}", val); y += 1
        self.fb.put_string(x0, y, f"  V = {lat.volume:.2f} A^3", val)
