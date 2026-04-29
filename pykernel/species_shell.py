"""
species_shell.py — Interactive Species Record Shell for VSEPR-SIM
==================================================================

Provides an interactive command-line interface for:
    - Browsing the NIST-JANAF species database
    - Computing Cp, H, S, G at any temperature
    - Exporting species in NIST, JANAF, VSEPR, or VSPES format
    - Generating Cp(T) tables and figures
    - Batch export of all species to JSON / VSEPR text

Format modes:
    NIST  — NIST WebBook tabular output (Cp, H, S, G table)
    JANAF — NIST-JANAF record card style (traditional print layout)
    VSEPR — Native VSEPR-SIM species text block (5-section schema)
    VSPES — VSEPR Pillar Engine Species (compact JSON for engine ingest)

Anti-black-box: every computed value traces to Shomate coefficients.
Deterministic: same species + T → identical output.

VSEPR-SIM 3.0.0
"""

from __future__ import annotations

import json
import math
import os
import sys
from datetime import datetime
from pathlib import Path
from typing import Optional

_SCRIPT_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(_SCRIPT_DIR))

from pykernel.species_record import (
    SpeciesRecord, ShomateRegion, save_species_json, save_species_vsepr,
)
from pykernel.species_janaf_db import (
    JANAF_SPECIES, get_species, get_species_by_formula,
    list_species, list_species_table,
)

# Try optional imports for figures
try:
    from pykernel.chart_helpers import (
        configure_style, save_figure, chart_line, PALETTE_CYCLE,
    )
    import numpy as np
    HAS_CHARTS = True
except ImportError:
    HAS_CHARTS = False


# ═══════════════════════════════════════════════════════════════════════
# Format renderers
# ═══════════════════════════════════════════════════════════════════════

def render_nist_table(rec: SpeciesRecord, T_start: float = 298.15,
                      T_end: float = 3000.0, step: float = 100.0) -> str:
    """Render NIST WebBook style tabular output."""
    lines = [
        f"# NIST Chemistry WebBook — Shomate Equation Data",
        f"# Species: {rec.name} ({rec.formula}), Phase: {rec.phase}",
        f"# M = {rec.molar_mass_gmol:.4f} g/mol",
        f"# ΔfH°(298) = {rec.thermo_reference.Hf298_kJmol:.3f} kJ/mol",
        f"# S°(298) = {rec.thermo_reference.S298_JmolK:.3f} J/(mol·K)",
        f"# Source: {rec.source_ref}",
        f"#",
        f"# Cp model: {rec.cp_model}, {rec.n_regions} region(s)",
    ]
    for i, reg in enumerate(rec.regions):
        lines.append(f"#   Region {i+1}: {reg.Tmin_K:.0f} – {reg.Tmax_K:.0f} K")
    lines.append("#")
    lines.append(f"{'T (K)':>10}  {'Cp J/(mol·K)':>14}  {'H-H298 kJ/mol':>16}  "
                 f"{'S J/(mol·K)':>14}  {'G kJ/mol':>12}")
    lines.append("-" * 76)

    table = rec.cp_table(T_start, T_end, step)
    for row in table:
        lines.append(
            f"{row['T_K']:10.2f}  {row['Cp_JmolK']:14.4f}  "
            f"{row['H_kJmol']:16.4f}  {row['S_JmolK']:14.4f}  "
            f"{row['G_kJmol']:12.4f}"
        )
    return "\n".join(lines)


