#include "discovery.hpp"
#include "pot/periodic_db.hpp"
#include <random>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace atomistic {
namespace reaction {

namespace {
    std::mt19937 rng(std::random_device{}());
}

// ============================================================================
// DISCOVERY DATABASE
// ============================================================================

void DiscoveryDatabase::log_reaction(const ProposedReaction& reaction, bool successful) {
    reactions_.push_back(reaction);
    success_flags_.push_back(successful);
    
    // Update mechanism counts
    auto mech_idx = mechanism_reaction_indices_.find(reaction.mechanism);
    if (mech_idx == mechanism_reaction_indices_.end()) {
        mechanism_reaction_indices_[reaction.mechanism] = {};
    }
    mechanism_reaction_indices_[reaction.mechanism].push_back(reactions_.size() - 1);
}

std::vector<Motif> DiscoveryDatabase::extract_motifs(uint32_t min_frequency) {
    std::vector<Motif> motifs;
    
    // Simplified motif extraction: count common atom pairs
    // Full implementation would use subgraph isomorphism (VF2 algorithm)
    
    std::map<std::string, uint64_t> pair_counts;
    
    for (size_t i = 0; i < reactions_.size(); ++i) {
        if (!success_flags_[i]) continue; // Only successful reactions
        
        const auto& reaction = reactions_[i];
        
        // Extract atom pairs from reactants (placeholder - needs topology)
        // For now, just count element combinations
        std::map<std::string, uint32_t> element_counts;
        
        // This would iterate over bonds and create motif signatures
        // Placeholder: just count elements
        for (uint32_t j = 0; j < reaction.reactant_A.N; ++j) {
            // Would get element from State
            element_counts["C"]++; // Placeholder
        }
    }
    
    // Convert counts to motifs
    for (const auto& [motif_name, count] : motif_counts_) {
        if (count >= min_frequency) {
            Motif m;
            m.name = motif_name;
            m.frequency = static_cast<double>(count);
            m.success_rate = 1.0; // Would compute from success_flags
            motifs.push_back(m);
        }
    }
    
    return motifs;
}

std::vector<ReactionPattern> DiscoveryDatabase::mine_patterns(double min_support) const {
    std::vector<ReactionPattern> patterns;
    
    uint64_t total_reactions = reactions_.size();
    uint64_t min_count = static_cast<uint64_t>(min_support * total_reactions);
    
    // Group reactions by mechanism
    for (const auto& [mechanism, indices] : mechanism_reaction_indices_) {
        if (indices.size() < min_count) continue;
        
        ReactionPattern pattern;
        pattern.mechanism = mechanism;
        pattern.observation_count = indices.size();
        
        // Compute statistics over this mechanism class
        double sum_barrier = 0.0;
        double sum_barrier_sq = 0.0;
        double sum_exotherm = 0.0;
        uint64_t success_count = 0;
        
        for (uint64_t idx : indices) {
            const auto& reaction = reactions_[idx];
            sum_barrier += reaction.activation_barrier;
            sum_barrier_sq += reaction.activation_barrier * reaction.activation_barrier;
            sum_exotherm += reaction.reaction_energy;
            
            if (success_flags_[idx]) success_count++;
        }
        
        double n = static_cast<double>(indices.size());
        pattern.avg_barrier = sum_barrier / n;
        pattern.std_barrier = std::sqrt(sum_barrier_sq / n - pattern.avg_barrier * pattern.avg_barrier);
        pattern.avg_exothermicity = sum_exotherm / n;
        pattern.success_rate = static_cast<double>(success_count) / n;
        
        // Generate name
        switch (mechanism) {
            case MechanismType::SUBSTITUTION:
                pattern.name = "Substitution";
                break;
            case MechanismType::ADDITION:
                pattern.name = "Addition";
                break;
            case MechanismType::ELIMINATION:
                pattern.name = "Elimination";
                break;
            case MechanismType::PERICYCLIC:
                pattern.name = "Pericyclic";
                break;
            case MechanismType::ACID_BASE:
                pattern.name = "Acid-Base";
                break;
            default:
                pattern.name = "Unknown";
        }
        
        patterns.push_back(pattern);
    }
    
    return patterns;
}

ReactionTemplate DiscoveryDatabase::generate_template_from_pattern(const ReactionPattern& pattern) {
    ReactionTemplate tmpl;
    tmpl.mechanism = pattern.mechanism;
    tmpl.name = pattern.name + " (learned)";
    tmpl.description = "Auto-generated from " + std::to_string(pattern.observation_count) + " observations";
    
    // Set constraints from statistics
    tmpl.max_barrier = pattern.avg_barrier + 2.0 * pattern.std_barrier; // 95% confidence
    tmpl.min_exotherm = pattern.avg_exothermicity - 2.0 * 5.0; // Conservative
    
    // Default Fukui thresholds (would learn from data)
    tmpl.min_fukui_electrophile = 0.25;
    tmpl.min_fukui_nucleophile = 0.25;
    tmpl.min_fukui_radical = 0.0;
    
    // Default geometric constraints
    tmpl.min_distance = 1.5;
    tmpl.max_distance = 3.5;
    tmpl.min_angle = 0.0;
    tmpl.max_angle = 180.0;
    
    tmpl.require_hardness_match = false;
    tmpl.hardness_tolerance = 10.0;
    
    tmpl.conserve_valence = true;
    tmpl.allow_radicals = false;
    tmpl.require_octet = true;
    
    return tmpl;
}

DiscoveryStats DiscoveryDatabase::get_stats() const {
    DiscoveryStats stats;
    stats.reactions_proposed = reactions_.size();
    
    double sum_barrier = 0.0;
    double sum_exotherm = 0.0;
    stats.best_score = 0.0;
    
    for (size_t i = 0; i < reactions_.size(); ++i) {
        const auto& r = reactions_[i];
        
        if (success_flags_[i]) {
            stats.reactions_validated++;
            
            if (r.thermodynamically_feasible) {
                stats.reactions_feasible++;
            }
        }
        
        stats.mechanism_counts[r.mechanism]++;
        
        sum_barrier += r.activation_barrier;
        sum_exotherm += r.reaction_energy;
        
        if (r.overall_score > stats.best_score) {
            stats.best_score = r.overall_score;
        }
    }
    
    if (reactions_.size() > 0) {
        stats.avg_barrier = sum_barrier / reactions_.size();
        stats.avg_exothermicity = sum_exotherm / reactions_.size();
    }
    
    return stats;
}

void DiscoveryDatabase::save(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file) {
        std::cerr << "Failed to open " << filename << " for writing\n";
        return;
    }
    
