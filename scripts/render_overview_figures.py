#!/usr/bin/env python3
"""
render_overview_figures.py — Generate overview figures for the VSEPR-SIM master document.

Creates architectural diagrams, state-flow visualizations, dataset schematics,
and document map figures for embedding in VSEPR_SIM_COMPLETE.tex.

Usage:
    python scripts/render_overview_figures.py

Output:
    docs/figures/overview_*.png  (12+ PNG figures)
"""

import os
import sys
import numpy as np

# Force non-interactive backend
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch
from matplotlib.collections import PatchCollection
import matplotlib.patheffects as pe

# ── Output directory ────────────────────────────────────────────────────────
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR   = os.path.dirname(SCRIPT_DIR)
FIG_DIR    = os.path.join(ROOT_DIR, "docs", "figures")
os.makedirs(FIG_DIR, exist_ok=True)

DPI = 180

def savefig(fig, name):
    path = os.path.join(FIG_DIR, name)
    fig.savefig(path, dpi=DPI, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print(f"  [OK] {name}")


# ════════════════════════════════════════════════════════════════════════════
# 1. PROJECT ARCHITECTURE MAP
# ════════════════════════════════════════════════════════════════════════════
def fig_architecture_map():
    fig, ax = plt.subplots(figsize=(14, 9))
    ax.set_xlim(0, 14)
    ax.set_ylim(0, 9)
    ax.set_aspect("equal")
    ax.axis("off")
    fig.suptitle("VSEPR-SIM — System Architecture Map", fontsize=16, fontweight="bold", y=0.97)

    # Layer colors
    colors = {
        "kernel":  "#1a5276",
        "atomistic": "#2e86c1",
        "coarse":  "#27ae60",
        "viz":     "#8e44ad",
        "report":  "#c0392b",
        "test":    "#7f8c8d",
        "docs":    "#d4ac0d",
    }

    layers = [
        (1.0, 0.5, 12.0, 1.0, "Kernel / Core Physics", colors["kernel"],
         "state.hpp · state_c.hpp · alignment.hpp · inertia_frame.hpp"),
        (1.0, 1.8, 12.0, 1.0, "Atomistic Layer", colors["atomistic"],
         "emergence_dataset.hpp · deep_research.hpp · conformer_finder.hpp · properties.cpp"),
        (1.0, 3.1, 5.5, 1.0, "Coarse-Grained Layer", colors["coarse"],
         "bead.hpp · environment_state.hpp · model_selector.hpp"),
        (6.8, 3.1, 6.2, 1.0, "Simulation Engine", colors["coarse"],
         "sim_state.hpp · run_result.hpp · system_state.hpp"),
        (1.0, 4.4, 5.5, 1.0, "Visualization Layer", colors["viz"],
         "cartoon_renderer.py · zmq_producer.py · render_*.py"),
        (6.8, 4.4, 6.2, 1.0, "Reporting Layer", colors["report"],
         "molecular_census.hpp · report.tex · layering_report.tex"),
        (1.0, 5.7, 5.5, 1.0, "Test Infrastructure", colors["test"],
         "15 test groups · 500+ test cases · CMakeLists.txt"),
        (6.8, 5.7, 6.2, 1.0, "Documentation Suite", colors["docs"],
         "42 LaTeX documents · 530+ pages · 96 figures"),
    ]

    for x, y, w, h, label, color, detail in layers:
        box = FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.1",
                             facecolor=color, edgecolor="white", linewidth=2, alpha=0.85)
        ax.add_patch(box)
        ax.text(x + w/2, y + h*0.65, label, ha="center", va="center",
                fontsize=11, fontweight="bold", color="white",
                path_effects=[pe.withStroke(linewidth=2, foreground="black")])
        ax.text(x + w/2, y + h*0.25, detail, ha="center", va="center",
                fontsize=7, color="white", alpha=0.9)

    # Arrows between layers
    arrow_kw = dict(arrowstyle="-|>", color="#2c3e50", lw=2, mutation_scale=15)
    arrows = [
        ((7.0, 1.5), (7.0, 1.8)),   # kernel -> atomistic
        ((3.75, 2.8), (3.75, 3.1)),  # atomistic -> coarse
        ((9.9, 2.8), (9.9, 3.1)),    # atomistic -> sim
        ((3.75, 4.1), (3.75, 4.4)),  # coarse -> viz
        ((9.9, 4.1), (9.9, 4.4)),    # sim -> report
        ((3.75, 5.4), (3.75, 5.7)),  # viz -> test
        ((9.9, 5.4), (9.9, 5.7)),    # report -> docs
    ]
    for (x1, y1), (x2, y2) in arrows:
        ax.annotate("", xy=(x2, y2), xytext=(x1, y1),
                    arrowprops=dict(arrowstyle="-|>", color="#2c3e50", lw=2))

    # Version label
    ax.text(7.0, 7.2, "VSEPR-SIM v3.0.0 — Research Platform", ha="center",
            fontsize=13, fontstyle="italic", color="#2c3e50")

    savefig(fig, "overview_architecture_map.png")


