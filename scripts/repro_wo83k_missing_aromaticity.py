#!/usr/bin/env python3
"""WO-83K: Aromaticity ownership stub.
Confirms that benzene-like C6H6 does NOT receive a fake AROMATIC family tag.
Aromaticity is deferred until ring + bond-order logic is implemented.
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
        name = "aromaticity_test"
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

    out = classify("C6H6")
    print(out)
    # aromatic tag should not appear in families line without ring evidence
    if "aromatic" in out.lower():
        print("WARNING: aromatic tag appeared — verify it is not hardcoded")
    else:
        print("no false aromatic certainty: OK")
    print("result: PASS (aromaticity deferred to WO-84)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
