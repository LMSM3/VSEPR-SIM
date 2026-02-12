#!/usr/bin/env bash
################################################################################
# LabelMe - Molecular Phase State Labeling System
# VSEPR-Sim v2.3.1
#
# Labels molecular states (SOLID, LIQUID, GAS, PLASMA) based on temperature
################################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."

DB="${LABELME_DB:-$ROOT/data/states_db.csv}"
BIN="${LABELME_BIN:-$ROOT/build/bin/labelme}"
SRC="$ROOT/tools/labelme.c"

# Colors
BLUE='\033[0;34m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

usage() {
    cat <<EOF
╔════════════════════════════════════════════════════════════════╗
║  LabelMe v2.3.1 - Molecular Phase State Labeling System       ║
╚════════════════════════════════════════════════════════════════╝

Usage:
  $0 build                                  # Build labelme binary
  $0 label <MOLECULE> <TEMP_K>              # Label single state
  $0 range <MOLECULE> <T_START> <T_END> <STEP>  # Label range
  $0 test                                   # Run test suite
  $0 help                                   # Show this help

Environment Variables:
  LABELME_DB   Path to database (default: data/states_db.csv)
  LABELME_BIN  Path to binary (default: build/bin/labelme)

Examples:
  $0 build
  $0 label H2O 298.15
  $0 label H2O 100    # Ice
  $0 label H2O 450    # Steam
  $0 range H2O 200 600 25
  $0 range I2 300 600 50

Output Format:
  molecule,tempK,state,meltK,boilK,plasmaK

States:
  SOLID   - Below melting point
  LIQUID  - Between melting and boiling
  GAS     - Above boiling, below plasma
  PLASMA  - Above plasma threshold
EOF
}

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Build labelme binary
build() {
    log_info "Building LabelMe..."
    
    if [[ ! -f "$SRC" ]]; then
        log_error "Source not found: $SRC"
        exit 1
    fi
    
    mkdir -p "$(dirname "$BIN")"
    
    gcc -O2 -std=c11 -Wall -Wextra -o "$BIN" "$SRC"
    
    if [[ -f "$BIN" ]]; then
        log_success "Built: $BIN"
    else
        log_error "Build failed"
        exit 1
    fi
}

# Check if binary exists
check_binary() {
    if [[ ! -f "$BIN" ]]; then
        log_warning "Binary not found: $BIN"
        log_info "Building..."
        build
    fi
    
    if [[ ! -f "$DB" ]]; then
        log_error "Database not found: $DB"
        exit 1
    fi
}

# Label single temperature
label_one() {
    local mol="$1"
    local temp="$2"
    
    check_binary
    echo "molecule,tempK,state,meltK,boilK,plasmaK"
    "$BIN" "$DB" "$mol" "$temp"
}

# Label temperature range
label_range() {
    local mol="$1"
    local start="$2"
    local end="$3"
    local step="$4"
    
    check_binary
    
    # Header
    echo "molecule,tempK,state,meltK,boilK,plasmaK"
    
    # Generate temperature range using awk
    awk -v s="$start" -v e="$end" -v st="$step" 'BEGIN{
        for (t=s; t<=e+1e-12; t+=st) printf("%.6f\n", t);
    }' | while read -r t; do
        "$BIN" "$DB" "$mol" "$t" 2>/dev/null || true
    done
}

# Test suite
run_tests() {
    "$ROOT/scripts/labelme_test.sh"
}

# Main
cmd="${1:-}"

case "$cmd" in
    build)
        build
        ;;
    label)
        if [[ $# -ne 3 ]]; then
            usage
            exit 2
        fi
        label_one "$2" "$3"
        ;;
    range)
        if [[ $# -ne 5 ]]; then
            usage
            exit 2
        fi
        label_range "$2" "$3" "$4" "$5"
        ;;
    test)
        run_tests
        ;;
    help|-h|--help)
        usage
        ;;
    "")
        usage
        ;;
    *)
        log_error "Unknown command: $cmd"
        usage
        exit 2
        ;;
esac
