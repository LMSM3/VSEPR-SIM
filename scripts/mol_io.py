"""
mol_io.py — Multi-format XYZ-family loader for mol_viewer.py

Supports:
  .xyz       — standard XYZ (single frame)
  .xyza      — XYZ with energy annotation in comment (single frame)
  .xyzf      — multi-frame XYZ trajectory
  .xyzFull   — alias for .xyzf (rich per-step trajectory)
  .xyzc      — XYZ checkpoint (multi-frame, checkpoint metadata)
  .dynx      — VSIM dynamic session archive (multi-frame text, FRAME blocks)
  .X         — VSIM suite bundle (XBUNDLE manifest; entry= lines point to members)

Return type for load_file():
    MolData(
        title:  str                              display name
        frames: list[list[tuple[str,f,f,f]]]    list of atom lists per frame
        meta:   list[dict]                       per-frame metadata (time, energy, ...)
        fmt:    str                              detected format label
    )

Atom tuple: (element_symbol, x, y, z)  — floats in Angstrom
"""

from __future__ import annotations

import math
import os
import re
import zipfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Dict, Tuple, Optional

Atom  = Tuple[str, float, float, float]
Frame = List[Atom]


@dataclass
class MolData:
    title:  str
    frames: List[Frame]
    meta:   List[Dict]
    fmt:    str = "xyz"

    @property
    def atoms(self) -> Frame:
        """Convenience: atoms from the first (or only) frame."""
        return self.frames[0] if self.frames else []


# ---------------------------------------------------------------------------
# Bond detection (shared utility, uses pykernel radii)
# ---------------------------------------------------------------------------

_BOND_CUTOFF = 1.3   # scale factor on sum-of-covalent-radii


def detect_bonds(
    atoms: Frame,
    cov_radius_fn,
    cutoff_factor: float = _BOND_CUTOFF,
) -> List[Tuple[int, int]]:
    bonds: List[Tuple[int, int]] = []
    n = len(atoms)
    for i in range(n):
        for j in range(i + 1, n):
            ei, xi, yi, zi = atoms[i]
            ej, xj, yj, zj = atoms[j]
            d = math.sqrt((xi - xj) ** 2 + (yi - yj) ** 2 + (zi - zj) ** 2)
            if d < (cov_radius_fn(ei) + cov_radius_fn(ej)) * cutoff_factor:
                bonds.append((i, j))
    return bonds


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

def _ext(path: str) -> str:
    """Lowercase extension, preserving .xyzFull casing check separately."""
    return Path(path).suffix.lower()


def _parse_xyz_block(lines: List[str], offset: int) -> Tuple[Frame, Dict, int]:
    """
    Parse one XYZ block starting at lines[offset].
    Returns (atoms, meta_dict, next_offset).
    meta_dict may contain 'energy', 'temperature', 'time', 'dt', 'comment'.
    """
    try:
        n = int(lines[offset].strip())
    except (ValueError, IndexError):
        return [], {}, offset + 1

    comment = lines[offset + 1].strip() if offset + 1 < len(lines) else ""
    meta: Dict = {"comment": comment}

    # Extract tagged values from comment: key=value or key = value
    for tok in re.findall(r'(\w+)\s*=\s*([^\s]+)', comment):
        key, val = tok
        try:
            meta[key] = float(val)
        except ValueError:
            meta[key] = val

    atoms: Frame = []
    for k in range(n):
        li = offset + 2 + k
        if li >= len(lines):
            break
        parts = lines[li].split()
        if len(parts) < 4:
            continue
        sym = parts[0]
        try:
            x, y, z = float(parts[1]), float(parts[2]), float(parts[3])
        except ValueError:
            continue
        atoms.append((sym, x, y, z))

    return atoms, meta, offset + 2 + n


def _read_all_xyz_frames(text: str) -> Tuple[List[Frame], List[Dict]]:
    """Parse zero or more XYZ blocks from text."""
    lines = text.splitlines()
    frames: List[Frame] = []
    metas:  List[Dict]  = []
    i = 0
    while i < len(lines):
        # Skip blank lines between frames
        if not lines[i].strip():
            i += 1
            continue
        try:
            int(lines[i].strip())
        except ValueError:
            i += 1
            continue
        atoms, meta, i = _parse_xyz_block(lines, i)
        if atoms:
            frames.append(atoms)
            metas.append(meta)
    return frames, metas


# ---------------------------------------------------------------------------
# Format-specific loaders
# ---------------------------------------------------------------------------

def _load_xyz(path: str) -> MolData:
    text = Path(path).read_text(encoding="utf-8", errors="replace")
    frames, metas = _read_all_xyz_frames(text)
    if not frames:
        raise ValueError(f"No valid XYZ data in {path!r}")
    return MolData(title=Path(path).stem, frames=frames, meta=metas, fmt="xyz")