    // CSV header
    file << "reaction_id,mechanism,barrier_kcal_mol,delta_e_kcal_mol,rate_s,";
    file << "reactivity_score,geometric_score,thermodynamic_score,overall_score,";
    file << "successful,mass_balanced,charge_balanced\n";
    
    for (size_t i = 0; i < reactions_.size(); ++i) {
        const auto& r = reactions_[i];
        
        file << i << ",";
        file << static_cast<int>(r.mechanism) << ",";
        file << r.activation_barrier << ",";
        file << r.reaction_energy << ",";
        file << r.rate_constant << ",";
        file << r.reactivity_score << ",";
        file << r.geometric_score << ",";
        file << r.thermodynamic_score << ",";
        file << r.overall_score << ",";
        file << (success_flags_[i] ? 1 : 0) << ",";
        file << (r.mass_balanced ? 1 : 0) << ",";
        file << (r.charge_balanced ? 1 : 0) << "\n";
    }
    
    std::cout << "Saved " << reactions_.size() << " reactions to " << filename << "\n";
}

void DiscoveryDatabase::load(const std::string& filename) {
    // Placeholder: would parse CSV
    std::cout << "Loading database from " << filename << " (not implemented)\n";
}

// ============================================================================
// DISCOVERY ENGINE
// ============================================================================

DiscoveryEngine::DiscoveryEngine(const DiscoveryConfig& config)
    : config_(config) {
}

