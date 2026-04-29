#!/usr/bin/env python3
"""
VSEPR-SIM Five Pillars — Master Report Generator
==================================================

Runs all five pillars and produces consolidated reports:
  - SteamTables++ Report (saturation tables, state grids, pipe friction)
  - Crystal Discovery Report (catalog, substitutions, perturbations)
  - Metals & Macros Report (property tables, missions, heating sims)
  - SmartSampling Report (scoring breakdowns, distributions)
  - Golden Project Master Report (unified architecture + cross-pillar results)

Output: out/pillars/ directory with .md and .csv files per pillar.

Usage:
    python tools/run_five_pillars.py [--output DIR] [--seed N] [--verbose]
"""

from __future__ import annotations

import os
import sys
import time
import json

# Add pykernel/ to path so 'pillars' is importable as a top-level package
# without triggering pykernel/__init__.py (which has heavy deps).
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_PROJECT_ROOT = os.path.dirname(_SCRIPT_DIR)
_PYKERNEL_DIR = os.path.join(_PROJECT_ROOT, "pykernel")
for _p in (_PROJECT_ROOT, _PYKERNEL_DIR):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from pillars.steam_tables import (
    get_all_materials, materials_by_tier, Tier,
    generate_saturation_table_md, generate_state_table_md,
    generate_pipe_friction_table_md,
    export_saturation_csv, export_state_grid_csv,
    compute_state, compute_saturation_line,
    AdaptiveRegionSampler, compute_pipe_friction,
)
from pillars.crystal_discovery import (
    CrystalCatalog, CrystalDiscoveryEngine, CrystalClass,
    generate_crystal_catalog_md, generate_crystal_discovery_md,
    export_catalog_csv,
)
from pillars.metals_macros import (
    generate_metals_table_md, generate_mission_report_md,
    generate_heating_report_md, export_metals_csv,
    MissionSpec, MissionObjective, AlloyEstimator,
    simulate_heating, _METAL_DB,
)
from pillars.smart_sampling import (
    SmartSampler, SamplingWeights,
    create_steam_sampler, create_crystal_sampler, create_alloy_sampler,
    generate_sampling_report_md,
)
from pillars.golden_project import (
    DiscoveryAtlas, DiscoveryMission, MissionType,
)


def _banner(title: str):
    w = max(60, len(title) + 8)
    print()
    print("=" * w)
    print(f"    {title}")
    print("=" * w)
    print()


def _write(path: str, content: str):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"  -> {path}  ({len(content):,} chars)")


# ============================================================================
# Pillar A: SteamTables++
# ============================================================================

