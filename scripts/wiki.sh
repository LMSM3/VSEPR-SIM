#!/usr/bin/env bash
################################################################################
# VSEPR-Sim Wiki Integration
# Lookup chemical formulas and open Wikipedia pages
################################################################################

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

WIKI_DB="$PROJECT_ROOT/data/wiki/common_chemicals.txt"
WIKI_CACHE="$PROJECT_ROOT/data/wiki/cache"

# Create cache directory
mkdir -p "$WIKI_CACHE"

# ============================================================================
# Functions
# ============================================================================

normalize_formula() {
    # Remove spaces, normalize case
    echo "$1" | tr -d '[:space:]' | sed 's/([^)]*)//g'
}

lookup_wiki() {
    local formula=$1
    local normalized=$(normalize_formula "$formula")
    
    # Check cache first
    local cache_file="$WIKI_CACHE/${normalized}.url"
    if [ -f "$cache_file" ]; then
        cat "$cache_file"
        return 0
    fi
    
    # Search wiki database
    local result=$(grep -i "^${normalized}|" "$WIKI_DB" 2>/dev/null | head -n 1)
    
    if [ -n "$result" ]; then
        local name=$(echo "$result" | cut -d'|' -f2)
        local url=$(echo "$result" | cut -d'|' -f3)
        
        # Cache the result
        echo "$url" > "$cache_file"
        echo "$name|$url"
        return 0
    fi
    
    return 1
}

open_wiki() {
    local url=$1
    
    # Detect platform and open browser
    if command -v xdg-open &> /dev/null; then
        xdg-open "$url" &>/dev/null &
    elif command -v open &> /dev/null; then
        open "$url" &>/dev/null &
    elif command -v cmd.exe &> /dev/null; then
        cmd.exe /c start "$url" &>/dev/null &
    elif [ -f "/mnt/c/Windows/System32/cmd.exe" ]; then
        /mnt/c/Windows/System32/cmd.exe /c start "$url" &>/dev/null &
    else
        echo "Could not detect browser opener"
        return 1
    fi
}

# ============================================================================
# Main Commands
# ============================================================================

case "${1:-lookup}" in
    lookup)
        formula="${2:-}"
        if [ -z "$formula" ]; then
            echo "Usage: wiki.sh lookup <formula>"
            exit 1
        fi
        
        result=$(lookup_wiki "$formula")
        if [ $? -eq 0 ]; then
            name=$(echo "$result" | cut -d'|' -f1)
            url=$(echo "$result" | cut -d'|' -f2)
            echo "✓ Found: $name"
            echo "  URL: $url"
        else
            echo "⊘ No Wikipedia entry found for: $formula"
            exit 1
        fi
        ;;
        
    open)
        formula="${2:-}"
        if [ -z "$formula" ]; then
            echo "Usage: wiki.sh open <formula>"
            exit 1
        fi
        
        result=$(lookup_wiki "$formula")
        if [ $? -eq 0 ]; then
            name=$(echo "$result" | cut -d'|' -f1)
            url=$(echo "$result" | cut -d'|' -f2)
            echo "Opening Wikipedia: $name"
            open_wiki "$url"
        else
            echo "⊘ No Wikipedia entry found for: $formula"
            exit 1
        fi
        ;;
        
    auto)
        # Auto mode: lookup and open if found, silent if not
        formula="${2:-}"
        if [ -z "$formula" ]; then
            exit 1
        fi
        
        result=$(lookup_wiki "$formula")
        if [ $? -eq 0 ]; then
            url=$(echo "$result" | cut -d'|' -f2)
            open_wiki "$url"
            exit 0
        fi
        exit 1
        ;;
        
    list)
        echo "Common chemicals in database:"
        echo ""
        cat "$WIKI_DB" | grep -v '^#' | grep -v '^$' | while IFS='|' read formula name url; do
            printf "  %-15s %s\n" "$formula" "$name"
        done
        ;;
        
    stats)
        total=$(grep -v '^#' "$WIKI_DB" | grep -v '^$' | wc -l)
        cached=$(ls -1 "$WIKI_CACHE" 2>/dev/null | wc -l)
        echo "Wiki Database Statistics:"
        echo "  Total entries: $total"
        echo "  Cached lookups: $cached"
        ;;
        
    *)
        echo "VSEPR-Sim Wiki Integration"
        echo ""
        echo "Usage:"
        echo "  wiki.sh lookup <formula>   - Look up chemical in database"
        echo "  wiki.sh open <formula>     - Open Wikipedia page"
        echo "  wiki.sh auto <formula>     - Auto-open if found (silent)"
        echo "  wiki.sh list              - List all known chemicals"
        echo "  wiki.sh stats             - Show database statistics"
        echo ""
        echo "Examples:"
        echo "  wiki.sh lookup H2O"
        echo "  wiki.sh open NH3"
        echo "  wiki.sh list"
        ;;
esac
