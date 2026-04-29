// peptide_formation.cpp — Organic / Peptide Formation Engine implementation
#include "peptide_formation.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace vsepr::chem {

// ============================================================================
// MolecularGraph
// ============================================================================

auto MolecularGraph::find_atom(std::int32_t atom_id) noexcept -> Atom* {
    for (auto& atom : atoms_) {
        if (atom.atom_id == atom_id) return &atom;
    }
    return nullptr;
}

auto MolecularGraph::find_atom(std::int32_t atom_id) const noexcept -> const Atom* {
    for (const auto& atom : atoms_) {
        if (atom.atom_id == atom_id) return &atom;
    }
    return nullptr;
}

auto MolecularGraph::find_residue(std::int32_t residue_id) noexcept -> Residue* {
    for (auto& residue : residues_) {
        if (residue.residue_id == residue_id) return &residue;
    }
    return nullptr;
}

auto MolecularGraph::find_residue(std::int32_t residue_id) const noexcept -> const Residue* {
    for (const auto& residue : residues_) {
        if (residue.residue_id == residue_id) return &residue;
    }
    return nullptr;
}

auto MolecularGraph::add_atom(Atom atom) -> std::int32_t {
    auto id = atom.atom_id;
    atoms_.push_back(std::move(atom));
    return id;
}

auto MolecularGraph::add_residue(Residue residue) -> std::int32_t {
    auto id = residue.residue_id;
    residues_.push_back(std::move(residue));
    return id;
}

auto MolecularGraph::add_bond(Bond bond) -> void {
    bonds_.push_back(bond);

    // Update bonded_atom_ids on both ends
    if (auto* a = find_atom(bond.a)) {
        a->bonded_atom_ids.push_back(bond.b);
        a->bond_orders.push_back(bond.order);
    }
    if (auto* b = find_atom(bond.b)) {
        b->bonded_atom_ids.push_back(bond.a);
        b->bond_orders.push_back(bond.order);
    }
}

// ============================================================================
// OrganicRuleEngine
// ============================================================================

auto OrganicRuleEngine::validate_atom(const Atom& atom) const -> Result<void> {
    if (atom.atomic_number <= 0) {
        return std::unexpected(Error{
            .code = ErrorCode::invalid_valence,
            .message = "Atomic number must be positive (got " +
                       std::to_string(atom.atomic_number) + " for atom " +
                       std::to_string(atom.atom_id) + ")"
        });
    }
    return {};
}

auto OrganicRuleEngine::check_valence(const Atom& atom) const -> Result<void> {
    auto Z = static_cast<std::uint8_t>(atom.atomic_number);
    auto observed = static_cast<std::uint8_t>(atom.total_bond_order());
    const auto* entry = lookup_valence(Z);

    if (!entry) {
        // Unknown element — allow but don't validate
        return {};
    }

    if (observed > entry->max_valence) {
        return std::unexpected(Error{
            .code = ErrorCode::overbonded_atom,
            .message = "Atom " + std::to_string(atom.atom_id) + " (" +
                       atom.element_symbol + ") has bond order " +
                       std::to_string(observed) + " exceeding max valence " +
                       std::to_string(entry->max_valence)
        });
    }
    return {};
}

auto OrganicRuleEngine::validate_residue(const MolecularGraph& graph,
                                          const Residue& residue) const -> Result<void> {
    if (!residue.has_valid_backbone()) {
        return std::unexpected(Error{
            .code = ErrorCode::invalid_backbone,
            .message = "Residue " + residue.residue_name + " (id=" +
                       std::to_string(residue.residue_id) + ") backbone is incomplete"
        });
    }

    // Verify all backbone atoms exist in graph
    for (auto aid : {residue.backbone_N, residue.backbone_CA,
                     residue.backbone_C, residue.backbone_O}) {
        if (!graph.find_atom(aid)) {
            return std::unexpected(Error{
                .code = ErrorCode::missing_atom_in_residue,
                .message = "Backbone atom " + std::to_string(aid) +
                           " not found in graph for residue " + residue.residue_name
            });
        }
    }

    return {};
}

