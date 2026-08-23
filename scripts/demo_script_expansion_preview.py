#!/usr/bin/env python3
"""Run the neutral script expansion preview demo."""

from __future__ import annotations

import subprocess
from pathlib import Path


def main() -> int:
    repo = Path(__file__).resolve().parents[1]
    exe = repo / "build" / "vsepr.exe"
    script = (
        Path.home()
        / "OneDrive"
        / "Documents"
        / "Claude"
        / "Projects"
        / "VSIM Coding Buddy"
        / "examples"
        / "classify_acceptance"
        / "lone_pair_nh3.vsim"
    )

    if not exe.exists():
        print(f"missing executable: {exe}")
        print("result: FAIL")
        return 1

    if not script.exists():
        print(f"missing script: {script}")
        print("result: FAIL")
        return 1

    result = subprocess.run(
        [str(exe), "expand", str(script)],
        cwd=repo,
        text=True,
        capture_output=True,
        check=False,
    )

    print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="")

    if result.returncode != 0:
        print("result: FAIL")
        return result.returncode

    required = [
        "parse_status: ok",
        "direct_execution: not run",
        "expansion_candidates:",
        "selected_execution: none",
        "result: PASS",
    ]
    missing = [item for item in required if item not in result.stdout]
    if missing:
        print(f"missing expected output: {', '.join(missing)}")
        print("result: FAIL")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
