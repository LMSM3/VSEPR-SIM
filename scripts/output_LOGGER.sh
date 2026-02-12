#!/bin/bash
###############################################################################
# Output Logger - Comprehensive build logging and analysis system
###############################################################################
#
# Features:
# - Automatic timestamped logs
# - Success/failure tracking
# - Performance metrics (atoms, bonds, optimization time)
# - Error pattern detection
# - Log analysis and reporting
# - JSON export for automation
#
# Usage:
#   ./output_LOGGER.sh build H2O              # Build with logging
#   ./output_LOGGER.sh test formula.txt       # Batch test from file
#   ./output_LOGGER.sh analyze                # Analyze existing logs
#   ./output_LOGGER.sh report                 # Generate summary report
#   ./output_LOGGER.sh clean                  # Clean old logs
#
###############################################################################

VSEPR_BIN="./build/bin/vsepr"
LOG_DIR="logs/build_logs"
REPORT_DIR="logs/reports"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

###############################################################################
# Initialization
###############################################################################

init_logger() {
    mkdir -p "$LOG_DIR"
    mkdir -p "$REPORT_DIR"
    mkdir -p "$LOG_DIR/success"
    mkdir -p "$LOG_DIR/failed"
    mkdir -p "$LOG_DIR/xyz_outputs"
}

###############################################################################
# Logging Functions
###############################################################################

log_build() {
    local formula=$1
    local optimize=${2:-false}
    local output_file=""
    
    echo -e "${CYAN}[$(date +"%H:%M:%S")]${NC} Building: $formula"
    
    # Sanitize formula for filename
    local safe_formula=$(echo "$formula" | tr -d '()' | tr ' ' '_')
    local log_file="$LOG_DIR/${TIMESTAMP}_${safe_formula}.log"
    local xyz_file="$LOG_DIR/xyz_outputs/${safe_formula}.xyz"
    
    # Build command with optional optimization
    local cmd="$VSEPR_BIN build $formula"
    if [ "$optimize" = "true" ]; then
        cmd="$cmd --optimize --output $xyz_file"
    fi
    
    # Run and capture output
    local start_time=$(date +%s.%N)
    local exit_code=0
    
    {
        echo "========================================="
        echo "VSEPR Build Log"
        echo "========================================="
        echo "Timestamp: $(date '+%Y-%m-%d %H:%M:%S')"
        echo "Formula: $formula"
        echo "Command: $cmd"
        echo "========================================="
        echo ""
        
        $cmd 2>&1
        exit_code=$?
        
        echo ""
        echo "========================================="
        echo "Exit Code: $exit_code"
        echo "========================================="
    } | tee "$log_file"
    
    local end_time=$(date +%s.%N)
    local duration=$(echo "$end_time - $start_time" | bc)
    
    # Extract metrics from log
    local atoms=$(grep -oP '(?<=Atoms\s{15})\d+' "$log_file" | head -1)
    local bonds=$(grep -oP '(?<=Bonds\s{15})\d+' "$log_file" | head -1)
    local geometry=$(grep -oP '(?<=Geometry\s{12})[A-Za-z ]+' "$log_file" | head -1)
    local converged=$(grep -c "Optimization converged" "$log_file")
    
    # Categorize result
    if [ $exit_code -eq 0 ]; then
        mv "$log_file" "$LOG_DIR/success/"
        log_file="$LOG_DIR/success/$(basename $log_file)"
        echo -e "${GREEN}✓ Success${NC} (${duration}s) - $atoms atoms, $bonds bonds"
        
        # Create success metadata
        {
            echo "formula=$formula"
            echo "status=success"
            echo "atoms=$atoms"
            echo "bonds=$bonds"
            echo "geometry=$geometry"
            echo "duration=$duration"
            echo "converged=$converged"
            echo "timestamp=$(date -Iseconds)"
        } > "${log_file%.log}.meta"
        
    else
        mv "$log_file" "$LOG_DIR/failed/"
        log_file="$LOG_DIR/failed/$(basename $log_file)"
        
        # Extract error type
        local error_type="unknown"
        if grep -q "Invalid formula" "$log_file"; then
            error_type="parser_error"
        elif grep -q "Multi-center topology" "$log_file"; then
            error_type="unsupported_topology"
        elif grep -q "Graph builder error" "$log_file"; then
            error_type="graph_builder"
        fi
        
        echo -e "${RED}✗ Failed${NC} ($error_type)"
        
        # Create failure metadata
        {
            echo "formula=$formula"
            echo "status=failed"
            echo "error_type=$error_type"
            echo "duration=$duration"
            echo "timestamp=$(date -Iseconds)"
        } > "${log_file%.log}.meta"
    fi
    
    return $exit_code
}

