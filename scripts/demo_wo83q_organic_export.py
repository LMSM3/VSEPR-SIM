#!/usr/bin/env python3
"""WO-83Q: OrganicCandidate export demo.
Confirms the formatted output includes family tags, VSEPR-derived counts,
lipid-like score, and strain score.
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
        name = "organic_export_test"
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

    out = classify("C6H12O")
    print(out)
    for field in ("primary_family:", "sp3_count:", "strain_score:",
                  "lipid_like_score:", "decomp_risk:"):
        assert field in out, f"missing: {field}"

    print("organic_candidate export complete: OK")
    print("result: PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
