#pragma once
#include <string>

namespace vsepr {

/**
 * Visualization rendering modes for the VSEPR-Sim engine.
 * Controls rendering style and features.
 */
enum class VizMode {
    // Simple solid rendering (fastest)
    SIMPLE,

    // Cartoon style: flat shading + outlines (recommended)
    CARTOON,

    // Realistic: PBR-like materials (expensive)
    REALISTIC,

    // Debug: wireframe + force arrows + axes
    DEBUG,

    // Passive batch display — window stays alive during continuous runs.
    // No interaction processing, no outline/shadow overhead.
    // Display is throttled (default 5 fps); producer runs uncapped.
    // Status overlay shows run label, frame index, and artifact count.
    BATCH_PASSIVE
};

/**
 * Visualization router configuration.
 * Controls which rendering path is active.
 */
struct VizConfig {
    VizMode mode = VizMode::CARTOON;
    
    // Feature flags (can override based on mode)
    bool enable_outlines = true;
    bool enable_shadows = false;
    bool enable_motion_blur = false;
    bool enable_antialiasing = true;
    
    // Performance settings
    bool enable_interpolation = true;  // Smooth motion between physics steps
    float target_fps = 60.0f;
    float physics_hz = 120.0f;         // Fixed physics timestep

    // BATCH_PASSIVE settings
    float batch_display_fps = 5.0f;    // Max display rate during batch runs
    bool  batch_show_status  = true;   // Show run label / frame / artifact count overlay
    std::string batch_run_label;       // e.g. material tag or report ID
    
    // Debug overlays
    bool show_force_arrows = false;
    bool show_axes = true;
    bool show_box = true;
    bool show_fps = true;
    bool show_energy = true;
    
    /**
     * Apply preset configuration for a given mode.
     */
    void apply_mode_preset(VizMode new_mode) {
        mode = new_mode;
        
        switch (mode) {
        case VizMode::SIMPLE:
            enable_outlines = false;
            enable_shadows = false;
            enable_antialiasing = false;
            show_force_arrows = false;
            show_axes = false;
            break;
            
        case VizMode::CARTOON:
            enable_outlines = true;
            enable_shadows = false;
            enable_antialiasing = true;
            show_force_arrows = false;
            show_axes = true;
            break;
            
        case VizMode::REALISTIC:
            enable_outlines = false;
            enable_shadows = true;
            enable_antialiasing = true;
            show_force_arrows = false;
            show_axes = false;
            break;
            
        case VizMode::DEBUG:
            enable_outlines = true;
            enable_shadows = false;
            enable_antialiasing = false;
            show_force_arrows = true;
            show_axes = true;
            break;

        case VizMode::BATCH_PASSIVE:
            // Minimum overhead — no outlines, no shadows, no AA, no interaction
            enable_outlines      = false;
            enable_shadows       = false;
            enable_antialiasing  = false;
            enable_motion_blur   = false;
            enable_interpolation = false;  // producer drives frame timing
            show_force_arrows    = false;
            show_axes            = false;
            show_box             = true;   // keep box so structure is readable
            show_fps             = false;  // replaced by batch status overlay
            show_energy          = true;
            batch_show_status    = true;
            target_fps           = batch_display_fps;
            break;
        }
    }
};

} // namespace vsepr
