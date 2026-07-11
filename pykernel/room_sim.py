"""
room_sim.py -- 3D Room Geometry with Helium Thermal Diffusion

Simulates helium gas heat dispersion in an enclosed room with
hard-coded geometric obstacles that block gas flow / diffusion.

Architecture:
  RoomGeometry    3D voxel grid (NxNxN) with boolean obstacle mask.
  ThermalSolver   Explicit finite-difference 3D heat equation.
  RoomSimulation  Ties it all together with presets.

VSEPR-SIM 4.0.4
"""

from __future__ import annotations

import math
import time
import logging
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Tuple
from enum import Enum, auto

_log = logging.getLogger(__name__)

try:
    import numpy as np
except ImportError:
    raise ImportError("room_sim requires numpy")

# =====================================================================
# Constants
# =====================================================================

R_GAS = 8.31446
ATM   = 101325.0
HE_M  = 4.003e-3     # kg/mol
HE_CP = 5193.0       # J/(kg K)  monatomic 5/2 R / M


def he_thermal_diffusivity(T_K: float, P_Pa: float) -> float:
    """alpha = k / (rho Cp)  for helium."""
    rho = P_Pa * HE_M / (R_GAS * T_K)
    k = 0.1513 * (T_K / 300.0) ** 0.67
    return k / (rho * HE_CP)


# =====================================================================
# Obstacle types
# =====================================================================

class ObstacleType(Enum):
    COLUMN    = auto()
    WALL      = auto()
    EQUIPMENT = auto()
    PIPE_RUN  = auto()


@dataclass
class Obstacle:
    """Record for a placed obstacle."""
    kind: ObstacleType
    label: str = ""
    x0: int = 0; y0: int = 0; z0: int = 0
    x1: int = 0; y1: int = 0; z1: int = 0
    cx: float = 0; cy: float = 0; radius: float = 0
    T_fixed: float = float("nan")


# =====================================================================
# Room geometry (voxel grid + obstacle mask)
# =====================================================================

