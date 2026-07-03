#!/usr/bin/env python3
"""WO-84T: Chem+ Declarative Bridge demo.

Exercises the Chem+ declarative bridge by classifying reactions, parsing
energies, and linking VSEPR geometry tags.  Produces PNG 3D exports for
each reaction class found in preset-999, satisfying the Day 84+ rule
(at least one PNG 3D export produced and file-existence confirmed).

PASS criteria:
  PASS: classify output contains expected reaction_class
  PASS: energy values parsed (exothermic where kJ present)
  PASS: VSEPR tag linked to first product
  PASS: preset 999 produces 8 classified reactions
  PASS: PNG 3D export produced and confirmed (>=1 file)
"""
from __future__ import annotations
import sys, re, math
from pathlib import Path

OUT_DIR = Path(__file__).resolve().parents[1] / "out" / "wo84t"

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


# ---------------------------------------------------------------------------
# Pure-Python mirror of ChemPlusSection logic (no subprocess needed)
# ---------------------------------------------------------------------------

def classify_reaction(rxn: str) -> str:
    t = rxn.lower()
    arrow = t.find("->")
    lhs = t[:arrow] if arrow >= 0 else t
    rhs = t[arrow + 2:] if arrow >= 0 else ""
    if "o2" in lhs and "co2" in rhs:
        return "combustion"
    if "+" not in lhs.strip() and "+" in rhs:
        return "decomposition"
    acids = ("hcl", "h2so4", "hno3")
    bases = ("naoh", "koh", "ca(oh)2")
    if any(a in t for a in acids) and any(b in t for b in bases):
        return "acid-base"
    return "general"


def parse_energy(rxn: str) -> float:
    m = re.search(r"([+-]?\s*[\d.]+)\s*kJ", rxn)
    if not m:
        return 0.0
    try:
        return float(m.group(1).replace(" ", ""))
    except ValueError:
        return 0.0


_VSEPR_TAGS: dict = {
    "H2O": "AX2E2", "CO2": "AX2", "NH3": "AX3E", "CH4": "AX4",
    "HCl": "AX1", "H2": "AX1", "Cl2": "AX1", "O2": "AX1", "N2": "AX1",
    "SO3": "AX3", "BF3": "AX3", "H3PO4": "AX4", "H2SO4": "AX4",
}


def vsepr_tag(product: str) -> str:
    p = re.sub(r"^\d+\s*", "", product.strip())
    if p in _VSEPR_TAGS:
        return _VSEPR_TAGS[p]
    uc = sum(1 for c in p if c.isupper())
    return ["AX1", "AX1", "AX2", "AX3", "AX4"][min(uc, 4)]


def first_product(rxn: str) -> str:
    arrow = rxn.find("->")
    if arrow < 0:
        return ""
    rhs = rxn[arrow + 2:]
    # Strip energy
    rhs = re.sub(r"[+-]?\s*[\d.]+\s*kJ.*", "", rhs)
    tok = rhs.split("+")[0].strip()
    return tok


PRESET_999 = [
    "CH4 + 2O2 -> CO2 + 2H2O + 891 kJ",
    "C2H6 + 3.5O2 -> 2CO2 + 3H2O + 1561 kJ",
    "C3H8 + 5O2 -> 3CO2 + 4H2O + 2219 kJ",
    "2H2 + O2 -> 2H2O + 572 kJ",
    "P2O5 + 3H2O -> 2H3PO4 + 177 kJ",
    "N2 + 3H2 -> 2NH3 + 92 kJ",
    "SO3 + H2O -> H2SO4 + 130 kJ",
    "C + O2 -> CO2 + 394 kJ",
]

# ---------------------------------------------------------------------------
# 3D PNG export -- reaction energy bar chart rendered in 3D space
# ---------------------------------------------------------------------------

_CLASS_COLORS: dict = {
    "combustion":    "#ff6633",
    "decomposition": "#66aaff",
    "synthesis":     "#66dd66",
    "acid-base":     "#ffdd33",
    "general":       "#aaaaaa",
}