###############################################################################
# Batch Testing
###############################################################################

batch_test() {
    local input_file=$1
    
    if [ ! -f "$input_file" ]; then
        echo -e "${RED}Error: File not found: $input_file${NC}"
        return 1
    fi
    
    echo -e "${BLUE}Starting batch test from $input_file${NC}"
    echo ""
    
    local total=0
    local passed=0
    local failed=0
    
    while IFS= read -r line; do
        # Skip comments and empty lines
        [[ "$line" =~ ^#.*$ ]] && continue
        [[ -z "$line" ]] && continue
        
        ((total++))
        
        if log_build "$line" "true"; then
            ((passed++))
        else
            ((failed++))
        fi
        
        echo ""
    done < "$input_file"
    
    # Summary
    echo ""
    echo -e "${CYAN}=========================================${NC}"
    echo -e "${CYAN}Batch Test Complete${NC}"
    echo -e "${CYAN}=========================================${NC}"
    echo -e "Total:   $total"
    echo -e "${GREEN}Passed:  $passed${NC}"
    echo -e "${RED}Failed:  $failed${NC}"
    echo -e "Success: $(echo "scale=1; $passed * 100 / $total" | bc)%"
}

###############################################################################
# Analysis Functions
###############################################################################

analyze_logs() {
    echo -e "${BLUE}Analyzing build logs...${NC}"
    echo ""
    
    local success_count=$(find "$LOG_DIR/success" -name "*.meta" 2>/dev/null | wc -l)
    local failed_count=$(find "$LOG_DIR/failed" -name "*.meta" 2>/dev/null | wc -l)
    local total=$((success_count + failed_count))
    
    if [ $total -eq 0 ]; then
        echo "No logs found. Run some builds first!"
        return 1
    fi
    
    echo "═══ Overall Statistics ═══"
    echo "Total builds: $total"
    echo -e "${GREEN}Successful: $success_count${NC}"
    echo -e "${RED}Failed: $failed_count${NC}"
    echo "Success rate: $(echo "scale=1; $success_count * 100 / $total" | bc)%"
    echo ""
    
    # Analyze successful builds
    if [ $success_count -gt 0 ]; then
        echo "═══ Success Analysis ═══"
        
        # Average atoms
        local total_atoms=0
        local count=0
        for meta in "$LOG_DIR/success"/*.meta; do
            atoms=$(grep "^atoms=" "$meta" | cut -d= -f2)
            if [ -n "$atoms" ] && [ "$atoms" != "null" ]; then
                total_atoms=$((total_atoms + atoms))
                ((count++))
            fi
        done
        
        if [ $count -gt 0 ]; then
            local avg_atoms=$(echo "scale=1; $total_atoms / $count" | bc)
            echo "Average atoms per molecule: $avg_atoms"
        fi
        
        # Most common geometries
        echo ""
        echo "Geometry distribution:"
        grep "^geometry=" "$LOG_DIR/success"/*.meta 2>/dev/null | cut -d= -f2 | sort | uniq -c | sort -rn | head -5 | while read count geom; do
            echo "  $geom: $count"
        done
    fi
    
    # Analyze failures
    if [ $failed_count -gt 0 ]; then
        echo ""
        echo "═══ Failure Analysis ═══"
        echo "Error types:"
        grep "^error_type=" "$LOG_DIR/failed"/*.meta 2>/dev/null | cut -d= -f2 | sort | uniq -c | sort -rn | while read count error; do
            echo "  $error: $count"
        done
    fi
}

generate_report() {
    local report_file="$REPORT_DIR/report_${TIMESTAMP}.md"
    
    echo "Generating report: $report_file"
    
    {
        echo "# VSEPR-Sim Build Report"
        echo ""
        echo "**Generated:** $(date '+%Y-%m-%d %H:%M:%S')"
        echo ""
        
        # Overall stats
        local success_count=$(find "$LOG_DIR/success" -name "*.meta" 2>/dev/null | wc -l)
        local failed_count=$(find "$LOG_DIR/failed" -name "*.meta" 2>/dev/null | wc -l)
        local total=$((success_count + failed_count))
        
        echo "## Summary"
        echo ""
        echo "| Metric | Value |"
        echo "|--------|-------|"
        echo "| Total Builds | $total |"
        echo "| Successful | $success_count |"
        echo "| Failed | $failed_count |"
        
        if [ $total -gt 0 ]; then
            echo "| Success Rate | $(echo "scale=1; $success_count * 100 / $total" | bc)% |"
        fi
        
        echo ""
        echo "## Recent Successful Builds"
        echo ""
        echo "| Formula | Atoms | Bonds | Geometry | Duration |"
        echo "|---------|-------|-------|----------|----------|"
        
        find "$LOG_DIR/success" -name "*.meta" -type f | sort -r | head -10 | while read meta; do
            formula=$(grep "^formula=" "$meta" | cut -d= -f2)
            atoms=$(grep "^atoms=" "$meta" | cut -d= -f2)
            bonds=$(grep "^bonds=" "$meta" | cut -d= -f2)
            geometry=$(grep "^geometry=" "$meta" | cut -d= -f2)
            duration=$(grep "^duration=" "$meta" | cut -d= -f2)
            
            echo "| $formula | $atoms | $bonds | $geometry | ${duration}s |"
        done
        
        echo ""
        echo "## Recent Failures"
        echo ""
        echo "| Formula | Error Type |"
        echo "|---------|------------|"
        
        find "$LOG_DIR/failed" -name "*.meta" -type f | sort -r | head -10 | while read meta; do
            formula=$(grep "^formula=" "$meta" | cut -d= -f2)
            error=$(grep "^error_type=" "$meta" | cut -d= -f2)
            
            echo "| $formula | $error |"
        done
        
    } > "$report_file"
    
    echo -e "${GREEN}Report saved to: $report_file${NC}"
    
    # Also output to terminal
    cat "$report_file"
}

export_json() {
    local json_file="$REPORT_DIR/builds_${TIMESTAMP}.json"
    
    echo "Exporting to JSON: $json_file"
    
    {
        echo "{"
        echo "  \"timestamp\": \"$(date -Iseconds)\","
        echo "  \"builds\": ["
        
        local first=true
        for meta in "$LOG_DIR"/success/*.meta "$LOG_DIR"/failed/*.meta; do
            [ ! -f "$meta" ] && continue
            
            if [ "$first" = true ]; then
                first=false
            else
                echo ","
            fi
            
            echo "    {"
            
            while IFS='=' read -r key value; do
                echo "      \"$key\": \"$value\","
            done < "$meta" | sed '$ s/,$//'
            
            echo -n "    }"
        done
        
        echo ""
        echo "  ]"
        echo "}"
    } > "$json_file"
    
    echo -e "${GREEN}JSON exported to: $json_file${NC}"
}

###############################################################################
# Cleanup
###############################################################################

clean_logs() {
    local days=${1:-7}
    
    echo "Cleaning logs older than $days days..."
    
    find "$LOG_DIR" -type f -name "*.log" -mtime +$days -delete
    find "$LOG_DIR" -type f -name "*.meta" -mtime +$days -delete
    
    echo -e "${GREEN}Cleanup complete${NC}"
}

###############################################################################
# Main Entry Point
###############################################################################

init_logger

case "${1:-help}" in
    build)
        shift
        log_build "$@"
        ;;
    
    test)
        batch_test "${2:-formulas.txt}"
        ;;
    
    analyze)
        analyze_logs
        ;;
    
    report)
        generate_report
        ;;
    
    json)
        export_json
        ;;
    
    clean)
        clean_logs "${2:-7}"
        ;;
    
    help|--help|-h)
        echo "VSEPR-Sim Output Logger"
        echo ""
        echo "Usage: $0 <command> [args]"
        echo ""
        echo "Commands:"
        echo "  build <formula> [optimize]  Build molecule with logging"
        echo "  test <file>                 Batch test from file"
        echo "  analyze                     Analyze existing logs"
        echo "  report                      Generate markdown report"
        echo "  json                        Export logs to JSON"
        echo "  clean [days]                Clean logs older than N days (default: 7)"
        echo "  help                        Show this help"
        echo ""
        echo "Examples:"
        echo "  $0 build H2O true           # Build water with optimization"
        echo "  $0 test molecules.txt       # Batch test"
        echo "  $0 analyze                  # Analyze all logs"
        echo "  $0 clean 30                 # Clean logs older than 30 days"
        ;;
    
    *)
        echo "Unknown command: $1"
        echo "Use '$0 help' for usage information"
        exit 1
        ;;
esac
