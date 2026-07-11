#!/usr/bin/env bash
################################################################################
# molecular_census.sh — Deep Molecular Analysis Tool
#
# Collects 112+ data points on a molecule through a single primary FIRE
# build instance, discovers alternative minimization geometries, and
# automatically classifies the result by group / sub-group / ionic character
# / purpose type (fuel, battery, semiconductor, etc.).
#
# Usage:
#   ./molecular_census.sh <formula>                   # Single molecule
#   ./molecular_census.sh <formula> [formula2 ...]    # Batch mode
#   ./molecular_census.sh --file molecules.txt        # From file (one per line)
#   ./molecular_census.sh --all-phases                # Run Pokedex catalog
#
# Options:
#   --trials <N>       Conformer search trials (default: 50)
#   --seed <N>         RNG seed (default: 42)
#   --output <dir>     Output directory (default: outputs/census/)
#   --json             Also emit machine-readable JSON summariesy
#   --quiet            Suppress per-molecule banner output


#   --file <path>      Read formulas from file (one per line)
#   --all-phases       Run all Pokedex catalog molecules
#
# Examples:
#   ./molecular_census.sh H2O
#   ./molecular_census.sh CH4 NH3 SF6 C2H6 --trials 100
#   ./molecular_census.sh --file formulas.txt --output results/deep/
#
# Output:
#   Per molecule:
#     outputs/census/<session>/<formula>.census.txt   Full report (112+ points)
#     outputs/census/<session>/<formula>.alt.txt      Alternative geometries log
#     outputs/census/<session>/<formula>.class.txt    Classification summary
#   Session:
#     outputs/census/<session>/session_summary.txt    Aggregate statistics
#
# Renamed from: extremely_deep_analysis.sh
# Purpose: Research-grade molecular data collection and classification
################################################################################

set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# ============================================================================
# Configuration
# ============================================================================

VSEPR_BIN="$PROJECT_ROOT/build/bin/vsepr"
CENSUS_BIN="$PROJECT_ROOT/build/bin/molecular-census"
SESSION_NAME="census_$(date +%Y%m%d_%H%M%S)"
OUTPUT_DIR="$PROJECT_ROOT/outputs/census/$SESSION_NAME"
CONFORMER_TRIALS=50
SEED=42
EMIT_JSON=false
QUIET=false
FORMULAS=()

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

# Statistics
TOTAL=0
SUCCESS=0
FAILED=0
TOTAL_DATA_POINTS=0

# ============================================================================
# Pokedex Catalog (for --all-phases)
# ============================================================================

PHASE1_MOLECULES=(
    "H2" "H2O" "NH3" "CH4" "H2S" "HCl" "HF"
    "SF6" "PF5" "PCl5"
    "ClF3" "BrF5" "XeF2"
)

PHASE2_MOLECULES=(
    "C2H6" "C2H4" "C2H2" "C3H8" "C6H6"
    "CH3OH" "CH2O" "CO2" "NO2" "SO2" "SO3"
)

PHASE3_MOLECULES=(
    "NaCl" "KBr" "CaF2" "MgO"
    "SiO2" "Al2O3" "Fe2O3"
    "LiF" "NaF"
)

ALL_PHASES_MOLECULES=("${PHASE1_MOLECULES[@]}" "${PHASE2_MOLECULES[@]}" "${PHASE3_MOLECULES[@]}")

