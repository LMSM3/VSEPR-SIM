#pragma once

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
    DEBUG
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
        }
    }
};

} // namespace vsepr
