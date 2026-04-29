// src/v4/uff/ufx_material_record.hpp
// UFX_AUTO_2 -- Layered Material Record
// Formation Engine v4.1.0 / VSEPR-SIM v5 beta8 -> beta9 target
//
// UFXMaterialRecord is the central output object of UFX_AUTO_2.
// It is NOT a single flat table. It is a tiered record with 19 specialised
// blocks, each with independent provenance and confidence.
//
// Tier priority for implementation (from UFX_continual_2.tex):
//   0  Identity + Provenance       -- mandatory; nothing is valid without these
//   1  Force-field (UFF extended)  -- how atoms move
//   2  Molecular descriptors       -- cheap sanity filters
//   3  Thermo + EOS                -- state-dependent properties
//   4  Crystal / solid-state       -- structural material data
//   5  Transport + Mechanics       -- flow and failure properties
//   6  Reaction + Corrosion        -- surface chemistry pathways
//   7  Radiation / Nuclear         -- separate tables; never in UFF table
//   8  Spectra / Fingerprints      -- structural validation
//   9  Meta-scores                 -- generation steering
//
// Hard rules (from UFX_continual_2.tex §20):
//   - No provenance = no promotion.
//   - Nuclear/radiation fields never go in the UFF parameter table.
//   - State (T, P, phase) is required on every property value.
//   - Label the approximation on every electronic/quantum field.
//   - SQLite is truth. JSONL is history. CSV is convenience.

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include "core/math_vec3.hpp"

namespace vsepr::ufx {

// ============================================================================
// Utility types
// ============================================================================

// Day #56: Vec3 unified. No local struct — alias to authoritative vsepr::Vec3.
using Vec3 = vsepr::Vec3;

// A single (T_K, value) data point for a temperature-dependent curve.
struct ThermoPoint {
	double T_K    = 0.0;
	double value  = 0.0;
	double P_Pa   = 101325.0;  // reference pressure
};

// A sequence of ThermoPoints representing a property curve over T (and P).
using PropertyCurve = std::vector<ThermoPoint>;

// A (T_K, P_Pa, value) triplet for a T-P property surface.
struct StatePoint {
	double T_K   = 0.0;
	double P_Pa  = 0.0;
	double value = 0.0;
};

using PropertySurface = std::vector<StatePoint>;

// ============================================================================
// SourceClass — classification of every record and every property value
// ============================================================================

enum class SourceClass : std::uint8_t {
	Reference,    // trusted pinned external source (Rappe 1992, NIST, etc.)
	Generated,    // created by UFX_AUTO_2 rules or randomised sweep
	Validated,    // passed local + external checks; promoted
	Rejected,     // failed sanity or validation; retained for audit
	Derived,      // computed from other stored quantities
	Imported,     // loaded from external database (Materials Project, OQMD)
	Experimental, // direct measured property
	Simulated     // produced by VSEPR / UFX simulation run
};

constexpr const char* to_string(SourceClass sc) noexcept {
	switch (sc) {
		case SourceClass::Reference:    return "reference";
		case SourceClass::Generated:    return "generated";
		case SourceClass::Validated:    return "validated";
		case SourceClass::Rejected:     return "rejected";
		case SourceClass::Derived:      return "derived";
		case SourceClass::Imported:     return "imported";
		case SourceClass::Experimental: return "experimental";
		case SourceClass::Simulated:    return "simulated";
	}
	return "unknown";
}

// ============================================================================
// Tier 0 — IDENTITY BLOCK
// ============================================================================

struct IdentityBlock {
	std::string material_key;          // primary lookup key, e.g. "Fe_ox2_oct_solid"
	std::string formula;               // e.g. "Fe2O3"
	std::string phase;                 // "gas" | "liquid" | "solid" | "powder" | "molten_salt"

	std::vector<std::string> elements;
	std::vector<int>         atomic_numbers;

	std::optional<int>    isotope_mass_number;
	std::optional<double> formal_charge;
	std::optional<double> oxidation_state;

	int         coordination_number = 0;
	std::string geometry_tag;          // "tetrahedral", "octahedral", "linear", etc.
	std::string hybridization;         // "sp3", "sp2", "sp", "d2sp3", etc.

	std::string local_environment_hash; // SHA-256 of {element,coord,geometry,ox_state}

