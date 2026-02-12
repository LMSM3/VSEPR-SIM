#!/usr/bin/env bash
################################################################################
# VSEPR-Sim Universal Cleanup Script
# Removes default/placeholder files and restores clean state
################################################################################

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${CYAN}${BOLD}"
cat << 'EOF'
╔════════════════════════════════════════════════════════════════╗
║  VSEPR-Sim Universal Cleanup                                  ║
║  Removing default/placeholder files                           ║
╚════════════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

REMOVED=0
KEPT=0

# Function to remove file if it exists
remove_if_exists() {
    local file="$1"
    local reason="$2"
    
    if [[ -f "$file" ]]; then
        echo -e "${YELLOW}[REMOVE]${NC} $file - $reason"
        rm "$file"
        ((REMOVED++))
    fi
}

# Function to remove directory if it exists
remove_dir_if_exists() {
    local dir="$1"
    local reason="$2"
    
    if [[ -d "$dir" ]]; then
        echo -e "${YELLOW}[REMOVE]${NC} $dir/ - $reason"
        rm -rf "$dir"
        ((REMOVED++))
    fi
}

echo ""
echo "Cleaning up default/placeholder files..."
echo ""

# Remove Windows-specific line ending artifacts
echo "Removing line ending artifacts..."
find . -name "*.sh" -type f -exec sed -i 's/\r$//' {} \; 2>/dev/null || true

# Remove CMake generated files (not universal)
remove_dir_if_exists "CMakeFiles" "CMake generated"
remove_if_exists "CMakeCache.txt" "CMake cache"
remove_if_exists "cmake_install.cmake" "CMake installer"
remove_if_exists "Makefile" "Generated makefile"

# Remove IDE-specific files
remove_dir_if_exists ".vs" "Visual Studio cache"
remove_dir_if_exists ".vscode" "VSCode settings (should be in gitignore)"
remove_if_exists "compile_commands.json" "Compilation database"

# Remove build artifacts that shouldn't be versioned
remove_dir_if_exists "build/CMakeFiles" "CMake build cache"
remove_if_exists "build/CMakeCache.txt" "CMake build cache"
remove_if_exists "build/cmake_install.cmake" "CMake build installer"
remove_if_exists "build/Makefile" "Generated makefile"

# Remove Python cache
remove_dir_if_exists "__pycache__" "Python cache"
remove_dir_if_exists ".pytest_cache" "Pytest cache"
find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true

# Remove temporary files
find . -name "*.tmp" -type f -delete 2>/dev/null || true
find . -name "*.bak" -type f -delete 2>/dev/null || true
find . -name "*~" -type f -delete 2>/dev/null || true

# Remove empty directories
find . -type d -empty -delete 2>/dev/null || true

echo ""
echo -e "${GREEN}${BOLD}Cleanup Complete!${NC}"
echo ""
echo "Removed: $REMOVED items"
echo ""
echo "Universal build system restored:"
echo "  ✓ Cross-platform scripts"
echo "  ✓ No platform-specific artifacts"
echo "  ✓ Clean repository state"
echo ""
