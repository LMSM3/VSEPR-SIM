"""
gas3_plant_helper.py -- Standalone Gas-Side Helper for Power Plant Analysis
===========================================================================

RS-0001 Framework Adapted for Power Plant Analysis.

Implements:
  [1] PlantMetadata      -- report ID, mode, family, phase, scale, confidence
  [2] PlantInitialState  -- inlet/outlet T/P, mass flow, compositions, machine states
  [3] PlantNodeTable     -- per-component state table (T, P, rho, h, s, phase, mdot)
  [4] PlantGasAnalyzer   -- multi-node cycle analysis piped through Pipe[dict]

Cycle types supported:
  - Brayton (He, sCO2, N2, air)
  - Rankine (H2O steam)
  - Combined (gas turbine + HRSG + steam)
  - Nuclear coolant loops (He HTGR, Na fast, FLiBe MSR)

Links to gas3 (pykernel.gas) for EOS, transport, compressibility.
Links to pipe3 (pykernel.pipe) for streaming results.

Anti-black-box: every intermediate is traceable, every default physically motivated.

VSEPR-SIM 4.0.4
"""

from __future__ import annotations

import math
import time
import logging
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple
from enum import Enum, auto
from datetime import datetime

_log = logging.getLogger(__name__)

# ── Late imports (bypass __init__ gate) ───────────────────────────────
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

R = 8.31446          # J/(mol K)
ATM = 101325.0       # Pa
MISSING = -9999.0    # sentinel


# =====================================================================
# [1] Metadata Block
# =====================================================================

class PlantFamily(Enum):
    THERMAL       = auto()
    NUCLEAR       = auto()
    COMBINED      = auto()
    ELECTROTHERMAL = auto()


class PlantMode(Enum):
    STARTUP        = auto()
    RAMP           = auto()
    BASE_LOAD      = auto()
    TRANSIENT      = auto()
    SHUTDOWN       = auto()
    FAULT_RESPONSE = auto()


class ConfidenceTier(Enum):
    C1 = "C1"   # published / validated
    C2 = "C2"   # estimated / extrapolated
    C3 = "C3"   # placeholder


@dataclass
class PlantMetadata:
    """RS-0001 Section [1]: Plant identity and run mode."""
    report_id: str = "PP-0001"
    mode: str = "Steady-State"
    system: str = ""
    date: str = ""
    operator: str = "gas3_plant_helper"
    family: PlantFamily = PlantFamily.THERMAL
    phase: str = "gas"
    scale: str = "loop"
    source_theoretical: str = "EOS / ideal-Brayton"
    source_experimental: str = ""
    units: str = "SI"
    confidence: ConfidenceTier = ConfidenceTier.C2
    plant_mode: PlantMode = PlantMode.BASE_LOAD

    def __post_init__(self):
        if not self.date:
            self.date = datetime.now().strftime("%Y-%m-%d")

    def as_dict(self) -> Dict:
        return {
            "report_id": self.report_id,
            "mode": self.mode,
            "system": self.system,
            "date": self.date,
            "operator": self.operator,
            "family": self.family.name,
            "phase": self.phase,
            "scale": self.scale,
            "source_theoretical": self.source_theoretical,
            "source_experimental": self.source_experimental,
            "units": self.units,
            "confidence": self.confidence.value,
            "plant_mode": self.plant_mode.name,
        }

    def header_text(self) -> str:
        lines = [
            "=" * 80,
            "  RS-0001 PLANT ANALYSIS REPORT",
            "=" * 80,
        ]
        for k, v in self.as_dict().items():
            lines.append("  {:25s}: {}".format(k, v))
        lines.append("=" * 80)
        return "\n".join(lines)


# =====================================================================
# [2] Initial State Definition
# =====================================================================

@dataclass
class MachineState:
    """Machine / component state inputs."""
    turbine_rpm: float = MISSING
    compressor_rpm: float = MISSING
    pump_power_W: float = MISSING
    hx_duty_W: float = MISSING
    generator_output_W: float = MISSING


@dataclass
class ControlState:
    """Control-state inputs."""
    valve_positions: Dict[str, float] = field(default_factory=dict)
    reactivity_input: float = MISSING
    bypass_fraction: float = 0.0
    recirculation_fraction: float = 0.0


@dataclass
class AmbientConditions:
    """Environmental boundary conditions."""
    T_ambient_K: float = 293.15
    P_ambient_Pa: float = ATM
    T_cooling_sink_K: float = 288.15
    condenser_conditions: str = ""


