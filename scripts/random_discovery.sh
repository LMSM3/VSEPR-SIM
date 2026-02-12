#!/usr/bin/env bash
################################################################################
# VSEPR-Sim Random Molecule Fuzzer
# Automated discovery and visualization of molecules
# Tests formula parser robustness with random inputs
################################################################################

# NOTE: Not using 'set -e' to continue testing even when molecules fail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Configuration
ITERATIONS=${1:-10000}
SESSION_NAME="random_discovery_$(date +%Y%m%d_%H%M%S)"
SESSION_DIR="$PROJECT_ROOT/outputs/sessions/$SESSION_NAME"
VSEPR_BIN="$PROJECT_ROOT/build/bin/vsepr"

# Statistics - Default to zero
TOTAL=0
SUCCESS=0
FAILED=0
REJECTED=0
TIMEOUT=0

# Colors - Add themes 
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

# ============================================================================
# Random Generation Functions
# ============================================================================

# Common elements for molecule generation
ELEMENTS=(H C N O F P S Cl Br I Si B Al Fe Cu Zn Ag Au Li Na K Ca Mg)
IONS=(+ - 2+ 2- 3+ 3-)

generate_random_element() {
    echo "${ELEMENTS[$RANDOM % ${#ELEMENTS[@]}]}"
}

generate_random_count() {
    # Random count 1-8
    echo $((RANDOM % 8 + 1))
}

generate_random_ion() {
    if [ $((RANDOM % 3)) -eq 0 ]; then
        echo "${IONS[$RANDOM % ${#IONS[@]}]}"
    else
        echo ""
    fi
}

# Pattern 1: Simple formula (e.g., H2O, NH3, CH4)
generate_simple_formula() {
    local elem1=$(generate_random_element)
    local count1=$(generate_random_count)
    local elem2=$(generate_random_element)
    local count2=$(generate_random_count)
    
    echo "${elem1}${count1}${elem2}${count2}"
}

# Pattern 2: Complex formula (e.g., Ca(OH)2, Fe2(SO4)3)
generate_complex_formula() {
    local elem1=$(generate_random_element)
    local count1=$(generate_random_count)
    local elem2=$(generate_random_element)
    local elem3=$(generate_random_element)
    local count2=$(generate_random_count)
    local count3=$(generate_random_count)
    
    echo "${elem1}${count1}(${elem2}${elem3})${count2}"
}

# Pattern 3: Ionic formula (e.g., Na+, SO4 2-, Fe3+)
generate_ionic_formula() {
    local elem=$(generate_random_element)
    local count=$(generate_random_count)
    local ion=$(generate_random_ion)
    
    if [ -n "$ion" ]; then
        echo "${elem}${count}${ion}"
    else
        echo "${elem}${count}"
    fi
}

# Pattern 4: Random garbage (stress test parser)
generate_garbage() {
    local len=$((RANDOM % 20 + 5))
    local result=""
    
    for i in $(seq 1 $len); do
        case $((RANDOM % 6)) in
            0) result+="${ELEMENTS[$RANDOM % ${#ELEMENTS[@]}]}" ;;
            1) result+="$((RANDOM % 10))" ;;
            2) result+="(" ;;
            3) result+=")" ;;
            4) result+="+" ;;
            5) result+="-" ;;
        esac
    done
    
    echo "$result"
}

# Pattern 5: Edge cases
generate_edge_case() {
    local cases=(
        ""                          # Empty
        "X"                        # Single letter
        "123"                      # Only numbers
        "H1000000"                 # Huge count
        "((((H))))"                # Nested parens
        "H2O3N4C5S6"              # Long chain
        "+++---"                   # Only symbols
        "H2O H2O"                  # Spaces
        "水"                       # Unicode
        "H2O\nNH3"                 # Newline
    )
    
    echo "${cases[$RANDOM % ${#cases[@]}]}"
}

# Generate random formula using various patterns
generate_random_formula() {
    local pattern=$((RANDOM % 10))
    
    case $pattern in
        0|1|2) generate_simple_formula ;;
        3|4) generate_complex_formula ;;
        5|6) generate_ionic_formula ;;
        7|8) generate_garbage ;;
        9) generate_edge_case ;;
    esac
}

# ============================================================================
# Testing Functions
# ============================================================================

