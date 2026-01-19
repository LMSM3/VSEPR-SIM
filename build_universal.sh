#!/bin/bash
# build_universal.sh
# Universal build script for VSEPR-Sim (Linux/WSL)

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  VSEPR-Sim Universal Build System (Bash)                      ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Parse arguments
BUILD_TYPE="Release"
CLEAN=false
JOBS=8
TARGET="all"
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug|-d)
            BUILD_TYPE="Debug"
            shift
            ;;
        --clean|-c)
            CLEAN=true
            shift
            ;;
        --jobs|-j)
            JOBS="$2"
            shift 2
            ;;
        --target|-t)
            TARGET="$2"
            shift 2
            ;;
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --help|-h)
            echo "Usage: ./build_universal.sh [options]"
            echo ""
            echo "Options:"
            echo "  --debug, -d         Build in Debug mode (default: Release)"
            echo "  --clean, -c         Clean build directory before building"
            echo "  --jobs, -j <N>      Number of parallel jobs (default: 8)"
            echo "  --target, -t <T>    Specific target to build (default: all)"
            echo "  --verbose, -v       Verbose build output"
            echo "  --help, -h          Show this help message"
            echo ""
            echo "Examples:"
            echo "  ./build_universal.sh                    # Standard release build"
            echo "  ./build_universal.sh --debug            # Debug build"
            echo "  ./build_universal.sh --clean --jobs 16  # Clean rebuild with 16 cores"
            echo "  ./build_universal.sh --target vsepr     # Build only vsepr binary"
            exit 0
            ;;
        *)
            echo -e "${RED}✗ Unknown option: $1${NC}"
            echo "Run with --help for usage information"
            exit 1
            ;;
    esac
done

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}▶ Cleaning build directory...${NC}"
    rm -rf build/
    echo -e "${GREEN}✓ Build directory cleaned${NC}"
    echo ""
fi

# Create build directory
if [ ! -d "build" ]; then
    echo -e "${YELLOW}▶ Creating build directory...${NC}"
    mkdir -p build
    echo -e "${GREEN}✓ Build directory created${NC}"
    echo ""
fi

# Configure with CMake
echo -e "${YELLOW}▶ Configuring CMake (${BUILD_TYPE})...${NC}"
cd build
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE
if [ $? -ne 0 ]; then
    echo -e "${RED}✗ CMake configuration failed${NC}"
    exit 1
fi
echo -e "${GREEN}✓ CMake configuration complete${NC}"
echo ""

# Build
echo -e "${YELLOW}▶ Building target: ${TARGET} (${JOBS} parallel jobs)...${NC}"
if [ "$VERBOSE" = true ]; then
    make $TARGET -j$JOBS VERBOSE=1
else
    make $TARGET -j$JOBS 2>&1 | tail -30
fi

if [ $? -ne 0 ]; then
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  ✓ Build Complete!                                            ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Show binary locations
if [ -f "bin/vsepr" ]; then
    echo -e "${BLUE}Binary location:${NC} $(pwd)/bin/vsepr"
    echo -e "${BLUE}Quick test:${NC}      ./build/bin/vsepr --help"
    echo -e "${BLUE}Therm test:${NC}      ./build/bin/vsepr therm <file.xyz>"
fi

echo ""