	bool is_populated() const noexcept { return !material_key.empty() && !formula.empty(); }
};

// ============================================================================
// Tier 0 — PROVENANCE BLOCK
// ============================================================================

// Validation / promotion status of a record or individual property.
enum class ValidationStatus : std::uint8_t {
	Unknown,
	Published,    // from peer-reviewed or authoritative source
	DFT,          // from first-principles calculation
	Generated,    // created by UFX_AUTO_2 rules
	Inferred,     // derived from periodic trends
	Cursed        // do not trust; retained for audit
};

constexpr const char* to_string(ValidationStatus vs) noexcept {
	switch (vs) {
		case ValidationStatus::Unknown:   return "unknown";
		case ValidationStatus::Published: return "published";
		case ValidationStatus::DFT:       return "dft";
		case ValidationStatus::Generated: return "generated";
		case ValidationStatus::Inferred:  return "inferred";
		case ValidationStatus::Cursed:    return "cursed";
	}
	return "unknown";
}

struct ProvenanceBlock {
	std::string source_id;              // citation key, URL, or API endpoint ID
	std::string source_class_tag;       // mirrors SourceClass as string
	std::string source_version;
	std::string retrieval_date;         // ISO-8601
	std::string raw_response_hash;      // SHA-256 of raw API / file response
	std::string method_tag;             // "UFF_RAPPE_1992", "DFT_PBE", "HEURISTIC", etc.

	double confidence           = 0.0;  // 0.0 – 1.0
	double uncertainty          = 0.0;  // physical units (property-specific)

	std::string unit_conversion_trace;  // e.g. "kJ/mol -> kcal/mol * 0.239006"
	ValidationStatus validation_status = ValidationStatus::Unknown;
	std::string      validation_method;
	int              number_of_confirmations = 0;
	int              conflict_count          = 0;
	std::string      last_checked_date;      // ISO-8601
	std::string      promotion_status;       // "generated" | "validated_high" | "rejected" | etc.

	bool has_provenance() const noexcept { return !source_id.empty(); }
};

// ============================================================================
// Tier 1 — FORCE-FIELD BLOCK (UFF extended)
// ============================================================================

// Source class for UFF parameters specifically.
enum class FFSourceClass : std::uint8_t {
	Reference,   // Rappe et al. 1992 UFF baseline
	Generated,   // UFX_AUTO_2 fallback rules
	Validated    // passed local, molecular, and web cross-checks
};

struct ForceFieldBlock {
	std::string atom_type;             // UFF label: "C_3", "Os6+3", etc.

	// Rappe et al. 1992 core parameters
	double r1     = 0.0;   // bond radius (Å)
	double theta0 = 0.0;   // equilibrium valence angle (degrees)
	double x1     = 0.0;   // vdW distance parameter (Å)
	double D1     = 0.0;   // vdW well depth (kcal/mol)
	double zeta   = 0.0;   // vdW scaling / GMP exponential decay
	double Z1     = 0.0;   // effective charge parameter

	// Extended correction terms
	double bond_order_correction    = 0.0;
	double coordination_correction  = 0.0;
	double torsion_barrier          = 0.0;   // Vi (kcal/mol)
	double inversion_barrier        = 0.0;   // Uj (kcal/mol)

	std::string mixing_rule;                 // "geometric", "arithmetic", "6th_power"
	std::string fallback_rule;               // "interpolate" | "period_stub" | "none"
	std::string parameter_source_class;      // FFSourceClass as string
	double      parameter_confidence = 0.0;  // 0.0 – 1.0

	std::string parameter_revision;          // "rappe1992_r1", "autocreate_rev2", etc.
	std::string local_environment_hash;      // ties to IdentityBlock hash
	std::string validated_against;           // reference tag used in validation
};

// ============================================================================
// Tier 2 — MOLECULAR DESCRIPTOR BLOCK
// ============================================================================

struct MolecularDescriptorBlock {
	std::string molecular_formula;
	double      molecular_weight        = 0.0;  // g/mol
	double      exact_mass              = 0.0;  // monoisotopic
	std::string canonical_smiles;
	std::string inchi;
	std::string inchikey;

	std::optional<double> formal_charge;
	int heavy_atom_count            = 0;
	int rotatable_bond_count        = 0;
	int hbond_donor_count           = 0;
	int hbond_acceptor_count        = 0;

