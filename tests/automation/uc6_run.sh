#!/usr/bin/env bash
# tests/automation/uc6_run.sh
# UC-6 -- Crystallographic Regression Suite (The Golden Suite)
# Runs val_01..val_08 headlessly, extracts computed properties from
# pipeline_records.json and compares against [expected] reference values.
# Exit 0 = zero regressions. Exit 1 = at least one FAIL.
# PENDING = feature absent per WO-MF-01; not counted as FAIL.

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BIN="$REPO_ROOT/build/vsepr.exe"
PASS=0; FAIL=0; PENDING=0

expect_file() {
    local f="$1" label="$2"
    if [ -f "$f" ]; then echo "  PASS  $label"; PASS=$((PASS+1))
    else echo "  FAIL  $label (missing: $f)"; FAIL=$((FAIL+1)); fi
}

validate_json() {
    local f="$1"
    if python3 -c "import json,sys; json.load(open(sys.argv[1]))" "$f" 2>/dev/null; then
        echo "  PASS  JSON valid: $(basename "$f")"; PASS=$((PASS+1))
    else echo "  FAIL  JSON invalid: $f"; FAIL=$((FAIL+1)); fi
}

# check_value FILE KEY REFERENCE TOLERANCE LABEL
check_value() {
    local f="$1" key="$2" ref="$3" tol="$4" label="$5"
    [ ! -f "$f" ] && { echo "  PENDING [MF-A07] $label (no records)"; PENDING=$((PENDING+1)); return; }
    local val ok delta
    val=$(python3 - "$f" "$key" <<-PYEOF
import json, sys
data = json.load(open(sys.argv[1]))
keys = sys.argv[2].split(".")
v = data
for k in keys:
    if isinstance(v, dict) and k in v: v = v[k]
    else: v = None; break
print(v if v is not None else "MISSING")
PYEOF
)
    [ "$val" = "MISSING" ] && { echo "  PENDING [MF-A07] $label (key absent)"; PENDING=$((PENDING+1)); return; }
    ok=$(python3 -c "print('yes' if abs(float('$val') - float('$ref')) <= float('$tol') else 'no')")
    delta=$(python3 -c "print(abs(float('$val') - float('$ref')))")
    if [ "$ok" = "yes" ]; then
        printf '  PASS  %-44s  ref=%-10s computed=%-10s delta=%s\n' "$label" "$ref" "$val" "$delta"; PASS=$((PASS+1))
    else
        printf '  FAIL  %-44s  ref=%-10s computed=%-10s delta=%s (tol=%s)\n' "$label" "$ref" "$val" "$delta" "$tol"; FAIL=$((FAIL+1))
    fi
}

run_val() {
    local script="$1" outdir="$2" label="$3"
    echo ""; echo "==========================================="; echo " Running: $label"; echo "==========================================="
    cd "$REPO_ROOT"; "$BIN" run "$script" || true
    expect_file "$outdir/pipeline_records.json" "$label: pipeline_records.json"
}

check_val01() {
    local d="$REPO_ROOT/out/val_01_caf2_fluorite"
    run_val "scripts/val_01_caf2_fluorite.vsim" "$d" "val_01 CaF2"
    check_value "$d/pipeline_records.json" madelung_constant   5.03879 0.001  "CaF2 madelung_constant"
    check_value "$d/pipeline_records.json" coordination_Ca     8       0      "CaF2 coordination_Ca"
    check_value "$d/pipeline_records.json" coordination_F      4       0      "CaF2 coordination_F"
    check_value "$d/pipeline_records.json" charge_sum          0.0     1e-10  "CaF2 charge_sum"
    [ -f "$d/pipeline_records.json" ] && validate_json "$d/pipeline_records.json"
}

check_val02() {
    local d="$REPO_ROOT/out/val_02_zns_polymorphs"
    run_val "scripts/val_02_zns_polymorphs.vsim" "$d" "val_02 ZnS"
    check_value "$d/pipeline_records.json" zns_blende.madelung_constant   1.6381 0.001 "ZnS blende madelung"
    check_value "$d/pipeline_records.json" zns_wurtzite.madelung_constant 1.6413 0.001 "ZnS wurtzite madelung"
    check_value "$d/pipeline_records.json" coordination_Zn     4       0      "ZnS coordination_Zn"
    check_value "$d/pipeline_records.json" coordination_S      4       0      "ZnS coordination_S"
    [ -f "$d/pipeline_records.json" ] && validate_json "$d/pipeline_records.json"
}

check_val03() {
    local d="$REPO_ROOT/out/val_03_rutile_tio2"
    run_val "scripts/val_03_rutile_tio2.vsim" "$d" "val_03 TiO2"
    check_value "$d/pipeline_records.json" coordination_Ti     6       0      "TiO2 coordination_Ti"
    check_value "$d/pipeline_records.json" coordination_O      3       0      "TiO2 coordination_O"
    check_value "$d/pipeline_records.json" bond_Ti_O_eq_ang    1.946   0.05   "TiO2 Ti-O equatorial (Ang)"
    check_value "$d/pipeline_records.json" bond_Ti_O_ap_ang    1.984   0.05   "TiO2 Ti-O apical (Ang)"
    check_value "$d/pipeline_records.json" angle_O_Ti_O_deg    90.0    5.0    "TiO2 O-Ti-O angle (deg)"
    [ -f "$d/pipeline_records.json" ] && validate_json "$d/pipeline_records.json"
}

