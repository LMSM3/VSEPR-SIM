#pragma once

#include "core/math_vec3.hpp"
#include "renderer_base.hpp"
#include <vector>
#include <functional>

namespace vsepr {
namespace render {

/**
 * Animation types
 */
enum class AnimationType {
    NONE,              // Static (no animation)
    ROTATE_Y,          // Rotate around Y-axis
    ROTATE_XYZ,        // Tumble (rotate around all axes)
    OSCILLATE,         // Oscillate atoms (thermal motion simulation)
    TRAJECTORY,        // Play back MD trajectory
    ZOOM_PULSE,        // Pulsating zoom (breathe effect)
    ORBIT_CAMERA       // Camera orbits around molecule
};

/**
 * Animation controller for molecular visualization
 * 
 * Provides simple animations:
 * - Rotation (Y-axis or tumble)
 * - Oscillation (thermal vibrations)
 * - Trajectory playback (MD frames)
 * - Camera animations
 */
class AnimationController {
public:
    AnimationController();
    
    /**
     * Set animation type
     */
    void set_animation(AnimationType type);
    AnimationType get_animation() const { return type_; }
    
    /**
     * Update animation state
     * 
     * @param dt Time delta (seconds)
     * @param geom Geometry to animate (may be modified)
     */
    void update(float dt, AtomicGeometry& geom);
    
    /**
     * Load trajectory for playback
     * 
     * @param frames Vector of geometries (one per frame)
     */
    void load_trajectory(const std::vector<AtomicGeometry>& frames);
    
    /**
     * Set animation speed
     * 
     * @param speed Speed multiplier (1.0 = normal, 2.0 = double speed)
     */
    void set_speed(float speed) { speed_ = speed; }
    float get_speed() const { return speed_; }
    
    /**
     * Pause/resume animation
     */
    void pause() { paused_ = true; }
    void resume() { paused_ = false; }
    void toggle_pause() { paused_ = !paused_; }
    bool is_paused() const { return paused_; }
    
    /**
     * Reset animation state
     */
    void reset();
    
    // ========================================================================
    // Rotation Settings
    // ========================================================================
    
    void set_rotation_speed(float radians_per_sec) { rotation_speed_ = radians_per_sec; }
    void set_rotation_axis(const Vec3& axis) { rotation_axis_ = axis; }
    
    // ========================================================================
    // Oscillation Settings
    // ========================================================================
    
    void set_oscillation_amplitude(float amplitude) { osc_amplitude_ = amplitude; }
    void set_oscillation_frequency(float freq_hz) { osc_frequency_ = freq_hz; }
    
    // ========================================================================
    // Trajectory Settings
    // ========================================================================
    
    void set_loop_trajectory(bool loop) { loop_trajectory_ = loop; }
    void set_trajectory_fps(float fps) { trajectory_fps_ = fps; }
    
    int get_current_frame() const { return current_frame_; }
    int get_frame_count() const { return static_cast<int>(trajectory_.size()); }
    
private:
    // Animation state
    AnimationType type_ = AnimationType::NONE;
    float time_ = 0.0f;
    float speed_ = 1.0f;
    bool paused_ = false;
    
    // Rotation
    float rotation_speed_ = 1.0f;  // radians/sec
    Vec3 rotation_axis_ = Vec3{0, 1, 0};  // Y-axis default
    float rotation_angle_ = 0.0f;
    
    // Oscillation
    float osc_amplitude_ = 0.05f;  // Angstroms
    float osc_frequency_ = 2.0f;   // Hz
    std::vector<Vec3> original_positions_;
    
    // Trajectory
    std::vector<AtomicGeometry> trajectory_;
    int current_frame_ = 0;
    float trajectory_fps_ = 30.0f;
    bool loop_trajectory_ = true;
    float frame_accumulator_ = 0.0f;
    
    // Camera orbit
    float orbit_angle_ = 0.0f;
    float orbit_radius_ = 10.0f;
    
    // Animation update functions
    void update_rotation(float dt, AtomicGeometry& geom);
    void update_oscillation(float dt, AtomicGeometry& geom);
    void update_trajectory(float dt, AtomicGeometry& geom);
    void update_orbit(float dt);
    
    // Utilities
    void apply_rotation(AtomicGeometry& geom, const Vec3& axis, float angle);
    Vec3 rotate_vector(const Vec3& v, const Vec3& axis, float angle);
};

} // namespace render
} // namespace vsepr