def render_janaf_card(rec: SpeciesRecord) -> str:
    """Render NIST-JANAF traditional record card layout."""
    sep = "=" * 72
    lines = [
        sep,
        f"  NIST-JANAF Thermochemical Tables",
        f"  Species: {rec.name}",
        f"  Formula: {rec.formula}        Phase: {rec.phase}",
        f"  CAS: {rec.cas_number}",
        sep,
        f"  Molar Mass:    {rec.molar_mass_gmol:.4f} g/mol",
        f"  ΔfH°(298.15):  {rec.thermo_reference.Hf298_kJmol:.3f} kJ/mol",
        f"  S°(298.15):    {rec.thermo_reference.S298_JmolK:.3f} J/(mol·K)",
        f"  Source:         {rec.source_ref}",
        "",
        f"  Structure: {rec.structure_model.category}, "
        f"geometry = {rec.structure_model.geometry}, "
        f"symmetry = {rec.structure_model.symmetry_hint}",
        "",
        f"  Cp Model: {rec.cp_model}",
    ]

    for i, reg in enumerate(rec.regions):
        lines.append(f"  ── Region {i+1}: {reg.Tmin_K:.0f} – {reg.Tmax_K:.0f} K ──")
        lines.append(f"    A = {reg.A:15.6f}   B = {reg.B:15.6f}")
        lines.append(f"    C = {reg.C:15.6f}   D = {reg.D:15.6f}")
        lines.append(f"    E = {reg.E:15.6f}   F = {reg.F:15.6f}")
        lines.append(f"    G = {reg.G:15.6f}   H = {reg.H:15.6f}")

    lines.append("")
    lines.append("  Selected Values:")
    lines.append(f"  {'T (K)':>8}  {'Cp':>10}  {'H-H298':>12}  {'S':>10}  {'G':>12}")
    lines.append(f"  {'':>8}  {'J/(mol·K)':>10}  {'kJ/mol':>12}  {'J/(mol·K)':>10}  {'kJ/mol':>12}")
    lines.append("  " + "-" * 60)

    for T in [298.15, 300, 400, 500, 600, 800, 1000, 1500, 2000, 3000, 4000, 5000]:
        cp = rec.cp(T)
        if math.isnan(cp):
            continue
        h = rec.enthalpy(T)
        s = rec.entropy(T)
        g = rec.gibbs(T)
        lines.append(
            f"  {T:8.2f}  {cp:10.4f}  {h:12.4f}  {s:10.4f}  {g:12.4f}"
        )

    lines.append(sep)
    return "\n".join(lines)


def render_vsepr_format(rec: SpeciesRecord) -> str:
    """Render native VSEPR text format."""
    return rec.to_vsepr_text()


def render_vspes_format(rec: SpeciesRecord) -> str:
    """Render VSPES compact JSON for engine ingest."""
    vspes = {
        "vspes_version": "1.0",
        "id": rec.id,
        "formula": rec.formula,
        "phase": rec.phase,
        "M": rec.molar_mass_gmol,
        "Hf298": rec.thermo_reference.Hf298_kJmol,
        "S298": rec.thermo_reference.S298_JmolK,
        "geometry": rec.structure_model.geometry,
        "symmetry": rec.structure_model.symmetry_hint,
        "atoms": {a.element: a.count for a in rec.atoms},
        "cp_model": rec.cp_model,
        "regions": [],
        "flags": {
            "ideal_gas": rec.engine_flags.allow_ideal_gas,
            "real_gas_upgrade": rec.engine_flags.allow_real_gas_upgrade,
            "dissociation": rec.engine_flags.allow_dissociation,
            "reactive_family": rec.engine_flags.reactive_family,
        },
    }
    for reg in rec.regions:
        vspes["regions"].append({
            "Tmin": reg.Tmin_K, "Tmax": reg.Tmax_K,
            "A": reg.A, "B": reg.B, "C": reg.C, "D": reg.D,
            "E": reg.E, "F": reg.F, "G": reg.G, "H": reg.H,
        })
    return json.dumps(vspes, indent=2)


# ═══════════════════════════════════════════════════════════════════════
# Cp figure generation
# ═══════════════════════════════════════════════════════════════════════

def generate_cp_figure(rec: SpeciesRecord, outdir: Path,
                       T_start: float = 298.15, T_end: float = 3000.0,
                       n_points: int = 500) -> Optional[Path]:
    """Generate a Cp(T) plot for a species and save as PNG."""
    if not HAS_CHARTS:
        return None

    configure_style()
    Tmin, Tmax = rec.T_range
    T_start = max(T_start, Tmin)
    T_end = min(T_end, Tmax)
    if T_end <= T_start:
        return None

    T_arr = np.linspace(T_start, T_end, n_points)
    Cp_arr = np.array([rec.cp(t) for t in T_arr])

    valid = ~np.isnan(Cp_arr)
    if valid.sum() < 5:
        return None

    fig = chart_line(
        [T_arr[valid]], [Cp_arr[valid]],
        labels=[f"{rec.formula} ({rec.phase})"],
        title=f"{rec.name} — Isobaric Heat Capacity",
        xlabel="Temperature (K)",
        ylabel="Cp (J/(mol·K))",
    )
    name = f"species_Cp_{rec.id}.png"
    outdir.mkdir(parents=True, exist_ok=True)
    path = save_figure(fig, name, outdir=outdir)
    return path


