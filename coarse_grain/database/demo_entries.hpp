#pragma once
/**
 * demo_entries.hpp — Canonical Demonstration Entries
 *
 * Provides factory functions for the demonstration records described
 * in Reports 1–7 of the Day 40C Database Architecture Demonstration Packet:
 *
 *   Elements:    H, Fe, U, Pu
 *   Materials:   Copper, Steel A36, UO2, Thorium Oxalate, Pu residue
 *   Compounds:   Water, Hexene, Cyclopentadienyl-like, Thorium oxalate, UO2
 *   Precursors:  Hexene bead, Cp ligand, Thorium oxalate slurry,
 *                Dissolved nitrate-metal, Pipe-scale solids
 *   Hybrid:      Pu + Cp + Hexene mixed-resolution packet (Report 7)
 *
 * Every entry carries computed seed and hash blocks.
 */

#include "coarse_grain/database/input_record.hpp"
#include "coarse_grain/database/material_record.hpp"
#include "coarse_grain/database/compound_record.hpp"
#include "coarse_grain/database/precursor_record.hpp"
#include <vector>

namespace vsepr {
namespace database {
namespace demo {

// ============================================================================
// Element Cores — Report 1.5
// ============================================================================

inline InputRecord hydrogen_core() {
    InputRecord r;
    r.identity = {"elem_001_H", "Hydrogen", "H", 1, "", ""};
    r.type      = RecordType::ELEMENT;
    r.dimension = dim_element_core();
    r.payload.set_u32("Z", 1);
    r.payload.set_str("name", "Hydrogen");
    r.payload.set_str("symbol", "H");
    r.payload.set_str("class", "nonmetal");
    r.payload.set_f64("atomic_mass", 1.008);
    r.payload.set_f64("electronegativity_pauling", 2.20);
    r.payload.set_vec_i32("oxidation_states", {-1, 1});
    r.seed.seed_32 = 1;
    r.compute_hashes();
    return r;
}

inline InputRecord iron_core() {
    InputRecord r;
    r.identity = {"elem_026_Fe", "Iron", "Fe", 26, "", "7439-89-6"};
    r.type      = RecordType::ELEMENT;
    r.dimension = dim_element_core();
    r.payload.set_u32("Z", 26);
    r.payload.set_str("name", "Iron");
    r.payload.set_str("symbol", "Fe");
    r.payload.set_str("class", "transition_metal");
    r.payload.set_f64("atomic_mass", 55.845);
    r.payload.set_f64("electronegativity_pauling", 1.83);
    r.payload.set_vec_i32("oxidation_states", {-2, -1, 0, 1, 2, 3, 4, 5, 6});
    r.seed.seed_32 = 26;
    r.compute_hashes();
    return r;
}

inline InputRecord uranium_core() {
    InputRecord r;
    r.identity = {"elem_092_U", "Uranium", "U", 92, "", "7440-61-1"};
    r.type      = RecordType::ELEMENT;
    r.dimension = dim_element_core();
    r.dimension.n_nuc = 6;  // extended nuclear descriptors
    r.payload.set_u32("Z", 92);
    r.payload.set_str("name", "Uranium");
    r.payload.set_str("symbol", "U");
    r.payload.set_str("class", "actinide");
    r.payload.set_f64("atomic_mass", 238.029);
    r.payload.set_f64("electronegativity_pauling", 1.38);
    r.payload.set_vec_i32("oxidation_states", {2, 3, 4, 5, 6});
    r.payload.set_f64("half_life_s", 1.41e17);  // U-238
    r.payload.set_str("decay_mode", "alpha");
    r.seed.seed_32 = 92;
    r.compute_hashes();
    return r;
}

inline InputRecord plutonium_core() {
    InputRecord r;
    r.identity = {"elem_094_Pu", "Plutonium", "Pu", 94, "", "7440-07-5"};
    r.type      = RecordType::ELEMENT;
    r.dimension = dim_element_core();
    r.dimension.n_nuc = 8;  // rich nuclear descriptors
    r.payload.set_u32("Z", 94);
    r.payload.set_str("name", "Plutonium");
    r.payload.set_str("symbol", "Pu");
    r.payload.set_str("class", "actinide");
    r.payload.set_f64("atomic_mass", 244.0);
    r.payload.set_f64("electronegativity_pauling", 1.28);
    r.payload.set_vec_i32("oxidation_states", {3, 4, 5, 6, 7});
    r.payload.set_f64("half_life_s", 2.41e11);  // Pu-239
    r.payload.set_str("decay_mode", "alpha");
    r.payload.set_f64("displacement_energy_eV", 25.0);
    r.seed.seed_32 = 94;
    r.compute_hashes();
    return r;
}

/// All four element demo cores
inline std::vector<InputRecord> element_demo_set() {
    return {hydrogen_core(), iron_core(), uranium_core(), plutonium_core()};
}

// ============================================================================
// Atomistic Objects — Report 1.5 Example B
// ============================================================================

inline InputRecord plutonium_atomistic() {
    InputRecord r;
    r.identity = {"atom_094_Pu", "Plutonium Atomistic", "Pu", 94, "", ""};
    r.type      = RecordType::ATOMISTIC;
    r.dimension = dim_atomistic_object();
    r.dimension.n_nuc = 8;
    r.payload.set_vec_f64("position", {0.0, 0.0, 0.0});
    r.payload.set_vec_f64("velocity", {0.0, 0.0, 0.0});
    r.payload.set_f64("charge", 0.0);
    r.payload.set_f64("sigma_chem", 1.86);
    r.payload.set_f64("lambda", 1.57e-2);
    r.payload.set_f64("E_dec", 5.245);       // MeV
    r.payload.set_f64("N_emit", 2.88);
    r.payload.set_f64("tau", 2.41e11);        // seconds
    r.payload.set_str("mode", "alpha");
    r.seed = {94001, 94001094000, "lattice_fcc_v1", "actinide_decay_v1"};
    r.compute_hashes();
    return r;
}

// ============================================================================
// Bead Objects — Report 1.5 Example C
// ============================================================================

inline InputRecord cyclopentadienyl_bead() {
    InputRecord r;
    r.identity = {"bead_Cp_001", "Cyclopentadienyl Bead", "Cp", 0, "C5H5", ""};
    r.type      = RecordType::BEAD;
    r.dimension = dim_bead_object();
    r.payload.set_vec_f64("R", {0.0, 0.0, 0.0});
    r.payload.set_vec_f64("V", {0.0, 0.0, 0.0});
    r.payload.set_f64("Q", -1.0);
    r.payload.set_f64("Theta", 0.0);
    r.payload.set_f64("beta", 0.0);
    r.payload.set_f64("chi", 2.54);
    r.payload.set_f64("eta", 5.0);  // hapticity
    r.seed = {5001, 5001005000, "ring_planar_v1", "cp_ligand_v1"};
    r.compute_hashes();
    return r;
}

inline InputRecord hexene_bead() {
    InputRecord r;
    r.identity = {"bead_hexene_001", "Hexene Bead", "C6H12", 0, "C6H12", ""};
    r.type      = RecordType::BEAD;
    r.dimension = dim_bead_object();
    r.payload.set_vec_f64("R", {0.0, 0.0, 0.0});
    r.payload.set_vec_f64("V", {0.0, 0.0, 0.0});
    r.payload.set_f64("Q", 0.0);
    r.payload.set_f64("Theta", 0.0);
    r.payload.set_f64("beta", 0.0);
    r.payload.set_f64("chi", 2.50);
    r.payload.set_f64("eta", 0.0);
    r.seed = {6001, 6001012000, "chain_linear_v1", "alkene_v1"};
    r.compute_hashes();
    return r;
}

// ============================================================================
// Materials — Report 3.3
// ============================================================================

inline MaterialDatabaseRecord copper_material() {
    MaterialDatabaseRecord m;
    m.material_id     = "mat_Cu_001";
    m.name            = "Copper";
    m.material_class  = MaterialClass::ELEMENTAL_MATERIAL;
    m.composition.fractions = {{"Cu", 1.0}};
    m.density_kg_m3   = 8960.0;
    m.phase           = PhaseState::SOLID;
    m.properties.elastic_modulus      = 110.0;
    m.properties.yield_strength       = 70.0;
    m.properties.poisson_ratio        = 0.34;
    m.properties.thermal_conductivity = 401.0;
    m.properties.melting_point        = 1357.77;
    m.properties.specific_heat        = 385.0;
    m.compute_hash();
    return m;
}

inline MaterialDatabaseRecord steel_a36_material() {
    MaterialDatabaseRecord m;
    m.material_id     = "mat_SteelA36_001";
    m.name            = "Steel A36";
    m.material_class  = MaterialClass::ALLOY;
    m.composition.fractions = {{"Fe", 0.98}, {"C", 0.0026}, {"Mn", 0.01},
                               {"Si", 0.004}, {"P", 0.0004}, {"S", 0.0005}};
    m.composition.is_mole_fraction = false;  // mass fraction
    m.density_kg_m3   = 7800.0;
    m.phase           = PhaseState::SOLID;
    m.properties.elastic_modulus      = 200.0;
    m.properties.yield_strength       = 250.0;
    m.properties.poisson_ratio        = 0.26;
    m.properties.thermal_conductivity = 50.0;
    m.properties.melting_point        = 1698.0;
    m.properties.tensile_strength     = 400.0;
    m.properties.elongation_pct       = 20.0;
    m.compute_hash();
    return m;
}

inline MaterialDatabaseRecord uranium_dioxide_material() {
    MaterialDatabaseRecord m;
    m.material_id     = "mat_UO2_001";
    m.name            = "Uranium Dioxide";
    m.material_class  = MaterialClass::COMPOUND_MATERIAL;
    m.composition.fractions = {{"U", 1.0}, {"O", 2.0}};
    m.composition.is_mole_fraction = true;
    m.density_kg_m3   = 10970.0;
    m.phase           = PhaseState::SOLID;
    m.properties.melting_point = 3138.0;
    m.properties.thermal_conductivity = 7.0;
    m.hazard.radioactive = true;
    m.compute_hash();
    return m;
}

inline MaterialDatabaseRecord thorium_oxalate_material() {
    MaterialDatabaseRecord m;
    m.material_id     = "mat_ThOx_001";
    m.name            = "Thorium Oxalate";
    m.material_class  = MaterialClass::COMPOUND_MATERIAL;
    m.composition.fractions = {{"Th", 1.0}, {"C", 2.0}, {"O", 6.0}, {"H", 2.0}};
    m.composition.is_mole_fraction = true;
    m.density_kg_m3   = 4637.0;
    m.phase           = PhaseState::SOLID;
    m.hazard.radioactive = true;
    m.compute_hash();
    return m;
}

inline MaterialDatabaseRecord plutonium_residue_material() {
    MaterialDatabaseRecord m;
    m.material_id     = "mat_PuRes_001";
    m.name            = "Plutonium Oxide-Bearing Residue";
    m.material_class  = MaterialClass::MIXED_RESIDUE;
    m.composition.fractions = {{"Pu", 0.40}, {"O", 0.30}, {"C", 0.10},
                               {"H", 0.05}, {"trace", 0.15}};
    m.composition.is_mole_fraction = false;
    m.density_kg_m3   = 6500.0;
    m.phase           = PhaseState::MIXED;
    m.hazard.radioactive = true;
    m.hazard.toxic       = true;
    m.hazard.notes.push_back("criticality_controlled");
    m.compute_hash();
    return m;
}

/// All five material demo entries
inline std::vector<MaterialDatabaseRecord> material_demo_set() {
    return {copper_material(), steel_a36_material(),
            uranium_dioxide_material(), thorium_oxalate_material(),
            plutonium_residue_material()};
}

// ============================================================================
// Compounds — Report 4.3
// ============================================================================

inline CompoundRecord water_compound() {
    CompoundRecord c;
    c.compound_id        = "cmp_water_001";
    c.name               = "Water";
    c.compound_class     = "molecular";
    c.stoichiometry      = {{{"H", 2}, {"O", 1}}, "H2O"};
    c.geometry           = {"bent", StructureFamily::MOLECULAR, "AX2E2", 104.5};
    c.bond_class         = BondClass::COVALENT_POLAR;
    c.phase_description  = "molecular liquid";
    c.molecular_weight   = 18.015;
    c.resolution.modes   = {ProjectionMode::ATOMISTIC};
    c.compute_hash();
    return c;
}

inline CompoundRecord hexene_compound() {
    CompoundRecord c;
    c.compound_id        = "cmp_hexene_001";
    c.name               = "Hexene";
    c.compound_class     = "organic_precursor";
    c.stoichiometry      = {{{"C", 6}, {"H", 12}}, "C6H12"};
    c.geometry           = {"chain / isomer family", StructureFamily::CHAIN, "", 0.0};
    c.bond_class         = BondClass::COVALENT_NONPOLAR;
    c.phase_description  = "molecular liquid";
    c.molecular_weight   = 84.16;
    c.resolution.modes   = {ProjectionMode::ATOMISTIC, ProjectionMode::BEAD};
    c.resolution.default_seed_class = "chain_seed_v1";
    c.compute_hash();
    return c;
}

inline CompoundRecord cyclopentadienyl_compound() {
    CompoundRecord c;
    c.compound_id        = "cmp_cp_001";
    c.name               = "Cyclopentadienyl-like";
    c.compound_class     = "organometallic_ligand";
    c.stoichiometry      = {{{"C", 5}, {"H", 5}}, "C5H5"};
    c.geometry           = {"planar ring", StructureFamily::RING, "", 108.0};
    c.bond_class         = BondClass::COORDINATION;
    c.phase_description  = "molecular / coordination";
    c.molecular_weight   = 65.09;
    c.resolution.modes   = {ProjectionMode::ATOMISTIC_BEAD};
    c.resolution.default_seed_class = "ring_seed_v1";
    c.compute_hash();
    return c;
}

inline CompoundRecord thorium_oxalate_compound() {
    CompoundRecord c;
    c.compound_id        = "cmp_thox_001";
    c.name               = "Thorium Oxalate";
    c.compound_class     = "precipitation_compound";
    c.stoichiometry      = {{{"Th", 1}, {"C", 2}, {"O", 6}, {"H", 2}}, "Th(C2O4)·H2O"};
    c.geometry           = {"coordination / precipitation", StructureFamily::COORDINATION, "", 0.0};
    c.bond_class         = BondClass::IONIC_COVALENT_HYB;
    c.phase_description  = "precursor solid";
    c.molecular_weight   = 408.07;
    c.resolution.modes   = {ProjectionMode::ATOMISTIC};
    c.compute_hash();
    return c;
}

inline CompoundRecord uranium_dioxide_compound() {
    CompoundRecord c;
    c.compound_id        = "cmp_uo2_001";
    c.name               = "Uranium Dioxide";
    c.compound_class     = "nuclear_fuel";
    c.stoichiometry      = {{{"U", 1}, {"O", 2}}, "UO2"};
    c.geometry           = {"crystal lattice", StructureFamily::CRYSTAL, "", 0.0};
    c.bond_class         = BondClass::LATTICE;
    c.phase_description  = "ceramic solid";
    c.molecular_weight   = 270.03;
    c.resolution.modes   = {ProjectionMode::ATOMISTIC};
    c.compute_hash();
    return c;
}

/// All five compound demo entries
inline std::vector<CompoundRecord> compound_demo_set() {
    return {water_compound(), hexene_compound(), cyclopentadienyl_compound(),
            thorium_oxalate_compound(), uranium_dioxide_compound()};
}

// ============================================================================
// Precursors — Report 5.3
// ============================================================================

inline PrecursorRecord hexene_bead_precursor() {
    PrecursorRecord p;
    p.precursor_id      = "prec_hexene_001";
    p.name              = "Hexene Bead Precursor";
    p.precursor_class   = PrecursorClass::ORGANIC_CHAIN;
    p.base_compound_id  = "cmp_hexene_001";
    p.stage             = 1;
    p.chemical_state    = {"liquid", 298.15, 101325.0, 7.0, "", {}};
    p.environment.atmosphere_inert = false;
    p.transformation.reactive = true;
    p.transformation.reaction_type = "polymerization";
    p.target            = TargetFormation::HYDROCARBON_FAMILY;
    p.compute_hash();
    return p;
}

inline PrecursorRecord cp_ligand_precursor() {
    PrecursorRecord p;
    p.precursor_id      = "prec_cp_001";
    p.name              = "Cyclopentadienyl Ligand Precursor";
    p.precursor_class   = PrecursorClass::ORGANOMETALLIC;
    p.base_compound_id  = "cmp_cp_001";
    p.stage             = 2;
    p.chemical_state    = {"molecular", 298.15, 101325.0, 7.0, "", {-1}};
    p.environment.atmosphere_inert = true;
    p.environment.moisture_sensitive = true;
    p.environment.containment_class = "glovebox";
    p.transformation.reactive = true;
    p.transformation.reaction_type = "coordination";
    p.transformation.mechanism = "ligand_exchange";
    p.target            = TargetFormation::COORDINATION_SYSTEM;
    p.compute_hash();
    return p;
}

inline PrecursorRecord thorium_oxalate_slurry_precursor() {
    PrecursorRecord p;
    p.precursor_id      = "prec_thox_001";
    p.name              = "Thorium Oxalate Slurry Precursor";
    p.precursor_class   = PrecursorClass::PRECIPITATION;
    p.base_compound_id  = "cmp_thox_001";
    p.stage             = 1;
    p.chemical_state    = {"dissolved", 333.15, 101325.0, 1.5, "water", {4}};
    p.environment.atmosphere_inert = false;
    p.environment.containment_class = "hot_cell";
    p.transformation.reactive = true;
    p.transformation.exothermic = false;
    p.transformation.reaction_type = "precipitation";
    p.transformation.mechanism = "nucleation";
    p.transformation.products = {"ThO2", "CO2", "H2O"};
    p.target            = TargetFormation::OXIDE_EVOLUTION;
    p.hash.tag_32       = compute_tag(p.precursor_id);
    p.compute_hash();
    return p;
}

inline PrecursorRecord nitrate_metal_precursor() {
    PrecursorRecord p;
    p.precursor_id      = "prec_nitrate_001";
    p.name              = "Dissolved Nitrate-Metal Complex";
    p.precursor_class   = PrecursorClass::DISSOLVED_COMPLEX;
    p.stage             = 1;
    p.chemical_state    = {"dissolved", 313.15, 101325.0, 2.0, "nitric_acid", {4, 6}};
    p.environment.containment_class = "hot_cell";
    p.transformation.reactive = true;
    p.transformation.reaction_type = "reprocessing";
    p.transformation.products = {"sludge", "salt", "oxide"};
    p.target            = TargetFormation::SLUDGE_SALT_OXIDE;
    p.compute_hash();
    return p;
}

inline PrecursorRecord pipe_scale_precursor() {
    PrecursorRecord p;
    p.precursor_id      = "prec_pipescale_001";
    p.name              = "Pipe-Scale Dissolved Solids Packet";
    p.precursor_class   = PrecursorClass::PROCESS_FIELD;
    p.stage             = 1;
    p.chemical_state    = {"dissolved", 353.15, 200000.0, 6.0, "process_water", {}};
    p.environment.containment_class = "open";
    p.transformation.reactive = true;
    p.transformation.reaction_type = "deposition";
    p.transformation.mechanism = "crystallization";
    p.target            = TargetFormation::DEPOSITION_MODEL;
    p.compute_hash();
    return p;
}

/// All five precursor demo entries
inline std::vector<PrecursorRecord> precursor_demo_set() {
    return {hexene_bead_precursor(), cp_ligand_precursor(),
            thorium_oxalate_slurry_precursor(), nitrate_metal_precursor(),
            pipe_scale_precursor()};
}

// ============================================================================
// Report 7: Integrated Hybrid Demonstration Packet
//   Pu + Cyclopentadienyl + Hexene mixed-resolution system
// ============================================================================

struct HybridDemoPacket {
    std::string demo_name = "Pu_Cp_Hexene_Hybrid_001";
    std::string mode      = "mixed_resolution";

