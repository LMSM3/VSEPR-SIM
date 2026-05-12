// peptide_formation.hpp — Organic / Peptide Formation Engine
// Day 48A: C++23, data-driven, deterministic, anti-black-box
//
// Architecture:
//   MolecularGraph         — core data container (atoms, residues, bonds)
//   OrganicRuleEngine      — valence validation, hybridization assignment
//   PeptideBondEngine      — peptide bond formation with geometry
//   BackboneGeometryEngine — backbone initialization, amide planarity
//   HydrogenBondEngine     — H-bond detection and scoring
//   EnergyModel            — full energy decomposition
//   ScoringEngine          — quality scoring
//   PeptideFormationPipeline — top-level orchestrator
//
// All engines are stateless. All intermediate results are exposed.
// No hidden state. Deterministic: same inputs -> identical outputs.
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../c_api/vsepr_chem_core.h"
#include "../peptide/vsepr_peptide_types.h"
#include "../organic/valence_tables.hpp"
#include "../organic/functional_groups.hpp"
#include "core/math_vec3.hpp"

namespace vsepr::chem {

// ============================================================================
// Error handling — std::expected pipeline
// ============================================================================

enum class ErrorCode : std::uint32_t {
    none = 0,
    invalid_valence,
    invalid_backbone,
    invalid_bond_request,
    incompatible_residues,
    atom_not_found,
    geometry_failure,
    scoring_failure,
    overbonded_atom,
    missing_atom_in_residue,
};

struct Error {
    ErrorCode code {};
    std::string message {};
};

template <typename T>
using Result = std::expected<T, Error>;

// ============================================================================
// Geometric primitive — Day #56: alias to vsepr::Vec3, no local struct.
// ============================================================================

using Vec3 = vsepr::Vec3;

inline double distance(const Vec3& a, const Vec3& b) noexcept {
    return (a - b).norm();
}

// ============================================================================
// Atom
// ============================================================================

struct Atom {
    std::int32_t atom_id {};
    std::int32_t atomic_number {};
    std::int32_t isotope {};
    std::int32_t formal_charge {};

    std::string atom_name;
    std::string element_symbol;

    Vec3 position {};
    Vec3 velocity {};

    VSEPR_Hybridization hybridization {VSEPR_HYB_UNKNOWN};
    VSEPR_PeptideRole   chem_role      {VSEPR_PEPTIDE_ROLE_UNKNOWN};

    double partial_charge {};
    double covalent_radius_pm {};
    double vdw_radius_pm {};
    double mass_u {};

    std::vector<std::int32_t> bonded_atom_ids;
    std::vector<std::int32_t> bond_orders;

    std::int32_t residue_id {-1};
    std::int32_t molecule_id {-1};

    [[nodiscard]] std::int32_t total_bond_order() const noexcept {
        std::int32_t sum = 0;
        for (auto o : bond_orders) sum += o;
        return sum;
    }
};

// ============================================================================
// Residue
// ============================================================================

struct Residue {
    std::int32_t residue_id {};
    std::string residue_name;

    std::vector<std::int32_t> atom_ids;

    std::int32_t backbone_N {-1};
    std::int32_t backbone_CA {-1};
    std::int32_t backbone_C {-1};
    std::int32_t backbone_O {-1};
    std::int32_t sidechain_root {-1};

    std::int32_t charge_state {};
    VSEPR_PeptideSidechainClass sidechain_class {VSEPR_SIDECHAIN_UNKNOWN};

    double phi_deg {};
    double psi_deg {};
    double omega_deg {180.0};