def generate_multi_cp_figure(records: list[SpeciesRecord], outdir: Path,
                             T_start: float = 298.15,
                             T_end: float = 3000.0,
                             n_points: int = 500) -> Optional[Path]:
    """Generate an overlay Cp(T) plot for multiple species."""
    if not HAS_CHARTS:
        return None

    configure_style()
    x_arrs, y_arrs, labels = [], [], []

    for rec in records:
        Tmin, Tmax = rec.T_range
        ts = max(T_start, Tmin)
        te = min(T_end, Tmax)
        if te <= ts:
            continue
        T_arr = np.linspace(ts, te, n_points)
        Cp_arr = np.array([rec.cp(t) for t in T_arr])
        valid = ~np.isnan(Cp_arr)
        if valid.sum() < 5:
            continue
        x_arrs.append(T_arr[valid])
        y_arrs.append(Cp_arr[valid])
        labels.append(f"{rec.formula}")

    if len(x_arrs) < 1:
        return None

    n_series = len(x_arrs)
    colors = [PALETTE_CYCLE[i % len(PALETTE_CYCLE)] for i in range(n_series)]

    fig = chart_line(
        x_arrs, y_arrs, labels=labels, colors=colors,
        title="Species Cp Comparison (Shomate)",
        xlabel="Temperature (K)",
        ylabel="Cp (J/(mol·K))",
        figsize=(12, 8),
    )
    name = "species_Cp_comparison.png"
    outdir.mkdir(parents=True, exist_ok=True)
    return save_figure(fig, name, outdir=outdir)


# ═══════════════════════════════════════════════════════════════════════
# Interactive Shell
# ═══════════════════════════════════════════════════════════════════════

_BANNER = """
╔══════════════════════════════════════════════════════════════════╗
║          VSEPR-SIM Species Record Interactive Shell             ║
║                                                                  ║
║  Formats:  NIST | JANAF | VSEPR | VSPES                        ║
║  Source:   NIST-JANAF Thermochemical Tables (Chase 1998)        ║
║                                                                  ║
║  Commands:                                                       ║
║    list              — List all species                          ║
║    info <id>         — Show species summary                      ║
║    cp <id> <T>       — Compute Cp at temperature T               ║
║    table <id>        — Full Cp/H/S/G table                       ║
║    format <mode>     — Set output format (nist/janaf/vsepr/vspes)║
║    show <id>         — Show species in current format            ║
║    plot <id>         — Generate Cp(T) figure                     ║
║    plotall           — Overlay Cp(T) for all species             ║
║    export <dir>      — Batch export all species                  ║
║    search <text>     — Search by name or formula                 ║
║    help              — Show this help                            ║
║    quit / exit       — Exit shell                                ║
╚══════════════════════════════════════════════════════════════════╝
"""