# ════════════════════════════════════════════════════════════════════════════
# 2. STATE-C TRANSFORMATION DIAGRAM
# ════════════════════════════════════════════════════════════════════════════
def fig_statec_flow():
    fig, ax = plt.subplots(figsize=(14, 6))
    ax.set_xlim(0, 14)
    ax.set_ylim(0, 6)
    ax.set_aspect("equal")
    ax.axis("off")
    fig.suptitle("StateC Transformation:  $S_0$  $\\longrightarrow$  $S_f = T(S_0, t, \\Lambda)$",
                 fontsize=15, fontweight="bold", y=0.97)

    # S_0 box
    s0_fields = [
        ("$m_0$", "initial mass"),
        ("$E_0$", "initial energy"),
        ("$\\mathbf{x}_0$", "positions"),
        ("$\\mathbf{v}_0$", "velocities"),
        ("$\\mathcal{C}$", "composition"),
        ("$\\mathcal{E}$", "environment"),
        ("$\\mathcal{K}$", "constraints"),
        ("$\\Sigma$", "provenance"),
    ]
    box_s0 = FancyBboxPatch((0.3, 0.5), 3.5, 5.0, boxstyle="round,pad=0.15",
                            facecolor="#2e86c1", edgecolor="#1a5276", linewidth=2, alpha=0.9)
    ax.add_patch(box_s0)
    ax.text(2.05, 5.15, "$S_0$ — Initial State", ha="center", va="center",
            fontsize=13, fontweight="bold", color="white")
    for i, (sym, desc) in enumerate(s0_fields):
        y = 4.5 - i * 0.55
        ax.text(1.0, y, sym, ha="left", fontsize=11, color="white", fontweight="bold")
        ax.text(2.3, y, desc, ha="left", fontsize=9, color="#d6eaf8")

    # Transformation arrow
    ax.annotate("", xy=(6.2, 3.0), xytext=(4.1, 3.0),
                arrowprops=dict(arrowstyle="-|>", color="#c0392b", lw=3, mutation_scale=20))
    ax.text(5.15, 3.5, "$T(S_0, t, \\Lambda)$", ha="center", fontsize=13,
            fontweight="bold", color="#c0392b")
    ax.text(5.15, 2.5, "evolve()", ha="center", fontsize=10,
            fontstyle="italic", color="#7f8c8d")

    # S_f box
    sf_fields = [
        ("$m_f$", "final mass"),
        ("$E_f$", "final energy"),
        ("$\\Phi$", "structural state"),
        ("$S$", "stability metric"),
        ("$\\Pi$", "performance"),
        ("$\\Omega$", "event log"),
        ("$Q$", "quality flags"),
    ]
    box_sf = FancyBboxPatch((6.5, 0.7), 3.5, 4.6, boxstyle="round,pad=0.15",
                            facecolor="#27ae60", edgecolor="#1e8449", linewidth=2, alpha=0.9)
    ax.add_patch(box_sf)
    ax.text(8.25, 4.95, "$S_f$ — Outcome State", ha="center", va="center",
            fontsize=13, fontweight="bold", color="white")
    for i, (sym, desc) in enumerate(sf_fields):
        y = 4.3 - i * 0.55
        ax.text(7.0, y, sym, ha="left", fontsize=11, color="white", fontweight="bold")
        ax.text(8.3, y, desc, ha="left", fontsize=9, color="#d5f5e3")

    # Energy balance box
    box_eb = FancyBboxPatch((10.5, 1.5), 3.2, 3.0, boxstyle="round,pad=0.15",
                            facecolor="#f39c12", edgecolor="#d68910", linewidth=2, alpha=0.85)
    ax.add_patch(box_eb)
    ax.text(12.1, 4.15, "Energy Balance", ha="center", va="center",
            fontsize=12, fontweight="bold", color="white")
    balance_lines = [
        "$E_0 + E_{in}$",
        "$\\approx$",
        "$E_f + E_{out} + E_{loss}$",
        "",
        "$|\\Delta m| < \\epsilon_m$",
    ]
    for i, line in enumerate(balance_lines):
        ax.text(12.1, 3.6 - i * 0.45, line, ha="center", fontsize=10, color="white")

    ax.annotate("", xy=(10.5, 3.0), xytext=(10.0, 3.0),
                arrowprops=dict(arrowstyle="-|>", color="#d68910", lw=2))

    savefig(fig, "overview_statec_flow.png")


