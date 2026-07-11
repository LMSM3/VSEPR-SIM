#!/usr/bin/env python3
"""
latex_markup_demo.py — Helper demo for the VSEPR-SIM revision markup system
============================================================================

Demonstrates every public API in pykernel/latex_markup.py:
  - Inline commands: vsadd, vsdel, vsedit, vsfig, vschart
  - Margin notes:    vsaddnote, vsdelnote, vseditnote
  - Block envs:      vsadd_block, vsdel_block, vsedit_block, vsfig_block, vschart_block
  - Diff classifier: markup_diff_line
  - Preamble inject: inject_markup_preamble
  - Batch inject:    inject_all_in_dir

Run from the workspace root:
    python scripts/demos/latex_markup_demo.py

Output is written to out/latex_markup_demo/ and printed to stdout.

VSEPR-SIM 3.0.0 — permanent infrastructure.
"""

from __future__ import annotations

import importlib.util
import sys
import shutil
from pathlib import Path

# ── load pykernel/latex_markup.py directly (bypasses __init__.py / VisPy) ──
_ROOT = Path(__file__).resolve().parents[2]

def _load(rel: str):
    path = _ROOT / rel
    spec = importlib.util.spec_from_file_location(path.stem, path)
    mod  = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

lm = _load("pykernel/latex_markup.py")

# ─────────────────────────────────────────────────────────────────────────────
# Output directory
# ─────────────────────────────────────────────────────────────────────────────
OUT = _ROOT / "out" / "latex_markup_demo"
OUT.mkdir(parents=True, exist_ok=True)

RULE = "─" * 68

def section(title: str) -> None:
    print(f"\n{RULE}")
    print(f"  {title}")
    print(RULE)

# ─────────────────────────────────────────────────────────────────────────────
# 1. Colour constants
# ─────────────────────────────────────────────────────────────────────────────
section("1. Hardcoded colour constants")

colours = {
    "COLOUR_GREEN":    lm.COLOUR_GREEN,
    "COLOUR_RED":      lm.COLOUR_RED,
    "COLOUR_YELLOW":   lm.COLOUR_YELLOW,
    "COLOUR_YELLOWBG": lm.COLOUR_YELLOWBG,
    "COLOUR_CYAN":     lm.COLOUR_CYAN,
    "COLOUR_DARKRED":  lm.COLOUR_DARKRED,
}
for name, val in colours.items():
    print(f"  {name:<18} = #{val}")

# ─────────────────────────────────────────────────────────────────────────────
# 2. Inline markup commands
# ─────────────────────────────────────────────────────────────────────────────
section("2. Inline markup commands")

inline_examples = [
    ("vsadd",   lm.vsadd("238 kJ/mol")),
    ("vsdel",   lm.vsdel("220 kJ/mol")),
    ("vsedit",  lm.vsedit("C = 20 (revised from 19)")),
    ("vsfig",   lm.vsfig("Figure 3 — Cp(T) curves")),
    ("vschart", lm.vschart("Chart 2 — stochastic Cp spread")),
]
for name, result in inline_examples:
    print(f"  {name:<10}  →  {result}")

# ─────────────────────────────────────────────────────────────────────────────
# 3. Margin note commands
# ─────────────────────────────────────────────────────────────────────────────
section("3. Margin note commands")

note_examples = [
    ("vsaddnote",  lm.vsaddnote("ref updated")),
    ("vsdelnote",  lm.vsdelnote("fig moved")),
    ("vseditnote", lm.vseditnote("LM const")),
]
for name, result in note_examples:
    print(f"  {name:<14}  →  {result}")

# ─────────────────────────────────────────────────────────────────────────────
# 4. Block-level environments
# ─────────────────────────────────────────────────────────────────────────────
section("4. Block-level environments")

block_examples = [
    ("vsadd_block",   lm.vsadd_block(
        "Added in revision 2: cylindrical heat-conduction wall flux term.")),
    ("vsdel_block",   lm.vsdel_block(
        "Removed: arithmetic mean ROM for thermal conductivity.")),
    ("vsedit_block",  lm.vsedit_block(
        "Revised: MC sample count increased from n=100 to n=200.")),
    ("vsfig_block",   lm.vsfig_block(
        "Figure 5 — T(r) radial profile for Hastelloy-N tube at steady state.")),
    ("vschart_block", lm.vschart_block(
        "Chart 3 — Stochastic Cp(T) spread, 200 samples, 3-sigma composition.")),
]
for name, result in block_examples:
    print(f"  [{name}]")
    for line in result.splitlines():
        print(f"    {line}")
    print()