DiscoveryStats DiscoveryEngine::run_discovery_loop() {
    std::cout << "═══ REACTION DISCOVERY LOOP ═══\n\n";
    std::cout << "Configuration:\n";
    std::cout << "  Molecules per batch: " << config_.molecules_per_batch << "\n";
    std::cout << "  Max batches: " << config_.max_batches << "\n";
    std::cout << "  Min score threshold: " << config_.min_score << "\n";
    std::cout << "  Max barrier: " << config_.max_barrier << " kcal/mol\n\n";
    
    // Create output directory
    std::filesystem::create_directories(config_.output_dir);
    
    for (uint32_t batch = 0; batch < config_.max_batches; ++batch) {
        std::cout << "--- Batch " << (batch + 1) << "/" << config_.max_batches << " ---\n";
        
        // Generate random molecules
        std::vector<State> molecules;
        molecules.reserve(config_.molecules_per_batch);
        
        for (uint32_t i = 0; i < config_.molecules_per_batch; ++i) {
            std::uniform_int_distribution<uint32_t> atom_dist(config_.min_atoms, config_.max_atoms);
            uint32_t num_atoms = atom_dist(rng);
            
            State mol = generate_random_molecule(num_atoms);
            if (is_reasonable_molecule(mol)) {
                molecules.push_back(mol);
            }
        }
        
        std::cout << "  Generated " << molecules.size() << " molecules\n";
        
        // Test all pairs
        uint64_t reactions_tested = 0;
        uint64_t reactions_accepted = 0;
        
        for (size_t i = 0; i < molecules.size(); ++i) {
            for (size_t j = i + 1; j < molecules.size(); ++j) {
                auto proposals = test_all_templates(molecules[i], molecules[j]);
                reactions_tested += proposals.size();
                
                for (const auto& proposal : proposals) {
                    bool accept = (proposal.overall_score >= config_.min_score &&
                                  proposal.activation_barrier <= config_.max_barrier &&
                                  proposal.reaction_energy >= config_.min_exothermicity);
                    
                    database_.log_reaction(proposal, accept);
                    
                    if (accept) {
                        reactions_accepted++;
                        
                        if (config_.save_successful_reactions && config_.verbose) {
                            std::cout << "    ✓ " << proposal.description 
                                     << " (score=" << proposal.overall_score 
                                     << ", Ea=" << proposal.activation_barrier << " kcal/mol)\n";
                        }
                    }
                }
            }
        }
        
        std::cout << "  Tested " << reactions_tested << " reactions, accepted " 
                  << reactions_accepted << "\n\n";
        
        // Every 3 batches: mine patterns and update templates
        if ((batch + 1) % 3 == 0) {
            std::cout << "  Mining patterns...\n";
            
            auto patterns = database_.mine_patterns(config_.min_pattern_support);
            std::cout << "  Discovered " << patterns.size() << " patterns\n";
            
            // Generate new templates from patterns
            for (const auto& pattern : patterns) {
                if (pattern.success_rate > 0.5) { // Only high-success patterns
                    auto tmpl = database_.generate_template_from_pattern(pattern);
                    engine_.add_template(tmpl);
                    
                    if (config_.verbose) {
                        std::cout << "    + Added template: " << tmpl.name 
                                 << " (success rate: " << pattern.success_rate << ")\n";
                    }
                }
            }
            
            std::cout << "\n";
        }
    }
    
    // Final statistics
    auto stats = database_.get_stats();
    
    std::cout << "═══ DISCOVERY COMPLETE ═══\n\n";
    std::cout << "Total reactions proposed: " << stats.reactions_proposed << "\n";
    std::cout << "Validated: " << stats.reactions_validated << "\n";
    std::cout << "Feasible: " << stats.reactions_feasible << "\n";
    std::cout << "Average barrier: " << stats.avg_barrier << " kcal/mol\n";
    std::cout << "Average exothermicity: " << stats.avg_exothermicity << " kcal/mol\n";
    std::cout << "Best score: " << stats.best_score << "\n\n";
    
    // Save database
    std::string db_file = config_.output_dir + "/reactions.csv";
    database_.save(db_file);
    
    // Generate report
    std::string report_file = config_.output_dir + "/discovery_report.md";
    generate_discovery_report(database_, report_file);
    
    return stats;
}