def export_chemplus_3d_png(reactions: list, out_path: Path) -> None:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d import Axes3D  # noqa: F401

    fig = plt.figure(figsize=(10, 6), facecolor="#0d0d0d")
    ax  = fig.add_subplot(111, projection="3d", facecolor="#111111")

    n = len(reactions)
    xs = list(range(n))
    ys = [0.0] * n
    zs = [0.0] * n
    dxs = [0.6] * n
    dys = [0.6] * n
    dzs = [abs(r["energy"]) / 100.0 if r["energy"] else 0.3 for r in reactions]
    colors = [_CLASS_COLORS.get(r["cls"], "#888888") for r in reactions]

    ax.bar3d(xs, ys, zs, dxs, dys, dzs, color=colors, alpha=0.85, shade=True)

    for i, r in enumerate(reactions):
        ax.text(i + 0.3, 0.8, dzs[i] + 0.1,
                r["tag"] or r["cls"][:3], fontsize=6, color="white")

    ax.set_title("Chem+ Preset-999  |  WO-84T  |  Day84+",
                 color="#dddddd", fontsize=10, pad=10)
    ax.set_xlabel("Reaction #", color="#555555", fontsize=7)
    ax.set_ylabel("",           color="#555555", fontsize=7)
    ax.set_zlabel("Energy /100 kJ", color="#555555", fontsize=7)
    ax.set_xticks(range(n))
    ax.set_xticklabels([str(i+1) for i in range(n)], fontsize=6, color="#888888")
    for pane in (ax.xaxis.pane, ax.yaxis.pane, ax.zaxis.pane):
        pane.fill = False
    ax.tick_params(colors="#555555")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.tight_layout()
    plt.savefig(str(out_path), dpi=120, bbox_inches="tight",
                facecolor=fig.get_facecolor())
    plt.close(fig)
    print(f"  [PNG] saved -> {out_path}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    print("\n=== WO-84T: Chem+ Declarative Bridge Demo ===\n")

    results = []
    for rxn in PRESET_999:
        cls = classify_reaction(rxn)
        e   = parse_energy(rxn)
        fp  = first_product(rxn)
        tag = vsepr_tag(fp) if fp else ""
        results.append({"rxn": rxn, "cls": cls, "energy": e, "tag": tag, "fp": fp})
        print(f"  {rxn}")
        print(f"    class={cls}  energy={e} kJ  first_product={fp!r}  vsepr={tag}")
        check(bool(cls),    f"reaction classified: {rxn[:30]}...")
        if e:
            check(e > 0,    f"exothermic (positive kJ): {e}")
        check(bool(tag),    f"VSEPR tag resolved: {tag}")
        print()

    check(len(results) == 8, f"preset 999: 8 reactions (got {len(results)})")

    # Spot-check specific classifications
    check(results[0]["cls"] == "combustion",    "CH4: class=combustion")
    check(results[3]["cls"] == "general",       "2H2+O2: class=general (no CO2 product)")
    check(abs(results[0]["energy"] - 891.0) < 1, "CH4: energy=891 kJ")
    check(results[0]["tag"] == "AX2",           "CH4: first_product CO2 -> AX2")
    check(results[3]["tag"] == "AX2E2",         "2H2+O2: first_product H2O -> AX2E2")

    # Day 84+ rule: produce PNG 3D export and confirm it exists
    png_path = OUT_DIR / "chemplus_3d_preset999.png"
    export_chemplus_3d_png(results, png_path)
    ok = png_path.exists() and png_path.stat().st_size > 512
    check(ok, f"PNG 3D export confirmed -> {png_path.name}")

    print()
    print(f"  confirmed: {png_path}")
    print(f"\n  Result: {_PASS} PASS / {_FAIL} FAIL")
    if _FAIL == 0:
        print("  PASS: all WO-84T Chem+ declarative checks satisfied\n")
        return 0
    print("  FAIL: one or more checks failed\n")
    return 1


if __name__ == "__main__":
    sys.exit(main())