    [[nodiscard]] bool has_valid_backbone() const noexcept {
        return backbone_N >= 0 && backbone_CA >= 0 &&
               backbone_C >= 0 && backbone_O >= 0;
    }
};

// ============================================================================
// Bond
// ============================================================================

struct Bond {
    std::int32_t a {};
    std::int32_t b {};
    std::int32_t order {1};
};

// ============================================================================
// Energy breakdown — every term exposed, inspectable
// ============================================================================

struct EnergyBreakdown {
    double total_kj_mol {};
    double bond_kj_mol {};
    double angle_kj_mol {};
    double torsion_kj_mol {};
    double improper_kj_mol {};
    double vdw_kj_mol {};
    double coulomb_kj_mol {};
    double hbond_kj_mol {};
    double solvation_kj_mol {};
    double formation_kj_mol {};

    void compute_total() noexcept {
        total_kj_mol = bond_kj_mol + angle_kj_mol + torsion_kj_mol +
                       improper_kj_mol + vdw_kj_mol + coulomb_kj_mol +
                       hbond_kj_mol + solvation_kj_mol + formation_kj_mol;
    }
};

// ============================================================================
// Score card — quality metrics
// ============================================================================

struct ScoreCard {
    double steric_score {};
    double hbond_score {};
    double electrostatic_score {};
    double hydrophobic_score {};
    double planarity_score {};
    double confidence_score {};
};

// ============================================================================
// Formation summary — the output of the full pipeline
// ============================================================================

struct FormationSummary {
    VSEPR_FormationClass  formation_class  {VSEPR_FORM_UNKNOWN};
    VSEPR_FormationState  formation_state  {VSEPR_STATE_PREFORM};
    VSEPR_PeptideState    peptide_state    {VSEPR_PEPTIDE_STATE_UNKNOWN};
    EnergyBreakdown energy {};
    ScoreCard score {};
    bool chemical_validity_pass {};
    bool valence_validity_pass {};
    std::int32_t atom_count {};
    std::int32_t residue_count {};
    std::int32_t bond_count {};
    std::int32_t hbond_count {};
    std::vector<DetectedGroup> functional_groups;
};

// ============================================================================
// MolecularGraph — central data container
// ============================================================================

class MolecularGraph {
public:
    MolecularGraph() = default;

    [[nodiscard]] auto atoms() noexcept -> std::vector<Atom>& { return atoms_; }
    [[nodiscard]] auto atoms() const noexcept -> const std::vector<Atom>& { return atoms_; }

    [[nodiscard]] auto residues() noexcept -> std::vector<Residue>& { return residues_; }
    [[nodiscard]] auto residues() const noexcept -> const std::vector<Residue>& { return residues_; }

    [[nodiscard]] auto bonds() noexcept -> std::vector<Bond>& { return bonds_; }
    [[nodiscard]] auto bonds() const noexcept -> const std::vector<Bond>& { return bonds_; }

    [[nodiscard]] auto find_atom(std::int32_t atom_id) noexcept -> Atom*;
    [[nodiscard]] auto find_atom(std::int32_t atom_id) const noexcept -> const Atom*;

    [[nodiscard]] auto find_residue(std::int32_t residue_id) noexcept -> Residue*;
    [[nodiscard]] auto find_residue(std::int32_t residue_id) const noexcept -> const Residue*;

    auto add_atom(Atom atom) -> std::int32_t;
    auto add_residue(Residue residue) -> std::int32_t;
    auto add_bond(Bond bond) -> void;

    [[nodiscard]] auto atom_count() const noexcept -> std::int32_t {
        return static_cast<std::int32_t>(atoms_.size());
    }
    [[nodiscard]] auto residue_count() const noexcept -> std::int32_t {
        return static_cast<std::int32_t>(residues_.size());
    }
    [[nodiscard]] auto bond_count() const noexcept -> std::int32_t {
        return static_cast<std::int32_t>(bonds_.size());
    }

private:
    std::vector<Atom> atoms_;
    std::vector<Residue> residues_;
    std::vector<Bond> bonds_;
};

// ============================================================================
// OrganicRuleEngine — valence + hybridization
// ============================================================================

class OrganicRuleEngine {
public:
    [[nodiscard]] auto validate_atom(const Atom& atom) const -> Result<void>;
    [[nodiscard]] auto validate_residue(const MolecularGraph& graph,
                                        const Residue& residue) const -> Result<void>;
    [[nodiscard]] auto detect_functional_groups(const MolecularGraph& graph) const
        -> std::vector<DetectedGroup>;
    auto assign_hybridization(MolecularGraph& graph) const -> void;
    [[nodiscard]] auto check_valence(const Atom& atom) const -> Result<void>;
};

// ============================================================================
// PeptideBondEngine
// ============================================================================

class PeptideBondEngine {
public:
    [[nodiscard]] auto can_form_peptide_bond(const MolecularGraph& graph,
                                             const Residue& lhs,
                                             const Residue& rhs) const -> Result<void>;

