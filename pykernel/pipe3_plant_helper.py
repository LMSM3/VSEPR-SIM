"""
pipe3_plant_helper.py -- Standalone Pipe-Side Helper for Power Plant Analysis
=============================================================================

RS-0001 Framework Adapted: Section [4] External / Engineering Conditions.

Implements:
  PipeSegment           -- single pipe run with geometry, material, insulation
  PlantPipeNetwork      -- collection of segments forming a plant piping network
  PlantEngineeringState -- electrical, transport, material, nuclear-specific fields
  NetworkAnalyzer       -- multi-segment pressure drop / flow analysis
                           using gas3 and pipe3 infrastructure

Links to gas3 (pykernel.gas) for EOS, friction, Reynolds.
Links to pipe3 (pykernel.pipe) for Pipe[dict] streaming + CSV/JSON sinks.

Anti-black-box: every segment result is individually traceable.

VSEPR-SIM 4.0.4
"""

from __future__ import annotations

import math
import time
import os
import logging
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple
from enum import Enum, auto

_log = logging.getLogger(__name__)

# ── Late imports ──────────────────────────────────────────────────────
def _import_gas():
    import sys, importlib.util, pathlib as _pl
    name = 'pykernel.gas'
    if name in sys.modules:
        return sys.modules[name]
    p = _pl.Path(__file__).parent / 'gas.py'
    spec = importlib.util.spec_from_file_location(name, str(p))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod

def _import_pipe():
    import sys, importlib.util, pathlib as _pl
    name = 'pykernel.pipe'
    if name in sys.modules:
        return sys.modules[name]
    p = _pl.Path(__file__).parent / 'pipe.py'
    spec = importlib.util.spec_from_file_location(name, str(p))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


# =====================================================================
# Constants
# =====================================================================

R = 8.31446
ATM = 101325.0
MISSING = -9999.0


# =====================================================================
# RS-0001 [4]: External / Engineering Conditions
# =====================================================================

@dataclass
class ElectricalState:
    """Grid and generator electrical state."""
    grid_load_MW: float = MISSING
    generator_voltage_V: float = MISSING
    current_A: float = MISSING
    frequency_Hz: float = 60.0
    power_factor: float = 0.95


@dataclass
class MaterialContext:
    """Structural / chemistry context for a pipe segment."""
    structural_alloy: str = ""
    salt_chemistry: str = ""
    corrosion_state: str = "nominal"
    fouling_index: float = 0.0
    oxidation_state: str = ""
    max_service_T_K: float = MISSING


@dataclass
class NuclearContext:
    """Nuclear-specific fields."""
    neutron_flux: float = MISSING     # n/(cm^2 s)
    k_eff: float = MISSING
    fuel_salt_composition: Dict[str, float] = field(default_factory=dict)
    decay_heat_W: float = MISSING
    reactivity_margin: float = MISSING


@dataclass
class PlantEngineeringState:
    """RS-0001 Section [4]: complete engineering state."""
    electrical: ElectricalState = field(default_factory=ElectricalState)
    material: MaterialContext = field(default_factory=MaterialContext)
    nuclear: NuclearContext = field(default_factory=NuclearContext)

    def as_dict(self) -> Dict:
        d = {}
        for k, v in self.electrical.__dict__.items():
            d["elec_" + k] = v
        for k, v in self.material.__dict__.items():
            d["mat_" + k] = v
        for k, v in self.nuclear.__dict__.items():
            if isinstance(v, dict):
                d["nuc_" + k] = str(v)
            else:
                d["nuc_" + k] = v
        return d


# =====================================================================
# Pipe segment definition
# =====================================================================

class InsulationType(Enum):
    NONE       = auto()
    MINERAL_WOOL = auto()
    CERAMIC_FIBER = auto()
    CALCIUM_SILICATE = auto()
    AEROGEL    = auto()