class SpeciesShell:
    """Interactive species record shell."""

    def __init__(self, output_dir: Path | None = None):
        self.format_mode: str = "vsepr"
        self.output_dir = output_dir or Path("out/species")
        self.output_dir.mkdir(parents=True, exist_ok=True)

    def run(self):
        """Enter the interactive shell loop."""
        print(_BANNER)
        print(f"  {len(JANAF_SPECIES)} species loaded from NIST-JANAF database")
        print(f"  Output directory: {self.output_dir}")
        print()

        while True:
            try:
                raw = input("species> ").strip()
            except (EOFError, KeyboardInterrupt):
                print("\nExiting species shell.")
                break

            if not raw:
                continue

            parts = raw.split(maxsplit=1)
            cmd = parts[0].lower()
            arg = parts[1].strip() if len(parts) > 1 else ""

            if cmd in ("quit", "exit", "q"):
                print("Exiting species shell.")
                break
            elif cmd == "help":
                print(_BANNER)
            elif cmd == "list":
                self._cmd_list()
            elif cmd == "info":
                self._cmd_info(arg)
            elif cmd == "cp":
                self._cmd_cp(arg)
            elif cmd == "table":
                self._cmd_table(arg)
            elif cmd == "format":
                self._cmd_format(arg)
            elif cmd == "show":
                self._cmd_show(arg)
            elif cmd == "plot":
                self._cmd_plot(arg)
            elif cmd == "plotall":
                self._cmd_plotall()
            elif cmd == "export":
                self._cmd_export(arg)
            elif cmd == "search":
                self._cmd_search(arg)
            else:
                print(f"  Unknown command: {cmd}. Type 'help' for commands.")

    def _resolve(self, arg: str) -> Optional[SpeciesRecord]:
        """Resolve a species from id, formula, or name."""
        rec = get_species(arg)
        if rec:
            return rec
        rec = get_species_by_formula(arg)
        if rec:
            return rec
        # Try id pattern: formula_phase
        for phase in ("gas", "liquid", "solid"):
            rec = get_species(f"{arg}_{phase}")
            if rec:
                return rec
        # Search by name
        for r in JANAF_SPECIES.values():
            if r.name.lower() == arg.lower():
                return r
        print(f"  Species not found: {arg}")
        return None

    def _cmd_list(self):
        table = list_species_table()
        print(f"\n  {'ID':<18} {'Name':<24} {'Formula':<8} {'M':>8} "
              f"{'Hf298':>10} {'S298':>8} {'T range':>14} {'Geom':<12}")
        print("  " + "-" * 110)
        for r in table:
            tr = f"{r['Tmin_K']:.0f}–{r['Tmax_K']:.0f}"
            print(f"  {r['id']:<18} {r['name']:<24} {r['formula']:<8} "
                  f"{r['M_gmol']:8.3f} {r['Hf298_kJmol']:10.3f} "
                  f"{r['S298_JmolK']:8.3f} {tr:>14} {r['geometry']:<12}")
        print(f"\n  Total: {len(table)} species\n")

    def _cmd_info(self, arg: str):
        rec = self._resolve(arg)
        if not rec:
            return
        print(f"\n  ID:       {rec.id}")
        print(f"  Name:     {rec.name}")
        print(f"  Formula:  {rec.formula}")
        print(f"  Phase:    {rec.phase}")
        print(f"  M:        {rec.molar_mass_gmol:.4f} g/mol")
        print(f"  CAS:      {rec.cas_number}")
        print(f"  Hf°298:   {rec.thermo_reference.Hf298_kJmol:.3f} kJ/mol")
        print(f"  S°298:    {rec.thermo_reference.S298_JmolK:.3f} J/(mol·K)")
        print(f"  Geometry: {rec.structure_model.geometry}")
        print(f"  Symmetry: {rec.structure_model.symmetry_hint}")
        print(f"  Regions:  {rec.n_regions} ({rec.T_range[0]:.0f}–{rec.T_range[1]:.0f} K)")
        print(f"  Family:   {rec.engine_flags.reactive_family}")
        print(f"  Source:   {rec.source_ref}\n")

    def _cmd_cp(self, arg: str):
        parts = arg.split()
        if len(parts) < 2:
            print("  Usage: cp <species> <T_K>")
            return
        rec = self._resolve(parts[0])
        if not rec:
            return
        try:
            T = float(parts[1])
        except ValueError:
            print(f"  Invalid temperature: {parts[1]}")
            return
        cp = rec.cp(T)
        h = rec.enthalpy(T)
        s = rec.entropy(T)
        g = rec.gibbs(T)
        print(f"\n  {rec.formula} at T = {T:.2f} K:")
        print(f"    Cp  = {cp:.4f} J/(mol·K)")
        print(f"    H-H298 = {h:.4f} kJ/mol")
        print(f"    S   = {s:.4f} J/(mol·K)")
        print(f"    G   = {g:.4f} kJ/mol\n")

    def _cmd_table(self, arg: str):
        rec = self._resolve(arg)
        if not rec:
            return
        print(render_nist_table(rec))
        print()

    def _cmd_format(self, arg: str):
        mode = arg.lower()
        if mode in ("nist", "janaf", "vsepr", "vspes"):
            self.format_mode = mode
            print(f"  Format set to: {mode.upper()}")
        else:
            print(f"  Valid formats: nist, janaf, vsepr, vspes")

    def _cmd_show(self, arg: str):
        rec = self._resolve(arg)
        if not rec:
            return
        if self.format_mode == "nist":
            print(render_nist_table(rec))
        elif self.format_mode == "janaf":
            print(render_janaf_card(rec))
        elif self.format_mode == "vsepr":
            print(render_vsepr_format(rec))
        elif self.format_mode == "vspes":
            print(render_vspes_format(rec))
        print()

    def _cmd_plot(self, arg: str):
        rec = self._resolve(arg)
        if not rec:
            return
        if not HAS_CHARTS:
            print("  matplotlib/numpy not available for plotting")
            return
        path = generate_cp_figure(rec, self.output_dir)
        if path:
            print(f"  Saved: {path}")
        else:
            print("  Failed to generate plot")

    def _cmd_plotall(self):
        if not HAS_CHARTS:
            print("  matplotlib/numpy not available for plotting")
            return
        records = list(JANAF_SPECIES.values())
        path = generate_multi_cp_figure(records, self.output_dir)
        if path:
            print(f"  Saved: {path}")
        else:
            print("  Failed to generate plot")

    def _cmd_export(self, arg: str):
        outdir = Path(arg) if arg else self.output_dir
        outdir.mkdir(parents=True, exist_ok=True)

        records = list(JANAF_SPECIES.values())

        # JSON
        json_path = outdir / "species_database.json"
        save_species_json(records, json_path)
        print(f"  JSON:  {json_path}")

        # VSEPR text
        vsepr_path = outdir / "species_database.vsepr"
        save_species_vsepr(records, vsepr_path)
        print(f"  VSEPR: {vsepr_path}")

        # VSPES (one file per species)
        vspes_dir = outdir / "vspes"
        vspes_dir.mkdir(parents=True, exist_ok=True)
        for rec in records:
            fp = vspes_dir / f"{rec.id}.vspes.json"
            with open(fp, "w") as f:
                f.write(render_vspes_format(rec))
        print(f"  VSPES: {vspes_dir}/ ({len(records)} files)")

        # Individual JANAF cards
        janaf_dir = outdir / "janaf_cards"
        janaf_dir.mkdir(parents=True, exist_ok=True)
        for rec in records:
            fp = janaf_dir / f"{rec.id}.janaf.txt"
            with open(fp, "w") as f:
                f.write(render_janaf_card(rec))
        print(f"  JANAF: {janaf_dir}/ ({len(records)} cards)")

        # NIST tables
        nist_dir = outdir / "nist_tables"
        nist_dir.mkdir(parents=True, exist_ok=True)
        for rec in records:
            fp = nist_dir / f"{rec.id}.nist.txt"
            with open(fp, "w") as f:
                f.write(render_nist_table(rec))
        print(f"  NIST:  {nist_dir}/ ({len(records)} tables)")

        print(f"\n  Exported {len(records)} species to {outdir}\n")

    def _cmd_search(self, arg: str):
        if not arg:
            print("  Usage: search <text>")
            return
        query = arg.lower()
        results = []
        for rec in JANAF_SPECIES.values():
            if (query in rec.id.lower() or query in rec.name.lower() or
                    query in rec.formula.lower() or
                    query in rec.structure_model.geometry.lower() or
                    query in rec.engine_flags.reactive_family.lower()):
                results.append(rec)
        if results:
            print(f"\n  Found {len(results)} match(es):")
            for r in results:
                print(f"    {r.id:<18} {r.name:<24} {r.formula:<8} "
                      f"({r.structure_model.geometry})")
            print()
        else:
            print(f"  No matches for '{arg}'")


