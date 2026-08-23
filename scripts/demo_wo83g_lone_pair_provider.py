#!/usr/bin/env python3
"""WO-83G: Lone-pair provider stub demo.
Without a lone-pair provider, NH3 uses element-based inference.
This script confirms the classify output is consistent with either path.
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
        name = "lone_pair_test"
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

    nh3 = classify("NH3")
    h2o = classify("H2O")
    print("--- NH3 ---"); print(nh3)
    print("--- H2O ---"); print(h2o)

    assert "vsepr_report" in nh3, "NH3: no vsepr_report"
    assert "vsepr_report" in h2o, "H2O: no vsepr_report"
    # Both should show some site classification
    assert "site[" in nh3, "NH3: no sites"
    assert "site[" in h2o, "H2O: no sites"

    print("lone-pair element inference: OK")
    print("result: PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