auto OrganicRuleEngine::detect_functional_groups(const MolecularGraph& graph) const
    -> std::vector<DetectedGroup> {
    std::vector<DetectedGroup> groups;

    // Detect amide groups: C(=O)-N pattern
    for (const auto& bond : graph.bonds()) {
        const auto* a = graph.find_atom(bond.a);
        const auto* b = graph.find_atom(bond.b);
        if (!a || !b) continue;

        // Look for C-N single bond where C also has a double bond to O
        if (a->atomic_number == 6 && b->atomic_number == 7 && bond.order == 1) {
            // Check if atom a has a C=O
            for (std::size_t i = 0; i < a->bonded_atom_ids.size(); ++i) {
                if (a->bond_orders[i] == 2) {
                    const auto* o = graph.find_atom(a->bonded_atom_ids[i]);
                    if (o && o->atomic_number == 8) {
                        groups.push_back({
                            .type = FunctionalGroup::amide,
                            .atom_ids = {a->atom_id, o->atom_id, b->atom_id},
                            .label = "amide (C=" + std::to_string(o->atom_id) +
                                     "O)-N" + std::to_string(b->atom_id)
                        });
                    }
                }
            }
        }
    }

    return groups;
}

auto OrganicRuleEngine::assign_hybridization(MolecularGraph& graph) const -> void {
    for (auto& atom : graph.atoms()) {
        if (atom.atomic_number == 6) { // Carbon
            auto bo = atom.total_bond_order();
            if (bo <= 2)      atom.hybridization = VSEPR_HYB_SP;
            else if (bo == 3) atom.hybridization = VSEPR_HYB_SP2;
            else              atom.hybridization = VSEPR_HYB_SP3;
        } else if (atom.atomic_number == 7) { // Nitrogen
            auto nBonds = static_cast<int>(atom.bonded_atom_ids.size());
            if (nBonds <= 2)      atom.hybridization = VSEPR_HYB_SP2;
            else                  atom.hybridization = VSEPR_HYB_SP3;
        } else if (atom.atomic_number == 8) { // Oxygen
            atom.hybridization = VSEPR_HYB_SP2;
        }
    }
}

// ============================================================================
// PeptideBondEngine
// ============================================================================

auto PeptideBondEngine::can_form_peptide_bond(const MolecularGraph& graph,
                                              const Residue& lhs,
                                              const Residue& rhs) const -> Result<void> {
    if (!lhs.has_valid_backbone()) {
        return std::unexpected(Error{
            .code = ErrorCode::incompatible_residues,
            .message = "LHS residue " + lhs.residue_name + " has incomplete backbone"
        });
    }
    if (!rhs.has_valid_backbone()) {
        return std::unexpected(Error{
            .code = ErrorCode::incompatible_residues,
            .message = "RHS residue " + rhs.residue_name + " has incomplete backbone"
        });
    }

    // Verify the C-terminal carbon and N-terminal nitrogen exist
    if (!graph.find_atom(lhs.backbone_C)) {
        return std::unexpected(Error{
            .code = ErrorCode::atom_not_found,
            .message = "LHS backbone C atom " + std::to_string(lhs.backbone_C) + " not found"
        });
    }
    if (!graph.find_atom(rhs.backbone_N)) {
        return std::unexpected(Error{
            .code = ErrorCode::atom_not_found,
            .message = "RHS backbone N atom " + std::to_string(rhs.backbone_N) + " not found"
        });
    }

    return {};
}

auto PeptideBondEngine::form_peptide_bond(MolecularGraph& graph,
                                          Residue& lhs,
                                          Residue& rhs) const -> Result<Bond> {
    auto valid = can_form_peptide_bond(graph, lhs, rhs);
    if (!valid) return std::unexpected(valid.error());

    Bond peptide_bond {
        .a = lhs.backbone_C,
        .b = rhs.backbone_N,
        .order = 1
    };

    graph.add_bond(peptide_bond);

    // Set the amide nitrogen role
    if (auto* n_atom = graph.find_atom(rhs.backbone_N)) {
        n_atom->chem_role = VSEPR_ROLE_AMIDE_N;
    }

    return peptide_bond;
}

// ============================================================================
// BackboneGeometryEngine
// ============================================================================

auto BackboneGeometryEngine::initialize_backbone_geometry(MolecularGraph& graph) const -> Result<void> {
    // Place backbone atoms along the chain with standard bond lengths
    // This is a simplified linear placement; real geometry uses phi/psi torsions
    double x = 0.0;

    for (auto& residue : graph.residues()) {
        if (!residue.has_valid_backbone()) continue;

        if (auto* n = graph.find_atom(residue.backbone_N)) {
            n->position = {x, 0.0, 0.0};
            n->chem_role = VSEPR_ROLE_BACKBONE_N;
            x += N_CA_BOND_LENGTH_A;
        }
        if (auto* ca = graph.find_atom(residue.backbone_CA)) {
            ca->position = {x, 0.0, 0.0};
            ca->chem_role = VSEPR_ROLE_ALPHA_C;
            x += CA_C_BOND_LENGTH_A;
        }
        if (auto* c = graph.find_atom(residue.backbone_C)) {
            c->position = {x, 0.0, 0.0};
            c->chem_role = VSEPR_ROLE_CARBONYL_C;
        }
        if (auto* o = graph.find_atom(residue.backbone_O)) {
            o->position = {x, C_O_BOND_LENGTH_A, 0.0};
            o->chem_role = VSEPR_ROLE_CARBONYL_O;
        }

        x += PEPTIDE_BOND_LENGTH_A;
    }

    return {};
}

