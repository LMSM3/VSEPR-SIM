#!/usr/bin/env python3
"""WO-83D: VSEPR report formatter demo.
Calls 'vsepr classify' on a simple NH3 script and shows the VSEPR output.
Proves format_vsepr_report() is live and deterministic.
"""
from __future__ import annotations
import subprocess, sys, textwrap
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE  = REPO / "build" / "vsepr.exe"

VSIM = textwrap.dedent("""
    [project]
    name    = "demo_wo83d"
    version = "test"
    [material]
    formula = "NH3"
    """)

def main() -> int:
    if not EXE.exists():
        print(f"missing: {EXE}"); return 1
    tmp = REPO / "build" / "demo_wo83d.vsim"
    tmp.write_text(VSIM, encoding="utf-8")

    r = subprocess.run([str(EXE), "classify", str(tmp)],
                       capture_output=True, text=True)
    print(r.stdout)
    if r.returncode != 0:
        print(r.stderr); return 1
    assert "vsepr_report" in r.stdout, "vsepr_report section missing"
    assert "trigonal_pyramidal" in r.stdout or "tetrahedral" in r.stdout, \
        "expected pyramidal or tetrahedral site"
    print("result: PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
