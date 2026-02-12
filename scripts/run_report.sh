#!/usr/bin/env bash
################################################################################
# VSEPR-Sim Report Generator
# Builds: Markdown → PDF, LaTeX → PDF, Python Reports
################################################################################

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV="$ROOT/.venv"
OUT="$ROOT/outputs/reports"
SRC="$ROOT/reporting"

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║  VSEPR-Sim Report Generator v2.3.1                            ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

mkdir -p "$OUT"

# Check virtual environment
if [[ ! -d "$VENV" ]]; then
    echo "[!] Missing virtual environment: $VENV"
    echo "[!] Run: ./scripts/install_reporting.sh"
    exit 1
fi

# Activate environment
# shellcheck disable=SC1091
source "$VENV/bin/activate"

echo "[*] Repository: $ROOT"
echo "[*] Source: $SRC"
echo "[*] Output: $OUT"
echo ""

# --- 1) Python-generated report assets ---
if [[ -f "$SRC/generate_report.py" ]]; then
    echo "[*] Running generate_report.py..."
    python "$SRC/generate_report.py" --out "$OUT"
    echo "[✓] Python report generated"
else
    echo "[!] $SRC/generate_report.py not found - skipping"
fi

echo ""

# --- 2) Markdown → PDF ---
if [[ -f "$SRC/report.md" ]]; then
    echo "[*] Building Markdown → PDF..."
    
    pandoc "$SRC/report.md" \
        -o "$OUT/report_md.pdf" \
        --pdf-engine=xelatex \
        --variable geometry:margin=1in \
        --toc \
        --number-sections
    
    echo "[✓] Markdown PDF: $OUT/report_md.pdf"
else
    echo "[!] $SRC/report.md not found - skipping Markdown PDF"
fi

echo ""

# --- 3) LaTeX → PDF ---
if [[ -f "$SRC/report.tex" ]]; then
    echo "[*] Building LaTeX → PDF..."
    
    # Copy to output for latexmk
    cp "$SRC/report.tex" "$OUT/"
    
    cd "$OUT"
    latexmk -pdf -interaction=nonstopmode report.tex >/dev/null 2>&1 || {
        echo "[!] LaTeX compilation failed (see $OUT/report.log)"
    }
    
    # Clean auxiliary files
    latexmk -c >/dev/null 2>&1 || true
    
    cd "$ROOT"
    
    if [[ -f "$OUT/report.pdf" ]]; then
        echo "[✓] LaTeX PDF: $OUT/report.pdf"
    else
        echo "[!] LaTeX PDF generation failed"
    fi
else
    echo "[!] $SRC/report.tex not found - skipping LaTeX PDF"
fi

echo ""

# --- 4) Jupyter notebook conversion (if exists) ---
if [[ -f "$SRC/analysis.ipynb" ]]; then
    echo "[*] Converting Jupyter notebook → HTML..."
    
    jupyter nbconvert \
        --to html \
        --output-dir="$OUT" \
        "$SRC/analysis.ipynb"
    
    echo "[✓] Notebook HTML: $OUT/analysis.html"
    
    echo ""
    echo "[*] Converting Jupyter notebook → PDF..."
    
    jupyter nbconvert \
        --to pdf \
        --output-dir="$OUT" \
        "$SRC/analysis.ipynb" 2>/dev/null || {
        echo "[!] Notebook PDF conversion failed (requires LaTeX)"
    }
else
    echo "[!] $SRC/analysis.ipynb not found - skipping notebook conversion"
fi

echo ""

# --- Summary ---
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║  ✅ Report Generation Complete!                               ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""
echo "Generated files in: $OUT"
echo ""

ls -lh "$OUT" | tail -n +2 | while read -r line; do
    echo "  $line"
done

echo ""
echo "View reports:"
if [[ -f "$OUT/report_md.pdf" ]]; then
    echo "  xdg-open $OUT/report_md.pdf       # Markdown"
fi
if [[ -f "$OUT/report.pdf" ]]; then
    echo "  xdg-open $OUT/report.pdf          # LaTeX"
fi
if [[ -f "$OUT/analysis.html" ]]; then
    echo "  xdg-open $OUT/analysis.html       # Notebook"
fi
echo ""