auto BackboneGeometryEngine::enforce_amide_planarity(MolecularGraph& graph) const -> Result<void> {
    // For each residue, check omega deviation from 180 (trans) or 0 (cis)
    for (auto& residue : graph.residues()) {
        double omega = residue.omega_deg;
        double dev_trans = std::abs(omega - OMEGA_TRANS_DEG);
        double dev_cis   = std::abs(omega - OMEGA_CIS_DEG);
        double dev = std::min(dev_trans, dev_cis);

        if (dev > PLANARITY_TOLERANCE_DEG) {
            // Snap to nearest planar configuration
            if (dev_trans <= dev_cis) {
                residue.omega_deg = OMEGA_TRANS_DEG;
            } else {
                residue.omega_deg = OMEGA_CIS_DEG;
            }
        }
    }
    return {};
}

auto BackboneGeometryEngine::assign_default_torsions(MolecularGraph& graph) const -> void {
    for (auto& residue : graph.residues()) {
        // Default omega = 180 (trans)
        if (std::abs(residue.omega_deg) < 1e-10 && residue.omega_deg != OMEGA_CIS_DEG) {
            residue.omega_deg = OMEGA_TRANS_DEG;
        }
    }
}

auto BackboneGeometryEngine::measure_planarity_deviation(const MolecularGraph& graph,
                                                          const Residue& residue) const -> double {
    // Measure how far the omega angle deviates from ideal planar
    double omega = residue.omega_deg;
    double dev_trans = std::abs(omega - OMEGA_TRANS_DEG);
    double dev_cis   = std::abs(omega - OMEGA_CIS_DEG);
    (void)graph; // geometry check uses torsion angle directly
    return std::min(dev_trans, dev_cis);
}

// ============================================================================
// HydrogenBondEngine
// ============================================================================

auto HydrogenBondEngine::detect_hbonds(const MolecularGraph& graph) const -> std::vector<HBond> {
    std::vector<HBond> hbonds;

    // Collect donors (N-H, O-H) and acceptors (O, N with lone pairs)
    std::vector<std::int32_t> donors;
    std::vector<std::int32_t> acceptors;

    for (const auto& atom : graph.atoms()) {
        if (atom.chem_role == VSEPR_ROLE_HYDROGEN_DONOR ||
            atom.chem_role == VSEPR_ROLE_BACKBONE_N ||
            atom.chem_role == VSEPR_ROLE_AMIDE_N) {
            donors.push_back(atom.atom_id);
        }
        if (atom.chem_role == VSEPR_ROLE_HYDROGEN_ACCEPTOR ||
            atom.chem_role == VSEPR_ROLE_CARBONYL_O) {
            acceptors.push_back(atom.atom_id);
        }
    }

    for (auto d_id : donors) {
        const auto* donor = graph.find_atom(d_id);
        if (!donor) continue;
        for (auto a_id : acceptors) {
            if (d_id == a_id) continue;
            const auto* acceptor = graph.find_atom(a_id);
            if (!acceptor) continue;

            double dist = distance(donor->position, acceptor->position);
            if (dist >= HBOND_DIST_MIN_A && dist <= HBOND_DIST_MAX_A) {
                // Score: linear interpolation, strongest at ~2.8 A
                double optimal = 2.8;
                double dev = std::abs(dist - optimal);
                double strength = std::max(0.0, 1.0 - dev / 1.0);

                hbonds.push_back({
                    .donor_atom_id = d_id,
                    .acceptor_atom_id = a_id,
                    .distance_angstrom = dist,
                    .angle_deg = 0.0, // simplified: full angle calc needs H position
                    .strength_score = strength
                });
            }
        }
    }

    return hbonds;
}

// ============================================================================
// EnergyModel
// ============================================================================

