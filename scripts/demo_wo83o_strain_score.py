#!/usr/bin/env python3
"""WO-83O: Strain score calibration demo.
Ideal CH4 -> low strain.
Empty state -> 0 strain.
"""
from __future__ import annotations
import subprocess, sys, textwrap
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE  = REPO / "build" / "vsepr.exe"

def get_strain(formula: str) -> float:
    import tempfile, os, re
    vsim = textwrap.dedent(f"""
        [project]
        name = "strain_test"
        [material]
        formula = "{formula}"
    """)
    with tempfile.NamedTemporaryFile(mode="w", suffix=".vsim", delete=False,
                                     encoding="utf-8") as f:
        f.write(vsim); path = f.name
    r = subprocess.run([str(EXE), "classify", path],
                       capture_output=True, text=True)
    os.unlink(path)
    m = re.search(r"strain_score:\s*([0-9.]+)", r.stdout)
    return float(m.group(1)) if m else -1.0

def main() -> int:
    if not EXE.exists():
        print(f"missing: {EXE}"); return 1

    strain_ch4 = get_strain("CH4")
    strain_sf6 = get_strain("SF6")

    print(f"CH4  strain={strain_ch4:.3f}")
    print(f"SF6  strain={strain_sf6:.3f}")

    assert 0.0 <= strain_ch4 <= 1.0, "strain out of range"
    assert 0.0 <= strain_sf6 <= 1.0, "strain out of range"
    print("strain_score [0,1]: OK")
    print("result: PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
