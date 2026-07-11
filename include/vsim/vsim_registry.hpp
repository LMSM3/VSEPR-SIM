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
    // ── Core structure fields (B9-1 / B9-2) ──────────────────────────────────
    std::string prototype;            // Canonical key: "B1_NaCl", "A4_diamond"
    std::string space_group;          // Hermann-Mauguin: "Fm-3m", "Fd-3m"
    std::string basis;                // "Na:0,0,0; Cl:0.5,0.5,0.5"
    std::string generator;            // Generator tag used by structure builder
    int         coordination  = 0;    // Typical coordination number (0 = unset)
    std::string default_charge_model; // "formal", "neutral", "bader", ""
    bool        is_periodic   = true; // false for molecules / 0-D structures
    bool        populated     = false;// true when at least one field was resolved

    // ── Sub-registry fields (B9-3 through B9-11) ─────────────────────────────
    // Each field is the registry default; apply_registry_defaults() merges
    // these into VsimDocument only when the user has not set the field explicitly.

    // B9-3: material_class — broad family: "ionic", "metallic", "molecular",
    //        "covalent", "polymer", "bead", "porous", "ceramic"
    std::string material_class;

    // B9-4: run_mode — preferred run mode for this material family
    //        e.g. "relax", "md", "nvt", "npt"
    std::string default_run_mode;

    // B9-5: environment defaults — periodic flag already covered by is_periodic;
    //        medium hint for MD setup
    std::string default_medium;       // "vacuum", "water", "inert_gas"
    double      default_temperature = 300.0;
    double      default_pressure    = 0.0;

    // B9-6: solver — integration scheme hint
    //        "fire", "verlet", "leapfrog", "runge_kutta4"
    std::string default_solver;

    // B9-7: forcefield — potential family to use
    //        "ewald_formal", "lj_neutral", "tersoff", "reaxff", "bead_spring"
    std::string default_forcefield;

    // B9-8: observables — comma-separated list of analysis quantities
    //        the runtime should compute by default for this material class
    std::string default_observables;

    // B9-9: export_profile — named export preset (resolved by resolve_export_profile)
    //        "minimal", "standard", "research_report", "publication"
    std::string default_export_profile;

    // B9-10: geometry_source — how the initial geometry is generated
    //         "lattice_builder", "basis_expand", "from_cif", "random_pack", "bead_builder"
    std::string geometry_source;

    // B9-11: radiation — applicable radiation type for excite.* defaults
    //         "none", "laser_visible", "laser_uv", "xray", "electron_beam", "neutron"
    std::string default_radiation;

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
        if (proto == "B1_NaCl" || proto == "B1_MgO"
                || proto == "magnesia" || proto == "halite") {
            b.space_group = (proto == "B1_MgO" || proto == "magnesia") ? "Fm-3m" : "Fm-3m";
            b.basis       = (proto == "B1_MgO" || proto == "magnesia")
                                ? "Mg:0,0,0; O:0.5,0.5,0.5"
                                : "Na:0,0,0; Cl:0.5,0.5,0.5";
            b.generator = "ionic_rocksalt"; b.coordination = 6;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "B2_CsCl" || proto == "cesium_chloride" || proto == "cscl") {
            b.space_group = "Pm-3m"; b.basis = "Cs:0,0,0; Cl:0.5,0.5,0.5";
            b.generator = "ionic_cesium_chloride"; b.coordination = 8;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "C1_CaF2" || proto == "C1_CeO2_fluorite"
                || proto == "C1_ZrO2_fluorite_like" || proto == "C1_UO2_fluorite"
                || proto == "C1_ThO2_fluorite" || proto == "C1" || proto == "fluorite") {
            b.space_group = "Fm-3m";
            b.basis = "A:0,0,0; B:0.25,0.25,0.25; B:0.75,0.75,0.75";
            b.generator = "ionic_fluorite"; b.coordination = 8;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "Anti_C1_Li2O" || proto == "antifluorite") {
            b.space_group = "Fm-3m";
            b.basis = "O:0,0,0; Li:0.25,0.25,0.25; Li:0.75,0.75,0.75";
            b.generator = "ionic_antifluorite"; b.coordination = 4;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "B3_ZnS" || proto == "B3" || proto == "zincblende") {
            b.space_group = "F-43m"; b.basis = "Zn:0,0,0; S:0.25,0.25,0.25";
            b.generator = "ionic_zincblende"; b.coordination = 4;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "B4_ZnS" || proto == "B4_CdS" || proto == "B4" || proto == "wurtzite") {
            b.space_group = "P6_3mc";
            b.basis = "Zn:0.333,0.667,0; S:0.333,0.667,0.375";
            b.generator = "ionic_wurtzite"; b.coordination = 4;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "C4_TiO2" || proto == "rutile") {
            b.space_group = "P4_2/mnm"; b.basis = "Ti:0,0,0; O:0.305,0.305,0";
            b.generator = "ionic_rutile"; b.coordination = 6;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "ABO3_perovskite" || proto == "E21" || proto == "perovskite") {
            b.space_group = "Pm-3m";
            b.basis = "A:0,0,0; B:0.5,0.5,0.5; O:0.5,0.5,0; O:0.5,0,0.5; O:0,0.5,0.5";
            b.generator = "ionic_perovskite"; b.coordination = 12;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "AB2O4_spinel" || proto == "spinel") {
            b.space_group = "Fd-3m";
            b.basis = "A:0,0,0; B:0.625,0.625,0.625; O:0.375,0.375,0.375";
            b.generator = "ionic_spinel"; b.coordination = 4;
            b.default_charge_model = "formal"; b.is_periodic = true;
        // ── oxides / ceramics ─────────────────────────────────────────────────
        } else if (proto == "D5_Al2O3_corundum" || proto == "alpha_alumina" || proto == "corundum") {
            b.space_group = "R-3c"; b.basis = "Al:0,0,0.352; O:0.306,0,0.25";
            b.generator = "ionic_corundum"; b.coordination = 6;
            b.default_charge_model = "formal"; b.is_periodic = true;
        // ── elemental metals / simple crystals ────────────────────────────────
        } else if (proto == "A_cP1" || proto == "A1_cubic" || proto == "simple_cubic") {
            b.space_group = "Pm-3m"; b.basis = "X:0,0,0";
            b.generator = "simple_cubic"; b.coordination = 6;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "A2_bcc" || proto == "bcc") {
            b.space_group = "Im-3m"; b.basis = "X:0,0,0; X:0.5,0.5,0.5";
            b.generator = "bcc_metal"; b.coordination = 8;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "A1_fcc" || proto == "fcc") {
            b.space_group = "Fm-3m";
            b.basis = "X:0,0,0; X:0.5,0.5,0; X:0.5,0,0.5; X:0,0.5,0.5";
            b.generator = "fcc_metal"; b.coordination = 12;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "A3_hcp" || proto == "hcp") {
            b.space_group = "P6_3/mmc"; b.basis = "X:0,0,0; X:0.333,0.667,0.5";
            b.generator = "hcp_metal"; b.coordination = 12;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "A4_diamond" || proto == "A4_Si" || proto == "diamond") {
            b.space_group = "Fd-3m"; b.basis = "X:0,0,0; X:0.25,0.25,0.25";
            b.generator = "diamond_cubic"; b.coordination = 4;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "A9_graphite" || proto == "graphite") {
            b.space_group = "P6_3/mmc";
            b.basis = "C:0,0,0; C:0.333,0.667,0; C:0,0,0.5; C:0.667,0.333,0.5";
            b.generator = "graphite_layered"; b.coordination = 3;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "A9_graphene_2D" || proto == "graphene") {
            b.space_group = "P6/mmm"; b.basis = "C:0,0,0; C:0.333,0.667,0";
            b.generator = "graphene_2d"; b.coordination = 3;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        // ── molecular geometry ────────────────────────────────────────────────
        } else if (proto == "geom_linear" || proto == "linear") {
            b.basis = "A:0,0,0; B:0,0,1"; b.generator = "geom_linear";
            b.coordination = 2; b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "geom_bent" || proto == "bent") {
            b.basis = "A:0,0,0; B:-0.5,0,0.866; B:0.5,0,0.866"; b.generator = "geom_bent";
            b.coordination = 2; b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "geom_trigonal_planar" || proto == "trigonal_planar") {
            b.basis = "A:0,0,0; B:1,0,0; B:-0.5,0.866,0; B:-0.5,-0.866,0";
            b.generator = "geom_trigonal_planar"; b.coordination = 3;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "geom_tetrahedral" || proto == "tetrahedral") {
            b.basis = "A:0,0,0; B:0.577,0.577,0.577; B:-0.577,-0.577,0.577; B:-0.577,0.577,-0.577; B:0.577,-0.577,-0.577";
            b.generator = "geom_tetrahedral"; b.coordination = 4;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "geom_trigonal_pyramidal" || proto == "trigonal_pyramidal") {
            b.basis = "A:0,0,0.2; B:0.577,0.577,0; B:-0.577,-0.577,0; B:-0.577,0.577,0";
            b.generator = "geom_trigonal_pyramidal"; b.coordination = 3;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "geom_octahedral" || proto == "octahedral") {
            b.basis = "A:0,0,0; B:1,0,0; B:-1,0,0; B:0,1,0; B:0,-1,0; B:0,0,1; B:0,0,-1";
            b.generator = "geom_octahedral"; b.coordination = 6;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "geom_square_planar" || proto == "square_planar") {
            b.basis = "A:0,0,0; B:1,0,0; B:-1,0,0; B:0,1,0; B:0,-1,0";
            b.generator = "geom_square_planar"; b.coordination = 4;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "geom_seesaw" || proto == "see_saw" || proto == "seesaw") {
            b.basis = "A:0,0,0; B:1,0,0; B:-1,0,0; B:0,1,0; B:0,0,1";
            b.generator = "geom_seesaw"; b.coordination = 4;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "geom_t_shaped" || proto == "t_shaped") {
            b.basis = "A:0,0,0; B:1,0,0; B:-1,0,0; B:0,1,0";
            b.generator = "geom_t_shaped"; b.coordination = 3;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        // ── polymers / organics ───────────────────────────────────────────────
        } else if (proto == "polymer_linear_chain" || proto == "linear_chain") {
            b.generator = "polymer_linear_chain"; b.coordination = 2;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "polymer_branched" || proto == "branched_chain") {
            b.generator = "polymer_branched"; b.coordination = 3;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "organic_aromatic_ring" || proto == "organic_benzene" || proto == "aromatic_ring" || proto == "benzene_ring") {
            b.generator = "organic_aromatic_ring"; b.coordination = 3;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "organic_alkane_chain" || proto == "alkane_chain") {
            b.generator = "organic_alkane_chain"; b.coordination = 2;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "organic_cycloalkane" || proto == "cycloalkane") {
            b.generator = "organic_cycloalkane"; b.coordination = 2;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        // ── porous / framework ────────────────────────────────────────────────
        } else if (proto == "framework_zeolite" || proto == "zeolite") {
            b.generator = "framework_zeolite"; b.coordination = 4;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "framework_mof" || proto == "mof") {
            b.generator = "framework_mof"; b.coordination = 4;
            b.default_charge_model = "formal"; b.is_periodic = true;
        } else if (proto == "framework_cof" || proto == "cof") {
            b.generator = "framework_cof"; b.coordination = 3;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "framework_prussian_blue_analog" || proto == "framework_prussian_blue" || proto == "prussian_blue" || proto == "pba") {
            b.space_group = "Fm-3m"; b.basis = "Fe:0,0,0; C:0.25,0,0; N:0.35,0,0";
            b.generator = "framework_prussian_blue"; b.coordination = 6;
            b.default_charge_model = "formal"; b.is_periodic = true;
        // ── bead / premacro ───────────────────────────────────────────────────
        } else if (proto == "bead_linear_chain" || proto == "bead_chain") {
            b.generator = "bead_linear_chain"; b.coordination = 2;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "bead_cluster_random" || proto == "bead_cluster") {
            b.generator = "bead_cluster_random"; b.coordination = 0;
            b.default_charge_model = "neutral"; b.is_periodic = false;
        } else if (proto == "premacro_powder_bed" || proto == "powder_bed") {
            b.generator = "premacro_powder_bed"; b.coordination = 0;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "premacro_packed_bed" || proto == "packed_bed") {
            b.generator = "premacro_packed_bed"; b.coordination = 0;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "premacro_granular_column" || proto == "granular_column") {
            b.generator = "premacro_granular_column"; b.coordination = 0;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "premacro_fiber_bundle" || proto == "fiber_bundle") {
            b.generator = "premacro_fiber_bundle"; b.coordination = 2;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        } else if (proto == "premacro_pipe_flow" || proto == "pipe_flow") {
            b.generator = "premacro_pipe_flow"; b.coordination = 0;
            b.default_charge_model = "neutral"; b.is_periodic = true;
        }

        b.populated = !b.generator.empty();

        // ── Sub-registry pass (B9-3 through B9-11) ───────────────────────────
        // Derive defaults from generator family. These are applied AFTER the
        // main structural resolution so every branch only sets what it needs.
        if (b.populated) {
            const std::string& gen = b.generator;

            // B9-3: material_class
            if (gen.find("ionic_") == 0 || gen == "ionic_rocksalt"
                    || gen == "ionic_fluorite" || gen == "ionic_antifluorite"
                    || gen == "ionic_corundum" || gen == "ionic_rutile"
                    || gen == "ionic_perovskite" || gen == "ionic_spinel"
                    || gen == "ionic_zincblende" || gen == "ionic_wurtzite"
                    || gen == "ionic_cesium_chloride")
                b.material_class = "ionic";
            else if (gen == "bcc_metal" || gen == "fcc_metal"
                    || gen == "hcp_metal" || gen == "simple_cubic")
                b.material_class = "metallic";
            else if (gen == "diamond_cubic" || gen == "graphite_layered"
                    || gen == "graphene_2d")
                b.material_class = "covalent";
            else if (gen.find("geom_") == 0)
                b.material_class = "molecular";
            else if (gen.find("polymer_") == 0 || gen.find("organic_") == 0)
                b.material_class = "polymer";
            else if (gen.find("bead_") == 0 || gen.find("premacro_") == 0)
                b.material_class = "bead";
            else if (gen.find("framework_") == 0)
                b.material_class = "porous";

            // B9-4: default_run_mode
            if (b.material_class == "ionic" || b.material_class == "covalent"
                    || b.material_class == "metallic")
                b.default_run_mode = "relax";
            else if (b.material_class == "molecular" || b.material_class == "polymer")
                b.default_run_mode = "md";
            else if (b.material_class == "bead")
                b.default_run_mode = "relax";
            else
                b.default_run_mode = "relax";

            // B9-5: environment defaults
            b.default_medium      = b.is_periodic ? "vacuum" : "vacuum";
            b.default_temperature = 300.0;
            b.default_pressure    = 0.0;

            // B9-6: default_solver
            if (b.material_class == "ionic" || b.material_class == "covalent"
                    || b.material_class == "metallic")
                b.default_solver = "fire";
            else if (b.material_class == "molecular")
                b.default_solver = "verlet";
            else if (b.material_class == "bead")
                b.default_solver = "fire";
            else
                b.default_solver = "fire";

            // B9-7: default_forcefield
            if (b.default_charge_model == "formal")
                b.default_forcefield = "ewald_formal";
            else if (b.material_class == "metallic")
                b.default_forcefield = "eam";
            else if (b.material_class == "covalent" && gen == "diamond_cubic")
                b.default_forcefield = "tersoff";
            else if (b.material_class == "covalent" && gen.find("graphit") != std::string::npos)
                b.default_forcefield = "tersoff";
            else if (b.material_class == "molecular")
                b.default_forcefield = "lj_neutral";
            else if (b.material_class == "polymer")
                b.default_forcefield = "lj_neutral";
            else if (b.material_class == "bead")
                b.default_forcefield = "bead_spring";
            else
                b.default_forcefield = "lj_neutral";

            // B9-8: default_observables
            if (b.material_class == "ionic")
                b.default_observables = "energy_map,coordination,rdf,diffusion";
            else if (b.material_class == "metallic")
                b.default_observables = "energy_map,coordination,rdf";
            else if (b.material_class == "covalent")
                b.default_observables = "energy_map,bond_angle,coordination";
            else if (b.material_class == "molecular")
                b.default_observables = "energy_map,bond_angle,dipole_moment";
            else if (b.material_class == "polymer")
                b.default_observables = "energy_map,end_to_end,radius_of_gyration";
            else if (b.material_class == "bead")
                b.default_observables = "energy_map,cluster_count";
            else
                b.default_observables = "energy_map";

            // B9-9: default_export_profile
            if (b.material_class == "ionic" || b.material_class == "covalent")
                b.default_export_profile = "research_report";
            else
                b.default_export_profile = "standard";

            // B9-10: geometry_source
            if (!b.basis.empty() && !b.space_group.empty())
                b.geometry_source = "lattice_builder";
            else if (!b.basis.empty())
                b.geometry_source = "basis_expand";
            else if (b.material_class == "bead")
                b.geometry_source = "bead_builder";
            else
                b.geometry_source = "random_pack";

            // B9-11: default_radiation
            if (b.material_class == "covalent")
                b.default_radiation = "laser_visible";
            else if (b.material_class == "ionic")
                b.default_radiation = "xray";
            else
                b.default_radiation = "none";
        }

        if (b.populated) {
            emit("prototype", b.prototype);
            if (!b.space_group.empty())         emit("space_group",          b.space_group);
            if (!b.basis.empty())               emit("basis",                b.basis);
            emit("generator",                                                 b.generator);
            if (b.coordination > 0)             emiti("coordination",        b.coordination);
            if (!b.default_charge_model.empty()) emit("default_charge_model",b.default_charge_model);
            if (!b.material_class.empty())       emit("material_class",       b.material_class);
            if (!b.default_run_mode.empty())     emit("default_run_mode",     b.default_run_mode);
            if (!b.default_solver.empty())       emit("default_solver",       b.default_solver);
            if (!b.default_forcefield.empty())   emit("default_forcefield",   b.default_forcefield);
            if (!b.default_observables.empty())  emit("default_observables",  b.default_observables);
            if (!b.default_export_profile.empty()) emit("default_export_profile", b.default_export_profile);
            if (!b.geometry_source.empty())      emit("geometry_source",      b.geometry_source);
            if (!b.default_radiation.empty())    emit("default_radiation",    b.default_radiation);
            emit("default_medium",                                            b.default_medium);
            emit("default_temperature", std::to_string(b.default_temperature));
        }

        return b;
    }
};

} // namespace vsim