auto EnergyModel::evaluate(const MolecularGraph& graph,
                           VSEPR_EnvironmentClass environment) const -> Result<EnergyBreakdown> {
    EnergyBreakdown e {};

    // Bond stretching energy (harmonic approximation)
    // E_bond = k_bond * (r - r0)^2
    constexpr double k_bond = 200.0; // kJ/mol/A^2 (typical AMBER)
    for (const auto& bond : graph.bonds()) {
        const auto* a = graph.find_atom(bond.a);
        const auto* b = graph.find_atom(bond.b);
        if (!a || !b) continue;

        double dist = distance(a->position, b->position);
        // Reference length from covalent radii
        double r0 = (a->covalent_radius_pm + b->covalent_radius_pm) / 100.0; // pm -> A
        if (r0 < 0.5) r0 = 1.5; // fallback
        double dr = dist - r0;
        e.bond_kj_mol += 0.5 * k_bond * dr * dr;
    }

    // VdW energy (Lennard-Jones 12-6, simplified)
    constexpr double epsilon_default = 0.5; // kJ/mol
    for (std::size_t i = 0; i < graph.atoms().size(); ++i) {
        for (std::size_t j = i + 2; j < graph.atoms().size(); ++j) {
            const auto& ai = graph.atoms()[i];
            const auto& aj = graph.atoms()[j];

            // Skip bonded pairs (1-2, handled by bond term)
            bool bonded = false;
            for (auto bid : ai.bonded_atom_ids) {
                if (bid == aj.atom_id) { bonded = true; break; }
            }
            if (bonded) continue;

            double dist = distance(ai.position, aj.position);
            if (dist < 0.1) continue;

            double sigma = (ai.vdw_radius_pm + aj.vdw_radius_pm) / 200.0; // pm -> A, combined
            if (sigma < 1.0) sigma = 3.0;
            double sr6 = std::pow(sigma / dist, 6);
            e.vdw_kj_mol += 4.0 * epsilon_default * (sr6 * sr6 - sr6);
        }
    }

    // Coulomb energy (simplified, vacuum permittivity)
    constexpr double coulomb_factor = 1389.354; // kJ*A/(mol*e^2) in vacuum
    double dielectric = (environment == VSEPR_ENV_POLAR_SOLVENT) ? 80.0 :
                        (environment == VSEPR_ENV_NONPOLAR_SOLVENT) ? 4.0 : 1.0;
    for (std::size_t i = 0; i < graph.atoms().size(); ++i) {
        for (std::size_t j = i + 1; j < graph.atoms().size(); ++j) {
            const auto& ai = graph.atoms()[i];
            const auto& aj = graph.atoms()[j];
            if (std::abs(ai.partial_charge) < 1e-10 || std::abs(aj.partial_charge) < 1e-10)
                continue;

            double dist = distance(ai.position, aj.position);
            if (dist < 0.1) continue;

            e.coulomb_kj_mol += coulomb_factor * ai.partial_charge * aj.partial_charge
                                / (dielectric * dist);
        }
    }

    // Solvation energy (simplified Born model)
    if (environment == VSEPR_ENV_POLAR_SOLVENT) {
        constexpr double born_factor = -69.5; // kJ/mol per charge in water
        for (const auto& atom : graph.atoms()) {
            if (std::abs(atom.partial_charge) > 0.1) {
                double r_born = atom.vdw_radius_pm / 100.0; // pm -> A
                if (r_born < 1.0) r_born = 1.5;
                e.solvation_kj_mol += born_factor * atom.partial_charge * atom.partial_charge
                                      / r_born;
            }
        }
    }

    // Formation energy: rough peptide bond formation enthalpy
    // Each peptide bond contributes ~-8 to -16 kJ/mol
    int n_peptide_bonds = 0;
    for (const auto& bond : graph.bonds()) {
        const auto* a = graph.find_atom(bond.a);
        const auto* b = graph.find_atom(bond.b);
        if (!a || !b) continue;
        if ((a->chem_role == VSEPR_ROLE_CARBONYL_C && b->chem_role == VSEPR_ROLE_AMIDE_N) ||
            (b->chem_role == VSEPR_ROLE_CARBONYL_C && a->chem_role == VSEPR_ROLE_AMIDE_N)) {
            ++n_peptide_bonds;
        }
    }
    e.formation_kj_mol = n_peptide_bonds * (-10.0); // avg condensation energy

    e.compute_total();
    return e;
}

// ============================================================================
// ScoringEngine
// ============================================================================

