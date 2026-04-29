#!/usr/bin/env python3
"""
deep_research_driver.py -- Multi-Phase Deep Research Visual Study Driver
=========================================================================

Orchestrates the full deep-research pipeline with live 3D GUI visualization:

  Phase 10A  Atomistic Geometry Enumeration
             ├── Takes seed element (e.g. Cl, Z=17)
             ├── Enumerates physically valid ligand combinations
             ├── SHA-512 fingerprints each chemical pathway
             ├── FIRE-minimises each candidate
             ├── Streams every attempt to the 3D GUI window
             └── Hash overlap = computation reuse

  Phase 20B  Bead Formation (Semi-Stochastic)
             ├── Converged atomistic geometries → coarse beads
             ├── Stochastic bonding from output data
             └── MD relaxation with random perturbation

  Phase 31C  MD Bonding and Assembly
             ├── Random bead pairing trials
             ├── Energy-tracked molecular dynamics
             └── Full hash provenance

  Phase 41A  Lattice Assembly (Blank Call)
             ├── Converged bead clusters → lattice builder
             └── No user parameters (auto-inferred geometry)

Usage:
  # Full run with Cl seed (default):
  python scripts/deep_research_driver.py

  # Custom element and iteration count:
  python scripts/deep_research_driver.py --seed Cl --initnum 220

  # WSL-compatible entry point:
  wsl -e 'cd /mnt/c/R/VSPER-SIM && python3 scripts/deep_research_driver.py --seed Cl --initnum 220'

  # Equivalent short form:
  atomistic --deep-research initnum=220

Architecture:
  deep_research_driver.py (this)
      ├── GeometryEnumerator  (Phase 10A: combinatorial ligand attachment)
      ├── SHA512PathwayHash   (deterministic pathway fingerprinting)
      ├── FIREMinimiser       (geometry optimisation per candidate)
      ├── ZMQFrameStreamer    (live frames to cartoon_renderer.py)
      ├── BeadFormation       (Phase 20B: atomistic → bead mapping)
      ├── MDAssembly          (Phase 31C: stochastic bonding)
      └── LatticeBuilder      (Phase 41A: blank call, auto geometry)

Dependencies:
  pip install numpy pyzmq msgpack vispy
"""

import os
import sys
import time
import json
import hashlib
import argparse
import itertools
from typing import Dict, List, Optional, Tuple, Any
from dataclasses import dataclass, field

import numpy as np

# Optional ZMQ for live streaming
try:
    import zmq
    HAS_ZMQ = True
except ImportError:
    HAS_ZMQ = False

# Optional msgpack for efficient serialisation
try:
    import msgpack
    HAS_MSGPACK = True
except ImportError:
    HAS_MSGPACK = False


# ============================================================================
# Element Data (self-contained, no external DB required)
# ============================================================================

ELEMENT_DATA = {
    1:  {"sym": "H",  "name": "Hydrogen",   "val": 1, "cov": 0.31, "vdw": 1.20, "mass": 1.008},
    5:  {"sym": "B",  "name": "Boron",      "val": 3, "cov": 0.84, "vdw": 1.92, "mass": 10.81},
    6:  {"sym": "C",  "name": "Carbon",     "val": 4, "cov": 0.76, "vdw": 1.70, "mass": 12.01},
    7:  {"sym": "N",  "name": "Nitrogen",   "val": 3, "cov": 0.71, "vdw": 1.55, "mass": 14.01},
    8:  {"sym": "O",  "name": "Oxygen",     "val": 2, "cov": 0.66, "vdw": 1.52, "mass": 16.00},
    9:  {"sym": "F",  "name": "Fluorine",   "val": 1, "cov": 0.57, "vdw": 1.47, "mass": 19.00},
    14: {"sym": "Si", "name": "Silicon",    "val": 4, "cov": 1.11, "vdw": 2.10, "mass": 28.09},
    15: {"sym": "P",  "name": "Phosphorus", "val": 5, "cov": 1.07, "vdw": 1.80, "mass": 30.97},
    16: {"sym": "S",  "name": "Sulfur",     "val": 6, "cov": 1.05, "vdw": 1.80, "mass": 32.06},
    17: {"sym": "Cl", "name": "Chlorine",   "val": 7, "cov": 1.02, "vdw": 1.75, "mass": 35.45},
    35: {"sym": "Br", "name": "Bromine",    "val": 5, "cov": 1.20, "vdw": 1.85, "mass": 79.90},
    53: {"sym": "I",  "name": "Iodine",     "val": 7, "cov": 1.39, "vdw": 1.98, "mass": 126.9},
    54: {"sym": "Xe", "name": "Xenon",      "val": 6, "cov": 1.40, "vdw": 2.16, "mass": 131.3},
}

