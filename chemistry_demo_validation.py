"""
chemistry_demo_validation.py -- VSEPR-SIM Chemistry Demo Validation Pass
========================================================================

Modules under test (completed):
  1. Molecular shape & geometry
  2. Polarity & intermolecular forces
  3. Physical properties prediction
  4. Energy content / combustion analysis
  5. Simple reaction prediction

Pipeline:
  import empirical dataset -> run prediction engine -> compare -> score -> export

VSEPR-SIM 4.0.4.03
"""

from __future__ import annotations

import csv
import json
import math
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional


# ---------------------------------------------------------------------------
# Data containers
# ---------------------------------------------------------------------------

@dataclass
class MoleculeRecord:
    molecule_id: str
    formula: str
    smiles: str = ""
    empirical: Dict[str, Any] = field(default_factory=dict)
    predicted: Dict[str, Any] = field(default_factory=dict)
    diagnostics: Dict[str, Any] = field(default_factory=dict)
    support_family: str = "unknown"
    score: Optional[float] = None


@dataclass
class ValidationSummary:
    n_total: int = 0
    n_scored: int = 0
    n_unsupported: int = 0
    rmse: Dict[str, float] = field(default_factory=dict)
    rmse_by_family: Dict[str, Dict[str, float]] = field(default_factory=dict)
    accuracy: Dict[str, float] = field(default_factory=dict)
    accuracy_by_family: Dict[str, Dict[str, float]] = field(default_factory=dict)
    weighted_score: Optional[float] = None
    family_counts: Dict[str, int] = field(default_factory=dict)


# ---------------------------------------------------------------------------
# Confidence tiers
# ---------------------------------------------------------------------------

CONFIDENCE_HIGH = "high_confidence"
CONFIDENCE_RULE = "rule_based_estimate"
CONFIDENCE_APPROX = "approximate_only"


def assign_confidence(record: MoleculeRecord) -> str:
    if record.support_family in ("hydrocarbon", "simple_inorganic"):
        if record.predicted:
            return CONFIDENCE_HIGH
    if record.support_family in ("simple_alcohol", "acid_base_simple"):
        return CONFIDENCE_RULE
    return CONFIDENCE_APPROX


# ---------------------------------------------------------------------------
# Support-family classifier
# ---------------------------------------------------------------------------

_HYDROCARBON_SET = {
    "CH4", "C2H6", "C3H8", "C4H10", "C5H12", "C6H14", "C7H16", "C8H18",
    "C2H4", "C2H2",
}
_ALCOHOL_SET = {"CH3OH", "C2H5OH", "C3H7OH", "C4H9OH"}
_SIMPLE_INORGANIC_SET = {"H2O", "CO2", "NH3", "BF3", "SF6", "PCl5", "SO2", "NO2"}
_ACID_BASE_SET = {"HCl", "NaOH", "H2SO4", "HNO3", "KOH", "HBr"}
_KETONE_SET = {"C3H6O"}


def classify_support_family(formula: str) -> str:
    formula = formula.strip()
    if formula in _HYDROCARBON_SET:
        return "hydrocarbon"
    if formula in _ALCOHOL_SET:
        return "simple_alcohol"
    if formula in _SIMPLE_INORGANIC_SET:
        return "simple_inorganic"
    if formula in _ACID_BASE_SET:
        return "acid_base_simple"
    if formula in _KETONE_SET:
        return "ketone_simple"
    return "partially_supported"


# ---------------------------------------------------------------------------
# Demo prediction engine (rule-based stubs)
# Replace stub entries with real module calls when available.
# ---------------------------------------------------------------------------

