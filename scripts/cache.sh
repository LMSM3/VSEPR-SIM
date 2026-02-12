#!/usr/bin/env bash
################################################################################
# VSEPR-Sim Cache Manager
# Manage geometry cache for fast lookups and reuse
################################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CACHE_DIR="$PROJECT_ROOT/data/cache"
CONFIG_FILE="$PROJECT_ROOT/data/paths.conf"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# ============================================================================
# Helper Functions
# ============================================================================

get_cache_path() {
    local formula=$1
    echo "$CACHE_DIR/${formula}.xyz"
}

cache_exists() {
    local formula=$1
    [ -f "$(get_cache_path "$formula")" ]
}

get_cache_info() {
    local formula=$1
    local cache_file=$(get_cache_path "$formula")
    
    if [ ! -f "$cache_file" ]; then
        echo "Not cached"
        return 1
    fi
    
    local size=$(stat -f%z "$cache_file" 2>/dev/null || stat -c%s "$cache_file" 2>/dev/null)
    local date=$(stat -f%Sm -t "%Y-%m-%d %H:%M" "$cache_file" 2>/dev/null || stat -c%y "$cache_file" 2>/dev/null | cut -d' ' -f1,2)
    local atoms=$(head -n1 "$cache_file")
    
    echo "Cached: $date | Atoms: $atoms | Size: $size bytes"
}

# ============================================================================
# Commands
# ============================================================================

cmd_list() {
    echo -e "${CYAN}Cached Geometries:${NC}"
    echo ""
    
    if [ ! -d "$CACHE_DIR" ] || [ -z "$(ls -A "$CACHE_DIR" 2>/dev/null)" ]; then
        echo "  (cache empty)"
        return
    fi
    
    printf "%-15s %-10s %-20s %-15s\n" "Formula" "Atoms" "Date" "Size"
    echo "────────────────────────────────────────────────────────────────"
    
    for xyz in "$CACHE_DIR"/*.xyz; do
        [ -f "$xyz" ] || continue
        
        local formula=$(basename "$xyz" .xyz)
        local atoms=$(head -n1 "$xyz")
        local size=$(stat -f%z "$xyz" 2>/dev/null || stat -c%s "$xyz" 2>/dev/null)
        local date=$(stat -f%Sm -t "%Y-%m-%d" "$xyz" 2>/dev/null || stat -c%y "$xyz" 2>/dev/null | cut -d' ' -f1)
        
        printf "%-15s %-10s %-20s %-15s\n" "$formula" "$atoms" "$date" "$size bytes"
    done
}

cmd_info() {
    local formula=$1
    
    if [ -z "$formula" ]; then
        echo "Usage: cache.sh info <formula>"
        exit 1
    fi
    
    local cache_file=$(get_cache_path "$formula")
    
    if [ ! -f "$cache_file" ]; then
        echo -e "${YELLOW}$formula not in cache${NC}"
        exit 1
    fi
    
    echo -e "${CYAN}Cache Info: $formula${NC}"
    echo ""
    echo "  File: $cache_file"
    
    local atoms=$(head -n1 "$cache_file")
    echo "  Atoms: $atoms"
    
    local comment=$(sed -n '2p' "$cache_file")
    echo "  Comment: $comment"
    
    local size=$(stat -f%z "$cache_file" 2>/dev/null || stat -c%s "$cache_file" 2>/dev/null)
    echo "  Size: $size bytes"
    
    local date=$(stat -f%Sm -t "%Y-%m-%d %H:%M:%S" "$cache_file" 2>/dev/null || stat -c%y "$cache_file" 2>/dev/null)
    echo "  Cached: $date"
}

cmd_get() {
    local formula=$1
    local output=$2
    
    if [ -z "$formula" ]; then
        echo "Usage: cache.sh get <formula> [output_file]"
        exit 1
    fi
    
    if ! cache_exists "$formula"; then
        echo -e "${YELLOW}$formula not in cache${NC}"
        exit 1
    fi
    
    local cache_file=$(get_cache_path "$formula")
    
    if [ -z "$output" ]; then
        cat "$cache_file"
    else
        cp "$cache_file" "$output"
        echo -e "${GREEN}Exported $formula to $output${NC}"
    fi
}

cmd_add() {
    local input=$1
    local formula=$2
    
    if [ -z "$input" ]; then
        echo "Usage: cache.sh add <input.xyz> [formula]"
        exit 1
    fi
    
    if [ ! -f "$input" ]; then
        echo "Error: Input file not found: $input"
        exit 1
    fi
    
    # Auto-detect formula from filename if not provided
    if [ -z "$formula" ]; then
        formula=$(basename "$input" .xyz)
    fi
    
    local cache_file=$(get_cache_path "$formula")
    
    cp "$input" "$cache_file"
    echo -e "${GREEN}Added $formula to cache${NC}"
}

cmd_remove() {
    local formula=$1
    
    if [ -z "$formula" ]; then
        echo "Usage: cache.sh remove <formula>"
        exit 1
    fi
    
    local cache_file=$(get_cache_path "$formula")
    
    if [ ! -f "$cache_file" ]; then
        echo -e "${YELLOW}$formula not in cache${NC}"
        exit 1
    fi
    
    rm "$cache_file"
    echo -e "${GREEN}Removed $formula from cache${NC}"
}

cmd_clear() {
    echo -n "Clear entire cache? [y/N] "
    read -r confirm
    
    if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
        echo "Aborted"
        exit 0
    fi
    
    rm -f "$CACHE_DIR"/*.xyz
    echo -e "${GREEN}Cache cleared${NC}"
}

cmd_stats() {
    echo -e "${CYAN}Cache Statistics:${NC}"
    echo ""
    
    local count=$(find "$CACHE_DIR" -name "*.xyz" 2>/dev/null | wc -l)
    local total_size=$(find "$CACHE_DIR" -name "*.xyz" -exec stat -f%z {} + 2>/dev/null | awk '{s+=$1} END {print s}')
    
    if [ -z "$total_size" ]; then
        total_size=$(find "$CACHE_DIR" -name "*.xyz" -exec stat -c%s {} + 2>/dev/null | awk '{s+=$1} END {print s}')
    fi
    
    local size_mb=$((total_size / 1048576))
    
    echo "  Entries: $count"
    echo "  Total Size: ${size_mb} MB"
    echo "  Location: $CACHE_DIR"
}

cmd_help() {
    cat <<EOF
VSEPR-Sim Cache Manager

Usage: cache.sh <command> [args]

Commands:
  list                List all cached geometries
  info <formula>      Show detailed info for a cache entry
  get <formula> [out] Get cached geometry (print or export)
  add <file> [name]   Add geometry to cache
  remove <formula>    Remove geometry from cache
  clear               Clear entire cache
  stats               Show cache statistics
  help                Show this help

Examples:
  cache.sh list
  cache.sh info H2O
  cache.sh get H2O outputs/water.xyz
  cache.sh add molecule.xyz H2O
  cache.sh remove CH4
  cache.sh stats
EOF
}

# ============================================================================
# Main
# ============================================================================

COMMAND=${1:-help}
shift || true

case $COMMAND in
    list) cmd_list ;;
    info) cmd_info "$@" ;;
    get) cmd_get "$@" ;;
    add) cmd_add "$@" ;;
    remove|rm) cmd_remove "$@" ;;
    clear) cmd_clear ;;
    stats) cmd_stats ;;
    help|--help|-h) cmd_help ;;
    *)
        echo "Unknown command: $COMMAND"
        echo "Run 'cache.sh help' for usage"
        exit 1
        ;;
esac