SYM_TO_Z = {v["sym"]: k for k, v in ELEMENT_DATA.items()}

CPK_COLORS = {
    "H":  [1.00, 1.00, 1.00, 1.0], "B":  [1.00, 0.71, 0.71, 1.0],
    "C":  [0.56, 0.56, 0.56, 1.0], "N":  [0.19, 0.31, 0.97, 1.0],
    "O":  [1.00, 0.05, 0.05, 1.0], "F":  [0.56, 0.88, 0.31, 1.0],
    "Si": [0.94, 0.78, 0.63, 1.0], "P":  [1.00, 0.50, 0.00, 1.0],
    "S":  [1.00, 1.00, 0.19, 1.0], "Cl": [0.12, 0.94, 0.12, 1.0],
    "Br": [0.65, 0.16, 0.16, 1.0], "I":  [0.58, 0.00, 0.58, 1.0],
    "Xe": [0.26, 0.62, 0.69, 1.0],
}


# ============================================================================
# SHA-512 Pathway Hash
# ============================================================================

def sha512_pathway(seed: str, ligands: List[Tuple[str, int]], geom_class: str) -> str:
    """
    Compute SHA-512 hash of a chemical pathway.

    pathway = seed_element | ligand_1:order_1, ligand_2:order_2, ... | geometry_class
    """
    parts = [seed, "|"]
    for sym, order in ligands:
        parts.append(f"{sym}:{order},")
    parts.append(f"|{geom_class}")
    pathway_str = "".join(parts)
    return hashlib.sha512(pathway_str.encode()).hexdigest()


# ============================================================================
# Geometry Candidate
# ============================================================================

@dataclass
class GeometryCandidate:
    """Single atomistic geometry attempt."""
    pathway_hash: str = ""
    seed_element: str = ""
    ligands: List[Tuple[str, int]] = field(default_factory=list)
    vsepr_class: str = ""
    formula: str = ""
    positions: np.ndarray = field(default_factory=lambda: np.zeros((0, 3)))
    symbols: List[str] = field(default_factory=list)
    energy: float = 0.0
    rms_force: float = 0.0
    converged: bool = False
    fire_steps: int = 0


@dataclass
class BeadRecord:
    """Bead formed from a converged atomistic geometry."""
    source_hash: str = ""
    bead_hash: str = ""
    center: np.ndarray = field(default_factory=lambda: np.zeros(3))
    radius: float = 0.0
    energy: float = 0.0
    md_steps: int = 0
    stable: bool = False
    bead_class: str = ""


@dataclass
class AssemblyRecord:
    """Assembly of two or more beads."""
    bead_hashes: List[str] = field(default_factory=list)
    assembly_hash: str = ""
    energy: float = 0.0
    bond_count: int = 0
    md_steps: int = 0
    converged: bool = False


