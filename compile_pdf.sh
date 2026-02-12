#!/usr/bin/env bash
################################################################################
# Compile Monazite LaTeX Document to PDF
# Requires: pdflatex, bibtex (TeXLive or MiKTeX)
################################################################################

CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${CYAN}${BOLD}"
cat << 'EOF'
╔════════════════════════════════════════════════════════════════╗
║  Monazite-Ce PDF Document Compiler                           ║
╚════════════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

# Check if LaTeX is installed
if ! command -v pdflatex &> /dev/null; then
    echo -e "${RED}Error: pdflatex not found!${NC}"
    echo ""
    echo "Install LaTeX:"
    echo "  Ubuntu/Debian:  sudo apt install texlive-full"
    echo "  Windows:        Install MiKTeX or TeXLive"
    echo "  macOS:          brew install mactex"
    exit 1
fi

TEX_FILE="MONAZITE_ANALYSIS.tex"
PDF_FILE="MONAZITE_ANALYSIS.pdf"

if [ ! -f "$TEX_FILE" ]; then
    echo -e "${RED}Error: $TEX_FILE not found!${NC}"
    exit 1
fi

echo -e "${GREEN}Found: $TEX_FILE${NC}"
echo ""

echo -e "${BOLD}=== Compilation Steps ===${NC}"
echo ""

# First pass
echo -e "${CYAN}[1/3] First pdflatex pass...${NC}"
pdflatex -interaction=nonstopmode "$TEX_FILE" > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo -e "${YELLOW}Warning: First pass had errors (expected)${NC}"
    pdflatex -interaction=nonstopmode "$TEX_FILE" | tail -20
fi

# Second pass (resolve references)
echo -e "${CYAN}[2/3] Second pdflatex pass (resolve references)...${NC}"
pdflatex -interaction=nonstopmode "$TEX_FILE" > /dev/null 2>&1

# Third pass (finalize TOC and refs)
echo -e "${CYAN}[3/3] Third pdflatex pass (finalize)...${NC}"
pdflatex -interaction=nonstopmode "$TEX_FILE" > /dev/null 2>&1

echo ""

# Check if PDF was created
if [ -f "$PDF_FILE" ]; then
    FILE_SIZE=$(ls -lh "$PDF_FILE" | awk '{print $5}')
    echo -e "${GREEN}${BOLD}✓ SUCCESS!${NC}"
    echo ""
    echo -e "${GREEN}PDF created: $PDF_FILE ($FILE_SIZE)${NC}"
    echo ""
    
    # Count pages
    if command -v pdfinfo &> /dev/null; then
        PAGES=$(pdfinfo "$PDF_FILE" 2>/dev/null | grep "Pages:" | awk '{print $2}')
        echo "  Pages: $PAGES"
    fi
    
    # Show file info
    echo "  Location: $(pwd)/$PDF_FILE"
    echo ""
    
    # Clean up auxiliary files
    echo -e "${CYAN}Cleaning up auxiliary files...${NC}"
    rm -f *.aux *.log *.out *.toc *.lof *.lot
    echo -e "${GREEN}Cleanup complete!${NC}"
    echo ""
    
    echo -e "${BOLD}=== View PDF ===${NC}"
    echo ""
    echo "Open with:"
    echo "  Windows:   start $PDF_FILE"
    echo "  Linux:     xdg-open $PDF_FILE"
    echo "  macOS:     open $PDF_FILE"
    echo ""
    
    # Offer to open automatically
    read -p "Open PDF now? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        if [[ "$OSTYPE" == "linux-gnu"* ]]; then
            xdg-open "$PDF_FILE" 2>/dev/null &
        elif [[ "$OSTYPE" == "darwin"* ]]; then
            open "$PDF_FILE" 2>/dev/null &
        elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
            start "$PDF_FILE" 2>/dev/null &
        fi
        echo -e "${GREEN}Opening PDF...${NC}"
    fi
    
else
    echo -e "${RED}${BOLD}✗ FAILED${NC}"
    echo ""
    echo "PDF was not created. Check errors:"
    echo ""
    echo "View full log:"
    echo "  cat ${TEX_FILE%.tex}.log | less"
    echo ""
    echo "Common issues:"
    echo "  • Missing LaTeX packages (install texlive-full)"
    echo "  • Syntax errors in .tex file"
    echo "  • Missing fonts or graphics"
    exit 1
fi

echo -e "${CYAN}${BOLD}PDF compilation complete! 📄${NC}"