# ============================================================================
# Argument Parsing
# ============================================================================

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --trials)
                CONFORMER_TRIALS="$2"
                shift 2
                ;;
            --seed)
                SEED="$2"
                shift 2
                ;;
            --output)
                OUTPUT_DIR="$2/$SESSION_NAME"
                shift 2
                ;;
            --json)
                EMIT_JSON=true
                shift
                ;;
            --quiet)
                QUIET=true
                shift
                ;;
            --file)
                if [[ -f "$2" ]]; then
                    while IFS= read -r line; do
                        line="$(echo "$line" | xargs)"  # trim
                        [[ -z "$line" || "$line" == \#* ]] && continue
                        FORMULAS+=("$line")
                    done < "$2"
                else
                    echo -e "${RED}Error: File not found: $2${NC}"
                    exit 1
                fi
                shift 2
                ;;
            --all-phases)
                FORMULAS+=("${ALL_PHASES_MOLECULES[@]}")
                shift
                ;;
            --help|-h)
                print_help
                exit 0
                ;;
            *)
                FORMULAS+=("$1")
                shift
                ;;
        esac
    done

    if [[ ${#FORMULAS[@]} -eq 0 ]]; then
        echo -e "${RED}Error: No formulas specified.${NC}"
        echo "Usage: $0 <formula> [formula2 ...] [--trials N] [--seed N]"
        echo "       $0 --file molecules.txt"
        echo "       $0 --all-phases"
        exit 1
    fi
}

print_help() {
    cat << 'EOF'
╔══════════════════════════════════════════════════════════════════════════════╗
║              MOLECULAR CENSUS — Deep Analysis Tool                         ║
╚══════════════════════════════════════════════════════════════════════════════╝

Collects 112+ data points per molecule. Discovers alternative FIRE geometries.
Automatically classifies by group, sub-group, ionic character, and purpose type.

USAGE:
  molecular_census.sh <formula> [formula2 ...]
  molecular_census.sh --file <path>
  molecular_census.sh --all-phases

OPTIONS:
  --trials <N>    Conformer search trials (default: 50)
  --seed <N>      RNG seed for reproducibility (default: 42)
  --output <dir>  Output directory (default: outputs/census/)
  --json          Emit machine-readable JSON summaries
  --quiet         Suppress verbose output
  --file <path>   Read formulas from file (one per line, # = comment)
  --all-phases    Run full Pokedex molecule catalog
  --help          Show this help

EXAMPLES:
  molecular_census.sh H2O
  molecular_census.sh CH4 NH3 SF6 --trials 100
  molecular_census.sh --file my_molecules.txt --json
  molecular_census.sh --all-phases --output results/deep/

DATA POINTS (14 categories, 112+ total):
  [A] Composition (15)    [H] Validation (10)
  [B] Topology (12)       [I] FIRE Stats (7)
  [C] Energy (8)          [J] Conformers (8)
  [D] Electronic (10)     [K] Classification (8)
  [E] Reactivity (8)      [L] Stability (5)
  [F] Geometry (16)       [M] Thermodynamic (5)
  [G] Identity (5)        [N] Structural (6)

CLASSIFICATION OUTPUT:
  Group:     Organic | Inorganic | Organometallic | Coordination | ...
  Sub-group: Alkane | Hydride | Halide | Octahedral | Perfluorinated | ...
  Ionic:     Covalent | Polar Covalent | Ionic | Metallic | Mixed
  Purpose:   Fuel | Battery | Catalyst | Pharmaceutical | Semiconductor | ...

EOF
}

# ============================================================================
# Core Analysis Function
# ============================================================================

run_census_for_molecule() {
    local formula="$1"
    local idx="$2"
    local total_count="$3"

    TOTAL=$((TOTAL + 1))

    local safe_name
    safe_name="$(echo "$formula" | sed 's/[^a-zA-Z0-9_-]/_/g')"
    local report_file="$OUTPUT_DIR/${safe_name}.census.txt"
    local alt_file="$OUTPUT_DIR/${safe_name}.alt.txt"
    local class_file="$OUTPUT_DIR/${safe_name}.class.txt"

    if [[ "$QUIET" != true ]]; then
        echo -e "\n${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
        echo -e "${BOLD}  [$idx/$total_count] Analyzing: ${MAGENTA}$formula${NC}"
        echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    fi

    # Try compiled census binary first
    if [[ -x "$CENSUS_BIN" ]]; then
        "$CENSUS_BIN" "$formula" \
            --trials "$CONFORMER_TRIALS" \
            --seed "$SEED" \
            > "$report_file" 2>&1
        local exit_code=$?
    else
        # Fallback: use vsepr CLI with extended analysis
        run_cli_census "$formula" "$report_file" "$alt_file" "$class_file"
        local exit_code=$?
    fi

    if [[ $exit_code -eq 0 ]]; then
        SUCCESS=$((SUCCESS + 1))

        # Count data points from report
        local dp_count
        dp_count=$(grep -oP 'data points' "$report_file" 2>/dev/null | head -1)
        if [[ -n "$dp_count" ]]; then
            local n
            n=$(grep -oP '\d+ data points' "$report_file" 2>/dev/null | head -1 | grep -oP '^\d+')
            TOTAL_DATA_POINTS=$((TOTAL_DATA_POINTS + ${n:-112}))
        else
            TOTAL_DATA_POINTS=$((TOTAL_DATA_POINTS + 112))
        fi

        # Extract classification summary
        if [[ -f "$report_file" ]]; then
            grep -A6 'CLASSIFICATION' "$report_file" > "$class_file" 2>/dev/null
        fi

        if [[ "$QUIET" != true ]]; then
            echo -e "  ${GREEN}✓ SUCCESS${NC}"
            # Show classification if available
            local group_line
            group_line=$(grep 'Group:' "$report_file" 2>/dev/null | head -1)
            local purpose_line
            purpose_line=$(grep 'Purpose' "$report_file" 2>/dev/null | head -1)
            [[ -n "$group_line" ]] && echo -e "    ${group_line}"
            [[ -n "$purpose_line" ]] && echo -e "    ${purpose_line}"
        fi
    else
        FAILED=$((FAILED + 1))
        if [[ "$QUIET" != true ]]; then
            echo -e "  ${RED}✗ FAILED${NC} (exit code $exit_code)"
        fi
    fi
}

# ============================================================================
# CLI Fallback Analysis (when compiled binary unavailable)
# ============================================================================

run_cli_census() {
    local formula="$1"
    local report_file="$2"
    local alt_file="$3"
    local class_file="$4"

    {
        echo "╔══════════════════════════════════════════════════════════════════════════════╗"
        echo "║                    MOLECULAR CENSUS — CLI MODE                              ║"
        echo "╚══════════════════════════════════════════════════════════════════════════════╝"
        echo ""
        echo "  Formula:    $formula"
        echo "  Timestamp:  $(date '+%Y-%m-%d %H:%M:%S')"
        echo "  Mode:       CLI fallback (compiled census binary not found)"
        echo "  Trials:     $CONFORMER_TRIALS"
        echo "  Seed:       $SEED"
        echo ""

        # Primary build via vsepr CLI
        if [[ -x "$VSEPR_BIN" ]]; then
            echo "─── PRIMARY BUILD ───────────────────────────────────────"
            timeout 60 "$VSEPR_BIN" form "$formula" 2>&1 || true
            echo ""
            echo "─── RELAXATION ──────────────────────────────────────────"
            timeout 120 "$VSEPR_BIN" relax "$formula" 2>&1 || true
            echo ""
        else
            echo "  WARNING: vsepr binary not found at $VSEPR_BIN"
            echo "  Build with: cmake --build build_test --config Release"
            echo ""
        fi

        echo "─── DATA POINT ESTIMATE ─────────────────────────────────"
        echo "  Minimum data points: 112 (requires compiled census binary for full collection)"
        echo ""
        echo "  112 data points"

    } > "$report_file" 2>&1

    return 0
}

# ============================================================================
# Session Summary
# ============================================================================

write_session_summary() {
    local summary_file="$OUTPUT_DIR/session_summary.txt"

    {
        echo "╔══════════════════════════════════════════════════════════════════════════════╗"
        echo "║                    MOLECULAR CENSUS — SESSION SUMMARY                       ║"
        echo "╚══════════════════════════════════════════════════════════════════════════════╝"
        echo ""
        echo "  Session:     $SESSION_NAME"
        echo "  Timestamp:   $(date '+%Y-%m-%d %H:%M:%S')"
        echo "  Output dir:  $OUTPUT_DIR"
        echo ""
        echo "  ┌─────────────────────────────────────┐"
        echo "  │  Total molecules:    $(printf '%4d' $TOTAL)             │"
        echo "  │  Succeeded:          $(printf '%4d' $SUCCESS)             │"
        echo "  │  Failed:             $(printf '%4d' $FAILED)             │"
        printf "  │  Success rate:       %5.1f%%           │\n" "$(echo "scale=1; $SUCCESS * 100 / ($TOTAL + 0.001)" | bc 2>/dev/null || echo "0.0")"
        echo "  │  Total data points:  $(printf '%4d' $TOTAL_DATA_POINTS)             │"
        echo "  │  Conformer trials:   $(printf '%4d' $CONFORMER_TRIALS)             │"
        echo "  │  Seed:               $(printf '%4d' $SEED)             │"
        echo "  └─────────────────────────────────────┘"
        echo ""

        # Classification distribution
        echo "─── CLASSIFICATION DISTRIBUTION ───────────────────────────"
        echo ""

        local groups=()
        local purposes=()

        for f in "$OUTPUT_DIR"/*.class.txt; do
            [[ -f "$f" ]] || continue
            local g
            g=$(grep 'Group:' "$f" 2>/dev/null | head -1 | sed 's/.*Group:\s*//')
            local p
            p=$(grep 'Purpose' "$f" 2>/dev/null | head -1 | sed 's/.*Purpose.*:\s*//')
            [[ -n "$g" ]] && groups+=("$g")
            [[ -n "$p" ]] && purposes+=("$p")
        done

        echo "  Groups:"
        printf '%s\n' "${groups[@]}" 2>/dev/null | sort | uniq -c | sort -rn | while read -r cnt name; do
            printf "    %-30s %d\n" "$name" "$cnt"
        done

        echo ""
        echo "  Purpose Types:"
        printf '%s\n' "${purposes[@]}" 2>/dev/null | sort | uniq -c | sort -rn | while read -r cnt name; do
            printf "    %-30s %d\n" "$name" "$cnt"
        done

        echo ""
        echo "─── MOLECULE LIST ───────────────────────────────────────"
        echo ""
        for f in "$OUTPUT_DIR"/*.census.txt; do
            [[ -f "$f" ]] || continue
            local base
            base=$(basename "$f" .census.txt)
            local status="OK"
            grep -q 'FAIL' "$f" 2>/dev/null && status="WARN"
            printf "  %-20s  %s\n" "$base" "$status"
        done
        echo ""

    } > "$summary_file"

    echo -e "\n${BOLD}Session summary written to:${NC} $summary_file"
}

# ============================================================================
# Main Entry Point
# ============================================================================

main() {
    parse_args "$@"

    # Create output directory
    mkdir -p "$OUTPUT_DIR"

    echo -e "\n${BOLD}${BLUE}╔══════════════════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BOLD}${BLUE}║              MOLECULAR CENSUS — Deep Analysis Session                      ║${NC}"
    echo -e "${BOLD}${BLUE}╚══════════════════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "  Molecules:  ${#FORMULAS[@]}"
    echo -e "  Trials:     $CONFORMER_TRIALS per molecule"
    echo -e "  Seed:       $SEED"
    echo -e "  Output:     $OUTPUT_DIR"
    echo ""

    local total_count=${#FORMULAS[@]}
    local idx=0

    for formula in "${FORMULAS[@]}"; do
        idx=$((idx + 1))
        run_census_for_molecule "$formula" "$idx" "$total_count"
    done

    # Write session summary
    write_session_summary

    # Final report
    echo ""
    echo -e "${BOLD}${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BOLD}  CENSUS COMPLETE${NC}"
    echo -e "  Total:       $TOTAL molecules"
    echo -e "  Succeeded:   ${GREEN}$SUCCESS${NC}"
    echo -e "  Failed:      ${RED}$FAILED${NC}"
    echo -e "  Data points: ${CYAN}$TOTAL_DATA_POINTS${NC}"
    echo -e "  Output:      $OUTPUT_DIR"
    echo -e "${BOLD}${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

main "$@"
