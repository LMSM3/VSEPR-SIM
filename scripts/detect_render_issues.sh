#!/bin/bash
# detect_render_issues.sh
# Automated scanner for common VSEPR rendering pitfalls

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║${NC}  ${GREEN}VSEPR Render Diagnostic Scanner${NC}                          ${BLUE}║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Files to check
FILES=(
    "examples/elevated_gui_app.cpp"
    "include/render/molecular_renderer.hpp"
    "src/render/molecular_renderer.cpp"
    "examples/vsepr_gui_live.cpp"
)

ISSUES=0

# ============================================================================
# TEST 1: File Existence
# ============================================================================
echo -e "${YELLOW}[1/7]${NC} Checking file existence..."
for f in "${FILES[@]}"; do
    if [[ ! -f "$f" ]]; then
        echo -e "  ${RED}❌ MISSING:${NC} $f"
        ((ISSUES++))
    else
        echo -e "  ${GREEN}✓${NC} Found: $f"
    fi
done
echo ""

# ============================================================================
# TEST 2: GL Error Handling
# ============================================================================
echo -e "${YELLOW}[2/7]${NC} Checking GL error handling..."
if grep -qE "glGetError|check_gl_error" "${FILES[@]}" 2>/dev/null; then
    count=$(grep -cE "glGetError|check_gl_error" "${FILES[@]}" 2>/dev/null || echo "0")
    echo -e "  ${GREEN}✓${NC} GL error checking present (${count} calls)"
else
    echo -e "  ${RED}❌ NO GL ERROR CHECKING FOUND${NC}"
    echo -e "     ${YELLOW}Fix:${NC} Add check_gl_error() calls after GL functions"
    ((ISSUES++))
fi
echo ""

# ============================================================================
# TEST 3: Ambient Lighting Value
# ============================================================================
echo -e "${YELLOW}[3/7]${NC} Checking ambient lighting..."
if [[ -f "src/render/molecular_renderer.cpp" ]]; then
    ambient=$(grep -nE "light_ambient.*\{" src/render/molecular_renderer.cpp | head -1 || true)
    if [[ -z "$ambient" ]]; then
        echo -e "  ${RED}⚠️  No ambient light definition found${NC}"
        ((ISSUES++))
    elif echo "$ambient" | grep -qE "0\.[0-5]"; then
        echo -e "  ${RED}⚠️  Ambient too dim (<0.5)${NC}"
        echo -e "     Line: $ambient"
        echo -e "     ${YELLOW}Fix:${NC} Increase to 0.6+"
        ((ISSUES++))
    else
        echo -e "  ${GREEN}✓${NC} Ambient lighting adequate (>0.5)"
    fi
else
    echo -e "  ${YELLOW}⚠️  File not found, skipping${NC}"
fi
echo ""

# ============================================================================
# TEST 4: Background Clear Color
# ============================================================================
echo -e "${YELLOW}[4/7]${NC} Checking background color..."
if [[ -f "src/render/molecular_renderer.cpp" ]]; then
    if grep -qE "glClearColor" src/render/molecular_renderer.cpp; then
        echo -e "  ${GREEN}✓${NC} Background color set"
    else
        echo -e "  ${RED}❌ NO BACKGROUND COLOR${NC}"
        echo -e "     ${YELLOW}Fix:${NC} Add: glClearColor(0.15f, 0.15f, 0.18f, 1.0f);"
        ((ISSUES++))
    fi
else
    echo -e "  ${YELLOW}⚠️  File not found, skipping${NC}"
fi
echo ""

# ============================================================================
# TEST 5: OpenGL Profile
# ============================================================================
echo -e "${YELLOW}[5/7]${NC} Checking OpenGL profile..."
if [[ -f "examples/elevated_gui_app.cpp" ]]; then
    if grep -qE "GLFW_OPENGL_CORE_PROFILE" examples/elevated_gui_app.cpp; then
        echo -e "  ${RED}❌ USING CORE PROFILE${NC} (fixed pipeline broken)"
        echo -e "     ${YELLOW}Fix:${NC} Change to: GLFW_OPENGL_COMPAT_PROFILE"
        ((ISSUES++))
    elif grep -qE "GLFW_OPENGL_COMPAT_PROFILE" examples/elevated_gui_app.cpp; then
        echo -e "  ${GREEN}✓${NC} Using compatibility profile"
    else
        echo -e "  ${YELLOW}⚠️  Profile not explicitly set${NC}"
    fi
else
    echo -e "  ${YELLOW}⚠️  File not found, skipping${NC}"
fi
echo ""

# ============================================================================
# TEST 6: Sphere Rendering Optimization
# ============================================================================
echo -e "${YELLOW}[6/7]${NC} Checking sphere rendering..."
if [[ -f "src/render/molecular_renderer.cpp" ]]; then
    # Check if spheres are rebuilt per atom (inefficient)
    if grep -qE "for.*atoms.*SphereRenderer|for.*atoms.*build.*sphere" src/render/molecular_renderer.cpp; then
        echo -e "  ${YELLOW}⚠️  Spheres may be rebuilt per atom (slow)${NC}"
        echo -e "     ${YELLOW}Tip:${NC} Build once, reuse via instancing"
    else
        echo -e "  ${GREEN}✓${NC} Sphere rendering looks optimized"
    fi
else
    echo -e "  ${YELLOW}⚠️  File not found, skipping${NC}"
fi
echo ""

# ============================================================================
# TEST 7: ImGui/GL State Conflicts
# ============================================================================
echo -e "${YELLOW}[7/7]${NC} Checking for ImGui/GL conflicts..."
if [[ -f "examples/vsepr_gui_live.cpp" ]]; then
    # Check if rendering happens inside ImGui window (potential state conflict)
    if grep -A 10 "ImGui::Begin.*Viewer" examples/vsepr_gui_live.cpp | grep -qE "renderer\\.render|glClear|glDraw"; then
        echo -e "  ${YELLOW}⚠️  Rendering inside ImGui window (check state)${NC}"
        echo -e "     ${YELLOW}Tip:${NC} Reset GL state after ImGui or render to FBO"
    else
        echo -e "  ${GREEN}✓${NC} Render/ImGui separation looks clean"
    fi
else
    echo -e "  ${YELLOW}⚠️  File not found, skipping${NC}"
fi
echo ""

# ============================================================================
# Summary
# ============================================================================
echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
if [[ $ISSUES -eq 0 ]]; then
    echo -e "${BLUE}║${NC}  ${GREEN}✓ Scan Complete: No critical issues found${NC}             ${BLUE}║${NC}"
else
    echo -e "${BLUE}║${NC}  ${RED}❌ Scan Complete: ${ISSUES} issue(s) found${NC}                       ${BLUE}║${NC}"
fi
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

if [[ $ISSUES -gt 0 ]]; then
    echo -e "${YELLOW}Next steps:${NC}"
    echo -e "  1. Review issues above"
    echo -e "  2. Follow suggested fixes"
    echo -e "  3. Re-run this script to verify"
    echo -e "  4. See QUICK_RENDER_FIX.md for detailed guide"
    echo ""
    exit 1
else
    echo -e "${GREEN}✓ Your rendering pipeline looks good!${NC}"
    echo -e "  Run: ${BLUE}./build/bin/vsepr_gui_live${NC} to test"
    echo ""
    exit 0
fi