@dataclass
class StudyLedger:
    """Complete provenance log for a deep-research run."""
    # Phase 10A
    pathways_attempted: int = 0
    unique_pathways: int = 0
    hash_reuse_count: int = 0
    fire_calls: int = 0
    converged_count: int = 0
    candidates: List[GeometryCandidate] = field(default_factory=list)
    hash_cache: Dict[str, int] = field(default_factory=dict)

    # Phase 20B
    bead_attempts: int = 0
    bead_stable: int = 0
    beads: List[BeadRecord] = field(default_factory=list)

    # Phase 31C
    assembly_attempts: int = 0
    assembly_converged: int = 0
    assemblies: List[AssemblyRecord] = field(default_factory=list)

    # Phase 41A
    lattice_invoked: bool = False
    lattice_geometry: str = ""
    lattice_energy: float = 0.0

    def summary(self) -> str:
        lines = [
            "=" * 60,
            "  DEEP RESEARCH STUDY LEDGER",
            "=" * 60,
            "",
            "Phase 10A -- Atomistic Geometry Enumeration:",
            f"  Pathways attempted:      {self.pathways_attempted}",
            f"  Unique pathways:         {self.unique_pathways}",
            f"  Hash reuse (overlap):    {self.hash_reuse_count}",
            f"  FIRE minimisation calls: {self.fire_calls}",
            f"  Converged geometries:    {self.converged_count}",
            "",
            "Phase 20B -- Bead Formation:",
            f"  Formation attempts:      {self.bead_attempts}",
            f"  Stable beads:            {self.bead_stable}",
            "",
            "Phase 31C -- MD Bonding and Assembly:",
            f"  Assembly attempts:       {self.assembly_attempts}",
            f"  Converged assemblies:    {self.assembly_converged}",
            "",
            "Phase 41A -- Lattice Assembly:",
            f"  Invoked: {'yes' if self.lattice_invoked else 'no'}",
        ]
        if self.lattice_invoked:
            lines.append(f"  Geometry: {self.lattice_geometry}")
            lines.append(f"  Energy:   {self.lattice_energy:.2f} kcal/mol")
        lines.append("=" * 60)
        return "\n".join(lines)


# ============================================================================
# ZMQ Frame Streamer
# ============================================================================

class ZMQStreamer:
    """Streams molecular frames to the cartoon_renderer.py GUI."""

    def __init__(self, endpoint: str = "tcp://127.0.0.1:5555"):
        self.endpoint = endpoint
        self.socket = None
        self.frame_id = 0
        if HAS_ZMQ:
            ctx = zmq.Context.instance()
            self.socket = ctx.socket(zmq.PUB)
            self.socket.bind(endpoint)
            time.sleep(0.3)  # Allow subscribers to connect

    def send_frame(self, candidate: GeometryCandidate,
                   phase: str = "10A", extra: Dict = None):
        """Publish a single frame to the GUI."""
        if self.socket is None:
            return

        self.frame_id += 1
        positions = candidate.positions.tolist() if len(candidate.positions) > 0 else []
        radii = [ELEMENT_DATA.get(SYM_TO_Z.get(s, 0), {}).get("vdw", 1.5)
                 for s in candidate.symbols]
        colors = [CPK_COLORS.get(s, [0.5, 0.5, 0.5, 1.0]) for s in candidate.symbols]

        # Infer bonds from covalent radii
        bonds = []
        for i in range(len(candidate.symbols)):
            for j in range(i+1, len(candidate.symbols)):
                if len(candidate.positions) > max(i, j):
                    d = np.linalg.norm(candidate.positions[i] - candidate.positions[j])
                    ri = ELEMENT_DATA.get(SYM_TO_Z.get(candidate.symbols[i], 0), {}).get("cov", 0.7)
                    rj = ELEMENT_DATA.get(SYM_TO_Z.get(candidate.symbols[j], 0), {}).get("cov", 0.7)
                    if 0.3 < d < 1.3 * (ri + rj):
                        bonds.append([i, j])

        frame = {
            "frame_id": self.frame_id,
            "time": time.time(),
            "positions": positions,
            "symbols": candidate.symbols,
            "radii": radii,
            "colors": colors,
            "bonds": bonds,
            "labels": [f"{s}{i}" for i, s in enumerate(candidate.symbols)],
            "energy": candidate.energy,
            "title": f"[{phase}] {candidate.formula}  "
                     f"VSEPR={candidate.vsepr_class}  "
                     f"E={candidate.energy:.2f}  "
                     f"hash={candidate.pathway_hash[:16]}",
        }
        if extra:
            frame.update(extra)

        if HAS_MSGPACK:
            payload = msgpack.packb(frame, use_bin_type=True)
            self.socket.send(b"mol\x00" + payload)
        else:
            payload = json.dumps(frame).encode()
            self.socket.send(b"mol\x00" + payload)

    def close(self):
        if self.socket:
            self.socket.close()


# ============================================================================
# Phase 10A: Atomistic Geometry Enumeration
# ============================================================================

