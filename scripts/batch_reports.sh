#!/usr/bin/env bash
################################################################################
# VSEPR-Sim Automated Batch Report Generator
# Version: 2.3.1
#
# Runs multiple discovery runs with different seeds and generates
# comparative reports for analysis
################################################################################

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV="$ROOT/.venv"
BATCH_DIR="$ROOT/outputs/batch_$(date +%Y%m%d_%H%M%S)"
NUM_RUNS=${1:-10}

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

print_banner() {
    clear
    echo -e "${CYAN}${BOLD}"
    cat << 'EOF'
╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║     VSEPR-Sim Automated Batch Report Generator v2.3.1        ║
║     Multiple Discovery Runs with Comparative Analysis        ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
}

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    if [[ ! -f "$ROOT/build/bin/vsepr" ]]; then
        log_error "vsepr binary not found. Run: ./scripts/build_stable.sh"
        exit 1
    fi
    
    if [[ ! -d "$VENV" ]]; then
        log_error "Python venv not found. Run: ./scripts/install_reporting.sh"
        exit 1
    fi
    
    log_success "Prerequisites OK"
}

# Create batch directory structure
setup_batch_dir() {
    log_info "Creating batch directory: $BATCH_DIR"
    
    mkdir -p "$BATCH_DIR"/{runs,reports,analysis}
    
    # Create metadata file
    cat > "$BATCH_DIR/metadata.json" << EOF
{
  "batch_id": "$(basename $BATCH_DIR)",
  "timestamp": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "num_runs": $NUM_RUNS,
  "vsepr_version": "2.3.1",
  "git_commit": "$(git rev-parse HEAD 2>/dev/null || echo 'unknown')"
}
EOF
    
    log_success "Batch directory created"
}

