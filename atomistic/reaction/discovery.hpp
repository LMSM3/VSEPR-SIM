#pragma once
#include "engine.hpp"
#include "../core/state.hpp"
#include <vector>
#include <string>
#include <map>
#include <set>

namespace atomistic {
namespace reaction {

/**
 * Molecular motif: common structural pattern
 * 
 * Examples: "carbonyl", "hydroxyl", "aromatic_ring", "leaving_group"
 */
struct Motif {
    std::string name;
    std::vector<std::string> atom_types;  // e.g., ["C", "O"] for carbonyl
    std::vector<std::pair<uint32_t, uint32_t>> bonds; // Connectivity pattern
    double frequency;  // How often this motif appears in successful reactions
    double success_rate; // Fraction of reactions with this motif that succeed
};

/**
 * Reaction pattern: frequently observed transformation
 * 
 * Example: "carbonyl + nucleophile → tetrahedral intermediate"
 */
struct ReactionPattern {
    std::string name;
    MechanismType mechanism;
    
    std::vector<std::string> reactant_motifs;  // Required patterns in reactants
    std::vector<std::string> product_motifs;   // Expected patterns in products
    
    // Learned constraints from data
    double avg_barrier;
    double std_barrier;
    double avg_exothermicity;
    double success_rate;
    
    uint64_t observation_count;
};

/**
 * Discovery configuration
 */
struct DiscoveryConfig {
    // Molecule generation
    uint32_t min_atoms = 5;
    uint32_t max_atoms = 20;
    uint32_t molecules_per_batch = 100;
    uint32_t max_batches = 10;
    
    // Reaction filtering
    double min_score = 0.5;           // Overall score threshold
    double max_barrier = 30.0;        // kcal/mol
    double min_exothermicity = -50.0; // kcal/mol (very endothermic rejected)
    
    // Pattern mining
    uint32_t min_motif_frequency = 5;  // Motif must appear 5+ times
    double min_pattern_support = 0.1;  // Pattern must cover 10%+ of reactions
    
    // Logging
    std::string output_dir = "discovery_output";
    bool save_successful_reactions = true;
    bool save_failed_reactions = false;
    bool verbose = true;
};

/**
 * Discovery database: accumulates reaction data
 */
class DiscoveryDatabase {
public:
    /**
     * Log a proposed reaction
     */
    void log_reaction(const ProposedReaction& reaction, bool successful);
    
    /**
     * Extract common motifs from logged reactions
     * 
     * Uses subgraph isomorphism to find recurring patterns
     * 
     * @param min_frequency Minimum occurrence count
     * @return List of discovered motifs
     */
    std::vector<Motif> extract_motifs(uint32_t min_frequency = 5);
    
    /**
     * Mine reaction patterns from successful reactions
     * 
     * Clusters reactions by:
     *   - Mechanism type
     *   - Reactant motifs
     *   - Product motifs
     * 
     * @param min_support Minimum fraction of reactions (0-1)
     * @return List of discovered patterns
     */
    std::vector<ReactionPattern> mine_patterns(double min_support = 0.1) const;
    
    /**
     * Generate reaction template from learned pattern
     * 
     * Uses statistics to set constraints:
     *   - max_barrier = avg_barrier + 2·std
     *   - min_fukui = avg_fukui - std
     * 
     * @param pattern Learned reaction pattern
     * @return Optimized reaction template
     */
    ReactionTemplate generate_template_from_pattern(const ReactionPattern& pattern);
    
    /**
     * Get all logged reactions
     */
    const std::vector<ProposedReaction>& get_reactions() const { return reactions_; }
    
    /**
     * Get discovery statistics
     */
    DiscoveryStats get_stats() const;
    
    /**
     * Save database to disk (CSV format)
     */
    void save(const std::string& filename) const;
    
    /**
     * Load database from disk
     */
    void load(const std::string& filename);
    
private:
    std::vector<ProposedReaction> reactions_;
    std::vector<bool> success_flags_;
    
    std::map<std::string, uint64_t> motif_counts_;
    std::map<MechanismType, std::vector<uint64_t>> mechanism_reaction_indices_;
};

/**
 * Reaction discovery engine: systematic exploration of chemical space
 */
class DiscoveryEngine {
public:
    DiscoveryEngine(const DiscoveryConfig& config = DiscoveryConfig());
    
    /**
     * Run discovery loop
     * 
     * Algorithm:
     *   1. Generate batch of random molecules
     *   2. For each molecule pair:
     *      a. Identify reactive sites
     *      b. Try all reaction templates
     *      c. Score and validate proposals
     *      d. Log results
     *   3. Every N batches:
     *      a. Extract motifs
     *      b. Mine patterns
     *      c. Generate new templates
     *      d. Add to reaction engine
     *   4. Repeat until convergence or max batches
     * 
     * @param output_dir Directory for logs and discovered reactions
     * @return Discovery statistics
     */
    DiscoveryStats run_discovery_loop();
    
    /**
     * Generate random molecule for testing
     * 
     * Creates chemically reasonable structures:
     *   - Organic molecules (C, H, O, N)
     *   - Valence constraints satisfied
     *   - No extreme strain
     * 
     * @param num_atoms Number of heavy atoms (H added automatically)
     * @return Random molecular state
     */
    State generate_random_molecule(uint32_t num_atoms);
    
    /**
     * Test all reaction templates on a molecule pair
     * 
     * @param mol_A First reactant
     * @param mol_B Second reactant
     * @return All feasible reactions (scored and filtered)
     */
    std::vector<ProposedReaction> test_all_templates(const State& mol_A, 
                                                     const State& mol_B);
    
    /**
     * Access reaction engine
     */
    ReactionEngine& engine() { return engine_; }
    
    /**
     * Access discovery database
     */
    DiscoveryDatabase& database() { return database_; }
    
    /**
     * Get configuration
     */
    const DiscoveryConfig& config() const { return config_; }
    
private:
    DiscoveryConfig config_;
    ReactionEngine engine_;
    DiscoveryDatabase database_;
    
    // Helper: generate reasonable 3D coordinates
    void generate_3d_structure(State& s);
    
    // Helper: add hydrogens to satisfy valence
    void add_hydrogens(State& s);
    
    // Helper: check if molecule is chemically reasonable
    bool is_reasonable_molecule(const State& s);
};

// ============================================================================
// PATTERN ANALYSIS UTILITIES
// ============================================================================

/**
 * Compute similarity between two motifs
 * 
 * Uses graph edit distance (approximate)
 * 
 * @return Similarity score 0-1 (1 = identical)
 */
double compute_motif_similarity(const Motif& a, const Motif& b);

/**
 * Cluster reactions by similarity
 * 
 * Uses hierarchical clustering with:
 *   - Mechanism type
 *   - Reactant fingerprints
 *   - Activation barrier
 * 
 * @param reactions List of reactions
 * @param similarity_threshold Merge clusters if similarity > threshold
 * @return Cluster assignments (reaction_index → cluster_id)
 */
std::vector<uint32_t> cluster_reactions(
    const std::vector<ProposedReaction>& reactions,
    double similarity_threshold = 0.7
);

/**
 * Generate report summarizing discovered patterns
 * 
 * Markdown format with:
 *   - Top 10 most frequent motifs
 *   - Top 10 most successful reaction patterns
 *   - Mechanism distribution
 *   - Barrier histograms
 * 
 * @param database Discovery database
 * @param filename Output file path
 */
void generate_discovery_report(const DiscoveryDatabase& database,
                               const std::string& filename);

} // namespace reaction
} // namespace atomistic
