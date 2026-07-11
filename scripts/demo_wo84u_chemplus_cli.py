#!/usr/bin/env python3
"""WO-84U: ChemPlus CLI classify integration demo.

Validates that `vsepr classify <script.vsim>` correctly surfaces a [ChemPlus]
block when the script contains a [chem_plus] section.

Exercises:
  - combustion reaction via [chem_plus]
  - vsepr_link = true populates VSEPR tag in classify output
  - preset = 999 surfaces multiple reactions
  - class_override respected
  - classify output contains [ChemPlus] header
  - PNG 3D bar export (reaction energy landscape, Day 84+ rule)

PASS criteria:
  PASS: [ChemPlus] header present in classify output
  PASS: combustion classified
  PASS: energy value in output
  PASS: vsepr tag in output
  PASS: preset 999 produces >=3 reaction lines
  PASS: class_override=synthesis in output
  PASS: PNG 3D export confirmed
"""
from __future__ import annotations
import sys, re, subprocess
from pathlib import Path

REPO    = Path(__file__).resolve().parents[1]
VSEPR   = REPO / "build" / "vsepr.exe"
OUT_DIR = REPO / "out" / "wo84u"
TEMP    = REPO / "Temp"

_PASS = 0
_FAIL = 0


def check(cond: bool, label: str) -> None:
    global _PASS, _FAIL
    if cond:
        _PASS += 1
        print(f"  PASS: {label}")
    else:
        _FAIL += 1
        print(f"  FAIL: {label}")


def run_classify(vsim_text: str, tag: str) -> str:
    """Write a temp vsim file and return the stdout of vsepr classify."""
    TEMP.mkdir(parents=True, exist_ok=True)
    p = TEMP / f"wo84u_{tag}.vsim"
    p.write_text(vsim_text, encoding="utf-8")
    result = subprocess.run(
        [str(VSEPR), "classify", str(p)],
        capture_output=True, text=True
    )
    return result.stdout + result.stderr


# ---------------------------------------------------------------------------
# Test 1: combustion + vsepr_link
# ---------------------------------------------------------------------------
def test_combustion():
    print("\n  CH4 + 2O2 -> CO2 + 2H2O + 891 kJ  (vsepr_link=true)")
    out = run_classify(
        "[project]\nname = wo84u_combustion\n\n"
        "[formula]\nformula = CH4\n\n"
        "[chem_plus]\nreaction = CH4 + 2O2 -> CO2 + 2H2O + 891 kJ\nvsepr_link = true\n",
        "combustion"
    )
    check("[ChemPlus]" in out,               "ChemPlus header present")
    check("combustion" in out,               "combustion classified")
    check("891" in out,                      "energy 891 kJ present")
    check("AX2" in out,                      "vsepr_tag AX2 (CO2 product) present")
    check("exothermic" in out,               "exothermic mode present")


# ---------------------------------------------------------------------------
# Test 2: preset 999
# ---------------------------------------------------------------------------
def test_preset_999():
    print("\n  preset = 999")
    out = run_classify(
        "[project]\nname = wo84u_preset999\n\n"
        "[formula]\nformula = CO2\n\n"
        "[chem_plus]\npreset = 999\nvsepr_link = true\n",
        "preset999"
    )
    check("[ChemPlus]" in out,               "preset999: ChemPlus header")
    # count reaction lines (lines starting with "  + ")
    rxn_lines = [l for l in out.splitlines() if l.strip().startswith("+ ")]
    check(len(rxn_lines) >= 3,              f"preset999: >= 3 reactions (got {len(rxn_lines)})")
    check("combustion" in out,               "preset999: combustion present")


# ---------------------------------------------------------------------------
# Test 3: class_override
# ---------------------------------------------------------------------------
def test_class_override():
    print("\n  class_override = synthesis")
    out = run_classify(
        "[project]\nname = wo84u_override\n\n"
        "[formula]\nformula = NH3\n\n"
        "[chem_plus]\nreaction = N2 + 3H2 -> 2NH3 + 92 kJ\nclass_override = synthesis\n",
        "override"
    )
    check("[ChemPlus]" in out,               "override: ChemPlus header")
    check("synthesis" in out,                "override: synthesis present")


