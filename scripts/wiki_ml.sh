#!/usr/bin/env bash
################################################################################
# VSEPR-Sim Wiki Discovery ML
# Simple ML system to learn and suggest Wikipedia pages for new molecules
################################################################################

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

WIKI_DB="$PROJECT_ROOT/data/wiki/common_chemicals.txt"
LEARNING_DB="$PROJECT_ROOT/data/wiki/learned.txt"
PATTERNS_DB="$PROJECT_ROOT/data/wiki/patterns.txt"

# Create learning database
touch "$LEARNING_DB"
touch "$PATTERNS_DB"

# ============================================================================
# Pattern Recognition Functions
# ============================================================================

extract_features() {
    local formula=$1
    
    # Count elements
    local num_C=$(echo "$formula" | grep -o 'C' | wc -l)
    local num_H=$(echo "$formula" | grep -o 'H' | wc -l)
    local num_O=$(echo "$formula" | grep -o 'O' | wc -l)
    local num_N=$(echo "$formula" | grep -o 'N' | wc -l)
    local num_F=$(echo "$formula" | grep -o 'F' | wc -l)
    local num_Cl=$(echo "$formula" | grep -o 'Cl' | wc -l)
    
    # Classify by pattern
    if [ $num_C -gt 0 ] && [ $num_H -gt 0 ]; then
        echo "organic"
    elif [ $num_F -gt 0 ] || [ $num_Cl -gt 0 ]; then
        echo "halogen"
    elif [ $num_O -gt 0 ] && [ $num_H -gt 0 ]; then
        echo "oxide_hydride"
    else
        echo "inorganic"
    fi
}

suggest_wiki_search() {
    local formula=$1
    local category=$(extract_features "$formula")
    
    # Build search term based on category
    case "$category" in
        organic)
            echo "https://en.wikipedia.org/w/index.php?search=${formula}+organic+compound"
            ;;
        halogen)
            echo "https://en.wikipedia.org/w/index.php?search=${formula}+fluoride+chloride"
            ;;
        oxide_hydride)
            echo "https://en.wikipedia.org/w/index.php?search=${formula}+oxide+hydroxide"
            ;;
        *)
            echo "https://en.wikipedia.org/w/index.php?search=${formula}+chemical"
            ;;
    esac
}

learn_from_success() {
    local formula=$1
    local name=$2
    local url=$3
    
    # Add to learned database
    echo "${formula}|${name}|${url}" >> "$LEARNING_DB"
    
    # Extract pattern
    local category=$(extract_features "$formula")
    echo "${category}|${formula}|${name}" >> "$PATTERNS_DB"
}

# ============================================================================
# Commands
# ============================================================================

case "${1:-help}" in
    suggest)
        formula="${2:-}"
        if [ -z "$formula" ]; then
            echo "Usage: wiki_ml.sh suggest <formula>"
            exit 1
        fi
        
        # Check if already known
        if grep -qi "^${formula}|" "$WIKI_DB" "$LEARNING_DB" 2>/dev/null; then
            echo "✓ Formula already in database"
            bash "$PROJECT_ROOT/scripts/wiki.sh" lookup "$formula"
            exit 0
        fi
        
        # Suggest search
        category=$(extract_features "$formula")
        search_url=$(suggest_wiki_search "$formula")
        
        echo "Formula: $formula"
        echo "Category: $category"
        echo "Suggested search: $search_url"
        
        # Auto-open search
        if command -v xdg-open &> /dev/null; then
            xdg-open "$search_url" &>/dev/null &
        elif [ -f "/mnt/c/Windows/System32/cmd.exe" ]; then
            /mnt/c/Windows/System32/cmd.exe /c start "$search_url" &>/dev/null &
        fi
        ;;
        
    learn)
        formula="${2:-}"
        name="${3:-}"
        url="${4:-}"
        
        if [ -z "$formula" ] || [ -z "$name" ] || [ -z "$url" ]; then
            echo "Usage: wiki_ml.sh learn <formula> <name> <url>"
            echo "Example: wiki_ml.sh learn 'C2H6' 'Ethane' 'https://en.wikipedia.org/wiki/Ethane'"
            exit 1
        fi
        
        learn_from_success "$formula" "$name" "$url"
        echo "✓ Learned: $formula → $name"
        ;;
        
    stats)
        total_known=$(grep -v '^#' "$WIKI_DB" | grep -v '^$' | wc -l)
        total_learned=$(wc -l < "$LEARNING_DB" 2>/dev/null || echo 0)
        total_patterns=$(wc -l < "$PATTERNS_DB" 2>/dev/null || echo 0)
        
        echo "Wiki ML Statistics:"
        echo "  Known chemicals: $total_known"
        echo "  Learned chemicals: $total_learned"
        echo "  Pattern entries: $total_patterns"
        echo ""
        
        if [ $total_patterns -gt 0 ]; then
            echo "Pattern distribution:"
            cut -d'|' -f1 "$PATTERNS_DB" | sort | uniq -c | while read count pattern; do
                printf "  %-20s %d\n" "$pattern" "$count"
            done
        fi
        ;;
        
    patterns)
        echo "Recognized patterns:"
        if [ -s "$PATTERNS_DB" ]; then
            cat "$PATTERNS_DB" | while IFS='|' read category formula name; do
                printf "  [%-15s] %-15s → %s\n" "$category" "$formula" "$name"
            done
        else
            echo "  (No patterns learned yet)"
        fi
        ;;
        
    *)
        echo "VSEPR-Sim Wiki Discovery ML"
        echo ""
        echo "Simple machine learning system for chemical name discovery"
        echo ""
        echo "Commands:"
        echo "  suggest <formula>              - Suggest wiki search for unknown formula"
        echo "  learn <formula> <name> <url>   - Add new chemical to learned database"
        echo "  stats                          - Show learning statistics"
        echo "  patterns                       - Show learned patterns"
        echo ""
        echo "Examples:"
        echo "  wiki_ml.sh suggest 'C6H12'"
        echo "  wiki_ml.sh learn 'C2H6' 'Ethane' 'https://en.wikipedia.org/wiki/Ethane'"
        echo "  wiki_ml.sh stats"
        ;;
esac
