#!/usr/bin/env python3
"""WO-83T: Determinism regression demo.
Runs classify three times on same formula and confirms output is identical.
"""
from __future__ import annotations
import subprocess, sys, textwrap
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE  = REPO / "build" / "vsepr.exe"

def classify(formula: str, tmp_path: Path) -> str:
    vsim = textwrap.dedent(f"""
        [project]
        name = "determinism_test"
        [material]
        formula = "{formula}"
    """)
    tmp_path.write_text(vsim, encoding="utf-8")
    r = subprocess.run([str(EXE), "classify", str(tmp_path)],
                       capture_output=True, text=True)
    return r.stdout

def main() -> int:
    if not EXE.exists():
        print(f"missing: {EXE}"); return 1

    tmp = REPO / "build" / "_determinism_test.vsim"
    for formula in ("NH3", "CO2", "CH4"):
        runs = [classify(formula, tmp) for _ in range(3)]
        assert runs[0] == runs[1] == runs[2], \
            f"{formula}: classify output is not deterministic!"
        print(f"{formula}: deterministic x3: OK")

    tmp.unlink(missing_ok=True)
    print("result: PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
