#!/usr/bin/env python3
"""WO-83R: Full classify pipeline smoke demo.
Runs fingerprints -> VSEPR -> organic -> export in one CLI call.
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
        name = "pipeline_smoke"
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

    for formula in ("CH4", "NH3", "CO2", "SF6", "C6H12O"):
        out = classify(formula)
        print(f"=== {formula} ==="); print(out)
        assert "vsepr_report" in out
        assert "organic_candidate" in out
        assert "result: PASS" in out

    print("full classify pipeline: OK")
    print("result: PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
