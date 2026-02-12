#pragma once
#include "../core/state.hpp"
#include "../predict/properties.hpp"
#include <vector>
#include <map>
#include <string>
#include <memory>

namespace atomistic {
namespace reaction {

/**
 * Reaction mechanism classification
 */
enum class MechanismType {
    SUBSTITUTION,      // SN1, SN2
    ADDITION,          // Electrophilic/nucleophilic addition
    ELIMINATION,       // E1, E2
    REARRANGEMENT,     // Sigmatropic, electrocyclic
    REDOX,            // Electron transfer
    PERICYCLIC,       // Diels-Alder, cycloaddition
    RADICAL,          // Radical chain reactions
    ACID_BASE,        // Proton transfer
};

/**
 * Reaction site: atom index + type of attack
 */
struct ReactionSite {
    uint32_t atom_index;
    double fukui_plus;     // Nucleophilic attack propensity
    double fukui_minus;    // Electrophilic attack propensity
    double fukui_zero;     // Radical attack propensity
    double local_softness; // HSAB softness
    Vec3 position;
    std::string element;
};

/**
 * Reaction template: pattern-based reaction rules
 * 
 * Example: Nucleophilic substitution
 *   R-X + Nu⁻ → R-Nu + X⁻
 *   
 * Constraints:
 *   - X must have f⁻ > 0.3 (good leaving group)
 *   - Nu must have f⁺ > 0.3 (nucleophile)
 *   - R-X bond must be polarized
 */
struct ReactionTemplate {
    MechanismType mechanism;
    std::string name;
    std::string description;
    
    // Reactivity requirements
    double min_fukui_electrophile;  // Attacking nucleophile needs f⁺ > this
    double min_fukui_nucleophile;   // Attacked center needs f⁻ > this
    double min_fukui_radical;       // For radical reactions
    
    // HSAB principle: soft-soft, hard-hard matching
    bool require_hardness_match;
    double hardness_tolerance;      // |η_A - η_B| < tolerance
    
    // Geometric constraints
    double min_distance;  // Å, orbital overlap requirement
    double max_distance;  // Å, collision theory
    double min_angle;     // Degrees, orbital alignment (e.g., SN2 backside attack ~180°)
    double max_angle;
    
    // Thermodynamic filters
    double max_barrier;   // kcal/mol, kinetically feasible
    double min_exotherm;  // kcal/mol, thermodynamically driven
    
    // Valence rules
    bool conserve_valence;
    bool allow_radicals;
    bool require_octet;
};

/**
 * Proposed reaction: reactants → products with scoring
 */
struct ProposedReaction {
    State reactant_A;
    State reactant_B;  // Empty for unimolecular
    State product_C;
    State product_D;   // Empty for single product
    
    MechanismType mechanism;
    std::string description;
    
    // Reaction sites
    ReactionSite attacking_site;
    ReactionSite attacked_site;
    
    // Energetics (from predict module)
    double reaction_energy;     // kcal/mol
    double activation_barrier;  // kcal/mol
    double rate_constant;       // s⁻¹ at 298 K
    
    // Feasibility scores
    double reactivity_score;    // 0-1, Fukui function matching
    double geometric_score;     // 0-1, orbital overlap quality
    double thermodynamic_score; // 0-1, exothermicity + barrier
    double overall_score;       // Weighted combination
    
    // Validation flags
    bool mass_balanced;
    bool charge_balanced;
    bool valence_satisfied;
    bool geometrically_feasible;
    bool thermodynamically_feasible;
};

/**
 * Reaction discovery statistics
 */
struct DiscoveryStats {
    uint64_t reactions_proposed;
    uint64_t reactions_validated;
    uint64_t reactions_feasible;
    
    std::map<MechanismType, uint64_t> mechanism_counts;
    std::map<std::string, uint64_t> motif_counts;  // Common patterns
    
    double avg_barrier;
    double avg_exothermicity;
    double best_score;
};

// ============================================================================
// REACTION ENGINE INTERFACE
// ============================================================================

/**
 * Core reaction engine: generates, validates, and scores reactions
 */
class ReactionEngine {
public:
    ReactionEngine();
    
    /**
     * Identify reactive sites in a molecule
     * 
     * Uses Fukui functions to rank atoms by nucleophilic/electrophilic character
     * 
     * @param s Molecular state
     * @return Ranked list of reaction sites
     */
    std::vector<ReactionSite> identify_reactive_sites(const State& s);
    