test_formula() {
    local formula=$1
    local index=$2
    local output_base="$SESSION_DIR/molecules/mol_${index}"
    
    # Escape special chars for filename
    local safe_formula=$(echo "$formula" | tr -d '[:space:]' | tr -cd '[:alnum:]()+\-' | cut -c1-50)
    if [ -z "$safe_formula" ]; then
        safe_formula="empty_${index}"
    fi
    
    output_base="$SESSION_DIR/molecules/${safe_formula}_${index}"
    
    # Try to build molecule
    local start_time=$(date +%s.%N)
    
    if timeout 5s "$VSEPR_BIN" build "$formula" \
        --optimize \
        --output "${output_base}.xyz" \
        --viz "${output_base}.html" \
        > "${output_base}.log" 2>&1; then
        
        local end_time=$(date +%s.%N)
        local duration=$(echo "$end_time - $start_time" | bc)
        
        ((SUCCESS++))
        echo -e "${GREEN}✓${NC} [$index/$ITERATIONS] SUCCESS: '$formula' (${duration}s)"
        
        # Log success
        echo "$formula|SUCCESS|${duration}|$(date -Iseconds)" >> "$SESSION_DIR/success.log"
        
        # Add to cache if valid
        if [ -f "${output_base}.xyz" ]; then
            bash "$PROJECT_ROOT/scripts/cache.sh" add "${output_base}.xyz" "$safe_formula" 2>/dev/null || true
        fi
        
        return 0
    else
        local exit_code=$?
        local end_time=$(date +%s.%N)
        local duration=$(echo "$end_time - $start_time" | bc)
        
        if [ $exit_code -eq 124 ]; then
            ((TIMEOUT++))
            echo -e "${YELLOW}⏱${NC} [$index/$ITERATIONS] TIMEOUT: '$formula'"
            echo "$formula|TIMEOUT|${duration}|$(date -Iseconds)" >> "$SESSION_DIR/timeout.log"
        else
            # Check if it's a parser rejection or other failure
            if grep -q "parse\|invalid\|unknown" "${output_base}.log" 2>/dev/null; then
                ((REJECTED++))
                echo -e "${CYAN}⊘${NC} [$index/$ITERATIONS] REJECTED: '$formula'"
                echo "$formula|REJECTED|${duration}|$(date -Iseconds)" >> "$SESSION_DIR/rejected.log"
            else
                ((FAILED++))
                echo -e "${RED}✗${NC} [$index/$ITERATIONS] FAILED: '$formula'"
                echo "$formula|FAILED|${duration}|$(date -Iseconds)" >> "$SESSION_DIR/failed.log"
            fi
        fi
        
        return 1
    fi
}

# ============================================================================
# Main Execution
# ============================================================================

main() {
    echo -e "${MAGENTA}╔═══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${MAGENTA}║                                                               ║${NC}"
    echo -e "${MAGENTA}║          VSEPR-Sim Random Molecule Discovery                  ║${NC}"
    echo -e "${MAGENTA}║           Fuzzing Test - Formula Parser                      ║${NC}"
    echo -e "${MAGENTA}║                                                               ║${NC}"
    echo -e "${MAGENTA}╚═══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${CYAN}Iterations: $ITERATIONS${NC}"
    echo -e "${CYAN}Session: $SESSION_NAME${NC}"
    echo ""
    
    # Check if vsepr binary exists
    if [ ! -f "$VSEPR_BIN" ]; then
        echo -e "${RED}Error: VSEPR binary not found at $VSEPR_BIN${NC}"
        echo "Build the project first: ./build.sh"
        exit 1
    fi
    
    # Create session
    mkdir -p "$SESSION_DIR"/{molecules,visualizations,analysis}
    
    # Session metadata
    cat > "$SESSION_DIR/session.json" <<EOF
{
  "session_id": "$SESSION_NAME",
  "name": "Random Discovery Fuzzing Test",
  "created": "$(date -Iseconds)",
  "iterations": $ITERATIONS,
  "status": "running"
}
EOF
    
    # Initialize log files
    touch "$SESSION_DIR"/{success,rejected,failed,timeout}.log
    
    # Start time
    local start_total=$(date +%s)
    
    # Main loop
    echo -e "${CYAN}Starting random molecule generation...${NC}"
    echo ""
    
    for i in $(seq 1 $ITERATIONS); do
        ((TOTAL++))
        
        # Generate random formula
        local formula=$(generate_random_formula)
        
        # Test it
        test_formula "$formula" "$i"
        
        # Progress update every 100 iterations
        if [ $((i % 100)) -eq 0 ]; then
            local progress=$((i * 100 / ITERATIONS))
            echo ""
            echo -e "${MAGENTA}═══ Progress: $i/$ITERATIONS ($progress%) ═══${NC}"
            echo -e "  Success: ${GREEN}$SUCCESS${NC} | Rejected: ${CYAN}$REJECTED${NC} | Failed: ${RED}$FAILED${NC} | Timeout: ${YELLOW}$TIMEOUT${NC}"
            echo ""
        fi
    done
    
    # End time
    local end_total=$(date +%s)
    local duration_total=$((end_total - start_total))
    
    # Generate report
    generate_report "$duration_total"
    
    # Update session metadata
    cat > "$SESSION_DIR/session.json" <<EOF
{
  "session_id": "$SESSION_NAME",
  "name": "Random Discovery Fuzzing Test",
  "created": "$(date -Iseconds)",
  "completed": "$(date -Iseconds)",
  "iterations": $ITERATIONS,
  "duration_seconds": $duration_total,
  "results": {
    "total": $TOTAL,
    "success": $SUCCESS,
    "rejected": $REJECTED,
    "failed": $FAILED,
    "timeout": $TIMEOUT
  },
  "status": "completed"
}
EOF
    
    echo ""
    echo -e "${GREEN}Fuzzing test completed!${NC}"
    echo -e "Results saved to: ${CYAN}$SESSION_DIR${NC}"
}