def run_pillar_A(out_dir: str, seed: int = 42):
    _banner("Pillar A: SteamTables++ — Multi-Material Thermophysical Atlas")

    report_lines = []
    report_lines.append("# SteamTables++ Report\n")
    report_lines.append("## VSEPR-SIM Pillar A — Multi-Material Thermophysical Atlas\n")
    report_lines.append(f"**Generated**: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")

    # Material coverage summary
    all_mats = get_all_materials()
    s1 = materials_by_tier(Tier.S1)
    s2 = materials_by_tier(Tier.S2)
    s3 = materials_by_tier(Tier.S3)

    report_lines.append("## Material Coverage Tiers\n")
    report_lines.append(f"**Total materials**: {len(all_mats)}\n")
    report_lines.append("### Tier S1: Exact / Reference-Backed\n")
    for f, m in sorted(s1.items()):
        report_lines.append(f"- **{m.name}** ({m.formula}) — {m.source}")
    report_lines.append(f"\n*{len(s1)} materials*\n")

    report_lines.append("### Tier S2: EOS-Derived Approximation\n")
    for f, m in sorted(s2.items()):
        report_lines.append(f"- **{m.name}** ({m.formula}) — {m.source}")
    report_lines.append(f"\n*{len(s2)} materials*\n")

    report_lines.append("### Tier S3: Surrogate / Fitted\n")
    for f, m in sorted(s3.items()):
        report_lines.append(f"- **{m.name}** ({m.formula}) — Tags: {', '.join(m.tags)}")
    report_lines.append(f"\n*{len(s3)} materials*\n")

    # Data schema
    report_lines.append("## Data Schema\n")
    report_lines.append("### StatePoint Fields\n")
    report_lines.append("| Field | Unit | Description |")
    report_lines.append("|-------|------|-------------|")
    report_lines.append("| T | K | Temperature |")
    report_lines.append("| P | Pa | Pressure |")
    report_lines.append("| phase | — | SUBCOOLED_LIQUID / WET_MIXTURE / SUPERHEATED_VAPOUR / SUPERCRITICAL |")
    report_lines.append("| h | J/kg | Specific enthalpy |")
    report_lines.append("| s | J/(kg·K) | Specific entropy |")
    report_lines.append("| v | m³/kg | Specific volume |")
    report_lines.append("| rho | kg/m³ | Density |")
    report_lines.append("| x | — | Quality (0-1 for wet, -1 for single phase) |")
    report_lines.append("| c_p | J/(kg·K) | Isobaric heat capacity |")
    report_lines.append("| c_v | J/(kg·K) | Isochoric heat capacity |")
    report_lines.append("| mu | Pa·s | Dynamic viscosity |")
    report_lines.append("| k_thermal | W/(m·K) | Thermal conductivity |")
    report_lines.append("| Pr | — | Prandtl number |")
    report_lines.append("| Z | — | Compressibility factor |")
    report_lines.append("")

    # Saturation tables for all S1 materials
    report_lines.append("## Saturation Tables\n")
    n_state_points = 0
    for formula in sorted(s1.keys()):
        print(f"  Saturation: {formula} ...", end=" ", flush=True)
        md = generate_saturation_table_md(formula, n_points=60)
        report_lines.append(md)
        report_lines.append("")
        csv_path = os.path.join(out_dir, f"sat_{formula}.csv")
        export_saturation_csv(formula, csv_path, n_points=100)
        n_state_points += 100
        print("OK")

    # State tables for key materials
    report_lines.append("## State Properties (multi-pressure)\n")
    for formula in ["H2O", "CO2", "NH3", "R134a", "CH4"]:
        if formula in all_mats:
            print(f"  State grid: {formula} ...", end=" ", flush=True)
            mat = all_mats[formula]
            Tc = mat.critical.T_c
            md = generate_state_table_md(formula, T_range=(Tc*0.4, Tc*1.2))
            report_lines.append(md)
            report_lines.append("")
            csv_path = os.path.join(out_dir, f"state_{formula}.csv")
            export_state_grid_csv(formula, csv_path, n_T=40, n_P=40)
            n_state_points += 1600
            print("OK")

    # Extended S2 state grids
    report_lines.append("## Extended Material State Grids (S2 tier)\n")
    for formula in sorted(s2.keys()):
        print(f"  S2 state grid: {formula} ...", end=" ", flush=True)
        mat = s2[formula]
        Tc = mat.critical.T_c
        md = generate_state_table_md(formula, T_range=(Tc*0.4, Tc*1.2))
        report_lines.append(md)
        report_lines.append("")
        csv_path = os.path.join(out_dir, f"state_{formula}.csv")
        export_state_grid_csv(formula, csv_path, n_T=25, n_P=25)
        n_state_points += 625
        print("OK")

    # Pipe friction tables
    report_lines.append("## Pipe Friction Tables\n")
    for formula in sorted(s1.keys()):
        if formula in all_mats:
            print(f"  Pipe friction: {formula} ...", end=" ", flush=True)
            mat = all_mats[formula]
            md = generate_pipe_friction_table_md(formula,
                T=mat.critical.T_c * 0.7, P=mat.critical.P_c * 0.3)
            report_lines.append(md)
            report_lines.append("")
            print("OK")

    # S2 pipe friction
    for formula in sorted(list(s2.keys())[:8]):
        if formula in all_mats:
            print(f"  Pipe friction S2: {formula} ...", end=" ", flush=True)
            mat = all_mats[formula]
            md = generate_pipe_friction_table_md(formula,
                T=mat.critical.T_c * 0.7, P=mat.critical.P_c * 0.3)
            report_lines.append(md)
            report_lines.append("")
            print("OK")

    # S3 state grids for surrogate materials
    report_lines.append("## Surrogate Material State Grids (S3 tier)\n")
    for formula in sorted(s3.keys()):
        print(f"  S3 state grid: {formula} ...", end=" ", flush=True)
        mat = s3[formula]
        Tc = mat.critical.T_c
        md = generate_state_table_md(formula, T_range=(Tc*0.4, Tc*1.1))
        report_lines.append(md)
        report_lines.append("")
        csv_path = os.path.join(out_dir, f"state_{formula}.csv")
        export_state_grid_csv(formula, csv_path, n_T=20, n_P=20)
        n_state_points += 400
        print("OK")

    # Adaptive sampling report
    report_lines.append("## Adaptive Sampling\n")
    for formula in ["H2O", "CO2", "NH3", "R134a", "CH4", "N2"]:
        print(f"  Adaptive sampling: {formula} ...", end=" ", flush=True)
        sampler_obj = AdaptiveRegionSampler(formula, seed)
        pts = sampler_obj.sample_PT_grid(25, 25, refine_near_sat=True)
        n_state_points += len(pts)
        phases = {}
        for p in pts:
            ph = p.phase.name
            phases[ph] = phases.get(ph, 0) + 1
        report_lines.append(f"### Adaptive Sample: {formula}")
        report_lines.append(f"- Total points: {len(pts)}")
        report_lines.append(f"- Phase distribution: {phases}")
        report_lines.append("")
        print(f"OK ({len(pts)} pts)")

    report_lines.append(f"\n---\n**Total state points computed**: {n_state_points}\n")
    content = "\n".join(report_lines)
    _write(os.path.join(out_dir, "steam_tables_report.md"), content)
    return n_state_points


