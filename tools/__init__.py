"""
VSEPR-Sim Python Orchestrator

Single entry point for building, running, and visualizing molecular simulations.

Architecture:
    Python → orchestration, inputs/outputs, packaging, reporting, automation
    C++ → simulation, geometry optimization, heavy math, performance
    Bash → tool glue (when needed)
    OpenGL → native visualization
    HTML → web export

Usage:
    CLI: python -m tools.flower <verb> [args]
    API: from tools import build, run, viz, export, report
"""

# Expose the verbs for API usage
from .targets import (
    build,
    test, 
    run_molecule as run,
    viz,
    export,
    report,
    clean
)

# Expose utilities
from .runner import Runner
from .artifacts import ArtifactManager
from . import config

__all__ = [
    'build',
    'test',
    'run',
    'viz',
    'export', 
    'report',
    'clean',
    'Runner',
    'ArtifactManager',
    'config'
]

__version__ = '1.0.0'