@dataclass
class PlantInitialState:
    """RS-0001 Section [2]: Initial state definition for a plant loop."""
    primary_fluid: str = "He"
    secondary_fluid: str = ""
    fuel_heat_source: str = ""
    T_in_K: float = 600.0
    T_out_K: float = 900.0
    P_in_Pa: float = 70.0 * ATM
    P_out_Pa: float = 30.0 * ATM
    m_dot_kg_s: float = 100.0
    rho_kg_m3: float = MISSING
    composition: Dict[str, float] = field(default_factory=dict)
    phase: str = "gas"
    quality_x: float = MISSING
    machine: MachineState = field(default_factory=MachineState)
    control: ControlState = field(default_factory=ControlState)
    ambient: AmbientConditions = field(default_factory=AmbientConditions)

    def __post_init__(self):
        if not self.composition:
            self.composition = {self.primary_fluid: 1.0}

    def as_dict(self) -> Dict:
        d = {
            "primary_fluid": self.primary_fluid,
            "secondary_fluid": self.secondary_fluid,
            "fuel_heat_source": self.fuel_heat_source,
            "T_in_K": self.T_in_K, "T_out_K": self.T_out_K,
            "P_in_Pa": self.P_in_Pa, "P_out_Pa": self.P_out_Pa,
            "m_dot_kg_s": self.m_dot_kg_s,
            "rho_kg_m3": self.rho_kg_m3,
            "composition": self.composition,
            "phase": self.phase, "quality_x": self.quality_x,
        }
        return d


# =====================================================================
# [3] Core Thermodynamic State -- per-node state table
# =====================================================================

@dataclass
class NodeState:
    """Thermodynamic state at a single plant node."""
    node_id: int = 0
    component: str = ""
    T_K: float = 0.0
    P_Pa: float = 0.0
    rho_kg_m3: float = 0.0
    h_kJ_kg: float = 0.0
    u_kJ_kg: float = 0.0
    s_kJ_kgK: float = 0.0
    phase: str = "gas"
    m_dot_kg_s: float = 0.0
    Z: float = 1.0
    gamma: float = 1.4
    a_m_s: float = 0.0
    notes: str = ""

    def as_dict(self) -> Dict:
        return {
            "node": self.node_id,
            "component": self.component,
            "T_K": round(self.T_K, 2),
            "P_Pa": round(self.P_Pa, 0),
            "P_atm": round(self.P_Pa / ATM, 3),
            "rho_kg_m3": round(self.rho_kg_m3, 4),
            "h_kJ_kg": round(self.h_kJ_kg, 2),
            "u_kJ_kg": round(self.u_kJ_kg, 2),
            "s_kJ_kgK": round(self.s_kJ_kgK, 4),
            "phase": self.phase,
            "m_dot_kg_s": round(self.m_dot_kg_s, 3),
            "Z": round(self.Z, 5),
            "gamma": round(self.gamma, 4),
            "a_m_s": round(self.a_m_s, 1),
            "notes": self.notes,
        }

    def header(self) -> str:
        return ("{:>4} {:>22} {:>9} {:>12} {:>8} {:>10} {:>10} {:>10} "
                "{:>6} {:>8} {:>8} {:>7} {:>8}").format(
            "Node", "Component", "T(K)", "P(Pa)", "P(atm)",
            "rho", "h(kJ/kg)", "u(kJ/kg)", "phase", "mdot", "Z",
            "gamma", "a(m/s)")

    def row(self) -> str:
        return ("{:4d} {:>22} {:9.1f} {:12.0f} {:8.3f} {:10.4f} {:10.2f} {:10.2f} "
                "{:>6} {:8.2f} {:8.5f} {:7.4f} {:8.1f}").format(
            self.node_id, self.component, self.T_K, self.P_Pa,
            self.P_Pa / ATM, self.rho_kg_m3, self.h_kJ_kg, self.u_kJ_kg,
            self.phase, self.m_dot_kg_s, self.Z, self.gamma, self.a_m_s)


class PlantNodeTable:
    """Table of NodeState entries for a complete cycle."""

    def __init__(self):
        self.nodes: List[NodeState] = []

    def add(self, node: NodeState):
        self.nodes.append(node)

    def print_table(self, title: str = "Plant Node State Table"):
        if not self.nodes:
            print("(empty)")
            return
        print("\n" + "=" * 160)
        print("  " + title)
        print("=" * 160)
        print(self.nodes[0].header())
        print("-" * 160)
        for n in self.nodes:
            print(n.row())
        print("=" * 160)

    def as_dicts(self) -> List[Dict]:
        return [n.as_dict() for n in self.nodes]


