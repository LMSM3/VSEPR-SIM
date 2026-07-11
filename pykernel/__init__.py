"""
PyKernel — Python bridge to VSEPR-SIM atomistic engine.

Walk-away improvement system:
  Phase A: Polynomial fits (11-15 layer) + eigen counters, GPU execution
  Phase B: Backend finalization, Python library packaging
  Phase C: Closed-loop autonomous improvement (flagging, reporting, simulation)

Benchmark & piping infrastructure:
  Pipe[T]:  Typed data pipes connecting stages
  TestForest: Tree-structured test discovery across C++ and Python
  BenchmarkOrchestrator: GPU vs CPU measurement + scaling analysis

Anti-black-box: every coefficient, every eigenvalue, every fit residual
is explicitly logged and inspectable.
"""

__version__ = "3.0.1"
__project__ = "vsepr-sim"

from pykernel.poly_fitter import PolyFitter, PolyFitResult
from pykernel.eigen_counter import EigenCounter, EigenSnapshot
from pykernel.gpu_bridge import GPUBridge
from pykernel.runner import ContinuousRunner
from pykernel.improvement_loop import ImprovementLoop
from pykernel.pipe import Pipe, PipeRecord, Transform, FanOut, Accumulator, CSVSink, JSONSink
from pykernel.test_forest import TestForest, TestCase, TestResult, DomainSummary
from pykernel.benchmark import BenchmarkOrchestrator, BenchmarkRow, BenchmarkSummary
from pykernel.live_viewer import LiveViewer, XYZAFrame, XYZAFrameWriter, SyntheticDriver, LiveAnalysis
from pykernel.crystal_tui import (
    CrystalTUI, TUIConfig, TUISnapshot, LatticeInfo,
    WindParticle, WindState, Vec3 as TUIVec3, FrameBuffer,
)
from pykernel.step_parser import (
    parse_step, parse_step_file, parse_step_string,
    StepAssembly, NamedPart, CartesianPoint, Direction, Axis2Placement3D,
)
from pykernel.metallic_cp import (
    MetalRecord, METAL_DB, lookup_metal, all_metals,
    debye_cv, electronic_cv, dulong_petit, compute_cp, compute_cp_curve, CpResult,
)
from pykernel.heating_sim import (
    HeatSchedule, PartThermalConfig, PartThermalResult, ThermalTimeStep,
    HeatingSimConfig, HeatingSimulation, quick_heat_single, heat_assembly,
)
from pykernel.thermo_pipe import (
    ThermoCheck, ValidationReport, ThermoValidator, XLSXSink,
    BatchJob, BatchResult, ThermoRunner,
    run_step_heating, validate_thermo_db,
)
from pykernel.metal_sweep import (
    MetalSweepRunner, ElementInfo, SweepFrame,
    load_elements, synthesise_metal_record, build_fcc_lattice, render_metal,
)
from pykernel.chaos_amplitude import (
    ScaleWeights, AmplitudeBin, AmplitudeScale, ChaosResult,
    ChaosSweepSummary, SensitivityResult,
    build_amplitude_scale, compute_chaos, summarise_chaos, weight_sensitivity,
    score_displacement, score_force, score_thermal, score_anisotropy,
    D_MAX_FRAC, A_MIN, A_MAX, N_BINS,
)
from pykernel.metal_graphs import (
    GraphReport, generate_all_graphs,
    StressStrainPoint, compute_stress_strain, plot_stress_strain_rainbow,
    BrittlenessRecord, compute_brittleness, plot_brittleness,
    XRDPeak, compute_xrd_pattern, plot_xrd_rainbow,
    ToughnessPoint, compute_toughness_history, plot_toughness_over_time,
    plot_chaos_dashboard, plot_lattice_projection, export_sweep_xyza,
)
from pykernel.flower_health import (
    HealthBand, HealthTrend, HealthDeduction, NodeMetrics, HealthScore,
    compute_health, classify_health, band_name, trend_name,
    Colour as HealthColour, FlowerTheme, build_theme,
    FlowerPalette, resolve_palette, apply_trend,
    FlowerAnim, FlowerAnimState,
    pick_animation, advance_animation, apply_animation,
    render_flower, render_deductions, health_notification,
)
from pykernel.env_particle import (
    EnvParticleKind, EnvParticle, BeadProps, PlantEnvResponse,
    sun_deposition, wind_force, drying_kernel, interact_env_particle,
    RootLocalState, RootChaosCoeffs, root_chaos_factor,
    PolySegment, PiecewiseRootPoly, default_root_poly,
    RootGrowthLimits, root_growth_modifier,
    LeafGateCoeffs, LeafLocalState,
    leaf_generation_gate, leaf_gate_signal, leaf_expansion_rate,
    EnvEnergyTerms, accumulate_env, advance_particle,
    shade_factor, kind_name as env_kind_name,
)
from pykernel.zmq_producer import (
    MolecularPublisher, parse_xyz_file, infer_bonds,
    CPK_COLORS as ZMQ_CPK_COLORS, VDW_RADII as ZMQ_VDW_RADII,
)
from pykernel.cartoon_renderer import (
    CartoonMoleculeRenderer, ZMQReceiver,
)
from pykernel.element_data import (
    cpk_color, covalent_radius, vdw_radius, atomic_mass, is_archived,
    load_xyz, molecules_dir, get_element,
    SYMBOLS, SYMBOL_TO_Z,
    COVALENT_RADII, VDW_RADII, CPK_COLORS, ATOMIC_MASSES,
    ARCHIVE_Z_MAX, ELEMENT_ARCHIVE, ARCHIVE_BY_SYMBOL, ElementRecord,
)

