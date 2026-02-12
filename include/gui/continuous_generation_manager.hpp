/**
 * Continuous Molecule Generation Tab Integration
 * vsepr-sim v2.3.1 - Phase 3
 * 
 * Features:
 * - Real-time molecule generation with streaming visualization
 * - Live statistics dashboard (rate, unique formulas, etc.)
 * - Thumbnail gallery with click-to-load
 * - Checkpoint system for long runs
 * - Category filtering (no hardcoded formulas!)
 * 
 * Physics:
 * - Uses real molecular database (50+ molecules)
 * - Proper formation energies from thermodynamic data
 * - Parametric generators based on chemical rules
 * 
 * Author: VSEPR-Sim Development Team
 * Date: January 2025
 * License: MIT
 */

#pragma once

#include "dynamic/real_molecule_generator.hpp"
#include "sim/molecule.hpp"
#include <functional>
#include <atomic>
#include <mutex>
#include <deque>
#include <string>
#include <vector>

namespace vsepr {
namespace gui {

// ============================================================================
// Continuous Generation UI State
// ============================================================================

struct ContinuousGenerationState {
    // Generation parameters
    int target_count;          // 0 = infinite
    int checkpoint_interval;   // Save every N molecules
    dynamic::MoleculeCategory category;  // Which category to generate from
    bool use_gpu;              // GPU acceleration (if available)
    std::string output_path;   // Where to save molecules
    
    // Display settings
    int gallery_columns;       // Thumbnails per row (default: 5)
    bool show_statistics;      // Show stats panel
    bool show_gallery;         // Show thumbnail gallery
    bool auto_load_latest;     // Auto-load newest molecule to main viewer
    
    // UI state
    bool is_running;
    bool is_paused;
    size_t selected_index;     // Currently selected thumbnail (-1 = none)
    
    ContinuousGenerationState()
        : target_count(0)
        , checkpoint_interval(100)
        , category(dynamic::MoleculeCategory::All)
        , use_gpu(false)
        , output_path("output/continuous/molecules.xyz")
        , gallery_columns(5)
        , show_statistics(true)
        , show_gallery(true)
        , auto_load_latest(false)
        , is_running(false)
        , is_paused(false)
        , selected_index(static_cast<size_t>(-1)) {}
};

// ============================================================================
// Continuous Generation Manager (GUI Integration)
// ============================================================================

/**
 * Manages continuous molecule generation for GUI integration
 * Wraps ContinuousRealMoleculeGenerator with UI-specific functionality
 */
class ContinuousGenerationManager {
public:
    using ThumbnailCallback = std::function<void(const Molecule&, void* texture_id)>;
    using SelectionCallback = std::function<void(const Molecule&)>;
    
    ContinuousGenerationManager();
    ~ContinuousGenerationManager();
    
    // ========================================================================
    // Control Methods
    // ========================================================================
    
    /**
     * Start continuous generation
     * @param state UI state with parameters
     */
    void start(const ContinuousGenerationState& state);
    
    /**
     * Stop and save checkpoint
     */
    void stop();
    
    /**
     * Pause generation
     */
    void pause();
    
    /**
     * Resume paused generation
     */
    void resume();
    
    /**
     * Check if currently generating
     */
    bool is_running() const;
    
    /**
     * Check if paused
     */
    bool is_paused() const;
    
    // ========================================================================
    // Data Access (Thread-Safe)
    // ========================================================================
    
    /**
     * Get current generation statistics
     */
    dynamic::GenerationStatistics get_statistics() const;
    
    /**
     * Get recent molecules for gallery
     * @param count Number of recent molecules (default: 50)
     */
    std::vector<Molecule> get_recent_molecules(size_t count = 50) const;
    
    /**
     * Get specific molecule by index in recent buffer
     * @param index Index in recent buffer (0 = oldest in buffer)
     */
    Molecule get_molecule(size_t index) const;
    
    /**
     * Get most recent molecule
     */
    Molecule get_latest_molecule() const;
    
    /**
     * Get number of molecules in recent buffer
     */
    size_t get_buffer_size() const;
    
    /**
     * Check if GPU acceleration is available
     */
    bool is_gpu_available() const;
    
    // ========================================================================
    // Callbacks
    // ========================================================================
    
    /**
     * Set callback for when new molecule is generated
     * Useful for auto-loading to main viewer
     */
    void set_new_molecule_callback(SelectionCallback callback);
    
    /**
     * Set callback for checkpoint events
     */
    void set_checkpoint_callback(std::function<void(const dynamic::GenerationStatistics&)> callback);
    
    // ========================================================================
    // Export Methods
    // ========================================================================
    
    /**
     * Export all molecules in buffer to multi-frame XYZ
     * @param path Output file path
     */
    void export_buffer_xyz(const std::string& path) const;
    
    /**
     * Export statistics to CSV
     * @param path Output file path
     */
    void export_statistics_csv(const std::string& path) const;
    
private:
    std::unique_ptr<dynamic::ContinuousRealMoleculeGenerator> generator_;
    SelectionCallback new_molecule_callback_;
    ContinuousGenerationState current_state_;
    
    mutable std::mutex state_mutex_;
};

// ============================================================================
// ImGui Rendering Helpers
// ============================================================================

/**
 * Render continuous generation control panel
 * @param state Current UI state
 * @param manager Generation manager
 * @return true if state changed (need to update)
 */
bool render_continuous_controls(ContinuousGenerationState& state, 
                                ContinuousGenerationManager& manager);

/**
 * Render statistics dashboard
 * @param stats Current statistics
 */
void render_statistics_panel(const dynamic::GenerationStatistics& stats);

/**
 * Render thumbnail gallery
 * @param molecules Recent molecules to display
 * @param state UI state (for selection tracking)
 * @param thumbnail_size Size of each thumbnail (pixels)
 * @return Index of clicked thumbnail, or -1 if none clicked
 */
int render_molecule_gallery(const std::vector<Molecule>& molecules,
                            ContinuousGenerationState& state,
                            int thumbnail_size = 100);

/**
 * Get category name for display (no hardcoded formulas!)
 * @param category Molecule category
 * @return Human-readable category name
 */
const char* get_category_name(dynamic::MoleculeCategory category);

/**
 * Get category description (explains what will be generated)
 * @param category Molecule category
 * @return Description string (no specific formulas!)
 */
const char* get_category_description(dynamic::MoleculeCategory category);

} // namespace gui
} // namespace vsepr
