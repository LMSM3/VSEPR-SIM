#!/usr/bin/env bash
# =====================================================================
# qa_ordeal_tests.sh
# VSIM Scripting Language — Ordeal Test Runner / Static Schema Gate
# =====================================================================

set -euo pipefail

VSIM_FILE="${1:-ordeal_tests.vsim}"

declare -A KV
declare -A SECTIONS

fail() {
    printf '[FAIL] qa_ordeal_tests.sh\n' >&2
    printf '       %s\n' "$*" >&2
    exit 1
}

pass() {
    printf '[PASS] qa_ordeal_tests.sh\n'
    printf '       Parsed and gated: %s\n' "$VSIM_FILE"
    printf '       Ordeal suite confirms complex valid schema breadth.\n'
}

trim() {
    local s="$*"
    while [[ "$s" =~ ^[[:space:]] ]]; do s="${s#?}"; done
    while [[ "$s" =~ [[:space:]]$ ]]; do s="${s%?}"; done
    printf '%s' "$s"
}

strip_inline_comment() {
    local raw="$1"
    local out=""
    local in_quote=0
    local c
    local i
    for ((i = 0; i < ${#raw}; i++)); do
        c="${raw:i:1}"
        if [[ "$c" == '"' ]]; then
            if (( in_quote == 0 )); then in_quote=1; else in_quote=0; fi
        fi
        if [[ "$c" == '#' && $in_quote -eq 0 ]]; then break; fi
        out+="$c"
    done
    trim "$out"
}

parse_vsim_file() {
    local path="$1"
    [[ -f "$path" ]] || fail "Could not open VSIM file: $path"
    local current=""
    local line=""
    local cleaned=""
    local key=""
    local value=""
    local line_no=0
    local continuation_key=""
    while IFS= read -r line || [[ -n "$line" ]]; do
        ((line_no += 1))
        cleaned="$(strip_inline_comment "$line")"
        [[ -z "$cleaned" ]] && continue
        if [[ "$cleaned" =~ ^\[(.+)\]$ ]]; then
            current="${BASH_REMATCH[1]}"
            SECTIONS["$current"]=1
            continuation_key=""
            continue
        fi
        [[ -n "$current" ]] || fail "Key-value pair before any section at line $line_no"
        if [[ "$cleaned" == *'='* ]]; then
            key="$(trim "${cleaned%%=*}")"
            value="$(trim "${cleaned#*=}")"
            [[ -n "$key" ]] || fail "Empty key at line $line_no"
            KV["$current|$key"]="$value"
            if [[ "$value" == *, ]]; then
                continuation_key="$key"
            else
                continuation_key=""
            fi
            continue
        fi
        if [[ -n "$continuation_key" ]]; then
            KV["$current|$continuation_key"]+="$cleaned"
            if [[ "$cleaned" != *, ]]; then continuation_key=""; fi
            continue
        fi
        fail "Invalid line without '=' at $line_no: $cleaned"
    done < "$path"
}

has_section() { [[ -n "${SECTIONS[$1]+x}" ]]; }
has_key()     { [[ -n "${KV[$1|$2]+x}" ]]; }

get_key() {
    has_section "$1" || fail "Missing section: [$1]"
    has_key "$1" "$2" || fail "Missing key '$2' in [$1]"
    printf '%s' "${KV[$1|$2]}"
}

require_value() {
    local actual
    actual="$(get_key "$1" "$2")"
    [[ "$actual" == "$3" ]] || fail "Expected [$1].$2 = $3, got $actual"
}

forbid_key() {
    has_section "$1" || fail "Missing section: [$1]"
    if has_key "$1" "$2"; then
        fail "Forbidden key '$2' found in [$1]"
    fi
}

require_section() {
    has_section "$1" || fail "Missing required ordeal section: [$1]"
}

test_section_names() {
    local section
    for section in "${!SECTIONS[@]}"; do
        [[ "$section" == test.* ]] || continue
        [[ "$section" == *.run ]] && continue
        [[ "$section" == *.analysis ]] && continue
        [[ "$section" == *.perturbation ]] && continue
        printf '%s\n' "$section"
    done | sort
}

gate_suite_header() {
    require_value "suite" "name"    "ordeal_tests"
    require_value "suite" "level"   "ordeal"
    require_value "suite" "purpose" "complex_valid_schema_breadth_examples"
}

gate_no_redundant_ids() {
    local section
    while IFS= read -r section; do
        [[ -z "$section" ]] && continue
        forbid_key "$section" "id"
    done < <(test_section_names)
}

gate_no_placeholder_hashes() {
    local joined value
    for joined in "${!KV[@]}"; do
        [[ "$joined" == *"|expected_hash" ]] || continue
        value="${KV[$joined]}"
        if [[ "$value" == PLACEHOLDER_* ]]; then
            fail "PLACEHOLDER hash forbidden in [${joined%|expected_hash}]"
        fi
    done
}

gate_required_ordeal_sections() {
    local required=(
        "test.Fe_Co_PBA" "test.Fe_Co_PBA.run" "test.Fe_Co_PBA.analysis"
        "test.borromean_rings" "test.borromean_rings.run" "test.borromean_rings.analysis"
        "test.AlMn_icosahedral_QC" "test.AlMn_icosahedral_QC.run" "test.AlMn_icosahedral_QC.analysis"
        "test.penrose_P3_surface" "test.penrose_P3_surface.run" "test.penrose_P3_surface.analysis"
    )
    local section
    for section in "${required[@]}"; do require_section "$section"; done
}

gate_fe_co_pba() {
    local s="test.Fe_Co_PBA"
    require_value "$s" "group"         "cage_network"
    require_value "$s" "type"          "crystal"
    require_value "$s" "formula"       "Fe4[Co(CN)6]3"
    require_value "$s" "topology"      "cubic_cage_CN_bridged"
    require_value "$s" "node_species"  "Fe,Co"
    require_value "$s" "linker"        "CN"
    require_value "$s" "vacancy_model" "Co_deficient_statistical"
    forbid_key "$s" "central_atom"
    forbid_key "$s" "ligands"
    forbid_key "$s" "lone_pairs"
    require_value "test.Fe_Co_PBA.run"      "canonical_target"  "framework_lattice"
    require_value "test.Fe_Co_PBA.analysis" "pore_volume"       "true"
    require_value "test.Fe_Co_PBA.analysis" "linker_angle_dist" "true"
}

gate_borromean_rings() {
    local s="test.borromean_rings"
    require_value "$s" "group"           "mechanical_bond"
    require_value "$s" "type"            "topology_object"
    require_value "$s" "topology"        "borromean_L6a4"
    require_value "$s" "component_count" "3"
    require_value "$s" "component_type"  "ring"
    require_value "$s" "pairwise_linked" "false"
    require_value "$s" "globally_linked" "true"
    forbid_key "$s" "formula"
    forbid_key "$s" "lattice"
    forbid_key "$s" "basis"
    forbid_key "$s" "central_atom"
    forbid_key "$s" "ligands"
    forbid_key "$s" "lone_pairs"
    require_value "test.borromean_rings.run"      "canonical_target" "topological_invariant"
    require_value "test.borromean_rings.analysis" "linking_number"   "true"
    require_value "test.borromean_rings.analysis" "writhe"           "true"
}

gate_almn_quasicrystal() {
    local s="test.AlMn_icosahedral_QC"
    require_value "$s" "group"            "quasicrystal"
    require_value "$s" "type"             "quasicrystal"
    require_value "$s" "formula"          "Al86Mn14"
    require_value "$s" "periodicity"      "none"
    require_value "$s" "long_range_order" "true"
    require_value "$s" "projection_from"  "6D_hypercubic"
    forbid_key "$s" "lattice"
    forbid_key "$s" "supercell"
    forbid_key "$s" "central_atom"
    forbid_key "$s" "ligands"
    forbid_key "$s" "lone_pairs"
    require_value "test.AlMn_icosahedral_QC.run"      "canonical_target"  "diffraction_pattern"
    require_value "test.AlMn_icosahedral_QC.analysis" "fivefold_symmetry" "true"
    require_value "test.AlMn_icosahedral_QC.analysis" "phason_strain"     "true"
}

gate_penrose_surface() {
    local s="test.penrose_P3_surface"
    require_value "$s" "group"       "aperiodic_surface"
    require_value "$s" "type"        "surface"
    require_value "$s" "topology"    "penrose_P3"
    require_value "$s" "tile_ratio"  "golden_ratio"
    require_value "$s" "periodicity" "none"
    require_value "$s" "bulk"        "false"
    require_value "$s" "patch_size"  "200_tiles"
    forbid_key "$s" "formula"
    forbid_key "$s" "lattice"
    forbid_key "$s" "basis"
    forbid_key "$s" "central_atom"
    forbid_key "$s" "ligands"
    forbid_key "$s" "lone_pairs"
    require_value "test.penrose_P3_surface.run"      "canonical_target"       "tiling_patch"
    require_value "test.penrose_P3_surface.run"      "seed"                   "42"
    require_value "test.penrose_P3_surface.analysis" "local_isomorphism"      "true"
    require_value "test.penrose_P3_surface.analysis" "tile_frequency_ratio"   "true"
}

run_all_gates() {
    gate_suite_header
    gate_no_redundant_ids
    gate_no_placeholder_hashes
    gate_required_ordeal_sections
    gate_fe_co_pba
    gate_borromean_rings
    gate_almn_quasicrystal
    gate_penrose_surface
}

main() {
    parse_vsim_file "$VSIM_FILE"
    run_all_gates
    pass
}

main "$@"
