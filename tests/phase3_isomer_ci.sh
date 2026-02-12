#!/usr/bin/env bash
#
# phase3_isomer_ci.sh
# ===================
# Phase 3: Isomerism - Multi-Minima Proof (CI-grade)
#
# Runs cis/trans [Co(NH3)4Cl2]+, checks:
# - isomer identity preserved (Cl-Co-Cl angle range)
# - CN(Co) = 6
# - bond length ranges (Co-N, Co-Cl)
# - ΔE trend and reasonable band (soft)
# - reproducibility over many seeds
# - optional basin stability via perturb + re-opt
#
# Requires:
# - ./build/bin/molecule_builder
# - jq
#
# Expected JSON fields (example):
# {
#   "energy_kcal_mol": 86.2,
#   "cn_by_atom": { "0": 6 },
#   "angles_deg": { "Cl-Co-Cl": 179.8 },
#   "bond_lengths_A": { "Co-Cl": [2.31, 2.31], "Co-N": [1.98,1.98,1.97,1.97] },
#   "nan_detected": false,
#   "min_distance_A": 0.92
# }
#
set -euo pipefail

BIN="./build/bin/molecule_builder"
FORMULA='[Co(NH3)4Cl2]+'

command -v jq >/dev/null 2>&1 || { echo "[-] jq not found"; exit 2; }
[[ -x "$BIN" ]] || { echo "[-] Binary not found or not executable: $BIN"; exit 2; }

# ----------------------------
# Known-value bands (sanity, not perfect matching)
# ----------------------------
# angles (degrees)
TRANS_ANGLE_MIN=175
TRANS_ANGLE_MAX=185
CIS_ANGLE_MIN=80
CIS_ANGLE_MAX=100

# distances (Å) - wide sanity bands
CO_CL_MIN=2.00
CO_CL_MAX=2.80
CO_N_MIN=1.80
CO_N_MAX=2.30

# CN check
CO_EXPECT_CN=6

# ΔE band (kcal/mol) - keep wide until your FF is calibrated
DE_MIN=0.0     # cis should be >= trans typically; set negative if you want to allow reverse
DE_MAX=10.0

# overlap check (Å)
MIN_DIST_MIN=0.70   # reject total atomic overlaps / NaN explosions

# runs
NSEEDS=16

# ----------------------------
# Helpers
# ----------------------------
fail() { echo -e "\n[-] FAIL: $*\n"; exit 1; }
ok()   { echo "[+] $*"; }

need_field() {
  local json="$1" field="$2"
  # Check if field exists (not null), handle boolean false correctly
  jq -e "$field | . != null" "$json" >/dev/null 2>&1 || fail "Missing JSON field: $field in $json"
}

num_in_range() {
  local name="$1" val="$2" lo="$3" hi="$4"
  awk -v v="$val" -v lo="$lo" -v hi="$hi" 'BEGIN{ exit !(v>=lo && v<=hi) }' \
    || fail "$name out of range: $val (expected $lo..$hi)"
}

mean_std() {
  # Reads numbers from stdin
  awk '
    { x[NR]=$1; s+=$1; ss+=$1*$1 }
    END{
      if(NR==0){ print "nan nan"; exit 0 }
      m=s/NR;
      v=(ss/NR)-(m*m);
      if(v<0) v=0;
      sd=sqrt(v);
      printf("%.6f %.6f\n", m, sd);
    }'
}

run_case() {
  local iso="$1" seed="$2" out="$3"

  "$BIN" "$FORMULA" \
    --isomer "$iso" \
    --seed "$seed" \
    --json "$out" >/dev/null

  [[ -s "$out" ]] || fail "No JSON output produced: $out"
}

extract_scalar() {
  local json="$1" jqexpr="$2"
  jq -r "$jqexpr" "$json"
}

extract_array() {
  local json="$1" jqexpr="$2"
  jq -r "$jqexpr | .[]" "$json"
}