@dataclass
class PipeSegment:
    """Single pipe segment in a plant network."""
    segment_id: str = ""
    label: str = ""
    # Geometry
    length_m: float = 10.0
    inner_diameter_m: float = 0.3
    wall_thickness_m: float = 0.01
    roughness_m: float = 4.5e-5
    elevation_change_m: float = 0.0
    n_elbows: int = 0
    n_valves: int = 0
    # Fluid
    gas_symbol: str = "He"
    T_K: float = 800.0
    P_Pa: float = 70.0 * ATM
    velocity_m_s: float = 30.0
    # Material
    material: MaterialContext = field(default_factory=MaterialContext)
    insulation: InsulationType = InsulationType.MINERAL_WOOL
    insulation_thickness_m: float = 0.05
    # Results (filled by analyzer)
    Re: float = 0.0
    friction_f: float = 0.0
    dP_Pa: float = 0.0
    dP_minor_Pa: float = 0.0
    dP_total_Pa: float = 0.0
    rho_kg_m3: float = 0.0
    mach: float = 0.0
    m_dot_kg_s: float = 0.0
    heat_loss_W: float = 0.0
    residence_time_s: float = 0.0

    @property
    def outer_diameter_m(self) -> float:
        return self.inner_diameter_m + 2 * self.wall_thickness_m

    @property
    def flow_area_m2(self) -> float:
        return math.pi * (self.inner_diameter_m / 2) ** 2

    def as_dict(self) -> Dict:
        return {
            "segment_id": self.segment_id,
            "label": self.label,
            "L_m": self.length_m,
            "D_m": self.inner_diameter_m,
            "gas": self.gas_symbol,
            "T_K": round(self.T_K, 2),
            "P_Pa": round(self.P_Pa, 0),
            "P_atm": round(self.P_Pa / ATM, 3),
            "v_m_s": round(self.velocity_m_s, 2),
            "Re": round(self.Re, 0),
            "f": round(self.friction_f, 6),
            "dP_Pa": round(self.dP_Pa, 2),
            "dP_minor_Pa": round(self.dP_minor_Pa, 2),
            "dP_total_Pa": round(self.dP_total_Pa, 2),
            "dP_total_kPa": round(self.dP_total_Pa / 1000, 4),
            "rho_kg_m3": round(self.rho_kg_m3, 4),
            "Mach": round(self.mach, 5),
            "mdot_kg_s": round(self.m_dot_kg_s, 4),
            "heat_loss_W": round(self.heat_loss_W, 1),
            "residence_s": round(self.residence_time_s, 4),
            "alloy": self.material.structural_alloy,
        }

    def header(self) -> str:
        return ("{:>8} {:>20} {:>8} {:>6} {:>4} {:>9} {:>10} {:>8} "
                "{:>12} {:>10} {:>12} {:>12} {:>10} {:>8} {:>10}").format(
            "Seg", "Label", "L(m)", "D(m)", "Gas", "T(K)", "P(atm)", "v(m/s)",
            "Re", "f", "dP(Pa)", "dP_tot(Pa)", "rho", "Mach", "mdot")

    def row(self) -> str:
        return ("{:>8} {:>20} {:8.1f} {:6.3f} {:>4} {:9.1f} {:10.3f} {:8.2f} "
                "{:12.0f} {:10.6f} {:12.2f} {:12.2f} {:10.4f} {:8.5f} {:10.4f}").format(
            self.segment_id, self.label, self.length_m, self.inner_diameter_m,
            self.gas_symbol, self.T_K, self.P_Pa / ATM, self.velocity_m_s,
            self.Re, self.friction_f, self.dP_Pa, self.dP_total_Pa,
            self.rho_kg_m3, self.mach, self.m_dot_kg_s)


# =====================================================================
# Plant pipe network
# =====================================================================

