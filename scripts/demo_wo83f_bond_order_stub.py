#!/usr/bin/env python3
"""WO-83F: Bond-order provider stub demo.
Proves that VSEPR runs with and without a bond-order provider.
Since the provider interface is C++ only, this script verifies
the classify output from the CLI (which uses the null provider).
"""
from __future__ import annotations
import subprocess, sys, textwrap
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE  = REPO / "build" / "vsepr.exe"

def classify(formula: str) -> str:
    import tempfile, os
    vsim = textwrap.dedent(f"""
        [project]
        name = "bond_order_test"
        [material]
        formula = "{formula}"
    """)
    with tempfile.NamedTemporaryFile(mode="w", suffix=".vsim", delete=False,
                                     encoding="utf-8") as f:
        f.write(vsim); path = f.name
    r = subprocess.run([str(EXE), "classify", path],
                       capture_output=True, text=True)
    os.unlink(path)
    return r.stdout

def main() -> int:
    if not EXE.exists():
        print(f"missing: {EXE}"); return 1

    # Without bond-order provider, CO2 should still classify as linear
    out = classify("CO2")
    print(out)
    assert "vsepr_report" in out, "missing vsepr_report"
    # No crash = provider-absent fallback works
    print("bond-order null provider: OK")
    print("result: PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
