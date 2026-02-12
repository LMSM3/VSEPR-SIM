#!/bin/bash
# find_sphere_code.sh
# Locate all sphere-related rendering code in the project

set -euo pipefail

# Colors
BLUE='\033[0;34m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

base="$(pwd)"

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║${NC}  ${GREEN}Sphere Rendering Code Scanner${NC}                           ${BLUE}║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Search patterns for sphere-related code
PATTERNS=(
    "glut.*Sphere"
    "gluSphere"
    "GLUquadric"
    "SphereRenderer"
    "sphere.*render"
    "draw.*sphere"
    "build.*sphere"
    "generate.*sphere"
    "icosa"
    "geodesic"
    "uv.*sphere"
    "latlong"
    "subdiv"
)

# Combine into one regex
REGEX=$(IFS='|'; echo "${PATTERNS[*]}")

echo -e "${YELLOW}Searching for sphere-related code...${NC}"
echo ""

# Use ripgrep if available (faster), fallback to grep
if command -v rg &>/dev/null; then
    echo -e "${GREEN}Using ripgrep (fast)${NC}"
    echo ""
    
    rg -n --hidden --follow \
       --glob '!**/build/**' \
       --glob '!**/.git/**' \
       --glob '!**/third_party/**' \
       -e "$REGEX" \
       src/ include/ examples/ 2>/dev/null |
    sort -t: -k1,1 -k2,2n |
    sed "s#^\([^:]*\):\([0-9]\+\):#${YELLOW}file://$base/\1:\2:${NC}#" |
    head -50
    
else
    echo -e "${YELLOW}Using grep (slower, install ripgrep for better performance)${NC}"
    echo ""
    
    grep -rn --include='*.cpp' --include='*.hpp' --include='*.h' --include='*.c' \
         -E "$REGEX" \
         src/ include/ examples/ 2>/dev/null |
    sort -t: -k1,1 -k2,2n |
    sed "s#^\([^:]*\):\([0-9]\+\):#${YELLOW}file://$base/\1:\2:${NC}#" |
    head -50
fi

echo ""
echo -e "${GREEN}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║${NC}  ${GREEN}✓ Scan complete${NC}                                          ${GREEN}║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

echo -e "${YELLOW}File locations (unique):${NC}"
if command -v rg &>/dev/null; then
    rg -n --hidden --follow \
       --glob '!**/build/**' \
       --glob '!**/.git/**' \
       --glob '!**/third_party/**' \
       -e "$REGEX" \
       src/ include/ examples/ 2>/dev/null |
    cut -d: -f1 |
    sort -u |
    sed "s#^#  ${BLUE}file://$base/#" |
    sed "s#\$#${NC}#"
else
    grep -rn --include='*.cpp' --include='*.hpp' \
         -E "$REGEX" \
         src/ include/ examples/ 2>/dev/null |
    cut -d: -f1 |
    sort -u |
    sed "s#^#  ${BLUE}file://$base/#" |
    sed "s#\$#${NC}#"
fi

echo ""
echo -e "${YELLOW}Tip:${NC} Click file:// links above to open in editor"
echo ""
