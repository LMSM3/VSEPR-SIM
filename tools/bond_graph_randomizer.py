#!/usr/bin/env python3
"""
bond_graph_randomizer.py  --  VSEPR-SIM Bond Graph Randomizer
==============================================================
Streams random molecule atom+bond graphs to viz_web.py via UDP so the
/api/bond-graph endpoint and viz_bond_graph.html viewer see a live,
continuously-updating feed.

Two modes
---------
  --mode xyz        Cycle through XYZ files in examples/my_molecules/ (default)
  --mode generate   Build random chemically-plausible molecules from scratch

Usage
-----
  python tools/bond_graph_randomizer.py
  python tools/bond_graph_randomizer.py --mode generate --interval 2
  python tools/bond_graph_randomizer.py --mode xyz --shuffle --interval 4
  python tools/bond_graph_randomizer.py --count 20 --verbose

Options
-------
  --mode {xyz|generate}   Molecule source             (default: xyz)
  --interval SECONDS      Delay between frames        (default: 3.0)
  --port PORT             UDP destination port        (default: 9999)
  --host HOST             UDP destination host        (default: localhost)
  --shuffle               Randomise XYZ file order
  --count N               Stop after N molecules      (default: infinite)
  --verbose               Print per-atom detail

VSEPR-SIM v5.1.4  |  tools/bond_graph_randomizer.py
"""

import sys
import os
import json
import math
import random
import socket
import time
import argparse
from pathlib import Path

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

TOOLS_DIR     = Path(__file__).resolve().parent
WORKSPACE     = TOOLS_DIR.parent
XYZ_DIRS      = [
    WORKSPACE / "examples" / "my_molecules",
    WORKSPACE / "examples" / "molecules",
]

# ---------------------------------------------------------------------------
# Covalent radii (Angstrom) -- bond detected when dist < (r_i + r_j) * 1.3
# ---------------------------------------------------------------------------

COVALENT_R = {
    "H":0.31,"He":0.28,"Li":1.28,"Be":0.96,"B":0.84,"C":0.77,"N":0.75,
    "O":0.73,"F":0.71,"Ne":0.69,"Na":1.66,"Mg":1.41,"Al":1.21,"Si":1.11,
    "P":1.07,"S":1.05,"Cl":1.02,"Ar":1.06,"K":2.03,"Ca":1.76,"Fe":1.32,
    "Co":1.26,"Ni":1.24,"Cu":1.32,"Zn":1.22,"Ga":1.22,"Ge":1.20,"As":1.19,
    "Se":1.20,"Br":1.20,"Kr":1.16,"Ag":1.45,"I":1.39,"Xe":1.40,"Au":1.36,
    "Pb":1.46,"Bi":1.48,
}

# Max bonds per element (used in generate mode)
VALENCE = {
    "H":1,"F":1,"Cl":1,"Br":1,"I":1,"Na":1,"K":1,"Ag":1,"Au":1,
    "O":2,"S":2,"Se":2,"Ca":2,"Mg":2,"Zn":2,"Pb":2,
    "N":3,"P":3,"As":3,"B":3,"Al":3,"Bi":3,
    "C":4,"Si":4,"Ge":4,"Sn":4,
    "Fe":6,"Co":6,"Ni":6,"Cu":4,"Xe":6,"Kr":4,
}

# Elements available in generate mode (weighted toward common ones)
GEN_POOL = (
    ["C"]*10 + ["H"]*8 + ["N"]*4 + ["O"]*4 +
    ["S"]*2  + ["P"]*2 + ["F"]*2 + ["Cl"]*2 +
    ["Br"]*1 + ["Si"]*1 + ["B"]*1
)

# ---------------------------------------------------------------------------
# Bond detection
# ---------------------------------------------------------------------------