# ════════════════════════════════════════════════════════════════════════════
# 3. EMERGENCE DATASET SCHEMA
# ════════════════════════════════════════════════════════════════════════════
def fig_emergence_schema():
    fig, ax = plt.subplots(figsize=(14, 8))
    ax.set_xlim(0, 14)
    ax.set_ylim(0, 8)
    ax.set_aspect("equal")
    ax.axis("off")
    fig.suptitle("Emergence Dataset #1 — Schema Overview", fontsize=15, fontweight="bold", y=0.97)

    # Three dataset families
    families = [
        ("ED-1A\nMolecular", 1.5, "#2e86c1",
         ["Small molecules", "Conformers", "Barriers", "Torsional freedom"]),
        ("ED-1B\nCoordination", 5.5, "#8e44ad",
         ["Transition metals", "Ligand fields", "Jahn-Teller", "Spin crossover"]),
        ("ED-1C\nBead/Structure", 9.5, "#c0392b",
         ["Polymer beads", "Nanoparticles", "Grain boundaries", "Assembly"]),
    ]

    for label, cx, color, items in families:
        box = FancyBboxPatch((cx - 1.5, 4.5), 3.0, 2.8, boxstyle="round,pad=0.15",
                             facecolor=color, edgecolor="white", linewidth=2, alpha=0.85)
        ax.add_patch(box)
        ax.text(cx, 6.95, label, ha="center", va="center",
                fontsize=12, fontweight="bold", color="white")
        for i, item in enumerate(items):
            ax.text(cx, 6.2 - i * 0.42, f"• {item}", ha="center", fontsize=8, color="#ecf0f1")

    # Anisotropy descriptor
    aniso_box = FancyBboxPatch((0.5, 1.5), 4.0, 2.5, boxstyle="round,pad=0.15",
                               facecolor="#1a5276", edgecolor="white", linewidth=2, alpha=0.8)
    ax.add_patch(aniso_box)
    ax.text(2.5, 3.65, "Anisotropy Descriptor", ha="center", fontsize=11,
            fontweight="bold", color="white")
    aniso_items = [
        "$\\lambda_1, \\lambda_2, \\lambda_3$ eigenvalues",
        "$\\kappa$ asphericity",
        "5 anisotropy types",
    ]
    for i, item in enumerate(aniso_items):
        ax.text(2.5, 3.1 - i * 0.45, item, ha="center", fontsize=9, color="#d6eaf8")

    # Fall detection
    fall_box = FancyBboxPatch((5.0, 1.5), 4.0, 2.5, boxstyle="round,pad=0.15",
                              facecolor="#6c3483", edgecolor="white", linewidth=2, alpha=0.8)
    ax.add_patch(fall_box)
    ax.text(7.0, 3.65, "Fall Detection", ha="center", fontsize=11,
            fontweight="bold", color="white")
    fall_items = [
        "13 categorical fall modes",
        "Severity weighting ($w_1 \\cdots w_5$)",
        "Barrier / RMSD / label criteria",
    ]
    for i, item in enumerate(fall_items):
        ax.text(7.0, 3.1 - i * 0.45, item, ha="center", fontsize=9, color="#e8daef")

    # Sample record
    sample_box = FancyBboxPatch((9.5, 1.5), 4.0, 2.5, boxstyle="round,pad=0.15",
                                facecolor="#922b21", edgecolor="white", linewidth=2, alpha=0.8)
    ax.add_patch(sample_box)
    ax.text(11.5, 3.65, "EmergenceSample", ha="center", fontsize=11,
            fontweight="bold", color="white")
    sample_items = [
        "30+ fields per record",
        "CSV export pipeline",
        "5 benchmark exemplars",
    ]
    for i, item in enumerate(sample_items):
        ax.text(11.5, 3.1 - i * 0.45, item, ha="center", fontsize=9, color="#fadbd8")

    # Arrows from families down
    for cx in [1.5, 5.5, 9.5]:
        ax.annotate("", xy=(cx, 4.2), xytext=(cx, 4.5),
                    arrowprops=dict(arrowstyle="-|>", color="white", lw=2))

    # Bottom label
    ax.text(7.0, 0.8, "EmergenceDataset — Container with query, CSV export, fall detection, summary",
            ha="center", fontsize=11, fontstyle="italic", color="#2c3e50",
            bbox=dict(boxstyle="round,pad=0.3", facecolor="#f9e79f", edgecolor="#d4ac0d", alpha=0.8))

    savefig(fig, "overview_emergence_schema.png")


