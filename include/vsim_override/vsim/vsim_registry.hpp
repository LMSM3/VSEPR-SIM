#pragma once
// =============================================================================
// vsim_registry.hpp  —  WO-VSIM-03C  Registry Resolution Engine
// =============================================================================
//
// RegistryBundle  — crystallographic expansion of a resolved prototype key.
// RegistryResolver — maps prototype keys to RegistryBundles + logs [REGISTRY].
//
// Include this header AFTER or ALONGSIDE vsim_document.hpp.
// vsim_runtime.hpp includes it explicitly.
// =============================================================================

#include "vsim_document.hpp"

#include <iostream>
#include <ostream>
#include <string>

namespace vsim {

// ============================================================================
// RegistryBundle
// ============================================================================
//
// Crystallographic expansion of a resolved prototype key.
// Produced by RegistryResolver::resolve(MaterialSection&, ostream&).
//
struct RegistryBundle {
    std::string prototype;            // Canonical key: "B1_NaCl", "A4_diamond"
    std::string space_group;          // Hermann-Mauguin: "Fm-3m", "Fd-3m"
    std::string basis;                // "Na:0,0,0; Cl:0.5,0.5,0.5"
    std::string generator;            // Generator tag used by structure builder
    int         coordination  = 0;    // Typical coordination number (0 = unset)
    std::string default_charge_model; // "formal", "neutral", "bader", ""
    bool        is_periodic   = true; // false for molecules / 0-D structures
    bool        populated     = false;// true when at least one field was resolved

    bool empty() const { return !populated; }
};

// ============================================================================
// RegistryResolver
// ============================================================================

struct RegistryResolver {

    // resolve — expand MaterialSection resolved prototype into a RegistryBundle.
    // Logs each resolved field to log with the [REGISTRY] prefix.
    static RegistryBundle resolve(const MaterialSection& mat, std::ostream& log) {
        const std::string proto = mat.resolved_prototype();
        const std::string alias = mat.structure;
        return resolve_proto(proto, alias, log);
    }

    // Convenience overload — logs to std::cout.
    static RegistryBundle resolve(const MaterialSection& mat) {
        return resolve(mat, std::cout);
    }