	std::optional<double> tpsa;         // topological polar surface area (Å²)
	std::optional<double> xlogp;        // octanol-water partition coefficient
	std::optional<double> complexity;
	std::string           fingerprint_hash; // structural fingerprint SHA-256
};

// ============================================================================
// Tier 3 — THERMO BLOCK
// ============================================================================

struct ThermoBlock {
	PropertyCurve cp_T;    // Cp(T) [J/(mol·K)]
	PropertyCurve h_T;     // H(T) [kJ/mol]
	PropertyCurve s_T;     // S(T) [J/(mol·K)]

	std::optional<double> delta_Hf_298_kJ_mol;   // standard enthalpy of formation
	std::optional<double> delta_Gf_298_kJ_mol;   // standard Gibbs free energy
	std::optional<double> S_298_J_mol_K;          // standard entropy

	std::optional<double> melting_point_K;
	std::optional<double> boiling_point_K;
	std::optional<double> critical_temperature_K;
	std::optional<double> critical_pressure_Pa;
	std::optional<double> critical_volume_m3_mol;
	std::optional<double> acentric_factor;

	std::optional<double> latent_heat_fusion_kJ_mol;
	std::optional<double> latent_heat_vaporization_kJ_mol;
	std::optional<double> vapor_pressure_Pa_298K;
	std::optional<double> henry_law_constant;
};

// ============================================================================
// Tier 3 — EQUATION-OF-STATE BLOCK
// ============================================================================

struct EOSBlock {
	std::string eos_model;  // "ideal" | "vdW" | "RK" | "PR" | "tabular" | "mixed_PvT"

	// vdW / RK / PR EOS parameters
	std::optional<double> a_coeff;
	std::optional<double> b_coeff;

	PropertySurface density_TP;              // rho(T,P) [kg/m³]
	PropertySurface Z_TP;                    // compressibility Z(T,P)
	PropertySurface viscosity_TP;            // dynamic viscosity [Pa·s]
	PropertySurface thermal_conductivity_TP; // k(T,P) [W/(m·K)]

	std::optional<double> surface_tension_298K;  // N/m
	std::optional<double> bulk_modulus_Pa;
	std::optional<double> speed_of_sound_m_s;
};

// ============================================================================
// Tier 4 — CRYSTAL / SOLID-STATE BLOCK
// ============================================================================

struct CrystalBlock {
	std::string space_group;             // e.g. "Fm-3m", "P63/mmc"
	int         space_group_number = 0;  // ITA number 1-230

	Vec3 lattice_abc_angstrom;           // a, b, c in Å
	Vec3 angles_alpha_beta_gamma_deg;    // alpha, beta, gamma in degrees

	double unit_cell_volume_angstrom3 = 0.0;
	double density_g_cm3              = 0.0;
	double packing_fraction           = 0.0;

	std::optional<double> formation_energy_eV_atom;
	std::optional<double> energy_above_hull_eV_atom;
	std::vector<std::string> decomposition_products;

	std::optional<double> band_gap_eV;
	std::optional<double> debye_temperature_K;
	std::optional<double> thermal_expansion_1_over_K;

	std::vector<Vec3>    fractional_positions;
	std::vector<std::string> site_elements;

	std::string structure_source;        // "materials_project", "oqmd", "generated", etc.
	std::string structure_id;            // external ID (mp-xxxx, etc.)
};

// ============================================================================
// Tier 5 — MECHANICAL BLOCK
// ============================================================================

struct MechanicalBlock {
	std::optional<double> E_eff_GPa;           // Young's modulus
	std::optional<double> G_eff_GPa;           // shear modulus
	std::optional<double> K_eff_GPa;           // bulk modulus
	std::optional<double> poisson_ratio;

	std::optional<double> yield_strength_MPa;
	std::optional<double> failure_strength_MPa;
	std::optional<double> fracture_toughness_MPa_m05;  // K_IC
	std::optional<double> hardness_GPa;
	std::optional<double> ductility;

	std::optional<double> strain_hardening_exponent;
	std::optional<double> fatigue_strength_MPa;
	std::optional<double> creep_A_constant;
	std::optional<double> creep_n_exponent;

	std::string test_method;
	double      strain_rate  = 0.0;
	double      temperature_K = 298.15;
};

// ============================================================================
// Tier 5 — TRANSPORT BLOCK
// ============================================================================

struct TransportBlock {
	std::optional<double> k_eff_W_mK;              // effective thermal conductivity
	std::optional<double> diffusivity_m2_s;
	std::optional<double> self_diffusivity_m2_s;
	std::optional<double> electrical_conductivity_S_m;
	std::optional<double> ionic_conductivity_S_m;
	std::optional<double> permeability_m2;
	std::optional<double> porosity;
	std::optional<double> tortuosity;
	std::optional<double> darcy_coefficient;
	std::optional<double> forchheimer_coefficient;
	std::optional<double> mass_transfer_coefficient;
	std::optional<double> heat_transfer_coefficient;
	std::optional<double> mobility_m2_Vs;
};

// ============================================================================
// Tier 6 — REACTION BLOCK
// ============================================================================

struct ReactionBlock {
	std::string reaction_id;
	std::string reaction_smiles;

