#!/usr/bin/env python3
"""WO-83M: Organic family guardrails demo.
Proves family tags show source (inferred/manual/default) and that
lipid-like remains score-based rather than enum-only.
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
        name = "family_guardrail_test"
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

    for formula in ("CH4", "C6H12", "C16H32O2"):
        out = classify(formula)
        print(f"--- {formula} ---"); print(out)
        # family source tag must appear
        assert "[inferred]" in out or "[default]" in out, \
            f"{formula}: missing family source tag"
        # lipid_like_score must appear
        assert "lipid_like_score:" in out, f"{formula}: missing lipid_like_score"

    print("family source tags visible: OK")
    print("lipid_like_score visible: OK")
    print("result: PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