# ─────────────────────────────────────────────────────────────────────────────
# 5. Diff-line classifier
# ─────────────────────────────────────────────────────────────────────────────
section("5. markup_diff_line() — diff-line classifier")

diff_lines = [
    "+ New sentence added in this revision.",
    "- Old sentence that was removed.",
    "~ Sentence that was modified.",
    "[Fig] Figure 3 caption was updated.",
    "[Chart] Chart 2 axis labels corrected.",
    "Unchanged line — passed through as-is.",
]
for raw in diff_lines:
    out = lm.markup_diff_line(raw)
    print(f"  IN : {raw!r}")
    print(f"  OUT: {out!r}")
    print()

# ─────────────────────────────────────────────────────────────────────────────
# 6. Sentence-level revision example (realistic usage)
# ─────────────────────────────────────────────────────────────────────────────
section("6. Realistic sentence-level usage")

sentence = (
    f"The activation energy of Mo dissolution in FLiBe was "
    f"{lm.vsdel('220 kJ/mol')} {lm.vsadd('238 kJ/mol')} "
    f"after re-fitting to the updated ORNL-4254 dataset. "
    f"See {lm.vsfig('Figure~3')} and {lm.vschart('Chart~2')} for details."
)
print(f"  {sentence}")

# ─────────────────────────────────────────────────────────────────────────────
# 7. Preamble injection — write a minimal .tex and inject into it
# ─────────────────────────────────────────────────────────────────────────────
section("7. inject_markup_preamble() — file injection demo")

# Write a minimal bare .tex file without the preamble
bare_tex = OUT / "bare_doc.tex"
bare_tex.write_text(
    r"""\documentclass[12pt]{article}

\begin{document}

The activation energy was \vsdel{220 kJ/mol} \vsadd{238 kJ/mol}.

\end{document}
""",
    encoding="utf-8",
)

# Inject preamble → new file
injected_tex = OUT / "injected_doc.tex"
lm.inject_markup_preamble(str(bare_tex), str(injected_tex))

# Verify marker is present
content = injected_tex.read_text(encoding="utf-8")
marker_present = lm.MARKUP_MARKER in content
print(f"  bare_doc.tex written to    : {bare_tex}")
print(f"  inject_markup_preamble()   → {injected_tex}")
print(f"  Marker present in output   : {marker_present}")

# Idempotency: inject again, line count must not grow
lm.inject_markup_preamble(str(injected_tex), str(injected_tex))
content2 = injected_tex.read_text(encoding="utf-8")
marker_count = content2.count(lm.MARKUP_MARKER)
print(f"  After second injection     : marker appears {marker_count}x (expect 1 — idempotent)")

# ─────────────────────────────────────────────────────────────────────────────
# 8. Batch injection via inject_all_in_dir
# ─────────────────────────────────────────────────────────────────────────────
section("8. inject_all_in_dir() — batch injection demo")

batch_dir = OUT / "batch_tex"
batch_dir.mkdir(exist_ok=True)

sample_bodies = [
    ("sample_a.tex", "The Cp value was \\vsdel{29.1} \\vsadd{29.5} J/mol/K."),
    ("sample_b.tex", "See \\vsfig{Figure 4} for the Debye curve comparison."),
    ("sample_c.tex", "\\vschart{Chart 1} shows the stochastic spread."),
]
for fname, body in sample_bodies:
    (batch_dir / fname).write_text(
        f"\\documentclass{{article}}\n\\begin{{document}}\n{body}\n\\end{{document}}\n",
        encoding="utf-8",
    )

lm.inject_all_in_dir(str(batch_dir))

injected = [f for f in batch_dir.iterdir() if f.suffix == ".tex"]
for tex in sorted(injected):
    has_marker = lm.MARKUP_MARKER in tex.read_text(encoding="utf-8")
    print(f"  {tex.name:<20} — marker present: {has_marker}")

# ─────────────────────────────────────────────────────────────────────────────
# 9. Write MARKUP_PREAMBLE to a standalone file for reference
# ─────────────────────────────────────────────────────────────────────────────
section("9. MARKUP_PREAMBLE — write standalone preamble reference")

preamble_out = OUT / "vssim_markup_preamble.tex"
preamble_out.write_text(lm.MARKUP_PREAMBLE, encoding="utf-8")
print(f"  Preamble written to: {preamble_out}")
print(f"  Lines             : {len(lm.MARKUP_PREAMBLE.splitlines())}")

# ─────────────────────────────────────────────────────────────────────────────
# Done
# ─────────────────────────────────────────────────────────────────────────────
section("Demo complete")
print(f"  All output written to: {OUT}")
print()