    [[nodiscard]] auto form_peptide_bond(MolecularGraph& graph,
                                         Residue& lhs,
                                         Residue& rhs) const -> Result<Bond>;
};

// ============================================================================
// BackboneGeometryEngine
// ============================================================================

class BackboneGeometryEngine {
public:
    [[nodiscard]] auto initialize_backbone_geometry(MolecularGraph& graph) const -> Result<void>;
    [[nodiscard]] auto enforce_amide_planarity(MolecularGraph& graph) const -> Result<void>;
    auto assign_default_torsions(MolecularGraph& graph) const -> void;

    [[nodiscard]] auto measure_planarity_deviation(const MolecularGraph& graph,
                                                    const Residue& residue) const -> double;
};

// ============================================================================
// HydrogenBondEngine
// ============================================================================

class HydrogenBondEngine {
public:
    struct HBond {
        std::int32_t donor_atom_id {};
        std::int32_t acceptor_atom_id {};
        double distance_angstrom {};
        double angle_deg {};
        double strength_score {};
    };

    [[nodiscard]] auto detect_hbonds(const MolecularGraph& graph) const -> std::vector<HBond>;

    static constexpr double HBOND_DIST_MAX_A  = 3.5;
    static constexpr double HBOND_DIST_MIN_A  = 2.0;
    static constexpr double HBOND_ANGLE_MIN   = 120.0;
};

// ============================================================================
// EnergyModel
// ============================================================================

class EnergyModel {
public:
    [[nodiscard]] auto evaluate(const MolecularGraph& graph,
                                VSEPR_EnvironmentClass environment) const -> Result<EnergyBreakdown>;
};

// ============================================================================
// ScoringEngine
// ============================================================================

class ScoringEngine {
public:
    [[nodiscard]] auto score(const MolecularGraph& graph,
                             const EnergyBreakdown& energy,
                             VSEPR_EnvironmentClass environment) const -> Result<ScoreCard>;
};

// ============================================================================
// PeptideFormationPipeline — top-level orchestrator
// ============================================================================

class PeptideFormationPipeline {
public:
    explicit PeptideFormationPipeline(VSEPR_EnvironmentClass environment = VSEPR_ENV_POLAR_MEDIUM)
        : environment_(environment) {}

    [[nodiscard]] auto build_from_residues(std::span<const Residue> residues,
                                           std::span<const Atom> atoms) const -> Result<MolecularGraph>;

    [[nodiscard]] auto form_chain(MolecularGraph graph) const -> Result<MolecularGraph>;

    [[nodiscard]] auto solve(MolecularGraph graph) const -> Result<FormationSummary>;

    [[nodiscard]] auto environment() const noexcept -> VSEPR_EnvironmentClass { return environment_; }

private:
    OrganicRuleEngine organic_rules_ {};
    PeptideBondEngine peptide_bonds_ {};
    BackboneGeometryEngine geometry_ {};
    HydrogenBondEngine hbonds_ {};
    EnergyModel energy_ {};
    ScoringEngine scoring_ {};
    VSEPR_EnvironmentClass environment_ {VSEPR_ENV_POLAR_MEDIUM};
};

} // namespace vsepr::chem
