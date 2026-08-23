#!/usr/bin/env python3
"""WO-83J: Ring detection ownership stub.
Proves that ring_count is a placeholder (0) in the current classify output
and that missing ring logic is visible rather than silently wrong.
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
        name = "ring_detection_test"
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

    # Benzene-like formula: ring_count should be 0 (placeholder, not detected)
    out = classify("C6H6")
    print(out)
    assert "ring_count:      0" in out, \
        "ring_count should be 0 (SSSR not implemented — ring detection deferred)"
    print("ring_count=0 (deferred): OK — ring detection not falsely claimed")
    print("result: PASS (known limitation documented)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
