"""
latex_markup.py — Permanent hardcoded LaTeX revision-markup colour system
==========================================================================

Provides the fixed, non-negotiable VSEPR-SIM colour scheme for LaTeX edits:

    GREEN   (#27AE60) — additions / new text
    RED     (#E74C3C) — deletions / removed text
    YELLOW  (#F1C40F) — edits / modified text
    CYAN    (#1ABC9C) — figure references and figure environments
    DARKRED (#C0392B) — chart references and chart environments

These colours are PERMANENT.  This module is the Python authority.
The identical definitions exist in:
    include/chart/latex_markup.h   (C header)
    scripts/inject_latex_markup.sh (shell helper)

Usage:
    from pykernel.latex_markup import (
        vsadd, vsdel, vsedit, vsfig, vschart,
        vsadd_block, vsdel_block, vsedit_block, vsfig_block, vschart_block,
        inject_markup_preamble, MARKUP_PREAMBLE,
    )

    # Inline
    line = f"The value was {vsdel('50 kJ')} {vsadd('52.3 kJ')} after recalibration."

    # Block
    block = vsadd_block("This entire paragraph was added in revision 2.")

    # Preamble injection
    inject_markup_preamble("report.tex")               # in-place
    inject_markup_preamble("report.tex", "out.tex")     # to new file

VSEPR-SIM 3.0.0 — permanent infrastructure.  DO NOT modify colours.
"""

from __future__ import annotations

import re
from pathlib import Path
from typing import Optional

# ============================================================================
# Hardcoded colour hex values — DO NOT CHANGE
# ============================================================================

COLOUR_GREEN    = "27AE60"   # additions
COLOUR_RED      = "E74C3C"   # deletions
COLOUR_YELLOW   = "F1C40F"   # edits
COLOUR_YELLOWBG = "FEF9E7"   # edit background
COLOUR_CYAN     = "1ABC9C"   # figures
COLOUR_DARKRED  = "C0392B"   # charts

# ============================================================================
# LaTeX preamble block — identical to C header and shell script
# ============================================================================

MARKUP_PREAMBLE = r"""
%%VSSIM-MARKUP-INJECTED
%% ═══════════════════════════════════════════════════════════════════
%% VSEPR-SIM Revision Markup — permanent colour definitions
%% DO NOT EDIT these definitions.  They are hardcoded project-wide.
%% ═══════════════════════════════════════════════════════════════════
\usepackage{xcolor}
\usepackage{soul}
\usepackage[normalem]{ulem}
\usepackage{tcolorbox}
\tcbuselibrary{skins,breakable}

%% ── Hardcoded colours ──────────────────────────────────────────────
\definecolor{vsgreen}{HTML}{27AE60}     %% additions
\definecolor{vsred}{HTML}{E74C3C}       %% deletions
\definecolor{vsyellow}{HTML}{F1C40F}    %% edits
\definecolor{vsyellowbg}{HTML}{FEF9E7}  %% edit background
\definecolor{vscyan}{HTML}{1ABC9C}      %% figures
\definecolor{vsdarkred}{HTML}{C0392B}   %% charts

%% ── Inline markup commands ────────────────────────────────────────
\newcommand{\vsadd}[1]{\textcolor{vsgreen}{\textbf{+}~#1}}
\newcommand{\vsdel}[1]{\textcolor{vsred}{\sout{#1}}}
\newcommand{\vsedit}[1]{\sethlcolor{vsyellowbg}\hl{\textcolor{black}{#1}}}
\newcommand{\vsfig}[1]{\textcolor{vscyan}{\textit{[Fig]~#1}}}
\newcommand{\vschart}[1]{\textcolor{vsdarkred}{\textit{[Chart]~#1}}}

%% ── Margin annotations ────────────────────────────────────────────
\newcommand{\vsaddnote}[1]{\marginpar{\tiny\textcolor{vsgreen}{+#1}}}
\newcommand{\vsdelnote}[1]{\marginpar{\tiny\textcolor{vsred}{-#1}}}
\newcommand{\vseditnote}[1]{\marginpar{\tiny\textcolor{vsyellow}{\textrm{#1}}}}

%% ── Block-level environments (tcolorbox) ──────────────────────────
\newtcolorbox{vsaddblock}{%
  colback=vsgreen!8, colframe=vsgreen, title=\textbf{Addition},
  fonttitle=\small, breakable, left=4pt, right=4pt, top=2pt, bottom=2pt}
\newtcolorbox{vsdelblock}{%
  colback=vsred!8, colframe=vsred, title=\textbf{Deletion},
  fonttitle=\small, breakable, left=4pt, right=4pt, top=2pt, bottom=2pt}
\newtcolorbox{vseditblock}{%
  colback=vsyellowbg, colframe=vsyellow, title=\textbf{Edit},
  fonttitle=\small, breakable, left=4pt, right=4pt, top=2pt, bottom=2pt}
\newtcolorbox{vsfigblock}{%
  colback=vscyan!8, colframe=vscyan, title=\textbf{Figure},
  fonttitle=\small, breakable, left=4pt, right=4pt, top=2pt, bottom=2pt}
\newtcolorbox{vschartblock}{%
  colback=vsdarkred!8, colframe=vsdarkred, title=\textbf{Chart},
  fonttitle=\small, breakable, left=4pt, right=4pt, top=2pt, bottom=2pt}
%% ═══════════════════════════════════════════════════════════════════
%% END VSEPR-SIM Revision Markup
%% ═══════════════════════════════════════════════════════════════════
""".lstrip("\n")

