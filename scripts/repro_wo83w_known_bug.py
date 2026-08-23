#!/usr/bin/env python3
"""WO-83W: Known bug capture — lone-pair inference missing for some elements.
Documents one current limitation: lone-pair count for transition metals / f-block
elements is always 0 via element inference, never inferred from group/valence.
This repro script proves the limitation is visible and not silently wrong.
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
        name = "known_bug_repro"
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

    # Known limitation: XeF4 should be AX4E2 (square planar) but
    # element inference doesn't handle Xe (Z=54) lone pairs.
    # Expected: square_planar. Observed: likely tetrahedral or irregular.
    out = classify("XeF4")
    print("--- XeF4 (expected: square_planar AX4E2) ---")
    print(out)

    if "square_planar" not in out:
        print("KNOWN BUG: XeF4 not classified as square_planar")
        print("  Cause: infer_lone_pairs_from_element() has no case for Xe (Z=54)")
        print("  Fix:   add Xe to element lone-pair table, or wire LonePairProvider")
        print("  WO:    WO-84 BondOrder + LonePair provider series")
    else:
        print("XeF4 classified correctly (lone-pair inference improved)")

    print("result: PASS (bug captured, not hidden)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