# ════════════════════════════════════════════════════════════════════════════
# 4. DOCUMENT MAP — ALL 42 SECTIONS
# ════════════════════════════════════════════════════════════════════════════
def fig_document_map():
    fig, ax = plt.subplots(figsize=(16, 10))
    ax.set_xlim(0, 16)
    ax.set_ylim(0, 10)
    ax.set_aspect("equal")
    ax.axis("off")
    fig.suptitle("VSEPR-SIM Documentation Map — 42 Documents, 530+ Pages",
                 fontsize=15, fontweight="bold", y=0.98)

    # Group documents by category
    groups = [
        ("Core Theory\n(§0–§11)", "#1a5276", [
            ("§0 Identity", 14), ("§1 Foundational", 21), ("§2 State Ontology", 17),
            ("§3 Interaction", 17), ("§4 Thermo", 15), ("§5 Integration", 14),
            ("§6 Formation", 15), ("§7 Statistics", 14), ("§8–9 Reaction", 14),
            ("§8b Heat-Gated", 12), ("§10–13 Closing", 23), ("§10b Defect", 12),
            ("§11 Audit", 15),
        ]),
        ("Models &\nArchitecture", "#27ae60", [
            ("Coarse-Grained", 37), ("Seed Bead", 16), ("Fuzzy Ball", 12),
            ("Anisotropic Beads", 8), ("Env-Responsive", 6), ("Polarization", 14),
            ("Bridge Arch.", 5), ("Layer Stack", 9), ("Theory Layer", 11),
            ("Database Arch.", 11), ("Env Particle", 5),
        ]),
        ("Research &\nDatasets", "#8e44ad", [
            ("Viz Inspection", 115), ("Deep Research", 17), ("StateC Notation", 12),
            ("Emergence DS", 9), ("Organometallic", 12), ("Hourglass/LG", 9),
            ("Petalized Alloc", 7), ("Reaction Engine", 4),
        ]),
        ("Methodology &\nReports", "#c0392b", [
            ("Alpha Booklet", 8), ("Methodology 12p", 7), ("Methodology 2p", 2),
            ("EHD Theory", 18), ("XYZ Formats", 11), ("Final Statement", 12),
            ("Phase Reports", 4), ("Test Infra", 5),
        ]),
    ]

    y_base = 8.5
    for gi, (group_name, color, docs) in enumerate(groups):
        gx = 0.3
        gy = y_base - gi * 2.4

        # Group header
        ax.text(0.5, gy + 0.2, group_name, fontsize=11, fontweight="bold",
                color=color, va="top")

        # Document boxes
        total_pages = sum(p for _, p in docs)
        for di, (name, pages) in enumerate(docs):
            bx = 2.8 + (di % 7) * 1.85
            by = gy - (di // 7) * 0.8
            # Size proportional to pages
            w = max(0.4, min(1.7, pages / 30.0))
            box = FancyBboxPatch((bx, by - 0.25), 1.7, 0.55,
                                 boxstyle="round,pad=0.05",
                                 facecolor=color, edgecolor="white",
                                 linewidth=1, alpha=max(0.4, min(0.95, pages/50.0)))
            ax.add_patch(box)
            ax.text(bx + 0.85, by + 0.1, name, ha="center", va="center",
                    fontsize=5.5, color="white", fontweight="bold")
            ax.text(bx + 0.85, by - 0.12, f"{pages}p", ha="center", va="center",
                    fontsize=5, color="#ecf0f1")

        ax.text(15.5, gy - 0.2, f"{total_pages}p", ha="right", fontsize=10,
                fontweight="bold", color=color)

    # Legend
    ax.text(8.0, 0.3, "Box opacity scales with page count  |  84 rendered PNG figures in Visualization Inspection",
            ha="center", fontsize=8, fontstyle="italic", color="#7f8c8d")

    savefig(fig, "overview_document_map.png")


# ════════════════════════════════════════════════════════════════════════════
# 5. MODEL LEVELS DIAGRAM
# ════════════════════════════════════════════════════════════════════════════
def fig_model_levels():
    fig, ax = plt.subplots(figsize=(12, 7))
    ax.set_xlim(0, 12)
    ax.set_ylim(0, 7)
    ax.set_aspect("equal")
    ax.axis("off")
    fig.suptitle("Atomistic Model Levels — From Empirical to Hybrid",
                 fontsize=14, fontweight="bold", y=0.97)

    levels = [
        ("C_Empirical", "Empirical potentials\nLennard-Jones, Morse, EAM",
         "#e74c3c", 0.5),
        ("C_SemiEmpirical", "Extended Hückel, PM7\nParameterized QM",
         "#e67e22", 1.5),
        ("C_DFT", "Density Functional Theory\nKohn-Sham, exchange-correlation",
         "#f1c40f", 2.5),
        ("C_PostHartreeFock", "MP2, CCSD(T)\nCorrelated wavefunction",
         "#2ecc71", 3.5),
        ("C_Reactive", "ReaxFF, DFTB\nBond-breaking dynamics",
         "#3498db", 4.5),
        ("Hybrid_AdRes", "Adaptive Resolution\nMulti-scale coupling",
         "#9b59b6", 5.5),
    ]

    for i, (name, desc, color, y) in enumerate(levels):
        box = FancyBboxPatch((1.0, y), 4.0, 0.8, boxstyle="round,pad=0.1",
                             facecolor=color, edgecolor="white", linewidth=2, alpha=0.85)
        ax.add_patch(box)
        ax.text(3.0, y + 0.4, name, ha="center", va="center",
                fontsize=11, fontweight="bold", color="white",
                path_effects=[pe.withStroke(linewidth=2, foreground="black")])

        # Description
        ax.text(5.5, y + 0.4, desc, ha="left", va="center",
                fontsize=9, color="#2c3e50")

        # Arrow up
        if i < len(levels) - 1:
            ax.annotate("", xy=(3.0, y + 0.8), xytext=(3.0, y + 1.0 + 0.2),
                        arrowprops=dict(arrowstyle="<|-", color="#7f8c8d", lw=1.5))

    # Cost / accuracy axes
    ax.annotate("", xy=(0.5, 6.5), xytext=(0.5, 0.3),
                arrowprops=dict(arrowstyle="-|>", color="#2c3e50", lw=2))
    ax.text(0.3, 3.5, "Accuracy", ha="center", va="center", fontsize=10,
            color="#2c3e50", rotation=90)

    ax.annotate("", xy=(11.5, 6.5), xytext=(11.5, 0.3),
                arrowprops=dict(arrowstyle="-|>", color="#c0392b", lw=2))
    ax.text(11.8, 3.5, "Cost", ha="center", va="center", fontsize=10,
            color="#c0392b", rotation=90)

    savefig(fig, "overview_model_levels.png")


# ════════════════════════════════════════════════════════════════════════════
# 6. SCIENTIFIC WORKFLOW PIPELINE
# ════════════════════════════════════════════════════════════════════════════
def fig_workflow_pipeline():
    fig, ax = plt.subplots(figsize=(15, 5))
    ax.set_xlim(0, 15)
    ax.set_ylim(0, 5)
    ax.set_aspect("equal")
    ax.axis("off")
    fig.suptitle("VSEPR-SIM Scientific Workflow Pipeline", fontsize=14, fontweight="bold", y=0.97)

    stages = [
        ("Input", "Molecule name\nFormula\nSMILES\nXYZ file", "#3498db"),
        ("Normalize", "Identity resolve\nCanonical form\nAlias lookup", "#2ecc71"),
        ("Generate", "Atomistic build\nVSEPR geometry\nForce field", "#f39c12"),
        ("Analyze", "Properties\nAnisotropy\nStability", "#e74c3c"),
        ("Visualize", "3D render\nPlot output\nInspection GUI", "#9b59b6"),
        ("Report", "LaTeX export\nCSV data\nFigures", "#1abc9c"),
    ]

    for i, (name, desc, color) in enumerate(stages):
        x = 0.5 + i * 2.4
        box = FancyBboxPatch((x, 1.0), 2.0, 3.0, boxstyle="round,pad=0.15",
                             facecolor=color, edgecolor="white", linewidth=2, alpha=0.85)
        ax.add_patch(box)
        ax.text(x + 1.0, 3.6, name, ha="center", va="center",
                fontsize=12, fontweight="bold", color="white",
                path_effects=[pe.withStroke(linewidth=2, foreground="black")])
        ax.text(x + 1.0, 2.2, desc, ha="center", va="center",
                fontsize=8, color="white", linespacing=1.5)

        # Stage number
        circle = plt.Circle((x + 1.0, 0.5), 0.25, facecolor=color, edgecolor="white", linewidth=2)
        ax.add_patch(circle)
        ax.text(x + 1.0, 0.5, str(i+1), ha="center", va="center",
                fontsize=11, fontweight="bold", color="white")

        # Arrow to next
        if i < len(stages) - 1:
            ax.annotate("", xy=(x + 2.4, 2.5), xytext=(x + 2.0, 2.5),
                        arrowprops=dict(arrowstyle="-|>", color="white", lw=2.5))

    savefig(fig, "overview_workflow_pipeline.png")


# ════════════════════════════════════════════════════════════════════════════
# 7. EHD PHYSICS SUMMARY DIAGRAM
# ════════════════════════════════════════════════════════════════════════════
def fig_ehd_summary():
    fig, ax = plt.subplots(figsize=(12, 7))
    ax.set_xlim(0, 12)
    ax.set_ylim(0, 7)
    ax.set_aspect("equal")
    ax.axis("off")
    fig.suptitle("Electrohydrodynamic (EHD) Module — Physics Overview",
                 fontsize=14, fontweight="bold", y=0.97)

    # Central EHD box
    ehd_box = FancyBboxPatch((4.0, 2.5), 4.0, 2.0, boxstyle="round,pad=0.2",
                             facecolor="#2c3e50", edgecolor="white", linewidth=3, alpha=0.9)
    ax.add_patch(ehd_box)
    ax.text(6.0, 3.5, "EHD Simulation\nKernel", ha="center", va="center",
            fontsize=14, fontweight="bold", color="white")

    # Surrounding physics modules
    modules = [
        (1.5, 5.5, "Electric Field\n$\\nabla^2\\phi = -\\rho/\\epsilon$", "#e74c3c"),
        (6.0, 5.5, "Fluid Dynamics\nNavier-Stokes", "#3498db"),
        (10.0, 5.5, "Particle\nTransport", "#2ecc71"),
        (1.5, 1.0, "Pump\nConfigurations", "#f39c12"),
        (6.0, 1.0, "Combustion\nKinetics", "#e67e22"),
        (10.0, 1.0, "Reactive\nMultiphase", "#9b59b6"),
    ]

    for mx, my, label, color in modules:
        mbox = FancyBboxPatch((mx - 1.2, my - 0.5), 2.4, 1.0, boxstyle="round,pad=0.1",
                              facecolor=color, edgecolor="white", linewidth=2, alpha=0.8)
        ax.add_patch(mbox)
        ax.text(mx, my, label, ha="center", va="center",
                fontsize=9, fontweight="bold", color="white")

        # Arrow to/from center
        cx, cy = 6.0, 3.5
        if my > 3.5:
            ax.annotate("", xy=(min(max(mx, 4.5), 7.5), 4.5), xytext=(mx, my - 0.5),
                        arrowprops=dict(arrowstyle="-|>", color=color, lw=1.5, alpha=0.6))
        else:
            ax.annotate("", xy=(mx, my + 0.5), xytext=(min(max(mx, 4.5), 7.5), 2.5),
                        arrowprops=dict(arrowstyle="-|>", color=color, lw=1.5, alpha=0.6))

    # Stats
    ax.text(6.0, 0.2, "26 headers  |  36 tests  |  18-page LaTeX  |  4 pump configs  |  3 mechanisms",
            ha="center", fontsize=9, fontstyle="italic", color="#7f8c8d")

    savefig(fig, "overview_ehd_summary.png")


# ════════════════════════════════════════════════════════════════════════════
# 8. TESTING COVERAGE CHART
# ════════════════════════════════════════════════════════════════════════════
def fig_test_coverage():
    fig, ax = plt.subplots(figsize=(14, 6))

    groups = [
        "Group 1\nState", "Group 2\nBead", "Group 3\nForce",
        "Group 4\nInertia", "Group 5\nAlign", "Group 6\nEnv",
        "Group 7\nPredict", "Group 8\nCensus", "Group 9\nConformer",
        "Group 10\nFuzz", "Group 11\nAlpha\n(100+)", "Group 12\nDeep\n(100+)",
        "Group 13\nStateC\n(100+)", "Group 14\nEmergence\n(100+)",
        "Group 15\nFormation\n(100+)",
    ]
    counts = [4, 3, 5, 4, 3, 4, 3, 5, 4, 3, 100, 100, 100, 100, 100]
    colors = ["#3498db"] * 10 + ["#e74c3c", "#e74c3c", "#9b59b6", "#9b59b6", "#27ae60"]

    bars = ax.bar(range(len(groups)), counts, color=colors, edgecolor="white", linewidth=2, alpha=0.85)

    for bar, count in zip(bars, counts):
        label = "100+" if count >= 100 else str(count)
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 1.5,
                label, ha="center", va="bottom", fontsize=9, fontweight="bold")

    ax.set_xticks(range(len(groups)))
    ax.set_xticklabels(groups, fontsize=6, rotation=0)
    ax.set_ylabel("Test Count", fontsize=12)
    ax.set_title("Test Groups — 15 Groups, 500+ Test Cases", fontsize=14, fontweight="bold")
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.set_ylim(0, 120)

    # Total
    total = sum(counts)
    ax.text(len(groups) - 1, 115, f"Total: {total}+ tests",
            ha="right", fontsize=12, fontweight="bold", color="#2c3e50")

    savefig(fig, "overview_test_coverage.png")