MARKUP_MARKER = "%%VSSIM-MARKUP-INJECTED"


# ============================================================================
# Inline markup functions — return LaTeX strings
# ============================================================================

def vsadd(text: str) -> str:
    r"""Green addition: \vsadd{text}"""
    return rf"\vsadd{{{text}}}"

def vsdel(text: str) -> str:
    r"""Red deletion with strikethrough: \vsdel{text}"""
    return rf"\vsdel{{{text}}}"

def vsedit(text: str) -> str:
    r"""Yellow-highlighted edit: \vsedit{text}"""
    return rf"\vsedit{{{text}}}"

def vsfig(text: str) -> str:
    r"""Cyan figure reference: \vsfig{text}"""
    return rf"\vsfig{{{text}}}"

def vschart(text: str) -> str:
    r"""Dark-red chart reference: \vschart{text}"""
    return rf"\vschart{{{text}}}"


# ============================================================================
# Margin note functions
# ============================================================================

def vsaddnote(text: str) -> str:
    return rf"\vsaddnote{{{text}}}"

def vsdelnote(text: str) -> str:
    return rf"\vsdelnote{{{text}}}"

def vseditnote(text: str) -> str:
    return rf"\vseditnote{{{text}}}"


# ============================================================================
# Block-level environments — return LaTeX environment strings
# ============================================================================

def vsadd_block(body: str) -> str:
    return f"\\begin{{vsaddblock}}\n{body}\n\\end{{vsaddblock}}\n"

def vsdel_block(body: str) -> str:
    return f"\\begin{{vsdelblock}}\n{body}\n\\end{{vsdelblock}}\n"

def vsedit_block(body: str) -> str:
    return f"\\begin{{vseditblock}}\n{body}\n\\end{{vseditblock}}\n"

def vsfig_block(body: str) -> str:
    return f"\\begin{{vsfigblock}}\n{body}\n\\end{{vsfigblock}}\n"

def vschart_block(body: str) -> str:
    return f"\\begin{{vschartblock}}\n{body}\n\\end{{vschartblock}}\n"


# ============================================================================
# Diff-line classifier — given a line from a text diff, wrap appropriately
# ============================================================================

def markup_diff_line(line: str) -> str:
    """
    Classify a diff-like line and wrap in the correct markup:
        + ...   → vsadd
        - ...   → vsdel
        ~ ...   → vsedit
        [Fig]   → vsfig
        [Chart] → vschart
        else    → unmodified
    """
    stripped = line.strip()
    if stripped.startswith("+"):
        return vsadd(stripped[1:].strip())
    elif stripped.startswith("-"):
        return vsdel(stripped[1:].strip())
    elif stripped.startswith("~"):
        return vsedit(stripped[1:].strip())
    elif stripped.lower().startswith("[fig]"):
        return vsfig(stripped[5:].strip())
    elif stripped.lower().startswith("[chart]"):
        return vschart(stripped[7:].strip())
    return stripped


# ============================================================================
# Preamble injection — idempotent
# ============================================================================

def inject_markup_preamble(src: str | Path,
                            dst: Optional[str | Path] = None) -> bool:
    """
    Inject the VSEPR-SIM revision-markup preamble into a .tex file.

    Inserts after the first ``\\documentclass`` line.  If the marker
    already exists in the file, no change is made (idempotent).

    Args:
        src: source .tex file path
        dst: destination path (default: overwrite src in-place)

    Returns:
        True if injection occurred, False if already present.
    """
    src = Path(src)
    if dst is None:
        dst = src
    else:
        dst = Path(dst)

    content = src.read_text(encoding="utf-8")

    if MARKUP_MARKER in content:
        if dst != src:
            dst.write_text(content, encoding="utf-8")
        return False

    # Find first \documentclass line
    match = re.search(r"(\\documentclass[^\n]*\n)", content)
    if match:
        insert_pos = match.end()
        out = content[:insert_pos] + "\n" + MARKUP_PREAMBLE + content[insert_pos:]
    else:
        out = MARKUP_PREAMBLE + content

    dst.write_text(out, encoding="utf-8")
    return True


def inject_all_in_dir(directory: str | Path) -> int:
    """Inject markup preamble into all .tex files in a directory tree."""
    directory = Path(directory)
    count = 0
    for tex in sorted(directory.rglob("*.tex")):
        if inject_markup_preamble(tex):
            count += 1
    return count
