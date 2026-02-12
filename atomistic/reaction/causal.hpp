#pragma once
#include "../core/state.hpp"
#include "engine.hpp"
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <functional>

namespace atomistic {
namespace reaction {

/**
 * Electron flow signature: canonical representation of reaction mechanism
 * 
 * Encodes:
 *   - Bonds broken/formed
 *   - Formal charge changes
 *   - Electron donor/acceptor roles
 *   - Coordination number changes
 * 
 * Example (SN2):
 *   bonds_broken: [(C-X)]
 *   bonds_formed: [(Nu-C)]
 *   charge_shifts: [Nu: 0→+1, X: 0→-1]
 *   electron_flow: Nu→C, C-X→X
 */
struct ElectronFlowSignature {
    std::vector<std::pair<uint32_t, uint32_t>> bonds_broken;  // Atom indices
    std::vector<std::pair<uint32_t, uint32_t>> bonds_formed;
    
    std::map<uint32_t, int> formal_charge_before;  // Atom index → charge
    std::map<uint32_t, int> formal_charge_after;
    
    std::map<uint32_t, int> coordination_before;   // Coordination number
    std::map<uint32_t, int> coordination_after;
    
    std::string donor_role;      // "nucleophile", "electrophile", "radical"
    std::string acceptor_role;
    
    // Canonical hash for matching
    uint64_t hash() const;
    
    // Similarity to another signature (0-1)
    double similarity(const ElectronFlowSignature& other) const;
};

/**
 * Graph rewrite rule: reusable micro-template
 * 
 * Example:
 *   name: "nucleophilic_displacement"
 *   pattern: Nu⁻ + R-X → R-Nu + X⁻
 *   signature: { bonds_broken: [(R-X)], bonds_formed: [(Nu-R)] }
 *   conditions: { f⁺(Nu) > 0.3, f⁻(X) > 0.3, angle(Nu-R-X) > 150° }
 */
struct GraphRewriteRule {
    std::string name;
    ElectronFlowSignature signature;
    
    // Conditions for applicability
    double min_fukui_donor;
    double min_fukui_acceptor;
    double min_geometric_score;
    
    // Learned statistics
    uint64_t times_applied;
    uint64_t times_successful;
    double avg_barrier;
    double std_barrier;
    
    // Uncertainty estimates
    double epistemic_uncertainty;   // Model uncertainty
    double aleatoric_uncertainty;   // Inherent randomness
    
    // Compute success probability
    double success_probability() const {
        if (times_applied == 0) return 0.5;  // Prior
        return static_cast<double>(times_successful) / times_applied;
    }
    
    // Compute uncertainty (Shannon entropy)
    double uncertainty() const {
        double p = success_probability();
        if (p == 0.0 || p == 1.0) return 0.0;
        return -p * std::log2(p) - (1-p) * std::log2(1-p);
    }
};

/**
 * Causal factor: condition that influences reaction success
 * 
 * Example:
 *   factor: "leaving_group_quality"
 *   measure: f⁻(X)
 *   effect_size: 0.82 (strong positive correlation)
 *   causal_confidence: 0.91 (high confidence it's causal, not just correlated)
 */
struct CausalFactor {
    std::string name;
    std::string description;
    
    // Measure this factor (returns value 0-1)
    std::function<double(const ProposedReaction&)> measure;
    
    // Learned from data
    double effect_size;         // Correlation with success (-1 to +1)
    double causal_confidence;   // Probability this is causal (0-1)
    
    // From counterfactual testing
    double robustness;          // How much perturbation survives
};

/**
 * Causal graph edge: factor → outcome
 */
struct CausalEdge {
    std::string from_factor;
    std::string to_outcome;
    double strength;            // Effect size
    double confidence;          // Confidence in causality
    
    std::vector<std::string> confounders;  // Other factors that mediate
};

/**
 * Counterfactual experiment: perturb conditions and observe
 * 
 * Example:
 *   original: CH3Br + OH⁻ → CH3OH + Br⁻ (success)
 *   perturbation: "increase_steric_bulk" → (CH3)3C-Br + OH⁻
 *   outcome: fails (E2 instead)
 *   conclusion: "backside_access" is necessary condition
 */
struct CounterfactualExperiment {
    ProposedReaction original;
    std::string perturbation_type;
    ProposedReaction perturbed;
    
    bool original_succeeded;
    bool perturbed_succeeded;
    
    // Causal conclusion
    std::string necessary_condition;    // What was required
    std::string sufficient_condition;   // What alone caused success
};

/**
 * Diversity metric: measure novelty in mechanism space
 */
struct DiversityMetric {
    // Mechanism diversity (not just molecule diversity)
    double electron_flow_novelty;      // New signature vs. database
    double intermediate_class_novelty; // New coordination/oxidation state
    double condition_novelty;          // New HSAB combination
    