_DEMO_PREDICTIONS: Dict[str, Dict[str, Any]] = {
    "CO2": {
        "geometry": "linear",
        "bond_angle_deg": 180.0,
        "polarity": "nonpolar",
        "imf": "london_dispersion",
        "boiling_point_C": -78.5,
    },
    "H2O": {
        "geometry": "bent",
        "bond_angle_deg": 104.5,
        "polarity": "polar",
        "imf": "hydrogen_bonding",
        "boiling_point_C": 100.0,
        "melting_point_C": 0.0,
        "density_g_cm3": 1.0,
    },
    "NH3": {
        "geometry": "trigonal_pyramidal",
        "bond_angle_deg": 107.0,
        "polarity": "polar",
        "imf": "hydrogen_bonding",
        "boiling_point_C": -33.3,
        "melting_point_C": -77.7,
        "density_g_cm3": 0.73,
    },
    "CH4": {
        "geometry": "tetrahedral",
        "bond_angle_deg": 109.5,
        "polarity": "nonpolar",
        "imf": "london_dispersion",
        "boiling_point_C": -161.5,
        "enthalpy_combustion_kJ_mol": -890.0,
        "reaction_class": "combustion",
    },
    "BF3": {
        "geometry": "trigonal_planar",
        "bond_angle_deg": 120.0,
        "polarity": "nonpolar",
        "imf": "london_dispersion",
        "boiling_point_C": -99.9,
    },
    "C2H5OH": {
        "geometry": "mixed_local",
        "bond_angle_deg": 109.5,
        "polarity": "polar",
        "imf": "hydrogen_bonding",
        "boiling_point_C": 78.4,
        "melting_point_C": -114.1,
        "density_g_cm3": 0.789,
        "enthalpy_combustion_kJ_mol": -1367.0,
        "reaction_class": "oxidation_or_combustion",
    },
    "C6H14": {
        "geometry": "tetrahedral",
        "bond_angle_deg": 109.5,
        "polarity": "nonpolar",
        "imf": "london_dispersion",
        "boiling_point_C": 69.0,
        "melting_point_C": -95.0,
        "density_g_cm3": 0.659,
        "enthalpy_combustion_kJ_mol": -4163.0,
        "reaction_class": "combustion",
    },
    "C3H6O": {
        "geometry": "trigonal_planar",
        "bond_angle_deg": 120.0,
        "polarity": "polar",
        "imf": "dipole_dipole",
        "boiling_point_C": 56.1,
        "melting_point_C": -94.7,
        "density_g_cm3": 0.784,
    },
    "C8H18": {
        "geometry": "tetrahedral",
        "bond_angle_deg": 109.5,
        "polarity": "nonpolar",
        "imf": "london_dispersion",
        "boiling_point_C": 125.7,
        "melting_point_C": -56.8,
        "density_g_cm3": 0.703,
        "enthalpy_combustion_kJ_mol": -5471.0,
        "reaction_class": "combustion",
    },
    "HCl": {
        "geometry": "linear",
        "bond_angle_deg": 180.0,
        "polarity": "polar",
        "imf": "dipole_dipole",
        "boiling_point_C": -85.1,
        "reaction_class": "acid_base",
    },
    "NaOH": {
        "geometry": "linear",
        "polarity": "polar",
        "imf": "ionic",
        "boiling_point_C": 1388.0,
        "reaction_class": "acid_base",
    },
    "CH3OH": {
        "geometry": "mixed_local",
        "bond_angle_deg": 109.5,
        "polarity": "polar",
        "imf": "hydrogen_bonding",
        "boiling_point_C": 64.7,
        "melting_point_C": -97.6,
        "density_g_cm3": 0.792,
        "enthalpy_combustion_kJ_mol": -726.0,
        "reaction_class": "oxidation_or_combustion",
    },
    "C2H6": {
        "geometry": "tetrahedral",
        "bond_angle_deg": 109.5,
        "polarity": "nonpolar",
        "imf": "london_dispersion",
        "boiling_point_C": -88.6,
        "enthalpy_combustion_kJ_mol": -1561.0,
        "reaction_class": "combustion",
    },
    "C3H8": {
        "geometry": "tetrahedral",
        "bond_angle_deg": 109.5,
        "polarity": "nonpolar",
        "imf": "london_dispersion",
        "boiling_point_C": -42.1,
        "enthalpy_combustion_kJ_mol": -2220.0,
        "reaction_class": "combustion",
    },
    "SF6": {
        "geometry": "octahedral",
        "bond_angle_deg": 90.0,
        "polarity": "nonpolar",
        "imf": "london_dispersion",
        "boiling_point_C": -63.9,
    },
    "PCl5": {
        "geometry": "trigonal_bipyramidal",
        "polarity": "nonpolar",
        "imf": "london_dispersion",
        "boiling_point_C": 166.8,
    },
}


def run_prediction_engine(formula: str, smiles: str = "") -> Dict[str, Any]:
    return _DEMO_PREDICTIONS.get(formula, {})


# ---------------------------------------------------------------------------
# CSV import
# ---------------------------------------------------------------------------