def _load_xyza(path: str) -> MolData:
    # .xyza is XYZ with energy fields in comment (E=... Ubond=... etc.)
    # The parser handles that via the comment tag extraction already.
    d = _load_xyz(path)
    d.fmt = "xyza"
    return d


def _load_xyzf(path: str) -> MolData:
    d = _load_xyz(path)
    d.fmt = "xyzFull" if Path(path).suffix.lower() == ".xyzfull" else "xyzf"
    return d


def _load_xyzc(path: str) -> MolData:
    d = _load_xyz(path)
    d.fmt = "xyzc"
    return d


def _load_dynx(path: str) -> MolData:
    """
    Parse .dynx v1 session archive.
    Header lines start with '#'.  Particle data lives between FRAME and END_FRAME.
    """
    text = Path(path).read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()

    frames: List[Frame] = []
    metas:  List[Dict]  = []
    hdr: Dict = {}
    cur_atoms: Frame = []
    cur_meta: Dict = {}
    in_frame = False

    for line in lines:
        stripped = line.strip()
        if not stripped:
            continue

        if stripped.startswith("#dynx"):
            hdr["version"] = stripped
            continue
        if stripped.startswith("#"):
            # Header key-value:  #key value  or  #key=value
            m = re.match(r'^#(\w+)\s+(.*)', stripped)
            if m:
                hdr[m.group(1)] = m.group(2).strip()
            continue

        if stripped.startswith("FRAME "):
            parts = stripped.split()
            in_frame  = True
            cur_atoms = []
            cur_meta  = {**hdr}
            if len(parts) >= 2:
                try:
                    cur_meta["frame_index"] = int(parts[1])
                except ValueError:
                    pass
            if len(parts) >= 3:
                try:
                    cur_meta["time"] = float(parts[2])
                except ValueError:
                    pass
            continue

        if stripped == "END_FRAME":
            if in_frame and cur_atoms:
                frames.append(cur_atoms)
                metas.append(cur_meta)
            in_frame  = False
            cur_atoms = []
            cur_meta  = {}
            continue

        if stripped == "#END_DYNX":
            break

        if in_frame:
            # Skip non-particle annotation lines
            if stripped.startswith(("FORCE ", "BOND_FORCE ", "FIELD ",
                                     "EVENT ", "RENDER ", "CAMERA ")):
                continue
            parts = stripped.split()
            if len(parts) >= 4:
                try:
                    sym = parts[0]
                    x, y, z = float(parts[1]), float(parts[2]), float(parts[3])
                    cur_atoms.append((sym, x, y, z))
                except ValueError:
                    pass

    # Fallback: if no FRAME blocks found, treat the whole file as xyzf-style frames
    if not frames:
        frames, metas = _read_all_xyz_frames(text)

    if not frames:
        raise ValueError(f"No valid frame data in .dynx file: {path!r}")

    title = hdr.get("source", Path(path).stem)
    title = Path(title).stem  # strip path from source header
    return MolData(title=title, frames=frames, meta=metas, fmt="dynx")


def _load_x_bundle(path: str) -> MolData:
    """
    Parse .X XBUNDLE suite container.
    Extracts member content (verbatim between >>> and <<<).
    Loads the entry_point member (or first vsim/xyz member).
    """
    text = Path(path).read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()

    members: Dict[str, str] = {}
    manifest: Dict[str, str] = {}
    cur_name: Optional[str] = None
    cur_kind: Optional[str] = None
    cur_lines: List[str] = []
    in_body = False

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("XBUNDLE"):
            continue
        if stripped == "[manifest]":
            cur_name = None
            in_body = False
            continue
        if stripped == "[[member]]":
            # Save previous member if any
            if cur_name and cur_lines:
                members[cur_name] = "\n".join(cur_lines)
            cur_name = None
            cur_kind = None
            cur_lines = []
            in_body = False
            continue
        if stripped == ">>>":
            in_body = True
            continue
        if stripped == "<<<":
            in_body = False
            if cur_name:
                members[cur_name] = "\n".join(cur_lines)
            cur_lines = []
            continue
        if in_body:
            cur_lines.append(line)
            continue
        # key = value lines
        m = re.match(r'^\s*(\w+)\s*=\s*(.+)', stripped)
        if m:
            key, val = m.group(1).strip(), m.group(2).strip()
            if cur_name is None and key in ("name", "entry_point"):
                manifest[key] = val
            elif key == "name":
                cur_name = val
            elif key == "kind":
                cur_kind = val

    # Save last member
    if cur_name and cur_lines:
        members[cur_name] = "\n".join(cur_lines)

    # Resolve entry to load
    entry_name = manifest.get("entry_point")
    content: Optional[str] = None
    chosen_name = path

    if entry_name and entry_name in members:
        content = members[entry_name]
        chosen_name = entry_name
    else:
        # Pick first member with xyz-like content
        for name, body in members.items():
            ext = Path(name).suffix.lower()
            if ext in (".xyz", ".xyza", ".xyzf", ".xyzfull", ".xyzc", ".dynx"):
                content = body
                chosen_name = name
                break
        if content is None and members:
            content = next(iter(members.values()))
            chosen_name = next(iter(members.keys()))

    if content is None:
        raise ValueError(f"No loadable member found in .X bundle: {path!r}")

    # Parse the extracted content as xyz frames
    frames, metas = _read_all_xyz_frames(content)
    if not frames:
        raise ValueError(
            f"Member {chosen_name!r} in .X bundle contains no XYZ data."
        )

    title = manifest.get("name", Path(path).stem)
    return MolData(title=title, frames=frames, meta=metas, fmt="X")


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def load_file(path: str) -> MolData:
    """
    Auto-detect format and load a molecule file.
    Returns a MolData instance.  Raises ValueError on parse failure.
    """
    p = Path(path)
    ext = p.suffix  # preserve case for .xyzFull / .X

    ext_lower = ext.lower()

    if ext_lower == ".xyz":
        return _load_xyz(path)
    elif ext_lower == ".xyza":
        return _load_xyza(path)
    elif ext_lower in (".xyzf", ".xyzfull"):
        return _load_xyzf(path)
    elif ext_lower == ".xyzc":
        return _load_xyzc(path)
    elif ext_lower == ".dynx":
        return _load_dynx(path)
    elif ext == ".X":         # case-sensitive: .X only
        return _load_x_bundle(path)
    else:
        # Unknown extension — try generic multi-frame XYZ parse
        d = _load_xyz(path)
        d.fmt = "xyz?"
        return d


