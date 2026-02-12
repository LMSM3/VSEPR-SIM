#include "engine.hpp"
#include "../predict/properties.hpp"
#include "pot/periodic_db.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace atomistic {
namespace reaction {

namespace {
    constexpr double BOLTZMANN_KCAL = 0.001987204; // kcal/(mol·K)
    constexpr double TEMPERATURE = 298.15; // K
}

ReactionEngine::ReactionEngine() {
    load_standard_templates();
}

// ============================================================================
// REACTIVE SITE IDENTIFICATION
// ============================================================================

std::vector<ReactionSite> ReactionEngine::identify_reactive_sites(const State& s) {
    // Predict electronic properties
    auto elec_props = predict::predict_electronic_properties(s);
    auto reactivity = predict::predict_reactivity(s, elec_props);
    
    std::vector<ReactionSite> sites;
    sites.reserve(s.N);
    
    for (uint32_t i = 0; i < s.N; ++i) {
        ReactionSite site;
        site.atom_index = i;
        site.fukui_plus = reactivity.fukui_plus[i];
        site.fukui_minus = reactivity.fukui_minus[i];
        site.fukui_zero = reactivity.fukui_zero[i];
        site.local_softness = reactivity.local_softness[i];
        site.position = s.X[i];
        
        // Get element name from state (would need to store in State, or infer)
        // For now, use placeholder - should integrate with periodic table
        site.element = "C"; // TODO: Get from State.elements or periodic table
        
        sites.push_back(site);
    }
    
    return sites;
}

// ============================================================================
// REACTIVE SITE MATCHING (HSAB PRINCIPLE)
// ============================================================================

std::vector<ProposedReaction> ReactionEngine::match_reactive_sites(
    const State& mol_A,
    const State& mol_B,
    const std::vector<ReactionSite>& sites_A,
    const std::vector<ReactionSite>& sites_B,
    const ReactionTemplate& template_rule)
{
    std::vector<ProposedReaction> proposals;
    
    // Match nucleophiles (high f⁺) with electrophiles (high f⁻)
    for (const auto& site_A : sites_A) {
        for (const auto& site_B : sites_B) {
            // Check if reactivity matches template requirements
            bool nucleophile_attack = 
                site_A.fukui_plus >= template_rule.min_fukui_electrophile &&
                site_B.fukui_minus >= template_rule.min_fukui_nucleophile;
            
            bool electrophile_attack =
                site_A.fukui_minus >= template_rule.min_fukui_nucleophile &&
                site_B.fukui_plus >= template_rule.min_fukui_electrophile;
            
            if (!nucleophile_attack && !electrophile_attack) {
                continue; // No reactivity match
            }
            
            // Check geometric feasibility
            double distance = norm(site_A.position - site_B.position);
            if (distance < template_rule.min_distance || 
                distance > template_rule.max_distance) {
                continue; // Too close or too far
            }
            
            // HSAB matching: soft-soft or hard-hard
            if (template_rule.require_hardness_match) {
                // Approximate hardness from Fukui functions
                // Hard: high f⁺ and low f⁻ (electron-deficient)
                // Soft: moderate f⁺ and f⁻ (polarizable)
                double hardness_A = std::abs(site_A.fukui_plus - site_A.fukui_minus);
                double hardness_B = std::abs(site_B.fukui_plus - site_B.fukui_minus);
                
                if (std::abs(hardness_A - hardness_B) > template_rule.hardness_tolerance) {
                    continue; // HSAB mismatch
                }
            }
            
            // Create proposed reaction
            ProposedReaction reaction;
            reaction.reactant_A = mol_A;
            reaction.reactant_B = mol_B;
            reaction.mechanism = template_rule.mechanism;
            reaction.description = template_rule.name + " reaction";
            
            reaction.attacking_site = nucleophile_attack ? site_A : site_B;
            reaction.attacked_site = nucleophile_attack ? site_B : site_A;
            
            // Generate products
            auto products = generate_products(
                {mol_A, mol_B},
                reaction.attacking_site,
                reaction.attacked_site,
                template_rule
            );
            
            if (products.size() >= 1) {
                reaction.product_C = products[0];
                if (products.size() >= 2) {
                    reaction.product_D = products[1];
                }
                
                // Validate and score
                if (validate_reaction(reaction)) {
                    estimate_energetics(reaction);
                    score_reaction(reaction);
                    
                    // Filter by thermodynamic feasibility
                    if (reaction.activation_barrier <= template_rule.max_barrier) {
                        proposals.push_back(reaction);
                    }
                }
            }
        }
    }
    
    // Sort by overall score (descending)
    std::sort(proposals.begin(), proposals.end(),
              [](const ProposedReaction& a, const ProposedReaction& b) {
                  return a.overall_score > b.overall_score;
              });
    
    return proposals;
}

// ============================================================================
// PRODUCT GENERATION
// ============================================================================

std::vector<State> ReactionEngine::generate_products(
    const std::vector<State>& reactants,
    const ReactionSite& attack_site,
    const ReactionSite& attacked_site,
    const ReactionTemplate& template_rule)
{
    (void)template_rule;  // TODO: Use template rules for product generation
    std::vector<State> products;
    
    // For now, implement simple bond breaking/forming
    // Full implementation would need sophisticated topology manipulation
    
    if (reactants.size() == 2) {
        // Bimolecular reaction: merge reactants, form new bond
        State product;
        
        // Merge coordinates
        product.N = reactants[0].N + reactants[1].N;
        product.X.reserve(product.N);
        product.V.reserve(product.N);
        product.M.reserve(product.N);
        
        for (const auto& r : reactants) {
            product.X.insert(product.X.end(), r.X.begin(), r.X.end());
            product.V.insert(product.V.end(), r.V.begin(), r.V.end());
            product.M.insert(product.M.end(), r.M.begin(), r.M.end());
        }
        
        // Form bond between attack site and attacked site
        uint32_t atom_i = attack_site.atom_index;
        uint32_t atom_j = attacked_site.atom_index + reactants[0].N; // Offset for second molecule
        
        form_bond(product, atom_i, atom_j, 1.0);
        
        // TODO: Handle leaving groups, bond breaking, etc.
        // This is a simplified placeholder
        
        products.push_back(product);
    }
    
    return products;
}

// ============================================================================
// VALIDATION
// ============================================================================

bool ReactionEngine::validate_reaction(ProposedReaction& reaction) {
    // Mass balance
    uint32_t reactant_atoms = reaction.reactant_A.N;
    if (reaction.reactant_B.N > 0) reactant_atoms += reaction.reactant_B.N;
    
    uint32_t product_atoms = reaction.product_C.N;
    if (reaction.product_D.N > 0) product_atoms += reaction.product_D.N;
    
    reaction.mass_balanced = (reactant_atoms == product_atoms);
    
    // Charge balance (would need to track formal charges in State)
    // For now, assume neutral molecules
    reaction.charge_balanced = true;
    
    // Valence satisfaction (simplified check)
    // Full implementation would verify octet rule, bond orders
    reaction.valence_satisfied = true;
    
    // Geometric feasibility (already checked in matching)
    reaction.geometrically_feasible = true;
    
    return reaction.mass_balanced && 
           reaction.charge_balanced && 
           reaction.valence_satisfied &&
           reaction.geometrically_feasible;
}

// ============================================================================
// SCORING
// ============================================================================

void ReactionEngine::score_reaction(ProposedReaction& reaction) {
    // Reactivity score: Fukui function matching quality
    double fukui_match = std::min(
        reaction.attacking_site.fukui_plus,
        reaction.attacked_site.fukui_minus
    );
    reaction.reactivity_score = std::min(1.0, fukui_match / 0.5); // Normalize to [0,1]
    
    // Geometric score: orbital overlap quality
    double distance = norm(reaction.attacking_site.position - 
                      reaction.attacked_site.position);
    
    // Optimal distance ~1.5-2.5 Å for bond formation
    double optimal_distance = 2.0;
    double distance_penalty = std::abs(distance - optimal_distance) / optimal_distance;
    reaction.geometric_score = std::exp(-distance_penalty * distance_penalty);
    
    // Thermodynamic score: exothermicity + reasonable barrier
    double barrier_score = std::exp(-reaction.activation_barrier / 20.0); // Favor low barriers
    double exotherm_score = (reaction.reaction_energy < 0) ? 
        (1.0 - std::exp(reaction.reaction_energy / 30.0)) : 0.0; // Favor exothermic
    
    reaction.thermodynamic_score = 0.6 * barrier_score + 0.4 * exotherm_score;
    
    // Thermodynamic feasibility: barrier < 30 kcal/mol at room temp
    reaction.thermodynamically_feasible = (reaction.activation_barrier < 30.0);
    
    // Overall score: weighted combination
    reaction.overall_score = 
        0.4 * reaction.reactivity_score +
        0.3 * reaction.geometric_score +
        0.3 * reaction.thermodynamic_score;
}

// ============================================================================
// ENERGETICS ESTIMATION
// ============================================================================

void ReactionEngine::estimate_energetics(ProposedReaction& reaction) {
    // Use predict module for BEP estimation
    reaction.reaction_energy = predict::predict_reaction_energy(
        reaction.reactant_A,
        reaction.reactant_B,
        reaction.product_C,
        reaction.product_D
    );
    
    reaction.activation_barrier = predict::predict_activation_barrier(
        reaction.reactant_A,
        reaction.product_C,
        15.0 // Intrinsic barrier
    );
    
    // Arrhenius rate constant: k = A·exp(-Ea/RT)
    // Pre-exponential factor A ~ 10^13 s⁻¹ (typical for unimolecular)
    double A = 1e13;
    double RT = BOLTZMANN_KCAL * TEMPERATURE;
    reaction.rate_constant = A * std::exp(-reaction.activation_barrier / RT);
}

// ============================================================================
// TEMPLATE MANAGEMENT
// ============================================================================

void ReactionEngine::load_standard_templates() {
    templates_.push_back(sn2_template());
    templates_.push_back(electrophilic_addition_template());
    templates_.push_back(e2_elimination_template());
    templates_.push_back(diels_alder_template());
    templates_.push_back(proton_transfer_template());
}

void ReactionEngine::add_template(const ReactionTemplate& tmpl) {
    templates_.push_back(tmpl);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

double ReactionEngine::compute_hsab_score(double hardness_A, double hardness_B) {
    // HSAB: similar hardness gives better match
    // Score = exp(-|η_A - η_B|²)
    double diff = hardness_A - hardness_B;
    return std::exp(-diff * diff / 4.0); // Normalize variance
}

double ReactionEngine::compute_geometric_score(
    const ReactionSite& site_A,
    const ReactionSite& site_B,
    const ReactionTemplate& tmpl)
{
    double distance = norm(site_A.position - site_B.position);
    
    // Linear interpolation between min and max distance
    if (distance < tmpl.min_distance || distance > tmpl.max_distance) {
        return 0.0;
    }
    
    double optimal = (tmpl.min_distance + tmpl.max_distance) / 2.0;
    double range = (tmpl.max_distance - tmpl.min_distance) / 2.0;
    
    return 1.0 - std::abs(distance - optimal) / range;
}

void ReactionEngine::break_bond(State& s, uint32_t atom_i, uint32_t atom_j) {
    (void)s; (void)atom_i; (void)atom_j;  // TODO: Implement bond breaking
    // Remove bond from topology
    // This requires State to have a bonds list - placeholder for now
    // TODO: Implement when State has topology storage
}

void ReactionEngine::form_bond(State& s, uint32_t atom_i, uint32_t atom_j, double bond_order) {
(void)s; (void)atom_i; (void)atom_j; (void)bond_order;  // TODO: Implement bond formation
    // Add bond to topology
    // TODO: Implement when State has topology storage
}

// ============================================================================
// STANDARD REACTION TEMPLATES
// ============================================================================

ReactionTemplate sn2_template() {
    ReactionTemplate tmpl;
    tmpl.mechanism = MechanismType::SUBSTITUTION;
    tmpl.name = "SN2 Nucleophilic Substitution";
    tmpl.description = "R-X + Nu⁻ → R-Nu + X⁻ (backside attack)";
    
    tmpl.min_fukui_electrophile = 0.3;  // Strong nucleophile
    tmpl.min_fukui_nucleophile = 0.3;   // Good leaving group
    tmpl.min_fukui_radical = 0.0;       // Not radical
    
    tmpl.require_hardness_match = false; // Hard-soft interactions common
    tmpl.hardness_tolerance = 10.0;
    
    tmpl.min_distance = 1.5;  // Å
    tmpl.max_distance = 3.5;  // Å
    tmpl.min_angle = 150.0;   // Backside attack
    tmpl.max_angle = 180.0;
    
    tmpl.max_barrier = 30.0;  // kcal/mol
    tmpl.min_exotherm = -5.0; // Slightly exothermic
    
    tmpl.conserve_valence = true;
    tmpl.allow_radicals = false;
    tmpl.require_octet = true;
    
    return tmpl;
}

ReactionTemplate electrophilic_addition_template() {
    ReactionTemplate tmpl;
    tmpl.mechanism = MechanismType::ADDITION;
    tmpl.name = "Electrophilic Addition";
    tmpl.description = "C=C + E⁺ → C-E-C⁺ (Markovnikov)";
    
    tmpl.min_fukui_electrophile = 0.2;  // π-nucleophile
    tmpl.min_fukui_nucleophile = 0.4;   // Strong electrophile
    tmpl.min_fukui_radical = 0.0;
    
    tmpl.require_hardness_match = true;  // Often hard electrophiles
    tmpl.hardness_tolerance = 5.0;
    
    tmpl.min_distance = 1.0;
    tmpl.max_distance = 4.0;
    tmpl.min_angle = 60.0;   // Above/below π-system
    tmpl.max_angle = 120.0;
    
    tmpl.max_barrier = 25.0;
    tmpl.min_exotherm = -10.0;
    
    tmpl.conserve_valence = true;
    tmpl.allow_radicals = false;
    tmpl.require_octet = true;
    
    return tmpl;
}

ReactionTemplate e2_elimination_template() {
    ReactionTemplate tmpl;
    tmpl.mechanism = MechanismType::ELIMINATION;
    tmpl.name = "E2 Elimination";
    tmpl.description = "R-CH₂-CH₂-X + B⁻ → R-CH=CH₂ + HB + X⁻";
    
    tmpl.min_fukui_electrophile = 0.25; // Base strength
    tmpl.min_fukui_nucleophile = 0.25;  // β-H acidity
    tmpl.min_fukui_radical = 0.0;
    
    tmpl.require_hardness_match = false;
    tmpl.hardness_tolerance = 10.0;
    
    tmpl.min_distance = 1.5;
    tmpl.max_distance = 3.0;
    tmpl.min_angle = 150.0;  // Anti-periplanar H-C-C-X
    tmpl.max_angle = 180.0;
    
    tmpl.max_barrier = 28.0;
    tmpl.min_exotherm = -8.0;
    
    tmpl.conserve_valence = true;
    tmpl.allow_radicals = false;
    tmpl.require_octet = true;
    
    return tmpl;
}

ReactionTemplate diels_alder_template() {
    ReactionTemplate tmpl;
    tmpl.mechanism = MechanismType::PERICYCLIC;
    tmpl.name = "Diels-Alder Cycloaddition";
    tmpl.description = "Diene + Dienophile → Cyclohexene";
    
    tmpl.min_fukui_electrophile = 0.15; // Diene HOMO
    tmpl.min_fukui_nucleophile = 0.15;  // Dienophile LUMO
    tmpl.min_fukui_radical = 0.0;
    
    tmpl.require_hardness_match = true;  // Orbital symmetry
    tmpl.hardness_tolerance = 3.0;
    
    tmpl.min_distance = 2.0;
    tmpl.max_distance = 4.5;  // Larger for π-stacking
    tmpl.min_angle = 0.0;     // Parallel approach
    tmpl.max_angle = 45.0;
    
    tmpl.max_barrier = 35.0;  // Often higher for pericyclic
    tmpl.min_exotherm = -15.0; // Very exothermic
    
    tmpl.conserve_valence = true;
    tmpl.allow_radicals = false;
    tmpl.require_octet = true;
    
    return tmpl;
}

ReactionTemplate proton_transfer_template() {
    ReactionTemplate tmpl;
    tmpl.mechanism = MechanismType::ACID_BASE;
    tmpl.name = "Proton Transfer";
    tmpl.description = "HA + B⁻ → A⁻ + HB";
    
    tmpl.min_fukui_electrophile = 0.35; // Strong base (high f⁺)
    tmpl.min_fukui_nucleophile = 0.35;  // Acidic proton (high f⁻)
    tmpl.min_fukui_radical = 0.0;
    
    tmpl.require_hardness_match = false;
    tmpl.hardness_tolerance = 15.0;
    
    tmpl.min_distance = 1.2;  // H-bond distance
    tmpl.max_distance = 2.5;
    tmpl.min_angle = 140.0;   // Linear proton transfer
    tmpl.max_angle = 180.0;
    
    tmpl.max_barrier = 15.0;  // Fast reaction
    tmpl.min_exotherm = -3.0; // ΔpKa driven
    
    tmpl.conserve_valence = true;
    tmpl.allow_radicals = false;
    tmpl.require_octet = true;
    
    return tmpl;
}

} // namespace reaction
} // namespace atomistic
