#include "discovery_viz.hpp"
#include "../core/alignment.hpp"
#include "discovery.hpp"
#include <cmath>
#include <iostream>

namespace atomistic {
namespace reaction {

State combine_molecules(
    const State& mol_A,
    const State& mol_B,
    double separation)
{
    State combined;
    
    // Compute COMs
    Vec3 com_A = compute_com(mol_A);
    Vec3 com_B = {0, 0, 0};
    if (mol_B.N > 0) {
        com_B = compute_com(mol_B);
    }
    
    // Determine combined size
    uint32_t total_N = mol_A.N;
    if (mol_B.N > 0) {
        total_N += mol_B.N;
    }
    
    combined.N = total_N;
    combined.X.reserve(total_N);
    combined.V.reserve(total_N);
    combined.Q.reserve(total_N);
    combined.M.reserve(total_N);
    combined.type.reserve(total_N);
    combined.F.reserve(total_N);
    
    // Add molecule A (centered at -separation/2 along x-axis)
    Vec3 offset_A = {-separation / 2.0, 0, 0};
    for (uint32_t i = 0; i < mol_A.N; ++i) {
        combined.X.push_back((mol_A.X[i] - com_A) + offset_A);
        combined.V.push_back(mol_A.V.size() > i ? mol_A.V[i] : Vec3{0, 0, 0});
        combined.Q.push_back(mol_A.Q.size() > i ? mol_A.Q[i] : 0.0);
        combined.M.push_back(mol_A.M.size() > i ? mol_A.M[i] : 1.0);
        combined.type.push_back(mol_A.type.size() > i ? mol_A.type[i] : 0);
        combined.F.push_back({0, 0, 0});
    }
    
    // Add molecule B (centered at +separation/2 along x-axis) if present
    if (mol_B.N > 0) {
        Vec3 offset_B = {separation / 2.0, 0, 0};
        for (uint32_t i = 0; i < mol_B.N; ++i) {
            combined.X.push_back((mol_B.X[i] - com_B) + offset_B);
            combined.V.push_back(mol_B.V.size() > i ? mol_B.V[i] : Vec3{0, 0, 0});
            combined.Q.push_back(mol_B.Q.size() > i ? mol_B.Q[i] : 0.0);
            combined.M.push_back(mol_B.M.size() > i ? mol_B.M[i] : 1.0);
            combined.type.push_back(mol_B.type.size() > i ? mol_B.type[i] : 0);
            combined.F.push_back({0, 0, 0});
        }
    }
    
    return combined;
}

AlignmentResult align_reaction_with_viz(
    const State& reactants,
    State& products,
    int n_steps,
    const std::function<void(double, double, const State&, const AlignmentCamera&)>& callback)
{
    // Compute initial alignment parameters
    AlignmentResult result;
    result.rmsd_before = compute_rmsd(products, reactants);
    result.reference_com = compute_com(reactants);
    result.target_com_before = compute_com(products);
    
    // Perform Kabsch alignment (gets optimal rotation)
    State products_copy = products;  // Work on copy for intermediate steps
    AlignmentResult final_result = kabsch_align(products, reactants);
    
    // Restore products to initial state for animation
    products = products_copy;
    
    // Animate alignment with camera tracking
    auto animation_callback = [&](double progress, double rmsd, const State& current) {
        if (callback) {
            // Compute camera for current state
            AlignmentResult temp_result = final_result;
            temp_result.target_com_after = compute_com(current);
            AlignmentCamera camera = compute_alignment_camera(reactants, current, temp_result);
            
            callback(progress, rmsd, current, camera);
        }
    };
    
    // Run animated alignment
    AlignmentResult animated_result = animated_align(products, reactants, n_steps, animation_callback);
    
    return animated_result;
}

DiscoveryStats DiscoveryEngineWithViz::run_discovery_loop_with_viz() {
DiscoveryStats stats{};
    
// Initialize discovery engine and database
DiscoveryEngine engine(this->config_);
DiscoveryDatabase database;
    
    // Main discovery loop
    for (uint32_t batch = 0; batch < config_.max_batches; ++batch) {
        uint32_t batch_reactions = 0;
        uint32_t batch_feasible = 0;
        
        // Generate molecule batch
        for (uint32_t mol_idx = 0; mol_idx < config_.molecules_per_batch; ++mol_idx) {
            // Generate two random molecules
            State mol_A = engine.generate_random_molecule(
                config_.min_atoms + (rand() % (config_.max_atoms - config_.min_atoms + 1))
            );
            State mol_B = engine.generate_random_molecule(
                config_.min_atoms + (rand() % (config_.max_atoms - config_.min_atoms + 1))
            );
            
            // Test all reaction templates
            auto reactions = engine.test_all_templates(mol_A, mol_B);
            
            for (auto& reaction : reactions) {
                stats.reactions_proposed++;
                batch_reactions++;
                
                // Callback: reaction proposed
                trigger_callback(callbacks_.on_reaction_proposed, reaction);
                
                // Validate reaction
                bool valid = engine.engine().validate_reaction(reaction);
                if (!valid) continue;
                
                stats.reactions_validated++;
                
                // Check feasibility
                bool feasible = reaction.overall_score >= config_.min_score;
                
                // Callback: reaction validated
                trigger_callback(callbacks_.on_reaction_validated, reaction, feasible);
                
                if (!feasible) continue;
                
                stats.reactions_feasible++;
                batch_feasible++;
                
                // Combine reactants and products for alignment visualization
                State reactants = combine_molecules(reaction.reactant_A, reaction.reactant_B);
                State products = combine_molecules(reaction.product_C, reaction.product_D);
                
                // Align products onto reactants with visualization
                auto align_callback = [&](double progress, double rmsd, 
                                          const State& current, const AlignmentCamera& camera) {
                    trigger_callback(callbacks_.on_alignment_update, 
                                   progress, rmsd, reactants, current, camera);
                };
                
                AlignmentResult align_result = align_reaction_with_viz(
                    reactants, products, 60, align_callback
                );
                
                // Callback: alignment complete
                trigger_callback(callbacks_.on_alignment_complete, reaction, align_result);
                
                // Log to database
                database.log_reaction(reaction, feasible);
                
                // Update statistics
                stats.mechanism_counts[reaction.mechanism]++;
                if (reaction.activation_barrier < stats.avg_barrier || stats.avg_barrier == 0) {
                    stats.avg_barrier = reaction.activation_barrier;
                }
                if (reaction.reaction_energy < stats.avg_exothermicity || stats.avg_exothermicity == 0) {
                    stats.avg_exothermicity = reaction.reaction_energy;
                }
                if (reaction.overall_score > stats.best_score) {
                    stats.best_score = reaction.overall_score;
                }
            }
        }
        
        // Pattern mining every batch
        auto motifs = database.extract_motifs(config_.min_motif_frequency);
        for (const auto& motif : motifs) {
            trigger_callback(callbacks_.on_motif_discovered,
                           motif.name, static_cast<uint64_t>(motif.frequency), motif.success_rate);
        }
        
        // Callback: batch complete
        trigger_callback(callbacks_.on_batch_complete, batch, stats);
        
        if (config_.verbose) {
            std::cout << "Batch " << batch << ": "
                      << batch_reactions << " reactions proposed, "
                      << batch_feasible << " feasible\n";
        }
    }
    
    return stats;
}

std::optional<ProposedReaction> DiscoveryEngineWithViz::test_reaction_with_viz(
    const State& mol_A,
    const State& mol_B,
    const ReactionTemplate& template_rule,
    int n_alignment_steps)
{
    // Initialize engine
    DiscoveryEngine engine(this->config_);
    
    // Identify reactive sites
    auto sites_A = engine.engine().identify_reactive_sites(mol_A);
    auto sites_B = engine.engine().identify_reactive_sites(mol_B);
    
    // Match sites and propose reactions
    auto reactions = engine.engine().match_reactive_sites(
        mol_A, mol_B, sites_A, sites_B, template_rule
    );
    
    if (reactions.empty()) {
        return std::nullopt;
    }
    
    // Take best reaction
    ProposedReaction reaction = reactions[0];
    
    // Callback: proposed
    trigger_callback(callbacks_.on_reaction_proposed, reaction);
    
    // Validate
    bool valid = engine.engine().validate_reaction(reaction);
    if (!valid) {
        trigger_callback(callbacks_.on_reaction_validated, reaction, false);
        return std::nullopt;
    }
    
    bool feasible = reaction.overall_score >= config_.min_score;
    trigger_callback(callbacks_.on_reaction_validated, reaction, feasible);
    
    if (!feasible) {
        return std::nullopt;
    }
    
    // Align with visualization
    State reactants = combine_molecules(reaction.reactant_A, reaction.reactant_B);
    State products = combine_molecules(reaction.product_C, reaction.product_D);
    
    auto align_callback = [&](double progress, double rmsd,
                              const State& current, const AlignmentCamera& camera) {
        trigger_callback(callbacks_.on_alignment_update,
                       progress, rmsd, reactants, current, camera);
    };
    
    AlignmentResult align_result = align_reaction_with_viz(
        reactants, products, n_alignment_steps, align_callback
    );
    
    trigger_callback(callbacks_.on_alignment_complete, reaction, align_result);
    
    return reaction;
}

} // namespace reaction
} // namespace atomistic