class PlantPipeNetwork:
    """Collection of pipe segments forming a plant piping network."""

    def __init__(self, name: str = "plant_network"):
        self.name = name
        self.segments: List[PipeSegment] = []
        self.engineering: PlantEngineeringState = PlantEngineeringState()

    def add(self, seg: PipeSegment) -> PipeSegment:
        if not seg.segment_id:
            seg.segment_id = "S{:03d}".format(len(self.segments) + 1)
        self.segments.append(seg)
        return seg

    @property
    def total_length_m(self) -> float:
        return sum(s.length_m for s in self.segments)

    @property
    def total_dP_Pa(self) -> float:
        return sum(s.dP_total_Pa for s in self.segments)

    @property
    def total_heat_loss_W(self) -> float:
        return sum(s.heat_loss_W for s in self.segments)

    def print_table(self, title: str = "Plant Pipe Network"):
        if not self.segments:
            print("(empty network)")
            return
        print("\n" + "=" * 180)
        print("  " + title)
        print("=" * 180)
        print(self.segments[0].header())
        print("-" * 180)
        for s in self.segments:
            print(s.row())
        print("-" * 180)
        print("  Total length: {:.1f} m | Total dP: {:.2f} Pa ({:.4f} atm) "
              "| Total heat loss: {:.1f} W".format(
            self.total_length_m, self.total_dP_Pa,
            self.total_dP_Pa / ATM, self.total_heat_loss_W))
        print("=" * 180)

    def as_dicts(self) -> List[Dict]:
        return [s.as_dict() for s in self.segments]


# =====================================================================
# Network analyzer
# =====================================================================

# Minor loss coefficients (K-factors)
K_ELBOW_90 = 0.9
K_ELBOW_45 = 0.4
K_GATE_VALVE = 0.2
K_GLOBE_VALVE = 10.0
K_CHECK_VALVE = 2.5

# Insulation thermal conductivity (W/(m K))
INSULATION_K = {
    InsulationType.NONE: 0.0,
    InsulationType.MINERAL_WOOL: 0.04,
    InsulationType.CERAMIC_FIBER: 0.08,
    InsulationType.CALCIUM_SILICATE: 0.06,
    InsulationType.AEROGEL: 0.015,
}


