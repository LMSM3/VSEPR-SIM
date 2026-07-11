"""
plant_bridge.py -- Bridge adapter exposing plant_alpha2 through chem_shell
for the pipeline FastAPI host and automatic report generation.

VSEPR-SIM 4.0.3
"""

from __future__ import annotations

import sys
from pathlib import Path

# Ensure pykernel is importable
_root = Path(__file__).resolve().parent.parent.parent
if str(_root) not in sys.path:
    sys.path.insert(0, str(_root))

# Direct-load to avoid vispy gate in pykernel.__init__
import importlib.util as _ilu

_spec = _ilu.spec_from_file_location(
    "plant_alpha2", str(_root / "pykernel" / "plant_alpha2.py"))
_mod = _ilu.module_from_spec(_spec)
sys.modules["plant_alpha2"] = _mod
_spec.loader.exec_module(_mod)

# Re-export public interface
MATERIALS       = _mod.MATERIALS
MISSING         = _mod.MISSING
Confidence      = _mod.Confidence
Material        = _mod.Material
SelectiveOutput = _mod.SelectiveOutput
fit_polynomial  = _mod.fit_polynomial
fit_shomate     = _mod.fit_shomate
export_csv      = _mod.export_csv
export_json     = _mod.export_json
validate        = _mod.validate
list_materials  = _mod.list_materials
lookup          = _mod.lookup


def material_summary(formula: str, T: float = 1000.0) -> dict:
    """Get a material property summary at temperature T."""
    mat = _mod.lookup(formula)
    if mat is None:
        return {"error": f"Unknown material: {formula}"}
    return mat.summary_at(T)


def generate_report_data(
    categories: list[str] | None = None,
    confidence: list[str] | None = None,
    T_points: list[float] | None = None,
) -> list[dict]:
    """Generate selective output table for automatic reports."""
    out = SelectiveOutput(
        include_categories=categories or ["fuel", "coolant", "structural"],
        include_confidence=confidence or ["C1", "C2", "C3"],
        T_points=T_points or [300, 500, 800, 1000, 1500, 2000, 2500],
    )
    return out.generate_table()


def fit_from_data(T_data: list[float], y_data: list[float],
                  degree: int = 3, form: str = "poly") -> dict:
    """Run curve fitting and return result as dict."""
    if form == "shomate":
        result = fit_shomate(T_data, y_data)
    else:
        result = fit_polynomial(T_data, y_data, degree=degree)
    return {
        "coeffs": result.coeffs,
        "form": result.form,
        "degree": result.degree,
        "r_squared": result.r_squared,
        "rms_error": result.rms_error,
        "max_error": result.max_error,
        "T_min": result.T_min,
        "T_max": result.T_max,
        "n_points": result.n_points,
        "confidence": result.confidence,
    }