# ---------------------------------------------------------------------------
# Export helpers
# ---------------------------------------------------------------------------

def write_xyz(path: str, atoms: Frame, comment: str = "") -> None:
    """Write a single-frame .xyz file."""
    with open(path, "w", encoding="utf-8") as f:
        f.write(f"{len(atoms)}\n")
        f.write(f"{comment}\n")
        for sym, x, y, z in atoms:
            f.write(f"{sym}  {x:.8f}  {y:.8f}  {z:.8f}\n")


def write_dynx(
    path: str,
    atoms: Frame,
    source_path: str = "",
    comment: str = "",
) -> None:
    """
    Write a minimal .dynx v1 session archive with a single static frame.
    Conforms to the VSIM_REFERENCE.md §Phase 9 spec.
    """
    import datetime
    ts = datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ")
    src_hash = "none"
    if source_path and Path(source_path).is_file():
        import hashlib
        data = Path(source_path).read_bytes()
        src_hash = hashlib.sha256(data).hexdigest()

    with open(path, "w", encoding="utf-8") as f:
        f.write("#dynx v1\n")
        f.write(f"#source {source_path or path}\n")
        f.write(f"#source_hash {src_hash}\n")
        f.write("#kernel_version mol_viewer-python\n")
        f.write("#frame_count 1\n")
        f.write("#frame_interval 0.0\n")
        f.write(f"#particle_count {len(atoms)}\n")
        f.write(f"#timestamp {ts}\n")
        f.write(f"FRAME 0 0.0\n")
        for sym, x, y, z in atoms:
            f.write(f"{sym}  {x:.8f}  {y:.8f}  {z:.8f}\n")
        f.write("END_FRAME\n")
        f.write("#END_DYNX\n")


def write_x_bundle(
    path: str,
    atoms: Frame,
    mol_name: str,
    xyz_content: str,
    dynx_content: str,
) -> None:
    """
    Write a .X XBUNDLE suite container with two members:
      - <mol_name>.xyz  (kind=asset, the current geometry)
      - <mol_name>.dynx (kind=asset, the session archive)
    """
    xyz_name  = f"{mol_name}.xyz"
    dynx_name = f"{mol_name}.dynx"

    xyz_size  = len(xyz_content.encode("utf-8"))
    dynx_size = len(dynx_content.encode("utf-8"))

    with open(path, "w", encoding="utf-8") as f:
        f.write("XBUNDLE v1\n")
        f.write("[manifest]\n")
        f.write(f"  name = {mol_name}\n")
        f.write(f"  entry_point = {xyz_name}\n")
        f.write("[[member]]\n")
        f.write(f"  name = {xyz_name}\n")
        f.write("  kind = asset\n")
        f.write(f"  size = {xyz_size}\n")
        f.write("  >>>\n")
        f.write(xyz_content)
        if not xyz_content.endswith("\n"):
            f.write("\n")
        f.write("  <<<\n")
        f.write("[[member]]\n")
        f.write(f"  name = {dynx_name}\n")
        f.write("  kind = asset\n")
        f.write(f"  size = {dynx_size}\n")
        f.write("  >>>\n")
        f.write(dynx_content)
        if not dynx_content.endswith("\n"):
            f.write("\n")
        f.write("  <<<\n")
