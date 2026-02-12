#!/usr/bin/env bash
################################################################################
# VSEPR-Sim Session Manager
# Organize outputs by session for better data management
################################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SESSION_DIR="$PROJECT_ROOT/outputs/sessions"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

# ============================================================================
# Helper Functions
# ============================================================================

get_timestamp() {
    date +%Y%m%d_%H%M%S
}

get_latest_session() {
    if [ -L "$SESSION_DIR/latest" ]; then
        readlink "$SESSION_DIR/latest"
    else
        ls -t "$SESSION_DIR" | grep "^session_" | head -n1
    fi
}

# ============================================================================
# Commands
# ============================================================================

cmd_create() {
    local name=$1
    local timestamp=$(get_timestamp)
    local session_id="session_${timestamp}"
    local session_path="$SESSION_DIR/$session_id"
    
    # Create session directory structure
    mkdir -p "$session_path"/{molecules,visualizations,analysis}
    
    # Create session metadata
    cat > "$session_path/session.json" <<EOF
{
  "session_id": "$session_id",
  "name": "${name:-unnamed}",
  "created": "$(date -Iseconds)",
  "last_modified": "$(date -Iseconds)",
  "command_count": 0,
  "molecules_computed": 0,
  "status": "active"
}
EOF
    
    # Create commands log
    touch "$session_path/commands.log"
    
    # Create latest symlink
    ln -sf "$session_id" "$SESSION_DIR/latest"
    
    echo -e "${GREEN}Created session: $session_id${NC}"
    [ -n "$name" ] && echo "  Name: $name"
    echo "  Path: $session_path"
}

cmd_list() {
    echo -e "${CYAN}Sessions:${NC}"
    echo ""
    
    if [ ! -d "$SESSION_DIR" ] || [ -z "$(ls -A "$SESSION_DIR" 2>/dev/null)" ]; then
        echo "  (no sessions)"
        return
    fi
    
    printf "%-30s %-15s %-20s %-10s\n" "Session ID" "Name" "Created" "Molecules"
    echo "────────────────────────────────────────────────────────────────────────────"
    
    for session in "$SESSION_DIR"/session_*; do
        [ -d "$session" ] || continue
        
        local session_id=$(basename "$session")
        local json_file="$session/session.json"
        
        if [ -f "$json_file" ]; then
            local name=$(grep '"name"' "$json_file" | cut -d'"' -f4)
            local created=$(grep '"created"' "$json_file" | cut -d'"' -f4 | cut -dT -f1)
            local mol_count=$(find "$session/molecules" -name "*.xyz" 2>/dev/null | wc -l)
        else
            local name="unknown"
            local created="unknown"
            local mol_count=0
        fi
        
        local marker=""
        if [ -L "$SESSION_DIR/latest" ] && [ "$(readlink "$SESSION_DIR/latest")" = "$session_id" ]; then
            marker=" ${GREEN}(latest)${NC}"
        fi
        
        printf "%-30s %-15s %-20s %-10s%s\n" "$session_id" "$name" "$created" "$mol_count" "$marker"
    done
}

cmd_info() {
    local session_id=${1:-latest}
    
    if [ "$session_id" = "latest" ]; then
        session_id=$(get_latest_session)
    fi
    
    local session_path="$SESSION_DIR/$session_id"
    
    if [ ! -d "$session_path" ]; then
        echo -e "${YELLOW}Session not found: $session_id${NC}"
        exit 1
    fi
    
    echo -e "${CYAN}Session: $session_id${NC}"
    echo ""
    
    if [ -f "$session_path/session.json" ]; then
        cat "$session_path/session.json" | grep -v "^}" | grep -v "^{" | sed 's/^  //'
    fi
    
    echo ""
    echo -e "${CYAN}Contents:${NC}"
    
    local mol_count=$(find "$session_path/molecules" -name "*.xyz" 2>/dev/null | wc -l)
    local viz_count=$(find "$session_path/visualizations" -name "*.html" 2>/dev/null | wc -l)
    
    echo "  Molecules: $mol_count"
    echo "  Visualizations: $viz_count"
    echo "  Path: $session_path"
}

