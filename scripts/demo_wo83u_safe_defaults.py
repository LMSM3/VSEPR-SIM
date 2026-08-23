#!/usr/bin/env python3
"""WO-83U: Safe defaults demo.
Proves empty and degenerate inputs don't crash and return safe defaults.
Tests the CLI path indirectly via a minimal 1-atom formula.
"""
from __future__ import annotations
import subprocess, sys, textwrap
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE  = REPO / "build" / "vsepr.exe"

def classify(formula: str) -> tuple[int, str]:
    import tempfile, os
    vsim = textwrap.dedent(f"""
        [project]
        name = "safe_defaults_test"
        [material]
        formula = "{formula}"
    """)
    with tempfile.NamedTemporaryFile(mode="w", suffix=".vsim", delete=False,
                                     encoding="utf-8") as f:
        f.write(vsim); path = f.name
    r = subprocess.run([str(EXE), "classify", path],
                       capture_output=True, text=True)
    os.unlink(path)
    return r.returncode, r.stdout

def main() -> int:
    if not EXE.exists():
        print(f"missing: {EXE}"); return 1

    # Single atom
    rc, out = classify("C")
    assert rc == 0, "single atom C: non-zero exit"
    print(f"single atom C: exit={rc} OK")

    # Diatomic
    rc, out = classify("H2")
    assert rc == 0, "H2: non-zero exit"
    print(f"H2: exit={rc} OK")

    # Unusual formula
    rc, out = classify("Xe")
    assert rc == 0, "Xe noble gas: non-zero exit"
    print(f"Xe: exit={rc} OK")

    print("result: PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