class NetworkAnalyzer:
    """
    Multi-segment pipe flow analysis for power plant networks.

    Uses gas3 for fluid properties and friction factor.
    Streams results through pipe3 Pipe[dict].

    Usage:
        network = PlantPipeNetwork("HTGR primary loop")
        network.add(PipeSegment(label="Core inlet", ...))
        network.add(PipeSegment(label="Hot leg", ...))
        analyzer = NetworkAnalyzer(network)
        analyzer.analyze()
        network.print_table()
        analyzer.stream_to_csv("output/pipe_results.csv")
    """

    def __init__(self, network: PlantPipeNetwork):
        self.network = network
        self._gas = _import_gas()

    def _analyze_segment(self, seg: PipeSegment):
        """Compute flow properties for a single segment."""
        gas = self._gas
        sp = gas.GAS_DB.get(seg.gas_symbol)
        if sp is None:
            _log.warning("Gas %s not in DB, skipping segment %s",
                         seg.gas_symbol, seg.segment_id)
            return

        # Fluid properties
        rho = gas.ideal_density(sp, seg.T_K, seg.P_Pa)
        Re = gas.reynolds_number(rho, seg.velocity_m_s,
                                 seg.inner_diameter_m, sp.mu_Pa_s)
        eps_D = seg.roughness_m / seg.inner_diameter_m
        f = gas.colebrook_white(Re, eps_D)

        # Major loss (Darcy-Weisbach)
        dP_major = gas.darcy_weisbach_dp(
            f, seg.length_m, seg.inner_diameter_m, rho, seg.velocity_m_s)

        # Minor losses
        K_total = (seg.n_elbows * K_ELBOW_90 +
                   seg.n_valves * K_GATE_VALVE)
        dP_minor = K_total * 0.5 * rho * seg.velocity_m_s ** 2

        # Elevation head
        dP_elev = rho * 9.80665 * seg.elevation_change_m

        # Mach number
        a = gas.speed_of_sound(sp, seg.T_K)
        Ma = seg.velocity_m_s / a

        # Mass flow
        A = seg.flow_area_m2
        mdot = rho * seg.velocity_m_s * A

        # Residence time
        t_res = seg.length_m / seg.velocity_m_s if seg.velocity_m_s > 0 else 0

        # Heat loss estimate (simplified cylindrical insulation)
        T_amb = self.network.engineering.electrical.frequency_Hz  # placeholder
        T_amb = 293.15  # ambient
        k_ins = INSULATION_K.get(seg.insulation, 0.0)
        if k_ins > 0 and seg.insulation_thickness_m > 0:
            r_inner = seg.inner_diameter_m / 2 + seg.wall_thickness_m
            r_outer = r_inner + seg.insulation_thickness_m
            R_ins = math.log(r_outer / r_inner) / (2 * math.pi * k_ins * seg.length_m)
            Q_loss = (seg.T_K - T_amb) / R_ins if R_ins > 0 else 0.0
        else:
            Q_loss = 0.0

        # Store results
        seg.rho_kg_m3 = rho
        seg.Re = Re
        seg.friction_f = f
        seg.dP_Pa = dP_major
        seg.dP_minor_Pa = dP_minor
        seg.dP_total_Pa = dP_major + dP_minor + dP_elev
        seg.mach = Ma
        seg.m_dot_kg_s = mdot
        seg.heat_loss_W = Q_loss
        seg.residence_time_s = t_res

    def analyze(self) -> PlantPipeNetwork:
        """Run flow analysis on all segments."""
        t0 = time.time()
        for seg in self.network.segments:
            self._analyze_segment(seg)
        elapsed = time.time() - t0
        print("[pipe3] Analyzed {} segments in {:.4f}s".format(
            len(self.network.segments), elapsed))
        return self.network

    def stream_to_pipe(self, pipe_name: str = "pipe_network") -> "Pipe":
        """Push segment results through a Pipe[dict]."""
        pipe_mod = _import_pipe()
        pipe = pipe_mod.Pipe(pipe_name)
        for seg in self.network.segments:
            pipe.push(seg.as_dict(), source=seg.segment_id)
        return pipe

    def stream_to_csv(self, filepath: str):
        """Stream results through Pipe -> CSVSink."""
        pipe_mod = _import_pipe()
        pipe = pipe_mod.Pipe("csv_export")
        if not self.network.segments:
            return
        columns = list(self.network.segments[0].as_dict().keys())
        os.makedirs(os.path.dirname(filepath) or '.', exist_ok=True)
        sink = pipe_mod.CSVSink(pipe, filepath, columns)
        for seg in self.network.segments:
            pipe.push(seg.as_dict(), source=seg.segment_id)
        print("[pipe3] Exported {} rows to {}".format(sink.count, filepath))

    def stream_to_json(self, filepath: str):
        """Stream results through Pipe -> JSONSink."""
        pipe_mod = _import_pipe()
        pipe = pipe_mod.Pipe("json_export")
        sink = pipe_mod.JSONSink(pipe, filepath)
        for seg in self.network.segments:
            pipe.push(seg.as_dict(), source=seg.segment_id)
        print("[pipe3] Exported {} records to {}".format(sink.count, filepath))

    def summary(self) -> Dict:
        """Network summary statistics."""
        net = self.network
        return {
            "name": net.name,
            "n_segments": len(net.segments),
            "total_length_m": round(net.total_length_m, 1),
            "total_dP_Pa": round(net.total_dP_Pa, 2),
            "total_dP_atm": round(net.total_dP_Pa / ATM, 4),
            "total_heat_loss_W": round(net.total_heat_loss_W, 1),
            "max_velocity_m_s": round(max((s.velocity_m_s for s in net.segments), default=0), 2),
            "max_Mach": round(max((s.mach for s in net.segments), default=0), 5),
            "max_Re": round(max((s.Re for s in net.segments), default=0), 0),
        }