# ════════════════════════════════════════════════════════════════════════════
# 9. PAGE COUNT BAR CHART
# ════════════════════════════════════════════════════════════════════════════
def fig_page_counts():
    fig, ax = plt.subplots(figsize=(14, 8))

    docs_pages = [
        ("Viz Inspection", 115), ("CG Layer", 37), ("§10–13 Closing", 23),
        ("§1 Foundational", 21), ("EHD Theory", 18), ("Deep Research", 17),
        ("§2 State Ontol.", 17), ("§3 Interaction", 17), ("Seed Bead", 16),
        ("§4 Thermo", 15), ("§11 Audit", 15), ("§6 Formation", 15),
        ("§5 Integration", 14), ("§7 Statistics", 14), ("§8–9 Reaction", 14),
        ("Polarization", 14), ("§0 Identity", 14), ("StateC", 12),
        ("Final Statement", 12), ("Fuzzy Ball", 12), ("Organometallic", 12),
        ("§8b Heat-Gated", 12), ("§10b Defect", 12), ("Database", 11),
        ("Theory Layer", 11), ("XYZ Formats", 11), ("Emergence DS", 9),
        ("Hourglass", 9), ("Layer Stack", 9), ("Alpha Booklet", 8),
        ("Petalized", 7), ("Method 12p", 7), ("Env Particle", 5),
        ("Reaction Eng.", 4), ("Method 2p", 2),
    ]

    names = [d[0] for d in docs_pages]
    pages = [d[1] for d in docs_pages]

    # Color by category
    def get_color(name):
        if name.startswith("§"):
            return "#1a5276"
        elif name in ["Viz Inspection", "Deep Research", "StateC", "Emergence DS"]:
            return "#8e44ad"
        elif name in ["EHD Theory", "CG Layer", "Seed Bead", "Fuzzy Ball",
                       "Polarization", "Organometallic", "Database", "Theory Layer"]:
            return "#27ae60"
        else:
            return "#c0392b"

    colors = [get_color(n) for n in names]

    bars = ax.barh(range(len(names)), pages, color=colors, edgecolor="white",
                   linewidth=1, alpha=0.85)

    ax.set_yticks(range(len(names)))
    ax.set_yticklabels(names, fontsize=7)
    ax.invert_yaxis()
    ax.set_xlabel("Pages", fontsize=12)
    ax.set_title(f"Document Page Counts — {sum(pages)} Total Pages",
                 fontsize=14, fontweight="bold")
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    for bar, p in zip(bars, pages):
        ax.text(bar.get_width() + 0.5, bar.get_y() + bar.get_height()/2,
                str(p), ha="left", va="center", fontsize=7, fontweight="bold")

    # Legend
    legend_items = [
        mpatches.Patch(color="#1a5276", label="Core Theory (§0–§11)"),
        mpatches.Patch(color="#8e44ad", label="Research & Datasets"),
        mpatches.Patch(color="#27ae60", label="Models & Architecture"),
        mpatches.Patch(color="#c0392b", label="Methodology & Reports"),
    ]
    ax.legend(handles=legend_items, loc="lower right", fontsize=9)

    savefig(fig, "overview_page_counts.png")


