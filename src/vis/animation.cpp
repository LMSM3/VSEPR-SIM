#include "animation.hpp"
#include <cmath>
#include <algorithm>

namespace vsepr {
namespace render {

AnimationController::AnimationController() = default;

void AnimationController::set_animation(AnimationType type) {
    type_ = type;
    reset();
}

void AnimationController::reset() {
    time_ = 0.0f;
    rotation_angle_ = 0.0f;
    current_frame_ = 0;
    frame_accumulator_ = 0.0f;
    orbit_angle_ = 0.0f;
    original_positions_.clear();
}

void AnimationController::update(float dt, AtomicGeometry& geom) {
    if (paused_ || type_ == AnimationType::NONE) {
        return;
    }
    
    time_ += dt * speed_;
    
    switch (type_) {
        case AnimationType::ROTATE_Y:
            update_rotation(dt * speed_, geom);
            break;
        
        case AnimationType::ROTATE_XYZ: {
            rotation_axis_ = Vec3{
                std::sin(time_ * 0.7f),
                std::cos(time_ * 0.5f),
                std::sin(time_ * 0.3f)
            };
            // Normalize
            float len = std::sqrt(rotation_axis_.x * rotation_axis_.x +
                                rotation_axis_.y * rotation_axis_.y +
                                rotation_axis_.z * rotation_axis_.z);
            if (len > 1e-6f) {
                rotation_axis_.x /= len;
                rotation_axis_.y /= len;
                rotation_axis_.z /= len;
            }
            update_rotation(dt * speed_, geom);
            break;
        }
        
        case AnimationType::OSCILLATE:
            update_oscillation(dt * speed_, geom);
            break;
        
        case AnimationType::TRAJECTORY:
            update_trajectory(dt * speed_, geom);
            break;
        
        case AnimationType::ORBIT_CAMERA:
            update_orbit(dt * speed_);
            break;
        
        default:
            break;
    }
}

void AnimationController::load_trajectory(const std::vector<AtomicGeometry>& frames) {
    trajectory_ = frames;
    current_frame_ = 0;
    frame_accumulator_ = 0.0f;
}

// ============================================================================
// Animation Updates
// ============================================================================

void AnimationController::update_rotation(float dt, AtomicGeometry& geom) {
    rotation_angle_ += rotation_speed_ * dt;
    
    // Keep angle in [0, 2π]
    const float two_pi = 2.0f * M_PI;
    while (rotation_angle_ > two_pi) {
        rotation_angle_ -= two_pi;
    }
    
    // Apply rotation to all atoms
    apply_rotation(geom, rotation_axis_, rotation_angle_);
}

void AnimationController::update_oscillation(float dt, AtomicGeometry& geom) {
    // Save original positions on first call
    if (original_positions_.empty()) {
        original_positions_ = geom.positions;
    }
    
    if (original_positions_.size() != geom.positions.size()) {
        // Geometry changed, reset
        original_positions_ = geom.positions;
        return;
    }
    
    // Apply sinusoidal displacement to each atom
    float phase = 2.0f * M_PI * osc_frequency_ * time_;
    float displacement = osc_amplitude_ * std::sin(phase);
    
    for (size_t i = 0; i < geom.positions.size(); ++i) {
        // Each atom oscillates with random phase offset
        float atom_phase = phase + i * 0.5f;  // Phase offset per atom
        float atom_displacement = osc_amplitude_ * std::sin(atom_phase);
        
        // Oscillate along original position direction (radial from COM)
        Vec3 orig = original_positions_[i];
        float len = std::sqrt(orig.x*orig.x + orig.y*orig.y + orig.z*orig.z);
        
        if (len > 1e-6f) {
            Vec3 dir = {orig.x/len, orig.y/len, orig.z/len};
            geom.positions[i].x = orig.x + dir.x * atom_displacement;
            geom.positions[i].y = orig.y + dir.y * atom_displacement;
            geom.positions[i].z = orig.z + dir.z * atom_displacement;
        } else {
            // Atom at origin, oscillate vertically
            geom.positions[i].y = atom_displacement;
        }
    }
}

void AnimationController::update_trajectory(float dt, AtomicGeometry& geom) {
    if (trajectory_.empty()) {
        return;
    }
    
    frame_accumulator_ += dt * trajectory_fps_;
    
    // Advance frames
    while (frame_accumulator_ >= 1.0f) {
        frame_accumulator_ -= 1.0f;
        current_frame_++;
        
        if (current_frame_ >= static_cast<int>(trajectory_.size())) {
            if (loop_trajectory_) {
                current_frame_ = 0;
            } else {
                current_frame_ = static_cast<int>(trajectory_.size()) - 1;
                paused_ = true;  // Stop at end
            }
        }
    }
    
    // Update geometry to current frame
    geom = trajectory_[current_frame_];
}

void AnimationController::update_orbit(float dt) {
    orbit_angle_ += rotation_speed_ * dt;
    
    // Keep in [0, 2π]
    const float two_pi = 2.0f * M_PI;
    while (orbit_angle_ > two_pi) {
        orbit_angle_ -= two_pi;
    }
    
    // Camera position will be computed in renderer
    // (orbit_radius * cos(orbit_angle_), 0, orbit_radius * sin(orbit_angle_))
}

// ============================================================================
// Utilities
// ============================================================================

void AnimationController::apply_rotation(AtomicGeometry& geom, const Vec3& axis, float angle) {
    // Rodrigues' rotation formula
    for (auto& pos : geom.positions) {
        pos = rotate_vector(pos, axis, angle);
    }
}

Vec3 AnimationController::rotate_vector(const Vec3& v, const Vec3& axis, float angle) {
    // Rodrigues' rotation formula: v' = v cos(θ) + (k × v) sin(θ) + k (k · v) (1 - cos(θ))
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);
    float one_minus_cos = 1.0f - cos_a;
    
    // k × v (cross product)
    Vec3 k_cross_v = {
        axis.y * v.z - axis.z * v.y,
        axis.z * v.x - axis.x * v.z,
        axis.x * v.y - axis.y * v.x
    };
    
    // k · v (dot product)
    float k_dot_v = axis.x * v.x + axis.y * v.y + axis.z * v.z;
    
    // Rodrigues' formula
    Vec3 result;
    result.x = v.x * cos_a + k_cross_v.x * sin_a + axis.x * k_dot_v * one_minus_cos;
    result.y = v.y * cos_a + k_cross_v.y * sin_a + axis.y * k_dot_v * one_minus_cos;
    result.z = v.z * cos_a + k_cross_v.z * sin_a + axis.z * k_dot_v * one_minus_cos;
    
    return result;
}

} // namespace render
} // namespace vsepr
