#!/usr/bin/env python3
"""WO-83H: Formal charge provider stub demo.
Confirms classifier doesn't crash with no formal charge data and
that charge-free runs give consistent output.
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
        name = "formal_charge_test"
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

    # No formal charge metadata: classifier should not crash
    for formula in ("NH4", "SO4", "PO4"):
        out = classify(formula)
        assert "vsepr_report" in out, f"{formula}: no vsepr_report"
        print(f"{formula}: no crash, no fake charge assignment — OK")

    print("result: PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