	std::optional<double> delta_H_kJ_mol;         // reaction enthalpy
	std::optional<double> delta_G_kJ_mol;          // reaction free energy
	std::optional<double> activation_energy_kJ_mol;
	std::optional<double> arrhenius_A;             // pre-exponential factor
	std::optional<double> reaction_order;
	std::optional<double> equilibrium_constant_298K;

	PropertyCurve rate_constant_T;  // k(T)

	std::optional<double> adsorption_energy_eV;
	std::optional<double> desorption_energy_eV;
	std::string           surface_binding_site;
	std::string           catalytic_scaling_descriptor;
	std::string           reaction_pathway_id;
};

// ============================================================================
// Tier 6 — CORROSION BLOCK
// ============================================================================

struct CorrosionBlock {
	std::string environment;               // e.g. "molten_fluoride_salt", "brine", "H2SO4_10pct"

	std::optional<double> corrosion_potential_V;
	std::optional<double> pitting_risk;          // 0.0 – 1.0
	std::optional<double> passivation_score;     // 0.0 – 1.0
	std::optional<double> oxide_growth_rate;     // nm/h
	std::optional<double> salt_compatibility_score;
	std::optional<double> radiolysis_coupling_score;
	std::optional<double> hydrogen_embrittlement_risk;
	std::optional<double> chloride_sensitivity;
	std::optional<double> fluoride_sensitivity;
	std::string           pourbaix_region;
	std::string           galvanic_series_tag;
};

// ============================================================================
// Tier 7 — RADIATION / NUCLEAR BLOCK
// ============================================================================
// Hard rule: these fields NEVER go in the UFF parameter table.
// Store in separate DB tables: nuclear_properties, radiation_response,
// decay_channels, activation_products.

struct RadiationBlock {
	std::string isotope;                 // e.g. "Pu-239", "U-235"
	double      half_life_seconds = 0.0;
	std::vector<std::string> decay_modes;  // "alpha", "beta-", "gamma", "SF"

	std::optional<double> decay_heat_W_per_kg;
	std::optional<double> neutron_absorption_cross_section_barn;
	std::optional<double> scattering_cross_section_barn;
	std::optional<double> fission_cross_section_barn;
	std::optional<double> dpa_rate_proxy;         // displacement per atom proxy
	std::optional<double> helium_production_rate; // appm/dpa
	std::optional<double> gas_swelling_risk;      // 0.0 – 1.0
	std::optional<double> decay_energy_MeV;
	std::optional<double> shielding_attenuation_coefficient;

	std::vector<std::string> activation_products;
};

// ============================================================================
// Tier 8 — SPECTRAL / FINGERPRINT BLOCK
// ============================================================================

struct SpectralPoint {
	double wavenumber_or_mz = 0.0;   // cm⁻¹ for IR/Raman, m/z for MS, ppm for NMR
	double intensity         = 0.0;
	std::string label;
};

struct SpectralBlock {
	std::vector<SpectralPoint> ir_peaks;
	std::vector<SpectralPoint> raman_peaks;
	std::vector<SpectralPoint> uvvis_peaks;     // nm, absorbance
	std::vector<SpectralPoint> mass_spec_peaks;
	std::vector<SpectralPoint> nmr_shifts;      // ppm

	std::vector<SpectralPoint> xrd_peaks;       // 2-theta, intensity
	std::vector<double>        vibrational_frequencies_cm1;
	std::vector<std::string>   normal_mode_labels;

	std::string ir_source_id;
	std::string ms_source_id;
};

// ============================================================================
// Tier 9 — META-SCORE BLOCK (generation steering)
// ============================================================================

struct MetaScoreBlock {
	double weirdness            = 0.0;  // how unusual vs. periodic/structural neighbours
	double coverage             = 0.0;  // how well-represented in parameter space
	double confidence           = 0.0;  // aggregate validation confidence
	double novelty              = 0.0;  // distance from existing validated entries
	double local_consistency    = 0.0;  // agreement with immediate neighbours
	double source_conflict      = 0.0;  // inter-source disagreement
	double macro_usefulness     = 0.0;  // helps predict E_eff, k_eff, dP, damage, phase stability
	double simulation_stability = 0.0;  // does this entry destabilise VSEPR runs?
	double extrapolation_risk   = 0.0;  // model outside validated space?

