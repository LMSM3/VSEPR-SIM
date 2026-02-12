#pragma once
#include "../core/state.hpp"
#include "../core/alignment.hpp"
#include "engine.hpp"
#include "discovery.hpp"
#include <functional>
#include <optional>

namespace atomistic {
namespace reaction {

/**
 * Discovery visualization callback interface
 * 
 * Enables live visualization of reaction discovery with automatic
 * camera tracking and structure alignment.
 * 
 * Integration:
 * - Discovery engine calls these hooks during exploration
 * - Alignment system auto-frames reactants + products
 * - Renderer displays both structures with smooth transitions
 * - UI shows real-time statistics and mechanism info
 */
struct DiscoveryVizCallbacks {
    /**
     * Called when a new reaction is proposed
     * 
     * @param reaction Proposed reaction (before validation)
     */
    std::function<void(const ProposedReaction&)> on_reaction_proposed;
    
    /**
     * Called when a reaction is validated (passes all checks)
     * 
     * @param reaction Validated reaction
     * @param is_feasible True if thermodynamically/kinetically feasible
     */
    std::function<void(const ProposedReaction&, bool)> on_reaction_validated;
    
    /**
     * Called during alignment of products onto reactants
     * 
     * Provides real-time updates during animated alignment:
     * - progress: 0.0 to 1.0
     * - rmsd: Current RMSD during rotation
     * - reactants: Reference structure (fixed)
     * - products: Target structure (being rotated)
     * - camera: Auto-computed camera parameters
     * 
     * This is the key integration point for live visualization!
     * 
     * @param progress Animation progress [0, 1]
     * @param rmsd Current RMSD (Angstroms)
     * @param reactants Combined reactant state
     * @param products Combined product state (current orientation)
     * @param camera Auto-framed camera parameters
     */
    std::function<void(
        double progress,
        double rmsd,
        const State& reactants,
        const State& products,
        const AlignmentCamera& camera
    )> on_alignment_update;
    
    /**
     * Called when alignment completes
     * 
     * @param reaction The reaction
     * @param result Alignment result (final RMSD, rotation matrix, etc.)
     */
    std::function<void(
        const ProposedReaction&,
        const AlignmentResult&
    )> on_alignment_complete;
    
    /**
     * Called when pattern mining discovers a new motif
     * 
     * @param motif_name Name of discovered pattern
     * @param frequency How many times it appears
     * @param success_rate Success rate for reactions with this motif
     */
    std::function<void(
        const std::string& motif_name,
        uint64_t frequency,
        double success_rate
    )> on_motif_discovered;
    
    /**
     * Called when discovery loop completes a batch
     * 
     * @param batch_num Current batch number
     * @param stats Cumulative discovery statistics
     */
    std::function<void(
        uint32_t batch_num,
        const DiscoveryStats& stats
    )> on_batch_complete;
};

/**
 * Helper: Combine reactants into single State for visualization
 * 
 * @param mol_A First reactant
 * @param mol_B Second reactant (can be empty)
 * @param separation Distance between COMs (Angstroms)
 * @return Combined state with both molecules
 */
State combine_molecules(
    const State& mol_A,
    const State& mol_B,
    double separation = 5.0
);

/**
 * Helper: Align products onto reactants with visualization callbacks
 * 
 * This is the main integration function that ties together:
 * - Kabsch alignment
 * - Camera tracking
 * - Live visualization
 * 
 * @param reactants Combined reactant state
 * @param products Combined product state (modified in-place)
 * @param n_steps Number of animation steps
 * @param callback Visualization callback (receives camera + RMSD updates)
 * @return Alignment result
 */
AlignmentResult align_reaction_with_viz(
    const State& reactants,
    State& products,
    int n_steps,
    const std::function<void(double, double, const State&, const AlignmentCamera&)>& callback
);

/**
 * Enhanced discovery engine with visualization support
 * 
 * Extends DiscoveryEngine to call visualization callbacks during:
 * - Reaction proposal
 * - Validation
 * - Alignment
 * - Pattern discovery
 * 
 * Usage:
 *   DiscoveryEngineWithViz engine(config);
 *   engine.set_viz_callbacks(callbacks);
 *   engine.run_discovery_loop();  // Calls callbacks at each step
 */
class DiscoveryEngineWithViz {
public:
    DiscoveryEngineWithViz(const DiscoveryConfig& config = DiscoveryConfig())
        : config_(config), callbacks_() {}
    
    /**
     * Set visualization callbacks
     */
    void set_viz_callbacks(const DiscoveryVizCallbacks& callbacks) {
        callbacks_ = callbacks;
    }
    
    /**
     * Run discovery loop with live visualization
     * 
     * Same as DiscoveryEngine::run_discovery_loop() but calls
     * visualization callbacks at each step for real-time updates
     * 
     * @return Discovery statistics
     */
    DiscoveryStats run_discovery_loop_with_viz();
    
    /**
     * Test reaction with alignment visualization
     * 
     * Proposes reaction, validates, aligns, and visualizes
     * 
     * @param mol_A First reactant
     * @param mol_B Second reactant
     * @param template_rule Reaction template to apply
     * @param n_alignment_steps Animation steps for alignment
     * @return Validated and aligned reaction
     */
    std::optional<ProposedReaction> test_reaction_with_viz(
        const State& mol_A,
        const State& mol_B,
        const ReactionTemplate& template_rule,
        int n_alignment_steps = 60
    );

private:
    DiscoveryConfig config_;
    DiscoveryVizCallbacks callbacks_;
    
    // Helper: Trigger callback if set
    template<typename Func, typename... Args>
    void trigger_callback(const Func& func, Args&&... args) {
        if (func) {
            func(std::forward<Args>(args)...);
        }
    }
};

} // namespace reaction
} // namespace atomistic