# ---------------------------------------------------------------------------
# Test 4: vsepr_link + H2O product -> AX2E2
# ---------------------------------------------------------------------------
def test_h2o_vsepr():
    print("\n  2H2 + O2 -> 2H2O + 572 kJ  (vsepr_link=true)")
    out = run_classify(
        "[project]\nname = wo84u_h2o\n\n"
        "[formula]\nformula = H2O\n\n"
        "[chem_plus]\nreaction = 2H2 + O2 -> 2H2O + 572 kJ\nvsepr_link = true\n",
        "h2o"
    )
    check("[ChemPlus]" in out,               "H2O: ChemPlus header")
    check("AX2E2" in out,                    "H2O: vsepr_tag AX2E2 present")


# ---------------------------------------------------------------------------
# Test 5: script without [chem_plus] — no ChemPlus block
# ---------------------------------------------------------------------------
def test_no_chemplus():
    print("\n  Script without [chem_plus] — should have no [ChemPlus] block")
    out = run_classify(
        "[project]\nname = wo84u_plain\n\n[formula]\nformula = BF3\n",
        "plain"
    )
    check("[ChemPlus]" not in out,           "no chem_plus: no [ChemPlus] block")
    check("[VSEPR]" in out,                  "no chem_plus: [VSEPR] still present")


# ---------------------------------------------------------------------------
# PNG 3D export — reaction energy bar chart (Day 84+ artifact rule)
# ---------------------------------------------------------------------------
def export_chemplus_3d_png() -> Path:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d import Axes3D  # noqa: F401

    reactions = [
        ("CH4+2O2->CO2+2H2O", 891.0, "combustion"),
        ("2H2+O2->2H2O",       572.0, "combustion"),
        ("C+O2->CO2",          394.0, "combustion"),
        ("N2+3H2->2NH3",        92.0, "general"),
        ("SO3+H2O->H2SO4",     130.0, "general"),
    ]
    colors = {"combustion": "#ff6633", "general": "#aaaaaa"}

    fig = plt.figure(figsize=(10, 6), facecolor="#0d0d0d")
    ax  = fig.add_subplot(111, projection="3d", facecolor="#0d0d0d")

    for i, (rxn, kj, cls) in enumerate(reactions):
        col = colors.get(cls, "#888888")
        ax.bar3d(i * 1.4, 0, 0, 0.9, 0.5, kj / 1000, color=col, alpha=0.88)
        ax.text(i * 1.4 + 0.45, 0.55, kj / 1000 + 0.02,
                f"{kj:.0f} kJ", color="white", fontsize=6.5, ha="center")

    ax.set_xticks([i * 1.4 + 0.45 for i in range(len(reactions))])
    ax.set_xticklabels(
        [r[0][:12] for r in reactions], fontsize=6, color="white", rotation=12
    )
    ax.set_zlabel("Energy (MJ)", color="white", fontsize=8)
    ax.tick_params(colors="white")
    ax.xaxis.pane.fill = False
    ax.yaxis.pane.fill = False
    ax.zaxis.pane.fill = False
    for spine in [ax.xaxis, ax.yaxis, ax.zaxis]:
        spine.line.set_color("#444444")

    ax.set_title("WO-84U  ChemPlus CLI  Energy Landscape", color="white",
                 fontsize=11, pad=14)
    ax.view_init(elev=28, azim=-55)

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    out_path = OUT_DIR / "chemplus_cli_energy_3d.png"
    plt.savefig(out_path, dpi=130, bbox_inches="tight",
                facecolor="#0d0d0d", edgecolor="none")
    plt.close(fig)
    print(f"  [PNG] saved -> {out_path}")
    return out_path


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
def main() -> int:
    print("WO-84U: ChemPlus CLI classify integration")
    print("=" * 52)

    if not VSEPR.exists():
        print(f"  FAIL: vsepr.exe not found at {VSEPR}")
        return 1

    test_no_chemplus()
    test_combustion()
    test_preset_999()
    test_class_override()
    test_h2o_vsepr()

    # PNG 3D export
    print("\n  Generating PNG 3D energy landscape...")
    try:
        png = export_chemplus_3d_png()
        check(png.exists(), f"PNG 3D export confirmed -> {png.name}")
    except Exception as exc:
        check(False, f"PNG export failed: {exc}")

    print(f"\n  confirmed: {OUT_DIR / 'chemplus_cli_energy_3d.png'}")
    print(f"\n  Result: {_PASS} PASS / {_FAIL} FAIL")
    if _FAIL == 0:
        print("  PASS: all WO-84U ChemPlus CLI classify checks satisfied\n")
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