# ============================================================================
# Pillar B: Crystal Discovery
# ============================================================================

def run_pillar_B(out_dir: str, seed: int = 42):
    _banner("Pillar B: Crystal Discovery — Structure Generation & Catalog")

    report_lines = []
    report_lines.append("# Crystal Discovery Report\n")
    report_lines.append(f"**Generated**: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")

    catalog = CrystalCatalog()
    engine = CrystalDiscoveryEngine(catalog, seed)

    # Catalog
    report_lines.append(generate_crystal_catalog_md(catalog))

    # Organic / inorganic split summary
    organic = catalog.search(crystal_class=CrystalClass.MOLECULAR_ORGANIC)
    ionic = catalog.search(crystal_class=CrystalClass.IONIC)
    metallic = catalog.search(crystal_class=CrystalClass.METALLIC)
    covalent = catalog.search(crystal_class=CrystalClass.COVALENT)

    report_lines.append("## Crystal Class Summary\n")
    report_lines.append(f"- **Metallic**: {len(metallic)} entries")
    report_lines.append(f"- **Ionic**: {len(ionic)} entries")
    report_lines.append(f"- **Covalent network**: {len(covalent)} entries")
    report_lines.append(f"- **Molecular organic**: {len(organic)} entries")
    report_lines.append(f"- **Total catalog**: {len(catalog.entries)} entries\n")

    # Discovery scans — expanded to cover more base structures
    discovery_bases = ["NaCl", "MgO", "CaO", "KCl", "Al", "Cu", "Fe", "Ni",
                       "SrTiO3", "BaTiO3", "Si", "GaAs", "TiO2", "Al2O3", "CaF2"]
    for base in discovery_bases:
        base_exists = any(e.formula == base for e in catalog.entries)
        if not base_exists:
            continue
        print(f"  Discovery scan: {base} ...", end=" ", flush=True)
        md = generate_crystal_discovery_md(engine, base)
        report_lines.append(md)
        report_lines.append("")
        print("OK")

    # Lattice perturbation scans for all catalog entries
    report_lines.append("## Lattice Perturbation Scans\n")
    for entry in catalog.entries:
        perts = engine.stoichiometric_perturbation(entry.formula, 8)
        if perts:
            report_lines.append(f"### {entry.name} ({entry.formula})\n")
            report_lines.append("| ID | a (A) | b (A) | c (A) | V (A3) | Density | Score |")
            report_lines.append("|----|-------|-------|-------|--------|---------|-------|")
            for p in perts:
                report_lines.append(
                    f"| {p.entry_id} | {p.lattice.a:.3f} | {p.lattice.b:.3f} "
                    f"| {p.lattice.c:.3f} | {p.lattice.volume:.1f} "
                    f"| {p.density:.2f} | {p.stability_score:.0f} |")
            report_lines.append("")

    # Supercell generation — all entries
    report_lines.append("## Supercell Generation\n")
    report_lines.append("| Base | Supercell | Atoms | Volume (A3) |")
    report_lines.append("|------|-----------|-------|------------|")
    for entry in catalog.entries:
        for nx, ny, nz in [(2,2,2), (3,3,3), (2,2,1)]:
            sc = catalog.generate_supercell(entry, nx, ny, nz)
            report_lines.append(
                f"| {entry.name} | {nx}x{ny}x{nz} | {sc.n_atoms} "
                f"| {sc.lattice.volume:.1f} |")
    report_lines.append("")

    # Defect variants
    report_lines.append("## Defect Variants\n")
    report_lines.append("| Base | Defect | Atoms | Score | Stability |")
    report_lines.append("|------|--------|-------|-------|-----------|")
    from pillars.crystal_discovery import DefectSpec
    # Expanded defect generation: vacancy + substitution for all entries
    substitution_elements = ["Cu", "Ag", "Li", "Mg", "Fe", "Zn", "Al", "Ti"]
    for entry in catalog.entries:
        if entry.n_atoms > 1:
            vac = catalog.insert_defect(entry, DefectSpec("vacancy", 0))
            report_lines.append(
                f"| {entry.name} | vacancy | {vac.n_atoms} "
                f"| {vac.stability_score:.0f} | {vac.stability.value} |")
            for sub_elem in substitution_elements[:3]:
                sub = catalog.insert_defect(entry,
                    DefectSpec("substitution", 0, sub_elem))
                report_lines.append(
                    f"| {entry.name} | {sub_elem} sub | {sub.n_atoms} "
                    f"| {sub.stability_score:.0f} | {sub.stability.value} |")
    report_lines.append("")

    # Export
    csv_path = os.path.join(out_dir, "crystal_catalog.csv")
    export_catalog_csv(catalog, csv_path)
    print(f"  Catalog CSV: {csv_path}")

    content = "\n".join(report_lines)
    _write(os.path.join(out_dir, "crystal_discovery_report.md"), content)
    return len(catalog.entries)