    // Component records
    InputRecord           pu_atomistic;      ///< X_Pu^(a)
    InputRecord           cp_bead;           ///< X_Cp^(b)
    InputRecord           hexene_bead_rec;   ///< X_hexene^(b)

    // Supporting registry entries
    InputRecord           pu_element;
    CompoundRecord        cp_compound;
    CompoundRecord        hexene_comp;
    PrecursorRecord       cp_precursor;
    PrecursorRecord       hexene_precursor;
    MaterialDatabaseRecord pu_core_material;

    // Aggregate dimensions (from Report 7.3 manifest)
    DimensionBlock        aggregate_dim;

    // Seed and hash for the packet itself
    SeedBlock             packet_seed;
    HashBlock             packet_hash;

    std::string summary() const {
        std::ostringstream os;
        os << "═══ Hybrid Demo Packet: " << demo_name << " ═══\n"
           << "Mode: " << mode << "\n"
           << "Aggregate " << aggregate_dim.summary() << "\n"
           << "Components:\n"
           << "  " << pu_atomistic.summary() << "\n"
           << "  " << cp_bead.summary() << "\n"
           << "  " << hexene_bead_rec.summary() << "\n"
           << "Seed: " << packet_seed.summary() << "\n"
           << "Hash: " << packet_hash.summary() << "\n";
        return os.str();
    }
};

inline HybridDemoPacket build_hybrid_demo() {
    HybridDemoPacket pkt;

    // Atomistic Pu
    pkt.pu_element      = plutonium_core();
    pkt.pu_atomistic    = plutonium_atomistic();

    // Bead Cp
    pkt.cp_compound     = cyclopentadienyl_compound();
    pkt.cp_precursor    = cp_ligand_precursor();
    pkt.cp_bead         = cyclopentadienyl_bead();

    // Bead hexene
    pkt.hexene_comp     = hexene_compound();
    pkt.hexene_precursor = hexene_bead_precursor();
    pkt.hexene_bead_rec  = hexene_bead();

    // Pu material overlay
    pkt.pu_core_material.material_id    = "mat_Pu_core";
    pkt.pu_core_material.name           = "Plutonium Core";
    pkt.pu_core_material.material_class = MaterialClass::ELEMENTAL_MATERIAL;
    pkt.pu_core_material.composition.fractions = {{"Pu", 1.0}};
    pkt.pu_core_material.density_kg_m3  = 19816.0;
    pkt.pu_core_material.phase          = PhaseState::SOLID;
    pkt.pu_core_material.hazard.radioactive = true;
    pkt.pu_core_material.hazard.toxic       = true;
    pkt.pu_core_material.compute_hash();

    // Aggregate dimensions from Report 7.3 manifest
    pkt.aggregate_dim = {41, 3, 19, 5, 12};

    // Packet seed
    pkt.packet_seed = {41092, 41092202640001ULL,
                       "ring_chain_offset_v1", "hybrid_decay_response_v1"};

    // Packet hash
    std::string blob = pkt.demo_name + ":"
                     + pkt.pu_atomistic.identity.canonical_id + ":"
                     + pkt.cp_bead.identity.canonical_id + ":"
                     + pkt.hexene_bead_rec.identity.canonical_id;
    pkt.packet_hash.tag_32     = compute_tag(pkt.demo_name);
    pkt.packet_hash.content    = compute_content_hash(blob.data(), blob.size());
    pkt.packet_hash.provenance = compute_provenance_hash(blob.data(), blob.size());

    return pkt;
}

// ============================================================================
// Print all demo records (terminal report)
// ============================================================================

inline std::string full_demo_report() {
    std::ostringstream os;

    os << "╔══════════════════════════════════════════════════════════╗\n"
       << "║   Database Architecture Demonstration — Day 40C Suite   ║\n"
       << "╚══════════════════════════════════════════════════════════╝\n\n";

    os << "── Element Cores ──────────────────────────────────────────\n";
    for (auto& e : element_demo_set())
        os << "  " << e.summary() << "\n";

    os << "\n── Materials ──────────────────────────────────────────────\n";
    for (auto& m : material_demo_set())
        os << "  " << m.summary() << "\n";

    os << "\n── Compounds ──────────────────────────────────────────────\n";
    for (auto& c : compound_demo_set())
        os << "  " << c.summary() << "\n";

    os << "\n── Precursors ─────────────────────────────────────────────\n";
    for (auto& p : precursor_demo_set())
        os << "  " << p.summary() << "\n";

    os << "\n── Hybrid Demonstration Packet ────────────────────────────\n";
    auto hybrid = build_hybrid_demo();
    os << hybrid.summary();

    return os.str();
}

} // namespace demo
} // namespace database
} // namespace vsepr
