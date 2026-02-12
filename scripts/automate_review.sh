#!/usr/bin/env bash
################################################################################
# VSEPR-Sim Automated Review
# Reviews each discovered molecule with detailed output and visualization
################################################################################

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

VSEPR_BIN="$PROJECT_ROOT/build/bin/vsepr"
REVIEW_OUTPUT="$PROJECT_ROOT/outputs/reviews"
SESSION_DIR="${1:-}"

# Colors
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
MAGENTA='\033[0;35m'
RED='\033[0;31m'
BOLD='\033[1m'
NC='\033[0m'

# ============================================================================
# Functions
# ============================================================================

print_header() {
    echo ""
    echo -e "${MAGENTA}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${MAGENTA}║                                                                ║${NC}"
    echo -e "${MAGENTA}║             VSEPR-Sim Automated Review                         ║${NC}"
    echo -e "${MAGENTA}║          Detailed Analysis of Discovered Molecules             ║${NC}"
    echo -e "${MAGENTA}║                                                                ║${NC}"
    echo -e "${MAGENTA}╚════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

find_latest_session() {
    local latest=$(ls -td "$PROJECT_ROOT"/outputs/sessions/random_discovery_* 2>/dev/null | head -n 1)
    echo "$latest"
}

review_molecule() {
    local formula=$1
    local index=$2
    local total=$3
    
    echo ""
    echo -e "${CYAN}═══════════════════════════════════════════════════════════════════${NC}"
    echo -e "${BOLD}${YELLOW}Molecule [$index/$total]: $formula${NC}"
    echo -e "${CYAN}═══════════════════════════════════════════════════════════════════${NC}"
    echo ""
    
    # Try to open Wikipedia page for common chemicals
    bash "$PROJECT_ROOT/scripts/wiki.sh" auto "$formula" 2>/dev/null && \
        echo -e "${GREEN}📖 Opened Wikipedia page${NC}" || true
    echo ""
    
    # Create review directory for this molecule
    local safe_formula=$(echo "$formula" | tr -cd '[:alnum:]()+\-' | cut -c1-50)
    if [ -z "$safe_formula" ]; then
        safe_formula="mol_${index}"
    fi
    
    local review_dir="$REVIEW_OUTPUT/${safe_formula}_${index}"
    mkdir -p "$review_dir"
    
    echo -e "${CYAN}▶ Building molecule with full details...${NC}"
    echo ""
    
    # Build molecule with verbose output
    "$VSEPR_BIN" build "$formula" \
        --optimize \
        --output "$review_dir/geometry.xyz" \
        --viz "$review_dir/viewer.html" \
        2>&1 | tee "$review_dir/build.log"
    
    local exit_code=${PIPESTATUS[0]}
    
    echo ""
    
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}✓ Build successful${NC}"
        
        # Display XYZ file contents
        if [ -f "$review_dir/geometry.xyz" ]; then
            echo ""
            echo -e "${CYAN}▶ Geometry (XYZ format):${NC}"
            echo -e "${YELLOW}─────────────────────────────────────────────────────────────${NC}"
            cat "$review_dir/geometry.xyz"
            echo -e "${YELLOW}─────────────────────────────────────────────────────────────${NC}"
            
            # Copy to clipboard if xclip available
            if command -v xclip &> /dev/null; then
                cat "$review_dir/geometry.xyz" | xclip -selection clipboard
                echo -e "${GREEN}✓ Geometry copied to clipboard (xclip)${NC}"
            elif command -v clip.exe &> /dev/null; then
                cat "$review_dir/geometry.xyz" | clip.exe
                echo -e "${GREEN}✓ Geometry copied to clipboard (Windows)${NC}"
            fi
            
            # File statistics
            echo ""
            echo -e "${CYAN}▶ File Information:${NC}"
            local atom_count=$(head -n 1 "$review_dir/geometry.xyz")
            local file_size=$(du -h "$review_dir/geometry.xyz" | cut -f1)
            echo -e "  ${YELLOW}•${NC} Atoms: $atom_count"
            echo -e "  ${YELLOW}•${NC} File size: $file_size"
            echo -e "  ${YELLOW}•${NC} XYZ file: $review_dir/geometry.xyz"
            
            if [ -f "$review_dir/viewer.html" ]; then
                local html_size=$(du -h "$review_dir/viewer.html" | cut -f1)
                echo -e "  ${YELLOW}•${NC} HTML viewer: $review_dir/viewer.html (${html_size})"
                
                # Auto-open HTML visualization
                if command -v xdg-open &> /dev/null; then
                    xdg-open "$review_dir/viewer.html" &>/dev/null &
                elif [ -f "/mnt/c/Windows/System32/cmd.exe" ]; then
                    /mnt/c/Windows/System32/cmd.exe /c start "$review_dir/viewer.html" &>/dev/null &
                fi
                echo -e "  ${GREEN}•${NC} Auto-opened HTML viewer"
            fi
        fi
        
        # Extract key information from build log
        echo ""
        echo -e "${CYAN}▶ Analysis Summary:${NC}"
        
        # Geometry type
        local geometry=$(grep "Geometry" "$review_dir/build.log" | grep -v "Optimizing" | tail -n 1)
        if [ -n "$geometry" ]; then
            echo -e "  ${YELLOW}•${NC} $geometry"
        fi
        
        # Energy
        local energy=$(grep "Final energy" "$review_dir/build.log" | tail -n 1)
        if [ -n "$energy" ]; then
            echo -e "  ${YELLOW}•${NC} $energy"
        fi
        
        # Convergence
        local iterations=$(grep "converged in" "$review_dir/build.log" | tail -n 1)
        if [ -n "$iterations" ]; then
            echo -e "  ${YELLOW}•${NC} $iterations"
        fi
        
        # Lone pairs
        local lone_pairs=$(grep "Lone pairs" "$review_dir/build.log" | tail -n 1)
        if [ -n "$lone_pairs" ]; then
            echo -e "  ${YELLOW}•${NC} $lone_pairs"
        fi
        
    else
        echo -e "${RED}✗ Build failed${NC}"
    fi
    
    echo ""
    echo -e "${CYAN}▶ Review files saved to:${NC} ${review_dir}"
    echo ""
    
    # Sleep between molecules
    echo -e "${YELLOW}⏱  Waiting 10 seconds before next molecule...${NC}"
    sleep 10
}

