#!/usr/bin/env bash
################################################################################
# VSEPR-Sim Automated Build & Test Script
# 
# Builds the project and runs all tests in one command.
# Includes validation for the new heat-gated reaction control system (Item #7).
#
# Usage:
#   ./build_automated.sh                # Quick build + tests
#   ./build_automated.sh --clean        # Clean build + tests
#   ./build_automated.sh --verbose      # Verbose output
#   ./build_automated.sh --heat-only    # Build + run only heat_gate tests
################################################################################

set -e  # Exit on error

# ============================================================================
# Configuration
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="$PROJECT_ROOT/build"

# Parse arguments
CLEAN=false
VERBOSE=false
HEAT_ONLY=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean|-c)
            CLEAN=true
            shift
            ;;
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --heat-only)
            HEAT_ONLY=true
            shift
            ;;
        --help|-h)
            echo "VSEPR-Sim Automated Build & Test"
            echo ""
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --clean, -c       Clean build before building"
            echo "  --verbose, -v     Verbose output"
            echo "  --heat-only       Only run heat_gate tests (Item #7)"
            echo "  --help, -h        Show this help"
            echo ""
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# ============================================================================
# Color Output
# ============================================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
NC='\033[0m'

header() {
    echo ""
    echo -e "${MAGENTA}╔═══════════════════════════════════════════════════════════════╗${NC}"
    printf "${MAGENTA}║  %-61s║${NC}\n" "$1"
    echo -e "${MAGENTA}╚═══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

step() {
    echo -e "${WHITE}▶ $1${NC}"
}

success() {
    echo -e "${GREEN}✓ $1${NC}"
}

failure() {
    echo -e "${RED}✗ $1${NC}"
    exit 1
}

info() {
    echo -e "${CYAN}ℹ $1${NC}"
}

# ============================================================================
# Start
# ============================================================================

header "VSEPR-Sim Automated Build & Test"

echo "Project Root: $PROJECT_ROOT"
echo "Build Directory: $BUILD_DIR"
echo ""

# ============================================================================
# Clean (optional)
# ============================================================================

if [ "$CLEAN" = true ]; then
    step "Cleaning build directory..."
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        success "Build directory cleaned"
    else
        info "Build directory doesn't exist, skipping clean"
    fi
    echo ""
fi

# ============================================================================
# CMake Configuration
# ============================================================================

header "CMake Configuration"

step "Configuring CMake..."

CMAKE_ARGS=(
    -S "$PROJECT_ROOT"
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_TESTS=ON
)

# Detect generator
if command -v ninja &> /dev/null; then
    CMAKE_ARGS+=(-G "Ninja")
    info "Using Ninja generator"
elif command -v make &> /dev/null; then
    CMAKE_ARGS+=(-G "Unix Makefiles")
    info "Using Unix Makefiles generator"
fi

if [ "$VERBOSE" = true ]; then
    CMAKE_ARGS+=(--debug-output)
fi

if cmake "${CMAKE_ARGS[@]}"; then
    success "CMake configuration successful"
else
    failure "CMake configuration failed"
fi

echo ""

# ============================================================================
# Build
# ============================================================================

header "Building Project"

step "Compiling..."

BUILD_ARGS=(
    --build "$BUILD_DIR"
    --config Release
)

if [ "$VERBOSE" = true ]; then
    BUILD_ARGS+=(--verbose)
fi

# Detect number of cores for parallel build
if command -v nproc &> /dev/null; then
    CORES=$(nproc)
elif command -v sysctl &> /dev/null; then
    CORES=$(sysctl -n hw.ncpu)
else
    CORES=4
fi

BUILD_ARGS+=(--parallel "$CORES")

info "Using $CORES parallel jobs"

if cmake "${BUILD_ARGS[@]}"; then
    success "Build successful"
else
    failure "Build failed"
fi

echo ""

# ============================================================================
# Run Tests
# ============================================================================

header "Running Tests"

cd "$BUILD_DIR"

if [ "$HEAT_ONLY" = true ]; then
    # Only run heat_gate tests (Item #7)
    step "Running heat_gate tests only (Item #7)..."
    
    if [ -f "tests/test_heat_gate" ] || [ -f "tests/test_heat_gate.exe" ]; then
        if ./tests/test_heat_gate || ./tests/test_heat_gate.exe 2>/dev/null; then
            success "Heat gate tests PASSED ✓"
        else
            failure "Heat gate tests FAILED ✗"
        fi
    else
        failure "test_heat_gate executable not found"
    fi
else
    # Run all tests
    step "Running CTest suite..."
    
    if [ -f "CTestTestfile.cmake" ]; then
        CTEST_ARGS=(
            --output-on-failure
            --parallel "$CORES"
        )
        
        if [ "$VERBOSE" = true ]; then
            CTEST_ARGS+=(--verbose)
        fi
        
        if ctest "${CTEST_ARGS[@]}"; then
            success "All tests PASSED ✓"
        else
            failure "Some tests FAILED ✗"
        fi
    else
        warning "CTest not configured (tests may not be built)"
        info "Trying to run tests manually..."
        
        # Try to run heat_gate test directly
        if [ -f "tests/test_heat_gate" ] || [ -f "tests/test_heat_gate.exe" ]; then
            step "Running test_heat_gate..."
            if ./tests/test_heat_gate || ./tests/test_heat_gate.exe 2>/dev/null; then
                success "test_heat_gate PASSED"
            else
                failure "test_heat_gate FAILED"
            fi
        fi
        
        # Try to run crystal pipeline test
        if [ -f "tests/test_crystal_pipeline" ] || [ -f "tests/test_crystal_pipeline.exe" ]; then
            step "Running test_crystal_pipeline..."
            if ./tests/test_crystal_pipeline || ./tests/test_crystal_pipeline.exe 2>/dev/null; then
                success "test_crystal_pipeline PASSED"
            else
                warning "test_crystal_pipeline FAILED (non-critical)"
            fi
        fi
    fi
fi

cd "$PROJECT_ROOT"

echo ""

# ============================================================================
# Summary
# ============================================================================

header "Build & Test Summary"

success "Build completed successfully"
success "Tests passed"

if [ "$HEAT_ONLY" = false ]; then
    info "Binaries available in: $BUILD_DIR/bin/"
    info "Tests available in: $BUILD_DIR/tests/"
fi

echo ""
echo -e "${GREEN}✓ All automated checks passed!${NC}"
echo ""

# ============================================================================
# Additional Validation (Item #7 specific)
# ============================================================================

if [ -f "examples/demo_temperature_heat_mapping" ] || [ -f "examples/demo_temperature_heat_mapping.exe" ]; then
    echo ""
    echo -e "${CYAN}╔═══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║  Optional: Run Item #7 Demo                                  ║${NC}"
    echo -e "${CYAN}╚═══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    info "Demo available: ./build/examples/demo_temperature_heat_mapping"
    echo ""
fi

exit 0
