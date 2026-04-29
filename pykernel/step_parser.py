"""
step_parser — STEP (ISO 10303-21) file parser for SolidWorks model import.

Reads AP203/AP214 STEP files and extracts:
  - Named product definitions (part names, assembly structure)
  - Shape representations (vertices, edges, faces)
  - Coordinate transforms (axis2_placement_3d)
  - Material property hooks (for downstream c_p assignment)

The parser handles the STEP physical file format:
  HEADER; ... ENDSEC;
  DATA;   #id = ENTITY_NAME(...); ... ENDSEC;

Only geometry-relevant entities are extracted; manufacturing/tolerance
data is skipped.  This is a research parser, not a CAD kernel.

VSEPR-SIM 3.0.0
"""

from __future__ import annotations

import re
import os
import math
from dataclasses import dataclass, field
from typing import Optional, IO


# ═══════════════════════════════════════════════════════════════════════
# Data structures
# ═══════════════════════════════════════════════════════════════════════

@dataclass(frozen=True)
class Vec3:
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0

    def norm(self) -> float:
        return math.sqrt(self.x ** 2 + self.y ** 2 + self.z ** 2)

    def __add__(self, o: "Vec3") -> "Vec3":
        return Vec3(self.x + o.x, self.y + o.y, self.z + o.z)

    def __sub__(self, o: "Vec3") -> "Vec3":
        return Vec3(self.x - o.x, self.y - o.y, self.z - o.z)

    def __mul__(self, s: float) -> "Vec3":
        return Vec3(self.x * s, self.y * s, self.z * s)


@dataclass
class StepEntity:
    """Raw STEP entity: #id = TYPE(args);"""
    id: int
    entity_type: str
    args_raw: str
    args: list[str] = field(default_factory=list)


@dataclass
class CartesianPoint:
    id: int
    name: str
    coords: Vec3


@dataclass
class Direction:
    id: int
    name: str
    direction: Vec3


@dataclass
class Axis2Placement3D:
    id: int
    name: str
    location: Vec3
    axis: Vec3
    ref_direction: Vec3


@dataclass
class NamedPart:
    """A product/part definition extracted from a STEP file."""
    id: int
    name: str
    description: str = ""
    # Geometry (populated during shape extraction)
    vertices: list[Vec3] = field(default_factory=list)
    placements: list[Axis2Placement3D] = field(default_factory=list)
    # Material assignment (populated downstream)
    material: str = ""
    atomic_number: int = 0


@dataclass
class StepAssembly:
    """Complete parsed STEP file."""
    filename: str = ""
    schema: str = ""
    description: str = ""
    parts: list[NamedPart] = field(default_factory=list)
    # Raw entity store for debugging / provenance
    entities: dict[int, StepEntity] = field(default_factory=dict)
    points: dict[int, CartesianPoint] = field(default_factory=dict)
    directions: dict[int, Direction] = field(default_factory=dict)
    placements: dict[int, Axis2Placement3D] = field(default_factory=dict)

    @property
    def part_names(self) -> list[str]:
        return [p.name for p in self.parts]

    @property
    def num_parts(self) -> int:
        return len(self.parts)


# ═══════════════════════════════════════════════════════════════════════
# Tokeniser / entity parser
# ═══════════════════════════════════════════════════════════════════════

_ENTITY_RE = re.compile(
    r"#(\d+)\s*=\s*([A-Z_][A-Z0-9_]*)\s*\(([^;]*)\)\s*;",
    re.IGNORECASE,
)

