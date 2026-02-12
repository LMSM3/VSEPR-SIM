"""
Configuration - Single source of truth for paths and defaults
"""
import os
from pathlib import Path
from datetime import datetime

# Project root
ROOT = Path(__file__).parent.parent.absolute()

# Build directories
BUILD_DIR = ROOT / "build"
BIN_DIR = BUILD_DIR / "bin"

# Output directories
OUT_DIR = ROOT / "out"
LOGS_DIR = ROOT / "logs"

# Data directories
DATA_DIR = ROOT / "data"
EXAMPLES_DIR = ROOT / "examples"

# Executables (C++ binaries - the authoritative compute)
VSEPR_CLI = BIN_DIR / "vsepr"
VSEPR_VIEW = BIN_DIR / "vsepr-view"
VSEPR_BATCH = BIN_DIR / "vsepr_batch"

# Scripts
SCRIPTS_DIR = ROOT / "scripts"
VIEWER_GEN = SCRIPTS_DIR / "viewer_generator.py"

# Default parameters
DEFAULT_OPTIMIZE = True
DEFAULT_MAX_STEPS = 5000
DEFAULT_FORCE_TOL = 1e-4

# Logging
LOG_DATE_FORMAT = "%Y-%m-%d"
LOG_TIME_FORMAT = "%H:%M:%S"

def get_today_log_dir():
    """Get today's log directory"""
    today = datetime.now().strftime(LOG_DATE_FORMAT)
    log_dir = LOGS_DIR / today
    log_dir.mkdir(parents=True, exist_ok=True)
    return log_dir

def ensure_dirs():
    """Ensure all required directories exist"""
    for d in [OUT_DIR, LOGS_DIR, BUILD_DIR]:
        d.mkdir(parents=True, exist_ok=True)

# Environment defaults
ENV_DEFAULTS = {
    "VSEPR_DATA": str(DATA_DIR),
    "VSEPR_OUT": str(OUT_DIR),
}

# Status symbols (because apparently you like these)
CHECKMARK = "✓"
CROSSMARK = "✗"
ARROW = "→"
BULLET = "•"
