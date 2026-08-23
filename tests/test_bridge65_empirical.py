"""
tests/test_bridge65_empirical.py
=================================
Python validation tests for WO-BRIDGE-65 empirical bridge.

Covers:
    - bridge65_empirical.build_bridge65_tables() structure
    - bridge65_empirical.write_bridge65_tables() artifact output
    - bridge65_validate.run_validation() gate (all checks pass)
    - bridge65_matrix_export._VALIDATION_CASES schema
    - bridge65_matrix_export._MATERIAL_SEED_CASES schema

WO-BRIDGE-65  |  v5.1.13
"""

import json
import pathlib
import sys
import tempfile

_REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
if str(_REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(_REPO_ROOT))

from pykernel.pillars import bridge65_empirical as b65e
from pykernel.pillars import bridge65_validate as b65v
from pykernel.pillars.bridge65_matrix_export import (
    _VALIDATION_CASES,
    _MATERIAL_SEED_CASES,
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_PASS = 0
_FAIL = 0


def check(name: str, cond: bool, detail: str = "") -> None:
    global _PASS, _FAIL
    tag = "PASS" if cond else "FAIL"
    msg = f"  [{tag}] {name}"
    if detail:
        msg += f" — {detail}"
    print(msg)
    if cond:
        _PASS += 1
    else:
        _FAIL += 1


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------


def test_build_bridge65_tables_schema():
    tables = b65e.build_bridge65_tables()
    check("schema_key", tables.get("schema") == "vsepr.bridge65.empirical.v1")
    check("version_key", tables.get("version") == "5.1.13")
    check("elements_non_empty", len(tables.get("elements", [])) >= 90,
          f"found {len(tables.get('elements', []))}")
    check("policy_keys",
          all(k in tables.get("policy", {}) for k in
              ["radius_policy", "polarization_policy", "thermal_policy", "bond_gate_policy"]))


def test_element_fields():
    tables = b65e.build_bridge65_tables()
    elements = {el["symbol"]: el for el in tables["elements"]}

    for sym in ("H", "O", "Fe", "Ar", "Na", "Cl"):
        el = elements.get(sym)
        check(f"element_{sym}_present", el is not None)
        if el:
            check(f"element_{sym}_Z_positive", el.get("Z", 0) > 0, str(el.get("Z")))
            check(f"element_{sym}_radius_A_key", "radius_A" in el)
            check(f"element_{sym}_radius_source_key", "radius_source" in el)


def test_write_bridge65_tables_creates_file():
    with tempfile.TemporaryDirectory() as tmp:
        out = pathlib.Path(tmp) / "data" / "bridge65.force_matrix.json"
        b65e.write_bridge65_tables(out)
        check("write_creates_file", out.exists())
        check("write_non_empty", out.stat().st_size > 100)
        data = json.loads(out.read_text(encoding="utf-8"))
        check("written_schema", data.get("schema") == "vsepr.bridge65.empirical.v1")


def test_validation_gate():
    report = b65v.run_validation(verbose=False)
    check("validation_gate_all_pass", report.all_pass,
          f"{sum(1 for r in report.results if r.passed)}/{len(report.results)}")


def test_validation_cases_schema():
    check("validation_cases_count", len(_VALIDATION_CASES) == 6,
          str(len(_VALIDATION_CASES)))
    for case in _VALIDATION_CASES:
        name = case.get("name", "?")
        check(f"case_{name}_has_name", bool(case.get("name")))
        check(f"case_{name}_has_description", bool(case.get("description")))


def test_material_seed_cases_schema():
    check("material_seed_count", len(_MATERIAL_SEED_CASES) == 3,
          str(len(_MATERIAL_SEED_CASES)))
    for seed in _MATERIAL_SEED_CASES:
        name = seed.get("name", "?")
        check(f"seed_{name}_has_lattice", bool(seed.get("lattice")))
        check(f"seed_{name}_has_species", bool(seed.get("species")))


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    print("[BRIDGE65] test_bridge65_empirical.py\n")

    test_build_bridge65_tables_schema()
    test_element_fields()
    test_write_bridge65_tables_creates_file()
    test_validation_gate()
    test_validation_cases_schema()
    test_material_seed_cases_schema()

    total = _PASS + _FAIL
    print(f"\n  {_PASS}/{total} checks passed")
    sys.exit(0 if _FAIL == 0 else 1)
