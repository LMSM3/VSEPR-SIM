#pragma once

#include "vis/viz_config.hpp"
#include "core/frame_snapshot.hpp"
#include <chrono>

namespace vsepr {

// Forward declarations
class Renderer;
class Camera;

/**
 * Interpolated scene state for smooth rendering.
 * Combines previous and current physics snapshots with interpolation factor.
 */
struct InterpolatedScene {
    FrameSnapshot current;
    FrameSnapshot previous;
    float alpha = 0.0f;  // Interpolation factor (0.0 = previous, 1.0 = current)
    
    /**
     * Get interpolated position for an atom.
     */
    Vec3 get_position(size_t idx) const {
        if (idx >= current.positions.size() || idx >= previous.positions.size()) {
            return idx < current.positions.size() ? current.positions[idx] : Vec3(0,0,0);
        }
        
        const Vec3& p0 = previous.positions[idx];
        const Vec3& p1 = current.positions[idx];
        
        return Vec3(
            p0.x + alpha * (p1.x - p0.x),
            p0.y + alpha * (p1.y - p0.y),
            p0.z + alpha * (p1.z - p0.z)
        );
    }
};

/**
 * Visualization mode router.
 * Routes rendering to appropriate render path based on VizMode.
 * Handles fixed timestep physics with render interpolation.
 */
class VizRouter {
public:
    VizRouter();
    
    /**
     * Initialize with configuration.
     */
    void init(const VizConfig& config);
    
    /**
     * Update timing and interpolation state.
     * Call once per frame before rendering.
     * 
     * @param frame_time Time elapsed since last frame (seconds)
     */
    void update(double frame_time);
    
    /**
     * Update physics snapshot.
     * Call this when a new physics frame is available.
     */
    void update_physics(const FrameSnapshot& snapshot);
    
    /**
     * Render current interpolated scene.
     * Routes to appropriate rendering path based on current mode.
     */
    void render(Renderer& renderer, int width, int height);
    
    /**
     * Get current configuration.
     */
    VizConfig& config() { return config_; }
    const VizConfig& config() const { return config_; }
    
    /**
     * Switch rendering mode (applies preset).
     */
    void set_mode(VizMode mode);
    
    /**
     * Get current interpolated scene (for custom rendering).
     */
    const InterpolatedScene& scene() const { return scene_; }
    
    /**
     * Get rendering statistics.
     */
    struct Stats {
        float fps = 0.0f;
        float frame_time_ms = 0.0f;
        int physics_steps_per_frame = 0;
        float interpolation_alpha = 0.0f;
    };
    
    const Stats& stats() const { return stats_; }
    
private:
    // Rendering paths
    void render_simple(Renderer& renderer, int width, int height);
    void render_cartoon(Renderer& renderer, int width, int height);
    void render_realistic(Renderer& renderer, int width, int height);
    void render_debug(Renderer& renderer, int width, int height);
    
    // Configuration
    VizConfig config_;
    
    // Interpolation state
    InterpolatedScene scene_;
    double accumulator_ = 0.0;
    double physics_dt_ = 1.0 / 120.0;  // Fixed physics timestep
    
    // Timing
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    
    TimePoint last_frame_time_;
    Stats stats_;
    
    // FPS calculation
    static const int FPS_SAMPLE_COUNT = 60;
    float fps_samples_[FPS_SAMPLE_COUNT];
    int fps_sample_idx_ = 0;
};

} // namespace vsepr