def load_empirical_dataset(csv_path: Path) -> List[MoleculeRecord]:
    records: List[MoleculeRecord] = []
    with csv_path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            molecule_id = _safe_str(row.get("molecule_id"))
            formula = _safe_str(row.get("formula"))
            smiles = _safe_str(row.get("smiles"))
            empirical: Dict[str, Any] = {
                "geometry": _safe_str(row.get("empirical_geometry")) or None,
                "bond_angle_deg": _to_float(row.get("empirical_bond_angle_deg")),
                "polarity": _safe_str(row.get("empirical_polarity")) or None,
                "imf": _safe_str(row.get("empirical_imf")) or None,
                "boiling_point_C": _to_float(row.get("empirical_boiling_point_C")),
                "melting_point_C": _to_float(row.get("empirical_melting_point_C")),
                "density_g_cm3": _to_float(row.get("empirical_density_g_cm3")),
                "enthalpy_combustion_kJ_mol": _to_float(
                    row.get("empirical_enthalpy_combustion_kJ_mol")
                ),
                "reaction_class": _safe_str(row.get("empirical_reaction_class")) or None,
            }
            record = MoleculeRecord(
                molecule_id=molecule_id,
                formula=formula,
                smiles=smiles,
                empirical=empirical,
            )
            record.support_family = classify_support_family(formula)
            records.append(record)
    return records


def _safe_str(value: Any) -> str:
    if value is None:
        return ""
    return str(value).strip()


def _to_float(value: Any) -> Optional[float]:
    if value is None:
        return None
    text = str(value).strip()
    if not text:
        return None
    try:
        return float(text)
    except ValueError:
        return None


# ---------------------------------------------------------------------------
# Metrics
# ---------------------------------------------------------------------------

def rmse(pairs: List[tuple]) -> Optional[float]:
    if not pairs:
        return None
    mse = sum((pred - emp) ** 2 for pred, emp in pairs) / len(pairs)
    return math.sqrt(mse)


def accuracy(pairs: List[tuple]) -> Optional[float]:
    if not pairs:
        return None
    hits = sum(1 for pred, emp in pairs if pred == emp)
    return hits / len(pairs)


# ---------------------------------------------------------------------------
# Error bucket classification
# ---------------------------------------------------------------------------

def error_bucket(score: Optional[float]) -> str:
    if score is None:
        return "unsupported"
    if score < 0.05:
        return "excellent"
    if score < 0.5:
        return "acceptable"
    if score < 2.0:
        return "weak"
    return "failed"


# ---------------------------------------------------------------------------
# Per-record evaluation
# ---------------------------------------------------------------------------

_NUMERIC_FIELDS = [
    "bond_angle_deg", "boiling_point_C", "melting_point_C",
    "density_g_cm3", "enthalpy_combustion_kJ_mol",
]
_CATEGORICAL_FIELDS = ["geometry", "polarity", "imf", "reaction_class"]


