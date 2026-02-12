#include "causal.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <iostream>
#include <fstream>
#include <functional>
#include <set>

namespace atomistic {
namespace reaction {

namespace {
    std::mt19937 rng(std::random_device{}());
    
    // Hash combining function
    uint64_t hash_combine(uint64_t seed, uint64_t value) {
        return seed ^ (value + 0x9e3779b9 + (seed << 6) + (seed >> 2));
    }
}

// ============================================================================
// ELECTRON FLOW SIGNATURE
// ============================================================================

uint64_t ElectronFlowSignature::hash() const {
    uint64_t h = 0;
    
    // Hash bonds broken/formed (order-independent)
    for (const auto& [i, j] : bonds_broken) {
        uint64_t bond_hash = std::min(i, j) * 1000 + std::max(i, j);
        h = hash_combine(h, bond_hash);
    }
    
    for (const auto& [i, j] : bonds_formed) {
        uint64_t bond_hash = std::min(i, j) * 1000000 + std::max(i, j);
        h = hash_combine(h, bond_hash);
    }
    
    // Hash charge changes
    for (const auto& [atom, charge] : formal_charge_after) {
        auto it = formal_charge_before.find(atom);
        if (it != formal_charge_before.end()) {
            int delta = charge - it->second;
            if (delta != 0) {
                h = hash_combine(h, atom * 1000000000ULL + static_cast<uint64_t>(delta + 10));
            }
        }
    }
    
    return h;
}

double ElectronFlowSignature::similarity(const ElectronFlowSignature& other) const {
    // Jaccard similarity on bond changes
    std::set<std::pair<uint32_t, uint32_t>> broken_a(bonds_broken.begin(), bonds_broken.end());
    std::set<std::pair<uint32_t, uint32_t>> broken_b(other.bonds_broken.begin(), other.bonds_broken.end());
    
    std::vector<std::pair<uint32_t, uint32_t>> broken_intersection;
    std::set_intersection(broken_a.begin(), broken_a.end(),
                         broken_b.begin(), broken_b.end(),
                         std::back_inserter(broken_intersection));
    
    std::vector<std::pair<uint32_t, uint32_t>> broken_union;
    std::set_union(broken_a.begin(), broken_a.end(),
                  broken_b.begin(), broken_b.end(),
                  std::back_inserter(broken_union));
    
    double jaccard_broken = broken_union.empty() ? 0.0 :
        static_cast<double>(broken_intersection.size()) / broken_union.size();
    
    // Same for formed bonds
    std::set<std::pair<uint32_t, uint32_t>> formed_a(bonds_formed.begin(), bonds_formed.end());
    std::set<std::pair<uint32_t, uint32_t>> formed_b(other.bonds_formed.begin(), other.bonds_formed.end());
    
    std::vector<std::pair<uint32_t, uint32_t>> formed_intersection;
    std::set_intersection(formed_a.begin(), formed_a.end(),
                         formed_b.begin(), formed_b.end(),
                         std::back_inserter(formed_intersection));
    
    std::vector<std::pair<uint32_t, uint32_t>> formed_union;
    std::set_union(formed_a.begin(), formed_a.end(),
                  formed_b.begin(), formed_b.end(),
                  std::back_inserter(formed_union));
    
    double jaccard_formed = formed_union.empty() ? 0.0 :
        static_cast<double>(formed_intersection.size()) / formed_union.size();
    
    // Role similarity
    double role_similarity = (donor_role == other.donor_role && 
                             acceptor_role == other.acceptor_role) ? 1.0 : 0.0;
    
    return 0.4 * jaccard_broken + 0.4 * jaccard_formed + 0.2 * role_similarity;
}

// ============================================================================
// CAUSAL DISCOVERY ENGINE
// ============================================================================

CausalDiscoveryEngine::CausalDiscoveryEngine() {
    // Initialize causal factors to track
    CausalFactor nucleophile_strength;
    nucleophile_strength.name = "nucleophile_strength";
    nucleophile_strength.description = "Fukui f+ of attacking species";
    nucleophile_strength.measure = [](const ProposedReaction& r) {
        return r.attacking_site.fukui_plus;
    };
    causal_factors_.push_back(nucleophile_strength);
    
    CausalFactor leaving_group_quality;
    leaving_group_quality.name = "leaving_group_quality";
    leaving_group_quality.description = "Fukui f- of leaving group";
    leaving_group_quality.measure = [](const ProposedReaction& r) {
        return r.attacked_site.fukui_minus;
    };
    causal_factors_.push_back(leaving_group_quality);
    
    CausalFactor geometric_feasibility;
    geometric_feasibility.name = "geometric_feasibility";
    geometric_feasibility.description = "Orbital overlap quality";
    geometric_feasibility.measure = [](const ProposedReaction& r) {
        return r.geometric_score;
    };
    causal_factors_.push_back(geometric_feasibility);
    
    CausalFactor thermodynamic_driving_force;
    thermodynamic_driving_force.name = "thermodynamic_driving_force";
    thermodynamic_driving_force.description = "Exothermicity";
    thermodynamic_driving_force.measure = [](const ProposedReaction& r) {
        return -r.reaction_energy / 50.0;  // Normalize to 0-1
    };
    causal_factors_.push_back(thermodynamic_driving_force);
}

ElectronFlowSignature CausalDiscoveryEngine::extract_signature(
    const ProposedReaction& reaction)
{
    ElectronFlowSignature sig;
    
    // Infer formal charges from states
    sig.formal_charge_before = infer_formal_charges(reaction.reactant_A);
    sig.formal_charge_after = infer_formal_charges(reaction.product_C);
    
    // Compute coordination numbers
    sig.coordination_before = compute_coordination(reaction.reactant_A);
    sig.coordination_after = compute_coordination(reaction.product_C);
    
    // Identify bonds broken/formed (simplified - needs full topology)
    // This would compare bond lists in reactants vs products
    // For now, placeholder based on reaction sites
    uint32_t attacking_idx = reaction.attacking_site.atom_index;
    uint32_t attacked_idx = reaction.attacked_site.atom_index;
    
    sig.bonds_formed.push_back({attacking_idx, attacked_idx});
    
    // Identify roles
    identify_roles(reaction, sig.donor_role, sig.acceptor_role);
    
    return sig;
}

GraphRewriteRule CausalDiscoveryEngine::signature_to_rule(
    const ElectronFlowSignature& signature,
    const std::vector<ProposedReaction>& reactions)
{
    GraphRewriteRule rule;
    rule.signature = signature;
    rule.times_applied = reactions.size();
    
    // Compute statistics over reactions with this signature
    uint64_t successes = 0;
    double sum_barrier = 0.0;
    double sum_barrier_sq = 0.0;
    
    double min_fukui_donor = 1.0;
    double min_fukui_acceptor = 1.0;
    double min_geom = 1.0;
    
    for (const auto& r : reactions) {
        if (r.thermodynamically_feasible) successes++;
        
        sum_barrier += r.activation_barrier;
        sum_barrier_sq += r.activation_barrier * r.activation_barrier;
        
        min_fukui_donor = std::min(min_fukui_donor, r.attacking_site.fukui_plus);
        min_fukui_acceptor = std::min(min_fukui_acceptor, r.attacked_site.fukui_minus);
        min_geom = std::min(min_geom, r.geometric_score);
    }
    
    rule.times_successful = successes;
    
    double n = static_cast<double>(reactions.size());
    rule.avg_barrier = sum_barrier / n;
    rule.std_barrier = std::sqrt(sum_barrier_sq / n - rule.avg_barrier * rule.avg_barrier);
    
    // Set thresholds (conservative: min observed values)
    rule.min_fukui_donor = min_fukui_donor * 0.9;
    rule.min_fukui_acceptor = min_fukui_acceptor * 0.9;
    rule.min_geometric_score = min_geom * 0.9;
    
    // Uncertainty: high when few observations or high variance
    rule.epistemic_uncertainty = 1.0 / std::sqrt(n + 1.0);  // Decreases with data
    rule.aleatoric_uncertainty = rule.std_barrier / 20.0;   // Inherent randomness
    
    // Generate name
    if (signature.donor_role == "nucleophile" && signature.acceptor_role == "electrophile") {
        rule.name = "nucleophilic_attack";
    } else if (signature.donor_role == "electrophile" && signature.acceptor_role == "nucleophile") {
        rule.name = "electrophilic_attack";
    } else {
        rule.name = "unknown_mechanism";
    }
    
    return rule;
}

std::vector<CausalEdge> CausalDiscoveryEngine::build_causal_graph(
    const std::vector<ProposedReaction>& reactions)
{
    std::vector<CausalEdge> edges;
    
    // For each causal factor, compute effect on success
    for (auto& factor : causal_factors_) {
        // Compute correlation
        double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0;
        double sum_x2 = 0.0, sum_y2 = 0.0;
        
        for (size_t i = 0; i < reactions.size(); ++i) {
            double x = factor.measure(reactions[i]);
            double y = success_flags_[i] ? 1.0 : 0.0;
            
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_x2 += x * x;
            sum_y2 += y * y;
        }
        
        double n = static_cast<double>(reactions.size());
        double correlation = (n * sum_xy - sum_x * sum_y) /
            std::sqrt((n * sum_x2 - sum_x * sum_x) * (n * sum_y2 - sum_y * sum_y));
        
        factor.effect_size = correlation;
        
        // Compute causal effect via intervention (simplified do-calculus)
        double causal_effect = compute_causal_effect(factor.name, "success", reactions);
        
        // If intervention effect > correlation, likely causal (not confounded)
        factor.causal_confidence = std::abs(causal_effect) / (std::abs(correlation) + 0.01);
        factor.causal_confidence = std::min(1.0, factor.causal_confidence);
        
        // Add edge if strong effect
        if (std::abs(correlation) > 0.3 && factor.causal_confidence > 0.5) {
            CausalEdge edge;
            edge.from_factor = factor.name;
            edge.to_outcome = "success";
            edge.strength = causal_effect;
            edge.confidence = factor.causal_confidence;
            edges.push_back(edge);
        }
    }
    
    return edges;
}

std::vector<CounterfactualExperiment> CausalDiscoveryEngine::generate_counterfactuals(
    const ProposedReaction& reaction)
{
    std::vector<CounterfactualExperiment> experiments;
    
    // Perturbation 1: Weaken nucleophile
    {
        CounterfactualExperiment exp;
        exp.original = reaction;
        exp.perturbation_type = "weaken_nucleophile";
        exp.perturbed = reaction;
        
        // Reduce Fukui f+ by 30%
        exp.perturbed.attacking_site.fukui_plus *= 0.7;
        exp.perturbed.reactivity_score *= 0.7;
        
        exp.original_succeeded = reaction.thermodynamically_feasible;
        
        experiments.push_back(exp);
    }
    
    // Perturbation 2: Worsen leaving group
    {
        CounterfactualExperiment exp;
        exp.original = reaction;
        exp.perturbation_type = "worsen_leaving_group";
        exp.perturbed = reaction;
        
        // Reduce Fukui f- by 30%
        exp.perturbed.attacked_site.fukui_minus *= 0.7;
        exp.perturbed.reactivity_score *= 0.7;
        
        exp.original_succeeded = reaction.thermodynamically_feasible;
        
        experiments.push_back(exp);
    }
    
    // Perturbation 3: Increase steric hindrance
    {
        CounterfactualExperiment exp;
        exp.original = reaction;
        exp.perturbation_type = "increase_steric_bulk";
        exp.perturbed = reaction;
        
        // Reduce geometric score
        exp.perturbed.geometric_score *= 0.6;
        exp.perturbed.activation_barrier += 5.0;  // Steric strain
        
        exp.original_succeeded = reaction.thermodynamically_feasible;
        
        experiments.push_back(exp);
    }
    
    // Perturbation 4: Make less exothermic
    {
        CounterfactualExperiment exp;
        exp.original = reaction;
        exp.perturbation_type = "reduce_thermodynamic_driving";
        exp.perturbed = reaction;
        
        // Make reaction less favorable
        exp.perturbed.reaction_energy += 10.0;  // Less exothermic
        exp.perturbed.activation_barrier += 4.0;  // BEP relation
        
        exp.original_succeeded = reaction.thermodynamically_feasible;
        
        experiments.push_back(exp);
    }
    
    return experiments;
}

CounterfactualExperiment CausalDiscoveryEngine::run_counterfactual(
    const CounterfactualExperiment& experiment)
{
    CounterfactualExperiment result = experiment;
    
    // Re-score perturbed reaction
    double overall = 0.4 * result.perturbed.reactivity_score +
                    0.3 * result.perturbed.geometric_score +
                    0.3 * result.perturbed.thermodynamic_score;
    
    result.perturbed.overall_score = overall;
    
    // Check if still feasible
    result.perturbed_succeeded = (overall >= 0.5 &&
                                 result.perturbed.activation_barrier <= 30.0);
    
    // Infer necessary condition
    if (result.original_succeeded && !result.perturbed_succeeded) {
        result.necessary_condition = result.perturbation_type + " was critical";
    } else if (result.original_succeeded && result.perturbed_succeeded) {
        result.sufficient_condition = "reaction robust to " + result.perturbation_type;
    }
    
    return result;
}

std::vector<CausalFactor> CausalDiscoveryEngine::infer_causal_factors(
    const std::vector<CounterfactualExperiment>& experiments)
{
    std::map<std::string, uint64_t> perturbation_kills;  // How often perturbation fails
    std::map<std::string, uint64_t> perturbation_total;
    
    for (const auto& exp : experiments) {
        perturbation_total[exp.perturbation_type]++;
        
        if (exp.original_succeeded && !exp.perturbed_succeeded) {
            perturbation_kills[exp.perturbation_type]++;
        }
    }
    
    // Update causal factors based on counterfactuals
    for (auto& factor : causal_factors_) {
        std::string perturb_key;
        
        // Map factor to perturbation type
        if (factor.name == "nucleophile_strength") {
            perturb_key = "weaken_nucleophile";
        } else if (factor.name == "leaving_group_quality") {
            perturb_key = "worsen_leaving_group";
        } else if (factor.name == "geometric_feasibility") {
            perturb_key = "increase_steric_bulk";
        } else if (factor.name == "thermodynamic_driving_force") {
            perturb_key = "reduce_thermodynamic_driving";
        }
        
        if (perturbation_total[perturb_key] > 0) {
            double kill_rate = static_cast<double>(perturbation_kills[perturb_key]) /
                              perturbation_total[perturb_key];
            
            // High kill rate → factor is necessary (robustness is inverse)
            factor.robustness = 1.0 - kill_rate;
            
            // If perturbing this factor kills reactions, it's likely causal
            factor.causal_confidence = std::min(1.0, factor.causal_confidence + kill_rate * 0.5);
        }
    }
    
    return causal_factors_;
}

DiversityMetric CausalDiscoveryEngine::compute_diversity(
    const ProposedReaction& reaction,
    const std::vector<ProposedReaction>& database)
{
    DiversityMetric metric;
    
    // Extract signature
    auto sig = extract_signature(reaction);
    uint64_t h = sig.hash();
    
    // Electron flow novelty: how rare is this signature?
    uint64_t signature_count = signature_counts_[h];
    uint64_t total_reactions = database.size();
    
    double frequency = total_reactions > 0 ?
        static_cast<double>(signature_count) / total_reactions : 0.0;
    
    metric.electron_flow_novelty = 1.0 - frequency;  // Rare = novel
    
    // Intermediate class novelty: new coordination numbers?
    bool novel_coordination = false;
    for (const auto& [atom, coord_after] : sig.coordination_after) {
        auto it = sig.coordination_before.find(atom);
        if (it != sig.coordination_before.end()) {
            int delta = coord_after - it->second;
            if (std::abs(delta) >= 2) {  // Significant change
                novel_coordination = true;
                break;
            }
        }
    }
    
    metric.intermediate_class_novelty = novel_coordination ? 1.0 : 0.3;
    
    // Condition novelty: unusual HSAB combination?
    double hardness_product = reaction.attacking_site.fukui_plus *
                             reaction.attacked_site.fukui_minus;
    
    // Compute average hardness product in database
    double sum_hardness = 0.0;
    for (const auto& r : database) {
        sum_hardness += r.attacking_site.fukui_plus * r.attacked_site.fukui_minus;
    }
    
    double avg_hardness = database.empty() ? 0.2 : sum_hardness / database.size();
    
    metric.condition_novelty = std::abs(hardness_product - avg_hardness) / avg_hardness;
    metric.condition_novelty = std::min(1.0, metric.condition_novelty);
    
    return metric;
}

InformationGain CausalDiscoveryEngine::compute_information_gain(
    const ProposedReaction& reaction,
    const std::vector<GraphRewriteRule>& rules)
{
    InformationGain gain;
    
    // Find matching rule
    auto sig = extract_signature(reaction);
    const GraphRewriteRule* matching_rule = nullptr;
    
    for (const auto& rule : rules) {
        if (sig.similarity(rule.signature) > 0.8) {
            matching_rule = &rule;
            break;
        }
    }
    
    // Uncertainty reduction: Shannon entropy
    if (matching_rule) {
        gain.uncertainty_reduction = matching_rule->uncertainty();
    } else {
        gain.uncertainty_reduction = 1.0;  // Maximum for unknown mechanism
    }
    
    // Mechanism coverage: how much do we explore this region?
    auto diversity = compute_diversity(reaction, reaction_database_);
    gain.mechanism_coverage = diversity.overall_novelty();
    
    // Causal clarity: would this disambiguate causal factors?
    // Check if reaction is near decision boundary
    double min_factor_value = 1.0;
    for (const auto& factor : causal_factors_) {
        double value = factor.measure(reaction);
        min_factor_value = std::min(min_factor_value, value);
    }
    
    // Near boundary (value ~ 0.5) → high causal clarity
    gain.causal_clarity = 1.0 - 2.0 * std::abs(min_factor_value - 0.5);
    
    return gain;
}

std::vector<ProposedReaction> CausalDiscoveryEngine::rank_by_learning_value(
    const std::vector<ProposedReaction>& proposals,
    double exploration_weight)
{
    std::vector<std::pair<double, ProposedReaction>> scored;
    
    for (const auto& proposal : proposals) {
        auto info_gain = compute_information_gain(proposal, rules_);
        
        // UCB1-like formula
        double exploitation = proposal.overall_score;
        double exploration = info_gain.expected_gain();
        
        double learning_value = (1.0 - exploration_weight) * exploitation +
                               exploration_weight * exploration;
        
        scored.push_back({learning_value, proposal});
    }
    
    // Sort descending by learning value
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    std::vector<ProposedReaction> ranked;
    for (const auto& [score, proposal] : scored) {
        ranked.push_back(proposal);
    }
    
    return ranked;
}

std::vector<std::string> CausalDiscoveryEngine::detect_failure_modes(
    const std::vector<ProposedReaction>& failed_reactions)
{
    std::vector<std::string> patterns;
    
    uint64_t valence_violations = 0;
    uint64_t energy_imbalances = 0;
    uint64_t geometric_failures = 0;
    
    for (const auto& r : failed_reactions) {
        if (!r.valence_satisfied) {
            valence_violations++;
        }
        
        if (r.activation_barrier > 50.0) {
            energy_imbalances++;
        }
        
        if (r.geometric_score < 0.2) {
            geometric_failures++;
        }
    }
    
    uint64_t total = failed_reactions.size();
    
    if (valence_violations > total / 3) {
        patterns.push_back("Frequent valence violations - tighten octet constraints");
    }
    
    if (energy_imbalances > total / 3) {
        patterns.push_back("Energy imbalance - barriers too high, need better estimation");
    }
    
    if (geometric_failures > total / 3) {
        patterns.push_back("Geometric infeasibility - improve orbital overlap checks");
    }
    
    return patterns;
}

std::vector<GraphRewriteRule> CausalDiscoveryEngine::refine_rules(
    const std::vector<GraphRewriteRule>& rules,
    const std::vector<CausalEdge>& causal_graph)
{
    std::vector<GraphRewriteRule> refined;
    
    for (auto rule : rules) {
        // Find causal edges relevant to this rule
        for (const auto& edge : causal_graph) {
            if (edge.strength > 0.5 && edge.confidence > 0.7) {
                // Strong causal factor - tighten constraint
                if (edge.from_factor == "nucleophile_strength") {
                    rule.min_fukui_donor += 0.05;
                } else if (edge.from_factor == "leaving_group_quality") {
                    rule.min_fukui_acceptor += 0.05;
                } else if (edge.from_factor == "geometric_feasibility") {
                    rule.min_geometric_score += 0.05;
                }
            }
        }
        
        refined.push_back(rule);
    }
    
    return refined;
}

std::vector<ProposedReaction> CausalDiscoveryEngine::generate_next_experiments(
    uint32_t num_experiments)
{
    (void)num_experiments;  // TODO: Implement experimental design
    // This would integrate with DiscoveryEngine to propose reactions
    // For now, return placeholder
    return {};
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::map<uint32_t, int> CausalDiscoveryEngine::infer_formal_charges(const State& s) {
    std::map<uint32_t, int> charges;
    
    // Simplified: use partial charges from QEq
    // Full implementation would use valence analysis
    auto elec_props = predict::predict_electronic_properties(s);
    
    for (uint32_t i = 0; i < s.N; ++i) {
        double q = elec_props.partial_charges[i];
        charges[i] = static_cast<int>(std::round(q));
    }
    
    return charges;
}

std::map<uint32_t, int> CausalDiscoveryEngine::compute_coordination(const State& s) {
    std::map<uint32_t, int> coord;
    
    // Count neighbors within bonding distance
    for (uint32_t i = 0; i < s.N; ++i) {
        int count = 0;
        for (uint32_t j = 0; j < s.N; ++j) {
            if (i == j) continue;
            
            double dist = norm(s.X[i] - s.X[j]);
            if (dist < 1.8) {  // Bonding cutoff
                count++;
            }
        }
        coord[i] = count;
    }
    
    return coord;
}

void CausalDiscoveryEngine::identify_roles(
    const ProposedReaction& reaction,
    std::string& donor,
    std::string& acceptor)
{
    // Donor = high f+ (nucleophile)
    // Acceptor = high f- (electrophile)
    
    if (reaction.attacking_site.fukui_plus > reaction.attacking_site.fukui_minus) {
        donor = "nucleophile";
    } else if (reaction.attacking_site.fukui_minus > reaction.attacking_site.fukui_plus) {
        donor = "electrophile";
    } else {
        donor = "radical";
    }
    
    if (reaction.attacked_site.fukui_minus > reaction.attacked_site.fukui_plus) {
        acceptor = "electrophile";
    } else if (reaction.attacked_site.fukui_plus > reaction.attacked_site.fukui_minus) {
        acceptor = "nucleophile";
    } else {
        acceptor = "radical";
    }
}

double CausalDiscoveryEngine::compute_causal_effect(
    const std::string& factor,
    const std::string& outcome,
    const std::vector<ProposedReaction>& data)
{
    (void)outcome;  // TODO: Implement causal effect estimation
    // Simplified do-calculus: compare P(Y|do(X=high)) vs P(Y|do(X=low))
    // Full implementation would use propensity score matching or IV
    
    // Find factor measure function
    std::function<double(const ProposedReaction&)> measure;
    for (const auto& f : causal_factors_) {
        if (f.name == factor) {
            measure = f.measure;
            break;
        }
    }
    
    if (!measure) return 0.0;
    
    // Split data into high/low factor value
    std::vector<size_t> high_indices, low_indices;
    double median = 0.5;  // Simplified threshold
    
    for (size_t i = 0; i < data.size(); ++i) {
        double value = measure(data[i]);
        if (value > median) {
            high_indices.push_back(i);
        } else {
            low_indices.push_back(i);
        }
    }
    
    // Compute success rate in each group
    uint64_t high_success = 0, low_success = 0;
    for (size_t i : high_indices) {
        if (success_flags_[i]) high_success++;
    }
    for (size_t i : low_indices) {
        if (success_flags_[i]) low_success++;
    }
    
    double p_high = high_indices.empty() ? 0.0 :
        static_cast<double>(high_success) / high_indices.size();
    double p_low = low_indices.empty() ? 0.0 :
        static_cast<double>(low_success) / low_indices.size();
    
    return p_high - p_low;  // Average causal effect
}

bool CausalDiscoveryEngine::is_conditionally_independent(
    const std::string& X,
    const std::string& Y,
    const std::vector<std::string>& Z,
    const std::vector<ProposedReaction>& data)
{
    (void)X; (void)Y; (void)Z; (void)data;  // TODO: Implement conditional independence test
    // Placeholder: would use chi-squared test or mutual information
    return false;
}

// ============================================================================
// UTILITIES
// ============================================================================

double compare_signatures(const ElectronFlowSignature& a,
                         const ElectronFlowSignature& b)
{
    return a.similarity(b);
}

void export_causal_graph_dot(const std::vector<CausalEdge>& edges,
                             const std::string& filename)
{
    std::ofstream file(filename);
    if (!file) return;
    
    file << "digraph CausalGraph {\n";
    file << "  rankdir=LR;\n";
    file << "  node [shape=box];\n\n";
    
    for (const auto& edge : edges) {
        file << "  \"" << edge.from_factor << "\" -> \"" << edge.to_outcome << "\"";
        file << " [label=\"" << edge.strength << "\", penwidth=" << (edge.confidence * 5) << "];\n";
    }
    
    file << "}\n";
    
    std::cout << "Causal graph exported to " << filename << "\n";
    std::cout << "Visualize with: dot -Tpng " << filename << " -o causal_graph.png\n";
}

void generate_causal_report(const CausalDiscoveryEngine& engine,
                           const std::string& filename)
{
    std::ofstream file(filename);
    if (!file) return;
    
    file << "# Causal Analysis Report\n\n";
    
    file << "## Discovered Graph Rewrite Rules\n\n";
    for (const auto& rule : engine.get_rules()) {
        file << "### " << rule.name << "\n\n";
        file << "- **Times applied:** " << rule.times_applied << "\n";
        file << "- **Success rate:** " << (rule.success_probability() * 100) << "%\n";
        file << "- **Avg barrier:** " << rule.avg_barrier << " ± " << rule.std_barrier << " kcal/mol\n";
        file << "- **Uncertainty:** " << rule.uncertainty() << " bits\n\n";
    }
    
    file << "## Causal Graph\n\n";
    file << "| Factor | Outcome | Effect Size | Confidence |\n";
    file << "|--------|---------|-------------|------------|\n";
    
    for (const auto& edge : engine.get_causal_graph()) {
        file << "| " << edge.from_factor << " | " << edge.to_outcome << " | ";
        file << edge.strength << " | " << edge.confidence << " |\n";
    }
    
    file << "\n---\n*Generated by CausalDiscoveryEngine*\n";
    
    std::cout << "Causal report saved to " << filename << "\n";
}

} // namespace reaction
} // namespace atomistic