# ════════════════════════════════════════════════════════════════════════════
# 10. ANISOTROPY EIGENVALUE ILLUSTRATION
# ════════════════════════════════════════════════════════════════════════════
def fig_anisotropy_illustration():
    fig, axes = plt.subplots(1, 3, figsize=(15, 5))
    fig.suptitle("Anisotropy Types — Eigenvalue Spectra of the Gyration Tensor",
                 fontsize=14, fontweight="bold", y=1.0)

    cases = [
        ("Isotropic\n(Spherical)", [1.0, 1.0, 1.0], "#3498db"),
        ("Oblate\n(Disk-like)", [0.3, 1.0, 1.0], "#e74c3c"),
        ("Prolate\n(Rod-like)", [1.0, 0.3, 0.3], "#2ecc71"),
    ]

    for ax, (title, eigenvals, color) in zip(axes, cases):
        eigenvals = np.array(eigenvals)
        eigenvals /= eigenvals.max()

        # Draw ellipsoid cross-section
        theta = np.linspace(0, 2 * np.pi, 100)
        for i, (a, b, alpha_val) in enumerate([
            (eigenvals[0], eigenvals[1], 0.3),
            (eigenvals[0], eigenvals[2], 0.2),
            (eigenvals[1], eigenvals[2], 0.15),
        ]):
            x = a * np.cos(theta)
            y = b * np.sin(theta)
            ax.fill(x, y, color=color, alpha=alpha_val)
            ax.plot(x, y, color=color, lw=2, alpha=0.7)

        # Eigenvalue bars
        bar_x = [-0.6, 0.0, 0.6]
        for bx, ev in zip(bar_x, eigenvals):
            ax.bar(bx, ev * 0.3 - 1.5, 0.15, bottom=-1.5, color=color, alpha=0.8,
                   edgecolor="white")

        kappa = 1.0 - 3.0 * (eigenvals[0]*eigenvals[1] + eigenvals[1]*eigenvals[2] +
                              eigenvals[0]*eigenvals[2]) / (sum(eigenvals))**2
        ax.set_title(title, fontsize=12, fontweight="bold")
        ax.text(0, -1.8, f"$\\kappa = {kappa:.2f}$", ha="center", fontsize=11)
        ax.set_xlim(-1.5, 1.5)
        ax.set_ylim(-2.0, 1.5)
        ax.set_aspect("equal")
        ax.axis("off")

    plt.tight_layout()
    savefig(fig, "overview_anisotropy_types.png")