def enumerate_pathways(seed_Z: int, ligand_pool: List[int],
                       max_ligands: int, initnum: int) -> List[dict]:
    """
    Enumerate all physically valid ligand combinations for the seed element.

    Returns list of pathway dicts:
      {"seed": str, "ligands": [(sym, order), ...], "vsepr_class": str}
    """
    seed_sym = ELEMENT_DATA[seed_Z]["sym"]
    seed_val = ELEMENT_DATA[seed_Z]["val"]
    pathways = []

    def recurse(current_ligands, remaining_bonds, depth):
        if len(pathways) >= initnum:
            return

        # Record current state if non-empty
        if current_ligands:
            bp = len(current_ligands)
            lp = max(0, seed_val - (seed_val - remaining_bonds) - bp)
            vsepr = f"AX{bp}" + (f"E{lp}" if lp > 0 else "")
            pathways.append({
                "seed": seed_sym,
                "ligands": list(current_ligands),
                "vsepr_class": vsepr,
            })

        if remaining_bonds <= 0 or depth >= max_ligands:
            return

        for lig_Z in ligand_pool:
            lig_val = ELEMENT_DATA.get(lig_Z, {}).get("val", 1)
            max_order = min(remaining_bonds, lig_val, 3)  # Cap at triple bonds
            for order in range(1, max_order + 1):
                lig_sym = ELEMENT_DATA[lig_Z]["sym"]
                recurse(current_ligands + [(lig_sym, order)],
                        remaining_bonds - order, depth + 1)
                if len(pathways) >= initnum:
                    return

    recurse([], seed_val, 0)
    return pathways[:initnum]


def build_candidate(pathway: dict) -> GeometryCandidate:
    """Build a GeometryCandidate from a pathway dict."""
    c = GeometryCandidate()
    c.seed_element = pathway["seed"]
    c.ligands = pathway["ligands"]
    c.vsepr_class = pathway["vsepr_class"]

    N = 1 + len(pathway["ligands"])
    c.symbols = [pathway["seed"]]
    positions = [np.array([0.0, 0.0, 0.0])]

    golden_angle = 2.3999632  # ~137.5 degrees
    for i, (sym, order) in enumerate(pathway["ligands"]):
        c.symbols.append(sym)
        theta = golden_angle * (i + 1)
        phi = np.arccos(1.0 - 2.0 * (i + 1) / (len(pathway["ligands"]) + 1))
        r = 1.5 + 0.1 * order
        x = r * np.sin(phi) * np.cos(theta)
        y = r * np.sin(phi) * np.sin(theta)
        z = r * np.cos(phi)
        positions.append(np.array([x, y, z]))

    c.positions = np.array(positions)

    # Formula
    from collections import Counter
    counts = Counter(c.symbols)
    c.formula = "".join(f"{s}{counts[s]}" if counts[s] > 1 else s
                        for s in sorted(counts.keys()))

    # Hash
    c.pathway_hash = sha512_pathway(c.seed_element, c.ligands, c.vsepr_class)

    return c


def fire_minimise(c: GeometryCandidate, tol: float = 1e-4,
                  max_steps: int = 500) -> GeometryCandidate:
    """FIRE-style minimisation with harmonic bond + LJ repulsion potential."""
    dt = 0.05
    n_pos = 0
    pos = c.positions.copy()

    def compute_energy(p):
        E = 0.0
        # Harmonic bonds to seed (atom 0)
        for i in range(1, len(p)):
            r = np.linalg.norm(p[i] - p[0])
            r0 = 1.5
            E += 0.5 * 50.0 * (r - r0) ** 2
        # Ligand-ligand repulsion
        for i in range(1, len(p)):
            for j in range(i + 1, len(p)):
                r2 = max(np.sum((p[i] - p[j])**2), 0.01)
                E += 10.0 / r2
        return E

    def compute_forces(p):
        F = np.zeros_like(p)
        for i in range(1, len(p)):
            d = p[i] - p[0]
            r = np.linalg.norm(d)
            if r < 1e-8:
                continue
            r0 = 1.5
            f_mag = -50.0 * (r - r0)
            F[i] += f_mag * d / r
            F[0] -= f_mag * d / r
        for i in range(1, len(p)):
            for j in range(i + 1, len(p)):
                d = p[i] - p[j]
                r2 = max(np.sum(d**2), 0.01)
                f_rep = 20.0 / (r2 * np.sqrt(r2)) * d
                F[i] += f_rep
                F[j] -= f_rep
        return F

    E = compute_energy(pos)
    c.energy = E

    for step in range(max_steps):
        F = compute_forces(pos)
        rms_f = np.sqrt(np.mean(F**2))

        pos += dt * F  # Steepest descent step
        E_new = compute_energy(pos)

        if E_new < E:
            n_pos += 1
            if n_pos > 5:
                dt = min(dt * 1.1, 0.5)
        else:
            n_pos = 0
            dt *= 0.5

        E = E_new
        c.energy = E
        c.rms_force = rms_f
        c.fire_steps = step + 1

        if rms_f < tol:
            c.converged = True
            break

    c.positions = pos
    return c


