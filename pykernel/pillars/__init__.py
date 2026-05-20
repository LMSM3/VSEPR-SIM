"""
VSEPR-SIM Five Pillars Engine
=============================

Pillar A: SteamTables++     — Multi-material thermophysical atlas
Pillar B: Crystal Discovery — Structure generation and catalog
Pillar C: Metals & Macros   — Macro material property engine
Pillar D: SmartSampling     — Universal adaptive sampling manager
Pillar E: Golden Project    — Unified discovery atlas orchestrator
Pillar F: EmpiricalChem     — Atomic empirical data layer (v5.1.3)

Each pillar is self-contained but shares the SmartSampling core
and feeds into the Golden Project for cross-pillar ranking.
"""

__version__ = "1.0.0"

from . import empirical_chem  # noqa: F401  Pillar F -- atomic empirical data layer