# ════════════════════════════════════════════════════════════════════════════
# 11. DEEP RESEARCH PHASES
# ════════════════════════════════════════════════════════════════════════════
def fig_deep_research_phases():
    fig, ax = plt.subplots(figsize=(14, 5))
    ax.set_xlim(0, 14)
    ax.set_ylim(0, 5)
    ax.set_aspect("equal")
    ax.axis("off")
    fig.suptitle("Deep Research Engine — 4-Phase Pipeline",
                 fontsize=14, fontweight="bold", y=0.97)

    phases = [
        ("Phase 1\nSCAN", "Enumerate\npathways", "#e74c3c"),
        ("Phase 2\nFILTER", "Thermodynamic\nscreening", "#f39c12"),
        ("Phase 3\nDEEP", "Detailed\nanalysis", "#3498db"),
        ("Phase 4\nRANK", "SHA-512 hash\nfinal ranking", "#2ecc71"),
    ]

    for i, (name, desc, color) in enumerate(phases):
        x = 0.5 + i * 3.4
        box = FancyBboxPatch((x, 1.0), 2.8, 3.0, boxstyle="round,pad=0.15",
                             facecolor=color, edgecolor="white", linewidth=3, alpha=0.85)
        ax.add_patch(box)
        ax.text(x + 1.4, 3.2, name, ha="center", va="center",
                fontsize=13, fontweight="bold", color="white",
                path_effects=[pe.withStroke(linewidth=2, foreground="black")])
        ax.text(x + 1.4, 1.8, desc, ha="center", va="center",
                fontsize=10, color="white", alpha=0.9)

        if i < len(phases) - 1:
            ax.annotate("", xy=(x + 3.4, 2.5), xytext=(x + 2.8, 2.5),
                        arrowprops=dict(arrowstyle="-|>", color="white", lw=3, mutation_scale=20))

    ax.text(7.0, 0.5, "220 pathways tested  |  Python driver  |  17-page LaTeX report",
            ha="center", fontsize=10, fontstyle="italic", color="#7f8c8d")

    savefig(fig, "overview_deep_research_phases.png")