# ============================================================================
# Main
# ============================================================================

print_header

# Check if VSEPR binary exists
if [ ! -f "$VSEPR_BIN" ]; then
    echo -e "${RED}Error: VSEPR binary not found at: $VSEPR_BIN${NC}"
    echo "Please build the project first: bash scripts/build/build.sh"
    exit 1
fi

# Determine session directory
if [ -z "$SESSION_DIR" ]; then
    SESSION_DIR=$(find_latest_session)
    if [ -z "$SESSION_DIR" ]; then
        echo -e "${RED}Error: No random discovery sessions found${NC}"
        echo "Run: bash scripts/random_discovery.sh <iterations>"
        exit 1
    fi
    echo -e "${CYAN}Using latest session:${NC} $(basename "$SESSION_DIR")"
else
    if [ ! -d "$SESSION_DIR" ]; then
        echo -e "${RED}Error: Session directory not found: $SESSION_DIR${NC}"
        exit 1
    fi
fi

# Check for success log
SUCCESS_LOG="$SESSION_DIR/success.log"
if [ ! -f "$SUCCESS_LOG" ]; then
    echo -e "${RED}Error: No success log found at: $SUCCESS_LOG${NC}"
    exit 1
fi

# Count successful molecules
TOTAL=$(wc -l < "$SUCCESS_LOG")
echo -e "${CYAN}Found:${NC} ${GREEN}$TOTAL${NC} successful molecules to review"
echo ""

# Create review output directory
REVIEW_OUTPUT="$REVIEW_OUTPUT/$(basename "$SESSION_DIR")"
mkdir -p "$REVIEW_OUTPUT"

echo -e "${CYAN}Review output directory:${NC} $REVIEW_OUTPUT"

# Confirm with user
read -p "$(echo -e ${YELLOW}Continue with automated review? [y/N]: ${NC})" -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Review cancelled."
    exit 0
fi

# Process each molecule
INDEX=1
while IFS='|' read -r formula status duration timestamp; do
    review_molecule "$formula" "$INDEX" "$TOTAL"
    ((INDEX++))
done < "$SUCCESS_LOG"

# Final summary
echo ""
echo -e "${MAGENTA}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${MAGENTA}║                     Review Complete                            ║${NC}"
echo -e "${MAGENTA}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${CYAN}Reviewed:${NC} ${GREEN}$TOTAL${NC} molecules"
echo -e "${CYAN}Results:${NC} $REVIEW_OUTPUT"
echo ""
echo -e "${YELLOW}You can now:${NC}"
echo -e "  ${YELLOW}•${NC} Open HTML viewers in your browser"
echo -e "  ${YELLOW}•${NC} Load XYZ files in visualization software"
echo -e "  ${YELLOW}•${NC} Review build logs for detailed information"
echo ""