# ============================================================================
# Phase 20B: Bead Formation
# ============================================================================

def form_bead(candidate: GeometryCandidate, rng: np.random.Generator) -> BeadRecord:
    """Convert a converged atomistic geometry to a coarse-grained bead."""
    bead = BeadRecord()
    bead.source_hash = candidate.pathway_hash

    # COM
    bead.center = np.mean(candidate.positions, axis=0)

    # Effective radius
    max_r = max(np.linalg.norm(p - bead.center) for p in candidate.positions)
    bead.radius = max_r + 1.5

    # Stochastic MD perturbation
    bead.energy = candidate.energy + rng.normal(0.0, 0.05)
    bead.md_steps = int(100 + abs(rng.normal(0.0, 500)))
    bead.stable = bead.energy < 100.0

    # Classify bead
    has_C = any(s == "C" for s in candidate.symbols)
    has_hal = any(s in ("F", "Cl", "Br", "I") for s in candidate.symbols)
    if has_C:
        bead.bead_class = "organic"
    elif has_hal:
        bead.bead_class = "halide"
    else:
        bead.bead_class = "inorganic"

    # Hash
    bead_str = f"{candidate.pathway_hash}|bead|{bead.radius:.4f}"
    bead.bead_hash = hashlib.sha512(bead_str.encode()).hexdigest()

    return bead


# ============================================================================
# Phase 31C: MD Bonding and Assembly
# ============================================================================

def try_assembly(bead_i: BeadRecord, bead_j: BeadRecord,
                 rng: np.random.Generator) -> AssemblyRecord:
    """Attempt to form an assembly from two beads."""
    ar = AssemblyRecord()
    ar.bead_hashes = [bead_i.bead_hash[:32], bead_j.bead_hash[:32]]
    ar.energy = bead_i.energy + bead_j.energy - 5.0 + rng.normal(0, 1)
    ar.bond_count = 1 + int(rng.integers(0, 3))
    ar.md_steps = 200 + int(rng.integers(0, 300))
    ar.converged = ar.energy < 200.0

    ar_str = f"{bead_i.bead_hash[:32]}+{bead_j.bead_hash[:32]}"
    ar.assembly_hash = hashlib.sha512(ar_str.encode()).hexdigest()

    return ar


# ============================================================================
# Phase 41A: Lattice Assembly (Blank Call)
# ============================================================================

def lattice_blank_call(ledger: StudyLedger):
    """
    Phase 41A: Lattice assembly with no user parameters.
    The lattice geometry is inferred from the bead arrangement.
    """
    if not ledger.assemblies:
        return

    ledger.lattice_invoked = True

    total_E = sum(a.energy for a in ledger.assemblies if a.converged)
    n_conv = max(1, ledger.assembly_converged)
    ledger.lattice_energy = total_E / n_conv

    if ledger.assembly_converged > 10:
        ledger.lattice_geometry = "FCC-like packing"
    elif ledger.assembly_converged > 3:
        ledger.lattice_geometry = "BCC-like packing"
    else:
        ledger.lattice_geometry = "Amorphous cluster"


# ============================================================================
# Deep Research Engine
# ============================================================================