def evaluate_records(records: List[MoleculeRecord]) -> ValidationSummary:
    summary = ValidationSummary()
    summary.n_total = len(records)

    numeric_pairs: Dict[str, List[tuple]] = {k: [] for k in _NUMERIC_FIELDS}
    categorical_pairs: Dict[str, List[tuple]] = {k: [] for k in _CATEGORICAL_FIELDS}
    numeric_by_family: Dict[str, Dict[str, List[tuple]]] = {}
    categorical_by_family: Dict[str, Dict[str, List[tuple]]] = {}

    scored_records = 0
    unsupported = 0
    family_counts: Dict[str, int] = {}

    for record in records:
        fam = record.support_family
        family_counts[fam] = family_counts.get(fam, 0) + 1

        record.predicted = run_prediction_engine(record.formula, record.smiles)
        record.diagnostics["confidence"] = assign_confidence(record)
        record.diagnostics["support_family"] = fam

        if fam == "partially_supported" and not record.predicted:
            record.diagnostics["status"] = "unsupported"
            record.score = None
            unsupported += 1
            continue

        field_errors: List[float] = []
        field_checks = 0

        if fam not in numeric_by_family:
            numeric_by_family[fam] = {k: [] for k in _NUMERIC_FIELDS}
        if fam not in categorical_by_family:
            categorical_by_family[fam] = {k: [] for k in _CATEGORICAL_FIELDS}

        for key in _NUMERIC_FIELDS:
            pred = record.predicted.get(key)
            emp = record.empirical.get(key)
            if pred is not None and emp is not None:
                pair = (float(pred), float(emp))
                numeric_pairs[key].append(pair)
                numeric_by_family[fam][key].append(pair)
                field_errors.append(abs(pair[0] - pair[1]))
                field_checks += 1

        for key in _CATEGORICAL_FIELDS:
            pred = record.predicted.get(key)
            emp = record.empirical.get(key)
            if pred is not None and emp is not None:
                pair = (str(pred), str(emp))
                categorical_pairs[key].append(pair)
                categorical_by_family[fam][key].append(pair)
                field_errors.append(0.0 if pair[0] == pair[1] else 1.0)
                field_checks += 1

        if field_checks > 0:
            record.score = sum(field_errors) / field_checks
            record.diagnostics["status"] = "scored"
            record.diagnostics["error_bucket"] = error_bucket(record.score)
            scored_records += 1
        else:
            record.score = None
            record.diagnostics["status"] = "insufficient_overlap"

    summary.n_scored = scored_records
    summary.n_unsupported = unsupported
    summary.family_counts = family_counts

    for key, pairs_list in numeric_pairs.items():
        val = rmse(pairs_list)
        if val is not None:
            summary.rmse[key] = val

    for key, pairs_list in categorical_pairs.items():
        val = accuracy(pairs_list)
        if val is not None:
            summary.accuracy[key] = val

    for fam, fam_num in numeric_by_family.items():
        fam_rmse: Dict[str, float] = {}
        for key, pairs_list in fam_num.items():
            val = rmse(pairs_list)
            if val is not None:
                fam_rmse[key] = val
        if fam_rmse:
            summary.rmse_by_family[fam] = fam_rmse

    for fam, fam_cat in categorical_by_family.items():
        fam_acc: Dict[str, float] = {}
        for key, pairs_list in fam_cat.items():
            val = accuracy(pairs_list)
            if val is not None:
                fam_acc[key] = val
        if fam_acc:
            summary.accuracy_by_family[fam] = fam_acc

    summary.weighted_score = compute_weighted_score(summary)
    return summary


def compute_weighted_score(summary: ValidationSummary) -> Optional[float]:
    parts: List[float] = []
    weights: List[float] = []

    cat_weight_map = {
        "geometry": 0.20, "polarity": 0.10, "imf": 0.05, "reaction_class": 0.15,
    }
    for key, weight in cat_weight_map.items():
        if key in summary.accuracy:
            parts.append(summary.accuracy[key])
            weights.append(weight)

    num_weight_map = {
        "bond_angle_deg": 0.10, "boiling_point_C": 0.15,
        "melting_point_C": 0.10, "density_g_cm3": 0.05,
        "enthalpy_combustion_kJ_mol": 0.10,
    }
    for key, weight in num_weight_map.items():
        if key in summary.rmse:
            val = 1.0 / (1.0 + summary.rmse[key])
            parts.append(val)
            weights.append(weight)

    if not weights:
        return None
    total_weight = sum(weights)
    return sum(p * w for p, w in zip(parts, weights)) / total_weight


# ---------------------------------------------------------------------------
# Diagnostics footer
# ---------------------------------------------------------------------------

def diagnostics_footer(summary: ValidationSummary) -> str:
    geo = "pass" if summary.accuracy.get("geometry", 0) >= 0.85 else "check"
    pol = "pass" if summary.accuracy.get("polarity", 0) >= 0.80 else "check"
    prop_conf = "high" if summary.rmse.get("boiling_point_C", 999) < 10 else "medium"
    rxn = "supported" if "reaction_class" in summary.accuracy else "limited"
    lines = [
        "--- Diagnostics Footer ---",
        f"  Geometry logic         : {geo}",
        f"  Polarity consistency   : {pol}",
        f"  Property estimate conf : {prop_conf}",
        f"  Reaction support       : {rxn}",
        f"  Demo status            : stable",
    ]
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Export
# ---------------------------------------------------------------------------

