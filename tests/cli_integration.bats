#!/usr/bin/env bats
# ============================================================
# VSEPR-Sim CLI Integration Tests
# Tests the command-line interface for basic functionality
# Requires: bats-core (https://github.com/bats-core/bats-core)
# ============================================================

# Setup: runs before every test
setup() {
  # Determine the vsepr binary location based on platform
  if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    VSEPR="./build/bin/vsepr.exe"
  else
    VSEPR="./build/bin/vsepr"
  fi
  
  # Ensure we're in the project root
  if [[ ! -f "CMakeLists.txt" ]]; then
    skip "Not in vsepr-sim project root"
  fi
  
  # Check if binary exists
  if [[ ! -f "$VSEPR" ]]; then
    skip "vsepr binary not found. Run ./build.sh first"
  fi
  
  # Create temp directory for test outputs
  TEST_TEMP_DIR="$(mktemp -d)"
}

# Teardown: runs after every test
teardown() {
  # Clean up temporary files
  if [[ -d "$TEST_TEMP_DIR" ]]; then
    rm -rf "$TEST_TEMP_DIR"
  fi
}

@test "vsepr version returns 0 and displays version info" {
  run $VSEPR version
  [ "$status" -eq 0 ]
  [[ "${lines[0]}" =~ "version" || "${lines[0]}" =~ "VSEPR" ]]
}

@test "vsepr help displays available commands" {
  run $VSEPR help
  [ "$status" -eq 0 ]
  [[ "$output" =~ "Available Commands" || "$output" =~ "Usage" || "$output" =~ "Commands" ]]
  [[ "$output" =~ "build" ]]
}

@test "vsepr build H2O creates valid output" {
  OUTPUT_FILE="$TEST_TEMP_DIR/water_test.xyz"
  
  run $VSEPR build H2O --optimize --output "$OUTPUT_FILE"
  [ "$status" -eq 0 ]
  [ -f "$OUTPUT_FILE" ]
  
  # Verify file is not empty
  [ -s "$OUTPUT_FILE" ]
}

@test "vsepr build CH4 (methane) succeeds" {
  OUTPUT_FILE="$TEST_TEMP_DIR/methane_test.xyz"
  
  run $VSEPR build CH4 --optimize --output "$OUTPUT_FILE"
  [ "$status" -eq 0 ]
  [ -f "$OUTPUT_FILE" ]
}

@test "vsepr build NH3 (ammonia) succeeds" {
  OUTPUT_FILE="$TEST_TEMP_DIR/ammonia_test.xyz"
  
  run $VSEPR build NH3 --optimize --output "$OUTPUT_FILE"
  [ "$status" -eq 0 ]
  [ -f "$OUTPUT_FILE" ]
}

@test "vsepr energy fails with invalid molecule formula" {
  run $VSEPR energy "NotAMolecule123"
  [ "$status" -ne 0 ]
}

@test "vsepr build fails with malformed formula" {
  run $VSEPR build "X99Y88"
  [ "$status" -ne 0 ]
}

@test "vsepr build with no arguments shows usage or fails gracefully" {
  run $VSEPR build
  # Should either fail (non-zero) or show help
  [[ "$status" -ne 0 || "$output" =~ "Usage" || "$output" =~ "required" ]]
}

@test "vsepr unknown command fails" {
  run $VSEPR thisCommandDoesNotExist
  [ "$status" -ne 0 ]
}

@test "vsepr build H2O without --output writes to default location or stdout" {
  run $VSEPR build H2O --optimize
  # Should succeed (either writes file or prints to stdout)
  [ "$status" -eq 0 ]
}

@test "vsepr build CO2 (linear molecule) succeeds" {
  OUTPUT_FILE="$TEST_TEMP_DIR/co2_test.xyz"
  
  run $VSEPR build CO2 --optimize --output "$OUTPUT_FILE"
  [ "$status" -eq 0 ]
  [ -f "$OUTPUT_FILE" ]
}

@test "vsepr build without --optimize still succeeds" {
  OUTPUT_FILE="$TEST_TEMP_DIR/h2o_unoptimized.xyz"
  
  run $VSEPR build H2O --output "$OUTPUT_FILE"
  [ "$status" -eq 0 ]
  [ -f "$OUTPUT_FILE" ]
}