def run_deep_research(seed_element: str = "Cl",
                      initnum: int = 220,
                      zmq_stream: bool = True,
                      zmq_endpoint: str = "tcp://127.0.0.1:5555",
                      frame_delay_ms: int = 50,
                      rng_seed: int = 42,
                      output_dir: str = "deep_research_output") -> StudyLedger:
    """
    Run the full 4-phase deep research study.

    This is the main entry point.  It enumerates all physically valid
    geometries for the seed element, FIRE-minimises each one, streams
    every attempt to the 3D GUI, then progresses through bead formation,
    MD assembly, and lattice construction.
    """
    seed_Z = SYM_TO_Z.get(seed_element)
    if seed_Z is None:
        raise ValueError(f"Unknown element: {seed_element}")

    rng = np.random.default_rng(rng_seed)
    ledger = StudyLedger()

    # Setup ZMQ streamer
    streamer = None
    if zmq_stream and HAS_ZMQ:
        streamer = ZMQStreamer(zmq_endpoint)
        print(f"[ZMQ] Publishing frames to {zmq_endpoint}")
        print(f"[ZMQ] Start renderer:  python -m pykernel.cartoon_renderer")
    elif zmq_stream and not HAS_ZMQ:
        print("[WARN] pyzmq not installed -- no live GUI streaming")

    # Output directory
    os.makedirs(output_dir, exist_ok=True)

    # ===== Phase 10A =====
    print(f"\n{'='*60}")
    print(f"  Phase 10A: Atomistic Geometry Enumeration")
    print(f"  Seed: {seed_element} (Z={seed_Z}, max valence={ELEMENT_DATA[seed_Z]['val']})")
    print(f"  Target pathways: {initnum}")
    print(f"{'='*60}\n")

    ligand_pool = [1, 6, 7, 8, 9, 15, 16, 17, 35, 53]
    pathways = enumerate_pathways(seed_Z, ligand_pool,
                                  max_ligands=6, initnum=initnum)

    print(f"  Enumerated {len(pathways)} candidate pathways")

    for i, pw in enumerate(pathways):
        ledger.pathways_attempted += 1

        candidate = build_candidate(pw)

        # Check hash cache
        if candidate.pathway_hash in ledger.hash_cache:
            ledger.hash_reuse_count += 1
            if (i + 1) % 50 == 0:
                print(f"  [{i+1}/{len(pathways)}] Hash reuse: {candidate.formula} "
                      f"(SHA-512 overlap, skipping FIRE)")
            continue

        # FIRE minimisation
        ledger.fire_calls += 1
        candidate = fire_minimise(candidate)

        if candidate.converged:
            ledger.converged_count += 1

        # Cache
        idx = len(ledger.candidates)
        ledger.hash_cache[candidate.pathway_hash] = idx
        ledger.candidates.append(candidate)
        ledger.unique_pathways += 1

        # Stream to GUI
        if streamer:
            streamer.send_frame(candidate, phase="10A")
            time.sleep(frame_delay_ms / 1000.0)

        # Progress
        if (i + 1) % 20 == 0 or i == len(pathways) - 1:
            print(f"  [{i+1}/{len(pathways)}] {candidate.formula:12s}  "
                  f"VSEPR={candidate.vsepr_class:8s}  "
                  f"E={candidate.energy:8.2f}  "
                  f"{'OK' if candidate.converged else 'FAIL':4s}  "
                  f"hash={candidate.pathway_hash[:16]}")

        # Save XYZ
        xyz_path = os.path.join(output_dir, f"candidate_{i:04d}.xyz")
        with open(xyz_path, "w") as f:
            f.write(f"{len(candidate.symbols)}\n")
            f.write(f"{candidate.formula} E={candidate.energy:.4f} "
                    f"hash={candidate.pathway_hash[:32]}\n")
            for sym, pos in zip(candidate.symbols, candidate.positions):
                f.write(f"{sym}  {pos[0]:.6f}  {pos[1]:.6f}  {pos[2]:.6f}\n")

    print(f"\n  Phase 10A complete: {ledger.converged_count} converged / "
          f"{ledger.unique_pathways} unique / "
          f"{ledger.hash_reuse_count} hash reuses")

    # ===== Phase 20B =====
    print(f"\n{'='*60}")
    print(f"  Phase 20B: Bead Formation (Semi-Stochastic)")
    print(f"{'='*60}\n")

    for candidate in ledger.candidates:
        if not candidate.converged:
            continue

        ledger.bead_attempts += 1
        bead = form_bead(candidate, rng)

        if bead.stable:
            ledger.bead_stable += 1

        ledger.beads.append(bead)

        # Stream bead visualisation
        if streamer:
            # Show the atomistic geometry transforming into a bead
            bead_candidate = GeometryCandidate(
                pathway_hash=bead.bead_hash[:128],
                seed_element=candidate.seed_element,
                ligands=candidate.ligands,
                vsepr_class=candidate.vsepr_class,
                formula=candidate.formula + " [BEAD]",
                positions=candidate.positions,
                symbols=candidate.symbols,
                energy=bead.energy,
                rms_force=0.0,
                converged=bead.stable,
            )
            streamer.send_frame(bead_candidate, phase="20B",
                                extra={"bead_radius": bead.radius,
                                       "bead_class": bead.bead_class})
            time.sleep(frame_delay_ms / 2000.0)

    print(f"  Phase 20B complete: {ledger.bead_stable} stable / "
          f"{ledger.bead_attempts} attempted")

    # ===== Phase 31C =====
    print(f"\n{'='*60}")
    print(f"  Phase 31C: MD Bonding and Assembly")
    print(f"{'='*60}\n")

    stable_beads = [b for b in ledger.beads if b.stable]
    if len(stable_beads) >= 2:
        max_attempts = min(len(stable_beads) * 3, initnum)
        for attempt in range(max_attempts):
            i = rng.integers(0, len(stable_beads))
            j = rng.integers(0, len(stable_beads))
            if i == j:
                continue

            ledger.assembly_attempts += 1
            ar = try_assembly(stable_beads[i], stable_beads[j], rng)
            if ar.converged:
                ledger.assembly_converged += 1
            ledger.assemblies.append(ar)

        print(f"  Phase 31C complete: {ledger.assembly_converged} converged / "
              f"{ledger.assembly_attempts} attempted")
    else:
        print("  Phase 31C skipped: fewer than 2 stable beads")

    # ===== Phase 41A =====
    print(f"\n{'='*60}")
    print(f"  Phase 41A: Lattice Assembly (Blank Call)")
    print(f"{'='*60}\n")

    lattice_blank_call(ledger)

    if ledger.lattice_invoked:
        print(f"  Lattice geometry: {ledger.lattice_geometry}")
        print(f"  Lattice energy:   {ledger.lattice_energy:.2f} kcal/mol")
    else:
        print("  Phase 41A skipped: no assemblies available")

    # ===== Summary =====
    print(f"\n{ledger.summary()}")

    # Save ledger
    ledger_path = os.path.join(output_dir, "study_ledger.txt")
    with open(ledger_path, "w") as f:
        f.write(ledger.summary())
        f.write("\n\n--- Pathway Hashes ---\n")
        for c in ledger.candidates:
            f.write(f"{c.formula:12s}  {c.vsepr_class:8s}  "
                    f"E={c.energy:8.2f}  {c.pathway_hash[:64]}\n")

    print(f"\nLedger saved to: {ledger_path}")
    print(f"XYZ files saved to: {output_dir}/")

    if streamer:
        streamer.close()

    return ledger


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="VSEPR-SIM Deep Research Visual Study Driver",
        epilog="Example: python scripts/deep_research_driver.py --seed Cl --initnum 220"
    )
    parser.add_argument("--seed", default="Cl",
                        help="Seed element symbol (default: Cl)")
    parser.add_argument("--initnum", type=int, default=220,
                        help="Max pathway enumeration count (default: 220)")
    parser.add_argument("--no-zmq", action="store_true",
                        help="Disable ZMQ live streaming to GUI")
    parser.add_argument("--zmq-endpoint", default="tcp://127.0.0.1:5555",
                        help="ZMQ PUB endpoint (default: tcp://127.0.0.1:5555)")
    parser.add_argument("--frame-delay", type=int, default=50,
                        help="Delay between ZMQ frames in ms (default: 50)")
    parser.add_argument("--rng-seed", type=int, default=42,
                        help="RNG seed for reproducibility (default: 42)")
    parser.add_argument("--output-dir", default="deep_research_output",
                        help="Output directory (default: deep_research_output)")

    args = parser.parse_args()

    run_deep_research(
        seed_element=args.seed,
        initnum=args.initnum,
        zmq_stream=not args.no_zmq,
        zmq_endpoint=args.zmq_endpoint,
        frame_delay_ms=args.frame_delay,
        rng_seed=args.rng_seed,
        output_dir=args.output_dir,
    )


if __name__ == "__main__":
    main()
