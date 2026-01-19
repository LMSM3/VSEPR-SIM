#!/bin/bash
# Example: Using TruthState for reproducibility

cd "$(dirname "$0")"

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  TruthState Demo - Reproducibility Ledger                     ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# Build with truth state tracking
echo "▶ Building water molecule with TruthState..."
./build/bin/vsepr build H2O --optimize --output /tmp/water_truth.xyz

echo ""
echo "═══ Truth State Files Created ═══"
echo ""

if [ -f "/tmp/water_truth.xyz.truth.json" ]; then
    echo "✓ Truth state JSON generated"
    echo ""
    echo "Preview:"
    head -30 /tmp/water_truth.xyz.truth.json
    echo ""
    echo "[... full file at /tmp/water_truth.xyz.truth.json ...]"
else
    echo "✗ Truth state file not found"
fi

echo ""
echo "═══ One-Line Summary (from stdout) ═══"
echo "Look for [TRUTH] line in build output above"
echo ""
echo "Example format:"
echo "[TRUTH] 20260117_123456_7890 | H2O | 3 atoms | 2 bonds | E=-45.123 | iter=234 | conv=YES | health=OK"
echo ""

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║  What TruthState Captures:                                    ║"
echo "║  • run_id (timestamp + hash)                                  ║"
echo "║  • input (formula, flags)                                     ║"
echo "║  • atoms (Z + xyz)                                            ║"
echo "║  • bonds (pairs + order + reason)                             ║"
echo "║  • local_geom (sp/sp2/sp3 hybridization)                      ║"
echo "║  • global_shape_candidates (ranked hypotheses)                ║"
echo "║  • energy (total + components)                                ║"
echo "║  • convergence (iter, rmsF, maxF)                             ║"
echo "║  • health (nan, exploded, warnings)                           ║"
echo "╚════════════════════════════════════════════════════════════════╝"
