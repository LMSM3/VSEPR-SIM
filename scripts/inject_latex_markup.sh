#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════
# inject_latex_markup.sh — VSEPR-SIM permanent LaTeX revision markup
# ═══════════════════════════════════════════════════════════════════════
#
# Injects the hardcoded VSEPR-SIM revision-markup colour preamble into
# a LaTeX .tex file.  Idempotent — safe to run multiple times.
#
#   GREEN   (#27AE60) — additions
#   RED     (#E74C3C) — deletions
#   YELLOW  (#F1C40F) — edits
#   CYAN    (#1ABC9C) — figures
#   DARKRED (#C0392B) — charts
#
# Usage:
#   ./inject_latex_markup.sh  file.tex              # in-place
#   ./inject_latex_markup.sh  file.tex  output.tex  # to new file
#   ./inject_latex_markup.sh  --all docs/            # all .tex in dir
#
# VSEPR-SIM 3.0.0 — permanent infrastructure.  DO NOT modify colours.
# ═══════════════════════════════════════════════════════════════════════
set -eu

MARKER="%%VSSIM-MARKUP-INJECTED"

# The preamble block — hardcoded, permanent
read -r -d '' PREAMBLE << 'PREAMBLE_EOF' || true
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
PREAMBLE_EOF

# ── Functions ──────────────────────────────────────────────────────────

inject_file() {
    local src="$1"
    local dst="${2:-$1}"

    if grep -q "$MARKER" "$src" 2>/dev/null; then
        echo "  [skip] $src — markup already present"
        if [[ "$src" != "$dst" ]]; then
            cp "$src" "$dst"
        fi
        return 0
    fi

    local tmp
    tmp=$(mktemp)

    # Find line number of first \documentclass
    local line_num
    line_num=$(grep -n '\\documentclass' "$src" | head -1 | cut -d: -f1)

    if [[ -z "$line_num" ]]; then
        # No \documentclass — prepend
        printf '%s\n' "$PREAMBLE" > "$tmp"
        cat "$src" >> "$tmp"
    else
        # Insert after \documentclass line
        head -n "$line_num" "$src" > "$tmp"
        printf '\n%s\n' "$PREAMBLE" >> "$tmp"
        tail -n +"$((line_num + 1))" "$src" >> "$tmp"
    fi

    if [[ "$src" == "$dst" ]]; then
        mv "$tmp" "$src"
    else
        mv "$tmp" "$dst"
    fi
    echo "  [done] $dst — markup injected"
}

# ── Main ───────────────────────────────────────────────────────────────

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <file.tex> [output.tex]"
    echo "       $0 --all <directory>"
    exit 1
fi

if [[ "$1" == "--all" ]]; then
    dir="${2:-.}"
    echo "Injecting markup into all .tex files in $dir"
    find "$dir" -name '*.tex' -type f | while read -r f; do
        inject_file "$f"
    done
    echo "Done."
else
    inject_file "$1" "${2:-$1}"
fi