def detect_bonds(atoms):
    """Return [[i,j],...] using covalent radii cutoff x1.3."""
    bonds = []
    n = len(atoms)
    for i in range(n):
        for j in range(i + 1, n):
            ri = COVALENT_R.get(atoms[i]["sym"], 0.77)
            rj = COVALENT_R.get(atoms[j]["sym"], 0.77)
            cut = (ri + rj) * 1.3
            dx = atoms[i]["x"] - atoms[j]["x"]
            dy = atoms[i]["y"] - atoms[j]["y"]
            dz = atoms[i]["z"] - atoms[j]["z"]
            if dx*dx + dy*dy + dz*dz < cut*cut:
                bonds.append([i, j])
    return bonds

# ---------------------------------------------------------------------------
# XYZ parser
# ---------------------------------------------------------------------------

def parse_xyz(path):
    """Return (comment, atoms) from an XYZ file.  atoms = [{sym,x,y,z}]."""
    try:
        lines = Path(path).read_text(encoding="utf-8", errors="replace").splitlines()
        n = int(lines[0].strip())
        comment = lines[1].strip() if len(lines) > 1 else ""
        atoms = []
        for line in lines[2 : 2 + n]:
            parts = line.split()
            if len(parts) >= 4:
                atoms.append({
                    "sym": parts[0][:2].strip(),
                    "x":   float(parts[1]),
                    "y":   float(parts[2]),
                    "z":   float(parts[3]),
                })
        if len(atoms) != n:
            return None, None
        return comment, atoms
    except Exception:
        return None, None

# ---------------------------------------------------------------------------
# Hill-order formula
# ---------------------------------------------------------------------------

def formula_from_atoms(atoms):
    counts = {}
    for a in atoms:
        counts[a["sym"]] = counts.get(a["sym"], 0) + 1
    order = ["C", "H"] + sorted(k for k in counts if k not in ("C", "H"))
    return "".join(s + (str(counts[s]) if counts[s] > 1 else "")
                   for s in order if s in counts)

# ---------------------------------------------------------------------------
# Random molecule generator
# ---------------------------------------------------------------------------

def generate_molecule(min_atoms=3, max_atoms=12):
    """Build a random connected graph obeying valence constraints."""
    n = random.randint(min_atoms, max_atoms)
    syms = []
    # Pick a central heavy atom
    heavy = [e for e in GEN_POOL if e not in ("H", "F", "Cl", "Br", "I")]
    syms.append(random.choice(heavy))
    for _ in range(n - 1):
        syms.append(random.choice(GEN_POOL))

    # Place atoms on a sphere
    atoms = []
    atoms.append({"sym": syms[0], "x": 0.0, "y": 0.0, "z": 0.0})
    bond_len = 1.5
    for i in range(1, n):
        angle_h = random.uniform(0, 2 * math.pi)
        angle_v = random.uniform(-math.pi / 2, math.pi / 2)
        r = bond_len * random.uniform(0.9, 2.5)
        atoms.append({
            "sym": syms[i],
            "x": r * math.cos(angle_v) * math.cos(angle_h),
            "y": r * math.cos(angle_v) * math.sin(angle_h),
            "z": r * math.sin(angle_v),
        })

    bonds = detect_bonds(atoms)
    # Ensure graph is connected: link isolated nodes to nearest neighbour
    connected = {i for pair in bonds for i in pair}
    for i in range(n):
        if i not in connected:
            best_j, best_d = -1, float("inf")
            for j in range(n):
                if j == i:
                    continue
                dx = atoms[i]["x"] - atoms[j]["x"]
                dy = atoms[i]["y"] - atoms[j]["y"]
                dz = atoms[i]["z"] - atoms[j]["z"]
                d = dx*dx + dy*dy + dz*dz
                if d < best_d:
                    best_d, best_j = d, j
            if best_j >= 0:
                bonds.append([i, best_j])

    return atoms, bonds

# ---------------------------------------------------------------------------
# UDP sender
# ---------------------------------------------------------------------------

