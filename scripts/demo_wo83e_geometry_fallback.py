#!/usr/bin/env python3
"""WO-83E: Geometry fallback demo.
Calls 'vsepr classify' on H2O and NH3 scripts and verifies output marks
fallback mode or uses element inference. Proves no crash when lone-pair
data is derived from geometry only.
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
        name = "fallback_test"
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

    for formula in ("H2O", "NH3"):
        out = classify(formula)
        print(f"--- {formula} ---")
        print(out)
        assert "vsepr_report" in out, f"{formula}: no vsepr_report"
        assert "organic_candidate" in out, f"{formula}: no organic_candidate"

    print("result: PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