	// Scoring model (from UFX_continual_2.tex §19):
	// S = 0.25*identity + 0.20*local + 0.20*reference + 0.15*web + 0.10*repeat + 0.10*macro
	double composite_score = 0.0;

	// Promotion thresholds: >=0.95 VALIDATED_HIGH, >=0.85 VALIDATED_WARN,
	// >=0.70 GENERATED_LOW_CONFIDENCE, <0.70 REJECTED.
	std::string promotion_class;  // "validated_high" | "validated_warn" | "generated_low" | "rejected"
};

// ============================================================================
// Tier 4 (additional) — GRANULAR / BEAD / POWDER BLOCK
// ============================================================================

struct GranularBlock {
	std::optional<double> particle_diameter_m;
	std::optional<double> sphericity;
	std::optional<double> aspect_ratio;
	std::optional<double> roughness;
	std::optional<double> restitution;
	std::optional<double> friction_static;
	std::optional<double> friction_rolling;
	std::optional<double> cohesion_Pa;
	std::optional<double> packing_fraction;
	std::optional<double> angle_of_repose_deg;
	std::optional<double> erosion_coefficient;
	std::optional<double> abrasion_coefficient;

	// Pipeline erosion fields
	std::optional<double> impact_velocity_m_s;
	std::optional<double> impact_angle_deg;
	std::optional<double> erosion_yield;           // volume removed per unit energy
	std::optional<double> localized_wear_rate;
};

// ============================================================================
// Tier 3 (additional) — ELECTRONIC / QUANTUM-ADJACENT BLOCK
// ============================================================================
// NOTE: These are CONSTRAINT and DESCRIPTOR fields, NOT full QM results.
// Label the approximation. Every field here is an estimate unless
// explicitly tagged as DFT or Experimental in its provenance.

struct ElectronicBlock {
	std::optional<double> ionization_energy_eV;
	std::optional<double> electron_affinity_eV;
	std::optional<double> work_function_eV;
	std::optional<double> band_gap_eV;
	std::optional<double> homo_lumo_gap_eV;
	std::optional<double> dipole_moment_Debye;
	std::optional<double> polarizability_angstrom3;
	std::optional<double> dielectric_constant;
	std::optional<double> fermi_level_eV;
	std::optional<double> redox_potential_V;
	std::optional<double> electronegativity_pauling;

	std::vector<double>   partial_charges;          // one per atom
	std::string           charge_model;             // "EEM", "QEq", "Gasteiger", etc.
	std::string           approximation_label;      // REQUIRED: label the approximation
};

// ============================================================================
// UFXMaterialRecord — the complete layered record
// ============================================================================

struct UFXMaterialRecord {
	// --- Tier 0 (mandatory) ---
	IdentityBlock   identity;
	ProvenanceBlock provenance;

	// --- Tier 1 ---
	ForceFieldBlock force_field;

	// --- Tier 2 ---
	MolecularDescriptorBlock molecular;

	// --- Tier 3 ---
	ThermoBlock   thermo;
	EOSBlock      eos;
	ElectronicBlock electronic;

	// --- Tier 4 ---
	CrystalBlock  crystal;
	GranularBlock granular;

	// --- Tier 5 ---
	MechanicalBlock  mechanical;
	TransportBlock   transport;

	// --- Tier 6 ---
	ReactionBlock  reactions;
	CorrosionBlock corrosion;

	// --- Tier 7 ---
	RadiationBlock radiation;

	// --- Tier 8 ---
	SpectralBlock spectra;

	// --- Tier 9 ---
	MetaScoreBlock meta;

	// Record-level classification
	SourceClass source_class = SourceClass::Generated;

	// Timestamps (ISO-8601)
	std::string created_at;
	std::string updated_at;

	// Convenience checks
	bool identity_valid() const noexcept { return identity.is_populated(); }
	bool has_provenance() const noexcept { return provenance.has_provenance(); }

	// A record may only be promoted if both identity and provenance are present.
	bool promotable() const noexcept { return identity_valid() && has_provenance(); }
};

} // namespace vsepr::ufx