# ============================================================================
# Pillar C: Metals & Macros
# ============================================================================

def run_pillar_C(out_dir: str, seed: int = 42):
    _banner("Pillar C: Metals & Macros — Macro Material Property Engine")

    report_lines = []
    report_lines.append("# Metals & Macros Report\n")
    report_lines.append(f"**Generated**: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")

    # Full database table
    report_lines.append(generate_metals_table_md())
    report_lines.append("")

    # Purpose-driven missions
    missions = [
        ("Low-density high-temp alloy candidates",
         [MissionObjective.LOW_MASS, MissionObjective.HIGH_TEMPERATURE,
          MissionObjective.HIGH_STRENGTH],
         {"density": (0, 8.0)}),
        ("High-conductivity low-expansion materials",
         [MissionObjective.HIGH_CONDUCTIVITY_THERMAL,
          MissionObjective.LOW_EXPANSION, MissionObjective.HIGH_STIFFNESS],
         {}),
        ("Corrosion-resistant high-strength budget alloys",
         [MissionObjective.CORROSION_RESISTANT, MissionObjective.HIGH_STRENGTH,
          MissionObjective.LOW_COST],
         {}),
        ("Maximum machinability with reasonable strength",
         [MissionObjective.MACHINABLE, MissionObjective.HIGH_STRENGTH,
          MissionObjective.LOW_COST],
         {}),
        ("Extreme temperature refractory candidates",
         [MissionObjective.HIGH_TEMPERATURE, MissionObjective.HIGH_STIFFNESS],
         {"melting_point": (2000, 1e6)}),
        ("High electrical conductivity materials",
         [MissionObjective.HIGH_CONDUCTIVITY_ELECTRICAL, MissionObjective.LOW_COST],
         {}),
        ("Lightweight structural alloys",
         [MissionObjective.LOW_MASS, MissionObjective.HIGH_STIFFNESS,
          MissionObjective.HIGH_STRENGTH, MissionObjective.CORROSION_RESISTANT],
         {"density": (0, 5.0)}),
        ("Nuclear reactor materials",
         [MissionObjective.HIGH_TEMPERATURE, MissionObjective.CORROSION_RESISTANT,
          MissionObjective.HIGH_STIFFNESS],
         {"melting_point": (1500, 1e6)}),
        ("Budget general-purpose metals",
         [MissionObjective.LOW_COST, MissionObjective.MACHINABLE,
          MissionObjective.HIGH_STRENGTH],
         {}),
        ("Aerospace high-performance",
         [MissionObjective.LOW_MASS, MissionObjective.HIGH_STRENGTH,
          MissionObjective.HIGH_TEMPERATURE, MissionObjective.CORROSION_RESISTANT],
         {}),
    ]

    for name, objectives, constraints in missions:
        print(f"  Mission: {name} ...", end=" ", flush=True)
        spec = MissionSpec(objectives=objectives, constraints=constraints)
        md = generate_mission_report_md(spec, name)
        report_lines.append(md)
        report_lines.append("")
        print("OK")

    # Alloy estimates
    report_lines.append("## Alloy Estimates (Rule of Mixtures)\n")
    alloy_comps = [
        {"Cu": 0.70, "Zn": 0.30},
        {"Cu": 0.88, "Sn": 0.12},
        {"Fe": 0.72, "Cr": 0.18, "Ni": 0.10},
        {"Ti": 0.90, "Al": 0.06, "V": 0.04},
        {"Ni": 0.55, "Cr": 0.20, "Co": 0.15, "Mo": 0.10},
        {"Al": 0.93, "Cu": 0.04, "Mg": 0.03},
        {"Fe": 0.64, "Ni": 0.36},
        {"Cu": 0.60, "Ni": 0.40},
        {"Cu": 0.95, "Be": 0.05},
        {"Fe": 0.80, "Cr": 0.20},
        {"Fe": 0.60, "Cr": 0.25, "Ni": 0.15},
        {"Ni": 0.70, "Cr": 0.16, "Fe": 0.08, "Mo": 0.06},
        {"Al": 0.90, "Zn": 0.06, "Mg": 0.04},
        {"Ti": 0.85, "V": 0.10, "Cr": 0.05},
        {"Cu": 0.80, "Sn": 0.10, "Zn": 0.10},
        {"Fe": 0.90, "Mo": 0.05, "Cr": 0.05},
        {"Ni": 0.80, "Mo": 0.20},
        {"Co": 0.50, "Cr": 0.30, "W": 0.10, "Ni": 0.10},
        {"Al": 0.85, "Cu": 0.10, "Mg": 0.05},
        {"Fe": 0.70, "Ni": 0.20, "Cr": 0.10},
    ]
    report_lines.append("| Alloy | ρ (g/cm³) | E (GPa) | σ_y (MPa) | σ_u (MPa) "
                        "| k (W/mK) | Tm (K) |")
    report_lines.append("|-------|-----------|---------|-----------|-----------|"
                        "----------|--------|")
    for comp in alloy_comps:
        alloy = AlloyEstimator.rule_of_mixtures(comp)
        if alloy:
            report_lines.append(
                f"| {alloy.formula} | {alloy.density:.2f} "
                f"| {alloy.elastic_modulus:.0f} | {alloy.yield_strength:.0f} "
                f"| {alloy.ultimate_strength:.0f} | {alloy.thermal_conductivity:.1f} "
                f"| {alloy.melting_point:.0f} |")
    report_lines.append("")

    # Heating simulations — all metals
    for sym in sorted(_METAL_DB.keys()):
        print(f"  Heating sim: {sym} ...", end=" ", flush=True)
        md = generate_heating_report_md(sym)
        report_lines.append(md)
        report_lines.append("")
        print("OK")

    csv_path = os.path.join(out_dir, "metals_database.csv")
    export_metals_csv(csv_path)
    print(f"  Metals CSV: {csv_path}")

    content = "\n".join(report_lines)
    _write(os.path.join(out_dir, "metals_macros_report.md"), content)
    return len(_METAL_DB)