    // Core lookup by prototype key.
    static RegistryBundle resolve_proto(const std::string& proto,
                                        const std::string& alias,
                                        std::ostream& log) {
        RegistryBundle b;
        b.prototype = proto;

        auto emit = [&](const char* field, const std::string& val) {
            log << "[REGISTRY] material." << field << " <- " << val;
            if (!alias.empty() && alias != proto)
                log << "  (from alias " << alias << ")";
            log << "\n";
        };
        auto emiti = [&](const char* field, int val) {
            emit(field, std::to_string(val));
        };

        // ── ionic / salts ────────────────────────────────────────────────────
        if (proto == "B1_NaCl" || proto == "B1_MgO") {
            b.space_group = (proto == "B1_MgO") ? "Fm-3m" : "Fm-3m";
            b.basis       = (proto == "B1_MgO") ? "Mg:0,0,0; O:0.5,0.5,0.5"
                                                 : "Na:0,0,0; Cl:0.5,0.5,0.5";
            b.generator = "ionic_rocksalt"; b.coordination = 6;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "B2_CsCl") {
            b.space_group = "Pm-3m"; b.basis = "Cs:0,0,0; Cl:0.5,0.5,0.5";
            b.generator = "ionic_cesium_chloride"; b.coordination = 8;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "C1_CaF2" || proto == "C1_CeO2_fluorite"
                || proto == "C1_ZrO2_fluorite_like" || proto == "C1_UO2_fluorite"
                || proto == "C1_ThO2_fluorite") {
            b.space_group = "Fm-3m";
            b.basis = "A:0,0,0; B:0.25,0.25,0.25; B:0.75,0.75,0.75";
            b.generator = "ionic_fluorite"; b.coordination = 8;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "Anti_C1_Li2O") {
            b.space_group = "Fm-3m";
            b.basis = "O:0,0,0; Li:0.25,0.25,0.25; Li:0.75,0.75,0.75";
            b.generator = "ionic_antifluorite"; b.coordination = 4;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "B3_ZnS") {
            b.space_group = "F-43m"; b.basis = "Zn:0,0,0; S:0.25,0.25,0.25";
            b.generator = "ionic_zincblende"; b.coordination = 4;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "B4_ZnS" || proto == "B4_CdS") {
            b.space_group = "P6_3mc";
            b.basis = "Zn:0.333,0.667,0; S:0.333,0.667,0.375";
            b.generator = "ionic_wurtzite"; b.coordination = 4;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "C4_TiO2") {
            b.space_group = "P4_2/mnm"; b.basis = "Ti:0,0,0; O:0.305,0.305,0";
            b.generator = "ionic_rutile"; b.coordination = 6;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "ABO3_perovskite") {
            b.space_group = "Pm-3m";
            b.basis = "A:0,0,0; B:0.5,0.5,0.5; O:0.5,0.5,0; O:0.5,0,0.5; O:0,0.5,0.5";
            b.generator = "ionic_perovskite"; b.coordination = 12;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "AB2O4_spinel") {
            b.space_group = "Fd-3m";
            b.basis = "A:0,0,0; B:0.625,0.625,0.625; O:0.375,0.375,0.375";
            b.generator = "ionic_spinel"; b.coordination = 4;
            b.default_charge_model = "formal"; b.is_periodic = true;
        // ── oxides / ceramics ─────────────────────────────────────────────────
        } else if (proto == "D5_Al2O3_corundum") {
            b.space_group = "R-3c"; b.basis = "Al:0,0,0.352; O:0.306,0,0.25";
            b.generator = "ionic_corundum"; b.coordination = 6;
            b.default_charge_model = "formal"; b.is_periodic = true;
        // ── elemental metals / simple crystals ────────────────────────────────
        } else if (proto == "A_cP1") {
            b.space_group = "Pm-3m"; b.basis = "X:0,0,0";
            b.generator = "simple_cubic"; b.coordination = 6;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "A2_bcc") {
            b.space_group = "Im-3m"; b.basis = "X:0,0,0; X:0.5,0.5,0.5";
            b.generator = "bcc_metal"; b.coordination = 8;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "A1_fcc") {
            b.space_group = "Fm-3m";
            b.basis = "X:0,0,0; X:0.5,0.5,0; X:0.5,0,0.5; X:0,0.5,0.5";
            b.generator = "fcc_metal"; b.coordination = 12;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "A3_hcp") {
            b.space_group = "P6_3/mmc"; b.basis = "X:0,0,0; X:0.333,0.667,0.5";
            b.generator = "hcp_metal"; b.coordination = 12;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "A4_diamond") {
            b.space_group = "Fd-3m"; b.basis = "X:0,0,0; X:0.25,0.25,0.25";
            b.generator = "diamond_cubic"; b.coordination = 4;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "A9_graphite") {
            b.space_group = "P6_3/mmc";
            b.basis = "C:0,0,0; C:0.333,0.667,0; C:0,0,0.5; C:0.667,0.333,0.5";
            b.generator = "graphite_layered"; b.coordination = 3;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "A9_graphene_2D") {
            b.space_group = "P6/mmm"; b.basis = "C:0,0,0; C:0.333,0.667,0";
            b.generator = "graphene_2d"; b.coordination = 3;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        // ── molecular geometry ────────────────────────────────────────────────
        } else if (proto == "geom_linear") {
            b.basis = "A:0,0,0; B:0,0,1"; b.generator = "geom_linear";
            b.coordination = 2; b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "geom_bent") {
            b.basis = "A:0,0,0; B:-0.5,0,0.866; B:0.5,0,0.866"; b.generator = "geom_bent";
            b.coordination = 2; b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "geom_trigonal_planar") {
            b.basis = "A:0,0,0; B:1,0,0; B:-0.5,0.866,0; B:-0.5,-0.866,0";
            b.generator = "geom_trigonal_planar"; b.coordination = 3;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "geom_tetrahedral") {
            b.basis = "A:0,0,0; B:0.577,0.577,0.577; B:-0.577,-0.577,0.577; B:-0.577,0.577,-0.577; B:0.577,-0.577,-0.577";
            b.generator = "geom_tetrahedral"; b.coordination = 4;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "geom_trigonal_pyramidal") {
            b.basis = "A:0,0,0.2; B:0.577,0.577,0; B:-0.577,-0.577,0; B:-0.577,0.577,0";
            b.generator = "geom_trigonal_pyramidal"; b.coordination = 3;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "geom_octahedral") {
            b.basis = "A:0,0,0; B:1,0,0; B:-1,0,0; B:0,1,0; B:0,-1,0; B:0,0,1; B:0,0,-1";
            b.generator = "geom_octahedral"; b.coordination = 6;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "geom_square_planar") {
            b.basis = "A:0,0,0; B:1,0,0; B:-1,0,0; B:0,1,0; B:0,-1,0";
            b.generator = "geom_square_planar"; b.coordination = 4;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "geom_seesaw") {
            b.basis = "A:0,0,0; B:1,0,0; B:-1,0,0; B:0,1,0; B:0,0,1";
            b.generator = "geom_seesaw"; b.coordination = 4;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "geom_t_shaped") {
            b.basis = "A:0,0,0; B:1,0,0; B:-1,0,0; B:0,1,0";
            b.generator = "geom_t_shaped"; b.coordination = 3;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        // ── polymers / organics ───────────────────────────────────────────────
        } else if (proto == "polymer_linear_chain") {
            b.generator = "polymer_linear_chain"; b.coordination = 2;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "polymer_branched") {
            b.generator = "polymer_branched"; b.coordination = 3;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "organic_aromatic_ring" || proto == "organic_benzene") {
            b.generator = "organic_aromatic_ring"; b.coordination = 3;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "organic_alkane_chain") {
            b.generator = "organic_alkane_chain"; b.coordination = 2;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "organic_cycloalkane") {
            b.generator = "organic_cycloalkane"; b.coordination = 2;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        // ── porous / framework ────────────────────────────────────────────────
        } else if (proto == "framework_zeolite") {
            b.generator = "framework_zeolite"; b.coordination = 4;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "framework_mof") {
            b.generator = "framework_mof"; b.coordination = 4;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "framework_cof") {
            b.generator = "framework_cof"; b.coordination = 3;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "framework_prussian_blue_analog" || proto == "framework_prussian_blue") {
            b.space_group = "Fm-3m"; b.basis = "Fe:0,0,0; C:0.25,0,0; N:0.35,0,0";
            b.generator = "framework_prussian_blue"; b.coordination = 6;
            b.default_charge_model = "formal"; b.is_periodic = true;
        // ── bead / premacro ───────────────────────────────────────────────────
        } else if (proto == "bead_linear_chain") {
            b.generator = "bead_linear_chain"; b.coordination = 2;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "bead_cluster_random") {
            b.generator = "bead_cluster_random"; b.coordination = 0;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "premacro_powder_bed") {
            b.generator = "premacro_powder_bed"; b.coordination = 0;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "premacro_packed_bed") {
            b.generator = "premacro_packed_bed"; b.coordination = 0;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "premacro_granular_column") {
            b.generator = "premacro_granular_column"; b.coordination = 0;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "premacro_fiber_bundle") {
            b.generator = "premacro_fiber_bundle"; b.coordination = 2;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "premacro_pipe_flow") {
            b.generator = "premacro_pipe_flow"; b.coordination = 0;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        }

        b.populated = !b.generator.empty();

        if (b.populated) {
            emit("prototype", b.prototype);
            if (!b.space_group.empty()) emit("space_group", b.space_group);
            if (!b.basis.empty())       emit("basis",       b.basis);
            emit("generator", b.generator);
            if (b.coordination > 0)     emiti("coordination", b.coordination);
            if (!b.default_charge_model.empty())
                emit("default_charge_model", b.default_charge_model);
        }

        return b;
    }
};

} // namespace vsim