_REF_RE = re.compile(r"#(\d+)")
_STR_RE = re.compile(r"'([^']*)'")
_NUM_RE = re.compile(r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?")


def _split_args(raw: str) -> list[str]:
    """Split STEP entity arguments respecting parentheses depth."""
    parts: list[str] = []
    depth = 0
    current: list[str] = []
    for ch in raw:
        if ch == "(":
            depth += 1
            current.append(ch)
        elif ch == ")":
            depth -= 1
            current.append(ch)
        elif ch == "," and depth == 0:
            parts.append("".join(current).strip())
            current = []
        else:
            current.append(ch)
    tail = "".join(current).strip()
    if tail:
        parts.append(tail)
    return parts


def _parse_float_list(s: str) -> list[float]:
    """Extract list of floats from '(1.0,2.0,3.0)' or '1.0,2.0,3.0'."""
    return [float(m) for m in _NUM_RE.findall(s)]


def _extract_string(s: str) -> str:
    m = _STR_RE.search(s)
    return m.group(1) if m else s.strip("' ")


def _extract_ref(s: str) -> Optional[int]:
    m = _REF_RE.search(s)
    return int(m.group(1)) if m else None


# ═══════════════════════════════════════════════════════════════════════
# Core parser
# ═══════════════════════════════════════════════════════════════════════

def _read_data_section(text: str) -> dict[int, StepEntity]:
    """Parse DATA section into entity dict."""
    # Collapse multi-line entities
    collapsed = text.replace("\n", " ").replace("\r", "")
    entities: dict[int, StepEntity] = {}
    for m in _ENTITY_RE.finditer(collapsed):
        eid = int(m.group(1))
        etype = m.group(2).upper()
        raw = m.group(3)
        entities[eid] = StepEntity(
            id=eid, entity_type=etype, args_raw=raw, args=_split_args(raw),
        )
    return entities


def _extract_header(text: str) -> tuple[str, str]:
    """Extract schema and description from HEADER section."""
    schema = ""
    desc = ""
    hdr_match = re.search(r"HEADER;(.*?)ENDSEC;", text, re.DOTALL | re.IGNORECASE)
    if hdr_match:
        hdr = hdr_match.group(1)
        schema_m = re.search(r"FILE_SCHEMA\s*\(\s*\(\s*'([^']+)'", hdr, re.IGNORECASE)
        if schema_m:
            schema = schema_m.group(1)
        desc_m = re.search(r"FILE_DESCRIPTION\s*\(\s*\(\s*'([^']*)'", hdr, re.IGNORECASE)
        if desc_m:
            desc = desc_m.group(1)
    return schema, desc


def parse_step(source: str | IO[str]) -> StepAssembly:
    """Parse a STEP file and return a StepAssembly.

    *source* may be a file path (str ending with .step/.stp) or a
    text stream / string containing STEP content.
    """
    if isinstance(source, str) and (
        source.lower().endswith(".step") or source.lower().endswith(".stp")
    ):
        if not os.path.isfile(source):
            raise FileNotFoundError(f"STEP file not found: {source}")
        with open(source, "r", encoding="utf-8", errors="replace") as f:
            text = f.read()
        filename = source
    elif isinstance(source, str):
        text = source
        filename = "<string>"
    else:
        text = source.read()
        filename = getattr(source, "name", "<stream>")

    assembly = StepAssembly(filename=filename)
    assembly.schema, assembly.description = _extract_header(text)

    # Locate DATA section
    data_match = re.search(r"DATA;(.*?)ENDSEC;", text, re.DOTALL | re.IGNORECASE)
    if not data_match:
        return assembly

    entities = _read_data_section(data_match.group(1))
    assembly.entities = entities

    # ── Pass 1: extract geometry primitives ──

    for eid, ent in entities.items():
        etype = ent.entity_type

        if etype == "CARTESIAN_POINT":
            name = _extract_string(ent.args[0]) if len(ent.args) > 0 else ""
            nums = _parse_float_list(ent.args[1]) if len(ent.args) > 1 else []
            coords = Vec3(
                nums[0] if len(nums) > 0 else 0.0,
                nums[1] if len(nums) > 1 else 0.0,
                nums[2] if len(nums) > 2 else 0.0,
            )
            assembly.points[eid] = CartesianPoint(eid, name, coords)

        elif etype == "DIRECTION":
            name = _extract_string(ent.args[0]) if len(ent.args) > 0 else ""
            nums = _parse_float_list(ent.args[1]) if len(ent.args) > 1 else []
            d = Vec3(
                nums[0] if len(nums) > 0 else 0.0,
                nums[1] if len(nums) > 1 else 0.0,
                nums[2] if len(nums) > 2 else 1.0,
            )
            assembly.directions[eid] = Direction(eid, name, d)

        elif etype == "AXIS2_PLACEMENT_3D":
            name = _extract_string(ent.args[0]) if len(ent.args) > 0 else ""
            loc_ref = _extract_ref(ent.args[1]) if len(ent.args) > 1 else None
            ax_ref = _extract_ref(ent.args[2]) if len(ent.args) > 2 else None
            rd_ref = _extract_ref(ent.args[3]) if len(ent.args) > 3 else None
            loc = assembly.points.get(loc_ref, CartesianPoint(0, "", Vec3())).coords if loc_ref else Vec3()
            ax = assembly.directions.get(ax_ref, Direction(0, "", Vec3(0, 0, 1))).direction if ax_ref else Vec3(0, 0, 1)
            rd = assembly.directions.get(rd_ref, Direction(0, "", Vec3(1, 0, 0))).direction if rd_ref else Vec3(1, 0, 0)
            assembly.placements[eid] = Axis2Placement3D(eid, name, loc, ax, rd)

    # ── Pass 2: extract named parts (PRODUCT_DEFINITION, PRODUCT) ──

    products: dict[int, tuple[str, str]] = {}  # id → (name, description)
    for eid, ent in entities.items():
        if ent.entity_type == "PRODUCT":
            name = _extract_string(ent.args[0]) if len(ent.args) > 0 else f"Part_{eid}"
            desc = _extract_string(ent.args[1]) if len(ent.args) > 1 else ""
            products[eid] = (name, desc)

    # Build NamedParts
    if products:
        for pid, (name, desc) in products.items():
            part = NamedPart(id=pid, name=name, description=desc)
            assembly.parts.append(part)
    else:
        # Fallback: create a single unnamed part from all vertices
        part = NamedPart(id=0, name=os.path.basename(filename))
        assembly.parts.append(part)

    # Attach vertices to first part (simplified — full assembly resolution
    # would trace SHAPE_REPRESENTATION → PRODUCT_DEFINITION chains)
    if assembly.parts and assembly.points:
        assembly.parts[0].vertices = [p.coords for p in assembly.points.values()]
    if assembly.parts and assembly.placements:
        assembly.parts[0].placements = list(assembly.placements.values())

    return assembly


# ═══════════════════════════════════════════════════════════════════════
# Convenience
# ═══════════════════════════════════════════════════════════════════════

def parse_step_file(path: str) -> StepAssembly:
    """Parse a STEP file from disk."""
    return parse_step(path)


def parse_step_string(text: str) -> StepAssembly:
    """Parse STEP content from a string."""
    return parse_step(text)