# ============================================================================
# Pillar D: SmartSampling
# ============================================================================

def run_pillar_D(out_dir: str, seed: int = 42):
    _banner("Pillar D: SmartSampling — Universal Adaptive Sampling Manager")

    report_lines = []
    report_lines.append("# SmartSampling Core Report\n")
    report_lines.append(f"**Generated**: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")

    # Scoring equation documentation
    report_lines.append("## Scoring Equation\n")
    report_lines.append("```")
    report_lines.append("score_total = w1 * novelty_score")
    report_lines.append("            + w2 * objective_match")
    report_lines.append("            + w3 * uncertainty_score")
    report_lines.append("            + w4 * physical_plausibility")
    report_lines.append("            + w5 * instability_interest")
    report_lines.append("            + w6 * compute_affordability")
    report_lines.append("```\n")

    # Run samplers for each pillar
    total_samples = 0

    # Steam sampler — multiple materials
    for mat_formula in ["H2O", "CO2", "NH3", "R134a", "CH4"]:
        print(f"  SmartSampling: {mat_formula} ...", end=" ", flush=True)
        s_sampler = create_steam_sampler(mat_formula, seed)
        for iteration in range(5):
            candidates = s_sampler.generate_candidates(100)
            ranked = s_sampler.rank_candidates(candidates)
            s_sampler.select_top(ranked, 30)
        total_samples += len(s_sampler.history)
        report_lines.append(generate_sampling_report_md(s_sampler, f"SteamTables++ ({mat_formula})"))
        report_lines.append("")
        print(f"OK ({len(s_sampler.history)} samples)")

    # Crystal sampler
    print("  SmartSampling: Crystal discovery ...", end=" ", flush=True)
    crystal_sampler = create_crystal_sampler(seed)
    for iteration in range(5):
        candidates = crystal_sampler.generate_candidates(100)
        ranked = crystal_sampler.rank_candidates(candidates)
        crystal_sampler.select_top(ranked, 30)
    total_samples += len(crystal_sampler.history)
    report_lines.append(generate_sampling_report_md(crystal_sampler, "Crystal Discovery"))
    report_lines.append("")
    print(f"OK ({len(crystal_sampler.history)} samples)")

    # Alloy samplers — multiple composition spaces
    alloy_spaces = [
        ("Fe-Cr-Ni-Mo", ["Fe", "Cr", "Ni", "Mo"]),
        ("Cu-Zn-Sn", ["Cu", "Zn", "Sn"]),
        ("Ti-Al-V", ["Ti", "Al", "V"]),
        ("Ni-Co-Cr-W", ["Ni", "Co", "Cr", "W"]),
        ("Al-Cu-Mg-Zn", ["Al", "Cu", "Mg", "Zn"]),
    ]
    for space_name, elements in alloy_spaces:
        print(f"  SmartSampling: {space_name} ...", end=" ", flush=True)
        a_sampler = create_alloy_sampler(elements, seed)
        for iteration in range(5):
            candidates = a_sampler.generate_candidates(80)
            ranked = a_sampler.rank_candidates(candidates)
            a_sampler.select_top(ranked, 25)
        total_samples += len(a_sampler.history)
        report_lines.append(generate_sampling_report_md(a_sampler, f"Alloy ({space_name})"))
        report_lines.append("")
        print(f"OK ({len(a_sampler.history)} samples)")

    # Stress tests across multiple materials
    for mat_f in ["H2O", "CO2"]:
        print(f"  SmartSampling: Stress test {mat_f} ...", end=" ", flush=True)
        st_sampler = create_steam_sampler(mat_f, seed + 1)
        stress = st_sampler.stochastic_stress_test(100)
        report_lines.append(f"## Stochastic Stress Test ({mat_f} domain)\n")
        report_lines.append(f"- Points generated: {len(stress)}")
        if stress:
            scores = [s.total for s in stress]
            report_lines.append(f"- Score range: {min(scores):.1f} -- {max(scores):.1f}")
        report_lines.append("")
        total_samples += len(stress)
        print(f"OK ({len(stress)} pts)")

    report_lines.append("## Cross-Pillar Summary\n")
    report_lines.append(f"**Total adaptive samples**: {total_samples}")
    report_lines.append("")

    content = "\n".join(report_lines)
    _write(os.path.join(out_dir, "smart_sampling_report.md"), content)
    return total_samples