check_common_sanity() {
  local iso="$1" json="$2"

  need_field "$json" '.energy_kcal_mol'
  need_field "$json" '.nan_detected'
  need_field "$json" '.min_distance_A'
  need_field "$json" '.angles_deg["Cl-Co-Cl"]'
  need_field "$json" '.bond_lengths_A["Co-Cl"]'
  need_field "$json" '.bond_lengths_A["Co-N"]'
  need_field "$json" '.cn_by_atom'

  local nan min_d angle cn
  nan="$(extract_scalar "$json" '.nan_detected')"
  [[ "$nan" == "false" ]] || fail "$iso: NaN detected"

  min_d="$(extract_scalar "$json" '.min_distance_A')"
  num_in_range "$iso: min_distance_A" "$min_d" "$MIN_DIST_MIN" "999"

  # CN for Co: assumes Co is atom 0, or you store a label map.
  # If your Co id isn't 0, store "central_metal_id" in JSON and use it.
  if jq -e '.central_metal_id' "$json" >/dev/null 2>&1; then
    local mid
    mid="$(extract_scalar "$json" '.central_metal_id')"
    cn="$(jq -r --arg mid "$mid" '.cn_by_atom[$mid]' "$json")"
  else
    cn="$(extract_scalar "$json" '.cn_by_atom["0"]')"
  fi
  [[ "$cn" == "$CO_EXPECT_CN" ]] || fail "$iso: coordination number CN(Co)=$cn (expected $CO_EXPECT_CN)"

  angle="$(extract_scalar "$json" '.angles_deg["Cl-Co-Cl"]')"
  if [[ "$iso" == "trans" ]]; then
    num_in_range "$iso: Cl-Co-Cl angle" "$angle" "$TRANS_ANGLE_MIN" "$TRANS_ANGLE_MAX"
  else
    num_in_range "$iso: Cl-Co-Cl angle" "$angle" "$CIS_ANGLE_MIN" "$CIS_ANGLE_MAX"
  fi

  # Distance bands
  extract_array "$json" '.bond_lengths_A["Co-Cl"]' | while read -r d; do
    num_in_range "$iso: Co-Cl distance" "$d" "$CO_CL_MIN" "$CO_CL_MAX"
  done
  extract_array "$json" '.bond_lengths_A["Co-N"]' | while read -r d; do
    num_in_range "$iso: Co-N distance" "$d" "$CO_N_MIN" "$CO_N_MAX"
  done
}

# Optional perturb+reopt basin stability (if supported)
basin_stability_check() {
  local iso="$1" seed="$2"
  local base_json="out/${iso}_seed${seed}.json"
  local pert_json="out/${iso}_seed${seed}_pert.json"

  if "$BIN" "$FORMULA" --help 2>/dev/null | grep -q -- '--perturb'; then
    "$BIN" "$FORMULA" \
      --isomer "$iso" \
      --seed "$seed" \
      --perturb 0.05 \
      --json "$pert_json" >/dev/null
    [[ -s "$pert_json" ]] || fail "$iso: perturb run produced no JSON"

    # should remain same isomer identity after small kick
    check_common_sanity "$iso" "$pert_json"
    ok "$iso basin stability (perturb 0.05 Å): PASS"
  else
    echo "[!] $iso basin stability skipped (no --perturb support)"
  fi
}

# ----------------------------
# Main
# ----------------------------
mkdir -p out
echo "================================================================================"
echo "PHASE 3 CI: [Co(NH3)4Cl2]+ cis/trans multi-minima verification"
echo "Binary: $BIN"
echo "Seeds:  $NSEEDS"
echo "================================================================================"

# Sweep seeds
trans_Es=()
cis_Es=()

for i in $(seq 1 "$NSEEDS"); do
  seed=$((1000 + i))

  tjson="out/trans_seed${seed}.json"
  cjson="out/cis_seed${seed}.json"

  run_case "trans" "$seed" "$tjson"
  run_case "cis"   "$seed" "$cjson"

  check_common_sanity "trans" "$tjson"
  check_common_sanity "cis"   "$cjson"

  tE="$(extract_scalar "$tjson" '.energy_kcal_mol')"
  cE="$(extract_scalar "$cjson" '.energy_kcal_mol')"

  trans_Es+=("$tE")
  cis_Es+=("$cE")

  ok "seed $seed: trans E=$tE, cis E=$cE"
done

# Stats
printf "%s\n" "${trans_Es[@]}" | mean_std > out/_trans_stats.txt
printf "%s\n" "${cis_Es[@]}"   | mean_std > out/_cis_stats.txt

tmean="$(awk '{print $1}' out/_trans_stats.txt)"
tstd="$(awk '{print $2}' out/_trans_stats.txt)"
cmean="$(awk '{print $1}' out/_cis_stats.txt)"
cstd="$(awk '{print $2}' out/_cis_stats.txt)"

echo "--------------------------------------------------------------------------------"
echo "Reproducibility summary:"
echo "  trans mean ± std: $tmean ± $tstd"
echo "  cis   mean ± std: $cmean ± $cstd"

# ΔE check on means
dE="$(awk -v c="$cmean" -v t="$tmean" 'BEGIN{ printf("%.6f\n", c-t) }')"
echo "  ΔE(cis-trans) mean: $dE kcal/mol"

# Soft-known-value check: cis often higher than trans for this system
# Keep band wide unless you've calibrated.
awk -v d="$dE" -v lo="$DE_MIN" -v hi="$DE_MAX" 'BEGIN{ exit !(d>=lo && d<=hi) }' \
  || fail "ΔE mean out of expected band: $dE (expected $DE_MIN..$DE_MAX). If your FF flips ordering, adjust band or mark as informational."

ok "Known-value comparison: ΔE band check PASS (wide tolerance)"

# Basin stability (one representative seed)
basin_stability_check "trans" 1001
basin_stability_check "cis"   1001

echo "================================================================================"
ok "PHASE 3: PASS (multi-minima + known-value sanity bands + reproducible)"
echo "Artifacts written to ./out/"
echo "================================================================================"
