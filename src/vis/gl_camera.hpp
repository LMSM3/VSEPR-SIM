#pragma once
/**
 * gl_camera.hpp
 * Modern camera system with multiple projection modes
 */

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace vsepr {
namespace vis {

// ============================================================================
// Camera Modes
// ============================================================================

enum class CameraMode {
    PERSPECTIVE,    // Standard 3D perspective
    ORTHOGRAPHIC,   // 2D orthographic view
    ISOMETRIC       // Technical isometric view
};

// ============================================================================
// Camera Projection
// ============================================================================

class Camera {
public:
    Camera();
    
    /**
     * Set perspective projection
     */
    void set_perspective(float fov, float aspect, float near, float far);
    
    /**
     * Set orthographic projection
     */
    void set_orthographic(float width, float height, float near, float far);
    
    /**
     * Set isometric view
     */
    void set_isometric(float size, float near, float far);
    
    /**
     * Position camera in world space
     */
    void set_position(const glm::vec3& pos);
    void set_target(const glm::vec3& target);
    void set_up(const glm::vec3& up);
    
    /**
     * Orbit camera around target (mouse control)
     */
    void orbit(float delta_x, float delta_y, float distance);
    
    /**
     * Pan camera (translate in view space)
     */
    void pan(float delta_x, float delta_y);
    
    /**
     * Zoom camera (scale distance from target)
     */
    void zoom(float delta);
    
    /**
     * Reset to default view
     */
    void reset();
    
    // ========== Getters ==========
    
    glm::mat4 get_view_matrix() const;
    glm::mat4 get_projection_matrix() const;
    glm::mat4 get_view_projection() const;
    
    glm::vec3 get_position() const { return position_; }
    glm::vec3 get_target() const { return target_; }
    glm::vec3 get_forward() const;
    glm::vec3 get_right() const;
    glm::vec3 get_up() const { return up_; }
    
    float get_distance() const;
    float get_fov() const { return fov_; }
    
    /**
     * Pick ray from screen coordinates (for selection)
     */
    glm::vec3 get_ray_from_screen(float screen_x, float screen_y, 
                                  float width, float height);
    
    /**
     * Update aspect ratio
     */
    void set_aspect_ratio(float aspect);
    
private:
    // Position and orientation
    glm::vec3 position_;
    glm::vec3 target_;
    glm::vec3 up_;
    
    // Projection parameters
    float fov_;
    float aspect_;
    float near_;
    float far_;
    
    // Orthographic parameters
    float ortho_width_;
    float ortho_height_;
    
    CameraMode mode_;
    
    void update_matrices();
};

// ============================================================================
// Camera Controller (Input handling)
// ============================================================================

class CameraController {
public:
    CameraController(Camera& camera);
    
    /**
     * Handle mouse movement
     */
    void on_mouse_move(float x, float y);
    
    /**
     * Handle mouse button (0=left, 1=right, 2=middle)
     */
    void on_mouse_button(int button, bool pressed);
    
    /**
     * Handle mouse wheel
     */
    void on_mouse_wheel(float delta);
    
    /**
     * Handle keyboard (for future use)
     */
    void on_key(int key, bool pressed);
    
    /**
     * Set mouse sensitivity
     */
    void set_sensitivity(float orbit_speed, float pan_speed, float zoom_speed);
    
private:
    Camera& camera_;
    float last_mouse_x_ = 0;
    float last_mouse_y_ = 0;
    bool left_button_down_ = false;
    bool right_button_down_ = false;
    bool middle_button_down_ = false;
    
    float orbit_speed_ = 0.005f;
    float pan_speed_ = 0.01f;
    float zoom_speed_ = 0.05f;
};

} // namespace vis
} // namespace vsepr