# Run single discovery with seed
run_discovery() {
    local seed=$1
    local run_id=$(printf "run_%03d" $seed)
    local run_dir="$BATCH_DIR/runs/$run_id"
    
    mkdir -p "$run_dir"
    
    log_info "Running discovery #$seed (seed=$seed)..."
    
    # Run discovery
    "$ROOT/build/bin/vsepr" build discover 100 --thermal --seed $seed \
        > "$run_dir/output.log" 2>&1 || {
        log_warning "Discovery #$seed had errors (continuing)"
    }
    
    # Parse results
    grep "Accepted" "$run_dir/output.log" > "$run_dir/accepted.txt" 2>/dev/null || touch "$run_dir/accepted.txt"
    grep "Rejected" "$run_dir/output.log" > "$run_dir/rejected.txt" 2>/dev/null || touch "$run_dir/rejected.txt"
    grep "HGST" "$run_dir/output.log" > "$run_dir/hgst.txt" 2>/dev/null || touch "$run_dir/hgst.txt"
    
    # Extract statistics
    local accepted=$(grep -c "Accepted" "$run_dir/output.log" 2>/dev/null || echo 0)
    local total=100
    local success_rate=$(awk "BEGIN {printf \"%.1f\", ($accepted/$total)*100}")
    
    # Create run summary
    cat > "$run_dir/summary.json" << EOF
{
  "seed": $seed,
  "total": $total,
  "accepted": $accepted,
  "rejected": $((total - accepted)),
  "success_rate": $success_rate,
  "timestamp": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF
    
    log_success "Discovery #$seed complete: $accepted/$total accepted ($success_rate%)"
}

# Run all discoveries in parallel
run_all_discoveries() {
    log_info "Running $NUM_RUNS discovery runs..."
    echo ""
    
    local pids=()
    
    for i in $(seq 1 $NUM_RUNS); do
        run_discovery $i &
        pids+=($!)
        
        # Limit parallelism
        if [ $(jobs -r | wc -l) -ge 4 ]; then
            wait -n
        fi
    done
    
    # Wait for all
    log_info "Waiting for all runs to complete..."
    wait
    
    echo ""
    log_success "All $NUM_RUNS runs complete"
}

# Generate individual reports
generate_individual_reports() {
    log_info "Generating individual reports..."
    
    # shellcheck disable=SC1091
    source "$VENV/bin/activate"
    
    for i in $(seq 1 $NUM_RUNS); do
        local run_id=$(printf "run_%03d" $i)
        local run_dir="$BATCH_DIR/runs/$run_id"
        local report_dir="$BATCH_DIR/reports/$run_id"
        
        mkdir -p "$report_dir"
        
        # Generate report for this run
        python "$ROOT/reporting/generate_report.py" \
            --out "$report_dir" \
            --seed $i \
            2>/dev/null || log_warning "Report generation failed for run #$i"
        
        log_info "Generated report for run #$i"
    done
    
    log_success "Individual reports generated"
}

# Create comparative analysis
generate_comparative_analysis() {
    log_info "Generating comparative analysis..."
    
    # shellcheck disable=SC1091
    source "$VENV/bin/activate"
    
    # Create Python analysis script
    cat > "$BATCH_DIR/analysis/analyze_batch.py" << 'PYEOF'
#!/usr/bin/env python3
import json
import glob
import os
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

# Set style
sns.set_style('whitegrid')

# Load all summaries
summaries = []
for summary_file in sorted(glob.glob('../runs/*/summary.json')):
    with open(summary_file) as f:
        summaries.append(json.load(f))

# Extract data
seeds = [s['seed'] for s in summaries]
success_rates = [s['success_rate'] for s in summaries]
accepted = [s['accepted'] for s in summaries]

# Statistics
avg_success = np.mean(success_rates)
std_success = np.std(success_rates)
min_success = np.min(success_rates)
max_success = np.max(success_rates)

# Plot 1: Success rates
fig, axes = plt.subplots(2, 2, figsize=(16, 12))

# Success rate distribution
axes[0, 0].bar(seeds, success_rates, color='#3498db', alpha=0.7, edgecolor='black')
axes[0, 0].axhline(avg_success, color='red', linestyle='--', linewidth=2, label=f'Mean: {avg_success:.1f}%')
axes[0, 0].set_xlabel('Run Number (Seed)', fontsize=12)
axes[0, 0].set_ylabel('Success Rate (%)', fontsize=12)
axes[0, 0].set_title('Success Rate by Run', fontsize=14, fontweight='bold')
axes[0, 0].legend()
axes[0, 0].grid(axis='y', alpha=0.3)

# Histogram
axes[0, 1].hist(success_rates, bins=10, color='#2ecc71', alpha=0.7, edgecolor='black')
axes[0, 1].axvline(avg_success, color='red', linestyle='--', linewidth=2, label=f'Mean: {avg_success:.1f}%')
axes[0, 1].set_xlabel('Success Rate (%)', fontsize=12)
axes[0, 1].set_ylabel('Frequency', fontsize=12)
axes[0, 1].set_title('Success Rate Distribution', fontsize=14, fontweight='bold')
axes[0, 1].legend()
axes[0, 1].grid(axis='y', alpha=0.3)

# Cumulative
axes[1, 0].plot(seeds, np.cumsum(accepted), 'o-', color='#9b59b6', linewidth=2)
axes[1, 0].set_xlabel('Run Number', fontsize=12)
axes[1, 0].set_ylabel('Cumulative Accepted Molecules', fontsize=12)
axes[1, 0].set_title('Cumulative Discovery Progress', fontsize=14, fontweight='bold')
axes[1, 0].grid(alpha=0.3)

# Box plot
axes[1, 1].boxplot([success_rates], vert=True, labels=['All Runs'])
axes[1, 1].set_ylabel('Success Rate (%)', fontsize=12)
axes[1, 1].set_title('Success Rate Statistics', fontsize=14, fontweight='bold')
axes[1, 1].grid(axis='y', alpha=0.3)

plt.tight_layout()
plt.savefig('comparative_analysis.png', dpi=300, bbox_inches='tight')
print("[✓] Plot saved: comparative_analysis.png")

# Generate summary report
report = f"""
╔═══════════════════════════════════════════════════════════════╗
║  Batch Discovery Analysis - {len(summaries)} Runs                       ║
╚═══════════════════════════════════════════════════════════════╝

OVERALL STATISTICS
------------------
Total Runs:        {len(summaries)}
Total Attempts:    {len(summaries) * 100}
Total Accepted:    {sum(accepted)}
Overall Success:   {sum(accepted) / (len(summaries) * 100) * 100:.1f}%

SUCCESS RATE STATISTICS
-----------------------
Mean:    {avg_success:.1f}%
Std Dev: {std_success:.1f}%
Min:     {min_success:.1f}% (Run #{seeds[success_rates.index(min_success)]})
Max:     {max_success:.1f}% (Run #{seeds[success_rates.index(max_success)]})

RUN-BY-RUN BREAKDOWN
--------------------
"""

for s in summaries:
    report += f"Run {s['seed']:>2d}: {s['accepted']:>2d}/100 ({s['success_rate']:>5.1f}%)\n"

with open('batch_summary.txt', 'w') as f:
    f.write(report)

print("[✓] Summary saved: batch_summary.txt")
print("\n" + report)
PYEOF
    
    chmod +x "$BATCH_DIR/analysis/analyze_batch.py"
    
    # Run analysis
    cd "$BATCH_DIR/analysis"
    python analyze_batch.py
    cd "$ROOT"
    
    log_success "Comparative analysis complete"
}

# Generate master report
generate_master_report() {
    log_info "Generating master report..."
    
    cat > "$BATCH_DIR/BATCH_REPORT.md" << EOF
# VSEPR-Sim Batch Discovery Report

**Batch ID**: $(basename $BATCH_DIR)

**Generated**: $(date '+%Y-%m-%d %H:%M:%S')

---

## Executive Summary

Automated batch discovery run with $NUM_RUNS independent runs (100 molecules each).

---

## Configuration

- **Number of Runs**: $NUM_RUNS
- **Molecules per Run**: 100
- **Total Attempts**: $((NUM_RUNS * 100))
- **Seeds**: 1 through $NUM_RUNS
- **VSEPR Version**: 2.3.1
- **Git Commit**: $(git rev-parse HEAD 2>/dev/null | cut -c1-8 || echo 'unknown')

---

## Results

See \`analysis/batch_summary.txt\` for detailed statistics.

See \`analysis/comparative_analysis.png\` for visualization.

---

## Individual Run Reports

EOF
    
    for i in $(seq 1 $NUM_RUNS); do
        local run_id=$(printf "run_%03d" $i)
        echo "- Run #$i: \`reports/$run_id/\`" >> "$BATCH_DIR/BATCH_REPORT.md"
    done
    
    cat >> "$BATCH_DIR/BATCH_REPORT.md" << EOF

---

## Reproducibility

To reproduce any individual run:

\`\`\`bash
./build/bin/vsepr build discover 100 --thermal --seed N
\`\`\`

Where N is the run number (1-$NUM_RUNS).

---

**Batch Processing Complete**: $(date)
EOF
    
    log_success "Master report generated: $BATCH_DIR/BATCH_REPORT.md"
}

# Main execution
main() {
    print_banner
    
    log_info "Batch Discovery Configuration:"
    echo "  Number of runs: $NUM_RUNS"
    echo "  Molecules per run: 100"
    echo "  Total attempts: $((NUM_RUNS * 100))"
    echo "  Output directory: $BATCH_DIR"
    echo ""
    
    read -p "Continue? (y/n): " -n 1 -r
    echo
    
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_warning "Batch processing cancelled"
        exit 0
    fi
    
    echo ""
    
    check_prerequisites
    setup_batch_dir
    echo ""
    
    run_all_discoveries
    echo ""
    
    generate_individual_reports
    echo ""
    
    generate_comparative_analysis
    echo ""
    
    generate_master_report
    echo ""
    
    # Final summary
    echo -e "${GREEN}${BOLD}"
    cat << 'EOF'
╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║  ✅ Batch Report Generation Complete!                         ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
    
    echo -e "${CYAN}Results Location:${NC}"
    echo "  $BATCH_DIR/"
    echo ""
    echo -e "${CYAN}Key Files:${NC}"
    echo "  BATCH_REPORT.md          # Master report"
    echo "  analysis/batch_summary.txt        # Statistics"
    echo "  analysis/comparative_analysis.png # Visualization"
    echo "  reports/run_*/           # Individual reports"
    echo ""
    echo -e "${CYAN}View Results:${NC}"
    echo "  cat $BATCH_DIR/analysis/batch_summary.txt"
    echo "  xdg-open $BATCH_DIR/analysis/comparative_analysis.png"
    echo ""
}

# Run main
main "$@"
