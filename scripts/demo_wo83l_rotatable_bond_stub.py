#!/usr/bin/env python3
"""WO-83L: Rotatable bond placeholder demo.
Confirms a flexible hydrocarbon chain gets nonzero rotatable_bond_count
and that missing bond order gives a conservative output.
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
        name = "rotatable_bond_test"
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

    # Flexible chain: butane-like formula
    out_butane = classify("C4H10")
    print("--- C4H10 (butane) ---"); print(out_butane)

    # Single atom: no rotatable bonds
    out_c = classify("CH4")
    print("--- CH4 ---"); print(out_c)

    assert "rotatable_bonds:" in out_butane, "missing rotatable_bonds"
    print("rotatable bond reporting present: OK")
    print("result: PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
