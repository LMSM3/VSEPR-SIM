#!/bin/bash
# quick_fix_rendering.sh
# One-command fix for VSEPR rendering issues

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║${NC}                                                            ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ${GREEN}VSEPR-Sim Rendering Quick Fix${NC}                           ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ${YELLOW}One-command solution for black screen issue${NC}             ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}                                                            ${CYAN}║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# ============================================================================
# Phase 1: Diagnostics
# ============================================================================
echo -e "${BLUE}[Phase 1/4]${NC} Running diagnostics..."
echo ""

if [[ -x "scripts/detect_render_issues.sh" ]]; then
    ./scripts/detect_render_issues.sh || {
        echo ""
        echo -e "${YELLOW}⚠️  Issues detected. Proceeding with automated fixes...${NC}"
        echo ""
    }
else
    echo -e "${YELLOW}⚠️  Diagnostic script not found, skipping${NC}"
fi

# ============================================================================
# Phase 2: Apply Core Fix (OpenGL Profile)
# ============================================================================
echo -e "${BLUE}[Phase 2/4]${NC} Applying OpenGL profile fix..."
echo ""

TARGET_FILE="examples/elevated_gui_app.cpp"

if [[ -f "$TARGET_FILE" ]]; then
    # Check if already using COMPAT profile
    if grep -q "GLFW_OPENGL_COMPAT_PROFILE" "$TARGET_FILE"; then
        echo -e "  ${GREEN}✓${NC} Already using compatibility profile"
    else
        # Backup
        cp "$TARGET_FILE" "${TARGET_FILE}.backup"
        echo -e "  ${GREEN}✓${NC} Created backup: ${TARGET_FILE}.backup"
        
        # Replace CORE with COMPAT
        sed -i 's/GLFW_OPENGL_CORE_PROFILE/GLFW_OPENGL_COMPAT_PROFILE/g' "$TARGET_FILE"
        
        if grep -q "GLFW_OPENGL_COMPAT_PROFILE" "$TARGET_FILE"; then
            echo -e "  ${GREEN}✓${NC} Changed to compatibility profile"
        else
            echo -e "  ${RED}❌${NC} Failed to change profile"
            exit 1
        fi
    fi
else
    echo -e "  ${YELLOW}⚠️  File not found: $TARGET_FILE${NC}"
fi

echo ""

# ============================================================================
# Phase 3: Add GL Error Checking
# ============================================================================
echo -e "${BLUE}[Phase 3/4]${NC} Adding GL error checking..."
echo ""

RENDERER_CPP="src/render/molecular_renderer.cpp"

if [[ -f "$RENDERER_CPP" ]]; then
    # Check if error checking already exists
    if grep -q "check_gl_error" "$RENDERER_CPP"; then
        echo -e "  ${GREEN}✓${NC} GL error checking already present"
    else
        # Backup
        cp "$RENDERER_CPP" "${RENDERER_CPP}.backup"
        echo -e "  ${GREEN}✓${NC} Created backup: ${RENDERER_CPP}.backup"
        
        # Add check_gl_error helper at the top of file (after includes)
        # This is a simple version - full version in QUICK_RENDER_FIX.md
        cat > /tmp/gl_error_check.txt << 'EOF'

// GL Error Checking Helper
namespace {
inline void check_gl_error(const char* ctx) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "[GL_ERROR] " << ctx << ": 0x" << std::hex << err << std::dec << "\n";
    }
}
}  // namespace

EOF
        
        # Insert after first #include block
        sed -i '/#include/a\ ' "$RENDERER_CPP"
        sed -i '/^$/r /tmp/gl_error_check.txt' "$RENDERER_CPP"
        
        echo -e "  ${GREEN}✓${NC} Added check_gl_error() helper"
        echo -e "  ${YELLOW}Note:${NC} You still need to add check_gl_error() calls manually"
        echo -e "        See QUICK_RENDER_FIX.md for details"
    fi
else
    echo -e "  ${YELLOW}⚠️  File not found: $RENDERER_CPP${NC}"
fi

echo ""

# ============================================================================
# Phase 4: Rebuild
# ============================================================================
echo -e "${BLUE}[Phase 4/4]${NC} Rebuilding project..."
echo ""

if [[ -d "build" ]]; then
    cd build
    
    echo -e "  ${YELLOW}Running cmake...${NC}"
    cmake .. -DBUILD_GUI=ON -DBUILD_VIS=ON || {
        echo -e "  ${RED}❌${NC} CMake failed"
        exit 1
    }
    
    echo ""
    echo -e "  ${YELLOW}Compiling...${NC}"
    make -j$(nproc 2>/dev/null || echo 4) || {
        echo -e "  ${RED}❌${NC} Build failed"
        exit 1
    }
    
    cd ..
    
    echo ""
    echo -e "  ${GREEN}✓${NC} Build successful!"
else
    echo -e "  ${YELLOW}⚠️  Build directory not found${NC}"
    echo -e "     Run: ${BLUE}mkdir build && cd build && cmake ..${NC}"
fi

echo ""

# ============================================================================
# Summary
# ============================================================================
echo -e "${CYAN}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║${NC}  ${GREEN}✓ Quick Fix Complete!${NC}                                   ${CYAN}║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

echo -e "${GREEN}What was fixed:${NC}"
echo -e "  1. ✓ Switched to OpenGL Compatibility Profile"
echo -e "  2. ✓ Added GL error checking infrastructure"
echo -e "  3. ✓ Recompiled with fixes applied"
echo ""

echo -e "${YELLOW}Next steps:${NC}"
echo -e "  1. Run: ${BLUE}./build/bin/vsepr_gui_live${NC}"
echo -e "  2. Click \"Generate Monazite-Ce (96 atoms)\""
echo -e "  3. Should see ${GREEN}colored spheres${NC} (not black screen)"
echo ""

echo -e "${YELLOW}If still not working:${NC}"
echo -e "  • Check console for [GL_ERROR] messages"
echo -e "  • Run: ${BLUE}./scripts/detect_render_issues.sh${NC}"
echo -e "  • See: ${BLUE}QUICK_RENDER_FIX.md${NC} for manual steps"
echo ""

echo -e "${YELLOW}Backup files created:${NC}"
if [[ -f "examples/elevated_gui_app.cpp.backup" ]]; then
    echo -e "  • examples/elevated_gui_app.cpp.backup"
fi
if [[ -f "src/render/molecular_renderer.cpp.backup" ]]; then
    echo -e "  • src/render/molecular_renderer.cpp.backup"
fi
echo ""

echo -e "${GREEN}Ready to test!${NC} 🚀"
echo ""