__all__ = [
    "PolyFitter",
    "PolyFitResult",
    "EigenCounter",
    "EigenSnapshot",
    "GPUBridge",
    "ContinuousRunner",
    "ImprovementLoop",
    "Pipe",
    "PipeRecord",
    "Transform",
    "FanOut",
    "Accumulator",
    "CSVSink",
    "JSONSink",
    "TestForest",
    "TestCase",
    "TestResult",
    "DomainSummary",
    "BenchmarkOrchestrator",
    "BenchmarkRow",
    "BenchmarkSummary",
    "LiveViewer",
    "XYZAFrame",
    "XYZAFrameWriter",
    "SyntheticDriver",
    "LiveAnalysis",
    "CrystalTUI",
    "TUIConfig",
    "TUISnapshot",
    "LatticeInfo",
    "WindParticle",
    "WindState",
    "TUIVec3",
    "FrameBuffer",
    # STEP parser
    "parse_step",
    "parse_step_file",
    "parse_step_string",
    "StepAssembly",
    "NamedPart",
    "CartesianPoint",
    "Direction",
    "Axis2Placement3D",
    # Metallic c_p
    "MetalRecord",
    "METAL_DB",
    "lookup_metal",
    "all_metals",
    "debye_cv",
    "electronic_cv",
    "dulong_petit",
    "compute_cp",
    "compute_cp_curve",
    "CpResult",
    # Heating simulation
    "HeatSchedule",
    "PartThermalConfig",
    "PartThermalResult",
    "ThermalTimeStep",
    "HeatingSimConfig",
    "HeatingSimulation",
    "quick_heat_single",
    "heat_assembly",
    # Thermo piping / batch
    "ThermoCheck",
    "ValidationReport",
    "ThermoValidator",
    "XLSXSink",
    "BatchJob",
    "BatchResult",
    "ThermoRunner",
    "run_step_heating",
    "validate_thermo_db",
    # Metal sweep
    "MetalSweepRunner",
    "ElementInfo",
    "SweepFrame",
    "load_elements",
    "synthesise_metal_record",
    "build_fcc_lattice",
    "render_metal",
    # Chaos / Amplitude / Scale Weights
    "ScaleWeights",
    "AmplitudeBin",
    "AmplitudeScale",
    "ChaosResult",
    "ChaosSweepSummary",
    "SensitivityResult",
    "build_amplitude_scale",
    "compute_chaos",
    "summarise_chaos",
    "weight_sensitivity",
    "score_displacement",
    "score_force",
    "score_thermal",
    "score_anisotropy",
    "D_MAX_FRAC",
    "A_MIN",
    "A_MAX",
    "N_BINS",
    # Metal graphs
    "GraphReport",
    "generate_all_graphs",
    "StressStrainPoint",
    "compute_stress_strain",
    "plot_stress_strain_rainbow",
    "BrittlenessRecord",
    "compute_brittleness",
    "plot_brittleness",
    "XRDPeak",
    "compute_xrd_pattern",
    "plot_xrd_rainbow",
    "ToughnessPoint",
    "compute_toughness_history",
    "plot_toughness_over_time",
    "plot_chaos_dashboard",
    "plot_lattice_projection",
    "export_sweep_xyza",
    # Flower health
    "HealthBand",
    "HealthTrend",
    "HealthDeduction",
    "NodeMetrics",
    "HealthScore",
    "compute_health",
    "classify_health",
    "band_name",
    "trend_name",
    "HealthColour",
    "FlowerTheme",
    "build_theme",
    "FlowerPalette",
    "resolve_palette",
    "apply_trend",
    "FlowerAnim",
    "FlowerAnimState",
    "pick_animation",
    "advance_animation",
    "apply_animation",
    "render_flower",
    "render_deductions",
    "health_notification",
    # Environmental particle extension
    "EnvParticleKind",
    "EnvParticle",
    "BeadProps",
    "PlantEnvResponse",
    "sun_deposition",
    "wind_force",
    "drying_kernel",
    "interact_env_particle",
    "RootLocalState",
    "RootChaosCoeffs",
    "root_chaos_factor",
    "PolySegment",
    "PiecewiseRootPoly",
    "default_root_poly",
    "RootGrowthLimits",
    "root_growth_modifier",
    "LeafGateCoeffs",
    "LeafLocalState",
    "leaf_generation_gate",
    "leaf_gate_signal",
    "leaf_expansion_rate",
    "EnvEnergyTerms",
    "accumulate_env",
    "advance_particle",
    "shade_factor",
    "env_kind_name",
    # ZMQ molecular frame publisher
    "MolecularPublisher",
    "parse_xyz_file",
    "infer_bonds",
    "ZMQ_CPK_COLORS",
    "ZMQ_VDW_RADII",
    # Cartoon-3D molecular renderer
    "CartoonMoleculeRenderer",
    "ZMQReceiver",
    # Element data (parsed from C++ kernel headers)
    "cpk_color",
    "covalent_radius",
    "vdw_radius",
    "atomic_mass",
    "is_archived",
    "load_xyz",
    "molecules_dir",
    "get_element",
    "SYMBOLS",
    "SYMBOL_TO_Z",
    "COVALENT_RADII",
    "VDW_RADII",
    "CPK_COLORS",
    "ATOMIC_MASSES",
    "ARCHIVE_Z_MAX",
    "ELEMENT_ARCHIVE",
    "ARCHIVE_BY_SYMBOL",
    "ElementRecord",
]