cmd_archive() {
    local session_id=${1:-latest}
    local archive_name=$2
    
    if [ "$session_id" = "latest" ]; then
        session_id=$(get_latest_session)
    fi
    
    local session_path="$SESSION_DIR/$session_id"
    
    if [ ! -d "$session_path" ]; then
        echo -e "${YELLOW}Session not found: $session_id${NC}"
        exit 1
    fi
    
    # Default archive name
    if [ -z "$archive_name" ]; then
        archive_name="${session_id}"
    fi
    
    local archive_dir="$PROJECT_ROOT/data/archive/$(date +%Y-%m)"
    mkdir -p "$archive_dir"
    
    local archive_file="$archive_dir/${archive_name}.tar.gz"
    
    echo "Archiving $session_id..."
    tar -czf "$archive_file" -C "$SESSION_DIR" "$session_id"
    
    # Create manifest
    cat > "$archive_dir/${archive_name}.json" <<EOF
{
  "archive_name": "$archive_name",
  "source_session": "$session_id",
  "archived_date": "$(date -Iseconds)",
  "archive_file": "$archive_file",
  "size_bytes": $(stat -f%z "$archive_file" 2>/dev/null || stat -c%s "$archive_file")
}
EOF
    
    echo -e "${GREEN}Archived to: $archive_file${NC}"
    
    # Ask to remove original
    echo -n "Remove original session? [y/N] "
    read -r confirm
    if [ "$confirm" = "y" ] || [ "$confirm" = "Y" ]; then
        rm -rf "$session_path"
        echo -e "${GREEN}Original session removed${NC}"
    fi
}

cmd_export() {
    local session_id=${1:-latest}
    local output_dir=$2
    
    if [ "$session_id" = "latest" ]; then
        session_id=$(get_latest_session)
    fi
    
    if [ -z "$output_dir" ]; then
        output_dir="."
    fi
    
    local session_path="$SESSION_DIR/$session_id"
    
    if [ ! -d "$session_path" ]; then
        echo -e "${YELLOW}Session not found: $session_id${NC}"
        exit 1
    fi
    
    tar -czf "${output_dir}/${session_id}.tar.gz" -C "$SESSION_DIR" "$session_id"
    echo -e "${GREEN}Exported to: ${output_dir}/${session_id}.tar.gz${NC}"
}

cmd_clean() {
    local days=${1:-30}
    
    echo "Cleaning sessions older than $days days..."
    
    local cutoff_date=$(date -d "$days days ago" +%s 2>/dev/null || date -v-${days}d +%s 2>/dev/null)
    local cleaned=0
    
    for session in "$SESSION_DIR"/session_*; do
        [ -d "$session" ] || continue
        
        local session_date=$(stat -f%B "$session" 2>/dev/null || stat -c%Y "$session" 2>/dev/null)
        
        if [ "$session_date" -lt "$cutoff_date" ]; then
            local session_id=$(basename "$session")
            echo "  Removing: $session_id"
            rm -rf "$session"
            ((cleaned++))
        fi
    done
    
    echo -e "${GREEN}Cleaned $cleaned old sessions${NC}"
}

cmd_help() {
    cat <<EOF
VSEPR-Sim Session Manager

Usage: session.sh <command> [args]

Commands:
  create [name]           Create new session
  list                    List all sessions
  info [session]          Show session details (default: latest)
  archive [session] [name] Archive session to data/archive/
  export [session] [dir]  Export session as tar.gz
  clean [days]            Remove sessions older than N days (default: 30)
  help                    Show this help

Examples:
  session.sh create "fluoride_study"
  session.sh list
  session.sh info latest
  session.sh archive latest fluoride_jan2026
  session.sh export latest ~/backups/
  session.sh clean 60
EOF
}

# ============================================================================
# Main
# ============================================================================

# Ensure session directory exists
mkdir -p "$SESSION_DIR"

COMMAND=${1:-help}
shift || true

case $COMMAND in
    create) cmd_create "$@" ;;
    list|ls) cmd_list ;;
    info) cmd_info "$@" ;;
    archive) cmd_archive "$@" ;;
    export) cmd_export "$@" ;;
    clean) cmd_clean "$@" ;;
    help|--help|-h) cmd_help ;;
    *)
        echo "Unknown command: $COMMAND"
        echo "Run 'session.sh help' for usage"
        exit 1
        ;;
esac
