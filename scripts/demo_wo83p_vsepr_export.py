#!/usr/bin/env python3
"""WO-83P: VSEPR report export demo.
Confirms 'vsepr classify' output is deterministic and contains
enough data for a reporting layer (site list, summary counts, confidence).
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
        name = "vsepr_export_test"
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

    out1 = classify("CO2")
    out2 = classify("CO2")
    # Determinism
    assert out1 == out2, "VSEPR output is not deterministic"
    # Required report fields
    for field in ("sites:", "linear_count:", "conf=", "rms_dev="):
        assert field in out1, f"missing field: {field}"

    print(out1)
    print("vsepr export: deterministic + complete: OK")
    print("result: PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