auto ScoringEngine::score(const MolecularGraph& graph,
                          const EnergyBreakdown& energy,
                          VSEPR_EnvironmentClass environment) const -> Result<ScoreCard> {
    ScoreCard s {};

    // Steric score: penalty for VdW clashes (positive vdw = bad)
    s.steric_score = std::max(0.0, 1.0 - std::abs(energy.vdw_kj_mol) / 100.0);

    // Electrostatic score: favorable if negative
    s.electrostatic_score = (energy.coulomb_kj_mol < 0.0)
        ? std::min(1.0, std::abs(energy.coulomb_kj_mol) / 50.0)
        : 0.0;

    // Hydrophobic score: fraction of hydrophobic sidechains
    int hydrophobic = 0;
    int total_res = 0;
    for (const auto& res : graph.residues()) {
        ++total_res;
        if (res.sidechain_class == VSEPR_SIDECHAIN_HYDROPHOBIC ||
            res.sidechain_class == VSEPR_SIDECHAIN_AROMATIC) {
            ++hydrophobic;
        }
    }
    s.hydrophobic_score = (total_res > 0) ? static_cast<double>(hydrophobic) / total_res : 0.0;

    // Planarity score: average omega deviation from ideal
    double planarity_sum = 0.0;
    int planarity_count = 0;
    for (const auto& res : graph.residues()) {
        double dev_trans = std::abs(res.omega_deg - 180.0);
        double dev_cis   = std::abs(res.omega_deg);
        double dev = std::min(dev_trans, dev_cis);
        planarity_sum += dev;
        ++planarity_count;
    }
    if (planarity_count > 0) {
        double avg_dev = planarity_sum / planarity_count;
        s.planarity_score = std::max(0.0, 1.0 - avg_dev / PLANARITY_TOLERANCE_DEG);
    } else {
        s.planarity_score = 1.0;
    }

    // Overall confidence (geometric mean of sub-scores)
    double product = (s.steric_score + 0.01) * (s.planarity_score + 0.01)
                   * (s.electrostatic_score + 0.01);
    s.confidence_score = std::pow(product, 1.0 / 3.0);

    (void)environment; // used in solvation energy already

    return s;
}

// ============================================================================
// PeptideFormationPipeline
// ============================================================================

auto PeptideFormationPipeline::build_from_residues(std::span<const Residue> residues,
                                                   std::span<const Atom> atoms) const -> Result<MolecularGraph> {
    MolecularGraph graph {};

    for (const auto& atom : atoms) {
        auto valid = organic_rules_.validate_atom(atom);
        if (!valid) return std::unexpected(valid.error());
        graph.add_atom(atom);
    }

    for (const auto& residue : residues) {
        auto valid = organic_rules_.validate_residue(graph, residue);
        if (!valid) return std::unexpected(valid.error());
        graph.add_residue(residue);
    }

    organic_rules_.assign_hybridization(graph);
    return graph;
}

auto PeptideFormationPipeline::form_chain(MolecularGraph graph) const -> Result<MolecularGraph> {
    auto& residues = graph.residues();

    if (residues.size() < 2) {
        return graph;
    }

    for (std::size_t i = 0; i + 1 < residues.size(); ++i) {
        auto bond_result = peptide_bonds_.form_peptide_bond(graph, residues[i], residues[i + 1]);
        if (!bond_result) {
            return std::unexpected(bond_result.error());
        }
    }

    auto geom_init = geometry_.initialize_backbone_geometry(graph);
    if (!geom_init) return std::unexpected(geom_init.error());

    geometry_.assign_default_torsions(graph);

    auto planar = geometry_.enforce_amide_planarity(graph);
    if (!planar) return std::unexpected(planar.error());

    return graph;
}

auto PeptideFormationPipeline::solve(MolecularGraph graph) const -> Result<FormationSummary> {
    auto formed = form_chain(std::move(graph));
    if (!formed) return std::unexpected(formed.error());

    auto& g = *formed;

    auto energy_result = energy_.evaluate(g, environment_);
    if (!energy_result) return std::unexpected(energy_result.error());

    auto score_result = scoring_.score(g, *energy_result, environment_);
    if (!score_result) return std::unexpected(score_result.error());

    auto hbonds = hbonds_.detect_hbonds(g);
    auto func_groups = organic_rules_.detect_functional_groups(g);

    FormationSummary summary {};
    summary.formation_class = VSEPR_FORM_PEPTIDE;
    summary.formation_state = VSEPR_STATE_LOCAL_FOLDED;
    summary.energy = *energy_result;
    summary.score = *score_result;
    summary.chemical_validity_pass = true;
    summary.valence_validity_pass = true;
    summary.atom_count = g.atom_count();
    summary.residue_count = g.residue_count();
    summary.bond_count = g.bond_count();
    summary.hbond_count = static_cast<std::int32_t>(hbonds.size());
    summary.functional_groups = std::move(func_groups);

    // Validate all atom valences
    for (const auto& atom : g.atoms()) {
        auto vc = organic_rules_.check_valence(atom);
        if (!vc) {
            summary.valence_validity_pass = false;
            break;
        }
    }

    return summary;
}

} // namespace vsepr::chem
