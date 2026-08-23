#!/bin/bash
# check_dependencies.sh
# Verify OpenGL rendering dependencies (Arch/MSYS2/Debian)

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║${NC}  ${GREEN}OpenGL Dependency Checker${NC}                               ${BLUE}║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Detect package manager
if command -v pacman &>/dev/null; then
    PKG_MGR="pacman"
    echo -e "${BLUE}Detected:${NC} Arch/MSYS2 (pacman)"
elif command -v apt-get &>/dev/null; then
    PKG_MGR="apt"
    echo -e "${BLUE}Detected:${NC} Debian/Ubuntu (apt)"
elif command -v dnf &>/dev/null; then
    PKG_MGR="dnf"
    echo -e "${BLUE}Detected:${NC} Fedora/RHEL (dnf)"
else
    PKG_MGR="unknown"
    echo -e "${YELLOW}⚠️  Unknown package manager${NC}"
fi
echo ""

missing=()

# ============================================================================
# Check Dependencies (by package manager)
# ============================================================================

if [[ "$PKG_MGR" == "pacman" ]]; then
    echo -e "${YELLOW}Checking Arch/MSYS2 packages...${NC}"
    DEPS=("mesa" "glu" "glew" "glfw" "glm")
    
    for dep in "${DEPS[@]}"; do
        if pacman -Qi "$dep" &>/dev/null || pacman -Qi "mingw-w64-x86_64-$dep" &>/dev/null; then
            echo -e "  ${GREEN}✓${NC} $dep"
        else
            echo -e "  ${RED}❌${NC} $dep (missing)"
            missing+=("$dep")
        fi
    done
    
elif [[ "$PKG_MGR" == "apt" ]]; then
    echo -e "${YELLOW}Checking Debian/Ubuntu packages...${NC}"
    DEPS=(
        "libgl1-mesa-dev"
        "libglu1-mesa-dev"
        "libglew-dev"
        "libglfw3-dev"
        "libglm-dev"
        "freeglut3-dev"
    )
    
    for dep in "${DEPS[@]}"; do
        if dpkg -l | grep -q "^ii  $dep"; then
            echo -e "  ${GREEN}✓${NC} $dep"
        else
            echo -e "  ${RED}❌${NC} $dep (missing)"
            missing+=("$dep")
        fi
    done
    
elif [[ "$PKG_MGR" == "dnf" ]]; then
    echo -e "${YELLOW}Checking Fedora/RHEL packages...${NC}"
    DEPS=(
        "mesa-libGL-devel"
        "mesa-libGLU-devel"
        "glew-devel"
        "glfw-devel"
        "glm-devel"
        "freeglut-devel"
    )
    
    for dep in "${DEPS[@]}"; do
        if rpm -q "$dep" &>/dev/null; then
            echo -e "  ${GREEN}✓${NC} $dep"
        else
            echo -e "  ${RED}❌${NC} $dep (missing)"
            missing+=("$dep")
        fi
    done
else
    echo -e "${YELLOW}⚠️  Cannot check dependencies (unknown package manager)${NC}"
    echo -e "   Manually verify you have:"
    echo -e "     - OpenGL development headers"
    echo -e "     - GLFW3"
    echo -e "     - GLEW"
    echo -e "     - GLM"
    exit 0
fi

echo ""

# ============================================================================
# Check OpenGL Version
# ============================================================================
echo -e "${YELLOW}Checking OpenGL version...${NC}"
if command -v glxinfo &>/dev/null; then
    GL_VERSION=$(glxinfo 2>/dev/null | grep "OpenGL version" | head -1 || echo "unknown")
    echo -e "  ${GREEN}✓${NC} $GL_VERSION"
    
    # Warn if < 3.3
    if echo "$GL_VERSION" | grep -qE "[0-2]\.[0-9]|3\.[0-2]"; then
        echo -e "  ${YELLOW}⚠️  OpenGL < 3.3 detected, may have issues${NC}"
    fi
else
    echo -e "  ${YELLOW}⚠️  glxinfo not available (install mesa-utils)${NC}"
fi
echo ""

# ============================================================================
# Summary
# ============================================================================
if [[ ${#missing[@]} -gt 0 ]]; then
    echo -e "${RED}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║${NC}  ${RED}❌ Missing ${#missing[@]} package(s)${NC}                                  ${RED}║${NC}"
    echo -e "${RED}╚════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    
    echo -e "${YELLOW}Install missing packages:${NC}"
    if [[ "$PKG_MGR" == "pacman" ]]; then
        echo -e "  ${BLUE}sudo pacman -S ${missing[*]}${NC}"
        echo -e "  Or for MSYS2 MinGW:"
        echo -e "  ${BLUE}pacman -S $(printf 'mingw-w64-x86_64-%s ' "${missing[@]}")${NC}"
    elif [[ "$PKG_MGR" == "apt" ]]; then
        echo -e "  ${BLUE}sudo apt-get install ${missing[*]}${NC}"
    elif [[ "$PKG_MGR" == "dnf" ]]; then
        echo -e "  ${BLUE}sudo dnf install ${missing[*]}${NC}"
    fi
    echo ""
    exit 1
else
    echo -e "${GREEN}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║${NC}  ${GREEN}✓ All dependencies installed${NC}                           ${GREEN}║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${GREEN}✓ Ready to build!${NC}"
    echo -e "  Run: ${BLUE}cd build && cmake .. && make -j8${NC}"
    echo ""
    exit 0
fi
