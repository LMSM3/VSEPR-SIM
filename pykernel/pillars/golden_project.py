"""
Golden Project — VSEPR-SIM Universal Materials Discovery Layer
===============================================================

Pillar E of the VSEPR-SIM Five Pillars architecture.

This is the unified discovery engine that spans:
  - Thermophysical properties (SteamTables++)
  - Structure generation (Crystal Discovery)
  - Material ranking (Metals & Macros)
  - Smart adaptive exploration (SmartSampling)

Purpose:
  - Ingest material definitions
  - Generate multi-scale candidates
  - Score across thermo / structure / macro targets
  - Adaptively sample toward mission objectives
  - Export ranked discoveries
  - Produce consolidated research reports

Internal codename: Discovery Atlas

VSEPR-SIM Five Pillars v1.0
"""

from __future__ import annotations

import os
import json
import csv
import time
import hashlib
from dataclasses import dataclass, field, asdict
from typing import Dict, List, Tuple, Optional, Any
from datetime import datetime
from enum import Enum, auto


# ============================================================================
# Discovery mission types
# ============================================================================

class MissionType(Enum):
    THERMO_EXPLORATION = "thermo_exploration"
    CRYSTAL_SEARCH = "crystal_search"
    ALLOY_OPTIMIZATION = "alloy_optimization"
    PROCESS_EVALUATION = "process_evaluation"
    FULL_DISCOVERY = "full_discovery"


@dataclass
class DiscoveryMission:
    """A unified discovery mission specification."""
    mission_id: str
    name: str
    mission_type: MissionType
    description: str
    targets: Dict[str, Any] = field(default_factory=dict)
    constraints: Dict[str, Tuple[float, float]] = field(default_factory=dict)
    materials: List[str] = field(default_factory=list)
    seed: int = 42
    max_iterations: int = 5
    samples_per_iteration: int = 50
    timestamp: str = ""

    def __post_init__(self):
        if not self.timestamp:
            self.timestamp = datetime.now().isoformat()


@dataclass
class DiscoveryResult:
    """Result from a discovery run."""
    mission_id: str
    pillar: str
    material: str
    score: float
    rank: int = 0
    properties: Dict[str, Any] = field(default_factory=dict)
    provenance: str = ""
    iteration: int = 0


# ============================================================================
# Cross-pillar runner
# ============================================================================