State DiscoveryEngine::generate_random_molecule(uint32_t num_atoms) {
    State s;
    s.N = num_atoms;
    
    // Generate random connectivity (simple linear chain for now)
    // Full implementation would use graph generation algorithms
    
    s.X.resize(num_atoms);
    s.V.resize(num_atoms, Vec3{0, 0, 0});
    s.M.resize(num_atoms);
    
    // Place atoms randomly in a box
    std::uniform_real_distribution<double> pos_dist(-5.0, 5.0);
    std::uniform_int_distribution<int> elem_dist(0, 3);
    
    for (uint32_t i = 0; i < num_atoms; ++i) {
        s.X[i] = Vec3{pos_dist(rng), pos_dist(rng), pos_dist(rng)};
        
        // Random element (C, N, O, S)
        int elem = elem_dist(rng);
        switch (elem) {
            case 0: s.M[i] = 12.01; break; // C
            case 1: s.M[i] = 14.01; break; // N
            case 2: s.M[i] = 16.00; break; // O
            case 3: s.M[i] = 32.07; break; // S
        }
    }
    
    // Generate 3D structure with reasonable geometry
    generate_3d_structure(s);
    
    // Add hydrogens
    add_hydrogens(s);
    
    return s;
}

std::vector<ProposedReaction> DiscoveryEngine::test_all_templates(
    const State& mol_A, 
    const State& mol_B)
{
    std::vector<ProposedReaction> all_proposals;
    
    // Identify reactive sites
    auto sites_A = engine_.identify_reactive_sites(mol_A);
    auto sites_B = engine_.identify_reactive_sites(mol_B);
    
    // Try each template
    for (const auto& tmpl : engine_.get_templates()) {
        auto proposals = engine_.match_reactive_sites(mol_A, mol_B, sites_A, sites_B, tmpl);
        
        all_proposals.insert(all_proposals.end(), proposals.begin(), proposals.end());
    }
    
    return all_proposals;
}

void DiscoveryEngine::generate_3d_structure(State& s) {
    // Simplified: use distance geometry
    // Full implementation would use ETKDG or similar
    
    for (uint32_t i = 0; i < s.N; ++i) {
        for (uint32_t j = i + 1; j < s.N; ++j) {
            Vec3 diff = s.X[j] - s.X[i];
            double dist = norm(diff);
            
            // Enforce reasonable bond lengths (1-2 Å)
            if (dist < 1.0) {
                s.X[j] = s.X[i] + diff * (1.5 / dist);
            } else if (dist > 3.0) {
                s.X[j] = s.X[i] + diff * (2.0 / dist);
            }
        }
    }
}

void DiscoveryEngine::add_hydrogens(State& s) {
(void)s;  // TODO: Implement hydrogen addition
// Placeholder: would add H atoms to satisfy valence
    // Full implementation would use valence rules from periodic table
}

bool DiscoveryEngine::is_reasonable_molecule(const State& s) {
// Check for reasonable geometry
for (uint32_t i = 0; i < s.N; ++i) {
    for (uint32_t j = i + 1; j < s.N; ++j) {
        double dist = norm(s.X[i] - s.X[j]);
            
        // No atom collisions
            if (dist < 0.5) return false;
            
            // No extremely long bonds
            if (dist > 10.0) return false;
        }
    }
    
    return true;
}

// ============================================================================
// PATTERN ANALYSIS UTILITIES
// ============================================================================

double compute_motif_similarity(const Motif& a, const Motif& b) {
    // Simplified: Jaccard similarity on atom types
    std::set<std::string> atoms_a(a.atom_types.begin(), a.atom_types.end());
    std::set<std::string> atoms_b(b.atom_types.begin(), b.atom_types.end());
    
    std::vector<std::string> intersection;
    std::set_intersection(atoms_a.begin(), atoms_a.end(),
                         atoms_b.begin(), atoms_b.end(),
                         std::back_inserter(intersection));
    
    std::vector<std::string> union_set;
    std::set_union(atoms_a.begin(), atoms_a.end(),
                  atoms_b.begin(), atoms_b.end(),
                  std::back_inserter(union_set));
    
    if (union_set.empty()) return 0.0;
    
    return static_cast<double>(intersection.size()) / union_set.size();
}

