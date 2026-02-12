#include "viz_router.hpp"
#include "renderer.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace vsepr {

VizRouter::VizRouter()
    : accumulator_(0.0)
    , last_frame_time_(Clock::now())
{
    std::fill(std::begin(fps_samples_), std::end(fps_samples_), 60.0f);
    config_.apply_mode_preset(VizMode::CARTOON);
    physics_dt_ = 1.0 / config_.physics_hz;
}

void VizRouter::init(const VizConfig& config) {
    config_ = config;
    physics_dt_ = 1.0 / config_.physics_hz;
    last_frame_time_ = Clock::now();
}

void VizRouter::update(double frame_time) {
    // Update accumulator for physics stepping
    if (config_.enable_interpolation) {
        accumulator_ += frame_time;
        
        // Cap accumulator to prevent spiral of death
        const double max_accumulator = physics_dt_ * 10.0;
        if (accumulator_ > max_accumulator) {
            accumulator_ = max_accumulator;
        }
        
        // Calculate interpolation alpha
        scene_.alpha = static_cast<float>(accumulator_ / physics_dt_);
        scene_.alpha = std::clamp(scene_.alpha, 0.0f, 1.0f);
        
        stats_.interpolation_alpha = scene_.alpha;
    } else {
        // No interpolation - always use current frame
        scene_.alpha = 1.0f;
        stats_.interpolation_alpha = 1.0f;
    }
    
    // Update FPS calculation
    float fps = frame_time > 0.0 ? static_cast<float>(1.0 / frame_time) : 60.0f;
    fps_samples_[fps_sample_idx_] = fps;
    fps_sample_idx_ = (fps_sample_idx_ + 1) % FPS_SAMPLE_COUNT;
    
    // Calculate average FPS
    float sum = std::accumulate(std::begin(fps_samples_), std::end(fps_samples_), 0.0f);
    stats_.fps = sum / FPS_SAMPLE_COUNT;
    stats_.frame_time_ms = static_cast<float>(frame_time * 1000.0);
}

void VizRouter::update_physics(const FrameSnapshot& snapshot) {
    // Shift current to previous
    scene_.previous = scene_.current;
    scene_.current = snapshot;
    
    // Reset accumulator when new physics frame arrives
    if (config_.enable_interpolation) {
        accumulator_ -= physics_dt_;
        if (accumulator_ < 0.0) {
            accumulator_ = 0.0;
        }
    }
}

void VizRouter::set_mode(VizMode mode) {
    config_.apply_mode_preset(mode);
}

void VizRouter::render(Renderer& renderer, int width, int height) {
    // Route to appropriate rendering path
    switch (config_.mode) {
    case VizMode::SIMPLE:
        render_simple(renderer, width, height);
        break;
        
    case VizMode::CARTOON:
        render_cartoon(renderer, width, height);
        break;
        
    case VizMode::REALISTIC:
        render_realistic(renderer, width, height);
        break;
        
    case VizMode::DEBUG:
        render_debug(renderer, width, height);
        break;
    }
}

// ============================================================================
// Rendering Paths
// ============================================================================

void VizRouter::render_simple(Renderer& renderer, int width, int height) {
    // Simple mode: just render current frame with no frills
    renderer.set_show_bonds(true);
    renderer.set_show_box(false);
    renderer.render(scene_.current, width, height);
}

void VizRouter::render_cartoon(Renderer& renderer, int width, int height) {
    // Cartoon mode: render with interpolation if enabled
    if (config_.enable_interpolation && scene_.previous.is_valid()) {
        // Create temporary interpolated snapshot
        FrameSnapshot interpolated = scene_.current;
        
        // Interpolate positions
        for (size_t i = 0; i < interpolated.positions.size(); ++i) {
            interpolated.positions[i] = scene_.get_position(i);
        }
        
        renderer.set_show_bonds(true);
        renderer.set_show_box(config_.show_box);
        renderer.render(interpolated, width, height);
    } else {
        // No interpolation - render current frame
        renderer.set_show_bonds(true);
        renderer.set_show_box(config_.show_box);
        renderer.render(scene_.current, width, height);
    }
}

void VizRouter::render_realistic(Renderer& renderer, int width, int height) {
    // Realistic mode: same as cartoon for now (PBR not yet implemented)
    // TODO: Add PBR materials, shadows, better lighting
    render_cartoon(renderer, width, height);
}

void VizRouter::render_debug(Renderer& renderer, int width, int height) {
    // Debug mode: render with all debug overlays
    renderer.set_show_bonds(true);
    renderer.set_show_box(true);
    
    // Render main scene
    renderer.render(scene_.current, width, height);
    
    // TODO: Add force arrow rendering
    // TODO: Add coordinate axes rendering
    // TODO: Add velocity vectors
}

} // namespace vsepr