generate_report() {
    local duration=$1
    local report_file="$SESSION_DIR/analysis/report.md"
    
    cat > "$report_file" <<EOF
# Random Molecule Discovery - Fuzzing Test Report

**Session**: $SESSION_NAME  
**Date**: $(date)  
**Duration**: ${duration}s ($(echo "scale=2; $duration / 60" | bc) minutes)

---

## Summary Statistics

| Metric | Count | Percentage |
|--------|-------|------------|
| **Total Iterations** | $TOTAL | 100% |
| **Successful** | $SUCCESS | $(echo "scale=2; $SUCCESS * 100 / $TOTAL" | bc)% |
| **Parser Rejected** | $REJECTED | $(echo "scale=2; $REJECTED * 100 / $TOTAL" | bc)% |
| **Failed** | $FAILED | $(echo "scale=2; $FAILED * 100 / $TOTAL" | bc)% |
| **Timeout** | $TIMEOUT | $(echo "scale=2; $TIMEOUT * 100 / $TOTAL" | bc)% |

**Success Rate**: $(echo "scale=2; $SUCCESS * 100 / $TOTAL" | bc)%  
**Parser Robustness**: $(echo "scale=2; ($SUCCESS + $REJECTED) * 100 / $TOTAL" | bc)% (handled gracefully)

---

## Performance

- **Average time per molecule**: $(echo "scale=3; $duration / $TOTAL" | bc)s
- **Successful molecules per second**: $(if [ "$SUCCESS" -gt 0 ] && [ "$duration" -gt 0 ]; then echo "scale=2; $SUCCESS / $duration" | bc; else echo "0.00"; fi)
- **Total molecules tested**: $TOTAL

---

## Top Successful Formulas

\`\`\`
$(head -n 20 "$SESSION_DIR/success.log" | cut -d'|' -f1)
\`\`\`

---

## Files Generated

- **XYZ geometries**: $SUCCESS files in \`molecules/\`
- **HTML visualizations**: $SUCCESS files in \`molecules/\`
- **Logs**: \`success.log\`, \`rejected.log\`, \`failed.log\`, \`timeout.log\`

---

## Findings

### Parser Rejection Examples
\`\`\`
$(head -n 10 "$SESSION_DIR/rejected.log" | cut -d'|' -f1)
\`\`\`

### Timeout Cases
\`\`\`
$(head -n 10 "$SESSION_DIR/timeout.log" | cut -d'|' -f1)
\`\`\`

---

## Recommendations

1. **Parser Improvements**: Review rejected formulas for edge cases
2. **Performance**: Investigate timeout cases for optimization opportunities
3. **Validation**: Examine failed cases for potential bugs

---

**Generated by**: VSEPR-Sim Random Discovery Fuzzer  
**Report Date**: $(date -Iseconds)
EOF
    
    echo ""
    echo -e "${CYAN}Report generated: $report_file${NC}"
}

# Run main
main

# Final summary
echo ""
echo -e "${MAGENTA}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${MAGENTA}║                    Final Statistics                           ║${NC}"
echo -e "${MAGENTA}╚═══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "  ${CYAN}Total Tested:${NC}     $TOTAL"
echo -e "  ${GREEN}✓ Success:${NC}        $SUCCESS ($(echo "scale=1; $SUCCESS * 100 / $TOTAL" | bc)%)"
echo -e "  ${CYAN}⊘ Rejected:${NC}       $REJECTED ($(echo "scale=1; $REJECTED * 100 / $TOTAL" | bc)%)"
echo -e "  ${RED}✗ Failed:${NC}         $FAILED ($(echo "scale=1; $FAILED * 100 / $TOTAL" | bc)%)"
echo -e "  ${YELLOW}⏱ Timeout:${NC}        $TIMEOUT ($(echo "scale=1; $TIMEOUT * 100 / $TOTAL" | bc)%)"
echo ""
echo -e "${CYAN}Session:${NC} $SESSION_DIR"
echo -e "${CYAN}Report:${NC}  $SESSION_DIR/analysis/report.md"
echo ""