# ════════════════════════════════════════════════════════════════════════════
# 12. COARSE-GRAINED BEAD TYPES
# ════════════════════════════════════════════════════════════════════════════
def fig_bead_types():
    fig, ax = plt.subplots(figsize=(12, 7))
    ax.set_xlim(0, 12)
    ax.set_ylim(0, 7)
    ax.set_aspect("equal")
    ax.axis("off")
    fig.suptitle("Coarse-Grained Bead Architecture",
                 fontsize=14, fontweight="bold", y=0.97)

    bead_types = [
        ("Backbone", "#e74c3c", 2.0, 5.5, 0.6),
        ("Side Chain", "#3498db", 4.5, 5.5, 0.45),
        ("Terminal", "#2ecc71", 7.0, 5.5, 0.35),
        ("Cross-link", "#f39c12", 9.5, 5.5, 0.5),
    ]

    for name, color, cx, cy, r in bead_types:
        circle = plt.Circle((cx, cy), r, facecolor=color, edgecolor="white",
                            linewidth=3, alpha=0.85)
        ax.add_patch(circle)
        ax.text(cx, cy, name, ha="center", va="center", fontsize=8,
                fontweight="bold", color="white",
                path_effects=[pe.withStroke(linewidth=2, foreground="black")])

    # Chain illustration
    chain_beads = [
        (1.5, 3.0, 0.4, "#e74c3c"),
        (3.0, 3.0, 0.4, "#e74c3c"),
        (4.5, 3.0, 0.4, "#e74c3c"),
        (6.0, 3.0, 0.4, "#e74c3c"),
        (7.5, 3.0, 0.4, "#e74c3c"),
        (9.0, 3.0, 0.35, "#2ecc71"),

        # Side chains
        (3.0, 4.0, 0.3, "#3498db"),
        (6.0, 4.0, 0.3, "#3498db"),
        (6.0, 2.0, 0.3, "#3498db"),

        # Cross-link
        (4.5, 1.5, 0.35, "#f39c12"),
    ]

    for cx, cy, r, color in chain_beads:
        circle = plt.Circle((cx, cy), r, facecolor=color, edgecolor="white",
                            linewidth=2, alpha=0.8)
        ax.add_patch(circle)

    # Bonds
    bonds = [
        (1.5, 3.0, 3.0, 3.0), (3.0, 3.0, 4.5, 3.0),
        (4.5, 3.0, 6.0, 3.0), (6.0, 3.0, 7.5, 3.0),
        (7.5, 3.0, 9.0, 3.0),
        (3.0, 3.0, 3.0, 4.0), (6.0, 3.0, 6.0, 4.0),
        (6.0, 3.0, 6.0, 2.0),
        (4.5, 3.0, 4.5, 1.5),
    ]
    for x1, y1, x2, y2 in bonds:
        ax.plot([x1, x2], [y1, y2], color="#7f8c8d", lw=3, zorder=0)

    ax.text(5.25, 0.7, "Structural roles: Backbone / Side Chain / Terminal / Cross-link",
            ha="center", fontsize=10, fontstyle="italic", color="#2c3e50")

    savefig(fig, "overview_bead_types.png")


# ════════════════════════════════════════════════════════════════════════════
# MAIN
# ════════════════════════════════════════════════════════════════════════════
def main():
    print("=" * 60)
    print("  VSEPR-SIM Overview Figure Generator")
    print("=" * 60)

    generators = [
        ("Architecture Map",       fig_architecture_map),
        ("StateC Flow",            fig_statec_flow),
        ("Emergence Schema",       fig_emergence_schema),
        ("Document Map",           fig_document_map),
        ("Model Levels",           fig_model_levels),
        ("Workflow Pipeline",      fig_workflow_pipeline),
        ("EHD Summary",            fig_ehd_summary),
        ("Test Coverage",          fig_test_coverage),
        ("Page Counts",            fig_page_counts),
        ("Anisotropy Types",       fig_anisotropy_illustration),
        ("Deep Research Phases",   fig_deep_research_phases),
        ("Bead Types",             fig_bead_types),
    ]

    for name, fn in generators:
        print(f"\n  Generating: {name}")
        try:
            fn()
        except Exception as e:
            print(f"  [FAIL] {name}: {e}")

    print(f"\n{'=' * 60}")
    print(f"  Done. {len(generators)} overview figures in {FIG_DIR}")
    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()