def send_frame(sock, host, port, atoms, bonds, label=""):
    frame = {
        "type":  "atomic",
        "atoms": atoms,
        "bonds": bonds,
        "_label": label,
    }
    data = json.dumps(frame, separators=(",", ":")).encode("utf-8")
    try:
        sock.sendto(data, (host, port))
        return True
    except Exception as exc:
        print(f"  [UDP] send error: {exc}", file=sys.stderr)
        return False

# ---------------------------------------------------------------------------
# Collect XYZ paths
# ---------------------------------------------------------------------------

def collect_xyz_paths():
    paths = []
    for d in XYZ_DIRS:
        if d.is_dir():
            paths.extend(sorted(d.glob("*.xyz")))
    return paths

# ---------------------------------------------------------------------------
# ANSI helpers
# ---------------------------------------------------------------------------

_ANSI = sys.platform != "win32" or os.environ.get("TERM")
def _c(code, s): return f"\033[{code}m{s}\033[0m" if _ANSI else s
YEL = lambda s: _c("93", s)
GRN = lambda s: _c("92", s)
CYN = lambda s: _c("96", s)
DIM = lambda s: _c("2",  s)

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description="VSEPR-SIM Bond Graph Randomizer — streams molecules to viz_web.py",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--mode",     choices=["xyz", "generate"], default="xyz")
    ap.add_argument("--interval", type=float, default=3.0,
                    help="seconds between frames (default: 3.0)")
    ap.add_argument("--port",     type=int,   default=9999,
                    help="UDP destination port (default: 9999)")
    ap.add_argument("--host",     default="localhost",
                    help="UDP destination host (default: localhost)")
    ap.add_argument("--shuffle",  action="store_true",
                    help="randomise XYZ file order")
    ap.add_argument("--count",    type=int,   default=0,
                    help="stop after N molecules (default: 0 = infinite)")
    ap.add_argument("--verbose",  action="store_true")
    args = ap.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    print(f"\n  {YEL('VSEPR-SIM')} Bond Graph Randomizer")
    print(f"  Mode     : {CYN(args.mode)}")
    print(f"  Target   : {CYN(args.host)}:{CYN(str(args.port))}")
    print(f"  Interval : {args.interval}s")
    if args.count:
        print(f"  Count    : {args.count} molecules")
    print(f"  Viewer   : http://localhost:8899/graph\n")

    if args.mode == "xyz":
        paths = collect_xyz_paths()
        if not paths:
            print("  [!] No XYZ files found in examples/my_molecules/ or examples/molecules/",
                  file=sys.stderr)
            sys.exit(1)
        print(f"  {GRN(str(len(paths)))} XYZ files found")
        if args.shuffle:
            random.shuffle(paths)

    sent = 0
    idx  = 0

    try:
        while True:
            if args.mode == "xyz":
                path = paths[idx % len(paths)]
                idx += 1
                comment, atoms = parse_xyz(path)
                if not atoms:
                    continue
                bonds = detect_bonds(atoms)
                label = path.stem
            else:
                atoms, bonds = generate_molecule()
                label = formula_from_atoms(atoms)

            ok = send_frame(sock, args.host, args.port, atoms, bonds, label)
            if ok:
                sent += 1
                f = formula_from_atoms(atoms)
                print(f"  [{sent:>4}]  {YEL(f'{f:<16}')}  "
                      f"{DIM(str(len(atoms)))} atoms  "
                      f"{DIM(str(len(bonds)))} bonds"
                      + (f"  {DIM(label)}" if label != f else ""))
                if args.verbose:
                    for a in atoms:
                        print(f"          {a['sym']:>2}  "
                              f"({a['x']:7.3f}, {a['y']:7.3f}, {a['z']:7.3f})")

            if args.count and sent >= args.count:
                print(f"\n  Done — {sent} molecules sent.")
                break

            time.sleep(args.interval)

    except KeyboardInterrupt:
        print(f"\n  Stopped — {sent} molecules sent.")
    finally:
        sock.close()


if __name__ == "__main__":
    main()