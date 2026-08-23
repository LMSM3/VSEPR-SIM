#!/usr/bin/env bats

# ============================================================================
# VSEPR-Sim Integration Tests (BATS)
# ============================================================================
# 
# Prerequisites:
#   - BATS installed: npm install -g bats
#   - Or: sudo apt-get install bats (Linux)
#   - Or: brew install bats-core (macOS)
#
# Usage:
#   bats tests/integration_tests.bats
#   bats tests/  # Run all .bats files
# ============================================================================

# Setup: runs before every test
setup() {
  # Detect platform and set appropriate executable
  if [ -f "./build/bin/vsepr.exe" ]; then
    VSEPR="./build/bin/vsepr.exe"
  elif [ -f "./build/bin/vsepr" ]; then
    VSEPR="./build/bin/vsepr"
  else
    echo "ERROR: VSEPR executable not found!" >&2
    return 1
  fi
  
  # Create temp directory for test outputs
  TEST_TEMP_DIR="$(mktemp -d)"
  export TEST_TEMP_DIR
  
  # Change to project root
  cd "${BATS_TEST_DIRNAME}/.." || return 1
}

# Teardown: runs after every test
teardown() {
  # Clean up temporary files
  if [ -n "$TEST_TEMP_DIR" ] && [ -d "$TEST_TEMP_DIR" ]; then
    rm -rf "$TEST_TEMP_DIR"
  fi
}

# ============================================================================
# Basic Command Tests
# ============================================================================

@test "vsepr version returns 0 and displays version info" {
  run $VSEPR version
  [ "$status" -eq 0 ]
  [[ "$output" == *"2.0"* ]] || [[ "$output" == *"version"* ]]
}

@test "vsepr help displays available commands" {
  run $VSEPR help
  [ "$status" -eq 0 ]
  [[ "$output" == *"Available Commands"* ]] || [[ "$output" == *"Commands"* ]]
  [[ "$output" == *"build"* ]]
}

@test "vsepr --help works as alias for help" {
  run $VSEPR --help
  [ "$status" -eq 0 ]
  [[ "$output" == *"build"* ]]
}

# ============================================================================
# Build Command Tests
# ============================================================================

@test "vsepr build H2O creates water molecule" {
  run $VSEPR build H2O
  [ "$status" -eq 0 ]
  [[ "$output" == *"H2O"* ]] || [[ "$output" == *"water"* ]]
  [[ "$output" == *"Atoms"* ]] || [[ "$output" == *"3"* ]]
}

@test "vsepr build H2O with optimization succeeds" {
  run $VSEPR build H2O --optimize
  [ "$status" -eq 0 ]
  [[ "$output" == *"Optimization"* ]] || [[ "$output" == *"converged"* ]]
}

@test "vsepr build H2O creates output file" {
  OUTPUT_FILE="$TEST_TEMP_DIR/water_test.xyz"
  run $VSEPR build H2O --optimize --output "$OUTPUT_FILE"
  [ "$status" -eq 0 ]
  [ -f "$OUTPUT_FILE" ]
  
  # Check file has content
  [ -s "$OUTPUT_FILE" ]
}

@test "vsepr build CH4 creates methane (tetrahedral)" {
  run $VSEPR build CH4
  [ "$status" -eq 0 ]
  [[ "$output" == *"CH4"* ]] || [[ "$output" == *"methane"* ]]
  [[ "$output" == *"Tetrahedral"* ]] || [[ "$output" == *"5"* ]] # 5 atoms
}

@test "vsepr build NH3 creates ammonia (trigonal pyramidal)" {
  run $VSEPR build NH3
  [ "$status" -eq 0 ]
  [[ "$output" == *"NH3"* ]] || [[ "$output" == *"ammonia"* ]]
  [[ "$output" == *"Pyramidal"* ]] || [[ "$output" == *"4"* ]] # 4 atoms
}

@test "vsepr build CO2 creates carbon dioxide (linear)" {
  run $VSEPR build CO2
  [ "$status" -eq 0 ]
  [[ "$output" == *"CO2"* ]]
  [[ "$output" == *"3"* ]] # 3 atoms
}

# ============================================================================
# Error Handling Tests
# ============================================================================

@test "vsepr build fails with invalid formula" {
  run $VSEPR build "XyZ999"
  [ "$status" -ne 0 ]
}

@test "vsepr build fails with empty formula" {
  run $VSEPR build ""
  [ "$status" -ne 0 ]
}

@test "vsepr build shows error for unknown element" {
  run $VSEPR build "Xx2"
  [ "$status" -ne 0 ]
  [[ "$output" == *"Unknown"* ]] || [[ "$output" == *"invalid"* ]] || [[ "$output" == *"error"* ]]
}

# ============================================================================
# Complex Molecule Tests
# ============================================================================

@test "vsepr build ethane C2H6 succeeds" {
  run $VSEPR build C2H6
  [ "$status" -eq 0 ]
  [[ "$output" == *"C2H6"* ]] || [[ "$output" == *"ethane"* ]]
}

@test "vsepr build sulfuric acid H2SO4 succeeds" {
  run $VSEPR build H2SO4
  [ "$status" -eq 0 ]
  [[ "$output" == *"H2SO4"* ]]
}

@test "vsepr build benzene C6H6 succeeds" {
  run $VSEPR build C6H6
  [ "$status" -eq 0 ]
  [[ "$output" == *"C6H6"* ]]
}

# ============================================================================
# Output Format Tests
# ============================================================================

@test "vsepr build generates valid XYZ file format" {
  OUTPUT_FILE="$TEST_TEMP_DIR/format_test.xyz"
  run $VSEPR build H2O --output "$OUTPUT_FILE"
  [ "$status" -eq 0 ]
  
  # XYZ format: first line is atom count
  FIRST_LINE=$(head -n 1 "$OUTPUT_FILE")
  [[ "$FIRST_LINE" =~ ^[0-9]+$ ]]
}

# ============================================================================
# Performance Tests (optional - can be slow)
# ============================================================================

@test "vsepr build completes in reasonable time for small molecule" {
  skip "Performance test - run manually"
  
  START=$(date +%s)
  run $VSEPR build H2O --optimize
  END=$(date +%s)
  DURATION=$((END - START))
  
  [ "$status" -eq 0 ]
  [ "$DURATION" -lt 10 ] # Should complete in under 10 seconds
}

# ============================================================================
# Integration with Data Files
# ============================================================================

@test "vsepr can access data directory" {
  # Build a molecule that requires periodic table data
  run $VSEPR build NaCl
  # Should succeed or give chemistry error, not file I/O error
  [[ "$output" != *"No such file"* ]]
  [[ "$output" != *"cannot open"* ]]
}
