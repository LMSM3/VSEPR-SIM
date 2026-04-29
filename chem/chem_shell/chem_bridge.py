"""
chem_shell/chem_bridge.py -- Bridge adapter for the pipeline FastAPI host.

Lives alongside controller.py so no sys.path manipulation is needed.
The pipeline host imports this module to serve chem API routes.
"""

from __future__ import annotations

from pathlib import Path
import importlib, sys

# Import sibling controller.py reliably regardless of cwd
_here = Path(__file__).resolve().parent
if str(_here) not in sys.path:
    sys.path.insert(0, str(_here))

import controller as _ctrl  # noqa: E402

# Re-export the public interface
handle_command   = _ctrl.handle_command
build_state      = _ctrl.build_state
write_state      = _ctrl.write_state
classify_reaction = _ctrl.classify_reaction
parse_energy     = _ctrl.parse_energy
shell_anim_frames = _ctrl.shell_anim_frames

PRESET_998       = _ctrl.PRESET_998
PRESET_999       = _ctrl.PRESET_999
HELP_SHORT       = _ctrl.HELP_SHORT
HELP_EXTENDED    = _ctrl.HELP_EXTENDED
STATE_PATH       = _ctrl.STATE_PATH
HISTORY_PATH     = _ctrl.HISTORY_PATH


def current_state() -> dict:
    """Read and return the current state JSON, or empty dict."""
    try:
        import json
        return json.loads(STATE_PATH.read_text(encoding="utf-8"))
    except Exception:
        return {}


def history_entries(limit: int = 50) -> list[dict]:
    """Return the last N history log entries."""
    import json
    if not HISTORY_PATH.exists():
        return []
    lines = HISTORY_PATH.read_text(encoding="utf-8").strip().splitlines()
    entries = []
    for line in lines[-limit:]:
        try:
            entries.append(json.loads(line))
        except Exception:
            pass
    return entries


def validate() -> dict:
    """Quick self-check that the chem subsystem is functional."""
    try:
        st = build_state("H2 + Cl2 -> 2HCl")
        return {
            "ok": True,
            "test_reaction": st["reaction"],
            "test_class": st["class"],
            "state_dir": str(STATE_PATH.parent),
            "presets_998": len(PRESET_998),
            "presets_999": len(PRESET_999),
        }
    except Exception as e:
        return {"ok": False, "error": str(e)}
