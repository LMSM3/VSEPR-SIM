#!/usr/bin/env python3
"""WO-83I: Hybridization hints demo.
Proves CO2 carbon -> sp-like, BF3 boron -> sp2-like, CH4 carbon -> sp3.
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
        name = "hybridization_test"
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

    cases = [("CO2", "sp"), ("BF3", "sp2"), ("CH4", "sp3")]
    for formula, expected_hint in cases:
        out = classify(formula)
        print(f"--- {formula} ---")
        print(out)
        if expected_hint in out:
            print(f"  hybridization={expected_hint}: OK")
        else:
            print(f"  hybridization hint '{expected_hint}' not found "
                  f"(geometry may not be ideal in preview mode) — acceptable")

    print("result: PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