    // Computed from KL divergence to existing distribution
    double overall_novelty() const {
        return (electron_flow_novelty + intermediate_class_novelty + condition_novelty) / 3.0;
    }
};

/**
 * Information gain: expected learning from exploring a reaction
 * 
 * High information gain when:
 *   - Uncertainty is high (could go either way)
 *   - Result would strongly update beliefs
 *   - Covers underexplored region of mechanism space
 */
struct InformationGain {
    double uncertainty_reduction;  // Bits of entropy reduced
    double mechanism_coverage;     // Fills gap in mechanism space
    double causal_clarity;         // Disambiguates causal factors
    
    double expected_gain() const {
        return 0.5 * uncertainty_reduction + 
               0.3 * mechanism_coverage + 
               0.2 * causal_clarity;
    }
};

// ============================================================================
// CAUSAL DISCOVERY ENGINE
// ============================================================================

/**
 * Layer 2: Causal discovery with mechanistic abstraction
 * 
 * Goes beyond pattern mining to understand WHY reactions work:
 *   - Encodes reactions as graph rewrite rules
 *   - Builds causal graphs (not just correlations)
 *   - Tests counterfactuals to validate causality
 *   - Enforces diversity to avoid overfitting
 */
class CausalDiscoveryEngine {
public:
    CausalDiscoveryEngine();
    
    /**
     * Extract electron flow signature from reaction
     * 
     * Analyzes topology changes to identify:
     *   - Which bonds break/form
     *   - Formal charge redistribution
     *   - Donor/acceptor roles
     * 
     * @param reaction Proposed reaction with reactants/products
     * @return Canonical electron flow signature
     */
    ElectronFlowSignature extract_signature(const ProposedReaction& reaction);
    
    /**
     * Convert signature to reusable graph rewrite rule
     * 
     * Generalizes from specific reaction to template:
     *   - Extracts structural pattern
     *   - Identifies required conditions
     *   - Estimates uncertainty
     * 
     * @param signature Electron flow signature
     * @param reactions All reactions with this signature
     * @return Graph rewrite rule (micro-template)
     */
    GraphRewriteRule signature_to_rule(
        const ElectronFlowSignature& signature,
        const std::vector<ProposedReaction>& reactions
    );
    
    /**
     * Build causal graph from reaction database
     * 
     * Uses causal inference to distinguish:
     *   - "Leaving group f⁻ is high" → correlation
     *   - "Good leaving group causes success" → causation
     * 
     * Methods:
     *   - Counterfactual testing (perturb and observe)
     *   - Do-calculus (intervention analysis)
     *   - Conditional independence tests
     * 
     * @param reactions Database of reactions
     * @return Causal graph with confidence scores
     */
    std::vector<CausalEdge> build_causal_graph(
        const std::vector<ProposedReaction>& reactions
    );
    
    /**
     * Generate counterfactual experiments
     * 
     * For successful reaction, create variants:
     *   - Perturb leaving group (Br → Cl → I)
     *   - Change nucleophile strength (OH⁻ → H₂O)
     *   - Modify steric bulk (CH₃ → (CH₃)₃C)
     *   - Vary solvent polarity proxy
     * 
     * Observe which perturbations kill the reaction → necessary conditions
     * 
     * @param reaction Successful reaction to perturb
     * @return List of counterfactual experiments
     */
    std::vector<CounterfactualExperiment> generate_counterfactuals(
        const ProposedReaction& reaction
    );
    
    /**
     * Run counterfactual experiment
     * 
     * Actually tests perturbed reaction:
     *   - Applies perturbation to structure
     *   - Re-runs reaction engine
     *   - Compares outcome
     * 
     * @param experiment Counterfactual to test
     * @return Updated experiment with outcome
     */
    CounterfactualExperiment run_counterfactual(
        const CounterfactualExperiment& experiment
    );
    
    /**
     * Infer causal factors from counterfactuals
     * 
     * Analyzes which perturbations matter:
     *   - Robust rule: survives all perturbations
     *   - Fragile rule: specific to original conditions
     * 
     * Updates causal confidence based on results.
     * 
     * @param experiments Completed counterfactuals
     * @return Causal factors with confidence scores
     */
    std::vector<CausalFactor> infer_causal_factors(
        const std::vector<CounterfactualExperiment>& experiments
    );
    
    /**
     * Compute diversity of proposed reaction
     * 
     * Measures novelty in mechanism space (not molecule space):
     *   - New electron flow signature?
     *   - New intermediate class?
     *   - Unexplored HSAB combination?
     * 
     * @param reaction Proposed reaction
     * @param database Existing reactions
     * @return Diversity metric (0 = redundant, 1 = novel)
     */
    DiversityMetric compute_diversity(
        const ProposedReaction& reaction,
        const std::vector<ProposedReaction>& database
    );
    
    /**
     * Compute expected information gain
     * 
     * How much would we learn from exploring this reaction?
     *   - High uncertainty → reduces entropy
     *   - Novel mechanism → fills knowledge gap
     *   - Causal ambiguity → disambiguates factors
     * 
     * Use for active learning: prioritize high-gain proposals.
     * 
     * @param reaction Proposed reaction
     * @param rules Current rule database
     * @return Information gain estimate
     */
    InformationGain compute_information_gain(
        const ProposedReaction& reaction,
        const std::vector<GraphRewriteRule>& rules
    );
    