# =====================================================================
# Preset factories
# =====================================================================

def htgr_primary_loop() -> Tuple[PlantPipeNetwork, NetworkAnalyzer]:
    """HTGR helium primary loop piping preset."""
    net = PlantPipeNetwork("HTGR He Primary Loop")
    net.engineering.material = MaterialContext(
        structural_alloy="Alloy 800H",
        max_service_T_K=1123.15,
    )
    net.engineering.nuclear = NuclearContext(
        neutron_flux=1e14,
        k_eff=1.0005,
        decay_heat_W=2.1e7,
    )
    net.engineering.electrical = ElectricalState(
        grid_load_MW=300.0,
        generator_voltage_V=22000.0,
        frequency_Hz=60.0,
    )
    # Hot leg (reactor -> turbine)
    net.add(PipeSegment(
        label="Core Outlet Duct", length_m=5.0, inner_diameter_m=0.8,
        gas_symbol="He", T_K=1123.15, P_Pa=70*ATM, velocity_m_s=40.0,
        n_elbows=1,
        material=MaterialContext(structural_alloy="Alloy 800H"),
        insulation=InsulationType.CERAMIC_FIBER, insulation_thickness_m=0.1,
    ))
    net.add(PipeSegment(
        label="Hot Leg Main", length_m=25.0, inner_diameter_m=0.7,
        gas_symbol="He", T_K=1120.0, P_Pa=69*ATM, velocity_m_s=45.0,
        n_elbows=4, n_valves=1,
        material=MaterialContext(structural_alloy="Alloy 800H"),
        insulation=InsulationType.CERAMIC_FIBER, insulation_thickness_m=0.1,
    ))
    # Cold leg (compressor -> reactor)
    net.add(PipeSegment(
        label="Precooler Outlet", length_m=8.0, inner_diameter_m=0.6,
        gas_symbol="He", T_K=308.15, P_Pa=27*ATM, velocity_m_s=25.0,
        n_elbows=2,
        material=MaterialContext(structural_alloy="SS316L"),
        insulation=InsulationType.MINERAL_WOOL, insulation_thickness_m=0.05,
    ))
    net.add(PipeSegment(
        label="Cold Leg Main", length_m=20.0, inner_diameter_m=0.6,
        gas_symbol="He", T_K=773.15, P_Pa=70*ATM, velocity_m_s=35.0,
        n_elbows=3, n_valves=1, elevation_change_m=5.0,
        material=MaterialContext(structural_alloy="Alloy 617"),
        insulation=InsulationType.MINERAL_WOOL, insulation_thickness_m=0.08,
    ))
    net.add(PipeSegment(
        label="Core Inlet Duct", length_m=4.0, inner_diameter_m=0.8,
        gas_symbol="He", T_K=773.15, P_Pa=70*ATM, velocity_m_s=30.0,
        n_elbows=1,
        material=MaterialContext(structural_alloy="Alloy 617"),
        insulation=InsulationType.CERAMIC_FIBER, insulation_thickness_m=0.08,
    ))

    return net, NetworkAnalyzer(net)