    /**
     * Match reactive sites between two molecules
     * 
     * Uses HSAB principle: soft-soft, hard-hard pairing
     * 
     * @param sites_A Reactive sites in molecule A
     * @param sites_B Reactive sites in molecule B
     * @param template_rule Reaction template with constraints
     * @return Ranked list of proposed reactions
     */
    std::vector<ProposedReaction> match_reactive_sites(
        const State& mol_A,
        const State& mol_B,
        const std::vector<ReactionSite>& sites_A,
        const std::vector<ReactionSite>& sites_B,
        const ReactionTemplate& template_rule
    );
    
    /**
     * Generate products from reactants using template
     * 
     * Modifies topology (breaks/forms bonds), updates geometry
     * 
     * @param reactants Input molecules
     * @param attack_site Attacking nucleophile/electrophile
     * @param attacked_site Reaction center
     * @param template_rule Mechanism template
     * @return Predicted product state(s)
     */
    std::vector<State> generate_products(
        const std::vector<State>& reactants,
        const ReactionSite& attack_site,
        const ReactionSite& attacked_site,
        const ReactionTemplate& template_rule
    );
    
    /**
     * Validate reaction conserves mass, charge, valence
     * 
     * @param reaction Proposed reaction
     * @return True if valid
     */
    bool validate_reaction(ProposedReaction& reaction);
    
    /**
     * Score reaction based on reactivity, geometry, thermodynamics
     * 
     * Uses weighted combination:
     *   overall = 0.4·reactivity + 0.3·geometric + 0.3·thermodynamic
     * 
     * @param reaction Proposed reaction (modified in-place with scores)
     */
    void score_reaction(ProposedReaction& reaction);
    
    /**
     * Estimate reaction energetics using predict module
     * 
     * @param reaction Proposed reaction (modified in-place with ΔE, Ea, k)
     */
    void estimate_energetics(ProposedReaction& reaction);
    
    /**
     * Load standard reaction templates (SN2, addition, elimination, etc.)
     */
    void load_standard_templates();
    
    /**
     * Add custom reaction template
     */
    void add_template(const ReactionTemplate& tmpl);
    
    /**
     * Get all loaded templates
     */
    const std::vector<ReactionTemplate>& get_templates() const { return templates_; }
    
private:
    std::vector<ReactionTemplate> templates_;
    
    // Helper: compute HSAB matching score
    double compute_hsab_score(double hardness_A, double hardness_B);
    
    // Helper: compute geometric overlap score
    double compute_geometric_score(const ReactionSite& site_A,
                                   const ReactionSite& site_B,
                                   const ReactionTemplate& tmpl);
    
    // Helper: break bond in topology
    void break_bond(State& s, uint32_t atom_i, uint32_t atom_j);
    
    // Helper: form bond in topology
    void form_bond(State& s, uint32_t atom_i, uint32_t atom_j, double bond_order = 1.0);
};

// ============================================================================
// STANDARD REACTION TEMPLATES
// ============================================================================

/**
 * Create SN2 nucleophilic substitution template
 * 
 * R-X + Nu⁻ → R-Nu + X⁻
 * 
 * Constraints:
 *   - Backside attack (angle ~180°)
 *   - Good nucleophile (f⁺ > 0.3)
 *   - Good leaving group (f⁻ > 0.3)
 */
ReactionTemplate sn2_template();

/**
 * Create electrophilic addition template (alkene + E⁺)
 * 
 * C=C + E⁺ → C-E-C⁺
 * 
 * Constraints:
 *   - Double bond present
 *   - Electrophile with f⁻ > 0.4
 */
ReactionTemplate electrophilic_addition_template();

/**
 * Create E2 elimination template
 * 
 * R-CH₂-CH₂-X + B⁻ → R-CH=CH₂ + HB + X⁻
 * 
 * Constraints:
 *   - β-hydrogen present
 *   - Anti-periplanar geometry (H-C-C-X dihedral ~180°)
 */
ReactionTemplate e2_elimination_template();

/**
 * Create Diels-Alder cycloaddition template
 * 
 * Diene + Dienophile → Cyclohexene
 * 
 * Constraints:
 *   - Conjugated diene (4 carbons)
 *   - Alkene dienophile
 *   - Orbital symmetry allowed
 */
ReactionTemplate diels_alder_template();

/**
 * Create acid-base proton transfer template
 * 
 * HA + B⁻ → A⁻ + HB
 * 
 * Constraints:
 *   - pKa difference > 2
 *   - Favorable ΔG
 */
ReactionTemplate proton_transfer_template();

} // namespace reaction
} // namespace atomistic
