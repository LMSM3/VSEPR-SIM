#!/usr/bin/env bash
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# latex_markup_demo.sh â€” helper demo for inject_latex_markup.sh
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
#
# Demonstrates every mode of scripts/inject_latex_markup.sh:
#   1. Print colour reference table
#   2. Print all inline command examples
#   3. Inject into a single file (in-place)
#   4. Inject into a single file (to new output file)
#   5. Batch inject into a directory with --all
#   6. Idempotency check (second injection must not duplicate the block)
#
# Run from workspace root:
#   bash scripts/demos/latex_markup_demo.sh
#
# Output files are written to out/latex_markup_demo_sh/
#
# VSEPR-SIM 3.0.0 â€” permanent infrastructure.
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
INJECT="$ROOT/scripts/inject_latex_markup.sh"
OUT="$ROOT/out/latex_markup_demo_sh"

mkdir -p "$OUT"

RULE="$(printf 'â”€%.0s' {1..68})"

section() { echo; echo "$RULE"; echo "  $1"; echo "$RULE"; }

# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# 1. Colour reference table
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
section "1. Hardcoded colour reference"

cat <<'TABLE'
  Command       Colour      Hex       Meaning
  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€    â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€  â”€â”€â”€â”€â”€â”€â”€â”€  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  \vsadd        Green       #27AE60   Addition / new text
  \vsdel        Red         #E74C3C   Deletion / removed text
  \vsedit       Yellow      #F1C40F   Edit / modified text
  \vsfig        Cyan        #1ABC9C   Figure reference
  \vschart      Dark Red    #C0392B   Chart reference
TABLE

# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# 2. Inline command examples (LaTeX strings)
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
section "2. Inline markup command examples"

echo "  \\vsadd{238 kJ/mol}                          â†’ addition in green"
echo "  \\vsdel{220 kJ/mol}                          â†’ deletion in red strikethrough"
echo "  \\vsedit{C = 20 (revised from 19)}           â†’ edit in yellow highlight"
echo "  \\vsfig{Figure 3 --- Cp(T) curves}           â†’ figure ref in cyan"
echo "  \\vschart{Chart 2 --- stochastic Cp spread}  â†’ chart ref in dark red"

echo
echo "  Margin notes:"
echo "  \\vsaddnote{ref updated}"
echo "  \\vsdelnote{fig moved}"
echo "  \\vseditnote{LM const}"

echo
echo "  Block environments:"
for env in vsaddblock vsdelblock vseditblock vsfigblock vschartblock; do
    echo "  \\begin{$env} ... \\end{$env}"
done

# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# 3. Single-file injection â€” in-place
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
section "3. Single-file injection (in-place)"

BARE="$OUT/bare_inplace.tex"
cat > "$BARE" <<'TEX'
\documentclass[12pt]{article}

\begin{document}
The activation energy was \vsdel{220 kJ/mol} \vsadd{238 kJ/mol}.
\end{document}
TEX

echo "  Wrote bare file: $BARE"
bash "$INJECT" "$BARE"
echo "  Injected (in-place): $BARE"

if grep -q '%%VSSIM-MARKUP-INJECTED' "$BARE"; then
    echo "  Marker present: YES"
else
    echo "  Marker present: NO (FAIL)"
    exit 1
fi

# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# 4. Single-file injection â€” to output file
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
section "4. Single-file injection (to new output file)"

BARE2="$OUT/bare_src.tex"
DEST="$OUT/injected_out.tex"
cat > "$BARE2" <<'TEX'
\documentclass[12pt]{article}

\begin{document}
See \vsfig{Figure~5} for the radial temperature profile.
\end{document}
TEX

bash "$INJECT" "$BARE2" "$DEST"
echo "  Source : $BARE2"
echo "  Output : $DEST"

if grep -q '%%VSSIM-MARKUP-INJECTED' "$DEST"; then
    echo "  Marker present: YES"
else
    echo "  Marker present: NO (FAIL)"
    exit 1
fi

# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# 5. Batch injection via --all
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
section "5. Batch injection with --all"

BATCH="$OUT/batch_tex"
mkdir -p "$BATCH"

cat > "$BATCH/sample_a.tex" <<'TEX'
\documentclass{article}
\begin{document}
The Cp value was \vsdel{29.1} \vsadd{29.5} J/mol/K.
\end{document}
TEX

cat > "$BATCH/sample_b.tex" <<'TEX'
\documentclass{article}
\begin{document}
See \vsfig{Figure 4} for the Debye curve comparison.
\end{document}
TEX

cat > "$BATCH/sample_c.tex" <<'TEX'
\documentclass{article}
\begin{document}
\vschart{Chart 1} shows the stochastic Cp spread.
\end{document}
TEX

bash "$INJECT" --all "$BATCH"
echo "  Batch dir: $BATCH"
echo

for f in "$BATCH"/*.tex; do
    name="$(basename "$f")"
    if grep -q '%%VSSIM-MARKUP-INJECTED' "$f"; then
        echo "  $name  â€” marker present: YES"
    else
        echo "  $name  â€” marker present: NO (FAIL)"
        exit 1
    fi
done

# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# 6. Idempotency check
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
section "6. Idempotency â€” second injection must not duplicate block"

bash "$INJECT" "$BARE"   # inject again into already-injected file
COUNT=$(grep -c '%%VSSIM-MARKUP-INJECTED' "$BARE" || true)
echo "  Marker count after second injection: $COUNT (expect 1)"

if [ "$COUNT" -eq 1 ]; then
    echo "  Idempotency: PASS"
else
    echo "  Idempotency: FAIL â€” marker duplicated"
    exit 1
fi

# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# Done
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
section "Demo complete"
echo "  All output written to: $OUT"
echo
