#!/usr/bin/env python3
"""WO-83N: Lipid-like score composite descriptor demo.
Long flexible hydrocarbon chain -> higher score.
Ester/carboxylic evidence -> higher score.
Excessive heteroatom fraction -> lower score.
"""
from __future__ import annotations
import subprocess, sys, textwrap
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE  = REPO / "build" / "vsepr.exe"

def get_lipid_score(formula: str) -> float:
    import tempfile, os, re
    vsim = textwrap.dedent(f"""
        [project]
        name = "lipid_score_test"
        [material]
        formula = "{formula}"
    """)
    with tempfile.NamedTemporaryFile(mode="w", suffix=".vsim", delete=False,
                                     encoding="utf-8") as f:
        f.write(vsim); path = f.name
    r = subprocess.run([str(EXE), "classify", path],
                       capture_output=True, text=True)
    os.unlink(path)
    m = re.search(r"lipid_like_score:\s*([0-9.]+)", r.stdout)
    return float(m.group(1)) if m else -1.0

def main() -> int:
    if not EXE.exists():
        print(f"missing: {EXE}"); return 1

    # Long hydrocarbon chain should be higher than simple CH4
    score_c16 = get_lipid_score("C16H34")   # hexadecane-like
    score_ch4 = get_lipid_score("CH4")
    score_ester = get_lipid_score("C16H32O2") # palmitic-acid-like
    score_hetero = get_lipid_score("C2N4O4")  # excessive heteroatom

    print(f"C16H34   lipid_like={score_c16:.3f}")
    print(f"CH4      lipid_like={score_ch4:.3f}")
    print(f"C16H32O2 lipid_like={score_ester:.3f}")
    print(f"C2N4O4   lipid_like={score_hetero:.3f}")

    assert score_c16 > score_ch4, "long chain should score higher than CH4"
    assert score_hetero <= score_c16, "high heteroatom should score lower"
    print("lipid_like_score ordering: OK")
    print("result: PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