# =====================================================================
# [4] Plant Gas Analyzer -- multi-node cycle with pipe streaming
# =====================================================================

class PlantGasAnalyzer:
    """
    Computes multi-node thermodynamic state for a power plant cycle.

    Uses gas3 (pykernel.gas) for EOS and transport.
    Streams results through pipe3 (pykernel.pipe) Pipe[dict].

    Usage:
        analyzer = PlantGasAnalyzer.brayton_he_htgr()
        table = analyzer.analyze()
        table.print_table()
        analyzer.stream_to_pipe()
    """

    def __init__(self, metadata: PlantMetadata = None,
                 initial: PlantInitialState = None):
        self.metadata = metadata or PlantMetadata()
        self.initial = initial or PlantInitialState()
        self.table = PlantNodeTable()
        self._gas = _import_gas()
        self._cycle_nodes: List[Tuple[int, str, float, float]] = []
        # (node_id, component_name, T_K, P_Pa)

    def add_node(self, node_id: int, component: str, T_K: float, P_Pa: float):
        """Add a cycle node definition."""
        self._cycle_nodes.append((node_id, component, T_K, P_Pa))

    def _compute_node(self, node_id: int, component: str,
                      T_K: float, P_Pa: float) -> NodeState:
        """Compute full thermodynamic state at one node."""
        gas = self._gas
        fluid = self.initial.primary_fluid
        sp = gas.GAS_DB.get(fluid)
        if sp is None:
            _log.warning("Fluid %s not in GAS_DB, using ideal gas defaults", fluid)
            ns = NodeState(node_id=node_id, component=component,
                           T_K=T_K, P_Pa=P_Pa,
                           m_dot_kg_s=self.initial.m_dot_kg_s,
                           notes="fluid not in DB")
            return ns

        rho = gas.ideal_density(sp, T_K, P_Pa)
        Z = gas.compressibility_factor(sp, T_K, P_Pa)
        a = gas.speed_of_sound(sp, T_K)
        M_kg = sp.molar_mass / 1000.0
        cp_kg = sp.cp_molar / M_kg
        cv_kg = sp.cv_molar / M_kg

        # Enthalpy h = Cp * T  (ideal gas, ref 0 K)
        h = cp_kg * T_K / 1000.0
        # Internal energy u = h - P/rho (ideal gas: u = Cv * T)
        u = cv_kg * T_K / 1000.0
        # Entropy change from reference: ds = Cp ln(T/T_ref) - R/M ln(P/P_ref)
        T_ref, P_ref = 298.0, ATM
        s = (cp_kg * math.log(T_K / T_ref)
             - (R / M_kg) * math.log(P_Pa / P_ref)) / 1000.0

        return NodeState(
            node_id=node_id, component=component,
            T_K=T_K, P_Pa=P_Pa,
            rho_kg_m3=rho, h_kJ_kg=h, u_kJ_kg=u,
            s_kJ_kgK=s, phase="gas",
            m_dot_kg_s=self.initial.m_dot_kg_s,
            Z=Z, gamma=sp.gamma, a_m_s=a,
        )

    def analyze(self) -> PlantNodeTable:
        """Compute state at all defined nodes."""
        self.table = PlantNodeTable()
        for nid, comp, T, P in self._cycle_nodes:
            ns = self._compute_node(nid, comp, T, P)
            self.table.add(ns)
        return self.table

    def stream_to_pipe(self, pipe_name: str = "plant_gas") -> "Pipe":
        """Push all node states through a Pipe[dict]."""
        pipe_mod = _import_pipe()
        pipe = pipe_mod.Pipe(pipe_name)
        for ns in self.table.nodes:
            pipe.push(ns.as_dict(), source=ns.component)
        return pipe

    def print_report(self):
        """Print full RS-0001 report."""
        print(self.metadata.header_text())
        self.table.print_table(
            "Cycle: {} | Fluid: {} | Mode: {}".format(
                self.metadata.system, self.initial.primary_fluid,
                self.metadata.plant_mode.name))

    def cycle_efficiency(self) -> Dict:
        """Estimate cycle thermal efficiency from node enthalpies."""
        nodes = self.table.nodes
        if len(nodes) < 3:
            return {"eta_th": MISSING, "notes": "insufficient nodes"}
        # Simple: eta = 1 - Q_out/Q_in
        # Find max and min enthalpy nodes
        h_max = max(n.h_kJ_kg for n in nodes)
        h_min = min(n.h_kJ_kg for n in nodes)
        T_max = max(n.T_K for n in nodes)
        T_min = min(n.T_K for n in nodes)
        # Carnot limit
        eta_carnot = 1.0 - T_min / T_max if T_max > 0 else 0.0
        # Approximate actual (ideal Brayton: eta = 1 - (P_low/P_high)^((g-1)/g))
        gas = self._gas
        fluid = self.initial.primary_fluid
        sp = gas.GAS_DB.get(fluid)
        gamma = sp.gamma if sp else 1.4
        P_max = max(n.P_Pa for n in nodes)
        P_min = min(n.P_Pa for n in nodes)
        pr = P_max / P_min if P_min > 0 else 1.0
        eta_brayton = 1.0 - (1.0 / pr) ** ((gamma - 1.0) / gamma) if pr > 1 else 0.0
        return {
            "eta_carnot": round(eta_carnot, 4),
            "eta_brayton_ideal": round(eta_brayton, 4),
            "T_max_K": T_max,
            "T_min_K": T_min,
            "pressure_ratio": round(pr, 3),
            "gamma": gamma,
        }

    # =================================================================
    # Preset factories
    # =================================================================

    @staticmethod
    def brayton_he_htgr() -> "PlantGasAnalyzer":
        """Helium HTGR Brayton cycle preset (700 MWth class)."""
        meta = PlantMetadata(
            report_id="PP-HTGR-001",
            system="He closed Brayton HTGR",
            family=PlantFamily.NUCLEAR,
            plant_mode=PlantMode.BASE_LOAD,
            confidence=ConfidenceTier.C2,
        )
        init = PlantInitialState(
            primary_fluid="He",
            fuel_heat_source="TRISO UO2 pebble bed",
            T_in_K=773.15,    # 500 C compressor outlet
            T_out_K=1123.15,  # 850 C reactor outlet
            P_in_Pa=70 * ATM,
            P_out_Pa=28 * ATM,
            m_dot_kg_s=320.0,
        )
        a = PlantGasAnalyzer(meta, init)
        # Define Brayton cycle nodes
        a.add_node(1, "Reactor Outlet",     1123.15,  70 * ATM)
        a.add_node(2, "Turbine Inlet",      1120.0,   69 * ATM)
        a.add_node(3, "Turbine Outlet",     823.15,   28 * ATM)
        a.add_node(4, "Recuperator Hot Out", 573.15,   27.5 * ATM)
        a.add_node(5, "Precooler Outlet",   308.15,   27 * ATM)
        a.add_node(6, "Compressor Outlet",  773.15,   70 * ATM)
        a.add_node(7, "Recuperator Cold Out", 823.15, 69.5 * ATM)
        return a

    @staticmethod
    def brayton_sco2() -> "PlantGasAnalyzer":
        """Supercritical CO2 Brayton cycle preset."""
        meta = PlantMetadata(
            report_id="PP-SCO2-001",
            system="sCO2 recompression Brayton",
            family=PlantFamily.THERMAL,
            plant_mode=PlantMode.BASE_LOAD,
            source_theoretical="EOS / ideal-Brayton / Pitzer Z",
        )
        init = PlantInitialState(
            primary_fluid="CO2",
            fuel_heat_source="concentrated solar / nuclear",
            T_in_K=823.15,   # 550 C
            T_out_K=308.15,  # 35 C (near critical)
            P_in_Pa=250 * ATM,
            P_out_Pa=77 * ATM,
            m_dot_kg_s=1200.0,
        )
        a = PlantGasAnalyzer(meta, init)
        a.add_node(1, "Heater Outlet",       823.15,  250 * ATM)
        a.add_node(2, "Turbine Outlet",      673.15,  77 * ATM)
        a.add_node(3, "HT Recuperator Cold", 573.15,  249 * ATM)
        a.add_node(4, "LT Recuperator Cold", 373.15,  248 * ATM)
        a.add_node(5, "Precooler Outlet",    308.15,  77 * ATM)
        a.add_node(6, "Main Compressor Out",  318.15, 250 * ATM)
        a.add_node(7, "Recompressor Out",    373.15,  250 * ATM)
        return a

    @staticmethod
    def rankine_steam() -> "PlantGasAnalyzer":
        """Simple Rankine steam cycle preset (coal / gas boiler)."""
        meta = PlantMetadata(
            report_id="PP-RANK-001",
            system="subcritical Rankine steam",
            family=PlantFamily.THERMAL,
            phase="mixed",
        )
        init = PlantInitialState(
            primary_fluid="H2O",
            fuel_heat_source="coal boiler",
            T_in_K=811.15,   # 538 C superheat
            T_out_K=318.15,  # 45 C condenser
            P_in_Pa=170 * ATM,
            P_out_Pa=0.1 * ATM,
            m_dot_kg_s=500.0,
            phase="mixed",
        )
        a = PlantGasAnalyzer(meta, init)
        a.add_node(1, "Boiler/SH Outlet",   811.15,   170 * ATM)
        a.add_node(2, "HP Turbine Outlet",   623.15,   40 * ATM)
        a.add_node(3, "Reheat Outlet",       811.15,   38 * ATM)
        a.add_node(4, "LP Turbine Outlet",   318.15,   0.1 * ATM)
        a.add_node(5, "Condenser Outlet",    313.15,   0.08 * ATM)
        a.add_node(6, "Feedpump Outlet",     315.15,   170 * ATM)
        return a

    @staticmethod
    def combined_cycle() -> "PlantGasAnalyzer":
        """Gas turbine combined cycle (GTCC) topping cycle preset."""
        meta = PlantMetadata(
            report_id="PP-GTCC-001",
            system="gas turbine + HRSG combined cycle",
            family=PlantFamily.COMBINED,
        )
        init = PlantInitialState(
            primary_fluid="N2",
            secondary_fluid="H2O",
            fuel_heat_source="natural gas CH4",
            T_in_K=1623.15,  # 1350 C TIT
            T_out_K=873.15,  # 600 C exhaust
            P_in_Pa=30 * ATM,
            P_out_Pa=1.0 * ATM,
            m_dot_kg_s=650.0,
        )
        a = PlantGasAnalyzer(meta, init)
        # Topping (gas) cycle nodes
        a.add_node(1, "Compressor Inlet",    293.15,   1.0 * ATM)
        a.add_node(2, "Compressor Outlet",   673.15,   30 * ATM)
        a.add_node(3, "Combustor Outlet",    1623.15,  29 * ATM)
        a.add_node(4, "GT Turbine Outlet",   873.15,   1.05 * ATM)
        a.add_node(5, "HRSG Exhaust",        423.15,   1.02 * ATM)
        return a

    @staticmethod
    def msr_flibe_loop() -> "PlantGasAnalyzer":
        """Molten salt reactor FLiBe primary loop (conceptual)."""
        meta = PlantMetadata(
            report_id="PP-MSR-001",
            system="FLiBe primary + He intermediate",
            family=PlantFamily.NUCLEAR,
            phase="liquid",
            scale="loop",
            confidence=ConfidenceTier.C3,
        )
        # Use He for the intermediate (gas) loop nodes
        init = PlantInitialState(
            primary_fluid="He",
            secondary_fluid="",
            fuel_heat_source="FLiBe-UF4 fuel salt",
            T_in_K=923.15,   # 650 C
            T_out_K=973.15,  # 700 C
            P_in_Pa=5 * ATM,
            P_out_Pa=4.5 * ATM,
            m_dot_kg_s=200.0,
        )
        a = PlantGasAnalyzer(meta, init)
        a.add_node(1, "IHX Salt-Side Out",  973.15,   5.0 * ATM)
        a.add_node(2, "IHX He-Side Out",    943.15,   5.0 * ATM)
        a.add_node(3, "He Turbine Outlet",  773.15,   2.0 * ATM)
        a.add_node(4, "Recuperator Cold Out", 873.15, 4.8 * ATM)
        a.add_node(5, "He Compressor Out",  923.15,   5.0 * ATM)
        return a


# =====================================================================
# Quick-run entry point
# =====================================================================

def run_all_presets():
    """Compute and print all preset cycle analyses."""
    presets = [
        ("He HTGR Brayton",   PlantGasAnalyzer.brayton_he_htgr),
        ("sCO2 Brayton",      PlantGasAnalyzer.brayton_sco2),
        ("Rankine Steam",     PlantGasAnalyzer.rankine_steam),
        ("Combined Cycle",    PlantGasAnalyzer.combined_cycle),
        ("MSR FLiBe Loop",    PlantGasAnalyzer.msr_flibe_loop),
    ]
    for name, factory in presets:
        print("\n" + "#" * 80)
        print("#  PRESET: " + name)
        print("#" * 80)
        analyzer = factory()
        table = analyzer.analyze()
        analyzer.print_report()
        eff = analyzer.cycle_efficiency()
        print("\n  Cycle efficiency estimates:")
        for k, v in eff.items():
            print("    {:25s}: {}".format(k, v))
        # Stream through pipe
        pipe = analyzer.stream_to_pipe("plant_" + name.replace(" ", "_"))
        print("  Pipe stats: {}".format(pipe.stats()))


if __name__ == "__main__":
    run_all_presets()