def sco2_recompression_loop() -> Tuple[PlantPipeNetwork, NetworkAnalyzer]:
    """sCO2 recompression Brayton piping preset."""
    net = PlantPipeNetwork("sCO2 Recompression Loop")
    net.engineering.electrical = ElectricalState(grid_load_MW=100.0)
    net.engineering.material = MaterialContext(
        structural_alloy="Alloy 740H",
        max_service_T_K=1023.15,
    )

    segments = [
        ("Heater Outlet",     6.0,  0.15, "CO2", 823.15,  250*ATM, 20.0, 2, 0),
        ("Turbine Inlet Pipe", 3.0, 0.12, "CO2", 820.0,   248*ATM, 25.0, 1, 1),
        ("Turbine Outlet",    4.0,  0.20, "CO2", 673.15,  77*ATM,  35.0, 2, 0),
        ("HT Recup Hot",     12.0,  0.18, "CO2", 573.15,  76*ATM,  30.0, 4, 0),
        ("LT Recup Hot",     10.0,  0.18, "CO2", 373.15,  75*ATM,  28.0, 3, 0),
        ("Precooler",         5.0,  0.15, "CO2", 308.15,  77*ATM,  15.0, 2, 1),
        ("Main Comp Out",     3.0,  0.10, "CO2", 318.15,  250*ATM, 12.0, 1, 1),
        ("Recomp Out",        4.0,  0.10, "CO2", 373.15,  250*ATM, 14.0, 1, 0),
    ]
    for lbl, L, D, gas, T, P, v, elb, vlv in segments:
        net.add(PipeSegment(
            label=lbl, length_m=L, inner_diameter_m=D,
            gas_symbol=gas, T_K=T, P_Pa=P, velocity_m_s=v,
            n_elbows=elb, n_valves=vlv,
            material=MaterialContext(structural_alloy="Alloy 740H"),
            insulation=InsulationType.AEROGEL, insulation_thickness_m=0.03,
        ))

    return net, NetworkAnalyzer(net)


def steam_rankine_piping() -> Tuple[PlantPipeNetwork, NetworkAnalyzer]:
    """Steam Rankine cycle piping preset (coal plant)."""
    net = PlantPipeNetwork("Steam Rankine Piping")
    net.engineering.electrical = ElectricalState(
        grid_load_MW=600.0,
        generator_voltage_V=24000.0,
        frequency_Hz=60.0,
    )
    segments = [
        ("SH Outlet",     15.0, 0.40, "H2O", 811.15,  170*ATM, 30.0, 4, 2),
        ("HP Turb Inlet",  5.0, 0.35, "H2O", 808.15,  168*ATM, 35.0, 1, 1),
        ("CRH (Cold RH)", 20.0, 0.50, "H2O", 623.15,  40*ATM,  40.0, 6, 1),
        ("HRH (Hot RH)",  18.0, 0.55, "H2O", 811.15,  38*ATM,  50.0, 5, 1),
        ("LP Crossover",  12.0, 0.80, "H2O", 623.15,  8*ATM,   60.0, 3, 0),
        ("Exhaust Hood",   3.0, 1.50, "H2O", 318.15,  0.1*ATM, 80.0, 1, 0),
    ]
    for lbl, L, D, gas, T, P, v, elb, vlv in segments:
        net.add(PipeSegment(
            label=lbl, length_m=L, inner_diameter_m=D,
            gas_symbol=gas, T_K=T, P_Pa=P, velocity_m_s=v,
            n_elbows=elb, n_valves=vlv,
            material=MaterialContext(structural_alloy="P91/P22"),
            insulation=InsulationType.CALCIUM_SILICATE, insulation_thickness_m=0.1,
        ))

    return net, NetworkAnalyzer(net)


# =====================================================================
# Quick-run
# =====================================================================

def run_all_presets():
    """Analyze and print all piping presets."""
    presets = [
        ("HTGR He Primary",     htgr_primary_loop),
        ("sCO2 Recompression",  sco2_recompression_loop),
        ("Steam Rankine",       steam_rankine_piping),
    ]

    for name, factory in presets:
        print("\n" + "#" * 80)
        print("#  PIPE NETWORK: " + name)
        print("#" * 80)
        net, analyzer = factory()
        analyzer.analyze()
        net.print_table()
        summary = analyzer.summary()
        print("\n  Network summary:")
        for k, v in summary.items():
            print("    {:25s}: {}".format(k, v))
        # Stream through pipe
        pipe = analyzer.stream_to_pipe("pipe_" + name.replace(" ", "_"))
        print("  Pipe stats: {}".format(pipe.stats()))
        # Engineering state
        eng = net.engineering.as_dict()
        print("  Engineering state:")
        for k, v in eng.items():
            if v != MISSING:
                print("    {:30s}: {}".format(k, v))


if __name__ == "__main__":
    run_all_presets()