# ============================================================================
# Pillar E: Golden Project
# ============================================================================

def run_pillar_E(out_dir: str, seed: int = 42):
    _banner("Pillar E: Golden Project — VSEPR-SIM Discovery Atlas")

    atlas = DiscoveryAtlas(out_dir)

    # Mission 1: Full discovery — all S1 materials
    mission_full = DiscoveryMission(
        mission_id="GOLD-001",
        name="Full Materials Discovery Atlas",
        mission_type=MissionType.FULL_DISCOVERY,
        description="Comprehensive cross-pillar discovery spanning thermo, crystal, alloy, and process domains",
        materials=["H2O", "CO2", "NH3", "R134a", "CH4", "N2", "O2", "He", "Ar"],
        seed=seed,
        max_iterations=5,
        samples_per_iteration=80)
    print("  Mission: Full Discovery Atlas ...", flush=True)
    results_full = atlas.run_mission(mission_full)
    print(f"    -> {len(results_full)} results")

    # Mission 2: Alloy optimization
    mission_alloy = DiscoveryMission(
        mission_id="GOLD-002",
        name="High-Temperature Low-Mass Alloy Search",
        mission_type=MissionType.ALLOY_OPTIMIZATION,
        description="Find alloy candidates with high melting point, low density, good corrosion resistance",
        targets={"objectives": ["high_temperature", "low_mass", "corrosion_resistant"]},
        seed=seed)
    print("  Mission: Alloy Optimization ...", flush=True)
    results_alloy = atlas.run_mission(mission_alloy)
    print(f"    -> {len(results_alloy)} results")

    # Mission 3: Crystal search
    mission_crystal = DiscoveryMission(
        mission_id="GOLD-003",
        name="Wide-Bandgap Crystal Candidates",
        mission_type=MissionType.CRYSTAL_SEARCH,
        description="Search for crystal structures with interesting electronic properties",
        seed=seed)
    print("  Mission: Crystal Search ...", flush=True)
    results_crystal = atlas.run_mission(mission_crystal)
    print(f"    -> {len(results_crystal)} results")

    # Mission 4: Process evaluation all metals
    mission_process = DiscoveryMission(
        mission_id="GOLD-004",
        name="Comprehensive Process Evaluation",
        mission_type=MissionType.PROCESS_EVALUATION,
        description="Heating simulation across all available metals",
        materials=sorted(_METAL_DB.keys()) if hasattr(_METAL_DB, 'keys') else [],
        seed=seed)
    print("  Mission: Process Evaluation ...", flush=True)
    # Import metals DB for process eval
    from pillars.metals_macros import _METAL_DB as mdb
    mission_process.materials = sorted(mdb.keys())
    results_process = atlas.run_mission(mission_process)
    print(f"    -> {len(results_process)} results")

    # Mission 5: Thermo-only deep dive for S2 materials
    mission_thermo = DiscoveryMission(
        mission_id="GOLD-005",
        name="S2 Tier Deep Thermophysical Survey",
        mission_type=MissionType.THERMO_EXPLORATION,
        description="Focused thermophysical exploration of EOS-derived approximation materials",
        materials=["C2H6", "C3H8", "C2H5OH", "CH3OH", "H2", "C6H6", "C7H8"],
        seed=seed,
        samples_per_iteration=60)
    print("  Mission: S2 Thermo Survey ...", flush=True)
    results_thermo = atlas.run_mission(mission_thermo)
    print(f"    -> {len(results_thermo)} results")

    # Generate master report
    master_md = atlas.generate_master_report_md()
    _write(os.path.join(out_dir, "golden_project_master_report.md"), master_md)

    csv_path = atlas.export_all_csv()
    print(f"  Discovery CSV: {csv_path}")

    return len(atlas.results)