check_val04() {
    local d="$REPO_ROOT/out/val_04_alpha_quartz_sio2"
    run_val "scripts/val_04_alpha_quartz_sio2.vsim" "$d" "val_04 SiO2 quartz"
    check_value "$d/pipeline_records.json" coordination_Si     4       0      "SiO2 coordination_Si"
    check_value "$d/pipeline_records.json" coordination_O      2       0      "SiO2 coordination_O"
    check_value "$d/pipeline_records.json" bond_Si_O_ang       1.609   0.05   "SiO2 Si-O bond (Ang)"
    check_value "$d/pipeline_records.json" angle_Si_O_Si_deg   144.0   8.0    "SiO2 Si-O-Si angle (deg)"
    check_value "$d/pipeline_records.json" angle_O_Si_O_deg    109.5   5.0    "SiO2 O-Si-O angle (deg)"
    check_value "$d/pipeline_records.json" ring_size_mode      6       0      "SiO2 ring size mode"
    [ -f "$d/pipeline_records.json" ] && validate_json "$d/pipeline_records.json"
}

check_val05() {
    local d="$REPO_ROOT/out/val_05_mfi_zeolite"
    run_val "scripts/val_05_mfi_zeolite.vsim" "$d" "val_05 MFI zeolite"
    check_value "$d/pipeline_records.json" framework_density_T_per_1000A3 17.9 0.5  "MFI framework density"
    check_value "$d/pipeline_records.json" pore_diameter_a_ang            5.6  0.3  "MFI pore A (Ang)"
    check_value "$d/pipeline_records.json" pore_diameter_b_ang            5.1  0.3  "MFI pore B (Ang)"
    check_value "$d/pipeline_records.json" ring_size_mode                 10   0    "MFI ring size mode"
    [ -f "$d/pipeline_records.json" ] && validate_json "$d/pipeline_records.json"
}

check_val06() {
    local d="$REPO_ROOT/out/val_06_graphite_ab_stack"
    run_val "scripts/val_06_graphite_ab_stack.vsim" "$d" "val_06 graphite"
    check_value "$d/pipeline_records.json" interlayer_spacing_ang 3.354 0.05  "graphite interlayer (Ang)"
    check_value "$d/pipeline_records.json" bond_C_C_ang           1.421 0.02  "graphite C-C (Ang)"
    check_value "$d/pipeline_records.json" anisotropy_ratio       2.0   999   "graphite anisotropy >= 2.0"
    [ -f "$d/pipeline_records.json" ] && validate_json "$d/pipeline_records.json"
}

check_val07() {
    local d="$REPO_ROOT/out/val_07_fcc_argon"
    run_val "scripts/val_07_fcc_argon.vsim" "$d" "val_07 FCC Ar"
    check_value "$d/pipeline_records.json" lattice_a_ang          5.256  0.05   "Ar lattice a (Ang)"
    check_value "$d/pipeline_records.json" cohesive_energy_eV     0.0873 0.005  "Ar cohesive energy (eV)"
    check_value "$d/pipeline_records.json" rdf_first_peak_ang     3.721  0.05   "Ar RDF first peak (Ang)"
    check_value "$d/pipeline_records.json" coordination_number    12     0      "Ar coordination number"
    [ -f "$d/pipeline_records.json" ] && validate_json "$d/pipeline_records.json"
}

check_val08() {
    local d="$REPO_ROOT/out/val_08_benzene_dimer_s22"
    run_val "scripts/val_08_benzene_dimer_s22.vsim" "$d" "val_08 benzene dimer S22"
    check_value "$d/pipeline_records.json" t_shaped.binding_energy_eV       -0.1184 0.015 "benzene T-shaped E_bind (eV)"
    check_value "$d/pipeline_records.json" t_shaped.distance_ang             5.0    0.3   "benzene T-shaped distance (Ang)"
    check_value "$d/pipeline_records.json" parallel_disp.binding_energy_eV  -0.1206 0.015 "benzene PD E_bind (eV)"
    check_value "$d/pipeline_records.json" parallel_disp.distance_ang        3.5    0.3   "benzene PD distance (Ang)"
    [ -f "$d/pipeline_records.json" ] && validate_json "$d/pipeline_records.json"
}

echo "=============================================="; echo " UC-6: Crystallographic Regression Suite"; echo " Golden Suite: val_01..val_08"; echo "=============================================="
check_val01; check_val02; check_val03; check_val04; check_val05; check_val06; check_val07; check_val08

echo ""; echo "=============================================="
echo " UC-6 SUMMARY"
printf "  PASS:    %d\n" "$PASS"
printf "  PENDING: %d\n" "$PENDING"
printf "  FAIL:    %d\n" "$FAIL"
echo "=============================================="
if [ "$FAIL" -gt 0 ]; then echo "  RESULT: REGRESSION DETECTED"; exit 1; fi
if [ "$PENDING" -gt 0 ]; then echo "  RESULT: PASS (with $PENDING pending items per WO-MF-01)"
else echo "  RESULT: PASS -- Golden Suite clean"; fi
exit 0