# ═══════════════════════════════════════════════════════════════════════
# Non-interactive API for report engine integration
# ═══════════════════════════════════════════════════════════════════════

def run_batch_export(output_dir: Path) -> dict:
    """Run a full batch export (called from report engine)."""
    shell = SpeciesShell(output_dir)
    shell._cmd_export(str(output_dir))
    return {
        "species_count": len(JANAF_SPECIES),
        "output_dir": str(output_dir),
    }


def run_batch_figures(output_dir: Path) -> int:
    """Generate Cp figures for all species (called from report engine)."""
    if not HAS_CHARTS:
        return 0
    count = 0
    for rec in JANAF_SPECIES.values():
        path = generate_cp_figure(rec, output_dir)
        if path:
            count += 1
    # Multi comparison
    path = generate_multi_cp_figure(list(JANAF_SPECIES.values()), output_dir)
    if path:
        count += 1
    return count


# ═══════════════════════════════════════════════════════════════════════
# Main entry point
# ═══════════════════════════════════════════════════════════════════════

def main():
    import argparse
    parser = argparse.ArgumentParser(
        description="VSEPR-SIM Species Record Interactive Shell")
    parser.add_argument("--output", type=str, default="out/species",
                        help="Output directory for exports and figures")
    parser.add_argument("--batch-export", action="store_true",
                        help="Run batch export and exit (non-interactive)")
    parser.add_argument("--batch-figures", action="store_true",
                        help="Generate all Cp figures and exit")
    args = parser.parse_args()

    output_dir = Path(args.output)

    if args.batch_export:
        run_batch_export(output_dir)
        return

    if args.batch_figures:
        n = run_batch_figures(output_dir)
        print(f"Generated {n} species figures in {output_dir}")
        return

    shell = SpeciesShell(output_dir)
    shell.run()


if __name__ == "__main__":
    main()
