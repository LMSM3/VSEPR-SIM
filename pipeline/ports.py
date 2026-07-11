"""
pipeline/ports.py -- Port reservation registry for VSEPR-SIM webapp services.

Reserved range: 8000-9000 (V4.0.4.01 beta)

All VSEPR-SIM network services MUST register their port here.
No service may bind a port in the 8000-9000 range without an entry.
"""

from __future__ import annotations
from dataclasses import dataclass


@dataclass(frozen=True)
class PortEntry:
    port: int
    service: str
    module: str
    description: str
    branch: str = "V4.0.4.01-beta"


# ── Registry ──────────────────────────────────────────────────────────────────

PORTS: dict[int, PortEntry] = {}


def _reg(port, service, module, desc):
    PORTS[port] = PortEntry(port, service, module, desc)


_reg(8765, "dashboard",  "pipeline.host",      "Result host, dashboard, JSON API, chem/plant routes")
_reg(8800, "continual",  "pipeline.continual",  "Continual reaction generation + SSE stream")
_reg(8801, "continual-ws", "pipeline.continual", "WebSocket feed (reserved, not yet active)")
_reg(8900, "gpu-bridge", "pipeline.gpu_bridge",  "GPU compute bridge (reserved, not yet active)")

del _reg


def is_reserved(port: int) -> bool:
    """Check if a port is in the VSEPR-SIM reserved range."""
    return 8000 <= port <= 9000


def available_ports() -> list[int]:
    """Return unassigned ports in the reserved range."""
    return [p for p in range(8000, 9001) if p not in PORTS]


def summary() -> str:
    lines = ["VSEPR-SIM Port Registry (8000-9000):"]
    for p in sorted(PORTS.values(), key=lambda e: e.port):
        status = "ACTIVE" if p.module in ("pipeline.host", "pipeline.continual") else "RESERVED"
        lines.append(f"  {p.port}  {p.service:<16s} [{status}]  {p.description}")
    lines.append(f"  {len(available_ports())} ports unassigned in range")
    return "\n".join(lines)