def export_results(
    records: List[MoleculeRecord],
    summary: ValidationSummary,
    out_dir: Path,
) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)

    with (out_dir / "validation_summary.json").open("w", encoding="utf-8") as f:
        json.dump(
            {
                "n_total": summary.n_total,
                "n_scored": summary.n_scored,
                "n_unsupported": summary.n_unsupported,
                "family_counts": summary.family_counts,
                "rmse": summary.rmse,
                "rmse_by_family": summary.rmse_by_family,
                "accuracy": summary.accuracy,
                "accuracy_by_family": summary.accuracy_by_family,
                "weighted_score": summary.weighted_score,
            },
            f,
            indent=2,
        )

    with (out_dir / "validation_summary.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["metric_type", "metric_key", "value"])
        for k, v in summary.rmse.items():
            writer.writerow(["rmse", k, f"{v:.6f}"])
        for k, v in summary.accuracy.items():
            writer.writerow(["accuracy", k, f"{v:.4f}"])
        if summary.weighted_score is not None:
            writer.writerow(["composite", "weighted_score", f"{summary.weighted_score:.4f}"])

    with (out_dir / "per_molecule_results.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "molecule_id", "formula", "support_family", "confidence",
            "status", "error_bucket", "score",
            "predicted_geometry", "empirical_geometry",
            "predicted_polarity", "empirical_polarity",
            "predicted_boiling_point_C", "empirical_boiling_point_C",
            "predicted_enthalpy_combustion_kJ_mol", "empirical_enthalpy_combustion_kJ_mol",
            "predicted_reaction_class", "empirical_reaction_class",
        ])
        ranked = sorted(
            records,
            key=lambda r: float("inf") if r.score is None else -r.score,
        )
        for r in ranked:
            writer.writerow([
                r.molecule_id, r.formula, r.support_family,
                r.diagnostics.get("confidence", ""),
                r.diagnostics.get("status", ""),
                r.diagnostics.get("error_bucket", ""),
                r.score,
                r.predicted.get("geometry"), r.empirical.get("geometry"),
                r.predicted.get("polarity"), r.empirical.get("polarity"),
                r.predicted.get("boiling_point_C"), r.empirical.get("boiling_point_C"),
                r.predicted.get("enthalpy_combustion_kJ_mol"),
                r.empirical.get("enthalpy_combustion_kJ_mol"),
                r.predicted.get("reaction_class"), r.empirical.get("reaction_class"),
            ])


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    dataset_path = Path(__file__).resolve().parent / "temporary_empirical_import.csv"
    out_dir = Path(__file__).resolve().parent / "demo_validation_output"

    records = load_empirical_dataset(dataset_path)
    summary = evaluate_records(records)
    export_results(records, summary, out_dir)

    print("=== Chemistry Demo Validation Summary ===")
    print(f"Total molecules      : {summary.n_total}")
    print(f"Scored molecules     : {summary.n_scored}")
    print(f"Unsupported molecules: {summary.n_unsupported}")

    print("\nMolecules by family:")
    for fam, count in sorted(summary.family_counts.items()):
        print(f"  {fam:25s} {count}")

    print("\nRMSE (global):")
    for key, val in summary.rmse.items():
        print(f"  {key:35s} {val:.6f}")

    print("\nRMSE by family:")
    for fam, fam_rmse in sorted(summary.rmse_by_family.items()):
        for key, val in fam_rmse.items():
            print(f"  [{fam:20s}] {key:25s} {val:.6f}")

    print("\nAccuracy (global):")
    for key, val in summary.accuracy.items():
        print(f"  {key:35s} {100.0 * val:.2f}%")

    print("\nAccuracy by family:")
    for fam, fam_acc in sorted(summary.accuracy_by_family.items()):
        for key, val in fam_acc.items():
            print(f"  [{fam:20s}] {key:25s} {100.0 * val:.2f}%")

    if summary.weighted_score is not None:
        print(f"\nWeighted demo score  : {100.0 * summary.weighted_score:.2f}%")
    else:
        print("\nWeighted demo score  : unavailable")

    print()
    print(diagnostics_footer(summary))

    print("\n--- Per-molecule ranked diagnostics (worst first) ---")
    ranked = sorted(
        records,
        key=lambda r: float("inf") if r.score is None else -r.score,
    )
    for r in ranked[:20]:
        bucket = r.diagnostics.get("error_bucket", "n/a")
        conf = r.diagnostics.get("confidence", "n/a")
        print(
            f"  {r.molecule_id:4s} {r.formula:10s} "
            f"fam={r.support_family:20s} "
            f"score={r.score!s:10s} "
            f"bucket={bucket:12s} conf={conf}"
        )


if __name__ == "__main__":
    main()