class RoomGeometry:
    """3D voxel room with obstacle mask."""

    def __init__(self, nx: int = 40, ny: int = 40, nz: int = 20,
                 room_size_m: Tuple[float, float, float] = (8.0, 8.0, 4.0)):
        self.nx, self.ny, self.nz = nx, ny, nz
        self.Lx, self.Ly, self.Lz = room_size_m
        self.dx = self.Lx / nx
        self.dy = self.Ly / ny
        self.dz = self.Lz / nz
        self.mask = np.zeros((nx, ny, nz), dtype=bool)
        self.obstacle_T = np.full((nx, ny, nz), float("nan"))
        self.obstacles: List[Obstacle] = []

    @property
    def n_cells(self) -> int:
        return self.nx * self.ny * self.nz

    @property
    def n_open(self) -> int:
        return int(np.sum(~self.mask))

    @property
    def n_solid(self) -> int:
        return int(np.sum(self.mask))

    # -- primitive inserters ------------------------------------------------

    def add_box(self, x0, y0, z0, x1, y1, z1, label="",
                kind=ObstacleType.EQUIPMENT, T_fixed=float("nan")):
        x0, x1 = max(0, x0), min(self.nx - 1, x1)
        y0, y1 = max(0, y0), min(self.ny - 1, y1)
        z0, z1 = max(0, z0), min(self.nz - 1, z1)
        self.mask[x0:x1+1, y0:y1+1, z0:z1+1] = True
        if math.isfinite(T_fixed):
            self.obstacle_T[x0:x1+1, y0:y1+1, z0:z1+1] = T_fixed
        obs = Obstacle(kind=kind, label=label,
                       x0=x0, y0=y0, z0=z0, x1=x1, y1=y1, z1=z1,
                       T_fixed=T_fixed)
        self.obstacles.append(obs)
        return obs

    def add_column(self, cx, cy, radius, z0=0, z1=None, label="",
                   T_fixed=float("nan")):
        if z1 is None:
            z1 = self.nz - 1
        for i in range(self.nx):
            for j in range(self.ny):
                if math.sqrt((i - cx)**2 + (j - cy)**2) <= radius:
                    self.mask[i, j, z0:z1+1] = True
                    if math.isfinite(T_fixed):
                        self.obstacle_T[i, j, z0:z1+1] = T_fixed
        self.obstacles.append(Obstacle(
            kind=ObstacleType.COLUMN, label=label,
            z0=z0, z1=z1, cx=cx, cy=cy, radius=radius, T_fixed=T_fixed))

    def add_interior_wall(self, axis, pos, extent_start, extent_end,
                          thickness=1, z0=0, z1=None, label="",
                          T_fixed=float("nan")):
        if z1 is None:
            z1 = self.nz - 1
        if axis == "x":
            self.mask[pos:pos+thickness, extent_start:extent_end+1, z0:z1+1] = True
            if math.isfinite(T_fixed):
                self.obstacle_T[pos:pos+thickness,
                                extent_start:extent_end+1, z0:z1+1] = T_fixed
        else:
            self.mask[extent_start:extent_end+1, pos:pos+thickness, z0:z1+1] = True
            if math.isfinite(T_fixed):
                self.obstacle_T[extent_start:extent_end+1,
                                pos:pos+thickness, z0:z1+1] = T_fixed
        self.obstacles.append(Obstacle(kind=ObstacleType.WALL,
                                       label=label, T_fixed=T_fixed))

    def add_l_shaped_wall(self, corner_x, corner_y, arm1_len, arm2_len,
                          axis1="x", thickness=1, z0=0, z1=None, label=""):
        """L-shaped wall: two perpendicular segments meeting at a corner."""
        if z1 is None:
            z1 = self.nz - 1
        if axis1 == "x":
            self.add_interior_wall("x", corner_x, corner_y,
                                   corner_y + arm1_len, thickness, z0, z1,
                                   label=label + "_arm1")
            self.add_interior_wall("y", corner_y, corner_x,
                                   corner_x + arm2_len, thickness, z0, z1,
                                   label=label + "_arm2")
        else:
            self.add_interior_wall("y", corner_y, corner_x,
                                   corner_x + arm1_len, thickness, z0, z1,
                                   label=label + "_arm1")
            self.add_interior_wall("x", corner_x, corner_y,
                                   corner_y + arm2_len, thickness, z0, z1,
                                   label=label + "_arm2")

    def add_pipe_run(self, x0, y0, z0, x1, y1, z1, label="",
                     T_fixed=float("nan")):
        """Pipe run as a thin box-like obstruction (hot or cold)."""
        return self.add_box(x0, y0, z0, x1, y1, z1, label=label,
                            kind=ObstacleType.PIPE_RUN, T_fixed=T_fixed)

    # -- preset room layouts -----------------------------------------------

    def generate_random_room(self, seed=42, n_columns=3, n_walls=2,
                             n_equipment=4):
        """Fill the room with random obstacles (deterministic seed)."""
        rng = np.random.RandomState(seed)
        for c in range(n_columns):
            cx = rng.randint(4, self.nx - 4)
            cy = rng.randint(4, self.ny - 4)
            r = rng.uniform(1.5, 3.0)
            self.add_column(cx, cy, r, label="col_" + str(c))
        for w in range(n_walls):
            axis = "x" if rng.rand() > 0.5 else "y"
            dim = self.nx if axis == "x" else self.ny
            cross = self.ny if axis == "x" else self.nx
            pos = rng.randint(dim // 4, 3 * dim // 4)
            length = rng.randint(cross // 3, 2 * cross // 3)
            start = rng.randint(0, cross - length)
            self.add_interior_wall(axis, pos, start, start + length,
                                   thickness=1, label="wall_" + str(w))
        for e in range(n_equipment):
            sx = rng.randint(2, 6)
            sy = rng.randint(2, 6)
            sz = rng.randint(2, min(6, self.nz - 1))
            x0 = rng.randint(1, self.nx - sx - 1)
            y0 = rng.randint(1, self.ny - sy - 1)
            self.add_box(x0, y0, 0, x0 + sx, y0 + sy, sz,
                         label="equip_" + str(e))

    def generate_lab_room(self):
        """Hard-coded lab layout with known geometry."""
        # Two structural columns
        self.add_column(10, 10, 2.0, label="struct_col_A")
        self.add_column(30, 30, 2.5, label="struct_col_B")
        # Interior partition (partial wall with doorway gap)
        self.add_interior_wall("x", 20, 0, 15, thickness=1, label="partition_A")
        self.add_interior_wall("x", 20, 22, self.ny - 1, thickness=1,
                               label="partition_B")
        # L-shaped service corridor wall
        self.add_l_shaped_wall(5, 25, 10, 8, axis1="y", label="L_wall")
        # Equipment blocks: fume hood, workbench, cabinet
        self.add_box(2, 2, 0, 5, 4, 4, label="fume_hood")
        self.add_box(33, 2, 0, 38, 5, 3, label="workbench")
        self.add_box(33, 33, 0, 37, 37, 5, label="cabinet")
        # Hot pipe run across ceiling
        self.add_pipe_run(0, 19, self.nz - 2, self.nx - 1, 20, self.nz - 1,
                          label="hot_pipe", T_fixed=350.0)

    def generate_reactor_hall(self):
        """Hard-coded reactor containment hall layout."""
        # Reactor vessel (large central cylinder, hot surface)
        cx, cy = self.nx // 2, self.ny // 2
        self.add_column(cx, cy, 5.0, z0=0, z1=self.nz - 1,
                        label="reactor_vessel", T_fixed=900.0)
        # Biological shield ring
        for i in range(self.nx):
            for j in range(self.ny):
                r = math.sqrt((i - cx)**2 + (j - cy)**2)
                if 7.0 <= r <= 8.5:
                    self.mask[i, j, :] = True
        self.obstacles.append(Obstacle(kind=ObstacleType.WALL,
                                       label="bio_shield"))
        # Coolant pipe penetrations (gaps in the shield)
        gap_positions = [(cx + 8, cy - 1), (cx - 8, cy - 1),
                         (cx - 1, cy + 8), (cx - 1, cy - 8)]
        for gx, gy in gap_positions:
            gx = max(0, min(self.nx - 1, gx))
            gy = max(0, min(self.ny - 1, gy))
            self.mask[gx:gx+2, gy:gy+2, :] = False
        # Service platforms
        self.add_box(2, 2, 0, 8, 8, 2, label="platform_NW")
        self.add_box(self.nx - 9, self.ny - 9, 0,
                     self.nx - 2, self.ny - 2, 2, label="platform_SE")
        # Overhead crane rail
        self.add_box(0, cy - 1, self.nz - 2,
                     self.nx - 1, cy, self.nz - 1, label="crane_rail")

    def summary(self) -> str:
        pct = 100.0 * self.n_solid / self.n_cells
        lines = []
        lines.append("Room: {}x{}x{} m  grid: {}x{}x{}".format(
            self.Lx, self.Ly, self.Lz, self.nx, self.ny, self.nz))
        lines.append("  dx={:.3f}m  cells: {}  open: {}  solid: {} ({:.1f}%)".format(
            self.dx, self.n_cells, self.n_open, self.n_solid, pct))
        lines.append("  obstacles: {}".format(len(self.obstacles)))
        for obs in self.obstacles:
            lines.append("    {:12s} {}".format(obs.kind.name, obs.label))
        return "\n".join(lines)


# =====================================================================
# Thermal solver (explicit finite-difference 3D heat equation)
# =====================================================================

class ThermalSolver:
    """Solves dT/dt = alpha * laplacian(T) on the voxel grid."""

    def __init__(self, geom: RoomGeometry, T_ambient: float = 300.0,
                 P_Pa: float = ATM, alpha_override: float = None):
        self.geom = geom
        self.T_ambient = T_ambient
        self.P_Pa = P_Pa
        self.T = np.full((geom.nx, geom.ny, geom.nz), T_ambient,
                         dtype=np.float64)
        # Apply fixed-temperature obstacles
        fixed = np.isfinite(geom.obstacle_T)
        self.T[fixed] = geom.obstacle_T[fixed]
        # Thermal diffusivity
        if alpha_override is not None:
            self.alpha = alpha_override
        else:
            self.alpha = he_thermal_diffusivity(T_ambient, P_Pa)
        # Stability: dt < dx^2 / (6 alpha) for 3D explicit
        dx_min = min(geom.dx, geom.dy, geom.dz)
        self.dt_max = dx_min**2 / (6.0 * self.alpha)
        self.dt = 0.4 * self.dt_max
        self.step_count = 0
        self.t_elapsed = 0.0
        # History for time-series
        self.history: List[Dict] = []

    def set_heat_source(self, x0, y0, z0, x1, y1, z1, T_source):
        g = self.geom
        x0 = max(0, x0); x1 = min(g.nx - 1, x1)
        y0 = max(0, y0); y1 = min(g.ny - 1, y1)
        z0 = max(0, z0); z1 = min(g.nz - 1, z1)
        region = ~g.mask[x0:x1+1, y0:y1+1, z0:z1+1]
        self.T[x0:x1+1, y0:y1+1, z0:z1+1][region] = T_source

    def step(self, n: int = 1, record_interval: int = 0):
        """Advance n time steps.  If record_interval > 0, save stats."""
        g = self.geom
        mask = g.mask
        T = self.T
        alpha = self.alpha
        dt = self.dt
        dx2, dy2, dz2 = g.dx**2, g.dy**2, g.dz**2
        for i in range(n):
            Tp = np.pad(T, 1, mode="constant",
                        constant_values=self.T_ambient)
            lap = ((Tp[2:, 1:-1, 1:-1] + Tp[:-2, 1:-1, 1:-1] - 2*T) / dx2 +
                   (Tp[1:-1, 2:, 1:-1] + Tp[1:-1, :-2, 1:-1] - 2*T) / dy2 +
                   (Tp[1:-1, 1:-1, 2:] + Tp[1:-1, 1:-1, :-2] - 2*T) / dz2)
            T_new = T + alpha * dt * lap
            T_new[mask] = T[mask]
            fixed_mask = np.isfinite(g.obstacle_T)
            T_new[fixed_mask] = g.obstacle_T[fixed_mask]
            T[:] = T_new
            self.step_count += 1
            self.t_elapsed += dt
            if record_interval > 0 and (self.step_count % record_interval == 0):
                self.history.append({
                    "step": self.step_count,
                    "t_s": self.t_elapsed,
                    "T_min": self.T_min,
                    "T_mean": self.T_mean,
                    "T_max": self.T_max,
                })

    @property
    def T_min(self) -> float:
        o = self.T[~self.geom.mask]
        return float(o.min()) if len(o) > 0 else self.T_ambient

    @property
    def T_max(self) -> float:
        o = self.T[~self.geom.mask]
        return float(o.max()) if len(o) > 0 else self.T_ambient

    @property
    def T_mean(self) -> float:
        o = self.T[~self.geom.mask]
        return float(o.mean()) if len(o) > 0 else self.T_ambient

    def summary(self) -> str:
        lines = []
        lines.append("Solver: step={} t={:.4f}s dt={:.4e}s alpha={:.4e}".format(
            self.step_count, self.t_elapsed, self.dt, self.alpha))
        lines.append("  T: min={:.1f}K mean={:.1f}K max={:.1f}K".format(
            self.T_min, self.T_mean, self.T_max))
        return "\n".join(lines)


# =====================================================================
# Room configuration and simulation driver
# =====================================================================

@dataclass
class RoomConfig:
    nx: int = 40
    ny: int = 40
    nz: int = 20
    room_size_m: Tuple[float, float, float] = (8.0, 8.0, 4.0)
    T_ambient: float = 300.0
    P_Pa: float = ATM
    seed: int = 42
    n_columns: int = 3
    n_walls: int = 2
    n_equipment: int = 4
    heat_source_T: float = 800.0
    heat_source_pos: Tuple[int, int, int] = (2, 2, 0)
    heat_source_size: Tuple[int, int, int] = (3, 3, 3)
    n_steps: int = 200
    layout: str = "random"          # "random", "lab", "reactor"
    alpha_override: Optional[float] = None


class RoomSimulation:
    """Full room simulation driver with preset configurations."""

    def __init__(self, config: RoomConfig = None):
        self.config = config or RoomConfig()
        cfg = self.config
        self.geom = RoomGeometry(cfg.nx, cfg.ny, cfg.nz, cfg.room_size_m)
        # Choose layout
        if cfg.layout == "lab":
            self.geom.generate_lab_room()
        elif cfg.layout == "reactor":
            self.geom.generate_reactor_hall()
        else:
            self.geom.generate_random_room(seed=cfg.seed,
                                            n_columns=cfg.n_columns,
                                            n_walls=cfg.n_walls,
                                            n_equipment=cfg.n_equipment)
        self.solver = ThermalSolver(self.geom, T_ambient=cfg.T_ambient,
                                     P_Pa=cfg.P_Pa,
                                     alpha_override=cfg.alpha_override)
        hs = cfg.heat_source_pos
        sz = cfg.heat_source_size
        self.solver.set_heat_source(hs[0], hs[1], hs[2],
                                     hs[0] + sz[0] - 1,
                                     hs[1] + sz[1] - 1,
                                     hs[2] + sz[2] - 1,
                                     cfg.heat_source_T)

    def run(self, n_steps: int = None, record_interval: int = 50) -> "RoomSimulation":
        steps = n_steps or self.config.n_steps
        t0 = time.time()
        self.solver.step(steps, record_interval=record_interval)
        elapsed = time.time() - t0
        rate = steps / max(elapsed, 1e-9)
        print("[room_sim] {} steps in {:.2f}s ({:.0f} steps/s)".format(
            steps, elapsed, rate))
        print(self.geom.summary())
        print(self.solver.summary())
        return self

    # -- presets -----------------------------------------------------------

    @staticmethod
    def preset_ambient() -> RoomConfig:
        return RoomConfig(T_ambient=300.0, heat_source_T=800.0, n_steps=300)

    @staticmethod
    def preset_cold() -> RoomConfig:
        return RoomConfig(T_ambient=200.0, heat_source_T=500.0, n_steps=300)

    @staticmethod
    def preset_hot() -> RoomConfig:
        return RoomConfig(T_ambient=400.0, heat_source_T=1000.0, n_steps=300)

    @staticmethod
    def preset_reactor() -> RoomConfig:
        return RoomConfig(
            nx=50, ny=50, nz=25,
            room_size_m=(12.0, 12.0, 6.0),
            T_ambient=600.0, heat_source_T=1200.0,
            P_Pa=ATM * 50, n_steps=400, layout="reactor",
            heat_source_pos=(23, 23, 5), heat_source_size=(4, 4, 4))

    @staticmethod
    def preset_large_volume() -> RoomConfig:
        return RoomConfig(
            nx=50, ny=50, nz=25,
            room_size_m=(20.0, 20.0, 8.0),
            T_ambient=300.0, heat_source_T=900.0,
            n_steps=250, n_columns=5, n_equipment=6)

    @staticmethod
    def preset_lab() -> RoomConfig:
        return RoomConfig(
            T_ambient=295.0, heat_source_T=600.0, n_steps=300,
            layout="lab",
            heat_source_pos=(15, 15, 0), heat_source_size=(3, 3, 3))