# ============================================================================
# Main
# ============================================================================

def main():
    import argparse
    parser = argparse.ArgumentParser(description="VSEPR-SIM Five Pillars Report Generator")
    parser.add_argument("--output", default="out/pillars", help="Output directory")
    parser.add_argument("--seed", type=int, default=42, help="Random seed")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    out = args.output
    seed = args.seed
    os.makedirs(out, exist_ok=True)

    print()
    print("=" * 70)
    print("  VSEPR-SIM Five Pillars — Master Report Generator")
    print("=" * 70)
    print(f"  Output: {os.path.abspath(out)}")
    print(f"  Seed: {seed}")
    print()

    t0 = time.time()
    totals = {}

    # Run all pillars
    totals["steam_state_points"] = run_pillar_A(
        os.path.join(out, "steam_tables"), seed)
    totals["crystal_entries"] = run_pillar_B(
        os.path.join(out, "crystal_discovery"), seed)
    totals["metals_entries"] = run_pillar_C(
        os.path.join(out, "metals_macros"), seed)
    totals["sampling_points"] = run_pillar_D(
        os.path.join(out, "smart_sampling"), seed)
    totals["discovery_results"] = run_pillar_E(
        os.path.join(out, "golden_project"), seed)

    elapsed = time.time() - t0

    # Final summary
    print()
    print("=" * 70)
    print("  FIVE PILLARS — COMPLETE")
    print("=" * 70)
    print(f"  Time: {elapsed:.1f}s")
    print(f"  Steam state points:  {totals['steam_state_points']:,}")
    print(f"  Crystal entries:     {totals['crystal_entries']:,}")
    print(f"  Metals entries:      {totals['metals_entries']:,}")
    print(f"  Sampling points:     {totals['sampling_points']:,}")
    print(f"  Discovery results:   {totals['discovery_results']:,}")
    total_data = sum(totals.values())
    print(f"  Total data points:   {total_data:,}")
    print()

    # Count output files
    file_count = 0
    total_size = 0
    for root, dirs, files in os.walk(out):
        for f in files:
            fp = os.path.join(root, f)
            file_count += 1
            total_size += os.path.getsize(fp)

    print(f"  Output files:  {file_count}")
    print(f"  Total size:    {total_size / 1024:.0f} KB")
    print()

    # Summary JSON
    summary = {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "seed": seed,
        "elapsed_seconds": round(elapsed, 2),
        "totals": totals,
        "output_files": file_count,
        "total_size_bytes": total_size
    }
    summary_path = os.path.join(out, "pillar_summary.json")
    with open(summary_path, "w") as f:
        json.dump(summary, f, indent=2)
    print(f"  Summary: {summary_path}")


if __name__ == "__main__":
    main()