std::vector<uint32_t> cluster_reactions(
    const std::vector<ProposedReaction>& reactions,
    double similarity_threshold)
{
    // Simplified hierarchical clustering
    std::vector<uint32_t> clusters(reactions.size());
    
    // Initially each reaction is its own cluster
    for (size_t i = 0; i < reactions.size(); ++i) {
        clusters[i] = i;
    }
    
    // Merge similar reactions
    for (size_t i = 0; i < reactions.size(); ++i) {
        for (size_t j = i + 1; j < reactions.size(); ++j) {
            // Similarity: same mechanism + similar barrier
            bool same_mech = (reactions[i].mechanism == reactions[j].mechanism);
            double barrier_diff = std::abs(reactions[i].activation_barrier - 
                                          reactions[j].activation_barrier);
            
            double similarity = same_mech ? (1.0 - barrier_diff / 30.0) : 0.0;
            similarity = std::max(0.0, similarity);
            
            if (similarity > similarity_threshold) {
                // Merge clusters
                uint32_t cluster_i = clusters[i];
                uint32_t cluster_j = clusters[j];
                
                for (size_t k = 0; k < clusters.size(); ++k) {
                    if (clusters[k] == cluster_j) {
                        clusters[k] = cluster_i;
                    }
                }
            }
        }
    }
    
    return clusters;
}

void generate_discovery_report(const DiscoveryDatabase& database,
                               const std::string& filename)
{
    std::ofstream file(filename);
    if (!file) {
        std::cerr << "Failed to write report to " << filename << "\n";
        return;
    }
    
    auto stats = database.get_stats();
    auto patterns = database.mine_patterns(0.05);
    
    file << "# Reaction Discovery Report\n\n";
    
    file << "## Summary Statistics\n\n";
    file << "- **Total reactions proposed:** " << stats.reactions_proposed << "\n";
    file << "- **Validated:** " << stats.reactions_validated << "\n";
    file << "- **Feasible:** " << stats.reactions_feasible << "\n";
    file << "- **Success rate:** " << (100.0 * stats.reactions_feasible / stats.reactions_proposed) << "%\n";
    file << "- **Average barrier:** " << stats.avg_barrier << " kcal/mol\n";
    file << "- **Average exothermicity:** " << stats.avg_exothermicity << " kcal/mol\n";
    file << "- **Best overall score:** " << stats.best_score << "\n\n";
    
    file << "## Mechanism Distribution\n\n";
    file << "| Mechanism | Count |\n";
    file << "|-----------|-------|\n";
    for (const auto& [mech, count] : stats.mechanism_counts) {
        file << "| " << static_cast<int>(mech) << " | " << count << " |\n";
    }
    file << "\n";
    
    file << "## Discovered Patterns\n\n";
    for (const auto& pattern : patterns) {
        file << "### " << pattern.name << "\n\n";
        file << "- **Observations:** " << pattern.observation_count << "\n";
        file << "- **Success rate:** " << (pattern.success_rate * 100) << "%\n";
        file << "- **Avg barrier:** " << pattern.avg_barrier << " ± " << pattern.std_barrier << " kcal/mol\n";
        file << "- **Avg exothermicity:** " << pattern.avg_exothermicity << " kcal/mol\n\n";
    }
    
    file << "## Top Reactions\n\n";
    auto reactions = database.get_reactions();
    
    // Sort by score
    std::vector<size_t> indices(reactions.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(),
              [&reactions](size_t a, size_t b) {
                  return reactions[a].overall_score > reactions[b].overall_score;
              });
    
    file << "| Rank | Mechanism | Barrier (kcal/mol) | ΔE (kcal/mol) | Score |\n";
    file << "|------|-----------|-------------------|---------------|-------|\n";
    
    for (size_t i = 0; i < std::min(size_t(20), reactions.size()); ++i) {
        const auto& r = reactions[indices[i]];
        file << "| " << (i + 1) << " | ";
        file << static_cast<int>(r.mechanism) << " | ";
        file << r.activation_barrier << " | ";
        file << r.reaction_energy << " | ";
        file << r.overall_score << " |\n";
    }
    
    file << "\n---\n\n";
    file << "*Report generated by meso-discover*\n";
    
    std::cout << "Discovery report saved to " << filename << "\n";
}

} // namespace reaction
} // namespace atomistic
