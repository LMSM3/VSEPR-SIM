#!/usr/bin/env bash
################################################################################
# vseprW.sh - VSEPR-Sim Wrapper Script
# Handles complex commands with PowerShell/C++ coordination
################################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VSEPR_BIN="$SCRIPT_DIR/build/bin/vsepr"
VSEPR_EXE="$SCRIPT_DIR/build/bin/vsepr.exe"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

log_info() {
    echo -e "${CYAN}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[OK]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_attention_tip() {
    echo ""
    echo -e "${YELLOW}▶ Tip:${NC} Run ${CYAN}source ./activate.sh${NC} to enable the ${GREEN}vsepr${NC} command"
    echo -e "${YELLOW}▶ Or:${NC} use ${CYAN}./vsepr build random --watch${NC} for instant 3D"
    echo ""
}

# ============================================================================
# Detect Executable
# ============================================================================

detect_executable() {
    if [ -f "$VSEPR_BIN" ]; then
        VSEPR_CMD="$VSEPR_BIN"
        log_info "Using Unix binary: $VSEPR_BIN"
    elif [ -f "$VSEPR_EXE" ]; then
        VSEPR_CMD="$VSEPR_EXE"
        log_info "Using Windows binary: $VSEPR_EXE"
    else
        log_error "No VSEPR executable found!"
        log_info "Build with: ./build.sh or build.bat"
        exit 1
    fi
}

# ============================================================================
# Command Handlers
# ============================================================================

# Batch processing with PowerShell coordination
handle_batch() {
    log_info "Batch processing mode"
    
    # Check if PowerShell batch helper exists
    if [ -f "$SCRIPT_DIR/scripts/batch_helper.ps1" ]; then
        log_info "Using PowerShell batch coordinator"
        pwsh -File "$SCRIPT_DIR/scripts/batch_helper.ps1" "$@"
    else
        # Direct C++ batch
        "$VSEPR_CMD" batch "$@"
    fi
}

# Optimization with progress tracking
handle_optimize() {
    log_info "Optimization mode"
    
    # Run with progress monitoring
    "$VSEPR_CMD" optimize "$@" 2>&1 | while IFS= read -r line; do
        echo "$line"
        
        # Parse energy from output and show progress
        if [[ $line =~ Energy:\ ([0-9.-]+) ]]; then
            energy="${BASH_REMATCH[1]}"
            echo -ne "\r  Current: $energy Ha  " >&2
        fi
    done
    
    echo "" >&2
    log_success "Optimization complete"
}

# Molecular dynamics with PowerShell analysis
handle_md() {
    log_info "Molecular dynamics mode"
    
    # Run MD simulation
    "$VSEPR_CMD" md "$@"
    
    # Check if we should post-process
    if [ -f "$SCRIPT_DIR/scripts/md_analysis.ps1" ]; then
        log_info "Running post-MD analysis..."
        pwsh -File "$SCRIPT_DIR/scripts/md_analysis.ps1"
    fi
}

# Build command with validation
handle_build() {
    log_info "Building molecule: $1"
    
    "$VSEPR_CMD" build "$@"
    
    local exit_code=$?
    if [ $exit_code -eq 0 ]; then
        log_success "Molecule built successfully"
    else
        log_error "Build failed with code $exit_code"
    fi
    
    return $exit_code
}

# Test command (delegate to test.sh)
handle_test() {
    log_info "Running tests"
    
    if [ -f "$SCRIPT_DIR/test.sh" ]; then
        bash "$SCRIPT_DIR/test.sh" "$@"
    else
        log_error "test.sh not found"
        exit 1
    fi
}

# ============================================================================
# Main Router
# ============================================================================

main() {
    detect_executable
    
    if [ $# -eq 0 ]; then
        "$VSEPR_CMD" --help
        print_attention_tip
        exit 0
    fi
    
    local command="$1"
    shift
    
    case "$command" in
        batch)
            handle_batch "$@"
            ;;
        optimize)
            handle_optimize "$@"
            ;;
        md)
            handle_md "$@"
            ;;
        build)
            handle_build "$@"
            ;;
        test)
            handle_test "$@"
            ;;
        *)
            # Pass through to C++ executable
            "$VSEPR_CMD" "$command" "$@"
            ;;
    esac
}

main "$@"