class DiscoveryAtlas:
    """The Golden Project: unified discovery engine across all pillars."""

    def __init__(self, output_dir: str = "out/pillars/golden_project"):
        self.output_dir = output_dir
        os.makedirs(output_dir, exist_ok=True)
        self.results: List[DiscoveryResult] = []
        self.missions: List[DiscoveryMission] = []

    def run_mission(self, mission: DiscoveryMission) -> List[DiscoveryResult]:
        """Execute a discovery mission across relevant pillars."""
        self.missions.append(mission)
        results = []

        if mission.mission_type in (MissionType.THERMO_EXPLORATION,
                                     MissionType.FULL_DISCOVERY):
            results.extend(self._run_thermo(mission))

        if mission.mission_type in (MissionType.CRYSTAL_SEARCH,
                                     MissionType.FULL_DISCOVERY):
            results.extend(self._run_crystal(mission))

        if mission.mission_type in (MissionType.ALLOY_OPTIMIZATION,
                                     MissionType.FULL_DISCOVERY):
            results.extend(self._run_alloy(mission))

        if mission.mission_type in (MissionType.PROCESS_EVALUATION,
                                     MissionType.FULL_DISCOVERY):
            results.extend(self._run_process(mission))

        # Sort by score
        results.sort(key=lambda r: r.score, reverse=True)
        for i, r in enumerate(results):
            r.rank = i + 1

        self.results.extend(results)
        return results

    def _run_thermo(self, mission: DiscoveryMission) -> List[DiscoveryResult]:
        from .steam_tables import (
            get_all_materials, compute_state, compute_saturation_line,
            AdaptiveRegionSampler)
        from .smart_sampling import create_steam_sampler

        results = []
        materials = mission.materials or list(get_all_materials().keys())

        for formula in materials:
            sampler = create_steam_sampler(formula, mission.seed)
            candidates = sampler.generate_candidates(mission.samples_per_iteration)
            ranked = sampler.rank_candidates(candidates)
            top = sampler.select_top(ranked, min(20, len(ranked)))

            for s in top:
                T = s.coordinates.get("T", 300)
                P = s.coordinates.get("P", 101325)
                sp = compute_state(formula, T, P)

                props = {
                    "T": sp.T, "P": sp.P, "phase": sp.phase.name,
                    "h": sp.h, "s": sp.s, "v": sp.v, "rho": sp.rho,
                    "c_p": sp.c_p, "Pr": sp.Pr, "Z": sp.Z,
                    "sampling_score": s.total,
                    "novelty": s.novelty, "uncertainty": s.uncertainty
                }

                results.append(DiscoveryResult(
                    mission_id=mission.mission_id,
                    pillar="SteamTables++",
                    material=formula,
                    score=s.total,
                    properties=props,
                    provenance=sp.provenance))

        return results

    def _run_crystal(self, mission: DiscoveryMission) -> List[DiscoveryResult]:
        from .crystal_discovery import (
            CrystalCatalog, CrystalDiscoveryEngine)

        results = []
        catalog = CrystalCatalog()
        engine = CrystalDiscoveryEngine(catalog, mission.seed)

        # Score all catalog entries
        for entry in catalog.entries:
            score = entry.stability_score
            if entry.formation_energy < 0:
                score += 10
            if entry.bandgap > 0:
                score += 5

            props = {
                "formula": entry.formula, "name": entry.name,
                "crystal_system": entry.crystal_system.value,
                "space_group": entry.space_group_symbol,
                "density": entry.density,
                "formation_energy": entry.formation_energy,
                "bandgap": entry.bandgap,
                "n_atoms": entry.n_atoms,
                "a": entry.lattice.a,
                "volume": entry.lattice.volume
            }

            results.append(DiscoveryResult(
                mission_id=mission.mission_id,
                pillar="Crystal Discovery",
                material=entry.formula,
                score=score,
                properties=props,
                provenance=entry.provenance))

        # Run substitution scans for selected bases
        for base in ["NaCl", "SrTiO3", "Al"]:
            base_entries = [e for e in catalog.entries if e.formula == base]
            if base_entries:
                entry = base_entries[0]
                for sym in entry.atoms:
                    subs = engine.substitution_scan(
                        base, sym.symbol,
                        ["Li", "K", "Ca", "Cu", "Ag", "Mg"])
                    for s in subs:
                        results.append(DiscoveryResult(
                            mission_id=mission.mission_id,
                            pillar="Crystal Discovery",
                            material=s.formula,
                            score=s.stability_score,
                            properties={"name": s.name,
                                       "stability": s.stability.value},
                            provenance=s.provenance))
                    break

        return results

    def _run_alloy(self, mission: DiscoveryMission) -> List[DiscoveryResult]:
        from .metals_macros import (
            run_mission as run_metal_mission, MissionSpec, MissionObjective,
            AlloyEstimator, _METAL_DB)

        results = []

        # Score all metals against mission objectives
        objectives = [MissionObjective.HIGH_STRENGTH,
                      MissionObjective.HIGH_TEMPERATURE,
                      MissionObjective.LOW_COST]
        if "objectives" in mission.targets:
            objectives = [MissionObjective(o) for o in mission.targets["objectives"]
                         if hasattr(MissionObjective, o.upper())]

        spec = MissionSpec(objectives=objectives, constraints=mission.constraints)
        ranked = run_metal_mission(spec, top_n=30)

        for rc in ranked:
            props = {
                "density": rc.material.density,
                "elastic_modulus": rc.material.elastic_modulus,
                "yield_strength": rc.material.yield_strength,
                "UTS": rc.material.ultimate_strength,
                "thermal_conductivity": rc.material.thermal_conductivity,
                "melting_point": rc.material.melting_point,
                "corrosion_class": rc.material.corrosion_class,
                "subscores": rc.subscores
            }

            results.append(DiscoveryResult(
                mission_id=mission.mission_id,
                pillar="Metals & Macros",
                material=rc.material.formula,
                score=rc.score,
                properties=props,
                provenance=rc.material.source))

        # Generate alloy estimates
        pairs = [
            {"Cu": 0.7, "Zn": 0.3},
            {"Cu": 0.9, "Sn": 0.1},
            {"Fe": 0.7, "Cr": 0.2, "Ni": 0.1},
            {"Ti": 0.9, "Al": 0.06, "V": 0.04},
            {"Ni": 0.7, "Cr": 0.15, "Fe": 0.15},
            {"Al": 0.95, "Cu": 0.03, "Mg": 0.02},
            {"Co": 0.6, "Cr": 0.25, "W": 0.15},
        ]
        for comp in pairs:
            alloy = AlloyEstimator.rule_of_mixtures(comp)
            if alloy:
                rc = run_metal_mission(spec, top_n=1)
                results.append(DiscoveryResult(
                    mission_id=mission.mission_id,
                    pillar="Metals & Macros",
                    material=alloy.formula,
                    score=50.0,
                    properties=asdict(alloy),
                    provenance="ROM alloy estimate"))

        return results

    def _run_process(self, mission: DiscoveryMission) -> List[DiscoveryResult]:
        from .metals_macros import simulate_heating, _METAL_DB

        results = []
        materials = mission.materials or ["Fe", "Al", "Ti", "Cu", "Ni", "W"]

        for sym in materials:
            if sym not in _METAL_DB:
                continue
            sim = simulate_heating(sym, 300, 2500, 30)
            for step in sim:
                results.append(DiscoveryResult(
                    mission_id=mission.mission_id,
                    pillar="Process Evaluation",
                    material=sym,
                    score=step.Cp_at_T / 10.0,
                    properties={
                        "T": step.T, "Cp": step.Cp_at_T,
                        "k": step.k_at_T, "phase": step.phase
                    },
                    provenance=f"heating_sim step"))

        return results

    # ======================================================================
    # Report generation
    # ======================================================================

    def generate_master_report_md(self) -> str:
        lines = []
        lines.append("# VSEPR-SIM Discovery Atlas — Master Report\n")
        lines.append(f"**Generated**: {datetime.now().isoformat()}")
        lines.append(f"**Total discoveries**: {len(self.results)}")
        lines.append(f"**Missions executed**: {len(self.missions)}\n")

        # Architecture overview
        lines.append("## Platform Architecture\n")
        lines.append("```")
        lines.append("VSEPR-SIM Platform")
        lines.append("|")
        lines.append("+-- Pillar A: SteamTables++")
        lines.append("|   +-- Reference data (S1: 9 materials)")
        lines.append("|   +-- EOS fits (S2: 21 materials)")
        lines.append("|   +-- Surrogate (S3: 10 materials)")
        lines.append("|   +-- Phase classifier")
        lines.append("|   +-- Adaptive region sampler")
        lines.append("|   +-- Pipe friction engine")
        lines.append("|")
        lines.append("+-- Pillar B: Crystal Discovery")
        lines.append("|   +-- 37 prototype lattices")
        lines.append("|   +-- Organic / inorganic split")
        lines.append("|   +-- Defect / polymorph engine")
        lines.append("|   +-- Crystal catalog")
        lines.append("|")
        lines.append("+-- Pillar C: Metals & Macros")
        lines.append("|   +-- 35+ metals and alloys")
        lines.append("|   +-- Alloy composition engine (ROM)")
        lines.append("|   +-- Purpose-driven selection (10 objectives)")
        lines.append("|   +-- Heating process simulation")
        lines.append("|")
        lines.append("+-- Pillar D: SmartSampling")
        lines.append("|   +-- 6-component scoring equation")
        lines.append("|   +-- Latin Hypercube generator")
        lines.append("|   +-- Adaptive refinement")
        lines.append("|   +-- Stochastic stress testing")
        lines.append("|")
        lines.append("+-- Pillar E: Golden Project (this)")
        lines.append("    +-- Cross-pillar discovery missions")
        lines.append("    +-- Unified ranking")
        lines.append("    +-- Export pipelines")
        lines.append("    +-- Consolidated reporting")
        lines.append("```\n")

        # Per-pillar summaries
        for pillar in ["SteamTables++", "Crystal Discovery",
                        "Metals & Macros", "Process Evaluation"]:
            pillar_results = [r for r in self.results if r.pillar == pillar]
            if not pillar_results:
                continue

            lines.append(f"## {pillar}\n")
            lines.append(f"**Results**: {len(pillar_results)}")
            if pillar_results:
                scores = [r.score for r in pillar_results]
                lines.append(f"**Score range**: {min(scores):.1f} — {max(scores):.1f}")
                materials = sorted(set(r.material for r in pillar_results))
                lines.append(f"**Materials**: {', '.join(materials[:20])}\n")

                lines.append("### Top Results\n")
                lines.append("| Rank | Material | Score | Key Properties |")
                lines.append("|------|----------|-------|----------------|")
                top = sorted(pillar_results, key=lambda r: r.score, reverse=True)[:20]
                for r in top:
                    key_props = ", ".join(f"{k}={v}" for k, v in
                                        list(r.properties.items())[:4]
                                        if not isinstance(v, dict))
                    lines.append(f"| {r.rank} | {r.material} "
                                f"| {r.score:.1f} | {key_props} |")
            lines.append("")

        # Mission summaries
        if self.missions:
            lines.append("## Missions Executed\n")
            for m in self.missions:
                lines.append(f"### {m.name}\n")
                lines.append(f"- **Type**: {m.mission_type.value}")
                lines.append(f"- **Materials**: {', '.join(m.materials[:10]) if m.materials else 'all'}")
                lines.append(f"- **Seed**: {m.seed}")
                lines.append(f"- **Timestamp**: {m.timestamp}")
                n_results = len([r for r in self.results
                                if r.mission_id == m.mission_id])
                lines.append(f"- **Results**: {n_results}\n")

        return "\n".join(lines)

    def export_all_csv(self):
        path = os.path.join(self.output_dir, "discovery_results.csv")
        with open(path, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["mission_id", "pillar", "material", "score", "rank",
                         "provenance", "properties_json"])
            for r in sorted(self.results, key=lambda x: (-x.score, x.material)):
                w.writerow([r.mission_id, r.pillar, r.material,
                           f"{r.score:.4f}", r.rank, r.provenance,
                           json.dumps(r.properties)])
        return path