    /**
     * Rank proposals by learning value
     * 
     * Not just "highest score" but "most informative":
     *   - Exploration: high uncertainty, novel mechanism
     *   - Exploitation: high confidence, known mechanism
     * 
     * Use epsilon-greedy or UCB1 strategy.
     * 
     * @param proposals Candidate reactions
     * @param exploration_weight 0 = exploit, 1 = explore
     * @return Ranked proposals (by learning value)
     */
    std::vector<ProposedReaction> rank_by_learning_value(
        const std::vector<ProposedReaction>& proposals,
        double exploration_weight = 0.3
    );
    
    /**
     * Detect failure modes
     * 
     * Analyzes failed reactions for patterns:
     *   - Valence violations (octet rule)
     *   - Radiation instability (too many electrons)
     *   - Energy imbalance (enormous barrier)
     * 
     * Updates rules to avoid these in future.
     * 
     * @param failed_reactions Reactions that failed validation
     * @return List of failure patterns
     */
    std::vector<std::string> detect_failure_modes(
        const std::vector<ProposedReaction>& failed_reactions
    );
    
    /**
     * Refine rules based on causal analysis
     * 
     * Uses causal graph + counterfactuals to:
     *   - Tighten constraints (avoid false positives)
     *   - Relax constraints (avoid false negatives)
     *   - Add necessary conditions discovered
     * 
     * @param rules Current rules
     * @param causal_graph Inferred causality
     * @return Updated rules
     */
    std::vector<GraphRewriteRule> refine_rules(
        const std::vector<GraphRewriteRule>& rules,
        const std::vector<CausalEdge>& causal_graph
    );
    
    /**
     * Generate "next experiments" queue
     * 
     * Active learning: propose reactions optimized for learning:
     *   - High information gain
     *   - Diverse mechanisms
     *   - Causal disambiguation
     * 
     * NOT just "highest predicted score"
     * 
     * @param num_experiments Number to propose
     * @return Queue of experiments ranked by learning value
     */
    std::vector<ProposedReaction> generate_next_experiments(
        uint32_t num_experiments = 20
    );
    
    /**
     * Access current graph rewrite rules
     */
    const std::vector<GraphRewriteRule>& get_rules() const { return rules_; }
    
    /**
     * Access causal graph
     */
    const std::vector<CausalEdge>& get_causal_graph() const { return causal_graph_; }
    
private:
    std::vector<GraphRewriteRule> rules_;
    std::vector<CausalEdge> causal_graph_;
    std::vector<CausalFactor> causal_factors_;
    
    // Database of all explored reactions
    std::vector<ProposedReaction> reaction_database_;
    std::vector<bool> success_flags_;
    
    // Signature database for diversity
    std::map<uint64_t, uint64_t> signature_counts_;  // hash → count
    
    // Helper: compute formal charges from partial charges
    std::map<uint32_t, int> infer_formal_charges(const State& s);
    
    // Helper: compute coordination numbers
    std::map<uint32_t, int> compute_coordination(const State& s);
    
    // Helper: identify donor/acceptor roles
    void identify_roles(const ProposedReaction& reaction,
                       std::string& donor, std::string& acceptor);
    
    // Helper: perturbation generators
    State perturb_leaving_group(const State& s, const std::string& new_group);
    State perturb_nucleophile(const State& s, double strength_factor);
    State perturb_steric_bulk(const State& s, uint32_t atom_idx);
    
    // Helper: causal inference (do-calculus)
    double compute_causal_effect(const std::string& factor,
                                const std::string& outcome,
                                const std::vector<ProposedReaction>& data);
    
    // Helper: conditional independence test
    bool is_conditionally_independent(const std::string& X,
                                     const std::string& Y,
                                     const std::vector<std::string>& Z,
                                     const std::vector<ProposedReaction>& data);
};

// ============================================================================
// UTILITIES
// ============================================================================

/**
 * Compare two electron flow signatures
 * 
 * @return Similarity score 0-1 (1 = identical mechanism)
 */
double compare_signatures(const ElectronFlowSignature& a,
                         const ElectronFlowSignature& b);

/**
 * Visualize causal graph as DOT format (for Graphviz)
 * 
 * @param edges Causal edges
 * @param filename Output .dot file
 */
void export_causal_graph_dot(const std::vector<CausalEdge>& edges,
                             const std::string& filename);

/**
 * Generate markdown report of causal analysis
 * 
 * Includes:
 *   - Top causal factors ranked by effect size
 *   - Robust vs. fragile rules
 *   - Counterfactual results
 *   - Failure mode patterns
 * 
 * @param engine Causal discovery engine
 * @param filename Output .md file
 */
void generate_causal_report(const CausalDiscoveryEngine& engine,
                           const std::string& filename);

} // namespace reaction
} // namespace atomistic
